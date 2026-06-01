#include "../Events.h"
#include "../Platform.h"

#include <BAN/Vector.h>

#include <LibGUI/Window.h>

struct BananWindow final : public PlatformWindow
{
	~BananWindow()
	{
		if (window)
			unregister_event_fd(window->server_fd());
	}

	BAN::UniqPtr<LibGUI::Window> window;
};

struct BananCursor final : public PlatformCursor
{
	BAN::Vector<uint32_t> pixels;
	uint32_t width;
	uint32_t height;
	int32_t origin_x;
	int32_t origin_y;
};

static bool bananos_initialize(uint32_t* display_w, uint32_t* display_h)
{
	auto attributes = LibGUI::Window::default_attributes;
	attributes.shown = false;

	auto dummy_or_error = LibGUI::Window::create(0, 0, ""_sv, attributes);
	if (dummy_or_error.is_error())
	{
		dwarnln("Could not get display size: {}", dummy_or_error.error());
		return false;
	}

	*display_w = dummy_or_error.value()->width();
	*display_h = dummy_or_error.value()->height();

	return true;
}

static void bananos_poll_events(void* window)
{
	auto& banan_window = *static_cast<BananWindow*>(window);
	banan_window.window->poll_events();
}

static BAN::ErrorOr<BAN::UniqPtr<PlatformWindow>> bananos_create_window(PlatformWindow* parent, WindowType type, WINDOW wid, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
	(void)parent;
	(void)type;
	(void)x;
	(void)y;

	auto window = TRY(BAN::UniqPtr<BananWindow>::create());

	auto attributes = LibGUI::Window::default_attributes;
	attributes.shown = true;
	attributes.title_bar = false;
	attributes.resizable = true;

	auto gui_window = TRY(LibGUI::Window::create(width, height, ""_sv, attributes));
	auto* winp = gui_window.ptr();

	gui_window->set_close_window_event_callback([wid] {
		on_window_close_event(wid);
	});
	gui_window->set_resize_window_event_callback([wid, winp]() {
		on_window_resize_event(wid, winp->width(), winp->height());
	});
	gui_window->set_window_focus_event_callback([wid](auto event) {
		on_window_focus_event(wid, event.focused);
	});
	gui_window->set_window_fullscreen_event_callback([wid](auto event) {
		on_window_fullscreen_event(wid, event.fullscreen);
	});
	gui_window->set_mouse_move_event_callback([wid](auto event) {
		on_mouse_move_event(wid, event.x, event.y);
	});
	gui_window->set_mouse_button_event_callback([wid](auto event) {
		uint8_t xbutton = 0;
		switch (event.button)
		{
			case LibInput::MouseButton::Left:   xbutton = 1; break;
			case LibInput::MouseButton::Middle: xbutton = 2; break;
			case LibInput::MouseButton::Right:  xbutton = 3; break;
			case LibInput::MouseButton::Extra1: xbutton = 8; break;
			case LibInput::MouseButton::Extra2: xbutton = 9; break;
		}
		if (xbutton)
			on_mouse_button_event(wid, xbutton, event.pressed);
	});
	gui_window->set_mouse_scroll_event_callback([wid](auto event) {
		on_mouse_button_event(wid, event.scroll > 0 ? 4 : 5, true);
		on_mouse_button_event(wid, event.scroll > 0 ? 4 : 5, false);
	});
	gui_window->set_key_event_callback([wid](auto event) {
		const uint8_t xmod =
			(event.shift()     ? (1 << 0) : 0) |
			(event.caps_lock() ? (1 << 1) : 0) |
			(event.ctrl()      ? (1 << 2) : 0) |
			(event.alt()       ? (1 << 3) : 0);
		on_key_event(wid, event.scancode, xmod, event.pressed());
	});

	window->window = BAN::move(gui_window);

	register_event_fd(window->window->server_fd(), window.ptr());

	return BAN::UniqPtr<PlatformWindow>(BAN::move(window));
}

static void bananos_request_resize(PlatformWindow* window, uint32_t width, uint32_t height)
{
	auto& banan_window = *static_cast<BananWindow*>(window);
	banan_window.window->request_resize(width, height);
}

static void bananos_request_reposition(PlatformWindow* window, int32_t x, int32_t y)
{
	(void)window;
	(void)x;
	(void)y;
}

static void bananos_invalidate(PlatformWindow* window, const uint32_t* pixels, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	auto& banan_window = *static_cast<BananWindow*>(window);

	const uint32_t win_width = banan_window.window->width();

	auto* out_pixels = banan_window.window->texture().pixels().data();
	for (uint32_t y_off = 0; y_off < height; y_off++)
		for (uint32_t x_off = 0; x_off < width; x_off++)
			out_pixels[(y + y_off) * win_width + (x + x_off)] = pixels[(y + y_off) * win_width + (x + x_off)];

	banan_window.window->invalidate(x, y, width, height);
}

static void bananos_request_fullscreen(PlatformWindow* window, bool fullscreen)
{
	auto& banan_window = *static_cast<BananWindow*>(window);
	banan_window.window->set_fullscreen(fullscreen);
}

static void bananos_warp_pointer(int32_t x, int32_t y, bool relative)
{
	(void)x;
	(void)y;
	(void)relative;
}

static void bananos_query_pointer(int32_t* x, int32_t* y)
{
	(void)x;
	(void)y;
}

static void bananos_set_pointer_grab(PlatformWindow* window, bool grabbed)
{
	(void)window;
	(void)grabbed;
}

static BAN::ErrorOr<BAN::UniqPtr<PlatformCursor>> bananos_create_system_cursor(SystemCursorType type)
{
	(void)type;
	return BAN::Error::from_errno(ENOTSUP);
}

static BAN::ErrorOr<BAN::UniqPtr<PlatformCursor>> bananos_create_bitmap_cursor(const uint32_t* pixels, uint32_t width, uint32_t height, int32_t origin_x, int32_t origin_y)
{
	auto cursor = TRY(BAN::UniqPtr<BananCursor>::create());
	cursor->width = width;
	cursor->height = height;
	cursor->origin_x = origin_x;
	cursor->origin_y = origin_y;

	TRY(cursor->pixels.resize(width * height));
	memcpy(cursor->pixels.data(), pixels, cursor->pixels.size() * 4);

	return BAN::UniqPtr<PlatformCursor>(BAN::move(cursor));
}

static void bananos_set_cursor(PlatformWindow* window, PlatformCursor* cursor)
{
	if (window == nullptr)
		return;

	auto& banan_window = *static_cast<BananWindow*>(window);
	if (cursor == nullptr)
		banan_window.window->set_cursor(0, 0, {}, 0, 0);
	else
	{
		auto& banan_cursor = *static_cast<BananCursor*>(cursor);
		banan_window.window->set_cursor(banan_cursor.width, banan_cursor.height, banan_cursor.pixels.span(), banan_cursor.origin_x, banan_cursor.origin_y);
	}
}

PlatformOps g_platform_ops = {
	.initialize           = bananos_initialize,
	.poll_events          = bananos_poll_events,
	.create_window        = bananos_create_window,
	.invalidate           = bananos_invalidate,
	.request_resize       = bananos_request_resize,
	.request_reposition   = bananos_request_reposition,
	.request_fullscreen   = bananos_request_fullscreen,
	.warp_pointer         = bananos_warp_pointer,
	.query_pointer        = nullptr, /* bananos_query_pointer */
	.set_pointer_grab     = bananos_set_pointer_grab,
	.create_system_cursor = bananos_create_system_cursor,
	.create_bitmap_cursor = bananos_create_bitmap_cursor,
	.set_cursor           = bananos_set_cursor,
};

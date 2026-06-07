#include "../Events.h"
#include "../Keymap.h"
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

static BAN::ErrorOr<void> bananos_initialize_keymap();

static bool bananos_initialize()
{
	auto attributes = LibGUI::Window::default_attributes;
	attributes.shown = false;

	auto dummy_or_error = LibGUI::Window::create(0, 0, ""_sv, attributes);
	if (dummy_or_error.is_error())
	{
		dwarnln("Could not get display size: {}", dummy_or_error.error());
		return false;
	}

	register_display(0, 0, dummy_or_error.value()->width(), dummy_or_error.value()->height());

	if (auto ret = bananos_initialize_keymap(); ret.is_error())
	{
		dwarnln("Could not initialize keymap: {}", ret.error());
		return false;
	}

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
			(event.alt()       ? (1 << 3) : 0) |
			(event.num_lock()  ? (1 << 4) : 0) |
			// level5 shift
			// super
			(event.ralt()      ? (1 << 7) : 0); // FIXME: altgr
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
	.request_reposition   = nullptr,
	.request_fullscreen   = bananos_request_fullscreen,
	.warp_pointer         = nullptr,
	.query_pointer        = nullptr,
	.set_pointer_grab     = nullptr,
	.create_system_cursor = nullptr,
	.create_bitmap_cursor = bananos_create_bitmap_cursor,
	.set_cursor           = bananos_set_cursor,
};

#include <LibInput/KeyboardLayout.h>
#include <LibInput/KeyEvent.h>

static uint32_t bananos_keyevent_to_x_keysym(LibInput::KeyEvent event);

static BAN::ErrorOr<void> bananos_initialize_keymap()
{
	static constexpr uint16_t modifier_map[] {
		0,
		LibInput::KeyEvent::LShift,
		LibInput::KeyEvent::RAlt,
		LibInput::KeyEvent::RAlt | LibInput::KeyEvent::LShift,
	};

	// FIXME: get this from somewhere (gui command? enviroment? tmp file?)
	const auto keymap_path = "./us.keymap"_sv;
	TRY(LibInput::KeyboardLayout::initialize());
	auto& keyboard_layout = LibInput::KeyboardLayout::get();
	TRY(keyboard_layout.load_from_file(keymap_path));

	for (uint8_t scancode = 0; scancode < g_keymap_size; scancode++)
		for (size_t layer = 0; layer < g_keymap_layers; layer++)
			if (const auto event = keyboard_layout.key_event_from_raw({ .modifier = modifier_map[layer], .keycode = scancode }); event.key != LibInput::Key::None)
				g_keymap[scancode][layer] = bananos_keyevent_to_x_keysym(event);

	using LibInput::keycode_normal;
	using LibInput::keycode_numpad;

	g_modifier_map[0][0] = keycode_normal(3,  0) + g_keymap_min_keycode; // lshift
	g_modifier_map[0][1] = keycode_normal(3, 12) + g_keymap_min_keycode; // rshift

	g_modifier_map[1][0] = keycode_normal(2,  0) + g_keymap_min_keycode; // caps lock

	g_modifier_map[2][0] = keycode_normal(4,  0) + g_keymap_min_keycode; // lctrl
	g_modifier_map[2][1] = keycode_normal(4,  6) + g_keymap_min_keycode; // rctrl

	g_modifier_map[3][0] = keycode_normal(4,  2) + g_keymap_min_keycode; // lalt
	g_modifier_map[3][1] = keycode_normal(4,  5) + g_keymap_min_keycode; // ralt

	g_modifier_map[4][0] = keycode_numpad(0,  0) + g_keymap_min_keycode; // num lock

	//g_modifier_map[5][0] = level5 shift;

	g_modifier_map[6][0] = keycode_normal(4,  1) + g_keymap_min_keycode; // lsuper
	g_modifier_map[6][1] = keycode_normal(4,  4) + g_keymap_min_keycode; // rsuper

	g_modifier_map[7][0] = keycode_normal(4,  5) + g_keymap_min_keycode; // FIXME: altgr

	return {};
}

#include <BAN/UTF8.h>

#include <X11/keysym.h>
#include <X11/XF86keysym.h>

#include <wctype.h>

static uint32_t bananos_keyevent_to_x_keysym(LibInput::KeyEvent event)
{
	using namespace LibInput;

	if (const char* utf8 = key_to_utf8(event.key, event.modifier))
	{
		uint32_t codepoint = BAN::UTF8::to_codepoint(utf8);
		if (codepoint != BAN::UTF8::invalid && iswprint(codepoint))
		{
			if (codepoint >= 0x100)
				codepoint |= 0x01000000;
			return codepoint;
		}
	}

	switch (event.key)
	{
		case Key::F1:             return XK_F1;
		case Key::F2:             return XK_F2;
		case Key::F3:             return XK_F3;
		case Key::F4:             return XK_F4;
		case Key::F5:             return XK_F5;
		case Key::F6:             return XK_F6;
		case Key::F7:             return XK_F7;
		case Key::F8:             return XK_F8;
		case Key::F9:             return XK_F9;
		case Key::F10:            return XK_F10;
		case Key::F11:            return XK_F11;
		case Key::F12:            return XK_F12;
		case Key::Insert:         return XK_Insert;
		case Key::PrintScreen:    return XK_Print;
		case Key::Delete:         return XK_Delete;
		case Key::Home:           return XK_Home;
		case Key::End:            return XK_End;
		case Key::PageUp:         return XK_Page_Up;
		case Key::PageDown:       return XK_Page_Down;
		case Key::Enter:          return XK_Return;
		case Key::Backspace:      return XK_BackSpace;
		case Key::Escape:         return XK_Escape;
		case Key::Tab:            return XK_Tab;
		case Key::CapsLock:       return XK_Caps_Lock;
		case Key::LeftShift:      return XK_Shift_L;
		case Key::LeftCtrl:       return XK_Control_L;
		case Key::Super:          return XK_Super_L;
		case Key::LeftAlt:        return XK_Alt_L;
		case Key::RightAlt:       return XK_Alt_R;
		case Key::RightCtrl:      return XK_Control_R;
		case Key::RightShift:     return XK_Shift_R;
		case Key::ArrowUp:        return XK_Up;
		case Key::ArrowDown:      return XK_Down;
		case Key::ArrowLeft:      return XK_Left;
		case Key::ArrowRight:     return XK_Right;
		case Key::NumLock:        return XK_Num_Lock;
		case Key::ScrollLock:     return XK_Scroll_Lock;
		case Key::Numpad0:        return XK_KP_0;
		case Key::Numpad1:        return XK_KP_1;
		case Key::Numpad2:        return XK_KP_2;
		case Key::Numpad3:        return XK_KP_3;
		case Key::Numpad4:        return XK_KP_4;
		case Key::Numpad5:        return XK_KP_5;
		case Key::Numpad6:        return XK_KP_6;
		case Key::Numpad7:        return XK_KP_7;
		case Key::Numpad8:        return XK_KP_8;
		case Key::Numpad9:        return XK_KP_9;
		case Key::NumpadPlus:     return XK_KP_Add;
		case Key::NumpadMinus:    return XK_KP_Subtract;
		case Key::NumpadMultiply: return XK_KP_Multiply;
		case Key::NumpadDivide:   return XK_KP_Divide;
		case Key::NumpadEnter:    return XK_KP_Enter;
		case Key::NumpadDecimal:  return XK_KP_Decimal;
		case Key::VolumeMute:     return XF86XK_AudioMute;
		case Key::VolumeUp:       return XF86XK_AudioRaiseVolume;
		case Key::VolumeDown:     return XF86XK_AudioLowerVolume;
		case Key::Calculator:     return XF86XK_Calculator;
		case Key::MediaPlayPause: return XF86XK_AudioPlay;
		case Key::MediaStop:      return XF86XK_AudioStop;
		case Key::MediaPrevious:  return XF86XK_AudioPrev;
		case Key::MediaNext:      return XF86XK_AudioNext;
		default: break;
	}

	static_assert(static_cast<size_t>(Key::Count) == 145, "update keymap");

	return 0;
}

#include "../Events.h"
#include "../Platform.h"

#include <BAN/Atomic.h>
#include <BAN/HashMap.h>

#include <SDL2/SDL.h>

#include <pthread.h>
#include <sys/eventfd.h>
#include <unistd.h>

BAN::HashMap<uint32_t, struct SDLWindow*> s_window_map;

struct SDLWindow final : public PlatformWindow
{
	~SDLWindow()
	{
		if (texture != nullptr)
			SDL_DestroyTexture(texture);

		if (renderer != nullptr)
			SDL_DestroyRenderer(renderer);

		if (window != nullptr)
		{
			s_window_map.remove(SDL_GetWindowID(window));
			SDL_DestroyWindow(window);
		}
	}

	WINDOW wid;

	uint32_t width;
	uint32_t height;

	SDL_Window*   window   { nullptr };
	SDL_Renderer* renderer { nullptr };
	SDL_Texture*  texture  { nullptr };
};

static int s_eventfd { -1 };

struct Keymap
{
	consteval Keymap();
	uint8_t map[SDL_NUM_SCANCODES];
};
static Keymap s_sdl_keymap;

static void* sdl2_thread(void*)
{
	for (;;)
	{
		const uint64_t value { 1 };
		write(s_eventfd, &value, sizeof(value));
		usleep(16'666);
	}

	return nullptr;
}

static bool sdl2_initialize(uint32_t* display_w, uint32_t* display_h)
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == -1)
	{
		dwarnln("Could not initialize SDL: {}", SDL_GetError());
		return false;
	}

	SDL_DisplayMode DM;
	if (SDL_GetCurrentDisplayMode(0, &DM) == -1)
	{
		dwarnln("Could not get display mode: {}", SDL_GetError());
		return false;
	}
	*display_w = DM.w;
	*display_h = DM.h;

	s_eventfd = eventfd(0, 0);
	if (s_eventfd == -1)
	{
		dwarnln("Could not create eventfd: {}", strerror(errno));
		return false;
	}

	register_event_fd(s_eventfd, nullptr);

	pthread_t thread;
	pthread_create(&thread, nullptr, sdl2_thread, nullptr);

	return true;
}

static bool sdl2_init_window(SDLWindow& window, uint32_t width, uint32_t height)
{
	window.renderer = SDL_CreateRenderer(window.window, -1, SDL_RENDERER_ACCELERATED);
	if (window.renderer == nullptr)
	{
		dwarnln("Could not create SDL renderer: {}\n", SDL_GetError());
		return false;
	}

	window.texture = SDL_CreateTexture(window.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
	if (window.texture == nullptr)
	{
		dwarnln("Could not create SDL texture: {}\n", SDL_GetError());
		return false;
	}

	return true;
}

static BAN::ErrorOr<BAN::UniqPtr<PlatformWindow>> sdl2_create_window(WINDOW wid, uint32_t width, uint32_t height)
{
	auto window = TRY(BAN::UniqPtr<SDLWindow>::create());

	window->wid = wid;
	window->width = width;
	window->height = height;

	const auto flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_UTILITY;
	window->window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, flags);
	if (window->window == nullptr)
	{
		dwarnln("Could not create SDL window: {}", SDL_GetError());
		return BAN::Error::from_errno(EFAULT);
	}

	if (!sdl2_init_window(*window, width, height))
		return BAN::Error::from_errno(EFAULT);

	TRY(s_window_map.insert(SDL_GetWindowID(window->window), window.ptr()));

	return BAN::UniqPtr<PlatformWindow>(BAN::move(window));
}

static void sdl2_request_resize(PlatformWindow* window, uint32_t width, uint32_t height)
{
	auto& sdl_window = *static_cast<SDLWindow*>(window);
	SDL_SetWindowSize(sdl_window.window, width, height);
}

static void sdl2_poll_events(void*)
{
	uint64_t dummy;
	read(s_eventfd, &dummy, sizeof(dummy));	

	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_WINDOWEVENT:
			{
				auto it = s_window_map.find(event.window.windowID);
				if (it == s_window_map.end())
					break;
				auto& window = *it->value;
				switch (event.window.event)
				{
					case SDL_WINDOWEVENT_CLOSE:
						on_window_close_event(window.wid);
						break;
					case SDL_WINDOWEVENT_SIZE_CHANGED:
						window.width  = event.window.data1;
						window.height = event.window.data2;

						SDL_DestroyTexture(window.texture);
						window.texture = SDL_CreateTexture(window.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, window.width, window.height);
						ASSERT(window.texture);

						on_window_resize_event(window.wid, event.window.data1, event.window.data2);
						break;
					case SDL_WINDOWEVENT_FOCUS_GAINED:
						on_window_focus_event(window.wid, true);
						break;
					case SDL_WINDOWEVENT_FOCUS_LOST:
						on_window_focus_event(window.wid, false);
						break;
					case SDL_WINDOWEVENT_MAXIMIZED:
						on_window_fullscreen_event(window.wid, true);
						break;
					case SDL_WINDOWEVENT_RESTORED:
						on_window_fullscreen_event(window.wid, false);
						break;
				}
				break;
			}
			case SDL_MOUSEMOTION:
			{
				auto it = s_window_map.find(event.window.windowID);
				if (it != s_window_map.end())
					on_mouse_move_event(it->value->wid, event.motion.x, event.motion.y);
				break;
			}
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEBUTTONDOWN:
			{
				uint8_t xbutton { 0 };
				switch (event.button.button)
				{
					case SDL_BUTTON_LEFT:   xbutton = 1; break;
					case SDL_BUTTON_MIDDLE: xbutton = 2; break;
					case SDL_BUTTON_RIGHT:  xbutton = 3; break;
					case SDL_BUTTON_X1:     xbutton = 8; break;
					case SDL_BUTTON_X2:     xbutton = 9; break;
				}

				auto it = s_window_map.find(event.window.windowID);
				if (xbutton && it != s_window_map.end())
					on_mouse_button_event(it->value->wid, xbutton, (event.type == SDL_MOUSEBUTTONDOWN));
				break;
			}
			case SDL_MOUSEWHEEL:
			{
				uint8_t xbutton { 0 };
				if (event.wheel.y > 0)
					xbutton = 4;
				if (event.wheel.y < 0)
					xbutton = 5;

				auto it = s_window_map.find(event.window.windowID);
				if (xbutton && it != s_window_map.end())
				{
					on_mouse_button_event(it->value->wid, xbutton, true);
					on_mouse_button_event(it->value->wid, xbutton, false);
				}

				break;
			}
			case SDL_KEYUP:
			case SDL_KEYDOWN:
			{
				uint8_t scancode = s_sdl_keymap.map[event.key.keysym.scancode];

				uint8_t xmod { 0 };
				if (event.key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT))	
					xmod |= (1 << 0);
				if (event.key.keysym.mod & (KMOD_CAPS))	
					xmod |= (1 << 1);
				if (event.key.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL))	
					xmod |= (1 << 2);
				if (event.key.keysym.mod & (KMOD_LALT | KMOD_RALT))	
					xmod |= (1 << 3);

				auto it = s_window_map.find(event.window.windowID);
				if (it != s_window_map.end())
					on_key_event(it->value->wid, scancode, xmod, (event.type == SDL_KEYDOWN));
				break;
			}
		}
	}
}

static void sdl2_invalidate(PlatformWindow* window, const uint32_t* src_pixels, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
	auto& sdl_window = *static_cast<SDLWindow*>(window);

	ASSERT(x < sdl_window.width  && width  <= sdl_window.width  - x);
	ASSERT(y < sdl_window.height && height <= sdl_window.height - y);

	const SDL_Rect rect {
		.x = static_cast<int>(x),
		.y = static_cast<int>(y),
		.w = static_cast<int>(width),
		.h = static_cast<int>(height),
	};

	void* dst_pixels;
	int dst_pitch;
	if (SDL_LockTexture(sdl_window.texture, &rect, &dst_pixels, &dst_pitch) == -1)
	{
		dwarnln("Could not lock texture: {}", SDL_GetError());
		return;
	}

	for (int32_t y_off = 0; y_off < rect.h; y_off++)
	{
		memcpy(
			static_cast<uint8_t*>(dst_pixels) + y_off * dst_pitch,
			&src_pixels[(rect.y + y_off) * sdl_window.width + rect.x],
			rect.w * sizeof(uint32_t)
		);
	}

	SDL_UnlockTexture(sdl_window.texture);

	SDL_RenderClear(sdl_window.renderer);
	SDL_RenderCopy(sdl_window.renderer, sdl_window.texture, NULL, NULL);
	SDL_RenderPresent(sdl_window.renderer);
}

static void sdl2_set_cursor(PlatformWindow* window, const uint32_t* pixels, uint32_t width, uint32_t height, int32_t origin_x, int32_t origin_y)
{
	auto& sdl_window = *static_cast<SDLWindow*>(window);
	(void)sdl_window;
	(void)pixels;
	(void)width;
	(void)height;
	(void)origin_x;
	(void)origin_y;
}

static void sdl2_request_fullscreen(PlatformWindow* window, bool fullscreen)
{
	auto& sdl_window = *static_cast<SDLWindow*>(window);
	SDL_SetWindowFullscreen(sdl_window.window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

PlatformOps g_platform_ops = {
	.initialize         = sdl2_initialize,
	.poll_events        = sdl2_poll_events,
	.create_window      = sdl2_create_window,
	.invalidate         = sdl2_invalidate,
	.request_resize     = sdl2_request_resize,
	.request_fullscreen = sdl2_request_fullscreen,
	.set_cursor         = sdl2_set_cursor,
};

#include <LibInput/KeyEvent.h>

consteval Keymap::Keymap()
{
	for (auto& scancode : map)
		scancode = 0;

	using LibInput::keycode_normal;
	using LibInput::keycode_function;
	using LibInput::keycode_numpad;

	map[SDL_SCANCODE_GRAVE]          = keycode_normal(0,  0);
	map[SDL_SCANCODE_1]              = keycode_normal(0,  1);
	map[SDL_SCANCODE_2]              = keycode_normal(0,  2);
	map[SDL_SCANCODE_3]              = keycode_normal(0,  3);
	map[SDL_SCANCODE_4]              = keycode_normal(0,  4);
	map[SDL_SCANCODE_5]              = keycode_normal(0,  5);
	map[SDL_SCANCODE_6]              = keycode_normal(0,  6);
	map[SDL_SCANCODE_7]              = keycode_normal(0,  7);
	map[SDL_SCANCODE_8]              = keycode_normal(0,  8);
	map[SDL_SCANCODE_9]              = keycode_normal(0,  9);
	map[SDL_SCANCODE_0]              = keycode_normal(0, 10);
	map[SDL_SCANCODE_MINUS]          = keycode_normal(0, 11);
	map[SDL_SCANCODE_EQUALS]         = keycode_normal(0, 12);
	map[SDL_SCANCODE_BACKSPACE]      = keycode_normal(0, 13);

	map[SDL_SCANCODE_TAB]            = keycode_normal(1,  0);
	map[SDL_SCANCODE_Q]              = keycode_normal(1,  1);
	map[SDL_SCANCODE_W]              = keycode_normal(1,  2);
	map[SDL_SCANCODE_E]              = keycode_normal(1,  3);
	map[SDL_SCANCODE_R]              = keycode_normal(1,  4);
	map[SDL_SCANCODE_T]              = keycode_normal(1,  5);
	map[SDL_SCANCODE_Y]              = keycode_normal(1,  6);
	map[SDL_SCANCODE_U]              = keycode_normal(1,  7);
	map[SDL_SCANCODE_I]              = keycode_normal(1,  8);
	map[SDL_SCANCODE_O]              = keycode_normal(1,  9);
	map[SDL_SCANCODE_P]              = keycode_normal(1, 10);
	map[SDL_SCANCODE_LEFTBRACKET]    = keycode_normal(1, 11);
	map[SDL_SCANCODE_RIGHTBRACKET]   = keycode_normal(1, 12);

	map[SDL_SCANCODE_CAPSLOCK]       = keycode_normal(2,  0);
	map[SDL_SCANCODE_A]              = keycode_normal(2,  1);
	map[SDL_SCANCODE_S]              = keycode_normal(2,  2);
	map[SDL_SCANCODE_D]              = keycode_normal(2,  3);
	map[SDL_SCANCODE_F]              = keycode_normal(2,  4);
	map[SDL_SCANCODE_G]              = keycode_normal(2,  5);
	map[SDL_SCANCODE_H]              = keycode_normal(2,  6);
	map[SDL_SCANCODE_J]              = keycode_normal(2,  7);
	map[SDL_SCANCODE_K]              = keycode_normal(2,  8);
	map[SDL_SCANCODE_L]              = keycode_normal(2,  9);
	map[SDL_SCANCODE_SEMICOLON]      = keycode_normal(2, 10);
	map[SDL_SCANCODE_APOSTROPHE]     = keycode_normal(2, 11);
	map[SDL_SCANCODE_BACKSLASH]      = keycode_normal(2, 12);
	map[SDL_SCANCODE_RETURN]         = keycode_normal(2, 13);

	map[SDL_SCANCODE_LSHIFT]         = keycode_normal(3,  0);
	map[SDL_SCANCODE_NONUSBACKSLASH] = keycode_normal(3,  1);
	map[SDL_SCANCODE_Z]              = keycode_normal(3,  2);
	map[SDL_SCANCODE_X]              = keycode_normal(3,  3);
	map[SDL_SCANCODE_C]              = keycode_normal(3,  4);
	map[SDL_SCANCODE_V]              = keycode_normal(3,  5);
	map[SDL_SCANCODE_B]              = keycode_normal(3,  6);
	map[SDL_SCANCODE_N]              = keycode_normal(3,  7);
	map[SDL_SCANCODE_M]              = keycode_normal(3,  8);
	map[SDL_SCANCODE_COMMA]          = keycode_normal(3,  9);
	map[SDL_SCANCODE_PERIOD]         = keycode_normal(3, 10);
	map[SDL_SCANCODE_SLASH]          = keycode_normal(3, 11);
	map[SDL_SCANCODE_RSHIFT]         = keycode_normal(3, 12);

	map[SDL_SCANCODE_LCTRL]          = keycode_normal(4, 0);
	map[SDL_SCANCODE_LGUI]           = keycode_normal(4, 1);
	map[SDL_SCANCODE_LALT]           = keycode_normal(4, 2);
	map[SDL_SCANCODE_SPACE]          = keycode_normal(4, 3);
	map[SDL_SCANCODE_RALT]           = keycode_normal(4, 4);
	map[SDL_SCANCODE_RCTRL]          = keycode_normal(4, 5);

	map[SDL_SCANCODE_UP]             = keycode_normal(5, 0);
	map[SDL_SCANCODE_LEFT]           = keycode_normal(5, 1);
	map[SDL_SCANCODE_DOWN]           = keycode_normal(5, 2);
	map[SDL_SCANCODE_RIGHT]          = keycode_normal(5, 3);

	map[SDL_SCANCODE_ESCAPE]         = keycode_function(0);
	map[SDL_SCANCODE_F1]             = keycode_function(1);
	map[SDL_SCANCODE_F2]             = keycode_function(2);
	map[SDL_SCANCODE_F3]             = keycode_function(3);
	map[SDL_SCANCODE_F4]             = keycode_function(4);
	map[SDL_SCANCODE_F5]             = keycode_function(5);
	map[SDL_SCANCODE_F6]             = keycode_function(6);
	map[SDL_SCANCODE_F7]             = keycode_function(7);
	map[SDL_SCANCODE_F8]             = keycode_function(8);
	map[SDL_SCANCODE_F9]             = keycode_function(9);
	map[SDL_SCANCODE_F10]            = keycode_function(10);
	map[SDL_SCANCODE_F11]            = keycode_function(11);
	map[SDL_SCANCODE_F12]            = keycode_function(12);
	map[SDL_SCANCODE_INSERT]         = keycode_function(13);
	map[SDL_SCANCODE_PRINTSCREEN]    = keycode_function(14);
	map[SDL_SCANCODE_DELETE]         = keycode_function(15);
	map[SDL_SCANCODE_HOME]           = keycode_function(16);
	map[SDL_SCANCODE_END]            = keycode_function(17);
	map[SDL_SCANCODE_PAGEUP]         = keycode_function(18);
	map[SDL_SCANCODE_PAGEDOWN]       = keycode_function(19);
	map[SDL_SCANCODE_SCROLLLOCK]     = keycode_function(20);

	map[SDL_SCANCODE_NUMLOCKCLEAR]   = keycode_numpad(0, 0);
	map[SDL_SCANCODE_KP_DIVIDE]      = keycode_numpad(0, 1);
	map[SDL_SCANCODE_KP_MULTIPLY]    = keycode_numpad(0, 2);
	map[SDL_SCANCODE_KP_MINUS]       = keycode_numpad(0, 3);
	map[SDL_SCANCODE_KP_7]           = keycode_numpad(1, 0);
	map[SDL_SCANCODE_KP_8]           = keycode_numpad(1, 1);
	map[SDL_SCANCODE_KP_9]           = keycode_numpad(1, 2);
	map[SDL_SCANCODE_KP_PLUS]        = keycode_numpad(1, 3);
	map[SDL_SCANCODE_KP_4]           = keycode_numpad(2, 0);
	map[SDL_SCANCODE_KP_5]           = keycode_numpad(2, 1);
	map[SDL_SCANCODE_KP_6]           = keycode_numpad(2, 2);
	map[SDL_SCANCODE_KP_1]           = keycode_numpad(3, 0);
	map[SDL_SCANCODE_KP_2]           = keycode_numpad(3, 1);
	map[SDL_SCANCODE_KP_3]           = keycode_numpad(3, 2);
	map[SDL_SCANCODE_KP_ENTER]       = keycode_numpad(3, 3);
	map[SDL_SCANCODE_KP_0]           = keycode_numpad(4, 0);
	map[SDL_SCANCODE_KP_COMMA]       = keycode_numpad(4, 1);
};
#include "../Events.h"
#include "../Keymap.h"
#include "../Platform.h"

#include <BAN/Atomic.h>
#include <BAN/HashMap.h>

#include <SDL3/SDL.h>

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

struct SDLCursor final : public PlatformCursor
{
	~SDLCursor()
	{
		if (cursor != nullptr)
			SDL_DestroyCursor(cursor);
	}

	SDL_Cursor* cursor { nullptr };
};

static int s_eventfd { -1 };

static SDL_Cursor* s_default_cursor { nullptr };

static void* sdl3_thread(void*)
{
	for (;;)
	{
		const uint64_t value { 1 };
		write(s_eventfd, &value, sizeof(value));
		usleep(16'666);
	}

	return nullptr;
}

static void sdl3_initialize_keymap();

static bool sdl3_initialize()
{
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
	{
		dwarnln("Could not initialize SDL: {}", SDL_GetError());
		return false;
	}

	const SDL_DisplayID* display_ids = SDL_GetDisplays(nullptr);
	for (int i = 0; display_ids[i]; i++)
	{
		SDL_Rect rect;
		if (!SDL_GetDisplayBounds(display_ids[i], &rect))
		{
			dwarnln("Could not display {} bounds: {}", SDL_GetError());
			continue;
		}
		register_display(rect.x, rect.y, rect.w, rect.h);
	}

	sdl3_initialize_keymap();

	s_default_cursor = SDL_GetCursor();

	s_eventfd = eventfd(0, 0);
	if (s_eventfd == -1)
	{
		dwarnln("Could not create eventfd: {}", strerror(errno));
		return false;
	}

	register_event_fd(s_eventfd, nullptr);

	pthread_t thread;
	pthread_create(&thread, nullptr, sdl3_thread, nullptr);

	return true;
}

static BAN::ErrorOr<BAN::UniqPtr<PlatformWindow>> sdl3_create_window(WindowType type, WINDOW wid, int32_t x, int32_t y, uint32_t width, uint32_t height)
{
	auto window = TRY(BAN::UniqPtr<SDLWindow>::create());

	window->wid = wid;
	window->width = width;
	window->height = height;

	int flags;
	switch (type)
	{
		case WindowType::Normal:
			flags = SDL_WINDOW_RESIZABLE;
			break;
		case WindowType::Utility:
			flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_UTILITY;
			break;
		case WindowType::Popup:
			flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_UTILITY;
			break;
	}

	flags |= SDL_WINDOW_TRANSPARENT;

	window->window = SDL_CreateWindow("", width, height, flags);

	if (window->window == nullptr)
	{
		dwarnln("Could not create SDL window: {}", SDL_GetError());
		return BAN::Error::from_errno(EFAULT);
	}

	if (x != 0 || y != 0)
		SDL_SetWindowPosition(window->window, x, y);

	window->renderer = SDL_CreateRenderer(window->window, nullptr);
	if (window->renderer == nullptr)
	{
		dwarnln("Could not create SDL renderer: {}", SDL_GetError());
		return BAN::Error::from_errno(EFAULT);
	}

	SDL_SetRenderDrawColor(window->renderer, 0, 0, 0, 0);

	window->texture = SDL_CreateTexture(window->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
	if (window->texture == nullptr)
	{
		dwarnln("Could not create SDL texture: {}", SDL_GetError());
		return BAN::Error::from_errno(EFAULT);
	}

	TRY(s_window_map.insert(SDL_GetWindowID(window->window), window.ptr()));

	return BAN::UniqPtr<PlatformWindow>(BAN::move(window));
}

static void sdl3_request_resize(PlatformWindow* window, uint32_t width, uint32_t height)
{
	auto& sdl_window = *static_cast<SDLWindow*>(window);
	SDL_SetWindowSize(sdl_window.window, width, height);
}

static void sdl3_request_reposition(PlatformWindow* window, int32_t x, int32_t y)
{
	auto& sdl_window = *static_cast<SDLWindow*>(window);
	SDL_SetWindowPosition(sdl_window.window, x, y);
}

static void sdl3_poll_events(void*)
{
	uint64_t dummy;
	read(s_eventfd, &dummy, sizeof(dummy));	

	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				if (auto it = s_window_map.find(event.window.windowID); it != s_window_map.end())
					on_window_close_event(it->value->wid);
				break;
			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
				if (auto it = s_window_map.find(event.window.windowID); it != s_window_map.end())
				{
					auto& window = *it->value;
					window.width  = event.window.data1;
					window.height = event.window.data2;

					SDL_DestroyTexture(window.texture);
					window.texture = SDL_CreateTexture(window.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, window.width, window.height);
					ASSERT(window.texture);

					on_window_resize_event(window.wid, window.width, window.height);
				}
				break;
			case SDL_EVENT_WINDOW_MOVED:
				if (auto it = s_window_map.find(event.motion.windowID); it != s_window_map.end())
					on_window_move_event(it->value->wid, event.window.data1, event.window.data2);
				break;
			case SDL_EVENT_WINDOW_FOCUS_GAINED:
			case SDL_EVENT_WINDOW_FOCUS_LOST:
				if (auto it = s_window_map.find(event.motion.windowID); it != s_window_map.end())
					on_window_focus_event(it->value->wid, (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED));
				break;
			case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
			case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
				if (auto it = s_window_map.find(event.motion.windowID); it != s_window_map.end())
					on_window_fullscreen_event(it->value->wid, (event.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN));
				break;
			case SDL_EVENT_WINDOW_MOUSE_LEAVE:
				if (auto it = s_window_map.find(event.motion.windowID); it != s_window_map.end())
					on_window_leave_event(it->value->wid);
				break;
			case SDL_EVENT_MOUSE_MOTION:
				if (auto it = s_window_map.find(event.motion.windowID); it != s_window_map.end())
					on_mouse_move_event(it->value->wid, event.motion.x, event.motion.y);
				break;
			case SDL_EVENT_MOUSE_BUTTON_UP:
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
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
					on_mouse_button_event(it->value->wid, xbutton, (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN));
				break;
			}
			case SDL_EVENT_MOUSE_WHEEL:
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
			case SDL_EVENT_KEY_UP:
			case SDL_EVENT_KEY_DOWN:
			{
				const uint8_t xmod =
					((event.key.mod & SDL_KMOD_SHIFT)  ? (1 << 0) : 0) |
					((event.key.mod & SDL_KMOD_CAPS)   ? (1 << 1) : 0) |
					((event.key.mod & SDL_KMOD_CTRL)   ? (1 << 2) : 0) |
					((event.key.mod & SDL_KMOD_ALT)    ? (1 << 3) : 0) |
					((event.key.mod & SDL_KMOD_NUM)    ? (1 << 4) : 0) |
					((event.key.mod & SDL_KMOD_LEVEL5) ? (1 << 5) : 0) |
					((event.key.mod & SDL_KMOD_GUI)    ? (1 << 6) : 0) |
					((event.key.mod & SDL_KMOD_MODE)   ? (1 << 7) : 0);

				auto it = s_window_map.find(event.window.windowID);
				if (it != s_window_map.end())
					on_key_event(it->value->wid, event.key.scancode, xmod, (event.type == SDL_EVENT_KEY_DOWN));
				break;
			}
			case SDL_EVENT_KEYMAP_CHANGED:
				sdl3_initialize_keymap();
				on_keymap_changed();
				break;
		}
	}
}

static void sdl3_invalidate(PlatformWindow* window, const void* src_pixels, uint32_t src_pitch, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
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
	if (!SDL_LockTexture(sdl_window.texture, &rect, &dst_pixels, &dst_pitch))
	{
		dwarnln("Could not lock texture: {}", SDL_GetError());
		return;
	}

	for (int32_t y_off = 0; y_off < rect.h; y_off++)
	{
		memcpy(
			static_cast<      uint8_t*>(dst_pixels) + y_off * dst_pitch,
			static_cast<const uint8_t*>(src_pixels) + y_off * src_pitch,
			rect.w * sizeof(uint32_t)
		);
	}

	SDL_UnlockTexture(sdl_window.texture);
}

static void sdl3_end_frame(PlatformWindow* window)
{
	auto& sdl_window = *static_cast<SDLWindow*>(window);
	SDL_RenderClear(sdl_window.renderer);
	SDL_RenderTexture(sdl_window.renderer, sdl_window.texture, nullptr, nullptr);
	SDL_RenderPresent(sdl_window.renderer);
}

static void sdl3_request_fullscreen(PlatformWindow* window, bool fullscreen)
{
	auto& sdl_window = *static_cast<SDLWindow*>(window);
	SDL_SetWindowFullscreen(sdl_window.window, fullscreen);
}

static void sdl3_warp_pointer(int32_t x, int32_t y, bool relative)
{
	if (relative)
	{
		float cx, cy;
		SDL_GetGlobalMouseState(&cx, &cy);
		x += cx;
		x += cy;
	}

	SDL_WarpMouseGlobal(x, y);
}

static void sdl3_query_pointer(int32_t* x, int32_t* y)
{
	float cx, cy;
	SDL_GetGlobalMouseState(&cx, &cy);
	*x = cx;
	*y = cy;
}

static void sdl3_set_pointer_grab(PlatformWindow* window, bool grabbed)
{
	auto& sdl_window = *static_cast<SDLWindow*>(window);
	SDL_SetWindowMouseGrab(sdl_window.window, grabbed);
}

static BAN::ErrorOr<BAN::UniqPtr<PlatformCursor>> sdl3_create_system_cursor(SystemCursorType type)
{
	static constexpr SDL_SystemCursor cursor_type_map[] {
		[static_cast<size_t>(SystemCursorType::Pointer)]          = SDL_SYSTEM_CURSOR_DEFAULT,
		[static_cast<size_t>(SystemCursorType::Text)]             = SDL_SYSTEM_CURSOR_TEXT,
		[static_cast<size_t>(SystemCursorType::Wait)]             = SDL_SYSTEM_CURSOR_WAIT,
		[static_cast<size_t>(SystemCursorType::Hand)]             = SDL_SYSTEM_CURSOR_POINTER,
		[static_cast<size_t>(SystemCursorType::Help)]             = SDL_SYSTEM_CURSOR_DEFAULT, // :(
		[static_cast<size_t>(SystemCursorType::Move)]             = SDL_SYSTEM_CURSOR_MOVE,
		[static_cast<size_t>(SystemCursorType::Forbidden)]        = SDL_SYSTEM_CURSOR_NOT_ALLOWED,
		[static_cast<size_t>(SystemCursorType::ResizeN)]          = SDL_SYSTEM_CURSOR_N_RESIZE,
		[static_cast<size_t>(SystemCursorType::ResizeE)]          = SDL_SYSTEM_CURSOR_E_RESIZE,
		[static_cast<size_t>(SystemCursorType::ResizeS)]          = SDL_SYSTEM_CURSOR_S_RESIZE,
		[static_cast<size_t>(SystemCursorType::ResizeW)]          = SDL_SYSTEM_CURSOR_W_RESIZE,
		[static_cast<size_t>(SystemCursorType::ResizeNW)]         = SDL_SYSTEM_CURSOR_NW_RESIZE,
		[static_cast<size_t>(SystemCursorType::ResizeNE)]         = SDL_SYSTEM_CURSOR_NE_RESIZE,
		[static_cast<size_t>(SystemCursorType::ResizeSW)]         = SDL_SYSTEM_CURSOR_SW_RESIZE,
		[static_cast<size_t>(SystemCursorType::ResizeSE)]         = SDL_SYSTEM_CURSOR_SE_RESIZE,
		[static_cast<size_t>(SystemCursorType::ResizeVertical)]   = SDL_SYSTEM_CURSOR_NS_RESIZE,
		[static_cast<size_t>(SystemCursorType::ResizeHorizontal)] = SDL_SYSTEM_CURSOR_EW_RESIZE,
	};

	auto cursor = TRY(BAN::UniqPtr<SDLCursor>::create());

	cursor->cursor = SDL_CreateSystemCursor(cursor_type_map[static_cast<size_t>(type)]);
	if (cursor->cursor == nullptr)
	{
		dwarnln("Could not create SDL system cursor: {}", SDL_GetError());
		return BAN::Error::from_errno(EFAULT);
	}

	return BAN::UniqPtr<PlatformCursor>(BAN::move(cursor));
}

static BAN::ErrorOr<BAN::UniqPtr<PlatformCursor>> sdl3_create_bitmap_cursor(const uint32_t* pixels, uint32_t width, uint32_t height, int32_t origin_x, int32_t origin_y)
{
	auto cursor = TRY(BAN::UniqPtr<SDLCursor>::create());

	SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_ARGB8888, const_cast<uint32_t*>(pixels), width * 4);
	if (surface == nullptr)
	{
		dwarnln("Could not create SDL surface for cursor: {}", SDL_GetError());
		return BAN::Error::from_errno(EFAULT);
	}

	origin_x = BAN::Math::clamp<int32_t>(origin_x, 0, width  - 1);
	origin_y = BAN::Math::clamp<int32_t>(origin_y, 0, height - 1);
	cursor->cursor = SDL_CreateColorCursor(surface, origin_x, origin_y);

	SDL_DestroySurface(surface);

	if (cursor->cursor == nullptr)
	{
		dwarnln("Could not create SDL color cursor: {}", SDL_GetError());
		return BAN::Error::from_errno(EFAULT);
	}

	return BAN::UniqPtr<PlatformCursor>(BAN::move(cursor));
}

static void sdl3_set_cursor(PlatformWindow*, PlatformCursor* cursor)
{
	if (cursor == nullptr)
		SDL_SetCursor(s_default_cursor);
	else
	{
		auto& sdl_cursor = *static_cast<SDLCursor*>(cursor);
		SDL_SetCursor(sdl_cursor.cursor);
	}
}

PlatformOps g_platform_ops = {
	.initialize           = sdl3_initialize,
	.poll_events          = sdl3_poll_events,
	.create_window        = sdl3_create_window,
	.invalidate           = sdl3_invalidate,
	.begin_frame          = nullptr,
	.end_frame            = sdl3_end_frame,
	.request_resize       = sdl3_request_resize,
	.request_reposition   = sdl3_request_reposition,
	.request_fullscreen   = sdl3_request_fullscreen,
	.warp_pointer         = sdl3_warp_pointer,
	.query_pointer        = sdl3_query_pointer,
	.set_pointer_grab     = sdl3_set_pointer_grab,
	.create_system_cursor = sdl3_create_system_cursor,
	.create_bitmap_cursor = sdl3_create_bitmap_cursor,
	.set_cursor           = sdl3_set_cursor,
};

static uint32_t sdl3_keycode_to_x_keysym(SDL_Keycode keycode);

static void sdl3_initialize_keymap()
{
	static constexpr SDL_Keymod modifier_map[] {
		SDL_KMOD_NONE,
		SDL_KMOD_SHIFT,
		SDL_KMOD_MODE,
		SDL_KMOD_MODE | SDL_KMOD_SHIFT,
	};

	memset(g_keymap,       0, sizeof(g_keymap));
	memset(g_modifier_map, 0, sizeof(g_modifier_map));

	for (size_t scancode = 0; scancode < g_keymap_size; scancode++)
		for (size_t layer = 0; layer < g_keymap_layers; layer++)
			if (const auto sdl_key = SDL_GetKeyFromScancode(static_cast<SDL_Scancode>(scancode), modifier_map[layer], false); sdl_key != SDLK_UNKNOWN)
				g_keymap[scancode][layer] = sdl3_keycode_to_x_keysym(sdl_key);

	const auto get_scancode = [](SDL_Keycode keycode) -> uint8_t {
		const auto scancode = SDL_GetScancodeFromKey(keycode, nullptr);;
		if (scancode != SDL_SCANCODE_UNKNOWN && scancode < g_keymap_size)
			return scancode + g_keymap_min_keycode;
		return 0;
	};

	g_modifier_map[0][0] = get_scancode(SDLK_LSHIFT);
	g_modifier_map[0][1] = get_scancode(SDLK_RSHIFT);

	g_modifier_map[1][0] = get_scancode(SDLK_CAPSLOCK);

	g_modifier_map[2][0] = get_scancode(SDLK_LCTRL);
	g_modifier_map[2][1] = get_scancode(SDLK_RCTRL);

	g_modifier_map[3][0] = get_scancode(SDLK_LALT);
	g_modifier_map[3][1] = get_scancode(SDLK_RALT);

	g_modifier_map[4][0] = get_scancode(SDLK_NUMLOCKCLEAR);

	g_modifier_map[5][0] = get_scancode(SDLK_LEVEL5_SHIFT);

	g_modifier_map[6][0] = get_scancode(SDLK_LGUI);
	g_modifier_map[6][1] = get_scancode(SDLK_RGUI);

	g_modifier_map[7][0] = get_scancode(SDLK_MODE);
}

#include <X11/keysym.h>
#include <X11/XF86keysym.h>

#include <wctype.h>

static uint32_t sdl3_keycode_to_x_keysym(SDL_Keycode keycode)
{
	if (iswprint(keycode))
	{
		if (keycode >= 0x100)
			keycode |= 0x01000000;
		return keycode;
	}

	switch (keycode)
	{
		case SDLK_RETURN:               return XK_Return;
		case SDLK_ESCAPE:               return XK_Escape;
		case SDLK_BACKSPACE:            return XK_BackSpace;
		case SDLK_TAB:                  return XK_Tab;
		case SDLK_DELETE:               return XK_Delete;
		case SDLK_CAPSLOCK:             return XK_Caps_Lock;
		case SDLK_F1:                   return XK_F1;
		case SDLK_F2:                   return XK_F2;
		case SDLK_F3:                   return XK_F3;
		case SDLK_F4:                   return XK_F4;
		case SDLK_F5:                   return XK_F5;
		case SDLK_F6:                   return XK_F6;
		case SDLK_F7:                   return XK_F7;
		case SDLK_F8:                   return XK_F8;
		case SDLK_F9:                   return XK_F9;
		case SDLK_F10:                  return XK_F10;
		case SDLK_F11:                  return XK_F11;
		case SDLK_F12:                  return XK_F12;
		case SDLK_PRINTSCREEN:          return XK_Print;
		case SDLK_SCROLLLOCK:           return XK_Scroll_Lock;
		case SDLK_PAUSE:                return XK_Pause;
		case SDLK_INSERT:               return XK_Insert;
		case SDLK_HOME:                 return XK_Home;
		case SDLK_PAGEUP:               return XK_Page_Up;
		case SDLK_END:                  return XK_End;
		case SDLK_PAGEDOWN:             return XK_Page_Down;
		case SDLK_RIGHT:                return XK_Right;
		case SDLK_LEFT:                 return XK_Left;
		case SDLK_DOWN:                 return XK_Down;
		case SDLK_UP:                   return XK_Up;
		case SDLK_NUMLOCKCLEAR:         return XK_Num_Lock;
		case SDLK_KP_DIVIDE:            return XK_KP_Divide;
		case SDLK_KP_MULTIPLY:          return XK_KP_Multiply;
		case SDLK_KP_MINUS:             return XK_KP_Subtract;
		case SDLK_KP_PLUS:              return XK_KP_Add;
		case SDLK_KP_ENTER:             return XK_KP_Enter;
		case SDLK_KP_1:                 return XK_KP_1;
		case SDLK_KP_2:                 return XK_KP_2;
		case SDLK_KP_3:                 return XK_KP_3;
		case SDLK_KP_4:                 return XK_KP_4;
		case SDLK_KP_5:                 return XK_KP_5;
		case SDLK_KP_6:                 return XK_KP_6;
		case SDLK_KP_7:                 return XK_KP_7;
		case SDLK_KP_8:                 return XK_KP_8;
		case SDLK_KP_9:                 return XK_KP_9;
		case SDLK_KP_0:                 return XK_KP_0;
		case SDLK_KP_PERIOD:            return XK_KP_Decimal;
		case SDLK_APPLICATION:          return XF86XK_ApplicationLeft;
		case SDLK_POWER:                return XF86XK_PowerOff;
		case SDLK_KP_EQUALS:            return XK_KP_Equal;
		case SDLK_F13:                  return XK_F13;
		case SDLK_F14:                  return XK_F14;
		case SDLK_F15:                  return XK_F15;
		case SDLK_F16:                  return XK_F16;
		case SDLK_F17:                  return XK_F17;
		case SDLK_F18:                  return XK_F18;
		case SDLK_F19:                  return XK_F19;
		case SDLK_F20:                  return XK_F20;
		case SDLK_F21:                  return XK_F21;
		case SDLK_F22:                  return XK_F22;
		case SDLK_F23:                  return XK_F23;
		case SDLK_F24:                  return XK_F24;
		case SDLK_EXECUTE:              return XK_Execute;
		case SDLK_HELP:                 return XK_Help;
		case SDLK_MENU:                 return XK_Menu;
		case SDLK_SELECT:               return XK_Select;
		case SDLK_STOP:                 return XF86XK_Stop;
		case SDLK_AGAIN:                return XK_Redo;
		case SDLK_UNDO:                 return XK_Undo;
		case SDLK_CUT:                  return XF86XK_Cut;
		case SDLK_COPY:                 return XF86XK_Copy;
		case SDLK_PASTE:                return XF86XK_Paste;
		case SDLK_FIND:                 return XK_Find;
		case SDLK_MUTE:                 return XF86XK_AudioMute;
		case SDLK_VOLUMEUP:             return XF86XK_AudioRaiseVolume;
		case SDLK_VOLUMEDOWN:           return XF86XK_AudioLowerVolume;
		case SDLK_KP_COMMA:             return XK_KP_Separator;
		//case SDLK_KP_EQUALSAS400:       return ;
		//case SDLK_ALTERASE:             return ;
		case SDLK_SYSREQ:               return XK_Sys_Req;
		case SDLK_CANCEL:               return XK_Cancel;
		case SDLK_CLEAR:                return XK_Clear;
		case SDLK_PRIOR:                return XK_Prior;
		case SDLK_RETURN2:              return XK_Return;
		//case SDLK_SEPARATOR:            return ;
		//case SDLK_OUT:                  return ;
		//case SDLK_OPER:                 return ;
		//case SDLK_CLEARAGAIN:           return ;
		//case SDLK_CRSEL:                return ;
		//case SDLK_EXSEL:                return ;
		//case SDLK_KP_00:                return ;
		//case SDLK_KP_000:               return ;
		//case SDLK_THOUSANDSSEPARATOR:   return ;
		//case SDLK_DECIMALSEPARATOR:     return ;
		//case SDLK_CURRENCYUNIT:         return ;
		//case SDLK_CURRENCYSUBUNIT:      return ;
		//case SDLK_KP_LEFTPAREN:         return ;
		//case SDLK_KP_RIGHTPAREN:        return ;
		//case SDLK_KP_LEFTBRACE:         return ;
		//case SDLK_KP_RIGHTBRACE:        return ;
		case SDLK_KP_TAB:               return XK_KP_Tab;
		//case SDLK_KP_BACKSPACE:         return ;
		//case SDLK_KP_A:                 return ;
		//case SDLK_KP_B:                 return ;
		//case SDLK_KP_C:                 return ;
		//case SDLK_KP_D:                 return ;
		//case SDLK_KP_E:                 return ;
		//case SDLK_KP_F:                 return ;
		//case SDLK_KP_XOR:               return ;
		//case SDLK_KP_POWER:             return ;
		//case SDLK_KP_PERCENT:           return ;
		//case SDLK_KP_LESS:              return ;
		//case SDLK_KP_GREATER:           return ;
		//case SDLK_KP_AMPERSAND:         return ;
		//case SDLK_KP_DBLAMPERSAND:      return ;
		//case SDLK_KP_VERTICALBAR:       return ;
		//case SDLK_KP_DBLVERTICALBAR:    return ;
		//case SDLK_KP_COLON:             return ;
		//case SDLK_KP_HASH:              return ;
		//case SDLK_KP_SPACE:             return ;
		//case SDLK_KP_AT:                return ;
		//case SDLK_KP_EXCLAM:            return ;
		//case SDLK_KP_MEMSTORE:          return ;
		//case SDLK_KP_MEMRECALL:         return ;
		//case SDLK_KP_MEMCLEAR:          return ;
		//case SDLK_KP_MEMADD:            return ;
		//case SDLK_KP_MEMSUBTRACT:       return ;
		//case SDLK_KP_MEMMULTIPLY:       return ;
		//case SDLK_KP_MEMDIVIDE:         return ;
		//case SDLK_KP_PLUSMINUS:         return ;
		//case SDLK_KP_CLEAR:             return ;
		//case SDLK_KP_CLEARENTRY:        return ;
		//case SDLK_KP_BINARY:            return ;
		//case SDLK_KP_OCTAL:             return ;
		//case SDLK_KP_DECIMAL:           return ;
		//case SDLK_KP_HEXADECIMAL:       return ;
		case SDLK_LCTRL:                return XK_Control_L;
		case SDLK_LSHIFT:               return XK_Shift_L;
		case SDLK_LALT:                 return XK_Alt_L;
		case SDLK_LGUI:                 return XK_Super_L;
		case SDLK_RCTRL:                return XK_Control_R;
		case SDLK_RSHIFT:               return XK_Shift_R;
		case SDLK_RALT:                 return XK_Alt_R;
		case SDLK_RGUI:                 return XK_Super_R;
		case SDLK_MODE:                 return XK_Mode_switch;
		case SDLK_SLEEP:                return XF86XK_Sleep;
		case SDLK_WAKE:                 return XF86XK_WakeUp;
		//case SDLK_CHANNEL_INCREMENT:    return XF86XK_ChannelUp;
		//case SDLK_CHANNEL_DECREMENT:    return XF86XK_ChannelDown;
		case SDLK_MEDIA_PLAY:           return XF86XK_AudioPlay;
		case SDLK_MEDIA_PAUSE:          return XF86XK_AudioPause;
		case SDLK_MEDIA_RECORD:         return XF86XK_AudioRecord;
		case SDLK_MEDIA_FAST_FORWARD:   return XF86XK_AudioForward;
		case SDLK_MEDIA_REWIND:         return XF86XK_AudioRewind;
		case SDLK_MEDIA_NEXT_TRACK:     return XF86XK_AudioNext;
		case SDLK_MEDIA_PREVIOUS_TRACK: return XF86XK_AudioPrev;
		case SDLK_MEDIA_STOP:           return XF86XK_AudioStop;
		case SDLK_MEDIA_EJECT:          return XF86XK_Eject;
		//case SDLK_MEDIA_PLAY_PAUSE:     return XF86XK_MediaPlayPause;
		case SDLK_MEDIA_SELECT:         return XF86XK_AudioMedia;
		case SDLK_AC_NEW:               return XF86XK_New;
		case SDLK_AC_OPEN:              return XF86XK_Open;
		case SDLK_AC_CLOSE:             return XF86XK_Close;
		//case SDLK_AC_EXIT:              return XF86XK_Exit;
		case SDLK_AC_SAVE:              return XF86XK_Save;
		case SDLK_AC_PRINT:             return XK_Print;
		//case SDLK_AC_PROPERTIES:        return ;
		case SDLK_AC_SEARCH:            return XF86XK_Search;
		case SDLK_AC_HOME:              return XF86XK_HomePage; // ?
		case SDLK_AC_BACK:              return XF86XK_Back;
		case SDLK_AC_FORWARD:           return XF86XK_Forward;
		case SDLK_AC_STOP:              return XF86XK_Stop;
		case SDLK_AC_REFRESH:           return XF86XK_Refresh;
		case SDLK_AC_BOOKMARKS:         return XF86XK_Book; // ?
		//case SDLK_SOFTLEFT:             return ;
		//case SDLK_SOFTRIGHT:            return ;
		//case SDLK_CALL:                 return ;
		//case SDLK_ENDCALL:              return ;
		case SDLK_LEFT_TAB:             return XK_ISO_Left_Tab;
		case SDLK_LEVEL5_SHIFT:         return XK_ISO_Level5_Shift;
		case SDLK_MULTI_KEY_COMPOSE:    return XK_Multi_key;
		case SDLK_LMETA:                return XK_Meta_L;
		case SDLK_RMETA:                return XK_Meta_R;
		case SDLK_LHYPER:               return XK_Hyper_L;
		case SDLK_RHYPER:               return XK_Hyper_R;
	}

	return 0;
}

#include "LibInput/KeyEvent.h"
#include "LibInput/MouseEvent.h"
#include "WindowServer.h"

#include <BAN/Debug.h>
#include <BAN/ScopeGuard.h>

#include <LibGUI/Window.h>
#include <LibInput/KeyboardLayout.h>

#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <SDL2/SDL.h>

extern SDL_Renderer* g_renderer;
extern SDL_Surface* g_surface;
extern SDL_Texture* g_texture;
extern SDL_Window* g_window;

struct Keymap
{
	consteval Keymap()
	{
		for (auto& scancode : map)
			scancode = 0;

		using LibInput::keycode_normal;
		using LibInput::keycode_function;
		using LibInput::keycode_numpad;

		map[SDL_SCANCODE_GRAVE]     = keycode_normal(0,  0);
		map[SDL_SCANCODE_1]         = keycode_normal(0,  1);
		map[SDL_SCANCODE_2]         = keycode_normal(0,  2);
		map[SDL_SCANCODE_3]         = keycode_normal(0,  3);
		map[SDL_SCANCODE_4]         = keycode_normal(0,  4);
		map[SDL_SCANCODE_5]         = keycode_normal(0,  5);
		map[SDL_SCANCODE_6]         = keycode_normal(0,  6);
		map[SDL_SCANCODE_7]         = keycode_normal(0,  7);
		map[SDL_SCANCODE_8]         = keycode_normal(0,  8);
		map[SDL_SCANCODE_9]         = keycode_normal(0,  9);
		map[SDL_SCANCODE_0]         = keycode_normal(0, 10);
		map[SDL_SCANCODE_MINUS]     = keycode_normal(0, 11);
		map[SDL_SCANCODE_EQUALS]    = keycode_normal(0, 12);
		map[SDL_SCANCODE_BACKSPACE] = keycode_normal(0, 13);

		map[SDL_SCANCODE_TAB]          = keycode_normal(1,  0);
		map[SDL_SCANCODE_Q]            = keycode_normal(1,  1);
		map[SDL_SCANCODE_W]            = keycode_normal(1,  2);
		map[SDL_SCANCODE_E]            = keycode_normal(1,  3);
		map[SDL_SCANCODE_R]            = keycode_normal(1,  4);
		map[SDL_SCANCODE_T]            = keycode_normal(1,  5);
		map[SDL_SCANCODE_Y]            = keycode_normal(1,  6);
		map[SDL_SCANCODE_U]            = keycode_normal(1,  7);
		map[SDL_SCANCODE_I]            = keycode_normal(1,  8);
		map[SDL_SCANCODE_O]            = keycode_normal(1,  9);
		map[SDL_SCANCODE_P]            = keycode_normal(1, 10);
		map[SDL_SCANCODE_LEFTBRACKET]  = keycode_normal(1, 11);
		map[SDL_SCANCODE_RIGHTBRACKET] = keycode_normal(1, 12);

		map[SDL_SCANCODE_CAPSLOCK]   = keycode_normal(2,  0);
		map[SDL_SCANCODE_A]          = keycode_normal(2,  1);
		map[SDL_SCANCODE_S]          = keycode_normal(2,  2);
		map[SDL_SCANCODE_D]          = keycode_normal(2,  3);
		map[SDL_SCANCODE_F]          = keycode_normal(2,  4);
		map[SDL_SCANCODE_G]          = keycode_normal(2,  5);
		map[SDL_SCANCODE_H]          = keycode_normal(2,  6);
		map[SDL_SCANCODE_J]          = keycode_normal(2,  7);
		map[SDL_SCANCODE_K]          = keycode_normal(2,  8);
		map[SDL_SCANCODE_L]          = keycode_normal(2,  9);
		map[SDL_SCANCODE_SEMICOLON]  = keycode_normal(2, 10);
		map[SDL_SCANCODE_APOSTROPHE] = keycode_normal(2, 11);
		map[SDL_SCANCODE_BACKSLASH]  = keycode_normal(2, 12);
		map[SDL_SCANCODE_RETURN]     = keycode_normal(2, 13);

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

		map[SDL_SCANCODE_LCTRL] = keycode_normal(4, 0);
		map[SDL_SCANCODE_LGUI]  = keycode_normal(4, 1);
		map[SDL_SCANCODE_LALT]  = keycode_normal(4, 2);
		map[SDL_SCANCODE_SPACE] = keycode_normal(4, 3);
		map[SDL_SCANCODE_RALT]  = keycode_normal(4, 4);
		map[SDL_SCANCODE_RCTRL] = keycode_normal(4, 5);

		map[SDL_SCANCODE_UP]    = keycode_normal(5, 0);
		map[SDL_SCANCODE_LEFT]  = keycode_normal(5, 1);
		map[SDL_SCANCODE_DOWN]  = keycode_normal(5, 2);
		map[SDL_SCANCODE_RIGHT] = keycode_normal(5, 3);

		map[SDL_SCANCODE_ESCAPE]      = keycode_function(0);
		map[SDL_SCANCODE_F1]          = keycode_function(1);
		map[SDL_SCANCODE_F2]          = keycode_function(2);
		map[SDL_SCANCODE_F3]          = keycode_function(3);
		map[SDL_SCANCODE_F4]          = keycode_function(4);
		map[SDL_SCANCODE_F5]          = keycode_function(5);
		map[SDL_SCANCODE_F6]          = keycode_function(6);
		map[SDL_SCANCODE_F7]          = keycode_function(7);
		map[SDL_SCANCODE_F8]          = keycode_function(8);
		map[SDL_SCANCODE_F9]          = keycode_function(9);
		map[SDL_SCANCODE_F10]         = keycode_function(10);
		map[SDL_SCANCODE_F11]         = keycode_function(11);
		map[SDL_SCANCODE_F12]         = keycode_function(12);
		map[SDL_SCANCODE_INSERT]      = keycode_function(13);
		map[SDL_SCANCODE_PRINTSCREEN] = keycode_function(14);
		map[SDL_SCANCODE_DELETE]      = keycode_function(15);
		map[SDL_SCANCODE_HOME]        = keycode_function(16);
		map[SDL_SCANCODE_END]         = keycode_function(17);
		map[SDL_SCANCODE_PAGEUP]      = keycode_function(18);
		map[SDL_SCANCODE_PAGEDOWN]    = keycode_function(19);
		map[SDL_SCANCODE_SCROLLLOCK]  = keycode_function(20);

		map[SDL_SCANCODE_NUMLOCKCLEAR] = keycode_numpad(0, 0);
		map[SDL_SCANCODE_KP_DIVIDE]    = keycode_numpad(0, 1);
		map[SDL_SCANCODE_KP_MULTIPLY]  = keycode_numpad(0, 2);
		map[SDL_SCANCODE_KP_MINUS]     = keycode_numpad(0, 3);
		map[SDL_SCANCODE_KP_7]         = keycode_numpad(1, 0);
		map[SDL_SCANCODE_KP_8]         = keycode_numpad(1, 1);
		map[SDL_SCANCODE_KP_9]         = keycode_numpad(1, 2);
		map[SDL_SCANCODE_KP_PLUS]      = keycode_numpad(1, 3);
		map[SDL_SCANCODE_KP_4]         = keycode_numpad(2, 0);
		map[SDL_SCANCODE_KP_5]         = keycode_numpad(2, 1);
		map[SDL_SCANCODE_KP_6]         = keycode_numpad(2, 2);
		map[SDL_SCANCODE_KP_1]         = keycode_numpad(3, 0);
		map[SDL_SCANCODE_KP_2]         = keycode_numpad(3, 1);
		map[SDL_SCANCODE_KP_3]         = keycode_numpad(3, 2);
		map[SDL_SCANCODE_KP_ENTER]     = keycode_numpad(3, 3);
		map[SDL_SCANCODE_KP_0]         = keycode_numpad(4, 0);
		map[SDL_SCANCODE_KP_COMMA]     = keycode_numpad(4, 1);
	};

	uint8_t map[SDL_NUM_SCANCODES];
};
static Keymap s_keymap;



struct Config
{
	BAN::UniqPtr<LibImage::Image> background_image;
	int32_t corner_radius = 0;
};

BAN::Optional<BAN::String> file_read_line(FILE* file)
{
	BAN::String line;

	char buffer[128];
	while (fgets(buffer, sizeof(buffer), file))
	{
		MUST(line.append(buffer));
		if (line.back() == '\n')
		{
			line.pop_back();
			return BAN::move(line);
		}
	}

	if (line.empty())
		return {};
	return BAN::move(line);
}

Config parse_config()
{
	Config config;

	auto home_env = getenv("HOME");
	if (!home_env)
	{
		dprintln("HOME environment variable not set");
		return config;
	}

	auto config_path = MUST(BAN::String::formatted("{}/.config/WindowServer.conf", home_env));
	FILE* fconfig = fopen(config_path.data(), "r");
	if (!fconfig)
	{
		dprintln("Could not open '{}'", config_path);
		return config;
	}

	BAN::ScopeGuard _([fconfig] { fclose(fconfig); });

	while (true)
	{
		auto line = file_read_line(fconfig);
		if (!line.has_value())
			break;
		if (line->empty())
			continue;

		auto parts = MUST(line->sv().split('='));
		if (parts.size() != 2)
		{
			dwarnln("Invalid config line: {}", line.value());
			break;
		}

		auto variable = parts[0];
		auto value = parts[1];

		if (variable == "bg"_sv)
		{
			auto image = LibImage::Image::load_from_file(value);
			if (image.is_error())
				dwarnln("Could not load image: {}", image.error());
			else
				config.background_image = image.release_value();
		}
		else if (variable == "corner-radius"_sv)
		{
			char* endptr = nullptr;
			long long corner_radius = strtoll(value.data(), &endptr, 0);
			if (corner_radius < 0 || corner_radius == LONG_MAX || corner_radius >= INT32_MAX)
				dwarnln("invalid corner-radius: '{}'", value);
			else
				config.corner_radius = corner_radius;
		}
		else
		{
			dwarnln("Unknown config variable: {}", variable);
			break;
		}
	}

	return config;
}

int open_server_fd()
{
	struct stat st;
	if (stat(LibGUI::s_window_server_socket.data(), &st) != -1)
		unlink(LibGUI::s_window_server_socket.data());

	int server_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (server_fd == -1)
	{
		perror("socket");
		exit(1);
	}

	sockaddr_un server_addr;
	server_addr.sun_family = AF_UNIX;
	strcpy(server_addr.sun_path, LibGUI::s_window_server_socket.data());
	if (bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1)
	{
		perror("bind");
		exit(1);
	}

	if (chmod(LibGUI::s_window_server_socket.data(), 0777) == -1)
	{
		perror("chmod");
		exit(1);
	}

	if (listen(server_fd, SOMAXCONN) == -1)
	{
		perror("listen");
		exit(1);
	}

	return server_fd;
}

int main()
{
	srand(time(nullptr));

	int server_fd = open_server_fd();
	auto framebuffer = open_framebuffer();
	if (framebuffer.bpp != 32)
	{
		dwarnln("Window server requires 32 bpp");
		return 1;
	}

	int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	if (epoll_fd == -1)
	{
		dwarnln("epoll_create1: {}", strerror(errno));
		return 1;
	}

	{
		epoll_event event {
			.events = EPOLLIN,
			.data = { .fd = server_fd },
		};
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1)
		{
			dwarnln("epoll_ctl server: {}", strerror(errno));
			return 1;
		}
	}

	constexpr int non_terminating_signals[] {
		SIGCHLD,
		SIGCONT,
		SIGSTOP,
		SIGTSTP,
		SIGTTIN,
		SIGTTOU,
		SIGWINCH,
	};
	constexpr int ignored_signals[] {
		SIGPIPE,
	};
	for (int sig = 1; sig < _NSIG; sig++)
		signal(sig, exit);
	for (int sig : non_terminating_signals)
		signal(sig, SIG_DFL);
	for (int sig : ignored_signals)
		signal(sig, SIG_IGN);

	MUST(LibInput::KeyboardLayout::initialize());
	MUST(LibInput::KeyboardLayout::get().load_from_file("./us.keymap"_sv));

	dprintln("Window server started");

	auto config = parse_config();

	WindowServer window_server(framebuffer, config.corner_radius);
	if (config.background_image)
		if (auto ret = window_server.set_background_image(BAN::move(config.background_image)); ret.is_error())
			dwarnln("Could not set background image: {}", ret.error());

	const auto get_current_us =
		[]() -> uint64_t
		{
			timespec current_ts;
			clock_gettime(CLOCK_MONOTONIC, &current_ts);
			return (current_ts.tv_sec * 1'000'000) + (current_ts.tv_nsec / 1000);
		};

	constexpr uint64_t sync_interval_us = 1'000'000 / 60;
	uint64_t last_sync_us = get_current_us() - sync_interval_us;
	while (!window_server.is_stopped())
	{
		const auto current_us = get_current_us();
		if (current_us - last_sync_us > sync_interval_us)
		{
			window_server.sync();
			//last_sync_us += sync_interval_us;
			last_sync_us = get_current_us();
		}

		timespec timeout = {};
		if (auto current_us = get_current_us(); current_us - last_sync_us < sync_interval_us)
			timeout.tv_nsec = (sync_interval_us - (current_us - last_sync_us)) * 1000;

		epoll_event events[16];
		int epoll_events = epoll_pwait2(epoll_fd, events, 16, &timeout, nullptr);
		if (epoll_events == -1 && errno != EINTR)
		{
			dwarnln("epoll_pwait2: {}", strerror(errno));
			break;
		}

		{
			static int prev_x { 0 };
			static int prev_y { 0 };

			int x, y;
			SDL_GetMouseState(&x, &y);

			if (prev_x != x || prev_y != y)
			{
				int w, h;
				SDL_GetWindowSize(g_window, &w, &h);

				window_server.on_mouse_move_abs({
					.abs_x = x,
					.abs_y = y,
					.min_x = 0,
					.min_y = 0,
					.max_x = w,
					.max_y = h,
				});
			}

			prev_x = x;
			prev_y = y;
		}

		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
				case SDL_QUIT:
					window_server.stop();
					break;
				case SDL_WINDOWEVENT:
					if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
						SDL_ShowCursor(SDL_DISABLE);
					if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
						SDL_ShowCursor(SDL_ENABLE);
					break;
				case SDL_KEYDOWN:
				case SDL_KEYUP:
				{
					uint16_t modifier = 0;
					if (event.key.keysym.mod & KMOD_LSHIFT)
						modifier |= LibInput::KeyEvent::Modifier::LShift;
					if (event.key.keysym.mod & KMOD_RSHIFT)
						modifier |= LibInput::KeyEvent::Modifier::RShift;
					if (event.key.keysym.mod & KMOD_LCTRL)
						modifier |= LibInput::KeyEvent::Modifier::LCtrl;
					if (event.key.keysym.mod & KMOD_RCTRL)
						modifier |= LibInput::KeyEvent::Modifier::RCtrl;
					if (event.key.keysym.mod & KMOD_LALT)
						modifier |= LibInput::KeyEvent::Modifier::LAlt;
					if (event.key.keysym.mod & KMOD_RALT)
						modifier |= LibInput::KeyEvent::Modifier::RAlt;
					if (event.key.keysym.mod & KMOD_NUM)
						modifier |= LibInput::KeyEvent::Modifier::NumLock;
					if (event.key.keysym.mod & KMOD_SCROLL)
						modifier |= LibInput::KeyEvent::Modifier::ScrollLock;
					if (event.key.keysym.mod & KMOD_CAPS)
						modifier |= LibInput::KeyEvent::Modifier::CapsLock;
					if (event.key.state == SDL_PRESSED)
						modifier |= LibInput::KeyEvent::Modifier::Pressed;

					window_server.on_key_event(LibInput::KeyboardLayout::get().key_event_from_raw({
						.modifier = modifier,
						.keycode = s_keymap.map[event.key.keysym.scancode],
					}));

					break;
				}
				case SDL_MOUSEBUTTONDOWN:
				case SDL_MOUSEBUTTONUP:
				{
					LibInput::MouseButton button_map[] {
						[0] = LibInput::MouseButton::Left,
						[SDL_BUTTON_LEFT] = LibInput::MouseButton::Left,
						[SDL_BUTTON_MIDDLE] = LibInput::MouseButton::Middle,
						[SDL_BUTTON_RIGHT] = LibInput::MouseButton::Right,
						[SDL_BUTTON_X1] = LibInput::MouseButton::Extra1,
						[SDL_BUTTON_X2] = LibInput::MouseButton::Extra2,
					};

					window_server.on_mouse_button({
						.button = button_map[event.button.button],
						.pressed = (event.button.state == SDL_PRESSED),
					});

					break;
				}
				case SDL_MOUSEWHEEL:
				{
					window_server.on_mouse_scroll({
						.scroll = event.wheel.y,
					});

					break;
				}
			}
		}

		for (int i = 0; i < epoll_events; i++)
		{
			if (events[i].data.fd == server_fd)
			{
				ASSERT(events[i].events & EPOLLIN);

				int window_fd = accept4(server_fd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
				if (window_fd == -1)
				{
					dwarnln("accept: {}", strerror(errno));
					continue;
				}

				epoll_event event {
					.events = EPOLLIN,
					.data = { .fd = window_fd },
				};
				if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, window_fd, &event) == -1)
				{
					dwarnln("epoll_ctl: {}", strerror(errno));
					close(window_fd);
					continue;
				}

				window_server.add_client_fd(window_fd);
				continue;
			}

			const int client_fd = events[i].data.fd;
			if (events[i].events & EPOLLHUP)
			{
				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
				window_server.remove_client_fd(client_fd);
				continue;
			}

			ASSERT(events[i].events & EPOLLIN);

			auto& client_data = window_server.get_client_data(client_fd);

			if (client_data.packet_buffer.empty())
			{
				uint32_t packet_size;
				const ssize_t nrecv = recv(client_fd, &packet_size, sizeof(uint32_t), 0);
				if (nrecv < 0)
					dwarnln("recv 1: {}", strerror(errno));
				if (nrecv > 0 && nrecv != sizeof(uint32_t))
					dwarnln("could not read packet size with a single recv call, closing connection...");
				if (nrecv != sizeof(uint32_t))
				{
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
					window_server.remove_client_fd(client_fd);
					break;
				}

				if (packet_size < 4)
				{
					dwarnln("client sent invalid packet, closing connection...");
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
					window_server.remove_client_fd(client_fd);
					break;
				}

				// this is a bit harsh, but i don't want to work on skipping streaming packets
				if (client_data.packet_buffer.resize(packet_size).is_error())
				{
					dwarnln("could not allocate memory for client packet, closing connection...");
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
					window_server.remove_client_fd(client_fd);
					break;
				}

				client_data.packet_buffer_nread = 0;
				continue;
			}

			const ssize_t nrecv = recv(
				client_fd,
				client_data.packet_buffer.data() + client_data.packet_buffer_nread,
				client_data.packet_buffer.size() - client_data.packet_buffer_nread,
				0
			);
			if (nrecv < 0)
				dwarnln("recv 2: {}", strerror(errno));
			if (nrecv <= 0)
			{
				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
				window_server.remove_client_fd(client_fd);
				break;
			}

			client_data.packet_buffer_nread += nrecv;
			if (client_data.packet_buffer_nread < client_data.packet_buffer.size())
				continue;

			ASSERT(client_data.packet_buffer.size() >= sizeof(uint32_t));

			switch (*reinterpret_cast<LibGUI::PacketType*>(client_data.packet_buffer.data()))
			{
#define WINDOW_PACKET_CASE(enum, function) \
				case LibGUI::PacketType::enum: \
					if (auto ret = LibGUI::WindowPacket::enum::deserialize(client_data.packet_buffer.span()); !ret.is_error()) \
						window_server.function(client_fd, ret.release_value()); \
					break
				WINDOW_PACKET_CASE(WindowCreate,           on_window_create);
				WINDOW_PACKET_CASE(WindowInvalidate,       on_window_invalidate);
				WINDOW_PACKET_CASE(WindowSetPosition,      on_window_set_position);
				WINDOW_PACKET_CASE(WindowSetAttributes,    on_window_set_attributes);
				WINDOW_PACKET_CASE(WindowSetMouseRelative, on_window_set_mouse_relative);
				WINDOW_PACKET_CASE(WindowSetSize,          on_window_set_size);
				WINDOW_PACKET_CASE(WindowSetMinSize,       on_window_set_min_size);
				WINDOW_PACKET_CASE(WindowSetMaxSize,       on_window_set_max_size);
				WINDOW_PACKET_CASE(WindowSetFullscreen,    on_window_set_fullscreen);
				WINDOW_PACKET_CASE(WindowSetTitle,         on_window_set_title);
				WINDOW_PACKET_CASE(WindowSetCursor,        on_window_set_cursor);
#undef WINDOW_PACKET_CASE
				default:
					dprintln("unhandled packet type: {}", *reinterpret_cast<uint32_t*>(client_data.packet_buffer.data()));
			}

			client_data.packet_buffer.clear();
			client_data.packet_buffer_nread = 0;
		}
	}
}

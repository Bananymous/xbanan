#include "Base.h"
#include "Definitions.h"
#include "Extensions.h"
#include "Utils.h"

#include <X11/X.h>
#include <X11/Xatom.h>

#include <time.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/stat.h>

#define USE_UNIX_SOCKET 0

#if USE_UNIX_SOCKET
#include <sys/un.h>
#else
#include <netinet/in.h>
#endif

const xPixmapFormat g_formats[] {
	{
		.depth = 1,
		.bitsPerPixel = 32,
		.scanLinePad = 32,
	},
	{
		.depth = 4,
		.bitsPerPixel = 32,
		.scanLinePad = 32,
	},
	{
		.depth = 8,
		.bitsPerPixel = 32,
		.scanLinePad = 32,
	},
	{
		.depth = 24,
		.bitsPerPixel = 32,
		.scanLinePad = 32,
	},
	{
		.depth = 32,
		.bitsPerPixel = 32,
		.scanLinePad = 32,
	}
};

const xWindowRoot g_root {
	.windowId = 1,
	.defaultColormap = 0,
	.whitePixel = 0xFFFFFF,
	.blackPixel = 0x000000,
	.currentInputMask = 0,
	.pixWidth = 1280,
	.pixHeight = 800,
	.mmWidth = 1280,
	.mmHeight = 800,
	.minInstalledMaps = 0,
	.maxInstalledMaps = 0,
	.rootVisualID = 0,
	.backingStore = 0,
	.saveUnders = 0,
	.rootDepth = 24,
	.nDepths = 1,
};

const xDepth g_depth {
	.depth = 24,
	.nVisuals = 1,
};

const xVisualType g_visual {
	.visualID = 0,
	.c_class = TrueColor,
	.bitsPerRGB = 8,
	.colormapEntries = 0,
	.redMask = 0xFF0000,
	.greenMask = 0x00FF00,
	.blueMask = 0x0000FF,
};

BAN::HashMap<BAN::String, ATOM> g_atoms_name_to_id;
BAN::HashMap<ATOM, BAN::String> g_atoms_id_to_name;
ATOM g_atom_value = XA_LAST_PREDEFINED + 1;

int g_epoll_fd;
BAN::HashMap<int, EpollThingy> g_epoll_thingies;

BAN::ErrorOr<void> extension_bigrequests(Client& client_info, BAN::ConstByteSpan packet)
{
	switch (packet[1])
	{
		case 0:
		{
			xGenericReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.data00 = (16 << 20) / 4, // 16 MiB
			};
			TRY(encode(client_info.output_buffer, reply));

			client_info.has_bigrequests = true;

			dprintln("client enabled big requests");

			break;
		}
		default:
			dwarnln("invalid BIG-REQUESTS minor opcode {}", packet[1]);
			return BAN::Error::from_errno(EINVAL);
	}

	return {};
}

int main()
{
	for (int sig = 1; sig < NSIG; sig++)
		if (sig != SIGWINCH)
			signal(sig, exit);

	if (mkdir("/tmp/.X11-unix", 01777) == -1 && errno != EEXIST)
	{
		perror("xbanan: mkdir");
		return 1;
	}

#if USE_UNIX_SOCKET
	int server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
#else
	int server_sock = socket(AF_INET, SOCK_STREAM, 0);
#endif
	if (server_sock == -1)
	{
		perror("xbanan: socket");
		return 1;
	}

#if USE_UNIX_SOCKET
	const sockaddr_un addr {
		.sun_family = AF_UNIX,
		.sun_path = "/tmp/.X11-unix/X69"
	};
#else
	const sockaddr_in addr {
		.sin_family = AF_INET,
		.sin_port = htons(6069),
		.sin_addr = { htonl(INADDR_ANY) },
	};
#endif

	{
		int one = 1;
		setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));
	}

	if (bind(server_sock, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == -1)
	{
		perror("xbanan: bind");
		return 1;
	}
#if USE_UNIX_SOCKET
	atexit([] { unlink("/tmp/.X11-unix/X69"); });
#endif

	if (listen(server_sock, SOMAXCONN) == -1)
	{
		perror("xbanan: listen");
		return 1;
	}

	g_epoll_fd = epoll_create1(0);
	if (g_epoll_fd == -1)
	{
		perror("xbanan: epoll_create1");
		return 1;
	}

	epoll_event event { .events = EPOLLIN, .data = { .ptr = nullptr } };
	if (epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, server_sock, &event) == -1)
	{
		perror("xbanan: epoll_ctl");
		return 1;
	}

#define APPEND_ATOM(name) do { \
			MUST(g_atoms_id_to_name.insert(name, #name##_sv)); \
			MUST(g_atoms_name_to_id.insert(#name##_sv, name)); \
		} while (0)
	APPEND_ATOM(XA_PRIMARY);
	APPEND_ATOM(XA_SECONDARY);
	APPEND_ATOM(XA_ARC);
	APPEND_ATOM(XA_ATOM);
	APPEND_ATOM(XA_BITMAP);
	APPEND_ATOM(XA_CARDINAL);
	APPEND_ATOM(XA_COLORMAP);
	APPEND_ATOM(XA_CURSOR);
	APPEND_ATOM(XA_CUT_BUFFER0);
	APPEND_ATOM(XA_CUT_BUFFER1);
	APPEND_ATOM(XA_CUT_BUFFER2);
	APPEND_ATOM(XA_CUT_BUFFER3);
	APPEND_ATOM(XA_CUT_BUFFER4);
	APPEND_ATOM(XA_CUT_BUFFER5);
	APPEND_ATOM(XA_CUT_BUFFER6);
	APPEND_ATOM(XA_CUT_BUFFER7);
	APPEND_ATOM(XA_DRAWABLE);
	APPEND_ATOM(XA_FONT);
	APPEND_ATOM(XA_INTEGER);
	APPEND_ATOM(XA_PIXMAP);
	APPEND_ATOM(XA_POINT);
	APPEND_ATOM(XA_RECTANGLE);
	APPEND_ATOM(XA_RESOURCE_MANAGER);
	APPEND_ATOM(XA_RGB_COLOR_MAP);
	APPEND_ATOM(XA_RGB_BEST_MAP);
	APPEND_ATOM(XA_RGB_BLUE_MAP);
	APPEND_ATOM(XA_RGB_DEFAULT_MAP);
	APPEND_ATOM(XA_RGB_GRAY_MAP);
	APPEND_ATOM(XA_RGB_GREEN_MAP);
	APPEND_ATOM(XA_RGB_RED_MAP);
	APPEND_ATOM(XA_STRING);
	APPEND_ATOM(XA_VISUALID);
	APPEND_ATOM(XA_WINDOW);
	APPEND_ATOM(XA_WM_COMMAND);
	APPEND_ATOM(XA_WM_HINTS);
	APPEND_ATOM(XA_WM_CLIENT_MACHINE);
	APPEND_ATOM(XA_WM_ICON_NAME);
	APPEND_ATOM(XA_WM_ICON_SIZE);
	APPEND_ATOM(XA_WM_NAME);
	APPEND_ATOM(XA_WM_NORMAL_HINTS);
	APPEND_ATOM(XA_WM_SIZE_HINTS);
	APPEND_ATOM(XA_WM_ZOOM_HINTS);
	APPEND_ATOM(XA_MIN_SPACE);
	APPEND_ATOM(XA_NORM_SPACE);
	APPEND_ATOM(XA_MAX_SPACE);
	APPEND_ATOM(XA_END_SPACE);
	APPEND_ATOM(XA_SUPERSCRIPT_X);
	APPEND_ATOM(XA_SUPERSCRIPT_Y);
	APPEND_ATOM(XA_SUBSCRIPT_X);
	APPEND_ATOM(XA_SUBSCRIPT_Y);
	APPEND_ATOM(XA_UNDERLINE_POSITION);
	APPEND_ATOM(XA_UNDERLINE_THICKNESS);
	APPEND_ATOM(XA_STRIKEOUT_ASCENT);
	APPEND_ATOM(XA_STRIKEOUT_DESCENT);
	APPEND_ATOM(XA_ITALIC_ANGLE);
	APPEND_ATOM(XA_X_HEIGHT);
	APPEND_ATOM(XA_QUAD_WIDTH);
	APPEND_ATOM(XA_WEIGHT);
	APPEND_ATOM(XA_POINT_SIZE);
	APPEND_ATOM(XA_RESOLUTION);
	APPEND_ATOM(XA_COPYRIGHT);
	APPEND_ATOM(XA_NOTICE);
	APPEND_ATOM(XA_FONT_NAME);
	APPEND_ATOM(XA_FAMILY_NAME);
	APPEND_ATOM(XA_FULL_NAME);
	APPEND_ATOM(XA_CAP_HEIGHT);
	APPEND_ATOM(XA_WM_CLASS);
	APPEND_ATOM(XA_WM_TRANSIENT_FOR);
#undef APPEND_ATOM

	install_extension("BIG-REQUESTS"_sv, extension_bigrequests);

	printf("xbanan started\n");

	const auto close_client =
		[](int client_fd)
		{
			auto& epoll_thingy = g_epoll_thingies[client_fd];
			ASSERT(epoll_thingy.type == EpollThingy::Type::Client);
			auto& client_info = epoll_thingy.value.get<Client>();

			dprintln("client {} disconnected", client_fd);

			for (auto& [_, object] : client_info.objects)
			{
				if (object->type != Object::Type::Window)
					continue;
				auto& window = object->object.get<Object::Window>();
				if (!window.window.has<BAN::UniqPtr<LibGUI::Window>>())
					continue;
				auto& gui_window = window.window.get<BAN::UniqPtr<LibGUI::Window>>();
				epoll_ctl(g_epoll_fd, EPOLL_CTL_DEL, gui_window->server_fd(), nullptr);
				g_epoll_thingies.remove(gui_window->server_fd());
			}

			epoll_ctl(g_epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
			close(client_fd);
			g_epoll_thingies.remove(client_fd);
		};

	for (;;)
	{
		epoll_event events[16];
		const int event_count = epoll_wait(g_epoll_fd, events, 16, -1);

		for (int i = 0; i < event_count; i++)
		{
			if (events[i].data.ptr == nullptr)
			{
				int client_sock = accept(server_sock, nullptr, nullptr);
				if (client_sock == -1)
				{
					perror("xbanan: accept");
					continue;
				}

				MUST(g_epoll_thingies.insert(client_sock, {
					.type = EpollThingy::Type::Client,
					.value = Client {
						.fd = client_sock,
						.state = Client::State::ConnectionSetup,
					}
				}));

				epoll_event event = { .events = EPOLLIN, .data = { .fd = client_sock } };
				if (epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, client_sock, &event) == -1)
				{
					perror("xbanan: epoll_ctl");
					close(client_sock);
					continue;
				}

				dprintln("client {} connected", client_sock);
				continue;
			}

			auto it = g_epoll_thingies.find(events[i].data.fd);
			if (it == g_epoll_thingies.end())
				continue;
			auto& epoll_thingy = it->value;

			if (epoll_thingy.type == EpollThingy::Type::Window)
			{
				auto* window = epoll_thingy.value.get<LibGUI::Window*>();
				window->poll_events();
				continue;
			}

			ASSERT(epoll_thingy.type == EpollThingy::Type::Client);

			auto& client_info = epoll_thingy.value.get<Client>();

			if (events[i].events & EPOLLHUP)
			{
				close_client(client_info.fd);
				continue;
			}

			if (events[i].events & EPOLLOUT)
			{
				const ssize_t nsend = send(
					client_info.fd,
					client_info.output_buffer.data(),
					client_info.output_buffer.size(),
					0
				);

				if (nsend == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
					goto send_done;

				if (nsend < 0)
					perror("xbanan: send");
				if (nsend <= 0)
				{
					close_client(client_info.fd);
					continue;
				}

				memmove(
					client_info.output_buffer.data(),
					client_info.output_buffer.data() + nsend,
					client_info.output_buffer.size() - nsend
				);
				MUST(client_info.output_buffer.resize(client_info.output_buffer.size() - nsend));

				if (client_info.output_buffer.empty())
				{
					epoll_event event { .events = EPOLLIN, .data = { .fd = client_info.fd } };
					if (epoll_ctl(g_epoll_fd, EPOLL_CTL_MOD, client_info.fd, &event) == -1)
					{
						perror("xbanan: epoll_ctl");
						close_client(client_info.fd);
						continue;
					}

					client_info.has_epollout = false;
				}

			send_done:
				(void)0;
			}

			if (!(events[i].events & EPOLLIN))
				continue;

			switch (client_info.state)
			{
				case Client::State::ConnectionSetup:
				{
					xConnClientPrefix client_prefix;

					const ssize_t nrecv = recv(client_info.fd, &client_prefix, sizeof(client_prefix), 0);
					if (nrecv < 0)
						perror("xbanan: recv");
					if (nrecv <= 0)
					{
						close_client(client_info.fd);
						continue;
					}

					ASSERT(nrecv == sizeof(client_prefix));

					if (auto ret = setup_client_conneciton(client_info, client_prefix); ret.is_error())
					{
						dwarnln("setup_client_connection: {}", ret.error());
						close_client(client_info.fd);
						continue;
					}

					break;
				}
				case Client::State::Connected:
				{
					char buffer[1024] {};

					const ssize_t nrecv = recv(client_info.fd, buffer, sizeof(buffer), 0);
					if (nrecv < 0)
						perror("xbanan: recv");
					if (nrecv <= 0)
					{
						close_client(client_info.fd);
						continue;
					}

					const size_t old_size = client_info.input_buffer.size();
					MUST(client_info.input_buffer.resize(old_size + nrecv));
					memcpy(client_info.input_buffer.data() + old_size, buffer, nrecv);

					for (;;)
					{
						if (client_info.input_buffer.size() < 4)
							break;

						bool is_big_request = false;
						uint32_t byte_length = 4 * *reinterpret_cast<const uint16_t*>(client_info.input_buffer.data() + 2);

						if (byte_length == 0 && client_info.has_bigrequests)
						{
							if (client_info.input_buffer.size() < 8)
								break;
							byte_length = 4 * *reinterpret_cast<const uint32_t*>(client_info.input_buffer.data() + 4);
							is_big_request = true;
						}

						if (client_info.input_buffer.size() < byte_length)
							break;

						auto packet = BAN::ConstByteSpan(client_info.input_buffer.span()).slice(0, byte_length);
						if (is_big_request)
						{
							auto* input_u32 = reinterpret_cast<uint32_t*>(client_info.input_buffer.data());
							input_u32[1] = input_u32[0];
							packet = packet.slice(4);

							dprintln("handling big request");
						}

						if (auto ret = handle_packet(client_info, packet); ret.is_error() && ret.error().get_error_code() != ENOENT)
						{
							dwarnln("handle_packet: {}", ret.error());
							close_client(client_info.fd);
							continue;
						}

						memmove(client_info.input_buffer.data(), client_info.input_buffer.data() + byte_length, client_info.input_buffer.size() - byte_length);
						MUST(client_info.input_buffer.resize(client_info.input_buffer.size() - byte_length));
					}

					break;
				}
			}
		}

	iterator_invalidated:
		for (auto& [_, thingy] : g_epoll_thingies)
		{
			if (thingy.type != EpollThingy::Type::Client)
				continue;

			auto& client_info = thingy.value.get<Client>();
			if (client_info.output_buffer.empty() || client_info.has_epollout)
				continue;

			epoll_event event { .events = EPOLLIN | EPOLLOUT, .data = { .fd = client_info.fd } };
			if (epoll_ctl(g_epoll_fd, EPOLL_CTL_MOD, client_info.fd, &event) == -1)
			{
				perror("xbanan: epoll_ctl");
				close_client(client_info.fd);
				goto iterator_invalidated;
			}

			client_info.has_epollout = true;
		}
	}
}

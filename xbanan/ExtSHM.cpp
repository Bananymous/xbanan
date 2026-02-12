#include "Base.h"
#include "Extensions.h"
#include "Image.h"
#include "Utils.h"

#include <X11/X.h>
#include <X11/extensions/shmproto.h>

#include <netinet/in.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <unistd.h>

struct ShmSegment
{
	void* addr;
};

static BAN::HashMap<CARD32, ShmSegment> s_shm_segments;

static BYTE s_shm_event_base;
static BYTE s_shm_error_base;

static bool is_local_socket(int socket)
{
	sockaddr_storage addr;
	socklen_t addr_len = sizeof(addr);
	if (getpeername(socket, reinterpret_cast<sockaddr*>(&addr), &addr_len) == -1)
		return false;

	switch (addr.ss_family)
	{
		case AF_UNIX:
			return true;
		case AF_INET:
		{
			const auto* addr_in = reinterpret_cast<const sockaddr_in*>(&addr);
			const auto ipv4 = ntohl(addr_in->sin_addr.s_addr);
			return (ipv4 & IN_CLASSA_NET) == IN_LOOPBACKNET;
		}
		case AF_INET6:
		{
			const auto* addr_in6 = reinterpret_cast<const sockaddr_in6*>(&addr);
			return IN6_IS_ADDR_LOOPBACK(&addr_in6->sin6_addr);
		}
	}

	return false;
}

static BAN::ErrorOr<void> extension_shm(Client& client_info, BAN::ConstByteSpan packet)
{
	struct DrawableInfo
	{
		BAN::ByteSpan data;
		CARD32 w, h;
		CARD8 depth;
	};

	const uint8_t major_opcode = packet[0];
	const uint8_t minor_opcode = packet[1];

	const auto get_drawable =
		[&client_info, minor_opcode, major_opcode](WINDOW drawable) -> BAN::ErrorOr<Object&>
		{
			auto it = g_objects.find(drawable);
			if (it == g_objects.end() || (it->value->type != Object::Type::Window && it->value->type != Object::Type::Pixmap))
			{
				xError error {
					.type = X_Error,
					.errorCode = BadDrawable,
					.sequenceNumber = client_info.sequence,
					.resourceID = drawable,
					.minorCode = minor_opcode,
					.majorCode = major_opcode,
				};
				TRY(encode(client_info.output_buffer, error));
				return BAN::Error::from_errno(ENOENT);
			}

			return *it->value;
		};

	const auto get_drawable_info =
		[](Object& object) -> DrawableInfo
		{
			DrawableInfo info;

			switch (object.type)
			{
				case Object::Type::Window:
				{
					auto& window = object.object.get<Object::Window>();
					auto& texture = window.texture();

					info.data = { reinterpret_cast<uint8_t*>(texture.pixels().data()), texture.pixels().size() * 4 };
					info.w = texture.width();
					info.h = texture.height();
					info.depth = window.depth;

					break;
				}
				case Object::Type::Pixmap:
				{
					auto& pixmap = object.object.get<Object::Pixmap>();

					info.data = pixmap.data.span();
					info.w = pixmap.width;
					info.h = pixmap.height;
					info.depth = pixmap.depth;

					break;
				}
				default:
					ASSERT_NOT_REACHED();
			}

			return info;
		};

	switch (packet[1])
	{
		case X_ShmQueryVersion:
		{
			dprintln("ShmQueryVersion");

			xShmQueryVersionReply reply {
				.type = X_Reply,
				.sharedPixmaps = is_local_socket(client_info.fd),
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.majorVersion = 1,
				.minorVersion = 2,
				.uid = static_cast<CARD16>(getuid()),
				.gid = static_cast<CARD16>(getgid()),
				.pixmapFormat = ZPixmap,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_ShmAttach:
		{
			auto request = decode<xShmAttachReq>(packet).value();

			dprintln("ShmAttach");
			dprintln("  shmseg:   {}", request.shmseg);
			dprintln("  shmid:    {}", request.shmid);
			dprintln("  readOnly: {}", request.readOnly);

			void* addr = shmat(request.shmid, nullptr, request.readOnly ? SHM_RDONLY : 0);
			ASSERT(addr != (void*)-1);

			TRY(s_shm_segments.insert(request.shmseg, {
				.addr = addr,
			}));

			xShmCompletionEvent event {
				.type = static_cast<BYTE>(s_shm_event_base + ShmCompletion),
				.sequenceNumber = client_info.sequence,
				.drawable = 0,
				.shmseg = request.shmseg,
				.offset = 0,
			};
			TRY(encode(client_info.output_buffer, event));

			break;
		}
		case X_ShmDetach:
		{
			auto request = decode<xShmDetachReq>(packet).value();

			dprintln("ShmDetach");
			dprintln("  shmseg:   {}", request.shmseg);

			auto it = s_shm_segments.find(request.shmseg);
			if (it != s_shm_segments.end())
			{
				shmdt(it->value.addr);
				s_shm_segments.remove(it);
			}
			else
			{
				xError error {
					.type = X_Error,
					.errorCode = static_cast<BYTE>(s_shm_error_base + BadShmSeg),
					.sequenceNumber = client_info.sequence,
					.resourceID = request.shmseg,
					.minorCode = minor_opcode,
					.majorCode = major_opcode,
				};
				TRY(encode(client_info.output_buffer, error));
			}

			break;
		}
		case X_ShmPutImage:
		{
			auto request = decode<xShmPutImageReq>(packet).value();

#if 0
			dprintln("ShmPutImage");
			dprintln("  drawable:    {}", request.drawable);
			dprintln("  gc:          {}", request.gc);
			dprintln("  totalWidth:  {}", request.totalWidth);
			dprintln("  totalHeight: {}", request.totalHeight);
			dprintln("  srcX:        {}", request.srcX);
			dprintln("  srcY:        {}", request.srcY);
			dprintln("  srcWidth:    {}", request.srcWidth);
			dprintln("  srcHeight:   {}", request.srcHeight);
			dprintln("  dstX:        {}", request.dstX);
			dprintln("  dstY:        {}", request.dstY);
			dprintln("  depth:       {}", request.depth);
			dprintln("  format:      {}", request.format);
			dprintln("  sendEvent:   {}", request.sendEvent);
			dprintln("  shmseg:      {}", request.shmseg);
			dprintln("  offset:      {}", request.offset);
#endif

			auto& shm_segment = s_shm_segments[request.shmseg];

			auto& object = TRY_REF(get_drawable(request.drawable));
			auto [out_data, out_w, out_h, out_depth] = get_drawable_info(object);

			put_image({
				.out_data = out_data.data(),
				.out_x = request.dstX,
				.out_y = request.dstY,
				.out_w = out_w,
				.out_h = out_h,
				.out_depth = out_depth,
				.in_data = (const uint8_t*)shm_segment.addr + request.offset,
				.in_x = request.srcX,
				.in_y = request.srcY,
				.in_w = request.totalWidth,
				.in_h = request.totalHeight,
				.in_depth = request.depth,
				.w = request.srcWidth,
				.h = request.srcHeight,
				.left_pad = 0,
				.format = request.format,
			});

			if (object.type == Object::Type::Window)
				invalidate_window(request.drawable, request.dstX, request.dstY, request.srcWidth, request.srcHeight);

			if (request.sendEvent)
			{
				xShmCompletionEvent event {
					.type = static_cast<BYTE>(s_shm_event_base + ShmCompletion),
					.sequenceNumber = client_info.sequence,
					.drawable = request.drawable,
					.shmseg = request.shmseg,
					.offset = request.offset,
				};
				TRY(encode(client_info.output_buffer, event));
			}

			break;
		}
		default:
			dwarnln("unsupported shm minor opcode {}", packet[1]);
			break;
	}

	return {};
}

static struct SHMInstaller
{
	SHMInstaller()
	{
		install_extension(SHMNAME, ShmNumberEvents, ShmNumberErrors, extension_shm);
		for (const auto& extension : g_extensions)
		{
			if (extension.name != SHMNAME)
				continue;
			s_shm_event_base = extension.event_base;
			s_shm_error_base = extension.error_base;
			break;
		}
	}
} installer;

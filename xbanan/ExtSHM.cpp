#include "Base.h"
#include "Extensions.h"
#include "Image.h"
#include "SafeGetters.h"
#include "Utils.h"

#include <X11/X.h>
#include <X11/extensions/shmproto.h>

#include <netinet/in.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <unistd.h>

static BYTE s_shm_event_base;
static BYTE s_shm_error_base;
static BYTE s_shm_major_opcode;

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

static BAN::ErrorOr<void*> get_shmseg(Client& client_info, CARD32 shmseg, BYTE op_major, BYTE op_minor)
{
	auto it = g_objects.find(shmseg);
	if (it != g_objects.end() && it->value->type == Object::Type::Extension)
	{
		auto& ext = it->value->object.get<Object::Extension>();
		if (ext.type_major == s_shm_major_opcode && ext.type_minor == BadShmSeg)
			return ext.c_private;
	}

	xError error {
		.type = X_Error,
		.errorCode = static_cast<BYTE>(s_shm_error_base + BadShmSeg),
		.sequenceNumber = client_info.sequence,
		.resourceID = shmseg,
		.minorCode = op_minor,
		.majorCode = op_major,
	};
	TRY(encode(client_info.output_buffer, error));
	return BAN::Error::from_errno(ENOENT);
}

static BAN::ErrorOr<void> extension_shm(Client& client_info, BAN::ConstByteSpan packet)
{
	const uint8_t op_major = packet[0];
	const uint8_t op_minor = packet[1];

	switch (op_minor)
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
			if (addr == (void*)-1)
			{
				xError error {
					.type = X_Error,
					.errorCode = static_cast<BYTE>(s_shm_error_base + BadShmSeg),
					.sequenceNumber = client_info.sequence,
					.resourceID = request.shmseg,
					.minorCode = op_minor,
					.majorCode = op_major,
				};
				TRY(encode(client_info.output_buffer, error));
				return {};
			}

			TRY(client_info.objects.insert(request.shmseg));
			TRY(g_objects.insert(
				request.shmseg,
				TRY(BAN::UniqPtr<Object>::create(Object {
					.type = Object::Type::Extension,
					.object = Object::Extension {
						.type_major = s_shm_major_opcode,
						.type_minor = BadShmSeg,
						.c_private = addr,
						.destructor = [](auto& ext) { shmdt(ext.c_private); },
					},
				}))
			));

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

			void* shm_segment = TRY(get_shmseg(client_info, request.shmseg, op_major, op_minor));
			shmdt(shm_segment);

			client_info.objects.remove(request.shmseg);
			g_objects.remove(request.shmseg);

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

			void* shm_segment = TRY(get_shmseg(client_info, request.shmseg, op_major, op_minor));

			auto [out_data, out_w, out_h, out_depth] = TRY(get_drawable_info(client_info, request.drawable, op_major, op_minor));

			const auto& gc = TRY_REF(get_gc(client_info, request.gc, op_major, op_minor));

			put_image({
				.out_data = out_data,
				.out_x = request.dstX,
				.out_y = request.dstY,
				.out_w = out_w,
				.out_h = out_h,
				.out_depth = out_depth,
				.in_data = (const uint8_t*)shm_segment + request.offset,
				.in_x = request.srcX,
				.in_y = request.srcY,
				.in_w = request.totalWidth,
				.in_h = request.totalHeight,
				.in_depth = request.depth,
				.w = request.srcWidth,
				.h = request.srcHeight,
				.left_pad = 0,
				.format = request.format,
				.gc = gc,
			});

			if (g_objects[request.drawable]->type == Object::Type::Window)
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
		case X_ShmGetImage:
		{
			auto request = decode<xShmGetImageReq>(packet).value();

			dprintln("ShmGetImage");
			dprintln("  drawable:  {}", request.drawable);
			dprintln("  x:         {}", request.x);
			dprintln("  y:         {}", request.y);
			dprintln("  wigth:     {}", request.width);
			dprintln("  height:    {}", request.height);
			dprintln("  planeMask: {}", request.planeMask);
			dprintln("  format:    {}", request.format);
			dprintln("  shmseg:    {}", request.shmseg);
			dprintln("  offset:    {}", request.offset);

			void* shm_segment = TRY(get_shmseg(client_info, request.shmseg, op_major, op_minor));

			auto [in_data, in_w, in_h, in_depth] = TRY(get_drawable_info(client_info, request.drawable, op_major, op_minor));

			const auto dwords = image_dwords(request.width, request.height, in_depth);

			get_image({
				.out_data = (uint8_t*)shm_segment + request.offset,
				.in_data = in_data,
				.in_x = request.x,
				.in_y = request.y,
				.in_w = in_w,
				.in_h = in_h,
				.w = request.width,
				.h = request.height,
				.depth = in_depth,
				.format = request.format,
			});

			xShmGetImageReply reply {
				.type = X_Reply,
				.depth = in_depth,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.visual = g_visual.visualID,
				.size = dwords * 4,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_ShmCreatePixmap:
		{
			auto request = decode<xShmCreatePixmapReq>(packet).value();

			dprintln("ShmCreatePixmap");
			dprintln("  depth:    {}", request.depth);
			dprintln("  pid:      {}", request.pid);
			dprintln("  drawable: {}", request.drawable);
			dprintln("  width:    {}", request.width);
			dprintln("  height:   {}", request.height);
			dprintln("  shmseg:   {}", request.shmseg);
			dprintln("  offset:   {}", request.offset);

			ASSERT(request.depth == 24 || request.depth == 32);

			void* shm_segment = TRY(get_shmseg(client_info, request.shmseg, op_major, op_minor));

			TRY(client_info.objects.insert(request.pid));
			TRY(g_objects.insert(
				request.pid,
				TRY(BAN::UniqPtr<Object>::create(Object {
					.type = Object::Type::Pixmap,
					.object = Object::Pixmap {
						.depth = request.depth,
						.width = request.width,
						.height = request.height,
						.data = BAN::ByteSpan(static_cast<uint8_t*>(shm_segment) + request.offset, request.width * request.height * 4),
						.owned_data = {},
					}
				}))
			));

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
			s_shm_major_opcode = extension.major_opcode;
			break;
		}
	}
} installer;

#include "SafeGetters.h"
#include "Utils.h"

#include <X11/X.h>

BAN::ErrorOr<Object::Window&> get_window(Client& client_info, CARD32 wid, BYTE op_major, BYTE op_minor)
{
	auto it = g_objects.find(wid);
	if (it == g_objects.end() || it->value->type != Object::Type::Window)
	{
		xError error {
			.type = X_Error,
			.errorCode = BadWindow,
			.sequenceNumber = client_info.sequence,
			.resourceID = wid,
			.minorCode = op_minor,
			.majorCode = op_major,
		};
		TRY(encode(client_info.output_buffer, error));
		return BAN::Error::from_errno(ENOENT);
	}

	return it->value->object.get<Object::Window>();
}

BAN::ErrorOr<Object::Pixmap&> get_pixmap(Client& client_info, CARD32 pid, BYTE op_major, BYTE op_minor)
{
	auto it = g_objects.find(pid);
	if (it == g_objects.end() || it->value->type != Object::Type::Pixmap)
	{
		xError error {
			.type = X_Error,
			.errorCode = BadPixmap,
			.sequenceNumber = client_info.sequence,
			.resourceID = pid,
			.minorCode = op_minor,
			.majorCode = op_major,
		};
		TRY(encode(client_info.output_buffer, error));
		return BAN::Error::from_errno(ENOENT);
	}

	return it->value->object.get<Object::Pixmap>();
}

BAN::ErrorOr<Object::GraphicsContext&> get_gc(Client& client_info, CARD32 gc, BYTE op_major, BYTE op_minor)
{
	auto it = g_objects.find(gc);
	if (it == g_objects.end() || it->value->type != Object::Type::GraphicsContext)
	{
		xError error {
			.type = X_Error,
			.errorCode = BadGC,
			.sequenceNumber = client_info.sequence,
			.resourceID = gc,
			.minorCode = op_minor,
			.majorCode = op_major,
		};
		TRY(encode(client_info.output_buffer, error));
		return BAN::Error::from_errno(ENOENT);
	}

	return it->value->object.get<Object::GraphicsContext>();
}

BAN::ErrorOr<DrawableInfo> get_drawable_info(Client& client_info, CARD32 drawable, BYTE op_major, BYTE op_minor)
{
	auto drawable_it = g_objects.find(drawable);
	if (drawable_it == g_objects.end() || (drawable_it->value->type != Object::Type::Window && drawable_it->value->type != Object::Type::Pixmap))
	{
		xError error {
			.type = X_Error,
			.errorCode = BadDrawable,
			.sequenceNumber = client_info.sequence,
			.resourceID = drawable,
			.minorCode = op_minor,
			.majorCode = op_major,
		};
		TRY(encode(client_info.output_buffer, error));
		return BAN::Error::from_errno(ENOENT);
	}

	DrawableInfo info;

	switch (drawable_it->value->type)
	{
		case Object::Type::Window:
		{
			auto& window = drawable_it->value->object.get<Object::Window>();
			auto& texture = window.texture();
			info.data_u32 = texture.pixels().data();
			info.w = texture.width();
			info.h = texture.height();
			info.depth = window.depth;
			break;
		}
		case Object::Type::Pixmap:
		{
			auto& pixmap = drawable_it->value->object.get<Object::Pixmap>();
			info.data_u32 = reinterpret_cast<uint32_t*>(pixmap.data.data());
			info.w = pixmap.width;
			info.h = pixmap.height;
			info.depth = pixmap.depth;
			break;
		}
		default:
			ASSERT_NOT_REACHED();
	}

	return info;
}

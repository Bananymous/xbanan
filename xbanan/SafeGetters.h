#pragma once

#include "Definitions.h"

struct DrawableInfo
{
	uint32_t* data_u32;
	uint32_t w, h;
	uint8_t depth;
};

BAN::ErrorOr<Object::Window&> get_window(Client& client_info, CARD32 wid, BYTE op_major, BYTE op_minor = 0);
BAN::ErrorOr<Object::Pixmap&> get_pixmap(Client& client_info, CARD32 pid, BYTE op_major, BYTE op_minor = 0);
BAN::ErrorOr<Object::GraphicsContext&> get_gc(Client& client_info, CARD32 gc, BYTE op_major, BYTE op_minor = 0);
BAN::ErrorOr<DrawableInfo> get_drawable_info(Client& client_info, CARD32 drawable, BYTE op_major, BYTE op_minor = 0);

Object::Cursor& get_cursor_safe(CURSOR cid);

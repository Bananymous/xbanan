#pragma once

#include "Font.h"
#include "Platform.h"
#include "Types.h"

#include <BAN/HashMap.h>
#include <BAN/HashSet.h>
#include <BAN/String.h>
#include <BAN/UniqPtr.h>
#include <BAN/Vector.h>

#include <X11/Xproto.h>

#include <stdint.h>

#if 1
#undef dprintln
#define dprintln(...)
#endif

struct Property
{
	CARD8 format;
	ATOM type;
	BAN::Vector<uint8_t> data;
};

struct Object
{
	enum class Type
	{
		Visual,
		Cursor,
		Window,
		Pixmap,
		GraphicsContext,
		Font,
		Extension,
	};

	Type type;

	struct Cursor
	{
		uint32_t width;
		uint32_t height;
		int32_t origin_x;
		int32_t origin_y;
		BAN::Vector<uint32_t> pixels;
	};

	struct Window
	{
		Client& owner;

		bool mapped { false };
		bool focused { false };
		bool fullscreen { false };
		uint8_t depth { 0 };
		int32_t x { 0 };
		int32_t y { 0 };
		int32_t cursor_x { -1 };
		int32_t cursor_y { -1 };
		WINDOW parent;
		CURSOR cursor;
		CARD16 c_class;
		BAN::Vector<WINDOW> children;

		uint32_t width  { 0 };
		uint32_t height { 0 };
		BAN::Vector<uint32_t> pixels;

		uint32_t background { 0 };

		BAN::UniqPtr<PlatformWindow> platform_window;

		BAN::HashMap<Client*, uint32_t> event_masks;

		BAN::HashMap<ATOM, Property> properties;

		uint32_t full_event_mask() const;
		BAN::ErrorOr<void> send_event(xEvent event, uint32_t event_mask);
	};

	struct Pixmap
	{
		CARD8 depth;
		CARD32 width;
		CARD32 height;
		BAN::ByteSpan data;
		BAN::Vector<uint8_t> owned_data;
	};

	struct GraphicsContext
	{
		uint32_t foreground;
		uint32_t background;

		uint16_t line_width;

		uint32_t font;

		bool graphics_exposures;

		uint32_t clip_mask;
		int32_t clip_origin_x;
		int32_t clip_origin_y;
		BAN::Vector<xRectangle> clip_rects;

		inline bool is_clipped(int32_t x, int32_t y) const
		{
			if (clip_mask == 0)
				return false;

			if (clip_mask == UINT32_MAX)
			{
				x -= clip_origin_x;
				y -= clip_origin_y;
				for (const auto& rect : clip_rects)
					if (x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height)
						return false;
				return true;
			}

			// TODO pixmap clipping
			return false;
		}
	};

	struct Extension
	{
		uint8_t type_major;
		uint8_t type_minor;
		void* c_private;
		void (*destructor)(Extension&);
	};

	BAN::Variant<Cursor, Window, Pixmap, GraphicsContext, BAN::RefPtr<PCFFont>, Extension> object;
};

struct Client
{
	enum class State : uint8_t
	{
		ConnectionSetup,
		Connected,
	};
	int fd;
	State state;
	bool has_epollout { false };
	bool has_bigrequests { false };
	CARD16 sequence { 0 };
	BAN::Vector<uint8_t> input_buffer;
	BAN::Vector<uint8_t> output_buffer;
	BAN::HashSet<CARD32> objects;
};

struct EpollThingy
{
	enum class Type
	{
		Client,
		Event,
	};

	Type type;
	BAN::Variant<Client, void*> value;
};

extern const xPixmapFormat g_formats[6];
extern const xDepth g_depth;
extern const xVisualType g_visual;
extern xWindowRoot g_root;

extern BAN::HashMap<CARD32, BAN::UniqPtr<Object>> g_objects;

extern BAN::HashMap<BAN::String, ATOM> g_atoms_name_to_id;
extern BAN::HashMap<ATOM, BAN::String> g_atoms_id_to_name;
extern ATOM g_atom_value;

extern int g_epoll_fd;
extern BAN::HashMap<int, EpollThingy> g_epoll_thingies;

extern int g_server_grabber_fd;

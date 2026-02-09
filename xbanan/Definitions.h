#pragma once

#include <BAN/Vector.h>
#include <BAN/UniqPtr.h>
#include <BAN/HashMap.h>
#include <BAN/HashSet.h>

#include <LibGUI/Window.h>

#include <X11/Xproto.h>

#include <stdint.h>

#if 0
#undef dprintln
#define dprintln(...)
#endif

typedef CARD32 ATOM;
typedef CARD32 BITGRAVITY;
typedef CARD32 COLORMAP;
typedef CARD32 CURSOR;
typedef CARD32 PIXMAP;
typedef CARD32 VISUALID;
typedef CARD32 WINDOW;
typedef CARD32 WINGRAVITY;

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
		Window,
		Pixmap,
		GraphicsContext,
	};

	Type type;

	struct Window
	{
		bool mapped { false };
		bool focused { false };
		int32_t x { 0 };
		int32_t y { 0 };
		int32_t cursor_x { -1 };
		int32_t cursor_y { -1 };
		uint32_t event_mask { 0 };
		WINDOW parent;
		CARD16 c_class;
		BAN::Vector<WINDOW> children;
		BAN::Variant<
			BAN::UniqPtr<LibGUI::Window>,
			LibGUI::Texture
		> window;

		BAN::HashMap<ATOM, Property> properties;

		LibGUI::Texture& texture()
		{
			if (window.has<LibGUI::Texture>())
				return window.get<LibGUI::Texture>();
			if (window.has<BAN::UniqPtr<LibGUI::Window>>())
				return window.get<BAN::UniqPtr<LibGUI::Window>>()->texture();
			ASSERT_NOT_REACHED();
		}

		const LibGUI::Texture& texture() const
		{
			if (window.has<LibGUI::Texture>())
				return window.get<LibGUI::Texture>();
			if (window.has<BAN::UniqPtr<LibGUI::Window>>())
				return window.get<BAN::UniqPtr<LibGUI::Window>>()->texture();
			ASSERT_NOT_REACHED();
		}
	};

	struct Pixmap
	{
		CARD8 depth;
		CARD32 width;
		CARD32 height;
		BAN::Vector<uint8_t> data;
	};

	struct GraphicsContext
	{
		uint32_t foreground;
		uint32_t background;
	};

	BAN::Variant<Window, Pixmap, GraphicsContext> object;
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
		Window,
	};

	Type type;
	BAN::Variant<Client, LibGUI::Window*> value;
};

extern const xPixmapFormat g_formats[5];
extern const xWindowRoot g_root;
extern const xDepth g_depth;
extern const xVisualType g_visual;

extern BAN::HashMap<CARD32, BAN::UniqPtr<Object>> g_objects;

extern BAN::HashMap<BAN::String, ATOM> g_atoms_name_to_id;
extern BAN::HashMap<ATOM, BAN::String> g_atoms_id_to_name;
extern ATOM g_atom_value;

extern int g_epoll_fd;
extern BAN::HashMap<int, EpollThingy> g_epoll_thingies;

#include "Base.h"
#include "Definitions.h"
#include "Drawing.h"
#include "Extensions.h"
#include "Font.h"
#include "Image.h"
#include "Keymap.h"
#include "SafeGetters.h"
#include "Utils.h"

#include <X11/X.h>
#include <X11/Xatom.h>

#include <time.h>
#include <sys/epoll.h>

struct Selection
{
	CARD32 time;
	WINDOW owner;
};

static BAN::HashMap<ATOM, Selection> g_selections;

CARD16 g_keymask { 0 };
CARD16 g_butmask { 0 };
WINDOW g_focus_window { None };

static WINDOW s_pointer_grab_wid = None;

static const char* s_opcode_to_name[] {
	[0] = "none",
	[X_CreateWindow] = "X_CreateWindow",
	[X_ChangeWindowAttributes] = "X_ChangeWindowAttributes",
	[X_GetWindowAttributes] = "X_GetWindowAttributes",
	[X_DestroyWindow] = "X_DestroyWindow",
	[X_DestroySubwindows] = "X_DestroySubwindows",
	[X_ChangeSaveSet] = "X_ChangeSaveSet",
	[X_ReparentWindow] = "X_ReparentWindow",
	[X_MapWindow] = "X_MapWindow",
	[X_MapSubwindows] = "X_MapSubwindows",
	[X_UnmapWindow] = "X_UnmapWindow",
	[X_UnmapSubwindows] = "X_UnmapSubwindows",
	[X_ConfigureWindow] = "X_ConfigureWindow",
	[X_CirculateWindow] = "X_CirculateWindow",
	[X_GetGeometry] = "X_GetGeometry",
	[X_QueryTree] = "X_QueryTree",
	[X_InternAtom] = "X_InternAtom",
	[X_GetAtomName] = "X_GetAtomName",
	[X_ChangeProperty] = "X_ChangeProperty",
	[X_DeleteProperty] = "X_DeleteProperty",
	[X_GetProperty] = "X_GetProperty",
	[X_ListProperties] = "X_ListProperties",
	[X_SetSelectionOwner] = "X_SetSelectionOwner",
	[X_GetSelectionOwner] = "X_GetSelectionOwner",
	[X_ConvertSelection] = "X_ConvertSelection",
	[X_SendEvent] = "X_SendEvent",
	[X_GrabPointer] = "X_GrabPointer",
	[X_UngrabPointer] = "X_UngrabPointer",
	[X_GrabButton] = "X_GrabButton",
	[X_UngrabButton] = "X_UngrabButton",
	[X_ChangeActivePointerGrab] = "X_ChangeActivePointerGrab",
	[X_GrabKeyboard] = "X_GrabKeyboard",
	[X_UngrabKeyboard] = "X_UngrabKeyboard",
	[X_GrabKey] = "X_GrabKey",
	[X_UngrabKey] = "X_UngrabKey",
	[X_AllowEvents] = "X_AllowEvents",
	[X_GrabServer] = "X_GrabServer",
	[X_UngrabServer] = "X_UngrabServer",
	[X_QueryPointer] = "X_QueryPointer",
	[X_GetMotionEvents] = "X_GetMotionEvents",
	[X_TranslateCoords] = "X_TranslateCoords",
	[X_WarpPointer] = "X_WarpPointer",
	[X_SetInputFocus] = "X_SetInputFocus",
	[X_GetInputFocus] = "X_GetInputFocus",
	[X_QueryKeymap] = "X_QueryKeymap",
	[X_OpenFont] = "X_OpenFont",
	[X_CloseFont] = "X_CloseFont",
	[X_QueryFont] = "X_QueryFont",
	[X_QueryTextExtents] = "X_QueryTextExtents",
	[X_ListFonts] = "X_ListFonts",
	[X_ListFontsWithInfo] = "X_ListFontsWithInfo",
	[X_SetFontPath] = "X_SetFontPath",
	[X_GetFontPath] = "X_GetFontPath",
	[X_CreatePixmap] = "X_CreatePixmap",
	[X_FreePixmap] = "X_FreePixmap",
	[X_CreateGC] = "X_CreateGC",
	[X_ChangeGC] = "X_ChangeGC",
	[X_CopyGC] = "X_CopyGC",
	[X_SetDashes] = "X_SetDashes",
	[X_SetClipRectangles] = "X_SetClipRectangles",
	[X_FreeGC] = "X_FreeGC",
	[X_ClearArea] = "X_ClearArea",
	[X_CopyArea] = "X_CopyArea",
	[X_CopyPlane] = "X_CopyPlane",
	[X_PolyPoint] = "X_PolyPoint",
	[X_PolyLine] = "X_PolyLine",
	[X_PolySegment] = "X_PolySegment",
	[X_PolyRectangle] = "X_PolyRectangle",
	[X_PolyArc] = "X_PolyArc",
	[X_FillPoly] = "X_FillPoly",
	[X_PolyFillRectangle] = "X_PolyFillRectangle",
	[X_PolyFillArc] = "X_PolyFillArc",
	[X_PutImage] = "X_PutImage",
	[X_GetImage] = "X_GetImage",
	[X_PolyText8] = "X_PolyText8",
	[X_PolyText16] = "X_PolyText16",
	[X_ImageText8] = "X_ImageText8",
	[X_ImageText16] = "X_ImageText16",
	[X_CreateColormap] = "X_CreateColormap",
	[X_FreeColormap] = "X_FreeColormap",
	[X_CopyColormapAndFree] = "X_CopyColormapAndFree",
	[X_InstallColormap] = "X_InstallColormap",
	[X_UninstallColormap] = "X_UninstallColormap",
	[X_ListInstalledColormaps] = "X_ListInstalledColormaps",
	[X_AllocColor] = "X_AllocColor",
	[X_AllocNamedColor] = "X_AllocNamedColor",
	[X_AllocColorCells] = "X_AllocColorCells",
	[X_AllocColorPlanes] = "X_AllocColorPlanes",
	[X_FreeColors] = "X_FreeColors",
	[X_StoreColors] = "X_StoreColors",
	[X_StoreNamedColor] = "X_StoreNamedColor",
	[X_QueryColors] = "X_QueryColors",
	[X_LookupColor] = "X_LookupColor",
	[X_CreateCursor] = "X_CreateCursor",
	[X_CreateGlyphCursor] = "X_CreateGlyphCursor",
	[X_FreeCursor] = "X_FreeCursor",
	[X_RecolorCursor] = "X_RecolorCursor",
	[X_QueryBestSize] = "X_QueryBestSize",
	[X_QueryExtension] = "X_QueryExtension",
	[X_ListExtensions] = "X_ListExtensions",
	[X_ChangeKeyboardMapping] = "X_ChangeKeyboardMapping",
	[X_GetKeyboardMapping] = "X_GetKeyboardMapping",
	[X_ChangeKeyboardControl] = "X_ChangeKeyboardControl",
	[X_GetKeyboardControl] = "X_GetKeyboardControl",
	[X_Bell] = "X_Bell",
	[X_ChangePointerControl] = "X_ChangePointerControl",
	[X_GetPointerControl] = "X_GetPointerControl",
	[X_SetScreenSaver] = "X_SetScreenSaver",
	[X_GetScreenSaver] = "X_GetScreenSaver",
	[X_ChangeHosts] = "X_ChangeHosts",
	[X_ListHosts] = "X_ListHosts",
	[X_SetAccessControl] = "X_SetAccessControl",
	[X_SetCloseDownMode] = "X_SetCloseDownMode",
	[X_KillClient] = "X_KillClient",
	[X_RotateProperties] = "X_RotateProperties",
	[X_ForceScreenSaver] = "X_ForceScreenSaver",
	[X_SetPointerMapping] = "X_SetPointerMapping",
	[X_GetPointerMapping] = "X_GetPointerMapping",
	[X_SetModifierMapping] = "X_SetModifierMapping",
	[X_GetModifierMapping] = "X_GetModifierMapping",
	[120] = "none",
	[121] = "none",
	[122] = "none",
	[123] = "none",
	[124] = "none",
	[125] = "none",
	[126] = "none",
	[X_NoOperation] = "X_NoOperation",
};

void register_display(int32_t x, int32_t y, uint32_t width, uint32_t height)
{
	MUST(g_displays.push_back({
		.x = x,
		.y = y,
		.w = width,
		.h = height,
	}));
}

uint32_t Object::Window::full_event_mask() const
{
	uint32_t full_event_mask = 0;
	for (auto [_, event_mask] : event_masks)
		full_event_mask |= event_mask;
	return full_event_mask;
}

BAN::ErrorOr<void> Object::Window::send_event(xEvent xevent, uint32_t xmask)
{
	for (auto [client, event_mask] : event_masks)
	{
		if (!(event_mask & xmask))
			continue;
		xevent.u.u.sequenceNumber = client->sequence;
		TRY(encode(client->output_buffer, xevent));
	}

	return {};
}

BAN::ErrorOr<void> setup_client_conneciton(Client& client_info, const xConnClientPrefix& client_prefix)
{
	dprintln("Connection Setup");
	dprintln("  byteOrder:        {2h}", client_prefix.byteOrder);
	dprintln("  majorVersion:     {}", client_prefix.majorVersion);
	dprintln("  minorVersion:     {}", client_prefix.minorVersion);
	dprintln("  nbytesAuthProto:  {}", client_prefix.nbytesAuthProto);
	dprintln("  nbytesAuthString: {}", client_prefix.nbytesAuthString);

	if (client_prefix.byteOrder != 0x6C)
	{
		dwarnln("non little endian request, fuck you");
		return BAN::Error::from_errno(ENOTSUP);
	}

	const CARD8 format_count = sizeof(g_formats) / sizeof(*g_formats);

	xConnSetupPrefix setup_prefix {
		.success = 1,
		.lengthReason = 0, // wtf is this
		.majorVersion = 11,
		.minorVersion = 0,
		.length = 8 + 2 * format_count + (8 + 0 + sz_xWindowRoot + sz_xDepth + sz_xVisualType) / 4,
	};
	TRY(encode(client_info.output_buffer, setup_prefix));

	ASSERT((client_info.fd >> 20) == 0);

	xConnSetup setup {
		.release = 0,
		.ridBase = static_cast<CARD32>(client_info.fd << 20),
		.ridMask = 0x000FFFFF,
		.motionBufferSize = 0,
		.nbytesVendor = 8,
		.maxRequestSize = 0xFFFF,
		.numRoots = 1,
		.numFormats = format_count,
		.imageByteOrder = LSBFirst,
		.bitmapBitOrder = LSBFirst,
		.bitmapScanlineUnit = 32,
		.bitmapScanlinePad = 32,
		.minKeyCode = g_keymap_min_keycode,
		.maxKeyCode = g_keymap_max_keycode,
	};

	TRY(encode(client_info.output_buffer, setup));
	TRY(encode(client_info.output_buffer, "_xbanan_"_sv));
	TRY(encode(client_info.output_buffer, BAN::ConstByteSpan::from(g_formats)));
	TRY(encode(client_info.output_buffer, g_root));
	TRY(encode(client_info.output_buffer, g_depth));
	TRY(encode(client_info.output_buffer, g_visual));

	client_info.state = Client::State::Connected;

	return {};
}

static bool is_visible(WINDOW wid)
{
	if (wid == g_root.windowId)
		return true;

	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);

	auto& window = object.object.get<Object::Window>();
	if (!window.mapped)
		return false;

	return is_visible(window.parent);
}

static BAN::ErrorOr<void> send_visibility_events_recursively(Client& client_info, WINDOW wid, bool visible)
{
	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);

	auto& window = object.object.get<Object::Window>();

	if (window.c_class == InputOutput)
	{
		xEvent event = { .u = {
			.visibility = {
				.window = wid,
				.state = static_cast<CARD8>(visible ? 0 : 2),
			}
		}};
		event.u.u.type = VisibilityNotify;
		TRY(window.send_event(event, VisibilityChangeMask));
	}

	for (auto child_wid : window.children)
	{
		auto& child_object = *g_objects[child_wid];
		ASSERT(child_object.type == Object::Type::Window);

		auto& child_window = object.object.get<Object::Window>();
		if (!child_window.mapped)
			continue;

		TRY(send_visibility_events_recursively(client_info, child_wid, visible));
	}

	return {};
}

static void invalidate_window_recursive(WINDOW wid, int32_t x, int32_t y, int32_t w, int32_t h)
{
	ASSERT(wid != g_root.windowId);

	if (x + w <= 0 || y + h <= 0)
		return;
	if (w <= 0 || h <= 0)
		return;

	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);

	auto& window = object.object.get<Object::Window>();
	if (!window.mapped)
		return;

	for (auto child_wid : window.children)
	{
		const auto& child_object = *g_objects[child_wid];
		ASSERT(child_object.type == Object::Type::Window);

		const auto& child_window = child_object.object.get<Object::Window>();
		if (!child_window.mapped)
			continue;

		const auto child_x = x - child_window.x;
		const auto child_y = y - child_window.y;

		const auto child_w = BAN::Math::min<int32_t>(w - child_window.x, child_window.width  - child_x);
		const auto child_h = BAN::Math::min<int32_t>(h - child_window.y, child_window.height - child_y);

		invalidate_window_recursive(
			child_wid,
			child_x, child_y,
			child_w, child_h
		);
	}

	if (window.platform_window || window.c_class == InputOnly)
		return;

	auto& parent_object = *g_objects[window.parent];
	ASSERT(parent_object.type == Object::Type::Window);

	auto& parent_window = parent_object.object.get<Object::Window>();

	const uint32_t* child_pix = window.pixels.data();
	const int32_t child_w = window.width;
	const int32_t child_h = window.height;

	uint32_t* parent_pix = parent_window.pixels.data();
	const int32_t parent_w = parent_window.width;
	const int32_t parent_h = parent_window.height;

	// FIXME: optimize this
	for (int32_t y_off = 0; y_off < h; y_off++)
	{
		const int32_t child_y = y + y_off;
		if (child_y < 0 || child_y >= child_h)
			continue;

		const int32_t parent_y = window.y + y + y_off;
		if (parent_y < 0 || parent_y >= parent_h)
			continue;

		for (int32_t x_off = 0; x_off < w; x_off++)
		{
			const int32_t child_x = x + x_off;
			if (child_x < 0 || child_x >= child_w)
				continue;

			const int32_t parent_x = window.x + x + x_off;
			if (parent_x < 0 || parent_x >= parent_w)
				continue;

			const uint32_t color = child_pix[child_y * child_w + child_x];
			if (color != COLOR_INVISIBLE)
				parent_pix[parent_y * parent_w + parent_x] = color;
		}
	}
}

void invalidate_window(WINDOW wid, int32_t x, int32_t y, int32_t w, int32_t h)
{
	ASSERT(wid != g_root.windowId);

	for (;;)
	{
		if (wid == g_root.windowId)
			return;

		auto& object = *g_objects[wid];
		ASSERT(object.type == Object::Type::Window);

		auto& window = object.object.get<Object::Window>();
		if (!window.mapped)
			return;
		if (window.platform_window)
			break;

		x += window.x;
		y += window.y;
		wid = window.parent;
	}

	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);

	auto& window = object.object.get<Object::Window>();
	ASSERT(window.platform_window);

	invalidate_window_recursive(wid, x, y, w, h);

	const auto min_x = BAN::Math::max<int32_t>(x, 0);
	const auto min_y = BAN::Math::max<int32_t>(y, 0);
	const auto max_x = BAN::Math::min<int32_t>(x + w, window.width);
	const auto max_y = BAN::Math::min<int32_t>(y + h, window.height);

	if (min_x >= max_x || min_y >= max_y)
		return;

	uint32_t* pixels = window.pixels.data();
	for (auto y = min_y; y < max_y; y++)
	{
		for (auto x = min_x; x < max_x; x++)
		{
			auto& pixel = pixels[y * window.width + x];
			if (pixel == COLOR_INVISIBLE)
				pixel = 0x00000000;
			else
				pixel |= 0xFF000000;
		}
	}

	g_platform_ops.invalidate(window.platform_window.ptr(), window.pixels.data(), min_x, min_y, max_x - min_x, max_y - min_y);
}

void send_exposure_recursive(WINDOW wid)
{
	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);

	auto& window = object.object.get<Object::Window>();
	if (!window.mapped)
		return;

	for (auto child_wid : window.children)
		send_exposure_recursive(child_wid);

	xEvent event = { .u = {
		.expose = {
			.window = wid,
			.x = 0,
			.y = 0,
			.width = static_cast<CARD16>(window.width),
			.height = static_cast<CARD16>(window.height),
		}
	}};
	event.u.u.type = Expose;
	MUST(window.send_event(event, ExposureMask));
}

struct PlatformWindowInfo
{
	PlatformWindow* parent;
	WindowType type;
};

static PlatformWindowInfo get_plaform_window_info(const Object::Window& window)
{
	static const CARD32 ATOM                              = g_atoms_name_to_id["ATOM"_sv];
	static const CARD32 WINDOW                            = g_atoms_name_to_id["WINDOW"_sv];
	static const CARD32 WM_TRANSIENT_FOR                  = g_atoms_name_to_id["WM_TRANSIENT_FOR"_sv];
	static const CARD32 _NET_WM_WINDOW_TYPE               = g_atoms_name_to_id["_NET_WM_WINDOW_TYPE"_sv];
	static const CARD32 _NET_WM_WINDOW_TYPE_DIALOG        = g_atoms_name_to_id["_NET_WM_WINDOW_TYPE_DIALOG"_sv];
	static const CARD32 _NET_WM_WINDOW_TYPE_NORMAL        = g_atoms_name_to_id["_NET_WM_WINDOW_TYPE_NORMAL"_sv];
	static const CARD32 _NET_WM_WINDOW_TYPE_SPLASH        = g_atoms_name_to_id["_NET_WM_WINDOW_TYPE_SPLASH"_sv];
	static const CARD32 _NET_WM_WINDOW_TYPE_UTILITY       = g_atoms_name_to_id["_NET_WM_WINDOW_TYPE_UTILITY"_sv];

	PlatformWindowInfo info {
		.parent = nullptr,
		.type = WindowType::Normal,
	};

	if (auto it = window.properties.find(_NET_WM_WINDOW_TYPE); it != window.properties.end())
	{
		if (it->value.type != ATOM || it->value.data.size() != sizeof(CARD32))
			goto wm_window_type_done;

		const CARD32 type = *reinterpret_cast<const CARD32*>(it->value.data.data());
		if (type == _NET_WM_WINDOW_TYPE_NORMAL)
			info.type = WindowType::Normal;
		else if (type == _NET_WM_WINDOW_TYPE_SPLASH || type == _NET_WM_WINDOW_TYPE_DIALOG)
			info.type = WindowType::Utility;
		else
			info.type = WindowType::Popup;
	}
wm_window_type_done:

	if (auto it = window.properties.find(WM_TRANSIENT_FOR); it != window.properties.end())
	{
		if (it->value.type != WINDOW || it->value.data.size() != sizeof(CARD32))
			goto transitient_for_done;

		const CARD32 wid = *reinterpret_cast<const CARD32*>(it->value.data.data());

		auto it2 = g_objects.find(wid);
		if (it2 == g_objects.end())
			goto transitient_for_done;
		if (it2->value->type != Object::Type::Window)
			goto transitient_for_done;

		// FIXME: support child windows
		auto& window = it2->value->object.get<Object::Window>();
		info.parent = window.platform_window.ptr();
	}
transitient_for_done:

	return info;
}

static BAN::ErrorOr<void> map_window(Client& client_info, WINDOW wid)
{
	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);

	auto& window = object.object.get<Object::Window>();
	if (window.mapped)
		return {};
	window.mapped = true;

	if (window.parent == g_root.windowId)
	{
		ASSERT(!window.platform_window);

		// NOTE: This is not perfect but seems to work.
		//       This is used to not display "dummy" event windows
		//       that should not be visible.
		if (window.width <= 10 || window.height <= 10)
			return {};

		auto info = get_plaform_window_info(window);
		window.platform_window = TRY(g_platform_ops.create_window(
			info.parent,
			info.type,
			wid,
			window.x,
			window.y,
			window.width,
			window.height
		));
	}

	for (auto& pixel : window.pixels)
		pixel = window.background;
	invalidate_window(wid, 0, 0, window.width, window.height);

	{
		xEvent event = { .u = {
			.mapNotify = {
				.event = wid,
				.window = wid,
				.override = xFalse,
			}
		}};
		event.u.u.type = MapNotify;
		TRY(window.send_event(event, StructureNotifyMask));
	}

	auto& parent_object = *g_objects[window.parent];
	ASSERT(parent_object.type == Object::Type::Window);
	auto& parent_window = parent_object.object.get<Object::Window>();

	{
		xEvent event = { .u = {
			.mapNotify = {
				.event = window.parent,
				.window = wid,
				.override = xFalse,
			}
		}};
		event.u.u.type = MapNotify;
		TRY(parent_window.send_event(event, SubstructureNotifyMask));
	}

	if (is_visible(window.parent))
		TRY(send_visibility_events_recursively(client_info, wid, true));

	{
		xEvent event = { .u = {
			.expose = {
				.window = wid,
				.x = 0,
				.y = 0,
				.width = static_cast<CARD16>(window.width),
				.height = static_cast<CARD16>(window.height),
			}
		}};
		event.u.u.type = Expose;
		TRY(window.send_event(event, ExposureMask));
	}

	return {};
}

static BAN::ErrorOr<void> unmap_window(Client& client_info, WINDOW wid)
{
	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);

	auto& window = object.object.get<Object::Window>();
	if (!window.mapped)
		return {};

	window.mapped = false;

	if (window.platform_window)
		window.platform_window.clear();

	if (is_visible(window.parent))
		TRY(send_visibility_events_recursively(client_info, wid, false));

	{
		xEvent event = { .u = {
			.unmapNotify = {
				.event = wid,
				.window = wid,
				.fromConfigure = xFalse,
			}
		}};
		event.u.u.type = UnmapNotify;
		TRY(window.send_event(event, StructureNotifyMask));
	}

	auto& parent_object = *g_objects[window.parent];
	ASSERT(parent_object.type == Object::Type::Window);
	auto& parent_window = parent_object.object.get<Object::Window>();

	{
		xEvent event = { .u = {
			.unmapNotify = {
				.event = window.parent,
				.window = wid,
				.fromConfigure = xFalse,
			}
		}};
		event.u.u.type = UnmapNotify;
		TRY(parent_window.send_event(event, SubstructureNotifyMask));
	}

	return {};
}

BAN::ErrorOr<void> destroy_window(Client& client_info, WINDOW wid)
{
	TRY(unmap_window(client_info, wid));

	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);

	auto& window = object.object.get<Object::Window>();

	for (auto child_wid : window.children)
		TRY(destroy_window(client_info, child_wid));

	{
		xEvent event = { .u = {
			.destroyNotify = {
				.event = wid,
				.window = wid,
			}
		}};
		event.u.u.type = DestroyNotify;
		TRY(window.send_event(event, StructureNotifyMask));
	}

	auto& parent_object = *g_objects[window.parent];
	ASSERT(parent_object.type == Object::Type::Window);
	auto& parent_window = parent_object.object.get<Object::Window>();

	{
		xEvent event = { .u = {
			.destroyNotify = {
				.event = window.parent,
				.window = wid,
			}
		}};
		event.u.u.type = DestroyNotify;
		TRY(parent_window.send_event(event, SubstructureNotifyMask));
	}

	for (size_t i = 0; i < parent_window.children.size(); i++)
	{
		if (parent_window.children[i] != wid)
			continue;
		parent_window.children.remove(i);
		break;
	}

	client_info.objects.remove(wid);
	g_objects.remove(wid);

	return {};
}

WINDOW find_child_window(WINDOW wid, int32_t& x, int32_t& y)
{
	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);

	auto& window = object.object.get<Object::Window>();

	if (x < 0 || y < 0 || x >= window.width || y >= window.height)
		return None;

	for (auto child_wid : window.children)
	{
		auto& child_object = *g_objects[child_wid];
		ASSERT(child_object.type == Object::Type::Window);

		auto& child_window = child_object.object.get<Object::Window>();

		x -= child_window.x;
		y -= child_window.y;
		if (auto result = find_child_window(child_wid, x, y); result != None)
			return result;
		x += child_window.x;
		y += child_window.y;
	}

	return wid;
}

xPoint get_window_position(WINDOW wid)
{
	xPoint result { .x = 0, .y = 0 };

	while (wid != None)
	{
		const auto& window = g_objects[wid]->object.get<Object::Window>();
		result.x += window.x;
		result.y += window.y;
		wid = window.parent;
	}

	return result;
}

static PlatformWindow* get_platform_window(const Object::Window& window)
{
	if (window.platform_window)
		return const_cast<PlatformWindow*>(window.platform_window.ptr());
	if (window.parent == None)
		return nullptr;

	auto& object = *g_objects[window.parent];
	ASSERT(object.type == Object::Type::Window);
	return get_platform_window(object.object.get<Object::Window>());
}

void update_cursor(WINDOW wid, int32_t x, int32_t y)
{
	if (g_platform_ops.set_cursor == nullptr)
		return;

	wid = find_child_window(wid, x, y);
	if (wid == None)
		return;

	const CURSOR cid = [&wid]() -> CURSOR {
		for (;;)
		{
			const auto& window = g_objects[wid]->object.get<Object::Window>();
			if (window.cursor != None || window.parent == None)
				return window.cursor;
			wid = window.parent;
		}
	}();

	static CURSOR active_cid = None;
	if (cid == active_cid)
		return;

	const auto& window = g_objects[wid]->object.get<Object::Window>();
	g_platform_ops.set_cursor(get_platform_window(window), get_cursor_safe(cid));

	active_cid = cid;
}

static void on_root_client_message(const xEvent& event)
{
	static const CARD32 _NET_WM_STATE            = g_atoms_name_to_id["_NET_WM_STATE"_sv];
	static const CARD32 _NET_WM_STATE_FULLSCREEN = g_atoms_name_to_id["_NET_WM_STATE_FULLSCREEN"_sv];

	ASSERT(event.u.u.type == ClientMessage);

	const auto type   = event.u.clientMessage.u.l.type;
	const auto action = event.u.clientMessage.u.l.longs0;
	const auto field  = event.u.clientMessage.u.l.longs1;
	if (type != _NET_WM_STATE || field != _NET_WM_STATE_FULLSCREEN)
		return;

	auto window_it = g_objects.find(event.u.clientMessage.window);
	if (window_it == g_objects.end() || window_it->value->type != Object::Type::Window)
		return;

	auto& window = window_it->value->object.get<Object::Window>();
	if (!window.platform_window)
		return;

	bool want_fullscren;
	switch (action)
	{
		case 0: want_fullscren = false;              break;
		case 1: want_fullscren = true;               break;
		case 2: want_fullscren = !window.fullscreen; break;
		default: return;
	}

	if (g_platform_ops.request_fullscreen)
		g_platform_ops.request_fullscreen(window.platform_window.ptr(), want_fullscren);
}

BAN::ErrorOr<void> handle_packet(Client& client_info, BAN::ConstByteSpan packet)
{
	const uint8_t opcode = packet[0];

	const auto validate_atom =
		[&client_info, opcode](ATOM atom) -> BAN::ErrorOr<void>
		{
			if (g_atoms_id_to_name.contains(atom))
				return {};

			xError error {
				.type = X_Error,
				.errorCode = BadAtom,
				.sequenceNumber = client_info.sequence,
				.resourceID = atom,
				.minorCode = 0,
				.majorCode = opcode,
			};
			TRY(encode(client_info.output_buffer, error));
			return BAN::Error::from_errno(ENOENT);
		};

	const auto get_atom_name_safe =
		[](ATOM atom) -> BAN::StringView
		{
			if (atom == None)
				return "None"_sv;
			auto it = g_atoms_id_to_name.find(atom);
			if (it == g_atoms_id_to_name.end())
				return "<INVALID ATOM>";
			return it->value.sv();
		};

	client_info.sequence++;

	if (opcode >= 128)
	{
		const auto& extension = g_extensions[opcode - 128];
		if (extension.handler != nullptr)
			return extension.handler(client_info, packet);
		dwarnln("invalid opcode {}", opcode);
		return BAN::Error::from_errno(EINVAL);
	}

	switch (opcode)
	{
		case X_CreateWindow:
		{
			auto request = decode<xCreateWindowReq>(packet).value();

			dprintln("CreateWindow");
			dprintln("  depth:   {}", request.depth);
			dprintln("  wid:     {}", request.wid);
			dprintln("  parent:  {}", request.parent);
			dprintln("  x:       {}", request.x);
			dprintln("  y:       {}", request.y);
			dprintln("  width:   {}", request.width);
			dprintln("  height:  {}", request.height);
			dprintln("  border:  {}", request.borderWidth);
			dprintln("  c_class: {}", request.c_class);
			dprintln("  visual:  {}", request.visual);
			dprintln("  mask:    {8h}", request.mask);

			auto& parent_window = TRY_REF(get_window(client_info, request.parent, opcode));
			if (request.c_class == CopyFromParent)
				request.c_class = parent_window.c_class;

			if (request.depth == 0 && request.c_class == InputOutput)
				request.depth = parent_window.depth;

			bool valid_depth = false;
			switch (request.c_class)
			{
				case InputOutput:
					valid_depth = (request.depth == g_depth.depth);
					break;
				case InputOnly:
					valid_depth = (request.depth == 0);
					break;
			}

			if (!valid_depth)
			{
				xError error {
					.type = X_Error,
					.errorCode = BadMatch,
					.sequenceNumber = client_info.sequence,
					.minorCode = 0,
					.majorCode = opcode,
				};
				TRY(encode(client_info.output_buffer, error));
				return {};
			}

			uint32_t event_mask { 0 };
			uint32_t background { 0x000000 };
			CURSOR cursor_id = None;

			for (size_t i = 0; i < 32; i++)
			{
				if (!((request.mask >> i) & 1))
					continue;
				const uint32_t value = decode<uint32_t>(packet).value();

				switch (i)
				{
					case 0:
						dprintln("    background-pixmap: {8h}", value);
						if (value == None || value == ParentRelative)
							background = COLOR_INVISIBLE;
						break;
					case 1:
						dprintln("    background-pixel: {8h}", value);
						background = value;
						break;
					case 11:
						dprintln("    event-mask: {8h}", value);
						event_mask = value;
						break;
					case 14:
						dprintln("    cursor: {8h}", value);
						cursor_id = value;
						break;
					default:
						dprintln("    {8h}: {8h}", 1 << i, value);
						break;
				}
			}

			Object::Window window {
				.owner = client_info,
				.depth = request.depth,
				.x = request.x,
				.y = request.y,
				.parent = request.parent,
				.cursor = cursor_id,
				.c_class = request.c_class,
				.width = request.width,
				.height = request.height,
				.background = background,
			};

			TRY(window.pixels.resize(window.width * window.height, window.background));

			TRY(client_info.objects.insert(request.wid));
			auto object_it = TRY(g_objects.insert(
				request.wid,
				TRY(BAN::UniqPtr<Object>::create(Object {
					.type = Object::Type::Window,
					.object = BAN::move(window),
				}))
			));
			if (event_mask != 0)
				TRY(object_it->value->object.get<Object::Window>().event_masks.insert(&client_info, event_mask));

			TRY(parent_window.children.push_back(request.wid));

			xEvent event = { .u = {
				.createNotify = {
					.parent = request.parent,
					.window = request.wid,
					.x = request.x,
					.y = request.y,
					.width = request.width,
					.height = request.height,
					.borderWidth = request.borderWidth,
					.override = false,
				}
			}};
			event.u.u.type = CreateNotify;
			TRY(parent_window.send_event(event, SubstructureNotifyMask));

			break;
		}
		case X_ChangeWindowAttributes:
		{
			auto request = decode<xChangeWindowAttributesReq>(packet).value();

			dprintln("ChangeWindowAttributes");
			dprintln("  window:    {}", request.window);
			dprintln("  valueMask: {8h}", request.valueMask);

			auto& window = TRY_REF(get_window(client_info, request.window, opcode));

			BAN::Optional<CURSOR> cursor_id = None;

			BAN::Optional<uint32_t> background;
			for (size_t i = 0; i < 32; i++)
			{
				if (!((request.valueMask >> i) & 1))
					continue;
				const uint32_t value = decode<uint32_t>(packet).value();

				switch (i)
				{
					case 0:
						dprintln("    background-pixmap: {8h}", value);
						if (value == None || value == ParentRelative)
							background = COLOR_INVISIBLE;
						break;
					case 1:
						dprintln("    background-pixel: {8h}", value);
						background = value;
						break;
					case 11:
						dprintln("    event-mask: {8h}", value);
						if (value != 0)
							TRY(window.event_masks.emplace_or_assign(&client_info, value));
						else
							window.event_masks.remove(&client_info);
						break;
					case 14:
						dprintln("    cursor: {8h}", value);
						cursor_id = value;
						break;
					default:
						dprintln("    {8h}: {8h}", 1 << i, value);
						break;
				}
			}

			if (cursor_id.has_value())
			{
				window.cursor = cursor_id.value();
				if (window.hovered)
					update_cursor(request.window, window.cursor_x, window.cursor_y);
			}

			if (background.has_value())
				window.background = background.value();

			break;
		}
		case X_GetWindowAttributes:
		{
			const CARD32 wid = packet.as_span<const uint32_t>()[1];

			dprintln("GetWindowAttributes");
			dprintln("  window: {}", wid);

			const auto& window = TRY_REF(get_window(client_info, wid, opcode));

			uint32_t my_event_mask = 0;
			if (auto it = window.event_masks.find(&client_info); it != window.event_masks.end())
				my_event_mask = it->value;

			xGetWindowAttributesReply reply {
				.type = X_Reply,
				.backingStore = 0,
				.sequenceNumber = client_info.sequence,
				.length = 3,
				.visualID = g_visual.visualID,
				.c_class = window.c_class,
				.bitGravity = 0,
				.winGravity = 0,
				.backingBitPlanes = 0,
				.backingPixel = 0,
				.saveUnder = 0,
				.mapInstalled = 0,
				.mapState = static_cast<CARD8>(is_visible(wid) ? 2 : window.mapped),
				.override = 0,
				.colormap = 0,
				.allEventMasks = window.full_event_mask(),
				.yourEventMask = my_event_mask,
				.doNotPropagateMask = 0,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_DestroyWindow:
		{
			const CARD32 wid = packet.as_span<const uint32_t>()[1];

			dprintln("DestroyWinow");
			dprintln("  window: {}", wid);

			(void)TRY_REF(get_window(client_info, wid, opcode));
			TRY(destroy_window(client_info, wid));

			break;
		}
		case X_ReparentWindow:
		{
			auto request = decode<xReparentWindowReq>(packet).value();

			dprintln("ReparentWinow");
			dprintln("  window: {}", request.window);
			dprintln("  parent: {}", request.parent);
			dprintln("  x:      {}", request.x);
			dprintln("  y:      {}", request.y);

			auto& window     = TRY_REF(get_window(client_info, request.window, opcode));
			auto& new_parent = TRY_REF(get_window(client_info, request.parent, opcode));
			auto& old_parent = TRY_REF(get_window(client_info, window.parent,  opcode));

			const auto wid = request.window;
			const auto oldpwid = window.parent;
			const auto newpwid = request.parent;

			const bool was_mapped = window.mapped;

			if (was_mapped)
				TRY(unmap_window(client_info, wid));

			TRY(new_parent.children.push_back(wid));
			window.x = request.x;
			window.y = request.y;
			window.parent = request.parent;

			for (size_t i = 0; i < old_parent.children.size(); i++)
			{
				if (old_parent.children[i] != wid)
					continue;
				old_parent.children.remove(i);
				break;
			}

			if (was_mapped)
				TRY(map_window(client_info, wid));

			{
				xEvent event = { .u = {
					.reparent = {
						.event = oldpwid,
						.window = wid,
						.parent = newpwid,
						.x = request.x,
						.y = request.y,
						.override = xFalse,
					}
				}};
				event.u.u.type = ReparentNotify;
				TRY(old_parent.send_event(event, SubstructureNotifyMask));
			}

			{
				xEvent event = { .u = {
					.reparent = {
						.event = newpwid,
						.window = wid,
						.parent = newpwid,
						.x = request.x,
						.y = request.y,
						.override = xFalse,
					}
				}};
				event.u.u.type = ReparentNotify;
				TRY(new_parent.send_event(event, SubstructureNotifyMask));
			}

			break;
		}
		case X_MapWindow:
		{
			const CARD32 wid = packet.as_span<const uint32_t>()[1];

			dprintln("MapWindow");
			dprintln("  window: {}", wid);

			(void)TRY_REF(get_window(client_info, wid, opcode));
			TRY(map_window(client_info, wid));

			break;
		}
		case X_MapSubwindows:
		{
			const CARD32 wid = packet.as_span<const uint32_t>()[1];

			dprintln("MapSubwindows");
			dprintln("  window: {}", wid);

			const auto& window = TRY_REF(get_window(client_info, wid, opcode));
			for (auto child_wid : window.children)
				TRY(map_window(client_info, child_wid));

			break;
		}
		case X_UnmapWindow:
		{
			const CARD32 wid = packet.as_span<const uint32_t>()[1];

			dprintln("UnmapWindow");
			dprintln("  window: {}", wid);

			(void)TRY_REF(get_window(client_info, wid, opcode));
			TRY(unmap_window(client_info, wid));

			break;
		}
		case X_UnmapSubwindows:
		{
			const CARD32 wid = packet.as_span<const uint32_t>()[1];

			dprintln("UnmapSubwindows");
			dprintln("  window: {}", wid);

			const auto& window = TRY_REF(get_window(client_info, wid, opcode));
			for (auto child_wid : window.children)
				TRY(unmap_window(client_info, child_wid));

			break;
		}
		case X_ConfigureWindow:
		{
			auto request = decode<xConfigureWindowReq>(packet).value();

			dprintln("ConfigureWindow");
			dprintln("  window: {}", request.window);

			auto& window = TRY_REF(get_window(client_info, request.window, opcode));

			int32_t new_x = window.x;
			int32_t new_y = window.y;
			uint32_t new_width = window.width;
			uint32_t new_height = window.height;

			dprintln("  mask:");
			for (size_t i = 0; i < 7; i++)
			{
				if (!(request.mask & (1 << i)))
					continue;
				const uint16_t value = decode<uint16_t>(packet).value();
				decode<int16_t>(packet).value();

				switch (i)
				{
					case 0:
						new_x = value;
						break;
					case 1:
						new_y = value;
						break;
					case 2:
						new_width = value;
						break;
					case 3:
						new_height = value;
						break;
					default:
						dprintln("    {4h}: {4h}", 1 << i, value);
						break;
				}
			}

			bool send_configure = false;

			if (new_x != window.x || new_y != window.y)
			{
				if (window.platform_window)
				{
					if (g_platform_ops.request_reposition)
						g_platform_ops.request_reposition(window.platform_window.ptr(), new_x, new_y);
				}
				else
				{
					window.x = new_x;
					window.y = new_y;
					send_configure = true;
				}
			}

			if (new_width != window.width || new_height != window.height)
			{
				if (window.platform_window)
				{
					if (g_platform_ops.request_resize)
						g_platform_ops.request_resize(window.platform_window.ptr(), new_width, new_height);
				}
				else
				{
					TRY(window.pixels.resize(new_width * new_height));
					for (auto& pixel : window.pixels)
						pixel = window.background;

					window.width  = new_width;
					window.height = new_height;
					send_configure = true;
				}
			}

			if (!send_configure)
				break;

			{
				xEvent event = { .u = {
					.configureNotify = {
						.event = request.window,
						.window = request.window,
						.aboveSibling = xFalse,
						.x = static_cast<INT16>(window.x),
						.y = static_cast<INT16>(window.y),
						.width = static_cast<CARD16>(window.width),
						.height = static_cast<CARD16>(window.height),
						.borderWidth = 0,
						.override = xFalse,
					}
				}};
				event.u.u.type = ConfigureNotify;
				TRY(window.send_event(event, StructureNotifyMask));
			}

			auto& parent_object = *g_objects[window.parent];
			ASSERT(parent_object.type == Object::Type::Window);
			auto& parent_window = parent_object.object.get<Object::Window>();

			{
				xEvent event = { .u = {
					.configureNotify = {
						.event = window.parent,
						.window = request.window,
						.aboveSibling = xFalse,
						.x = static_cast<INT16>(window.x),
						.y = static_cast<INT16>(window.y),
						.width = static_cast<CARD16>(window.width),
						.height = static_cast<CARD16>(window.height),
						.borderWidth = 0,
						.override = xFalse,
					}
				}};
				event.u.u.type = ConfigureNotify;
				TRY(parent_window.send_event(event, SubstructureNotifyMask));
			}

			break;
		}
		case X_GetGeometry:
		{
			const CARD32 drawable_id = packet.as_span<const uint32_t>()[1];

			dprintln("GetGeometry");
			dprintln("  drawable: {}", drawable_id);

			auto it = g_objects.find(drawable_id);
			if (it == g_objects.end() || (it->value->type != Object::Type::Window && it->value->type != Object::Type::Pixmap))
			{
				xError error {
					.type = X_Error,
					.errorCode = BadDrawable,
					.sequenceNumber = client_info.sequence,
					.resourceID = drawable_id,
					.minorCode = 0,
					.majorCode = opcode,
				};
				TRY(encode(client_info.output_buffer, error));
				return {};
			}

			const auto& drawable = *it->value;

			INT16 x, y;
			CARD16 width, height;
			CARD8 depth;

			if (drawable.type == Object::Type::Window)
			{
				const auto& window = drawable.object.get<Object::Window>();
				width = window.width;
				height = window.height;
				depth = g_root.rootDepth;
				x = window.x;
				y = window.y;
			}
			else if (drawable.type == Object::Type::Pixmap)
			{
				const auto& pixmap = drawable.object.get<Object::Pixmap>();
				width = pixmap.width;
				height = pixmap.height;
				depth = pixmap.depth;
				x = 0;
				y = 0;
			}
			else
			{
				ASSERT_NOT_REACHED();
			}

			xGetGeometryReply reply {
				.type = X_Reply,
				.depth = depth,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.root = g_root.windowId,
				.x = x,
				.y = y,
				.width = width,
				.height = height,
				.borderWidth = 0,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_QueryTree:
		{
			const auto wid = packet.as_span<const CARD32>()[1];

			dprintln("QueryTree");
			dprintln("  window: {}", wid);

			const auto& window = TRY_REF(get_window(client_info, wid, opcode));

			xQueryTreeReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = static_cast<CARD32>(window.children.size()),
				.root = g_root.windowId,
				.parent = window.parent,
				.nChildren = static_cast<CARD16>(window.children.size()),
			};
			TRY(encode(client_info.output_buffer, reply));
			for (auto child_wid : window.children)
				TRY(encode<CARD32>(client_info.output_buffer, child_wid));

			break;
		}
		case X_InternAtom:
		{
			auto request = decode<xInternAtomReq>(packet).value();

			auto name = BAN::StringView(reinterpret_cast<const char*>(packet.data()), request.nbytes);
			dprintln("InternAtom {}", name);

			auto it = g_atoms_name_to_id.find(name);
			if (it == g_atoms_name_to_id.end() && !request.onlyIfExists)
			{
				it = MUST(g_atoms_name_to_id.emplace(name, g_atom_value++));
				MUST(g_atoms_id_to_name.emplace(it->value, name));
			}

			xInternAtomReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.atom = it != g_atoms_name_to_id.end() ? it->value : 0,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_GetAtomName:
		{
			const CARD32 atom = packet.as_span<const uint32_t>()[1];

			dprintln("GetAtomName {}", atom);

			auto it = g_atoms_id_to_name.find(atom);
			if (it == g_atoms_id_to_name.end())
			{
				xError error {
					.type = X_Error,
					.errorCode = 5, // atom
					.sequenceNumber = client_info.sequence,
					.resourceID = atom,
					.minorCode = 0,
					.majorCode = X_GetAtomName,
				};
				TRY(encode(client_info.output_buffer, error));
			}
			else
			{
				const auto& name = it->value;
				xGetAtomNameReply reply {
					.type = X_Reply,
					.sequenceNumber = client_info.sequence,
					.length = static_cast<CARD32>((name.size() + 3) / 4),
					.nameLength = static_cast<CARD16>(name.size()),
				};
				TRY(encode(client_info.output_buffer, reply));
				TRY(encode(client_info.output_buffer, name));
				for (size_t i = 0; (name.size() + i) % 4; i++)
					TRY(encode<BYTE>(client_info.output_buffer, 0));
			}

			break;
		}
		case X_ChangeProperty:
		{
			auto request = decode<xChangePropertyReq>(packet).value();

			dprintln("ChangeProperty");
			dprintln("  mode:     {}", request.mode);
			dprintln("  window:   {}", request.window);
			dprintln("  property: {}", g_atoms_id_to_name[request.property]);
			dprintln("  type:     {}", g_atoms_id_to_name[request.type]);
			dprintln("  format:   {}", request.format);
			dprintln("  nUnits:   {}", request.nUnits);

			ASSERT(request.format == 8 || request.format == 16 || request.format == 32);

			auto& window = TRY_REF(get_window(client_info, request.window, opcode));

			auto it = window.properties.find(request.property);
			if (it == window.properties.end())
			{
				it = TRY(window.properties.insert(request.property, {
					.format = request.format,
					.type = request.type,
					.data = {},
				}));
			}

			auto& property = it->value;
			ASSERT(property.format == request.format);

			const size_t bytes = (request.format / 8) * request.nUnits;
			const size_t old_bytes = property.data.size();

			switch (request.mode)
			{
				case PropModeReplace:
					TRY(property.data.resize(bytes));
					memcpy(property.data.data(), packet.data(), bytes);
					break;
				case PropModePrepend:
					ASSERT(property.type == request.type);
					TRY(property.data.resize(old_bytes + bytes));
					memmove(property.data.data() + old_bytes, property.data.data(), old_bytes);
					memcpy(property.data.data(), packet.data(), bytes);
					break;
				case PropModeAppend:
					ASSERT(property.type == request.type);
					TRY(property.data.resize(old_bytes + bytes));
					memcpy(property.data.data() + old_bytes, packet.data(), bytes);
					break;
				default:
					ASSERT_NOT_REACHED();
			}

			xEvent event = { .u = {
				.property = {
					.window = request.window,
					.atom = request.property,
					.time = static_cast<CARD32>(time(nullptr)),
					.state = PropertyNewValue,
				}
			}};
			event.u.u.type = PropertyNotify;
			TRY(window.send_event(event, PropertyChangeMask));

			break;
		}
		case X_DeleteProperty:
		{
			auto request = decode<xDeletePropertyReq>(packet).value();

			dprintln("DeleteProperty");
			dprintln("  window:   {}", request.window);
			dprintln("  property: {}", g_atoms_id_to_name[request.property]);

			auto& window = TRY_REF(get_window(client_info, request.window, opcode));

			auto it = window.properties.find(request.property);
			if (it == window.properties.end())
				break;

			window.properties.remove(it);

			xEvent event = { .u = {
				.property = {
					.window = request.window,
					.atom = request.property,
					.time = static_cast<CARD32>(time(nullptr)),
					.state = PropertyDelete,
				}
			}};
			event.u.u.type = PropertyNotify;
			TRY(window.send_event(event, PropertyChangeMask));

			break;
		}
		case X_GetProperty:
		{
			auto request = decode<xGetPropertyReq>(packet).value();

			dprintln("GetProperty");
			dprintln("  c_delete:   {}", request.c_delete);
			dprintln("  window:     {}", request.window);
			dprintln("  property:   {}", g_atoms_id_to_name[request.property]);
			dprintln("  type:       {}", request.type);
			dprintln("  longOffset: {}", request.longOffset);
			dprintln("  longLength: {}", request.longLength);

			auto& window = TRY_REF(get_window(client_info, request.window, opcode));

			auto it = window.properties.find(request.property);
			if (it == window.properties.end())
			{
				xGetPropertyReply reply {
					.type = X_Reply,
					.format = 0,
					.sequenceNumber = client_info.sequence,
					.length = 0,
					.propertyType = None,
					.bytesAfter = 0,
					.nItems = 0,
				};
				TRY(encode(client_info.output_buffer, reply));
			}
			else if (request.type != AnyPropertyType && request.type != it->value.type)
			{
				const auto& property = it->value;

				xGetPropertyReply reply {
					.type = X_Reply,
					.format = property.format,
					.sequenceNumber = client_info.sequence,
					.length = 0,
					.propertyType = property.type,
					.bytesAfter = static_cast<CARD32>(property.data.size()),
					.nItems = 0,
				};
				TRY(encode(client_info.output_buffer, reply));
			}
			else
			{
				const auto& property = it->value;

				const size_t offset = request.longOffset * 4;
				const size_t bytes = BAN::Math::min<size_t>(request.longLength * 4, property.data.size() - offset);
				ASSERT(bytes % (property.format / 8) == 0);

				xGetPropertyReply reply {
					.type = X_Reply,
					.format = property.format,
					.sequenceNumber = client_info.sequence,
					.length = static_cast<CARD32>((bytes + 3) / 4),
					.propertyType = property.type,
					.bytesAfter = static_cast<CARD32>(property.data.size() - offset - bytes),
					.nItems = static_cast<CARD32>(bytes / (property.format / 8)),
				};
				TRY(encode(client_info.output_buffer, reply));
				TRY(encode(client_info.output_buffer, property.data.span().slice(offset, bytes)));
				for (size_t i = 0; (bytes + i) % 4; i++)
					TRY(encode<BYTE>(client_info.output_buffer, 0));

				if (reply.bytesAfter == 0 && request.c_delete)
				{
					window.properties.remove(it);

					xEvent event = { .u = {
						.property = {
							.window = request.window,
							.atom = request.property,
							.time = static_cast<CARD32>(time(nullptr)),
							.state = PropertyDelete,
						}
					}};
					event.u.u.type = PropertyNotify;
					TRY(window.send_event(event, PropertyChangeMask));
				}
			}

			break;
		}
		case X_ListProperties:
		{
			const CARD32 wid = packet.as_span<const uint32_t>()[1];

			dprintln("ListProperties");
			dprintln("  window:    {}", wid);

			const auto& window = TRY_REF(get_window(client_info, wid, opcode));

			xListPropertiesReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = static_cast<CARD32>(window.properties.size()),
				.nProperties = static_cast<CARD16>(window.properties.size()),
			};
			TRY(encode(client_info.output_buffer, reply));

			for (const auto& [atom, _] : window.properties)
				TRY(encode(client_info.output_buffer, atom));

			break;
		}
		case X_SetSelectionOwner:
		{
			// FIXME: selection owner should be set to None when owner is destroyed

			auto request = decode<xSetSelectionOwnerReq>(packet).value();

			dprintln("SetSelectionOwner");
			dprintln("  window:    {}", request.window);
			dprintln("  selection: {}", g_atoms_id_to_name[request.selection]);
			dprintln("  time:      {}", request.time);

			if (request.time == CurrentTime)
				request.time = time(nullptr);

			auto it = g_selections.find(request.selection);
			if (it != g_selections.end() && it->value.owner != request.window)
			{
				xEvent xevent = { .u = {
					.selectionClear = {
						.time = it->value.time,
						.window = it->value.owner,
						.atom = request.selection,
					}
				}};
				xevent.u.u.type = SelectionClear;
				xevent.u.u.sequenceNumber = client_info.sequence;
				TRY(encode(client_info.output_buffer, xevent));
			}

			TRY(g_selections.insert_or_assign(request.selection, {
				.time = request.time,
				.owner = request.window,
			}));

			break;
		}
		case X_GetSelectionOwner:
		{
			const CARD32 atom = packet.as_span<const uint32_t>()[1];

			dprintln("GetSelectionOwner");
			dprintln("  atom: {}", get_atom_name_safe(atom));

			TRY(validate_atom(atom));

			auto it = g_selections.find(atom);
			const WINDOW owner = (it == g_selections.end()) ? None : it->value.owner;

			xGetSelectionOwnerReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.owner = owner,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_ConvertSelection:
		{
			auto request = decode<xConvertSelectionReq>(packet).value();

			dprintln("ConvertSelection");
			dprintln("  requestor: {}", request.requestor);
			dprintln("  selection: {}", get_atom_name_safe(request.selection));
			dprintln("  target:    {}", get_atom_name_safe(request.target));
			dprintln("  property:  {}", get_atom_name_safe(request.property));
			dprintln("  time:      {}", request.time);

			auto it = g_selections.find(request.selection);
			if (it == g_selections.end() || it->value.owner == None)
			{
				xEvent xevent = { .u = {
					.selectionNotify = {
						.time = request.time,
						.requestor = request.requestor,
						.selection = request.selection,
						.target = request.target,
						.property = None,
					}
				}};
				xevent.u.u.type = SelectionNotify;
				xevent.u.u.sequenceNumber = client_info.sequence;
				TRY(encode(client_info.output_buffer, xevent));
			}
			else
			{
				xEvent xevent = { .u = {
					.selectionRequest = {
						.time = request.time,
						.owner = it->value.owner,
						.requestor = request.requestor,
						.selection = request.selection,
						.target = request.target,
						.property = None,
					}
				}};
				xevent.u.u.type = SelectionRequest;
				xevent.u.u.sequenceNumber = client_info.sequence;
				TRY(encode(client_info.output_buffer, xevent));
			}

			break;
		}
		case X_SendEvent:
		{
			auto request = decode<xSendEventReq>(packet).value();

			dprintln("SendEvent");
			dprintln("  propagate:   {}", request.propagate);
			dprintln("  destination: {}", request.destination);
			dprintln("  eventMask:   {}", request.eventMask);

			WINDOW wid = request.destination;
			if (wid == PointerWindow || wid == InputFocus)
				wid = g_focus_window;

			(void)TRY_REF(get_window(client_info, wid, X_SendEvent));

			Client* target_client = nullptr;
			for (auto& [_, thingy] : g_epoll_thingies)
			{
				if (thingy.type != EpollThingy::Type::Client)
					continue;
				auto& other_client = thingy.value.get<Client>();
				if (!other_client.objects.contains(wid))
					continue;
				target_client = &other_client;
				break;
			}

			// FIXME: propagate, event masks

			if (target_client)
			{
				request.event.u.u.sequenceNumber = target_client->sequence;
				TRY(encode(target_client->output_buffer, request.event));
			}

			if (request.destination == g_root.windowId && request.event.u.u.type == ClientMessage)
				on_root_client_message(request.event);

			break;
		}
		case X_GrabPointer:
		{
			auto request = decode<xGrabPointerReq>(packet).value();

			dprintln("GrabPointer");
			dprintln("  ownerEvents:  {}", request.ownerEvents);
			dprintln("  grabWindow:   {}", request.grabWindow);
			dprintln("  eventMask:    {}", request.eventMask);
			dprintln("  pointerMode:  {}", request.pointerMode);
			dprintln("  keyboardMode: {}", request.keyboardMode);
			dprintln("  confineTo:    {}", request.confineTo);
			dprintln("  cursor:       {}", request.cursor);
			dprintln("  time:         {}", request.time);

			auto& window = TRY_REF(get_window(client_info, request.grabWindow, X_GrabPointer));

			if (s_pointer_grab_wid != None && !g_objects.contains(s_pointer_grab_wid))
				s_pointer_grab_wid = None;

			if (s_pointer_grab_wid == None)
			{
				xGrabPointerReply reply {
					.type = X_Reply,
					.status = Success,
					.sequenceNumber = client_info.sequence,
					.length = 0,
				};
				TRY(encode(client_info.output_buffer, reply));

				if (g_platform_ops.set_pointer_grab)
					if (auto* platform_window = get_platform_window(window))
						g_platform_ops.set_pointer_grab(platform_window, true);

				s_pointer_grab_wid = request.grabWindow;
			}
			else
			{
				xGrabPointerReply reply {
					.type = X_Reply,
					.status = AlreadyGrabbed,
					.sequenceNumber = client_info.sequence,
					.length = 0,
				};
				TRY(encode(client_info.output_buffer, reply));
			}

			break;
		}
		case X_UngrabPointer:
		{
			auto time = packet.as_span<const uint32_t>()[1];

			dprintln("UngrabPointer");
			dprintln("  time: {}", time);

			if (s_pointer_grab_wid == None)
				break;

			auto it = g_objects.find(s_pointer_grab_wid);
			if (it != g_objects.end() && g_platform_ops.set_pointer_grab)
				if (auto* platform_window = get_platform_window(it->value->object.get<Object::Window>()))
					g_platform_ops.set_pointer_grab(platform_window, false);

			s_pointer_grab_wid = None;

			break;
		}
		case X_GrabKeyboard:
		{
			auto request = decode<xGrabKeyboardReq>(packet).value();

			dprintln("GrabKeyboard");
			dprintln("  ownerEvents:  {}", request.ownerEvents);
			dprintln("  grabWindow:   {}", request.grabWindow);
			dprintln("  pointerMode:  {}", request.pointerMode);
			dprintln("  keyboardMode: {}", request.keyboardMode);
			dprintln("  time:         {}", request.time);

			xGrabKeyboardReply reply {
				.type = X_Reply,
				.status = Success,
				.sequenceNumber = client_info.sequence,
				.length = 0,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_UngrabKeyboard:
		{
			auto time = packet.as_span<const uint32_t>()[1];

			dprintln("UngrabKeyboard");
			dprintln("  time: {}", time);

			break;
		}
		case X_GrabServer:
		{
			g_server_grabber_fd = client_info.fd;

			for (const auto& [_, thingy] : g_epoll_thingies)
			{
				if (thingy.type != EpollThingy::Type::Client)
					continue;

				auto& other_client = thingy.value.get<Client>();
				if (client_info.fd == other_client.fd)
					continue;

				uint32_t events = 0;
				if (other_client.has_epollout)
					events |= EPOLLOUT;

				epoll_event event { .events = events, .data = { .fd = other_client.fd } };
				epoll_ctl(g_epoll_fd, EPOLL_CTL_MOD, other_client.fd, &event);
			}

			break;
		}
		case X_UngrabServer:
		{
			g_server_grabber_fd = -1;

			for (const auto& [_, thingy] : g_epoll_thingies)
			{
				if (thingy.type != EpollThingy::Type::Client)
					continue;

				auto& other_client = thingy.value.get<Client>();
				if (client_info.fd == other_client.fd)
					continue;

				uint32_t events = EPOLLIN;
				if (other_client.has_epollout)
					events |= EPOLLOUT;

				epoll_event event { .events = events, .data = { .fd = other_client.fd } };
				epoll_ctl(g_epoll_fd, EPOLL_CTL_MOD, other_client.fd, &event);
			}

			break;
		}
		case X_QueryPointer:
		{
			const CARD32 wid = packet.as_span<const uint32_t>()[1];

			dprintln("QueryPointer");
			dprintln("  window: {}", wid);

			const auto& window = TRY_REF(get_window(client_info, wid, opcode));

			WINDOW child_wid { None };
			for (auto wid_ : window.children)
			{
				const auto& child_window = g_objects[wid_]->object.get<Object::Window>();
				if (!child_window.hovered)
					continue;
				child_wid = wid_;
				break;
			}

			const auto [root_x, root_y] = get_window_position(wid);

			int32_t root_cursor_x, root_cursor_y;
			if (g_platform_ops.query_pointer)
				g_platform_ops.query_pointer(&root_cursor_x, &root_cursor_y);
			else
			{
				root_cursor_x = root_x + window.cursor_x;
				root_cursor_y = root_y + window.cursor_y;
			}

			xQueryPointerReply reply {
				.type = X_Reply,
				.sameScreen = xTrue,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.root = g_root.windowId,
				.child = static_cast<CARD32>(child_wid),
				.rootX = static_cast<INT16>(root_cursor_x),
				.rootY = static_cast<INT16>(root_cursor_y),
				.winX = static_cast<INT16>(root_cursor_x - root_x),
				.winY = static_cast<INT16>(root_cursor_y - root_y),
				.mask = static_cast<KeyButMask>(g_keymask | g_butmask),
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_TranslateCoords:
		{
			auto request = decode<xTranslateCoordsReq>(packet).value();

			dprintln("TranslateCoords");
			dprintln("  src_wid: {}", request.srcWid);
			dprintln("  dst_wid: {}", request.dstWid);
			dprintln("  src_x:   {}", request.srcX);
			dprintln("  src_y:   {}", request.srcY);

			(void)TRY_REF(get_window(client_info, request.dstWid, X_TranslateCoords));
			(void)TRY_REF(get_window(client_info, request.srcWid, X_TranslateCoords));

			const auto [src_x, src_y] = get_window_position(request.srcWid);
			const auto [dst_x, dst_y] = get_window_position(request.dstWid);

			xTranslateCoordsReply reply {
				.type = X_Reply,
				.sameScreen = xTrue,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.child = None, // FIXME
				.dstX = static_cast<INT16>(src_x + request.srcX - dst_x),
				.dstY = static_cast<INT16>(src_y + request.srcY - dst_y),
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_WarpPointer:
		{
			auto request = decode<xWarpPointerReq>(packet).value();

			dprintln("WarpPointer");
			dprintln("  src_wid: {}", request.srcWid);
			dprintln("  src_x:   {}", request.srcX);
			dprintln("  src_y:   {}", request.srcY);
			dprintln("  src_w:   {}", request.srcWidth);
			dprintln("  src_h:   {}", request.srcHeight);
			dprintln("  dst_wid: {}", request.dstWid);
			dprintln("  dst_x:   {}", request.dstX);
			dprintln("  dst_y:   {}", request.dstY);

			if (g_platform_ops.warp_pointer == nullptr)
				break;

			// TODO: source window :)

			if (request.dstWid == None)
				g_platform_ops.warp_pointer(request.dstX, request.dstY, true);
			else
			{
				(void)TRY_REF(get_window(client_info, request.dstWid, X_WarpPointer));
				const auto [win_x, win_y] = get_window_position(request.dstWid);
				g_platform_ops.warp_pointer(win_x + request.dstX, win_y + request.dstY, false);
			}

			break;
		}
		case X_QueryKeymap:
		{
			dprintln("QueryKeymap");

			xQueryKeymapReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = 2,
			};
			memcpy(reply.map, g_pressed_keys, 32);
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_OpenFont:
			TRY(open_font(client_info, packet));
			break;
		case X_CloseFont:
			TRY(close_font(client_info, packet));
			break;
		case X_QueryFont:
			TRY(query_font(client_info, packet));
			break;
		case X_ListFonts:
			TRY(list_fonts(client_info, packet));
			break;
		case X_ListFontsWithInfo:
			TRY(list_fonts_with_info(client_info, packet));
			break;
		case X_GetInputFocus:
		{
			dprintln("GetInputFocus");

			xGetInputFocusReply reply {
				.type = X_Reply,
				.revertTo = None,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.focus = g_focus_window,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_CreatePixmap:
		{
			auto request = decode<xCreatePixmapReq>(packet).value();

			dprintln("CreatePixmap");
			dprintln("  depth:    {}", request.depth);
			dprintln("  pid:      {}", request.pid);
			dprintln("  drawable: {}", request.drawable);
			dprintln("  width:    {}", request.width);
			dprintln("  height:   {}", request.height);

			bool valid_depth = false;
			for (auto format : g_formats)
				if (format.depth == request.depth)
					valid_depth = true;
			ASSERT(valid_depth);

			BAN::Vector<uint8_t> data;
			TRY(data.resize(request.width * request.height * 4));

			TRY(client_info.objects.insert(request.pid));
			TRY(g_objects.insert(
				request.pid,
				TRY(BAN::UniqPtr<Object>::create(Object {
					.type = Object::Type::Pixmap,
					.object = Object::Pixmap {
						.depth = request.depth,
						.width = request.width,
						.height = request.height,
						.data = BAN::move(data),
					}
				}))
			));

			break;
		}
		case X_FreePixmap:
		{
			const CARD32 pid = packet.as_span<const uint32_t>()[1];

			dprintln("FreePixmap");
			dprintln("  pixmap: {}", pid);

			(void)TRY_REF(get_pixmap(client_info, pid, opcode));
			client_info.objects.remove(pid);
			g_objects.remove(pid);

			break;
		}
		case X_CreateGC:
		{
			auto request = decode<xCreateGCReq>(packet).value();

			dprintln("CreateGC");
			dprintln("  drawable {}", request.drawable);
			dprintln("  gc       {}", request.gc);
			dprintln("  mask     {8h}", request.mask);

			// TODO: support invisible background

			uint32_t foreground = 0x000000;
			uint32_t background = 0x000000;
			uint16_t line_width = 0;
			uint32_t font = None;
			bool graphics_exposures = true;
			uint32_t clip_mask = 0;
			int32_t clip_origin_x = 0;
			int32_t clip_origin_y = 0;
			for (size_t i = 0; i < 32; i++)
			{
				if (!((request.mask >> i) & 1))
					continue;
				const uint32_t value = decode<uint32_t>(packet).value();

				switch (i)
				{
					case 2:
						dprintln("    foreground: {8h}", value);
						foreground = value;
						break;
					case 3:
						dprintln("    background: {8h}", value);
						background = value;
						break;
					case 4:
						dprintln("    line-width: {}", value);
						line_width = value;
						break;
					case 14:
						dprintln("    font: {}", value);
						font = value;
						break;
					case 16:
						dprintln("    graphics-exposures: {}", value);
						graphics_exposures = value;
						break;
					case 17:
						dprintln("    clip-origin-x: {}", value);
						clip_origin_x = value;
						break;
					case 18:
						dprintln("    clip-origin-y: {}", value);
						clip_origin_y = value;
						break;
					case 19:
						dprintln("    clip-mask: {}", value);
						clip_mask = value;
						break;
					default:
						dprintln("    {8h}: {8h}", 1 << i, value);
						break;
				}
			}

			TRY(client_info.objects.insert(request.gc));
			TRY(g_objects.insert(request.gc, TRY(BAN::UniqPtr<Object>::create(Object {
				.type = Object::Type::GraphicsContext,
				.object = Object::GraphicsContext {
					.foreground = foreground,
					.background = background,
					.line_width = line_width,
					.font = font,
					.graphics_exposures = graphics_exposures,
					.clip_mask = clip_mask,
					.clip_origin_x = clip_origin_x,
					.clip_origin_y = clip_origin_y,
				}
			}))));

			break;
		}
		case X_ChangeGC:
		{
			auto request = decode<xChangeGCReq>(packet).value();

			dprintln("ChangeGC");
			dprintln("  gc:   {}", request.gc);
			dprintln("  mask: {8h}", request.mask);

			auto& object = *g_objects[request.gc];
			ASSERT(object.type == Object::Type::GraphicsContext);

			auto& gc = object.object.get<Object::GraphicsContext>();

			// TODO: support invisible background

			for (size_t i = 0; i < 32; i++)
			{
				if (!((request.mask >> i) & 1))
					continue;
				const uint32_t value = decode<uint32_t>(packet).value();

				switch (i)
				{
					case 2:
						dprintln("    foreground: {8h}", value);
						gc.foreground = value;
						break;
					case 3:
						dprintln("    background: {8h}", value);
						gc.background = value;
						break;
					case 4:
						dprintln("    line-width: {}", value);
						gc.line_width = value;
						break;
					case 14:
						dprintln("    font: {}", value);
						gc.font = value;
						break;
					case 16:
						dprintln("    graphics-exposures: {}", value);
						gc.graphics_exposures = value;
						break;
					case 17:
						dprintln("    clip-origin-x: {}", value);
						gc.clip_origin_x = value;
						break;
					case 18:
						dprintln("    clip-origin-y: {}", value);
						gc.clip_origin_y = value;
						break;
					case 19:
						dprintln("    clip-mask: {}", value);
						gc.clip_mask = value;
						gc.clip_rects.clear();
						break;
					default:
						dprintln("    {8h}: {8h}", 1 << i, value);
						break;
				}
			}

			break;
		}
		case X_SetClipRectangles:
		{
			auto request = decode<xSetClipRectanglesReq>(packet).value();

			dprintln("SetClipRectangles");
			dprintln("  gc:       {}", request.gc);
			dprintln("  ordering: {}", request.ordering);
			dprintln("  xOrigin:  {}", request.xOrigin);
			dprintln("  yOrigin:  {}", request.yOrigin);

			auto& object = *g_objects[request.gc];
			ASSERT(object.type == Object::Type::GraphicsContext);
			auto& gc = object.object.get<Object::GraphicsContext>();

			auto rects = packet.as_span<const xRectangle>();

			TRY(gc.clip_rects.resize(rects.size()));
			gc.clip_mask = UINT32_MAX;
			gc.clip_origin_x = request.xOrigin;
			gc.clip_origin_y = request.yOrigin;
			memcpy(gc.clip_rects.data(), rects.data(), rects.size() * sizeof(xRectangle));

			dprintln("  rects:");
			for (auto rect : rects)
				dprintln("  {},{}  {},{}", rect.x, rect.y, rect.width, rect.height);

			break;
		}
		case X_FreeGC:
		{
			const CARD32 gc = packet.as_span<const uint32_t>()[1];

			dprintln("FreeGC");
			dprintln("  gc: {}", gc);

			(void)TRY_REF(get_gc(client_info, gc, opcode));
			client_info.objects.remove(gc);
			g_objects.remove(gc);

			break;
		}
		case X_ClearArea:
		{
			auto request = decode<xClearAreaReq>(packet).value();

			dprintln("ClearArea");
			dprintln("  exposures: {}", request.exposures);
			dprintln("  window:    {}", request.window);
			dprintln("  x:         {}", request.x);
			dprintln("  y:         {}", request.y);
			dprintln("  width:     {}", request.width);
			dprintln("  height:    {}", request.height);

			auto& window = TRY_REF(get_window(client_info, request.window, opcode));

			if (request.width == 0)
				request.width = window.width - request.x;
			if (request.height == 0)
				request.height = window.height - request.y;

			const int32_t min_x = BAN::Math::max<int32_t>(request.x, 0);
			const int32_t min_y = BAN::Math::max<int32_t>(request.y, 0);
			const int32_t max_x = BAN::Math::min<int32_t>(request.x + request.width,  window.width);
			const int32_t max_y = BAN::Math::min<int32_t>(request.y + request.height, window.height);

			if (min_x >= max_x || min_y >= max_y)
				break;

			for (int32_t y = min_y; y < max_y; y++)
				for (int32_t x = min_x; x < max_x; x++)
					window.pixels[y * window.width + x] = window.background;

			invalidate_window(request.window, min_x, min_y, max_x - min_x, max_y - min_y);

			break;
		}
		case X_CopyArea:
		{
			auto request = decode<xCopyAreaReq>(packet).value();

			dprintln("CopyArea");
			dprintln("  srcDrawable: {}", request.srcDrawable);
			dprintln("  dstDrawable: {}", request.dstDrawable);
			dprintln("  gc:          {}", request.gc);
			dprintln("  srcX:        {}", request.srcX);
			dprintln("  srcY:        {}", request.srcY);
			dprintln("  dstX:        {}", request.dstX);
			dprintln("  dstY:        {}", request.dstY);
			dprintln("  width:       {}", request.width);
			dprintln("  heigth:      {}", request.height);

			auto [src_data_u32, src_w, src_h, _1] = TRY(get_drawable_info(client_info, request.srcDrawable, opcode));
			auto [dst_data_u32, dst_w, dst_h, _2] = TRY(get_drawable_info(client_info, request.dstDrawable, opcode));

			const auto& gc = TRY_REF(get_gc(client_info, request.gc, opcode));

			const auto get_pixel =
				[src_data_u32, src_w](int32_t x, int32_t y) -> uint32_t
				{
					return src_data_u32[y * src_w + x];
				};

			const auto set_pixel =
				[dst_data_u32, dst_w](int32_t x, int32_t y, uint32_t color) -> void
				{
					dst_data_u32[y * dst_w + x] = color;
				};

			const int32_t start_x = request.srcX < request.dstX ? request.width - 1 : 0;
			const int32_t stop_x  = request.srcX < request.dstX ? -1 : request.width;
			const int32_t step_x  = request.srcX < request.dstX ? -1 : 1;

			const int32_t start_y = request.srcY < request.dstY ? request.height - 1 : 0;
			const int32_t stop_y  = request.srcY < request.dstY ? -1 : request.height;
			const int32_t step_y  = request.srcY < request.dstY ? -1 : 1;

			for (int32_t yoff = start_y; yoff != stop_y; yoff += step_y)
			{
				const int32_t src_y = request.srcY + yoff;
				const int32_t dst_y = request.dstY + yoff;
				if (src_y < 0 || src_y >= src_h)
					continue;
				if (dst_y < 0 || dst_y >= dst_h)
					continue;
				for (int32_t xoff = start_x; xoff != stop_x; xoff += step_x)
				{
					const int32_t src_x = request.srcX + xoff;
					const int32_t dst_x = request.dstX + xoff;
					if (src_x < 0 || src_x >= src_w)
						continue;
					if (dst_x < 0 || dst_x >= dst_w)
						continue;
					if (!gc.is_clipped(dst_x, dst_y))
						set_pixel(dst_x, dst_y, get_pixel(src_x, src_y));
				}
			}

			if (gc.graphics_exposures)
			{
				xEvent event = { .u = {
					.noExposure = {
						.drawable = request.dstDrawable,
						.minorEvent = 0,
						.majorEvent = X_CopyArea,
					}
				}};
				event.u.u.type = NoExpose;
				event.u.u.sequenceNumber = client_info.sequence;
				TRY(encode(client_info.output_buffer, event));
			}

			if (g_objects[request.dstDrawable]->type == Object::Type::Window)
				invalidate_window(request.dstDrawable, request.dstX, request.dstY, request.width, request.height);

			break;
		}
		case X_PolyLine:
			TRY(poly_line(client_info, packet));
			break;
		case X_PolySegment:
			TRY(poly_segment(client_info, packet));
			break;
		case X_FillPoly:
			TRY(fill_poly(client_info, packet));
			break;
		case X_PolyFillRectangle:
			TRY(poly_fill_rectangle(client_info, packet));
			break;
		case X_PolyFillArc:
			TRY(poly_fill_arc(client_info, packet));
			break;
		case X_PutImage:
		{
			auto request = decode<xPutImageReq>(packet).value();

			dprintln("PutImage");
			dprintln("  drawable: {}", request.drawable);
			dprintln("  gc:       {}", request.gc);
			dprintln("  format:   {}", request.format);
			dprintln("  depth:    {}", request.depth);
			dprintln("  dstX:     {}", request.dstX);
			dprintln("  dstY:     {}", request.dstY);
			dprintln("  width:    {}", request.width);
			dprintln("  height:   {}", request.height);

			auto [out_data, out_w, out_h, out_depth] = TRY(get_drawable_info(client_info, request.drawable, opcode));

			const auto& gc = TRY_REF(get_gc(client_info, request.gc, opcode));

			if (packet.empty())
			{
				dwarnln("no data in PutImage");
				break;
			}

			put_image({
				.out_data = out_data,
				.out_x = request.dstX,
				.out_y = request.dstY,
				.out_w = out_w,
				.out_h = out_h,
				.out_depth = out_depth,
				.in_data = packet.data(),
				.in_x = 0,
				.in_y = 0,
				.in_w = request.width,
				.in_h = request.height,
				.in_depth = request.depth,
				.w = request.width,
				.h = request.height,
				.left_pad = request.leftPad,
				.format = request.format,
				.gc = gc,
			});

			if (g_objects[request.drawable]->type == Object::Type::Window)
				invalidate_window(request.drawable, request.dstX, request.dstY, request.width, request.height);

			break;
		}
		case X_GetImage:
		{
			auto request = decode<xGetImageReq>(packet).value();

			dprintln("GetImage");
			dprintln("  drawable:  {}", request.drawable);
			dprintln("  format:    {}", request.format);
			dprintln("  x:         {}", request.x);
			dprintln("  y:         {}", request.y);
			dprintln("  width:     {}", request.width);
			dprintln("  height:    {}", request.height);
			dprintln("  planeMask: {}", request.planeMask);

			auto [in_data, in_w, in_h, in_depth] = TRY(get_drawable_info(client_info, request.drawable, opcode));

			const auto dwords = image_dwords(request.width, request.height, in_depth);

			xGetImageReply reply {
				.type = X_Reply,
				.depth = in_depth,
				.sequenceNumber = client_info.sequence,
				.length = dwords,
				.visual = g_visual.visualID,
			};
			TRY(encode(client_info.output_buffer, reply));

			const auto old_size = client_info.output_buffer.size();
			TRY(client_info.output_buffer.resize(old_size + dwords * 4));

			auto* out_data = client_info.output_buffer.data() + old_size;

			get_image({
				.out_data = out_data,
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

			break;
		}
		case X_PolyText8:
			TRY(poly_text(client_info, packet, false));
			break;
		case X_PolyText16:
			TRY(poly_text(client_info, packet, true));
			break;
		case X_ImageText8:
			TRY(image_text(client_info, packet, false));
			break;
		case X_ImageText16:
			TRY(image_text(client_info, packet, true));
			break;
		case X_CreateColormap:
		{
			auto request = decode<xCreateColormapReq>(packet).value();

			dprintln("CreateColormap");
			dprintln("  alloc:  {}", request.alloc);
			dprintln("  mid:    {}", request.mid);
			dprintln("  window: {}", request.window);
			dprintln("  visual: {}", request.visual);

			break;
		}
		case X_AllocColor:
		{
			auto request = decode<xAllocColorReq>(packet).value();

			dprintln("AllocColor");
			dprintln("  cmap:  {}", request.cmap);
			dprintln("  red:   {}", request.red);
			dprintln("  green: {}", request.green);
			dprintln("  blue:  {}", request.blue);

			// FIXME
			xAllocColorReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.red = request.red,
				.green = request.green,
				.blue = request.blue,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_AllocNamedColor:
		{
			auto request = decode<xAllocNamedColorReq>(packet).value();

			dprintln("AllocNamedColor");
			dprintln("  cmap:   {}", request.cmap);
			dprintln("  name:   {}", BAN::StringView((const char*)packet.data(), request.nbytes));

			// FIXME
			xAllocNamedColorReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = 0,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_QueryColors:
		{
			auto request = decode<xQueryColorsReq>(packet).value();

			dprintln("QueryColors");
			dprintln("  cmap: {}", request.cmap);

			const size_t count = packet.size() / 4;

			xQueryColorsReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = static_cast<CARD32>(2 * count),
				.nColors = static_cast<CARD16>(count),
			};
			TRY(encode(client_info.output_buffer, reply));

			dprintln("  colors:");
			for (size_t i = 0; i < count; i++)
			{
				const auto color = decode<uint32_t>(packet).value();
				dprintln("    {8h}", color);

				const uint8_t r = (color >> 16) & 0xFF;
				const uint8_t g = (color >>  8) & 0xFF;
				const uint8_t b = (color >>  0) & 0xFF;

				TRY(encode<uint16_t>(client_info.output_buffer, (r << 8) | r));
				TRY(encode<uint16_t>(client_info.output_buffer, (g << 8) | g));
				TRY(encode<uint16_t>(client_info.output_buffer, (b << 8) | b));
				TRY(encode<uint16_t>(client_info.output_buffer, 0));
			}

			break;
		}
		case X_LookupColor:
		{
			auto request = decode<xLookupColorReq>(packet).value();

			auto name = BAN::StringView((char*)packet.data(), request.nbytes);
			dprintln("LookupColor");
			dprintln("  cmap:  {}", request.cmap);
			dprintln("  color: {}", name);

			// FIXME:
			xLookupColorReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.exactRed = 0,
				.exactGreen = 0,
				.exactBlue = 0,
				.screenRed = 0,
				.screenGreen = 0,
				.screenBlue = 0,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_CreateCursor:
		{
			auto request = decode<xCreateCursorReq>(packet).value();

			dprintln("CreateCursor");
			dprintln("  cid:       {}", request.cid);
			dprintln("  source:    {}", request.source);
			dprintln("  mask:      {}", request.mask);
			dprintln("  foreRed:   {}", request.foreRed);
			dprintln("  foreGreen: {}", request.foreGreen);
			dprintln("  foreBlue:  {}", request.foreBlue);
			dprintln("  backRed:   {}", request.backRed);
			dprintln("  backGreen: {}", request.backGreen);
			dprintln("  backBlue:  {}", request.backBlue);
			dprintln("  x:         {}", request.x);
			dprintln("  y:         {}", request.y);

			const auto& source = TRY_REF(get_pixmap(client_info, request.source, opcode));
			ASSERT(source.depth == 1);

			const uint32_t foreground =
				static_cast<uint32_t>(request.foreRed   >> 8) << 16 |
				static_cast<uint32_t>(request.foreBlue  >> 8) <<  8 |
				static_cast<uint32_t>(request.foreGreen >> 8) <<  0;
			const uint32_t background =
				static_cast<uint32_t>(request.backRed   >> 8) << 16 |
				static_cast<uint32_t>(request.backBlue  >> 8) <<  8 |
				static_cast<uint32_t>(request.backGreen >> 8) <<  0;

			BAN::Vector<uint32_t> pixels;
			TRY(pixels.resize(source.width * source.height));

			auto* source_data_u32 = reinterpret_cast<const uint32_t*>(source.data.data());
			for (size_t i = 0; i < source.width * source.height; i++)
				pixels[i] = 0xFF000000 | (source_data_u32[i] ? foreground : background);

			if (request.mask != None)
			{
				const auto& mask = TRY_REF(get_pixmap(client_info, request.mask, opcode));
				ASSERT(mask.depth == 1);
				ASSERT(mask.width == source.width);
				ASSERT(mask.height == source.height);

				auto* mask_data_u32 = reinterpret_cast<const uint32_t*>(mask.data.data());
				for (size_t i = 0; i < source.width * source.height; i++)
					if (!mask_data_u32[i])
						pixels[i] = 0;
			}

			BAN::UniqPtr<PlatformCursor> cursor;
			if (g_platform_ops.create_bitmap_cursor)
			{
				auto cursor_or_error = g_platform_ops.create_bitmap_cursor(pixels.data(), source.width, source.height, request.x, request.y);
				if (!cursor_or_error.is_error())
					cursor = cursor_or_error.release_value();
			}

			TRY(client_info.objects.insert(request.cid));
			TRY(g_objects.insert(
				request.cid,
				TRY(BAN::UniqPtr<Object>::create(Object {
					.type = Object::Type::Cursor,
					.object = BAN::move(cursor),
				}))
			));

			break;
		}
		case X_CreateGlyphCursor:
			TRY(create_glyph_cursor(client_info, packet));
			break;
		case X_FreeCursor:
		{
			const auto cid = packet.as_span<const uint32_t>()[1];

			dprintln("FreeCursor");
			dprintln("  cid:       {}", cid);

			client_info.objects.remove(cid);
			g_objects.remove(cid);

			break;
		}
		case X_QueryBestSize:
		{
			auto request = decode<xQueryBestSizeReq>(packet).value();

			dprintln("QueryBestSize");
			dprintln("  class:    {}", request.c_class);
			dprintln("  drawable: {}", request.drawable);
			dprintln("  width:    {}", request.width);
			dprintln("  height:   {}", request.height);

			// FIXME
			xQueryBestSizeReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.width = request.width,
				.height = request.height,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_QueryExtension:
		{
			auto request = decode<xQueryExtensionReq>(packet).value();

			auto name = BAN::StringView((char*)packet.data(), request.nbytes);
			dprintln("QueryExtension {}", name);

			xQueryExtensionReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.present = xFalse,
				.major_opcode = 0,
				.first_event = 0,
				.first_error = 0,
			};

			for (const auto& extension : g_extensions)
			{
				if (extension.name != name)
					continue;
				reply.present = xTrue;
				reply.major_opcode = extension.major_opcode;
				reply.first_event = extension.event_base;
				reply.first_error = extension.error_base;
				break;
			}

			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_ListExtensions:
		{
			dprintln("ListExtensions");

			CARD8 extension_count = 0;
			CARD32 extension_length = 0;
			for (const auto& extension : g_extensions)
			{
				if (extension.handler == nullptr)
					continue;
				extension_count++;
				extension_length += extension.name.size() + 1;
			}

			xListExtensionsReply reply {
				.type = X_Reply,
				.nExtensions = extension_count,
				.sequenceNumber = client_info.sequence,
				.length = (extension_length + 3) / 4,
			};
			TRY(encode(client_info.output_buffer, reply));

			for (const auto& extension : g_extensions)
			{
				if (extension.handler == nullptr)
					continue;
				TRY(encode<BYTE>(client_info.output_buffer, extension.name.size()));
				TRY(encode(client_info.output_buffer, extension.name));
			}

			for (size_t i = 0; (extension_length + i) % 4; i++)
				TRY(encode(client_info.output_buffer, '\0'));

			break;
		}
		case X_GetKeyboardMapping:
		{
			auto request = decode<xGetKeyboardMappingReq>(packet).value();

			dprintln("GetKeyboardMapping");
			dprintln("  firstKeyCode: {}", request.firstKeyCode);
			dprintln("  count:        {}", request.count);

			if (request.firstKeyCode < g_keymap_min_keycode || request.firstKeyCode + request.count - 1 > g_keymap_max_keycode)
			{
				xError error {
					.type = X_Error,
					.errorCode = BadValue,
					.sequenceNumber = client_info.sequence,
					.resourceID = 0, // TODO: should this be something meaningful?
					.minorCode = 0,
					.majorCode = opcode,
				};
				TRY(encode(client_info.output_buffer, error));
				break;
			}

			xGetKeyboardMappingReply reply {
				.type = X_Reply,
				.keySymsPerKeyCode = g_keymap_layers,
				.sequenceNumber = client_info.sequence,
				.length = static_cast<CARD32>(request.count * g_keymap_layers),
			};
			TRY(encode(client_info.output_buffer, reply));
			TRY(encode(client_info.output_buffer, BAN::Span(
				g_keymap + request.firstKeyCode - g_keymap_min_keycode,
				request.count
			)));

			break;
		}
		case X_GetKeyboardControl:
		{
			dprintln("GetKeyboardControl");

			// FIXME:
			xGetKeyboardControlReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = 5,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_GetPointerMapping:
		{
			dprintln("GetPointerMapping");

			xGetPointerMappingReply reply {
				.type = X_Reply,
				.nElts = 8,
				.sequenceNumber = client_info.sequence,
				.length = 2,
			};
			TRY(encode(client_info.output_buffer, reply));

			for (size_t i = 1; i <= 8; i++)
				TRY(encode<CARD8>(client_info.output_buffer, i));

			break;
		}
		case X_GetModifierMapping:
		{
			dprintln("GetModifierMapping");

			xGetModifierMappingReply reply {
				.type = X_Reply,
				.numKeyPerModifier = g_modifier_layers,
				.sequenceNumber = client_info.sequence,
				.length = 2 * g_modifier_layers,
			};
			TRY(encode(client_info.output_buffer, reply));
			TRY(encode(client_info.output_buffer, g_modifier_map));

			break;
		}
		default:
			dwarnln("unsupported opcode {}", s_opcode_to_name[packet[0]]);
			for (size_t i = 0; i < packet.size(); i++)
				fprintf(stddbg, " %02x", packet[i]);
			fprintf(stddbg, "\n");
			break;
	}

	return {};
}

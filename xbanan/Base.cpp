#include "Definitions.h"
#include "Drawing.h"
#include "Extensions.h"
#include "Font.h"
#include "Image.h"
#include "Keymap.h"
#include "SafeGetters.h"
#include "Utils.h"

#include <X11/X.h>

#include <time.h>
#include <sys/epoll.h>

struct Selection
{
	CARD32 time;
	WINDOW owner;
};

static BAN::HashMap<ATOM, Selection> g_selections;

static CARD16 s_keymask { 0 };
static CARD16 s_butmask { 0 };

static WINDOW s_focus_window { None };

static BYTE s_pressed_keys[32] {};

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
		.majorVersion = client_prefix.majorVersion,
		.minorVersion = client_prefix.minorVersion,
		.length = 8 + 2*format_count + (8 + 0 + sz_xWindowRoot + sz_xDepth + sz_xVisualType) / 4,
	};
	TRY(encode(client_info.output_buffer, setup_prefix));

	ASSERT((client_info.fd >> 24) == 0);

	xConnSetup setup {
		.release = 0,
		.ridBase = static_cast<CARD32>(client_info.fd << 24),
		.ridMask = 0x00FFFFFF,
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
	TRY(encode(client_info.output_buffer, "banan!!!"_sv));
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

static Object::Cursor& get_cursor_safe(CURSOR cid)
{
	static Object::Cursor dummy {};
	auto it = g_objects.find(cid);
	if (it == g_objects.end())
		return dummy;
	if (it->value->type != Object::Type::Cursor)
		return dummy;
	return it->value->object.get<Object::Cursor>();
}

static BAN::ErrorOr<void> send_visibility_events_recursively(Client& client_info, WINDOW wid, bool visible)
{
	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);

	auto& window = object.object.get<Object::Window>();

	if (window.c_class == InputOutput)
	{
		if (window.event_mask & VisibilityChangeMask)
		{
			xEvent event = { .u = {
				.visibility = {
					.window = wid,
					.state = static_cast<CARD8>(visible ? 0 : 2),
				}
			}};
			event.u.u.type = VisibilityNotify;
			event.u.u.sequenceNumber = client_info.sequence;
			TRY(encode(client_info.output_buffer, event));
		}
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

		const auto child_w = BAN::Math::min<int32_t>(w - child_window.x, child_window.texture().width() - child_x);
		const auto child_h = BAN::Math::min<int32_t>(h - child_window.y, child_window.texture().height() - child_y);

		invalidate_window_recursive(
			child_wid,
			child_x, child_y,
			child_w, child_h
		);
	}

	if (window.window.has<BAN::UniqPtr<LibGUI::Window>>())
		return;

	auto& parent_object = *g_objects[window.parent];
	ASSERT(parent_object.type == Object::Type::Window);

	auto& parent_window = parent_object.object.get<Object::Window>();
	parent_window.texture().copy_texture(
		window.texture(),
		window.x + x,
		window.y + y,
		x,
		y,
		w,
		h
	);
}

void invalidate_window(WINDOW wid, int32_t x, int32_t y, int32_t w, int32_t h)
{
	ASSERT(wid != g_root.windowId);

	for (;;)
	{
		auto& object = *g_objects[wid];
		ASSERT(object.type == Object::Type::Window);

		auto& window = object.object.get<Object::Window>();
		if (!window.mapped)
			return;
		if (window.window.has<BAN::UniqPtr<LibGUI::Window>>())
			break;

		x += window.x;
		y += window.y;
		wid = window.parent;
	}

	invalidate_window_recursive(wid, x, y, w, h);

	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);

	auto& window = object.object.get<Object::Window>();
	ASSERT(window.window.has<BAN::UniqPtr<LibGUI::Window>>());

	auto& texture = window.texture();
	const auto min_x = BAN::Math::max<int32_t>(x, 0);
	const auto min_y = BAN::Math::max<int32_t>(y, 0);
	const auto max_x = BAN::Math::min<int32_t>(x + w, texture.width());
	const auto max_y = BAN::Math::min<int32_t>(y + h, texture.height());

	uint32_t* pixels = texture.pixels().data();
	for (auto y = min_y; y < max_y; y++)
	{
		for (auto x = min_x; x < max_x; x++)
		{
			auto& pixel = pixels[y * texture.width() + x];
			if (pixel == LibGUI::Texture::color_invisible)
				pixel = 0x00000000;
			else
				pixel |= 0xFF000000;
		}
	}

	window.window.get<BAN::UniqPtr<LibGUI::Window>>()->invalidate(min_x, min_y, max_x - min_x, max_y - min_y);
}

static BAN::ErrorOr<void> map_window(Client& client_info, WINDOW wid)
{
	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);

	auto& window = object.object.get<Object::Window>();
	if (window.mapped)
		return {};

	auto& texture = window.texture();
	texture.clear();

	window.mapped = true;

	invalidate_window(wid, 0, 0, texture.width(), texture.height());

	if (window.window.has<BAN::UniqPtr<LibGUI::Window>>())
	{
		auto& gui_window = window.window.get<BAN::UniqPtr<LibGUI::Window>>();
		auto attributes = gui_window->get_attributes();
		attributes.shown = true;
		gui_window->set_attributes(attributes);

		gui_window->texture().clear();
		gui_window->invalidate();
	}

	if (window.event_mask & StructureNotifyMask)
	{
		xEvent event = { .u = {
			.mapNotify = {
				.event = wid,
				.window = wid,
				.override = xFalse,
			}
		}};
		event.u.u.type = MapNotify;
		event.u.u.sequenceNumber = client_info.sequence;
		TRY(encode(client_info.output_buffer, event));
	}

	auto& parent_object = *g_objects[window.parent];
	ASSERT(parent_object.type == Object::Type::Window);

	auto& parent_window = parent_object.object.get<Object::Window>();
	if (parent_window.event_mask & SubstructureNotifyMask)
	{
		xEvent event = { .u = {
			.mapNotify = {
				.event = window.parent,
				.window = wid,
				.override = xFalse,
			}
		}};
		event.u.u.type = MapNotify;
		event.u.u.sequenceNumber = client_info.sequence;
		TRY(encode(client_info.output_buffer, event));
	}

	if (is_visible(window.parent))
		TRY(send_visibility_events_recursively(client_info, wid, true));

	if (window.event_mask & ExposureMask)
	{
		xEvent event = { .u = {
			.expose = {
				.window = wid,
				.x = 0,
				.y = 0,
				.width = static_cast<CARD16>(texture.width()),
				.height = static_cast<CARD16>(texture.height()),
			}
		}};
		event.u.u.type = Expose;
		event.u.u.sequenceNumber = client_info.sequence;
		TRY(encode(client_info.output_buffer, event));
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

	if (window.window.has<BAN::UniqPtr<LibGUI::Window>>())
	{
		auto& gui_window = window.window.get<BAN::UniqPtr<LibGUI::Window>>();
		auto attributes = gui_window->get_attributes();
		attributes.shown = false;
		gui_window->set_attributes(attributes);
	}

	if (is_visible(window.parent))
		TRY(send_visibility_events_recursively(client_info, wid, false));

	if (window.event_mask & StructureNotifyMask)
	{
		xEvent event = { .u = {
			.unmapNotify = {
				.event = wid,
				.window = wid,
				.fromConfigure = xFalse,
			}
		}};
		event.u.u.type = UnmapNotify;
		event.u.u.sequenceNumber = client_info.sequence;
		TRY(encode(client_info.output_buffer, event));
	}

	auto& parent_object = *g_objects[window.parent];
	ASSERT(parent_object.type == Object::Type::Window);

	auto& parent_window = parent_object.object.get<Object::Window>();
	if (parent_window.event_mask & SubstructureNotifyMask)
	{
		xEvent event = { .u = {
			.unmapNotify = {
				.event = window.parent,
				.window = wid,
				.fromConfigure = xFalse,
			}
		}};
		event.u.u.type = UnmapNotify;
		event.u.u.sequenceNumber = client_info.sequence;
		TRY(encode(client_info.output_buffer, event));
	}

	return {};
}

static BAN::ErrorOr<void> destroy_window(Client& client_info, WINDOW wid)
{
	TRY(unmap_window(client_info, wid));

	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);

	auto& window = object.object.get<Object::Window>();

	for (auto child_wid : window.children)
		TRY(destroy_window(client_info, child_wid));

	if (window.event_mask & StructureNotifyMask)
	{
		xEvent event = { .u = {
			.destroyNotify = {
				.event = wid,
				.window = wid,
			}
		}};
		event.u.u.type = DestroyNotify;
		event.u.u.sequenceNumber = client_info.sequence;
		TRY(encode(client_info.output_buffer, event));
	}

	auto& parent_object = *g_objects[window.parent];
	ASSERT(parent_object.type == Object::Type::Window);

	auto& parent_window = parent_object.object.get<Object::Window>();
	if (parent_window.event_mask & SubstructureNotifyMask)
	{
		xEvent event = { .u = {
			.destroyNotify = {
				.event = window.parent,
				.window = wid,
			}
		}};
		event.u.u.type = DestroyNotify;
		event.u.u.sequenceNumber = client_info.sequence;
		TRY(encode(client_info.output_buffer, event));
	}

	for (size_t i = 0; i < parent_window.children.size(); i++)
	{
		if (parent_window.children[i] != wid)
			continue;
		parent_window.children.remove(i);
		break;
	}

	if (window.window.has<BAN::UniqPtr<LibGUI::Window>>())
	{
		const auto server_fd = window.window.get<BAN::UniqPtr<LibGUI::Window>>()->server_fd();
		epoll_ctl(g_epoll_fd, EPOLL_CTL_DEL, server_fd, nullptr);
		g_epoll_thingies.remove(server_fd);
	}

	client_info.objects.remove(wid);
	g_objects.remove(wid);

	return {};
}

static WINDOW find_child_window(WINDOW wid, int32_t& x, int32_t& y)
{
	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);

	auto& window = object.object.get<Object::Window>();

	uint32_t width, height;
	if (wid == g_root.windowId)
	{
		width = g_root.pixWidth;
		height = g_root.pixHeight;
	}
	else
	{
		auto& texture = window.texture();
		width = texture.width();
		height = texture.height();
	}
	if (x < 0 || y < 0 || x >= width || y >= height)
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

static BAN::Vector<WINDOW> get_path_to_child(WINDOW wid, int32_t x, int32_t y)
{
	BAN::Vector<WINDOW> result;

	const auto window_contains =
		[](const Object::Window& window, int32_t x, int32_t y) -> bool
		{
			const auto& texture = window.texture();
			return x >= 0 && y >= 0 && x < texture.width() && y < texture.height();
		};

	for (;;)
	{
		const auto& object = *g_objects[wid];
		ASSERT(object.type == Object::Type::Window);

		const auto& window = object.object.get<Object::Window>();
		if (!window_contains(window, x, y))
			break;
		
		MUST(result.push_back(wid));

		const WINDOW old_wid = wid;
		for (auto child_wid : window.children)
		{
			const auto& child_object = *g_objects[child_wid];
			ASSERT(child_object.type == Object::Type::Window);

			const auto& child_window = child_object.object.get<Object::Window>();
			if (!window_contains(child_window, x - child_window.x, y - child_window.y))
				continue;
			wid = child_wid;
			break;
		}

		if (old_wid == wid)
			break;
	}

	return result;
}

static void send_enter_and_leave_events(Client& client_info, WINDOW wid, int32_t old_x, int32_t old_y, int32_t new_x, int32_t new_y)
{
	// FIXME: correct event_x and event_y values in events

	const auto old_child_path = get_path_to_child(wid, old_x, old_y);
	const auto new_child_path = get_path_to_child(wid, new_x, new_y);

	size_t common_ancestors = 0;
	while (common_ancestors < old_child_path.size() && common_ancestors < new_child_path.size())
	{
		if (old_child_path[common_ancestors] != new_child_path[common_ancestors])
			break;
		common_ancestors++;
	}

	if (old_child_path.size() == common_ancestors && new_child_path.size() == common_ancestors)
		return;

	const bool linear = (common_ancestors == old_child_path.size() || common_ancestors == new_child_path.size());

	const auto get_flags =
		[](WINDOW wid) -> BYTE
		{
			return ELFlagSameScreen | (s_focus_window == wid ? ELFlagFocus : 0);
		};

	size_t leave_events = old_child_path.size() - common_ancestors;
	if (linear && common_ancestors && old_child_path.size() < new_child_path.size())
		leave_events++;

	size_t enter_events = new_child_path.size() - common_ancestors;
	if (linear && common_ancestors && new_child_path.size() < old_child_path.size())
		enter_events++;

	for (size_t i = 0; i < leave_events; i++)
	{
		const bool first = (i == 0);
		const BYTE detail =
			[&]() -> BYTE {
				if (!linear)
					return first ? NotifyNonlinear : NotifyNonlinearVirtual;
				if (!first)
					return NotifyVirtual;
				return (old_child_path.size() > new_child_path.size())
					? NotifyAncestor
					: NotifyInferior;
			}();

		const auto& wid = old_child_path[old_child_path.size() - i - 1];

		auto& object = *g_objects[wid];
		ASSERT(object.type == Object::Type::Window);
		auto& window = object.object.get<Object::Window>();
		if (!(window.event_mask & LeaveWindowMask))
			continue;

		xEvent xevent { .u = {
			.enterLeave = {
				.time = static_cast<CARD32>(time(nullptr)),
				.root = g_root.windowId,
				.event = wid,
				.child = first ? static_cast<WINDOW>(None) : old_child_path.back(),
				.rootX = static_cast<INT16>(old_x),
				.rootY = static_cast<INT16>(old_y),
				.eventX = static_cast<INT16>(old_x),
				.eventY = static_cast<INT16>(old_y),
				.state = static_cast<KeyButMask>(s_keymask | s_butmask),
				.mode = NotifyNormal,
				.flags = get_flags(wid),
			}
		}};
		xevent.u.u.type = LeaveNotify,
		xevent.u.u.detail = detail;
		xevent.u.u.sequenceNumber = client_info.sequence;
		MUST(encode(client_info.output_buffer, xevent));
	}

	for (size_t i = 0; i < enter_events; i++)
	{
		const bool last = (i == enter_events - 1);
		const BYTE detail =
			[&]() -> BYTE {
				if (!linear)
					return last ? NotifyNonlinear : NotifyNonlinearVirtual;
				if (!last)
					return NotifyVirtual;
				return (old_child_path.size() > new_child_path.size())
					? NotifyInferior
					: NotifyAncestor;
			}();

		const auto& wid = new_child_path[new_child_path.size() - enter_events + i];

		auto& object = *g_objects[wid];
		ASSERT(object.type == Object::Type::Window);
		auto& window = object.object.get<Object::Window>();
		if (!(window.event_mask & EnterWindowMask))
			continue;

		xEvent xevent { .u = {
			.enterLeave = {
				.time = static_cast<CARD32>(time(nullptr)),
				.root = g_root.windowId,
				.event = wid,
				.child = last ? static_cast<WINDOW>(None) : new_child_path.back(),
				.rootX = static_cast<INT16>(new_x),
				.rootY = static_cast<INT16>(new_y),
				.eventX = static_cast<INT16>(new_x),
				.eventY = static_cast<INT16>(new_y),
				.state = static_cast<KeyButMask>(s_keymask | s_butmask),
				.mode = NotifyNormal,
				.flags = get_flags(wid),
			}
		}};
		xevent.u.u.type = EnterNotify,
		xevent.u.u.detail = detail;
		xevent.u.u.sequenceNumber = client_info.sequence;
		MUST(encode(client_info.output_buffer, xevent));
	}
}

static void send_exposure_recursive(Client& client_info, WINDOW wid)
{
	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);

	auto& window = object.object.get<Object::Window>();
	if (!window.mapped)
		return;

	for (auto child_wid : window.children)
		send_exposure_recursive(client_info, child_wid);

	window.texture().clear();

	if (window.event_mask & ExposureMask)
	{
		xEvent event = { .u = {
			.expose = {
				.window = wid,
				.x = 0,
				.y = 0,
				.width = static_cast<CARD16>(window.texture().width()),
				.height = static_cast<CARD16>(window.texture().height()),
			}
		}};
		event.u.u.type = Expose;
		event.u.u.sequenceNumber = client_info.sequence;
		MUST(encode(client_info.output_buffer, event));
	}
}

static void on_window_resize_event(Client& client_info, WINDOW wid)
{
	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);

	auto& window = object.object.get<Object::Window>();
	if (window.event_mask & StructureNotifyMask)
	{
		xEvent event = { .u = {
			.configureNotify = {
				.event = wid,
				.window = wid,
				.aboveSibling = xFalse,
				.x = static_cast<INT16>(window.x),
				.y = static_cast<INT16>(window.y),
				.width = static_cast<CARD16>(window.texture().width()),
				.height = static_cast<CARD16>(window.texture().height()),
				.borderWidth = 0,
				.override = xFalse,
			}
		}};
		event.u.u.type = ConfigureNotify;
		event.u.u.sequenceNumber = client_info.sequence;
		MUST(encode(client_info.output_buffer, event));
	}

	auto& parent_object = *g_objects[window.parent];
	ASSERT(parent_object.type == Object::Type::Window);

	auto& parent_window = parent_object.object.get<Object::Window>();
	if (parent_window.event_mask & SubstructureNotifyMask)
	{
		xEvent event = { .u = {
			.configureNotify = {
				.event = window.parent,
				.window = wid,
				.aboveSibling = xFalse,
				.x = static_cast<INT16>(window.x),
				.y = static_cast<INT16>(window.y),
				.width = static_cast<CARD16>(window.texture().width()),
				.height = static_cast<CARD16>(window.texture().height()),
				.borderWidth = 0,
				.override = xFalse,
			}
		}};
		event.u.u.type = ConfigureNotify;
		event.u.u.sequenceNumber = client_info.sequence;
		MUST(encode(client_info.output_buffer, event));
	}

	send_exposure_recursive(client_info, wid);

	invalidate_window(wid, 0, 0, window.texture().width(), window.texture().height());
}

static void on_window_focus_event(Client& client_info, WINDOW wid, bool focused)
{
	if (focused)
		s_focus_window = wid;

	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);

	auto& window = object.object.get<Object::Window>();
	if (window.focused == focused)
		return;

	window.focused = focused;

	if (!(window.event_mask & FocusChangeMask))
		return;

	// FIXME: handle childs

	xEvent xevent { .u = {
		.focus = {
			.window = wid,
			.mode = NotifyNormal,
		}
	}};
	xevent.u.u.type = focused ? FocusIn : FocusOut,
	xevent.u.u.detail = NotifyNonlinear;
	xevent.u.u.sequenceNumber = client_info.sequence;
	MUST(encode(client_info.output_buffer, xevent));
}

static void send_key_button_pointer_event(Client& client_info, WINDOW root_wid, BYTE detail, uint32_t event_mask, BYTE event_type)
{
	int32_t root_x, root_y;
	int32_t event_x, event_y;

	{
		auto& object = *g_objects[root_wid];
		ASSERT(object.type == Object::Type::Window);
		auto& window = object.object.get<Object::Window>();
		root_x = event_x = window.cursor_x;
		root_y = event_y = window.cursor_y;
	}

	const auto child_wid = find_child_window(root_wid, event_x, event_y);

	auto wid = child_wid;
	while (wid != None)
	{
		auto& object = *g_objects[wid];
		ASSERT(object.type == Object::Type::Window);
		auto& window = object.object.get<Object::Window>();
		if (window.event_mask & event_mask)
			break;

		event_x += window.x;
		event_y += window.y;
		wid = window.parent;
	}

	if (wid == None)
	{
		if (event_type == MotionNotify && s_butmask == 0)
			return;
		wid = root_wid;
	}

	xEvent xevent { .u = {
		.keyButtonPointer = {
			.time = static_cast<CARD32>(time(nullptr)),
			.root = g_root.windowId,
			.event = wid,
			.child = child_wid,
			.rootX = static_cast<INT16>(root_x),
			.rootY = static_cast<INT16>(root_y),
			.eventX = static_cast<INT16>(event_x),
			.eventY = static_cast<INT16>(event_y),
			.state = static_cast<KeyButMask>(s_keymask | s_butmask),
			.sameScreen = xTrue,
		}
	}};
	xevent.u.u.type = event_type,
	xevent.u.u.detail = detail;
	xevent.u.u.sequenceNumber = client_info.sequence;
	MUST(encode(client_info.output_buffer, xevent));
}

static void update_cursor_position_recursive(WINDOW wid, int32_t new_x, int32_t new_y)
{
	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);
	auto& window = object.object.get<Object::Window>();

	window.cursor_x = new_x;
	window.cursor_y = new_y;

	for (auto child_wid : window.children)
	{
		auto& child_object = *g_objects[child_wid];
		ASSERT(child_object.type == Object::Type::Window);
		auto& child_window = child_object.object.get<Object::Window>();
		update_cursor_position_recursive(child_wid, new_x - child_window.x, new_y - child_window.y);
	}
}

static void update_cursor(WINDOW wid, int32_t old_x, int32_t old_y, int32_t new_x, int32_t new_y)
{
	const auto old_wid = find_child_window(wid, old_x, old_y);
	const auto new_wid = find_child_window(wid, new_x, new_y);
	if (old_wid == new_wid || old_wid == None || new_wid == None)
		return;

	const auto& old_window = g_objects[old_wid]->object.get<Object::Window>();
	const auto& new_window = g_objects[new_wid]->object.get<Object::Window>();
	if (old_window.cursor == new_window.cursor)
		return;

	auto& window = g_objects[wid]->object.get<Object::Window>();
	auto& gui_window = window.window.get<BAN::UniqPtr<LibGUI::Window>>();

	const auto& cursor = get_cursor_safe(window.cursor);
	gui_window->set_cursor(cursor.width, cursor.height, cursor.pixels.span(), cursor.origin_x, cursor.origin_y);
}

static void on_mouse_move_event(Client& client_info, WINDOW wid, int32_t x, int32_t y)
{
	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);
	auto& window = object.object.get<Object::Window>();

	update_cursor(wid, window.cursor_x, window.cursor_y, x, y);

	send_enter_and_leave_events(client_info, wid, window.cursor_x, window.cursor_y, x, y);

	update_cursor_position_recursive(wid, x, y);

	// TODO: optimize with PointerMotionHint
	uint32_t event_mask = PointerMotionMask | PointerMotionHintMask;
	if (s_butmask) event_mask |= ButtonMotionMask;
	if (s_butmask & Button1Mask) event_mask |= Button1MotionMask;
	if (s_butmask & Button2Mask) event_mask |= Button2MotionMask;
	if (s_butmask & Button3Mask) event_mask |= Button3MotionMask;
	if (s_butmask & Button4Mask) event_mask |= Button4MotionMask;
	if (s_butmask & Button5Mask) event_mask |= Button5MotionMask;
	send_key_button_pointer_event(client_info, wid, NotifyNormal, event_mask, MotionNotify);
}

static void on_mouse_button_event(Client& client_info, WINDOW wid, uint8_t xbutton, bool pressed)
{
	uint16_t mask = 0;
	switch (xbutton)
	{
		case Button1: mask = Button1Mask; break;
		case Button2: mask = Button2Mask; break;
		case Button3: mask = Button3Mask; break;
		case Button4: mask = Button4Mask; break;
		case Button5: mask = Button5Mask; break;
	}

	if (pressed)	
		s_butmask |= mask;
	else
		s_butmask &= ~mask;

	send_key_button_pointer_event(
		client_info,
		wid,
		xbutton,
		pressed ? ButtonPressMask : ButtonReleaseMask,
		pressed ? ButtonPress     : ButtonRelease
	);
}

static void on_key_event(Client& client_info, WINDOW wid, LibGUI::EventPacket::KeyEvent::event_t event)
{
	const uint8_t xkeycode = event.scancode + g_keymap_min_keycode;
	if (xkeycode < g_keymap_min_keycode)
	{
		dwarnln("cannot send keycode {}", xkeycode);
		return;
	}

	{
		const uint8_t byte = xkeycode / 8;
		const uint8_t bit = xkeycode % 8;
		if (event.pressed())
			s_pressed_keys[byte] |= 1 << bit;
		else
			s_pressed_keys[byte] &= ~(1 << bit);
	}

	s_keymask = 0;
	if (event.shift())
		s_keymask |= ShiftMask;
	if (event.caps_lock())
		s_keymask |= LockMask;
	if (event.ctrl())
		s_keymask |= ControlMask;
	if (event.alt())
		s_keymask |= Mod1Mask;

	send_key_button_pointer_event(
		client_info,
		wid,
		xkeycode,
		event.pressed() ? KeyPressMask : KeyReleaseMask,
		event.pressed() ? KeyPress     : KeyRelease
	);
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
						if (value == None || value == ParentRelative)
							background = LibGUI::Texture::color_invisible;
						else
							dprintln("    {8h}: {8h}", 1 << i, value);
						break;
					case 1:
						background = value;
						break;
					case 11:
						event_mask = value;
						break;
					case 14:
						cursor_id = value;
						break;
					default:
						dprintln("    {8h}: {8h}", 1 << i, value);
						break;
				}
			}

			decltype(Object::Window::window) window;

			LibGUI::Window* gui_window_ptr = nullptr;

			if (request.parent != g_root.windowId)
				window = TRY(LibGUI::Texture::create(request.width, request.height, background));
			else
			{
				auto attributes = LibGUI::Window::default_attributes;
				attributes.shown = false;
				attributes.title_bar = false;
				attributes.resizable = true;

				auto gui_window = TRY(LibGUI::Window::create(request.width, request.height, "window?"_sv, attributes));
				gui_window->texture().set_bg_color(background);
				gui_window_ptr = gui_window.ptr();

				if (cursor_id != None)
				{
					const auto& cursor = get_cursor_safe(cursor_id);
					gui_window->set_cursor(cursor.width, cursor.height, cursor.pixels.span(), cursor.origin_x, cursor.origin_y);
				}

				TRY(g_epoll_thingies.insert(gui_window->server_fd(), {
					.type = EpollThingy::Type::Window,
					.value = gui_window.ptr(),
				}));

				epoll_event event { .events = EPOLLIN, .data = { .fd = gui_window->server_fd() } };
				ASSERT(epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, gui_window->server_fd(), &event) == 0);

				window = BAN::move(gui_window);
			}

			auto& parent_object = g_objects[request.parent];
			ASSERT(parent_object->type == Object::Type::Window);

			auto& parent_window = parent_object->object.get<Object::Window>();
			TRY(parent_window.children.push_back(request.wid));

			TRY(client_info.objects.insert(request.wid));
			TRY(g_objects.insert(
				request.wid,
				TRY(BAN::UniqPtr<Object>::create(Object {
					.type = Object::Type::Window,
					.object = Object::Window {
						.depth = request.depth,
						.x = request.x,
						.y = request.y,
						.event_mask = event_mask,
						.parent = request.parent,
						.cursor = cursor_id,
						.c_class = request.c_class == CopyFromParent ? parent_window.c_class : request.c_class,
						.window = BAN::move(window),
					},
				}))
			));

			if (gui_window_ptr)
			{
				const WINDOW wid = request.wid;
				gui_window_ptr->set_close_window_event_callback([]{});
				gui_window_ptr->set_resize_window_event_callback([&client_info, wid]() {
					on_window_resize_event(client_info, wid);
				});
				gui_window_ptr->set_window_focus_event_callback([&client_info, wid](auto event) {
					on_window_focus_event(client_info, wid, event.focused);
				});
				gui_window_ptr->set_mouse_move_event_callback([&client_info, wid](auto event) {
					on_mouse_move_event(client_info, wid, event.x, event.y);
				});
				gui_window_ptr->set_mouse_button_event_callback([&client_info, wid](auto event) {
					uint8_t xbutton = 0;
					switch (event.button)
					{
						case LibInput::MouseButton::Left:   xbutton = 1; break;
						case LibInput::MouseButton::Middle: xbutton = 2; break;
						case LibInput::MouseButton::Right:  xbutton = 3; break;
						case LibInput::MouseButton::Extra1: xbutton = 8; break;
						case LibInput::MouseButton::Extra2: xbutton = 9; break;
					}
					if (xbutton)
						on_mouse_button_event(client_info, wid, xbutton, event.pressed);
				});
				gui_window_ptr->set_mouse_scroll_event_callback([&client_info, wid](auto event) {
					on_mouse_button_event(client_info, wid, event.scroll > 0 ? 4 : 5, true);
					on_mouse_button_event(client_info, wid, event.scroll > 0 ? 4 : 5, false);
				});
				gui_window_ptr->set_key_event_callback([&client_info, wid](auto event) {
					on_key_event(client_info, wid, event);
				});
			}

			if (parent_window.event_mask & SubstructureNotifyMask)
			{
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
				event.u.u.sequenceNumber = client_info.sequence;
				TRY(encode(client_info.output_buffer, event));
			}

			break;
		}
		case X_ChangeWindowAttributes:
		{
			auto request = decode<xChangeWindowAttributesReq>(packet).value();

			dprintln("ChangeWindowAttributes");
			dprintln(" window:    {}", request.window);
			dprintln(" valueMask: {8h}", request.valueMask);

			auto& window = TRY_REF(get_window(client_info, request.window, opcode));

			CURSOR cursor_id = None;

			BAN::Optional<uint32_t> background;
			for (size_t i = 0; i < 32; i++)
			{
				if (!((request.valueMask >> i) & 1))
					continue;
				const uint32_t value = decode<uint32_t>(packet).value();

				switch (i)
				{
					case 0:
						if (value == None || value == ParentRelative)
							background = LibGUI::Texture::color_invisible;
						else
							dprintln("    {8h}: {8h}", 1 << i, value);
						break;
					case 1:
						background = value;
						break;
					case 11:
						window.event_mask = value;
						break;
					case 14:
						cursor_id = value;
						break;
					default:
						dprintln("    {8h}: {8h}", 1 << i, value);
						break;
				}
			}

			if (window.cursor != cursor_id)
			{
				// FIXME: show cursor if wid is hovered

				if (window.window.has<BAN::UniqPtr<LibGUI::Window>>())
				{
					auto& gui_window = window.window.get<BAN::UniqPtr<LibGUI::Window>>();

					const auto& cursor = get_cursor_safe(cursor_id);
					gui_window->set_cursor(cursor.width, cursor.height, cursor.pixels.span(), cursor.origin_x, cursor.origin_y);
				}

				window.cursor = cursor_id;
			}

			if (background.has_value())
				window.texture().set_bg_color(background.value());

			break;
		}
		case X_GetWindowAttributes:
		{
			const CARD32 wid = packet.as_span<const uint32_t>()[1];

			dprintln("GetWindowAttributes");
			dprintln("  window: {}", wid);

			const auto& window = TRY_REF(get_window(client_info, wid, opcode));

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
				.allEventMasks = window.event_mask,
				.yourEventMask = window.event_mask,
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

			if (old_parent.event_mask & SubstructureNotifyMask)
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
				event.u.u.sequenceNumber = client_info.sequence;
				TRY(encode(client_info.output_buffer, event));
			}

			if (new_parent.event_mask & SubstructureNotifyMask)
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
				event.u.u.sequenceNumber = client_info.sequence;
				TRY(encode(client_info.output_buffer, event));
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
			auto& texture = window.texture();

			int32_t new_x = window.x;
			int32_t new_y = window.y;
			uint32_t new_width = texture.width();
			uint32_t new_height = texture.height();

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
					case 11:
						window.event_mask = value;
						break;
					default:
						dprintln("    {4h}: {4h}", 1 << i, value);
						break;
				}
			}

			bool window_changed = false;

			const int32_t min_x = BAN::Math::min(window.x, new_x);
			const int32_t min_y = BAN::Math::min(window.y, new_y);
			const int32_t max_x = BAN::Math::max(window.x + texture.width(), new_x + new_width);
			const int32_t max_y = BAN::Math::max(window.y + texture.height(), new_y + new_height);

			if (new_x != window.x || new_y != window.y)
			{
				window.x = new_x;
				window.y = new_y;
				window_changed = true;
			}

			if (new_width != texture.width() || new_height != texture.height())
			{
				if (window.window.has<BAN::UniqPtr<LibGUI::Window>>())
				{
					window.window.get<BAN::UniqPtr<LibGUI::Window>>()->request_resize(new_width, new_height);
					window_changed = false;
				}
				else
				{
					TRY(window.window.get<LibGUI::Texture>().resize(new_width, new_height));
					window_changed = true;
				}
			}

			if (!window_changed)
				break;
		
			invalidate_window(request.window, min_x, min_y, max_x - min_x, max_y + min_y);

			if (window.event_mask & StructureNotifyMask)
			{
				xEvent event = { .u = {
					.configureNotify = {
						.event = request.window,
						.window = request.window,
						.aboveSibling = xFalse,
						.x = static_cast<INT16>(window.x),
						.y = static_cast<INT16>(window.y),
						.width = static_cast<CARD16>(texture.width()),
						.height = static_cast<CARD16>(texture.height()),
						.borderWidth = 0,
						.override = xFalse,
					}
				}};
				event.u.u.type = ConfigureNotify;
				event.u.u.sequenceNumber = client_info.sequence;
				TRY(encode(client_info.output_buffer, event));
			}

			auto& parent_object = *g_objects[window.parent];
			ASSERT(parent_object.type == Object::Type::Window);

			auto& parent_window = parent_object.object.get<Object::Window>();
			if (parent_window.event_mask & SubstructureNotifyMask)
			{
				xEvent event = { .u = {
					.configureNotify = {
						.event = window.parent,
						.window = request.window,
						.aboveSibling = xFalse,
						.x = static_cast<INT16>(window.x),
						.y = static_cast<INT16>(window.y),
						.width = static_cast<CARD16>(texture.width()),
						.height = static_cast<CARD16>(texture.height()),
						.borderWidth = 0,
						.override = xFalse,
					}
				}};
				event.u.u.type = ConfigureNotify;
				event.u.u.sequenceNumber = client_info.sequence;
				TRY(encode(client_info.output_buffer, event));
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

			if (drawable_id == g_root.windowId)
			{
				width = g_root.pixWidth;
				height = g_root.pixHeight;
				depth = g_root.rootDepth;
				x = 0;
				y = 0;
			}
			else if (drawable.type == Object::Type::Window)
			{
				const auto& window = drawable.object.get<Object::Window>();
				width = window.texture().width();
				height = window.texture().height();
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
			dprintln("  type:     {}", request.type);
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

			if (window.event_mask & PropertyChangeMask)
			{
				xEvent event = { .u = {
					.property = {
						.window = request.window,
						.atom = request.property,
						.time = static_cast<CARD32>(time(nullptr)),
						.state = PropertyNewValue,
					}
				}};
				event.u.u.type = PropertyNotify;
				event.u.u.sequenceNumber = client_info.sequence;
				TRY(encode(client_info.output_buffer, event));
			}

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

			if (window.event_mask & PropertyChangeMask)
			{
				xEvent event = { .u = {
					.property = {
						.window = request.window,
						.atom = request.property,
						.time = static_cast<CARD32>(time(nullptr)),
						.state = PropertyDelete,
					}
				}};
				event.u.u.type = PropertyNotify;
				event.u.u.sequenceNumber = client_info.sequence;
				TRY(encode(client_info.output_buffer, event));
			}

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

					if (window.event_mask & PropertyChangeMask)
					{
						xEvent event = { .u = {
							.property = {
								.window = request.window,
								.atom = request.property,
								.time = static_cast<CARD32>(time(nullptr)),
								.state = PropertyDelete,
							}
						}};
						event.u.u.type = PropertyNotify;
						event.u.u.sequenceNumber = client_info.sequence;
						TRY(encode(client_info.output_buffer, event));
					}
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

			auto event = request.event;
			event.u.u.sequenceNumber = client_info.sequence;
			TRY(encode(client_info.output_buffer, event));

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

			xGrabPointerReply reply {
				.type = X_Reply,
				.status = Success,
				.sequenceNumber = client_info.sequence,
				.length = 0,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_UngrabPointer:
		{
			auto time = packet.as_span<const uint32_t>()[1];

			dprintln("UngrabPointer");
			dprintln("  time: {}", time);

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

			int32_t root_x, root_y;
			int32_t event_x, event_y;
			root_x = event_x = window.cursor_x;
			root_y = event_y = window.cursor_y;

			const auto child_wid = find_child_window(wid, event_x, event_y);

			xQueryPointerReply reply {
				.type = X_Reply,
				.sameScreen = xTrue,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.root = g_root.windowId,
				.child = static_cast<CARD32>(child_wid == wid ? None : child_wid),
				.rootX = static_cast<INT16>(root_x),
				.rootY = static_cast<INT16>(root_y),
				.winX = static_cast<INT16>(event_x),
				.winY = static_cast<INT16>(event_y),
				.mask = static_cast<KeyButMask>(s_keymask | s_butmask),
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

			xTranslateCoordsReply reply {
				.type = X_Reply,
				.sameScreen = xTrue,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.child = None,
				.dstX = request.srcX,
				.dstY = request.srcY,
			};
			TRY(encode(client_info.output_buffer, reply));

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
			memcpy(reply.map, s_pressed_keys, 32);
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
				.focus = s_focus_window,
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
						.data = data.span(),
						.owned_data = BAN::move(data),
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
			auto& texture = window.texture();

			if (request.width == 0)
				request.width = texture.width() - request.x;
			if (request.height == 0)
				request.height = texture.height() - request.y;

			texture.clear_rect(request.x, request.y, request.width, request.height);
			invalidate_window(request.window, request.x, request.y, request.width, request.height);

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
		case X_PolyFillRectangle:
			TRY(poly_fill_rectangle(client_info, packet));
			break;
		case X_PolyFillArc:
			TRY(poly_fill_arc(client_info, packet));
			break;
		case X_PutImage:
		{
			auto request = decode<xPutImageReq>(packet).value();

#if 0
			dprintln("PutImage");
			dprintln("  drawable: {}", request.drawable);
			dprintln("  gc:       {}", request.gc);
			dprintln("  format:   {}", request.format);
			dprintln("  depth:    {}", request.depth);
			dprintln("  dstX:     {}", request.dstX);
			dprintln("  dstY:     {}", request.dstY);
			dprintln("  width:    {}", request.width);
			dprintln("  height:   {}", request.height);
#endif


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

			Object::Cursor cursor {
				.width = source.width,
				.height = source.height,
				.origin_x = request.x,
				.origin_y = request.y,
			};
			TRY(cursor.pixels.resize(cursor.width * cursor.height));

			auto* source_data_u32 = reinterpret_cast<const uint32_t*>(source.data.data());
			for (size_t i = 0; i < cursor.width * cursor.height; i++)
				cursor.pixels[i] = 0xFF000000 | (source_data_u32[i] ? foreground : background);

			if (request.mask != None)
			{
				const auto& mask = TRY_REF(get_pixmap(client_info, request.mask, opcode));
				ASSERT(mask.depth == 1);
				ASSERT(mask.width == source.width);
				ASSERT(mask.height == source.height);

				auto* mask_data_u32 = reinterpret_cast<const uint32_t*>(mask.data.data());
				for (size_t i = 0; i < cursor.width * cursor.height; i++)
					if (!mask_data_u32[i])
						cursor.pixels[i] = 0;
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

			dprintln("FreeCursor");
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

			ASSERT(g_keymap_min_keycode <= request.firstKeyCode);
			ASSERT(g_keymap_max_keycode >= request.firstKeyCode + request.count - 1);

			xGetKeyboardMappingReply reply {
				.type = X_Reply,
				.keySymsPerKeyCode = g_keymap_layers,
				.sequenceNumber = client_info.sequence,
				.length = static_cast<CARD32>(request.count * g_keymap_layers),
			};
			TRY(encode(client_info.output_buffer, reply));

			for (size_t i = 0; i < request.count; i++)
				for (size_t j = 0; j < g_keymap_layers; j++)
					TRY(encode<CARD32>(client_info.output_buffer, g_keymap[request.firstKeyCode + i][j]));

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
				.numKeyPerModifier = 2,
				.sequenceNumber = client_info.sequence,
				.length = 4,
			};
			TRY(encode(client_info.output_buffer, reply));

			using LibInput::keycode_normal;

			// shift
			TRY(encode(client_info.output_buffer, keycode_normal(3, 0)));  // lshift
			TRY(encode(client_info.output_buffer, keycode_normal(3, 12))); // rshift

			// lock
			TRY(encode(client_info.output_buffer, keycode_normal(2, 0))); // caps lock
			TRY(encode(client_info.output_buffer, static_cast<uint8_t>(0)));

			// control
			TRY(encode(client_info.output_buffer, keycode_normal(4, 0))); // lcrtl
			TRY(encode(client_info.output_buffer, keycode_normal(4, 6))); // rctrl

			// mod1
			TRY(encode(client_info.output_buffer, keycode_normal(4, 2))); // lalt
			TRY(encode(client_info.output_buffer, keycode_normal(4, 5))); // ralt

			// mod2 -> mod5
			for (size_t i = 2; i <= 5; i++)
			{
				TRY(encode(client_info.output_buffer, static_cast<uint8_t>(0)));
				TRY(encode(client_info.output_buffer, static_cast<uint8_t>(0)));
			}

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

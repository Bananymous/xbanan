#include <BAN/ByteSpan.h>
#include <BAN/Endianness.h>
#include <BAN/Optional.h>
#include <BAN/StringView.h>
#include <BAN/String.h>
#include <BAN/Vector.h>
#include <BAN/HashMap.h>

#include <LibGUI/Window.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include <X11/keysym.h>
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xproto.h>

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
		Window,
		Pixmap,
		GraphicsContext,
	};

	Type type;

	struct Window
	{
		bool mapped { false };
		int32_t x { 0 };
		int32_t y { 0 };
		int32_t cursor_x { 0 };
		int32_t cursor_y { 0 };
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

struct Keymap
{
	consteval Keymap()
	{
		for (auto& sym : map)
			sym = XK_VoidSymbol;

		using LibInput::Key;
		map[(size_t)Key::A] = XK_A;
		map[(size_t)Key::B] = XK_B;
		map[(size_t)Key::C] = XK_C;
		map[(size_t)Key::D] = XK_D;
		map[(size_t)Key::E] = XK_E;
		map[(size_t)Key::F] = XK_F;
		map[(size_t)Key::G] = XK_G;
		map[(size_t)Key::H] = XK_H;
		map[(size_t)Key::I] = XK_I;
		map[(size_t)Key::J] = XK_J;
		map[(size_t)Key::K] = XK_K;
		map[(size_t)Key::L] = XK_L;
		map[(size_t)Key::M] = XK_M;
		map[(size_t)Key::N] = XK_N;
		map[(size_t)Key::O] = XK_O;
		map[(size_t)Key::P] = XK_P;
		map[(size_t)Key::Q] = XK_Q;
		map[(size_t)Key::R] = XK_R;
		map[(size_t)Key::S] = XK_S;
		map[(size_t)Key::T] = XK_T;
		map[(size_t)Key::U] = XK_U;
		map[(size_t)Key::V] = XK_V;
		map[(size_t)Key::W] = XK_W;
		map[(size_t)Key::X] = XK_X;
		map[(size_t)Key::Y] = XK_Y;
		map[(size_t)Key::Z] = XK_Z;
		map[(size_t)Key::A_Ring] = XK_Aring;
		map[(size_t)Key::A_Umlaut] = XK_Adiaeresis;
		map[(size_t)Key::O_Umlaut] = XK_Odiaeresis;
		map[(size_t)Key::_0] = XK_0;
		map[(size_t)Key::_1] = XK_1;
		map[(size_t)Key::_2] = XK_2;
		map[(size_t)Key::_3] = XK_3;
		map[(size_t)Key::_4] = XK_4;
		map[(size_t)Key::_5] = XK_5;
		map[(size_t)Key::_6] = XK_6;
		map[(size_t)Key::_7] = XK_7;
		map[(size_t)Key::_8] = XK_8;
		map[(size_t)Key::_9] = XK_9;
		map[(size_t)Key::F1] = XK_F1;
		map[(size_t)Key::F2] = XK_F2;
		map[(size_t)Key::F3] = XK_F3;
		map[(size_t)Key::F4] = XK_F4;
		map[(size_t)Key::F5] = XK_F5;
		map[(size_t)Key::F6] = XK_F6;
		map[(size_t)Key::F7] = XK_F7;
		map[(size_t)Key::F8] = XK_F8;
		map[(size_t)Key::F9] = XK_F9;
		map[(size_t)Key::F10] = XK_F10;
		map[(size_t)Key::F11] = XK_F11;
		map[(size_t)Key::F12] = XK_F12;
		map[(size_t)Key::Insert] = XK_Insert;
		map[(size_t)Key::PrintScreen] = XK_Print;
		map[(size_t)Key::Delete] = XK_Delete;
		map[(size_t)Key::Home] = XK_Home;
		map[(size_t)Key::End] = XK_End;
		map[(size_t)Key::PageUp] = XK_Page_Up;
		map[(size_t)Key::PageDown] = XK_Page_Down;
		map[(size_t)Key::Enter] = XK_Return;
		map[(size_t)Key::Space] = XK_space;
		map[(size_t)Key::ExclamationMark] = XK_exclam;
		map[(size_t)Key::DoubleQuote] = XK_quotedbl;
		map[(size_t)Key::Hashtag] = XK_numbersign;
		map[(size_t)Key::Currency] = XK_currency;
		map[(size_t)Key::Percent] = XK_percent;
		map[(size_t)Key::Ampersand] = XK_ampersand;
		map[(size_t)Key::Slash] = XK_slash;
		map[(size_t)Key::Section] = XK_section;
		map[(size_t)Key::Half] = XK_onehalf;
		map[(size_t)Key::OpenParenthesis] = ')';
		map[(size_t)Key::CloseParenthesis] = '(';
		map[(size_t)Key::OpenSquareBracket] = '[';
		map[(size_t)Key::CloseSquareBracket] = ']';
		map[(size_t)Key::OpenCurlyBracket] = '{';
		map[(size_t)Key::CloseCurlyBracket] = '}';
		map[(size_t)Key::Equals] = '=';
		map[(size_t)Key::QuestionMark] = '?';
		map[(size_t)Key::Plus] = '+';
		map[(size_t)Key::BackSlash] = '\\';
		map[(size_t)Key::Acute] = XK_acute;
		map[(size_t)Key::BackTick] = '`';
		map[(size_t)Key::TwoDots] = XK_diaeresis;
		map[(size_t)Key::Cedilla] = XK_Ccedilla;
		map[(size_t)Key::Backspace] = XK_BackSpace;
		map[(size_t)Key::AtSign] = XK_at;
		map[(size_t)Key::Pound] = XK_sterling;
		map[(size_t)Key::Dollar] = XK_dollar;
		map[(size_t)Key::Euro] = XK_EuroSign;
		map[(size_t)Key::Escape] = XK_Escape;
		map[(size_t)Key::Tab] = XK_Tab;
		map[(size_t)Key::CapsLock] = XK_Caps_Lock;
		map[(size_t)Key::LeftShift] = XK_Shift_L;
		map[(size_t)Key::LeftCtrl] = XK_Control_L;
		map[(size_t)Key::Super] = XK_Super_L;
		map[(size_t)Key::LeftAlt] = XK_Alt_L;
		map[(size_t)Key::RightAlt] = XK_Alt_R;
		map[(size_t)Key::RightCtrl] = XK_Control_R;
		map[(size_t)Key::RightShift] = XK_Shift_R;
		map[(size_t)Key::SingleQuote] = '\'';
		map[(size_t)Key::Asterix] = '*';
		map[(size_t)Key::Caret] = '^';
		map[(size_t)Key::Tilde] = '~';
		map[(size_t)Key::ArrowUp] = XK_Up;
		map[(size_t)Key::ArrowDown] = XK_Down;
		map[(size_t)Key::ArrowLeft] = XK_Left;
		map[(size_t)Key::ArrowRight] = XK_Right;
		map[(size_t)Key::Comma] = ',';
		map[(size_t)Key::Semicolon] = ';';
		map[(size_t)Key::Period] = '.';
		map[(size_t)Key::Colon] = ':';
		map[(size_t)Key::Hyphen] = '-';
		map[(size_t)Key::Underscore] = '_';
		map[(size_t)Key::NumLock] = XK_Num_Lock;
		map[(size_t)Key::ScrollLock] = XK_Scroll_Lock;
		map[(size_t)Key::LessThan] = '<';
		map[(size_t)Key::GreaterThan] = '>';
		map[(size_t)Key::Pipe] = '|';
		map[(size_t)Key::Negation] = XK_notsign;
		map[(size_t)Key::BrokenBar] = XK_brokenbar;
		map[(size_t)Key::Numpad0] = XK_KP_0;
		map[(size_t)Key::Numpad1] = XK_KP_1;
		map[(size_t)Key::Numpad2] = XK_KP_2;
		map[(size_t)Key::Numpad3] = XK_KP_3;
		map[(size_t)Key::Numpad4] = XK_KP_4;
		map[(size_t)Key::Numpad5] = XK_KP_5;
		map[(size_t)Key::Numpad6] = XK_KP_6;
		map[(size_t)Key::Numpad7] = XK_KP_7;
		map[(size_t)Key::Numpad8] = XK_KP_8;
		map[(size_t)Key::Numpad9] = XK_KP_9;
		map[(size_t)Key::NumpadPlus] = XK_KP_Add;
		map[(size_t)Key::NumpadMinus] = XK_KP_Subtract;
		map[(size_t)Key::NumpadMultiply] = XK_KP_Multiply;
		map[(size_t)Key::NumpadDivide] = XK_KP_Divide;
		map[(size_t)Key::NumpadEnter] = XK_KP_Enter;
		map[(size_t)Key::NumpadDecimal] = XK_KP_Decimal;
#if 0
		map[(size_t)Key::VolumeMute] = XF8_mute;
		map[(size_t)Key::VolumeUp] = XK_VolumeUp;
		map[(size_t)Key::VolumeDown] = XK_VolumeDown;
		map[(size_t)Key::Calculator] = XK_Calculator;
		map[(size_t)Key::MediaPlayPause] = XK_MediaPlayPause;
		map[(size_t)Key::MediaStop] = XK_MediaStop;
		map[(size_t)Key::MediaPrevious] = XK_MediaPrevious;
		map[(size_t)Key::MediaNext] = XK_MediaNext;
#endif
	}

	static constexpr size_t size = static_cast<size_t>(LibInput::Key::Count);
	KeySym map[size];
} keymap;

xPixmapFormat format {
	.depth = 24,
	.bitsPerPixel = 32,
	.scanLinePad = 0,
};

xWindowRoot root {
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
	.rootDepth = 32,
	.nDepths = 1,
};

xDepth depth {
	.depth = 32,
	.nVisuals = 1,
};

xVisualType visual {
	.visualID = 0,
	.c_class = TrueColor,
	.bitsPerRGB = 8,
	.colormapEntries = 0,
	.redMask = 0xFF0000,
	.greenMask = 0x00FF00,
	.blueMask = 0x0000FF,
};

struct Client
{
	enum class State
	{
		ConnectionSetup,
		Connected,
	};
	int fd;
	State state;
	CARD16 sequence { 0 };
	BAN::Vector<uint8_t> input_buffer;
	BAN::Vector<uint8_t> output_buffer;

	BAN::HashMap<CARD32, BAN::UniqPtr<Object>> objects;
};

BAN::HashMap<BAN::String, ATOM> g_atoms_name_to_id;
BAN::HashMap<ATOM, BAN::String> g_atoms_id_to_name;
ATOM atom_value = XA_LAST_PREDEFINED + 1;

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

int epoll_fd;
BAN::HashMap<int, EpollThingy> epoll_thingies;

const char* opcode_to_name[] {
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

template<typename T>
BAN::Optional<T> decode(BAN::ConstByteSpan& buffer)
{
	if (buffer.size() < sizeof(T))
		return {};
	T result = buffer.as<const T>();
	buffer = buffer.slice(sizeof(T));
	return result;
}

static BAN::ErrorOr<void> encode_bytes(BAN::Vector<uint8_t>& buffer, const void* data, size_t size)
{
	const size_t old_size = buffer.size();
	TRY(buffer.resize(old_size + size));
	memcpy(buffer.data() + old_size, data, size);
	return {};
}

template<typename T> requires BAN::is_pod_v<T>
static BAN::ErrorOr<void> encode(BAN::Vector<uint8_t>& buffer, const T& value)
{
	return encode_bytes(buffer, &value, sizeof(T));
}

template<typename T> requires requires(T value) { value.size(); value.data(); }
static BAN::ErrorOr<void> encode(BAN::Vector<uint8_t>& buffer, const T& value)
{
	return encode_bytes(buffer, value.data(), value.size());
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

	xConnSetupPrefix setup_prefix {
		.success = 1,
		.lengthReason = 0, // wtf is this
		.majorVersion = client_prefix.majorVersion,
		.minorVersion = client_prefix.minorVersion,
		.length = 8 + 2*1 + (8 + 0 + sz_xWindowRoot + sz_xDepth + sz_xVisualType) / 4,
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
		.numFormats = 1,
		.imageByteOrder = 0, // LSB
		.bitmapBitOrder = 0, // LSB
		.bitmapScanlineUnit = 0,
		.bitmapScanlinePad = 0,
		.minKeyCode = 1,
		.maxKeyCode = keymap.size,
	};

	TRY(encode(client_info.output_buffer, setup));
	TRY(encode(client_info.output_buffer, "banan!!!"_sv));
	TRY(encode(client_info.output_buffer, format));
	TRY(encode(client_info.output_buffer, root));
	TRY(encode(client_info.output_buffer, depth));
	TRY(encode(client_info.output_buffer, visual));

	client_info.state = Client::State::Connected;

	TRY(client_info.objects.insert(root.windowId,
		TRY(BAN::UniqPtr<Object>::create(Object {
			.type = Object::Type::Window,
			.object = Object::Window {
				.event_mask = 0,
				.c_class = InputOutput,
				.window = {},
			}
		}))
	));

	return {};
}

bool is_visible(Client& client_info, WINDOW wid)
{
	if (wid == root.windowId)
		return true;

	auto& object = *client_info.objects[wid];
	ASSERT(object.type == Object::Type::Window);

	auto& window = object.object.get<Object::Window>();
	if (!window.mapped)
		return false;

	return is_visible(client_info, window.parent);
}

BAN::ErrorOr<void> send_visibility_events_recursively(Client& client_info, WINDOW wid, bool visible)
{
	auto& object = *client_info.objects[wid];
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
		auto& child_object = *client_info.objects[child_wid];
		ASSERT(child_object.type == Object::Type::Window);

		auto& child_window = object.object.get<Object::Window>();
		if (!child_window.mapped)
			continue;

		TRY(send_visibility_events_recursively(client_info, child_wid, visible));
	}

	return {};
}

void _invalidate_window_recursive(Client& client_info, WINDOW wid, int32_t x, int32_t y, int32_t w, int32_t h)
{
	ASSERT(wid != root.windowId);

	if (w <= 0 || h <= 0)
		return;

	auto& object = *client_info.objects[wid];
	ASSERT(object.type == Object::Type::Window);

	auto& window = object.object.get<Object::Window>();
	if (!window.mapped)
		return;

	for (auto child_wid : window.children)
	{
		const auto& child_object = *client_info.objects[child_wid];
		ASSERT(child_object.type == Object::Type::Window);

		const auto& child_window = child_object.object.get<Object::Window>();
		if (!child_window.mapped)
			continue;

		_invalidate_window_recursive(
			client_info,
			child_wid,
			x - child_window.x,
			y - child_window.y,
			w - child_window.x,
			h - child_window.y
		);
	}

	if (window.window.has<BAN::UniqPtr<LibGUI::Window>>())
		return;

	auto& parent_object = *client_info.objects[window.parent];
	ASSERT(parent_object.type == Object::Type::Window);

	auto& parent_window = parent_object.object.get<Object::Window>();
	parent_window.texture().copy_texture(
		window.texture(),
		parent_window.x + x,
		parent_window.y + y,
		x,
		y,
		w,
		h
	);
}

void invalidate_window(Client& client_info, WINDOW wid, int32_t x, int32_t y, int32_t w, int32_t h)
{
	ASSERT(wid != root.windowId);

	for (;;)
	{
		auto& object = *client_info.objects[wid];
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

	_invalidate_window_recursive(client_info, wid, x, y, w, h);

	auto& object = *client_info.objects[wid];
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

	window.window.get<BAN::UniqPtr<LibGUI::Window>>()->invalidate(x, y, w, h);
}

BAN::ErrorOr<void> map_window(Client& client_info, WINDOW wid)
{
	auto& object = *client_info.objects[wid];
	ASSERT(object.type == Object::Type::Window);

	auto& window = object.object.get<Object::Window>();
	if (window.mapped)
		return {};

	auto& texture = window.texture();
	texture.clear();

	window.mapped = true;

	invalidate_window(client_info, wid, 0, 0, texture.width(), texture.height());

	if (window.window.has<BAN::UniqPtr<LibGUI::Window>>())
	{
		auto& gui_window = window.window.get<BAN::UniqPtr<LibGUI::Window>>();
		auto attributes = gui_window->get_attributes();
		attributes.shown = true;
		gui_window->set_attributes(attributes);
	}

	if (is_visible(client_info, window.parent))
		TRY(send_visibility_events_recursively(client_info, wid, true));

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

	auto& parent_object = *client_info.objects[window.parent];
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

BAN::ErrorOr<void> unmap_window(Client& client_info, WINDOW wid)
{
	auto& object = *client_info.objects[wid];
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

	if (is_visible(client_info, window.parent))
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

	auto& parent_object = *client_info.objects[window.parent];
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

BAN::ErrorOr<void> destroy_window(Client& client_info, WINDOW wid)
{
	TRY(unmap_window(client_info, wid));

	auto& object = *client_info.objects[wid];
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

	auto& parent_object = *client_info.objects[window.parent];
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
		epoll_ctl(epoll_fd, EPOLL_CTL_DEL, server_fd, nullptr);
		epoll_thingies.remove(server_fd);
	}

	client_info.objects.remove(wid);

	return {};
}

void update_cursor_position(Client& client_info, WINDOW wid, int32_t x, int32_t y)
{
	auto& object = *client_info.objects[wid];
	ASSERT(object.type == Object::Type::Window);

	auto& window = object.object.get<Object::Window>();
	window.cursor_x = x;
	window.cursor_y = y;

	for (auto child_wid : window.children)
	{
		auto& child_object = *client_info.objects[child_wid];
		ASSERT(child_object.type == Object::Type::Window);

		auto& child_window = object.object.get<Object::Window>();
		update_cursor_position(client_info, child_wid, child_window.x + x, child_window.y + y);
	}
}

BAN::ErrorOr<void> handle_packet(Client& client_info, BAN::ConstByteSpan packet)
{
	struct PixmapInfo
	{
		BAN::ByteSpan data;
		CARD32 w, h;
		CARD8 depth;
	};

	const auto get_pixmap_info =
		[](Object& object) -> PixmapInfo
		{
			PixmapInfo info;

			switch (object.type)
			{
				case Object::Type::Window:
				{
					auto& texture = object.object.get<Object::Window>().texture();

					info.data = { reinterpret_cast<uint8_t*>(texture.pixels().data()), texture.pixels().size() * 4 };
					info.w = texture.width();
					info.h = texture.height();
					info.depth = 32;

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

	client_info.sequence++;

	switch (packet[0])
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
						derrorln("    events: {8h}", event_mask);
						break;
					default:
						dprintln("    {8h}: {8h}", 1 << i, value);
						break;
				}
			}

			decltype(Object::Window::window) window;

			LibGUI::Window* gui_window_ptr = nullptr;

			if (request.parent != root.windowId)
				window = TRY(LibGUI::Texture::create(request.width, request.height, background));
			else
			{
				auto attributes = LibGUI::Window::default_attributes;
				attributes.shown = false;
				attributes.title_bar = false;
				attributes.alpha_channel = true;

				auto gui_window = TRY(LibGUI::Window::create(request.width, request.height, "window?"_sv, attributes));
				gui_window->texture().set_bg_color(background);
				gui_window_ptr = gui_window.ptr();

				TRY(epoll_thingies.insert(gui_window->server_fd(), {
					.type = EpollThingy::Type::Window,
					.value = gui_window.ptr(),
				}));

				epoll_event event { .events = EPOLLIN, .data = { .fd = gui_window->server_fd() } };
				ASSERT(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, gui_window->server_fd(), &event) == 0);

				window = BAN::move(gui_window);
			}

			auto& parent_object = client_info.objects[request.parent];
			ASSERT(parent_object->type == Object::Type::Window);

			auto& parent_window = parent_object->object.get<Object::Window>();
			TRY(parent_window.children.push_back(request.wid));

			auto it = TRY(client_info.objects.insert(
				request.wid,
				TRY(BAN::UniqPtr<Object>::create(Object {
					.type = Object::Type::Window,
					.object = Object::Window {
						.x = request.x,
						.y = request.y,
						.event_mask = event_mask,
						.parent = request.parent,
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
					auto& object = *client_info.objects[wid];
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

					auto& parent_object = *client_info.objects[window.parent];
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

					invalidate_window(client_info, wid, 0, 0, window.texture().width(), window.texture().height());
				});
				gui_window_ptr->set_mouse_move_event_callback([&client_info, wid](auto event) {
					update_cursor_position(client_info, wid, event.x, event.y);
				});
				gui_window_ptr->set_key_event_callback([&client_info, wid](auto event) {
					if (keymap.map[static_cast<BYTE>(event.key)] == XK_VoidSymbol)
						return;

					auto& object = *client_info.objects[wid];
					ASSERT(object.type == Object::Type::Window);

					auto& window = object.object.get<Object::Window>();
					if (!(window.event_mask & (event.pressed() ? KeyPressMask : KeyReleaseMask)))
						return;

					xEvent xevent { .u = {
						.keyButtonPointer = {
							.time = static_cast<CARD32>(time(nullptr)),
							.root = root.windowId,
							.event = wid,
							.child = None,
							.rootX = 0,
							.rootY = 0,
							.eventX = 0,
							.eventY = 0,
							.state = 0,
							.sameScreen = xTrue,
						}
					}};
					xevent.u.u.type = event.pressed() ? KeyPress : KeyRelease,
					xevent.u.u.detail = static_cast<BYTE>(event.key);
					xevent.u.u.sequenceNumber = client_info.sequence;
					MUST(encode(client_info.output_buffer, xevent));
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

			auto& object = *client_info.objects[request.window];
			ASSERT(object.type == Object::Type::Window);

			auto& window = object.object.get<Object::Window>();

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
						derrorln("    events: {8h}", window.event_mask);
						break;
					default:
						dprintln("    {8h}: {8h}", 1 << i, value);
						break;
				}
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

			const auto& object = *client_info.objects[wid];
			ASSERT(object.type == Object::Type::Window);

			const auto& window = object.object.get<Object::Window>();

			xGetWindowAttributesReply reply {
				.type = X_Reply,
				.backingStore = 0,
				.sequenceNumber = client_info.sequence,
				.length = 3,
				.visualID = visual.visualID,
				.c_class = window.c_class,
				.bitGravity = 0,
				.winGravity = 0,
				.backingBitPlanes = 0,
				.backingPixel = 0,
				.saveUnder = 0,
				.mapInstalled = 0,
				.mapState = static_cast<CARD8>(is_visible(client_info, wid) ? 2 : window.mapped),
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

			TRY(destroy_window(client_info, wid));

			break;
		}
		case X_MapWindow:
		{
			const CARD32 wid = packet.as_span<const uint32_t>()[1];

			dprintln("MapWindow");
			dprintln("  window: {}", wid);

			TRY(map_window(client_info, wid));

			break;
		}
		case X_MapSubwindows:
		{
			const CARD32 wid = packet.as_span<const uint32_t>()[1];

			dprintln("MapSubwindows");
			dprintln("  window: {}", wid);

			auto& object = *client_info.objects[wid];
			ASSERT(object.type == Object::Type::Window);

			auto& window = object.object.get<Object::Window>();

			for (auto child_wid : window.children)
				TRY(map_window(client_info, child_wid));

			break;
		}
		case X_UnmapWindow:
		{
			const CARD32 wid = packet.as_span<const uint32_t>()[1];

			dprintln("UnmapWindow");
			dprintln("  window: {}", wid);

			TRY(unmap_window(client_info, wid));

			break;
		}
		case X_UnmapSubwindows:
		{
			const CARD32 wid = packet.as_span<const uint32_t>()[1];

			dprintln("UnmapSubwindows");
			dprintln("  window: {}", wid);

			auto& object = *client_info.objects[wid];
			ASSERT(object.type == Object::Type::Window);

			auto& window = object.object.get<Object::Window>();

			for (auto child_wid : window.children)
				TRY(unmap_window(client_info, child_wid));

			break;
		}
		case X_ConfigureWindow:
		{
			auto request = decode<xConfigureWindowReq>(packet).value();

			dprintln("ConfigureWindow");
			dprintln("  window: {}", request.window);

			auto& object = *client_info.objects[request.window];
			ASSERT(object.type == Object::Type::Window);

			auto& window = object.object.get<Object::Window>();
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
				uint16_t value = decode<uint16_t>(packet).value();
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
						derrorln("    events: {8h}", window.event_mask);
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
		
			invalidate_window(client_info, request.window, min_x, min_y, max_x - min_x, max_y + min_y);

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

			auto& parent_object = *client_info.objects[window.parent];
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
			const CARD32 drawable = packet.as_span<const uint32_t>()[1];

			dprintln("GetGeometry");
			dprintln("  drawable: {}", drawable);

			const auto& object = *client_info.objects[drawable];
			ASSERT(object.type == Object::Type::Window);

			CARD16 width, height;

			if (drawable == root.windowId)
			{
				width = root.pixWidth;
				height = root.pixHeight;
			}
			else
			{
				const auto& texture = object.object.get<Object::Window>().texture();
				width = texture.width();
				height = texture.height();
			}

			xGetGeometryReply reply {
				.type = X_Reply,
				.depth = 32,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.root = root.windowId,
				.x = 0,
				.y = 0,
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

			const auto& object = *client_info.objects[wid];
			ASSERT(object.type == Object::Type::Window);

			const auto& window = object.object.get<Object::Window>();

			xQueryTreeReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = static_cast<CARD32>(window.children.size()),
				.root = root.windowId,
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
				it = MUST(g_atoms_name_to_id.emplace(name, atom_value++));
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

			auto& object = *client_info.objects[request.window];
			ASSERT(object.type == Object::Type::Window);

			auto& window = object.object.get<Object::Window>();

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
			ASSERT(property.type == request.type);
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
					TRY(property.data.resize(old_bytes + bytes));
					memmove(property.data.data() + old_bytes, property.data.data(), old_bytes);
					memcpy(property.data.data(), packet.data(), bytes);
					break;
				case PropModeAppend:
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

			auto& object = *client_info.objects[request.window];
			ASSERT(object.type == Object::Type::Window);

			auto& window = object.object.get<Object::Window>();

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

			auto& object = *client_info.objects[request.window];
			ASSERT(object.type == Object::Type::Window);

			auto& window = object.object.get<Object::Window>();

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
		case X_GetSelectionOwner:
		{
			const CARD32 atom = packet.as_span<const uint32_t>()[1];

			dprintln("GetSelectionOwner {}", atom);

			xGetSelectionOwnerReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.owner = root.windowId,
			};
			TRY(encode(client_info.output_buffer, reply));

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
		case X_GrabServer:
		{
			dwarnln("GrabServer");
			break;
		}
		case X_UngrabServer:
		{
			dwarnln("UngrabServer");
			break;
		}
		case X_QueryPointer:
		{
			const CARD32 wid = packet.as_span<const uint32_t>()[1];

			dprintln("QueryPointer");
			dprintln("  window: {}", wid);

			const auto& object = *client_info.objects[wid];
			ASSERT(object.type == Object::Type::Window);

			const auto& window = object.object.get<Object::Window>();

			xQueryPointerReply reply {
				.type = X_Reply,
				.sameScreen = xTrue,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.root = root.windowId,
				.child = wid,
				.rootX = static_cast<INT16>(window.cursor_x),
				.rootY = static_cast<INT16>(window.cursor_y),
				.winX = static_cast<INT16>(window.cursor_x),
				.winY = static_cast<INT16>(window.cursor_y),
				.mask = 0,
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
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_GetInputFocus:
		{
			dprintln("GetInputFocus");

			xGetInputFocusReply reply {
				.type = X_Reply,
				.revertTo = 0,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.focus = xTrue,
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

			BAN::Vector<uint8_t> bytes;
			TRY(bytes.resize(request.width * request.height * request.depth / 8));

			TRY(client_info.objects.insert(
				request.pid,
				TRY(BAN::UniqPtr<Object>::create(Object {
					.type = Object::Type::Pixmap,
					.object = Object::Pixmap {
						.depth = request.depth,
						.width = request.width,
						.height = request.height,
						.data = BAN::move(bytes),
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

			auto it = client_info.objects.find(pid);
			ASSERT(it != client_info.objects.end());
			ASSERT(it->value->type == Object::Type::Pixmap);

			client_info.objects.remove(it);

			break;
		}
		case X_CreateGC:
		{
			auto request = decode<xCreateGCReq>(packet).value();

			dprintln("CreateGC");
			dprintln("  drawable {}", request.drawable);
			dprintln("  gc       {}", request.gc);
			dprintln("  mask     {8h}", request.mask);

			uint32_t foreground = 0x000000;
			uint32_t background = 0x000000;
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
					default:
						dprintln("    {8h}: {8h}", 1 << i, value);
						break;
				}
			}

			TRY(client_info.objects.insert(request.gc, TRY(BAN::UniqPtr<Object>::create(Object {
				.type = Object::Type::GraphicsContext,
				.object = Object::GraphicsContext { 
					.foreground = foreground,
					.background = background,
				}
			}))));

			break;
		}
		case X_ChangeGC:
		{
			auto request = decode<xChangeGCReq>(packet).value();

			dprintln("ChangeGC");
			dprintln("  gc       {}", request.gc);
			dprintln("  mask     {8h}", request.mask);

			auto& object = *client_info.objects[request.gc];
			ASSERT(object.type == Object::Type::GraphicsContext);

			auto& gc = object.object.get<Object::GraphicsContext>();

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
					default:
						dprintln("    {8h}: {8h}", 1 << i, value);
						break;
				}
			}

			break;
		}
		case X_FreeGC:
		{
			const CARD32 gc = packet.as_span<const uint32_t>()[1];

			dprintln("FreeGC");
			dprintln("  gc: {}", gc);

			client_info.objects.remove(gc);

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

			auto& object = *client_info.objects[request.window];
			ASSERT(object.type == Object::Type::Window);

			auto& texture = object.object.get<Object::Window>().texture();

			if (request.width == 0)
				request.width = texture.width() - request.x;
			if (request.height == 0)
				request.height = texture.height() - request.y;

			texture.clear_rect(request.x, request.y, request.width, request.height);
			invalidate_window(client_info, request.window, request.x, request.y, request.width, request.height);

			break;
		}
		case X_CopyArea:
		{
			auto request = decode<xCopyAreaReq>(packet).value();

			dprintln("CopyArea");
			dprintln("  srcDrawable: {}", request.srcDrawable);
			dprintln("  dstDrawable: {}", request.dstDrawable);
			dprintln("  srcX:        {}", request.srcX);
			dprintln("  srcY:        {}", request.srcY);
			dprintln("  dstX:        {}", request.dstX);
			dprintln("  dstY:        {}", request.dstY);
			dprintln("  width:       {}", request.width);
			dprintln("  heigth:      {}", request.height);

			auto& src_object = *client_info.objects[request.srcDrawable];
			auto [src_data, src_w, src_h, src_depth] = get_pixmap_info(src_object);

			auto& dst_object = *client_info.objects[request.srcDrawable];
			auto [dst_data, dst_w, dst_h, dst_depth] = get_pixmap_info(dst_object);

			const auto get_pixel =
				[&](int32_t x, int32_t y) -> uint32_t
				{
					const int32_t index = y * src_w + x;
					switch (src_depth)
					{
						case 1:
						{
							const int32_t byte = index / 8;
							const int32_t bit = index % 8;
							return (src_data[byte] & (1 << bit)) ? 0xFFFFFF : 0x000000;
						}
						case 32:
						{
							const auto src_data_u32 = src_data.as_span<uint32_t>();
							return src_data_u32[index];
						}
						default:
							ASSERT_NOT_REACHED();
					}	
				};

			const auto set_pixel =
				[&](int32_t x, int32_t y, uint32_t color) -> void
				{
					const int32_t index = y * dst_w + x;
					switch (dst_depth)
					{
						case 1:
						{
							const int32_t byte = index / 8;
							const int32_t bit = index % 8;
							if (color)
								dst_data[byte] |=  (1 << bit);
							else
								dst_data[byte] &= ~(1 << bit);
							break;
						}
						case 32:
						{
							const auto dst_data_u32 = dst_data.as_span<uint32_t>();
							dst_data_u32[index] = color;
							break;
						}
						default:
							ASSERT_NOT_REACHED();
					}	
				};

			for (int32_t yoff = 0; yoff < request.height; yoff++)
			{
				const int32_t src_y = request.srcY + yoff;
				const int32_t dst_y = request.dstY + yoff;
				if (src_y < 0 || src_y >= src_h)
					continue;
				if (dst_y < 0 || dst_y >= dst_h)
					continue;
				for (int32_t xoff = 0; xoff < request.width; xoff++)
				{
					const int32_t src_x = request.srcX + xoff;
					const int32_t dst_x = request.dstX + xoff;
					if (src_x < 0 || src_x >= src_w)
						continue;
					if (dst_x < 0 || dst_x >= dst_w)
						continue;
					set_pixel(dst_x, dst_y, get_pixel(src_x, src_y));
				}
			}

			if (dst_object.type == Object::Type::Window)
				invalidate_window(client_info, request.dstDrawable, request.dstX, request.dstY, request.width, request.height);

			break;
		}
		case X_PolyFillRectangle:
		{
			auto request = decode<xPolyFillRectangleReq>(packet).value();

			dprintln("PolyFillRectangle");
			dprintln("  drawable: {}", request.drawable);
			dprintln("  gc:       {}", request.gc);

			auto& gc_object = *client_info.objects[request.gc];
			ASSERT(gc_object.type == Object::Type::GraphicsContext);

			const auto foreground = gc_object.object.get<Object::GraphicsContext>().foreground;

			auto& object = *client_info.objects[request.drawable];
			ASSERT(object.type == Object::Type::Window || object.type == Object::Type::Pixmap);

			auto [data, w, h, depth] = get_pixmap_info(object);
			ASSERT(depth == 32);

			auto* data_u32 = data.as_span<uint32_t>().data();

			dprintln("  rects:");
			while (!packet.empty())
			{
				const auto rect = decode<xRectangle>(packet).value();

				dprintln("    rect");
				dprintln("      x, y: {},{}", rect.x, rect.y);
				dprintln("      w, h: {},{}", rect.width, rect.height);

				const int32_t min_x = BAN::Math::max<int32_t>(rect.x, 0);
				const int32_t min_y = BAN::Math::max<int32_t>(rect.y, 0);
				const int32_t max_x = BAN::Math::min<int32_t>(rect.x + rect.width, w);
				const int32_t max_y = BAN::Math::min<int32_t>(rect.y + rect.height, h);

				for (int32_t y = min_y; y < max_y; y++)
					for (int32_t x = min_x; x < max_x; x++)
						data_u32[y * w + x] = foreground;

				if (object.type == Object::Type::Window)
					invalidate_window(client_info, request.drawable, rect.x, rect.y, rect.width, rect.height);
			}

			break;
		}
		case X_PolyFillArc:
		{
			auto request = decode<xPolyFillArcReq>(packet).value();

			dprintln("PolyFillArc");
			dprintln("  drawable: {}", request.drawable);
			dprintln("  gc:       {}", request.gc);

			auto& gc_object = *client_info.objects[request.gc];
			ASSERT(gc_object.type == Object::Type::GraphicsContext);

			const auto foreground = gc_object.object.get<Object::GraphicsContext>().foreground;

			auto& object = *client_info.objects[request.drawable];
			ASSERT(object.type == Object::Type::Window);

			auto& texture = object.object.get<Object::Window>().texture();

			const auto normalize_angle =
				[](float f) -> float
				{
					const float radians = f * (2.0f * BAN::numbers::pi_v<float> / 64.0f / 360.0f);
					const float mod = BAN::Math::fmod(radians, 2.0f * BAN::numbers::pi_v<float>);
					if (mod < 0.0f)
						return mod + 2.0f * BAN::numbers::pi_v<float>;
					return mod;
				};

			dprintln("  arcs:");
			while (!packet.empty())
			{
				const auto arc = decode<xArc>(packet).value();

				dprintln("    arc");
				dprintln("      x,  y:  {},{}", arc.x, arc.y);
				dprintln("      w,  h:  {},{}", arc.width, arc.height);
				dprintln("      a1, a2: {},{}", arc.angle1, arc.angle2);

				const int32_t min_x = BAN::Math::max<int32_t>(0, arc.x);
				const int32_t min_y = BAN::Math::max<int32_t>(0, arc.y);
				
				const int32_t max_x = BAN::Math::max<int32_t>(texture.width(),  arc.x + arc.width);
				const int32_t max_y = BAN::Math::max<int32_t>(texture.height(), arc.y + arc.height);

				const auto rx = arc.width / 2;
				const auto ry = arc.height / 2;

				const auto cx = arc.x + rx;
				const auto cy = arc.y + ry;

				auto angle1 = normalize_angle(arc.angle1);
				auto angle2 = normalize_angle(arc.angle1 + arc.angle2);
				if (arc.angle2 < 0)
					BAN::swap(angle1, angle2);

				const bool full_circle = BAN::Math::abs(arc.angle2) >= 360 * 64;

				for (int32_t y = min_y; y < max_y; y++)
				{
					for (int32_t x = min_x; x < max_x; x++)
					{
						const int32_t dx = x - cx;
						const int32_t dy = cy - y;

						if ((float)(dx*dx) / (rx*rx) + (float)(dy*dy) / (ry*ry) > 1.0f)
							continue;

						if (!full_circle)
						{
							const auto theta = BAN::Math::atan2<float>(dy, dx);
							if (angle1 <= angle2)
							{
								if (!(theta >= angle1 && theta <= angle2))
									continue;
							}
							else
							{
								if (!(theta >= angle1 || theta <= angle2))
									continue;
							}
						}

						texture.set_pixel(x, y, foreground);
					}
				}

				invalidate_window(client_info, request.drawable, min_x, min_y, max_x - min_x, max_y - min_y);
			}

			break;
		}
		case X_PutImage:
		{
			auto request = decode<xPutImageReq>(packet).value();

#if 0
			dprintln("PutImage");
			dprintln("  drawable: {}", request.drawable);
			dprintln("  format:   {}", request.format);
			dprintln("  depth:    {}", request.depth);
			dprintln("  dstX:     {}", request.dstX);
			dprintln("  dstY:     {}", request.dstY);
			dprintln("  width:    {}", request.width);
			dprintln("  height:   {}", request.height);
#endif

			auto& object = *client_info.objects[request.drawable];
			auto [out_data, out_w, out_h, out_depth] = get_pixmap_info(object);

			if (request.format == 0 && request.depth == 1)
				;
			else if (request.format == 2 && request.depth == 32)
				;
			else
			{
				dwarnln("PutImage unsupported format {}, depth {}", request.format, request.depth);
				break;
			}
 
			ASSERT(out_depth == 32);

			auto* out_data_u32 = out_data.as_span<uint32_t>().data();

			switch (request.format)
			{
				case 0:
					for (CARD32 y = 0; y < request.height; y++)
					{
						const auto dst_y = request.dstY + y;
						if (dst_y < 0 || dst_y >= out_h)
							continue;
						for (CARD32 x = 0; x < request.width; x++)
						{
							const auto dst_x = request.dstX + x;
							if (dst_x < 0 || dst_x >= out_w)
								continue;
							const auto bit_index = y * request.width + x;
							const auto byte = bit_index / 8;
							const auto bit = bit_index % 8;
							out_data_u32[dst_y * out_w + dst_x] = (packet[byte] & (1 << bit)) ? 0xFFFFFF : 0x000000; 
						}
					}
					break;
				case 2:
					auto* pixels_u32 = packet.as_span<const uint32_t>().data();
					for (CARD32 y = 0; y < request.height; y++)
					{
						const auto dst_y = request.dstY + y;
						if (dst_y < 0 || dst_y >= out_h)
							continue;
						for (CARD32 x = 0; x < request.width; x++)
						{
							const auto dst_x = request.dstX + x;
							if (dst_x < 0 || dst_x >= out_w)
								continue;
							out_data_u32[(request.dstY + y) * out_w + (request.dstX + x)] = pixels_u32[y * request.width + x];
						}
					}
					break;
			}

			if (object.type == Object::Type::Window)
				invalidate_window(client_info, request.drawable, request.dstX, request.dstY, request.width, request.height);

			break;
		}
		case X_GetImage:
		{
			auto request = decode<xGetImageReq>(packet).value();

			auto& object = *client_info.objects[request.drawable];
			auto [out_data, out_w, out_h, out_depth] = get_pixmap_info(object);

			// ZPixmap
			if (request.format != 2)
			{
				dwarnln("GetImage unsupported format {}", request.format);
				break;
			}

			ASSERT(request.x >= 0 && request.y >= 0);
			ASSERT(request.x + request.width <= out_w);
			ASSERT(request.y + request.height <= out_h);

			const size_t bytes = request.width * request.height * out_depth / 8;
			ASSERT(bytes % 4 == 0);

			xGetImageReply reply {
				.type = X_Reply,
				.depth = out_depth,
				.sequenceNumber = client_info.sequence,
				.length = static_cast<CARD32>(bytes / 4),
				.visual = visual.visualID,
			};
			TRY(encode(client_info.output_buffer, reply));
			TRY(encode(client_info.output_buffer, out_data));

			break;
		}
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
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_ListExtensions:
		{
			dprintln("ListExtensions");

			xListExtensionsReply reply {
				.type = X_Reply,
				.nExtensions = 0,
				.sequenceNumber = client_info.sequence,
				.length = 0,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_GetKeyboardMapping:
		{
			auto request = decode<xGetKeyboardMappingReq>(packet).value();

			dprintln("GetKeyboardMapping");
			dprintln("  firstKeyCode: {}", request.firstKeyCode);
			dprintln("  count:        {}", request.count);

			ASSERT(request.firstKeyCode + request.count - 1 <= keymap.size);

			xGetKeyboardMappingReply reply {
				.type = X_Reply,
				.keySymsPerKeyCode = 1,
				.sequenceNumber = client_info.sequence,
				.length = request.count,
			};
			TRY(encode(client_info.output_buffer, reply));

			for (size_t i = 0; i < request.count; i++)
				TRY(encode<CARD32>(client_info.output_buffer, keymap.map[request.firstKeyCode + i]));

			break;
		}
		case X_GetModifierMapping:
		{
			dprintln("GetModifierMapping");

			xGetModifierMappingReply reply {
				.type = X_Reply,
				.numKeyPerModifier = 1,
				.sequenceNumber = client_info.sequence,
				.length = 2,
			};
			TRY(encode(client_info.output_buffer, reply));
			TRY(encode<uint64_t>(client_info.output_buffer, 0));

			break;
		}
		default:
			dwarnln("unsupported opcode {}", opcode_to_name[packet[0]]);
			for (size_t i = 0; i < packet.size(); i++)
				fprintf(stddbg, " %02x", packet[i]);
			fprintf(stddbg, "\n");
			break;
	}

	return {};
}

int main()
{
	for (int sig = 1; sig < NSIG; sig++)
		signal(sig, exit);

	if (mkdir("/tmp/.X11-unix", 01777) == -1 && errno != EEXIST)
	{
		perror("xbanan: mkdir");
		return 1;
	}

	int server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (server_sock == -1)
	{
		perror("xbanan: socket");
		return 1;
	}

	const sockaddr_un addr {
		.sun_family = AF_UNIX,
		.sun_path = "/tmp/.X11-unix/X69"
	};
	if (bind(server_sock, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == -1)
	{
		perror("xbanan: bind");
		return 1;
	}
	atexit([] { unlink("/tmp/.X11-unix/X69"); });

	if (listen(server_sock, SOMAXCONN) == -1)
	{
		perror("xbanan: listen");
		return 1;
	}

	epoll_fd = epoll_create1(0);
	if (epoll_fd == -1)
	{
		perror("xbanan: epoll_create1");
		return 1;
	}

	epoll_event event { .events = EPOLLIN, .data = { .ptr = nullptr } };
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_sock, &event) == -1)
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

	printf("xbanan started\n");

	const auto close_client =
		[](int client_fd)
		{
			auto& epoll_thingy = epoll_thingies[client_fd];
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
				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, gui_window->server_fd(), nullptr);
				epoll_thingies.remove(gui_window->server_fd());
			}

			close(client_fd);
			epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
			epoll_thingies.remove(client_fd);
		};

	for (;;)
	{
		epoll_event events[16];
		const int event_count = epoll_wait(epoll_fd, events, 16, -1);

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

				MUST(epoll_thingies.insert(client_sock, {
					.type = EpollThingy::Type::Client,
					.value = Client {
						.fd = client_sock,
						.state = Client::State::ConnectionSetup,
					}
				}));

				epoll_event event = { .events = EPOLLIN, .data = { .fd = client_sock } };
				if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_sock, &event) == -1)
				{
					perror("xbanan: epoll_ctl");
					close(client_sock);
					continue;
				}

				dprintln("client {} connected", client_sock);
				continue;
			}

			auto it = epoll_thingies.find(events[i].data.fd);
			if (it == epoll_thingies.end())
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
			
			if (events[i].events & EPOLLOUT)
			{
				const ssize_t nsend = send(
					client_info.fd,
					client_info.output_buffer.data(),
					client_info.output_buffer.size(),
					0
				);

				if (nsend < 0)
					perror("xbanan: send");
				if (nsend <= 0)
				{
					close_client(events[i].data.fd);
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
					if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_info.fd, &event) == -1)
					{
						perror("xbanan: epoll_ctl");
						close_client(events[i].data.fd);
						continue;
					}
				}
			}

			if (!(events[i].events & EPOLLIN))
				continue;

			switch (client_info.state)
			{
				case Client::State::ConnectionSetup:
				{
					xConnClientPrefix client_prefix;
					ssize_t nrecv = recv(client_info.fd, &client_prefix, sizeof(client_prefix), 0);
					if (nrecv < 0)
						perror("xbanan: recv");
					if (nrecv <= 0)
					{
						close_client(events[i].data.fd);
						continue;
					}

					ASSERT(nrecv == sizeof(client_prefix));

					if (auto ret = setup_client_conneciton(client_info, client_prefix); ret.is_error())
					{
						dwarnln("setup_client_connection: {}", ret.error());
						close_client(events[i].data.fd);
						continue;
					}

					break;
				}
				case Client::State::Connected:
				{
					char buffer[1024] {};

					ssize_t nrecv = recv(client_info.fd, buffer, sizeof(buffer), 0);
					if (nrecv < 0)
						perror("xbanan: recv");
					if (nrecv <= 0)
					{
						close_client(events[i].data.fd);
						continue;
					}

					const size_t old_size = client_info.input_buffer.size();
					MUST(client_info.input_buffer.resize(old_size + nrecv));
					memcpy(client_info.input_buffer.data() + old_size, buffer, nrecv);

					for (;;)
					{
						if (client_info.input_buffer.size() < 4)
							break;

						const uint32_t byte_length = 4 * reinterpret_cast<const uint16_t*>(client_info.input_buffer.data())[1];
						if (client_info.input_buffer.size() < byte_length)
							break;

						auto packet = BAN::ConstByteSpan(client_info.input_buffer.span()).slice(0, byte_length);

						if (auto ret = handle_packet(client_info, packet); ret.is_error())
						{
							dwarnln("handle_packet: {}", ret.error());
							close_client(events[i].data.fd);
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
		for (auto& [_, thingy] : epoll_thingies)
		{
			if (thingy.type != EpollThingy::Type::Client)
				continue;

			auto& client_info = thingy.value.get<Client>();
			if (client_info.output_buffer.empty())
				continue;

			epoll_event event { .events = EPOLLIN | EPOLLOUT, .data = { .fd = client_info.fd } };
			if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client_info.fd, &event) == -1)
			{
				perror("xbanan: epoll_ctl");
				close_client(client_info.fd);
				goto iterator_invalidated;
			}
		}
	}
}

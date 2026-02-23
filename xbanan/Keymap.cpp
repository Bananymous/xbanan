#include "Keymap.h"

#include <BAN/StringView.h>

#include <LibInput/KeyboardLayout.h>

#include <X11/X.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>

#include <ctype.h>

#undef None

uint32_t g_keymap[0x100][g_keymap_layers];

static constexpr uint32_t my_key_to_x_keysym(LibInput::Key key)
{
	using LibInput::Key;

	switch (key)
	{
		case Key::A:
			return XK_A;
		case Key::B:
			return XK_B;
		case Key::C:
			return XK_C;
		case Key::D:
			return XK_D;
		case Key::E:
			return XK_E;
		case Key::F:
			return XK_F;
		case Key::G:
			return XK_G;
		case Key::H:
			return XK_H;
		case Key::I:
			return XK_I;
		case Key::J:
			return XK_J;
		case Key::K:
			return XK_K;
		case Key::L:
			return XK_L;
		case Key::M:
			return XK_M;
		case Key::N:
			return XK_N;
		case Key::O:
			return XK_O;
		case Key::P:
			return XK_P;
		case Key::Q:
			return XK_Q;
		case Key::R:
			return XK_R;
		case Key::S:
			return XK_S;
		case Key::T:
			return XK_T;
		case Key::U:
			return XK_U;
		case Key::V:
			return XK_V;
		case Key::W:
			return XK_W;
		case Key::X:
			return XK_X;
		case Key::Y:
			return XK_Y;
		case Key::Z:
			return XK_Z;
		case Key::A_Ring:
			return XK_Aring;
		case Key::A_Umlaut:
			return XK_Adiaeresis;
		case Key::O_Umlaut:
			return XK_Odiaeresis;
		case Key::_0:
			return XK_0;
		case Key::_1:
			return XK_1;
		case Key::_2:
			return XK_2;
		case Key::_3:
			return XK_3;
		case Key::_4:
			return XK_4;
		case Key::_5:
			return XK_5;
		case Key::_6:
			return XK_6;
		case Key::_7:
			return XK_7;
		case Key::_8:
			return XK_8;
		case Key::_9:
			return XK_9;
		case Key::F1:
			return XK_F1;
		case Key::F2:
			return XK_F2;
		case Key::F3:
			return XK_F3;
		case Key::F4:
			return XK_F4;
		case Key::F5:
			return XK_F5;
		case Key::F6:
			return XK_F6;
		case Key::F7:
			return XK_F7;
		case Key::F8:
			return XK_F8;
		case Key::F9:
			return XK_F9;
		case Key::F10:
			return XK_F10;
		case Key::F11:
			return XK_F11;
		case Key::F12:
			return XK_F12;
		case Key::Insert:
			return XK_Insert;
		case Key::PrintScreen:
			return XK_Print;
		case Key::Delete:
			return XK_Delete;
		case Key::Home:
			return XK_Home;
		case Key::End:
			return XK_End;
		case Key::PageUp:
			return XK_Page_Up;
		case Key::PageDown:
			return XK_Page_Down;
		case Key::Enter:
			return XK_Return;
		case Key::Space:
			return XK_space;
		case Key::ExclamationMark:
			return XK_exclam;
		case Key::DoubleQuote:
			return XK_quotedbl;
		case Key::Hashtag:
			return XK_numbersign;
		case Key::Currency:
			return XK_currency;
		case Key::Percent:
			return XK_percent;
		case Key::Ampersand:
			return XK_ampersand;
		case Key::Slash:
			return XK_slash;
		case Key::Section:
			return XK_section;
		case Key::Half:
			return XK_onehalf;
		case Key::OpenParenthesis:
			return '(';
		case Key::CloseParenthesis:
			return ')';
		case Key::OpenSquareBracket:
			return '[';
		case Key::CloseSquareBracket:
			return ']';
		case Key::OpenCurlyBracket:
			return '{';
		case Key::CloseCurlyBracket:
			return '}';
		case Key::Equals:
			return '=';
		case Key::QuestionMark:
			return '?';
		case Key::Plus:
			return '+';
		case Key::BackSlash:
			return '\\';
		case Key::Acute:
			return XK_acute;
		case Key::BackTick:
			return '`';
		case Key::TwoDots:
			return XK_diaeresis;
		case Key::Cedilla:
			return XK_Ccedilla;
		case Key::Backspace:
			return XK_BackSpace;
		case Key::AtSign:
			return XK_at;
		case Key::Pound:
			return XK_sterling;
		case Key::Dollar:
			return XK_dollar;
		case Key::Euro:
			return XK_EuroSign;
		case Key::Escape:
			return XK_Escape;
		case Key::Tab:
			return XK_Tab;
		case Key::CapsLock:
			return XK_Caps_Lock;
		case Key::LeftShift:
			return XK_Shift_L;
		case Key::LeftCtrl:
			return XK_Control_L;
		case Key::Super:
			return XK_Super_L;
		case Key::LeftAlt:
			return XK_Alt_L;
		case Key::RightAlt:
			return XK_Alt_R;
		case Key::RightCtrl:
			return XK_Control_R;
		case Key::RightShift:
			return XK_Shift_R;
		case Key::SingleQuote:
			return '\'';
		case Key::Asterix:
			return '*';
		case Key::Caret:
			return '^';
		case Key::Tilde:
			return '~';
		case Key::ArrowUp:
			return XK_Up;
		case Key::ArrowDown:
			return XK_Down;
		case Key::ArrowLeft:
			return XK_Left;
		case Key::ArrowRight:
			return XK_Right;
		case Key::Comma:
			return ',';
		case Key::Semicolon:
			return ';';
		case Key::Period:
			return '.';
		case Key::Colon:
			return ':';
		case Key::Hyphen:
			return '-';
		case Key::Underscore:
			return '_';
		case Key::NumLock:
			return XK_Num_Lock;
		case Key::ScrollLock:
			return XK_Scroll_Lock;
		case Key::LessThan:
			return '<';
		case Key::GreaterThan:
			return '>';
		case Key::Pipe:
			return '|';
		case Key::Negation:
			return XK_notsign;
		case Key::BrokenBar:
			return XK_brokenbar;
		case Key::Numpad0:
			return XK_KP_0;
		case Key::Numpad1:
			return XK_KP_1;
		case Key::Numpad2:
			return XK_KP_2;
		case Key::Numpad3:
			return XK_KP_3;
		case Key::Numpad4:
			return XK_KP_4;
		case Key::Numpad5:
			return XK_KP_5;
		case Key::Numpad6:
			return XK_KP_6;
		case Key::Numpad7:
			return XK_KP_7;
		case Key::Numpad8:
			return XK_KP_8;
		case Key::Numpad9:
			return XK_KP_9;
		case Key::NumpadPlus:
			return XK_KP_Add;
		case Key::NumpadMinus:
			return XK_KP_Subtract;
		case Key::NumpadMultiply:
			return XK_KP_Multiply;
		case Key::NumpadDivide:
			return XK_KP_Divide;
		case Key::NumpadEnter:
			return XK_KP_Enter;
		case Key::NumpadDecimal:
			return XK_KP_Decimal;
		case Key::VolumeMute:
			return XF86XK_AudioMute;
		case Key::VolumeUp:
			return XF86XK_AudioRaiseVolume;
		case Key::VolumeDown:
			return XF86XK_AudioLowerVolume;
		case Key::Calculator:
			return XF86XK_Calculator;
		case Key::MediaPlayPause:
			return XF86XK_AudioPlay;
		case Key::MediaStop:
			return XF86XK_AudioStop;
		case Key::MediaPrevious:
			return XF86XK_AudioPrev;
		case Key::MediaNext:
			return XF86XK_AudioNext;

		case Key::Invalid:
		case Key::None:
		case Key::Count:
			break;
	}

	return NoSymbol;
}

BAN::ErrorOr<void> initialize_keymap()
{
	for (auto& keycode : g_keymap)
		for (auto& keysym : keycode)
			keysym = NoSymbol;

	// FIXME: get this from somewhere (gui command? enviroment? tmp file?)
	const auto keymap_path = "./us.keymap"_sv;

	TRY(LibInput::KeyboardLayout::initialize());

	auto& keyboard_layout = LibInput::KeyboardLayout::get();
	TRY(keyboard_layout.load_from_file(keymap_path));

	const BAN::Span<const LibInput::Key> my_keymaps[] {
		keyboard_layout.keymap_normal(),
		keyboard_layout.keymap_shift(),
		keyboard_layout.keymap_altgr(),
		keyboard_layout.keymap_altgr(), // add shift+altgr map?
	};

	for (size_t keycode = g_keymap_min_keycode; keycode < g_keymap_max_keycode; keycode++)
		for (size_t layer = 0; layer < g_keymap_layers; layer++)
			if (auto my_key = my_keymaps[layer][keycode - g_keymap_min_keycode]; my_key != LibInput::Key::None)
				if (auto keysym = my_key_to_x_keysym(my_key); keysym != NoSymbol)
					g_keymap[keycode][layer] = (layer % 2) ? toupper(keysym) : tolower(keysym);

	return {};
}

#pragma once

#include <BAN/ByteSpan.h>
#include <BAN/Optional.h>
#include <BAN/Vector.h>
#include <BAN/WeakPtr.h>

#include <X11/Xproto.h>

struct PCFFont : public BAN::RefCounted<PCFFont>, public BAN::Weakable<PCFFont>
{
	struct Glyph
	{
		xCharInfo char_info;
		uint32_t bitmap_offset;
	};

	struct MapEntry
	{
		uint16_t codepoint;
		uint16_t glyph_index;
	};

	uint16_t min_char_or_byte2;
	uint16_t max_char_or_byte2;
	uint8_t min_byte1;
	uint8_t max_byte1;
	bool all_chars_exist;
	uint16_t default_char;

	xCharInfo min_bounds;
	xCharInfo max_bounds;

	int16_t font_ascent;
	int16_t font_descent;

	// TODO: parse properties

	BAN::Vector<Glyph> glyphs;
	BAN::Vector<MapEntry> map;
	BAN::Vector<uint8_t> bitmap;

	BAN::Optional<uint16_t> find_glyph(uint16_t codepoint) const
	{
		size_t l = 0;
		size_t r = map.size();
		while (l < r)
		{
			const size_t mid = (l + r) / 2;

			if (map[mid].codepoint == codepoint)
				return map[mid].glyph_index;

			if (map[mid].codepoint < codepoint)
				l = mid + 1;
			else
				r = mid;
		}
		return {};
	}
};

struct Client;
BAN::ErrorOr<void> open_font(Client& client_info, BAN::ConstByteSpan packet);
BAN::ErrorOr<void> close_font(Client& client_info, BAN::ConstByteSpan packet);
BAN::ErrorOr<void> query_font(Client& client_info, BAN::ConstByteSpan packet);
BAN::ErrorOr<void> list_fonts(Client& client_info, BAN::ConstByteSpan packet);
BAN::ErrorOr<void> poly_text(Client& client_info, BAN::ConstByteSpan packet, bool wide);
BAN::ErrorOr<void> image_text(Client& client_info, BAN::ConstByteSpan packet, bool wide);
BAN::ErrorOr<void> create_glyph_cursor(Client& client_info, BAN::ConstByteSpan packet);

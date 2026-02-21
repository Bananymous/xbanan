#include "Base.h"
#include "Definitions.h"
#include "Font.h"
#include "SafeGetters.h"
#include "Utils.h"

#include <BAN/Endianness.h>
#include <BAN/ScopeGuard.h>

#include <LibDEFLATE/Decompressor.h>

#include <X11/X.h>

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static BAN::HashMap<BAN::String, BAN::String> s_available_fonts;

static BAN::HashMap<BAN::String, BAN::WeakPtr<PCFFont>> s_loaded_fonts;

// https://fontforge.org/docs/techref/pcf-format.html

#define PCF_PROPERTIES       (1 << 0)
#define PCF_ACCELERATORS     (1 << 1)
#define PCF_METRICS          (1 << 2)
#define PCF_BITMAPS          (1 << 3)
#define PCF_INK_METRICS      (1 << 4)
#define PCF_BDF_ENCODINGS    (1 << 5)
#define PCF_SWIDTHS          (1 << 6)
#define PCF_GLYPH_NAMES      (1 << 7)
#define PCF_BDF_ACCELERATORS (1 << 8)

#define PCF_COMPRESSED_METRICS  0x00000100

struct pcf_table_entry
{
	BAN::LittleEndian<uint32_t> type;
	BAN::LittleEndian<uint32_t> format;
	BAN::LittleEndian<uint32_t> size;
	BAN::LittleEndian<uint32_t> offset;
};

struct pcf_header
{
	char magic[4];
	BAN::LittleEndian<uint32_t> table_count;
	pcf_table_entry tables[];
};

struct pcf_metrics_uncompressed
{
	int16_t left_sided_bearing;
	int16_t right_side_bearing;
	int16_t character_width;
	int16_t character_ascent;
	int16_t character_descent;
	uint16_t character_attributes;
};

struct pcf_metrics_compressed
{
	uint8_t left_sided_bearing;
	uint8_t right_side_bearing;
	uint8_t character_width;
	uint8_t character_ascent;
	uint8_t character_descent;
};

struct pcf_metrics
{
	BAN::LittleEndian<uint32_t> format;
	union
	{
		struct
		{
			int16_t count;
			pcf_metrics_compressed entries[];
		} compressed;
		struct
		{
			int32_t count;
			pcf_metrics_uncompressed entries[];
		} uncompressed;
	};
};

struct pcf_bitmap
{
	BAN::LittleEndian<uint32_t> format;
	int32_t glyph_count;
	int32_t glyph_offsets[/* glyph_count */];
	//int32_t bitmap_sizes[4];
	//char bitmap_data[bitmapsizes[format & 3]];
};

struct pcf_encoding
{
	BAN::LittleEndian<uint32_t> format;
	int16_t min_char_or_byte2;
	int16_t max_char_or_byte2;
	int16_t min_byte1;
	int16_t max_byte1;
	int16_t default_char;
	int16_t glyph_indices[];
};

static BAN::ErrorOr<BAN::Vector<uint8_t>> gzip_decompress_file(const BAN::String& path)
{
	int fd = open(path.data(), O_RDONLY);
	if (fd == -1)
		return BAN::Error::from_errno(errno);
	BAN::ScopeGuard _1([fd] { close(fd); });

	struct stat st;
	if (fstat(fd, &st) == -1)
	{
		close(fd);
		return BAN::Error::from_errno(errno);
	}

	void* addr = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (addr == MAP_FAILED)
		return BAN::Error::from_errno(errno);
	BAN::ScopeGuard _2([&st, addr] { munmap(addr, st.st_size); });

	auto file_data = BAN::ConstByteSpan(static_cast<const uint8_t*>(addr), st.st_size);
	LibDEFLATE::Decompressor decompressor(file_data, LibDEFLATE::StreamType::GZip);
	return TRY(decompressor.decompress());
}

static constexpr uint64_t reverse_bits(uint64_t value, uint8_t count)
{
	uint64_t reverse = 0;
	for (uint8_t bit = 0; bit < count; bit++)
		reverse |= ((value >> bit) & 1) << (count - bit - 1);
	return reverse;
}

static BAN::ErrorOr<BAN::RefPtr<PCFFont>> parse_font(const BAN::String& path)
{
	const auto font_data_vec = TRY(gzip_decompress_file(path));
	const auto font_data = BAN::ConstByteSpan(font_data_vec.span());

	const auto& header = *reinterpret_cast<const pcf_header*>(font_data.data());

	if (strncmp(header.magic, "\1fcp", 4) != 0)
	{
		dwarnln("pcf font invalid magic");
		return BAN::Error::from_errno(EINVAL);
	}

	BAN::ConstByteSpan metrics_data;
	BAN::ConstByteSpan bitmap_data;
	BAN::ConstByteSpan encoding_data;

	for (size_t i = 0; i < header.table_count; i++)
	{
		const auto& table = header.tables[i];

		switch (table.type)
		{
			case PCF_METRICS:
				metrics_data = font_data.slice(table.offset, table.size);
				break;
			case PCF_BITMAPS:
				bitmap_data = font_data.slice(table.offset, table.size);
				break;
			case PCF_BDF_ENCODINGS:
				encoding_data = font_data.slice(table.offset, table.size);
				break;
		}
	}

	if (metrics_data.empty() || bitmap_data.empty() || encoding_data.empty())
	{
		dwarnln("pcf font missing required table");
		return BAN::Error::from_errno(EINVAL);
	}

	auto* font_ptr = new PCFFont;
	if (font_ptr == nullptr)
		return BAN::Error::from_errno(ENOMEM);
	auto font = BAN::RefPtr<PCFFont>::adopt(font_ptr);

	{
		const auto& metrics_table = metrics_data.as<const pcf_metrics>();

		const bool little_endian = !(metrics_table.format & (1 << 2));

		const auto read_T =
			[little_endian]<typename T>(T value) -> T
			{
				return little_endian
					? BAN::little_endian_to_host(value)
					: BAN::big_endian_to_host(value);
			};

		if (metrics_table.format & PCF_COMPRESSED_METRICS)
		{
			const auto count = read_T(metrics_table.compressed.count);
			TRY(font->glyphs.resize(count));

			for (size_t i = 0; i < count; i++)
			{
				const auto& entry = metrics_table.compressed.entries[i];
				font->glyphs[i] = { .char_info = {
					.leftSideBearing  = static_cast<INT16>(entry.left_sided_bearing - 0x80),
					.rightSideBearing = static_cast<INT16>(entry.right_side_bearing - 0x80),
					.characterWidth   = static_cast<INT16>(entry.character_width    - 0x80),
					.ascent           = static_cast<INT16>(entry.character_ascent   - 0x80),
					.descent          = static_cast<INT16>(entry.character_descent  - 0x80),
					.attributes       = 0,
				}};
			}
		}
		else
		{
			const auto count = read_T(metrics_table.uncompressed.count);
			TRY(font->glyphs.resize(count));

			for (size_t i = 0; i < count; i++)
			{
				const auto& entry = metrics_table.uncompressed.entries[i];
				font->glyphs[i] = { .char_info = {
					.leftSideBearing  = read_T(entry.left_sided_bearing),
					.rightSideBearing = read_T(entry.right_side_bearing),
					.characterWidth   = read_T(entry.character_width),
					.ascent           = read_T(entry.character_ascent),
					.descent          = read_T(entry.character_descent),
					.attributes       = read_T(entry.character_attributes),
				}};
			}
		}
	}

	{
		const auto& bitmap_table = bitmap_data.as<const pcf_bitmap>();

		const bool little_endian = !(bitmap_table.format & (1 << 2));

		const auto read_T =
			[little_endian]<typename T>(T value) -> T
			{
				return little_endian
					? BAN::little_endian_to_host(value)
					: BAN::big_endian_to_host(value);
			};

		const auto count = read_T(bitmap_table.glyph_count);

		if (count != font->glyphs.size())
		{
			dwarnln("bitmap entry count and metric entry count mismatch");
			return BAN::Error::from_errno(EINVAL);
		}

		const uint8_t scanline_pad = 1 << ((bitmap_table.format >> 0) & 3);
		const uint8_t data_type    = 1 << ((bitmap_table.format >> 4) & 3);

		const uint8_t* bitmap_base = reinterpret_cast<const uint8_t*>(bitmap_table.glyph_offsets + count + 4);

		for (size_t i = 0; i < count; i++)
		{
			const uint8_t* glyph_bitmap = bitmap_base + read_T(bitmap_table.glyph_offsets[i]);

			const auto& char_info = font->glyphs[i].char_info;
			const auto width  = char_info.rightSideBearing - char_info.leftSideBearing;
			const auto height = char_info.ascent + char_info.descent;
			const auto bytes_per_scanline = (width + scanline_pad * 8 - 1) / (scanline_pad * 8) * scanline_pad;

			font->glyphs[i].bitmap_offset = font->bitmap.size();

			for (int32_t i = 0; i < height; i++)
			{
				const uint8_t* row_bytes = glyph_bitmap + i * bytes_per_scanline;

				for (int32_t j = 0; j * data_type * 8 < width; j++)
				{
					uint64_t normalized;
					switch (data_type)
					{
						case 1: normalized = row_bytes[j]; break;
						case 2: normalized = read_T(reinterpret_cast<const uint16_t*>(row_bytes)[j]); break;
						case 4: normalized = read_T(reinterpret_cast<const uint32_t*>(row_bytes)[j]); break;
						case 8: normalized = read_T(reinterpret_cast<const uint64_t*>(row_bytes)[j]); break;
					}

					if (bitmap_table.format & (1 << 3))
						normalized = reverse_bits(normalized, data_type * 8);

					for (int32_t k = 0; k < data_type; k++)
						TRY(font->bitmap.push_back(normalized >> (k * 8)));
				}
			}
		}

		(void)font->bitmap.shrink_to_fit();
	}

	{
		const auto& encoding_table = encoding_data.as<const pcf_encoding>();

		const bool little_endian = !(encoding_table.format & (1 << 2));

		const auto read_T =
			[little_endian]<typename T>(T value) -> T
			{
				return little_endian
					? BAN::little_endian_to_host(value)
					: BAN::big_endian_to_host(value);
			};

		font->min_char_or_byte2 = read_T(encoding_table.min_char_or_byte2);
		font->max_char_or_byte2 = read_T(encoding_table.max_char_or_byte2);
		font->min_byte1 = read_T(encoding_table.min_byte1);
		font->max_byte1 = read_T(encoding_table.max_byte1);
		font->default_char = read_T(encoding_table.default_char);

		const auto rows = font->max_byte1 - font->min_byte1 + 1;
		const auto cols = font->max_char_or_byte2 - font->min_char_or_byte2 + 1;

		font->all_chars_exist = true;
		for (size_t row = 0; row < rows; row++)
		{
			for (size_t col = 0; col < cols; col++)
			{
				const uint16_t glyph = read_T(encoding_table.glyph_indices[row * cols + col]);
				if (glyph == 0xFFFF || glyph >= font->glyphs.size())
				{
					font->all_chars_exist = false;
					continue;
				}

				const uint8_t byte1 = row + font->min_byte1;
				const uint8_t byte2 = col + font->min_char_or_byte2;
				const uint16_t codepoint = (byte1 << 8) | byte2;

				TRY(font->map.push_back({
					.codepoint = codepoint,
					.glyph_index = glyph,
				}));
			}
		}

		(void)font->map.shrink_to_fit();
	}

	font->min_bounds = {
		.leftSideBearing = INT16_MAX,
		.rightSideBearing = INT16_MAX,
		.characterWidth = INT16_MAX,
		.ascent = INT16_MAX,
		.descent = INT16_MAX,
	};
	font->max_bounds = {
		.leftSideBearing = INT16_MIN,
		.rightSideBearing = INT16_MIN,
		.characterWidth = INT16_MIN,
		.ascent = INT16_MIN,
		.descent = INT16_MIN,
	};

	for (const auto& glyph : font->glyphs)
	{
		const auto& ci = glyph.char_info;

		font->min_bounds.leftSideBearing  = BAN::Math::min(font->min_bounds.leftSideBearing,  ci.leftSideBearing);
		font->min_bounds.rightSideBearing = BAN::Math::min(font->min_bounds.rightSideBearing, ci.rightSideBearing);
		font->min_bounds.characterWidth   = BAN::Math::min(font->min_bounds.characterWidth,   ci.characterWidth);
		font->min_bounds.ascent           = BAN::Math::min(font->min_bounds.ascent,           ci.ascent);
		font->min_bounds.descent          = BAN::Math::min(font->min_bounds.descent,          ci.descent);

		font->max_bounds.leftSideBearing  = BAN::Math::max(font->max_bounds.leftSideBearing,  ci.leftSideBearing);
		font->max_bounds.rightSideBearing = BAN::Math::max(font->max_bounds.rightSideBearing, ci.rightSideBearing);
		font->max_bounds.characterWidth   = BAN::Math::max(font->max_bounds.characterWidth,   ci.characterWidth);
		font->max_bounds.ascent           = BAN::Math::max(font->max_bounds.ascent,           ci.ascent);
		font->max_bounds.descent          = BAN::Math::max(font->max_bounds.descent,          ci.descent);
	}

	font->font_ascent  = font->max_bounds.ascent;
	font->font_descent = font->max_bounds.descent;

	return font;
}

__attribute__((constructor))
static void initialize_fonts()
{
	const char* font_path = "fonts/misc";

	do
	{
		char fonts_dir[PATH_MAX];
		sprintf(fonts_dir, "%s/fonts.dir", font_path);

		FILE* fp = fopen(fonts_dir, "r");
		if (fp == NULL)
		{
			perror("fopen");
			break;
		}

		for (;;)
		{
			char buffer[1024];
			if (fgets(buffer, sizeof(buffer), fp) == NULL)
				break;
			if (buffer[0] == '\n')
				continue;

			char file[512], name[512], dummy;
			if (sscanf(buffer, "%s %s%c", file, name, &dummy) != 3 || dummy != '\n')
				continue;

			auto path = MUST(BAN::String::formatted("{}/{}", font_path, file));

			struct stat st;
			if (stat(path.data(), &st) != 0)
				continue;

			MUST(s_available_fonts.insert(BAN::String(name), BAN::move(path)));
		}

		fclose(fp);
	} while (false);

	do
	{
		char fonts_alias[PATH_MAX];
		sprintf(fonts_alias, "%s/fonts.alias", font_path);

		FILE* fp = fopen(fonts_alias, "r");
		if (fp == NULL)
		{
			perror("fopen");
			break;
		}

		for (;;)
		{
			char buffer[1024];
			if (fgets(buffer, sizeof(buffer), fp) == NULL)
				break;
			if (buffer[0] == '\n')
				continue;

			char alias[512], name[512], dummy;
			if (sscanf(buffer, "%s %s%c", alias, name, &dummy) != 3 || dummy != '\n')
				continue;

			auto it = s_available_fonts.find(BAN::String(name));
			if (it == s_available_fonts.end())
				continue;	

			MUST(s_available_fonts.insert(BAN::String(alias), it->value));
		}

		fclose(fp);
	} while(false);

	printf("found %zu fonts and aliases\n", s_available_fonts.size());
}

static bool matches_pattern(const char* pattern, const char* string)
{
	while (*pattern)
	{
		switch (*pattern)
		{
			case '*':
			{
				const char* ptr = string + strlen(string);
				while (ptr >= string)
					if (matches_pattern(pattern + 1, ptr--) == 0)
						return 0;
				return false;
			}
			case '?':
			{
				if (*string == '\0')
					return false;
				pattern++;
				string++;
				continue;
			}
		}

		if (*pattern == '\0')
			break;

		if (*pattern != *string)
			return false;
		pattern++;
		string++;
	}

	return *string ? false : true;
}

static BAN::ErrorOr<BAN::RefPtr<PCFFont>> get_fontable(Client& client_info, CARD32 fid, BYTE opcode)
{
	auto it = g_objects.find(fid);
	if (it != g_objects.end() && it->value->type == Object::Type::GraphicsContext)
		return get_fontable(client_info, it->value->object.get<Object::GraphicsContext>().font, opcode);
	if (it == g_objects.end() || it->value->type != Object::Type::Font)
	{
		xError error {
			.type = X_Error,
			.errorCode = BadFont,
			.sequenceNumber = client_info.sequence,
			.resourceID = fid,
			.minorCode = 0,
			.majorCode = opcode,
		};
		TRY(encode(client_info.output_buffer, error));
		return BAN::Error::from_errno(ENOENT);
	}

	return it->value->object.get<BAN::RefPtr<PCFFont>>();
}

BAN::ErrorOr<void> open_font(Client& client_info, BAN::ConstByteSpan packet)
{
	auto request = decode<xOpenFontReq>(packet).value();

	BAN::String pattern = BAN::StringView((char*)packet.data(), request.nbytes);

	dprintln("OpenFont");
	dprintln("  fid:     {}", request.fid);
	dprintln("  pattern: {}", pattern);

	for (const auto& [name, path] : s_available_fonts)
	{
		if (!matches_pattern(pattern.data(), name.data()))
			continue;

		BAN::RefPtr<PCFFont> font;
		if (auto it = s_loaded_fonts.find(path); it != s_loaded_fonts.end())
			font = it->value.lock();

		if (!font)
		{
			auto font_or_error = parse_font(path);
			if (font_or_error.is_error())
				continue;
			font = font_or_error.release_value();
			TRY(s_loaded_fonts.insert_or_assign(path, TRY((font->get_weak_ptr()))));
		}

		TRY(client_info.objects.insert(request.fid));
		TRY(g_objects.insert(
			request.fid,
			TRY(BAN::UniqPtr<Object>::create(Object {
				.type = Object::Type::Font,
				.object = font,
			}))
		));

		return {};
	}

	xError error {
		.type = X_Error,
		.errorCode = BadName,
		.sequenceNumber = client_info.sequence,
		.resourceID = 0,
		.minorCode = 0,
		.majorCode = X_OpenFont,
	};
	TRY(encode(client_info.output_buffer, error));
	return {};
}

BAN::ErrorOr<void> close_font(Client& client_info, BAN::ConstByteSpan packet)
{
	const CARD32 fid = packet.as_span<const CARD32>()[1];

	dprintln("CloseFont");
	dprintln("  fid: {}", fid);

	(void)TRY(get_fontable(client_info, fid, X_CloseFont));
	client_info.objects.remove(fid);
	g_objects.remove(fid);

	return {};
}

BAN::ErrorOr<void> query_font(Client& client_info, BAN::ConstByteSpan packet)
{
	const CARD32 fid = packet.as_span<const CARD32>()[1];

	dprintln("QueryFont");
	dprintln("  fid: {}", fid);

	auto font = TRY(get_fontable(client_info, fid, X_QueryFont));

	xQueryFontReply reply {
		.type = X_Reply,
		.sequenceNumber = client_info.sequence,
		.length = static_cast<CARD32>(7 + 3 * font->glyphs.size()),
		.minBounds = font->min_bounds,
		.maxBounds = font->max_bounds,
		.minCharOrByte2 = font->min_char_or_byte2,
		.maxCharOrByte2 = font->max_char_or_byte2,
		.defaultChar = font->default_char,
		.nFontProps = 0, // TODO
		.drawDirection = FontLeftToRight,
		.minByte1 = font->min_byte1,
		.maxByte1 = font->max_byte1,
		.allCharsExist = font->all_chars_exist,
		.fontAscent = font->font_ascent,
		.fontDescent = font->font_descent,
		.nCharInfos = static_cast<CARD32>(font->glyphs.size()),
	};
	TRY(encode(client_info.output_buffer, reply));

	for (const auto& glyph : font->glyphs)
		TRY(encode(client_info.output_buffer, glyph.char_info));

	return {};
}

BAN::ErrorOr<void> list_fonts(Client& client_info, BAN::ConstByteSpan packet)
{
	auto request = decode<xListFontsReq>(packet).value();

	BAN::String pattern = BAN::StringView((char*)packet.data(), request.nbytes);

	dprintln("ListFonts");
	dprintln("  maxNames: {}", request.maxNames);
	dprintln("  pattern:  {}", pattern);

	CARD16 nfonts = 0;
	BAN::Vector<uint8_t> result;
	for (const auto& [name, _] : s_available_fonts)
	{
		if (nfonts == request.maxNames)
			break;
		if (!matches_pattern(pattern.data(), name.data()))
			continue;
		TRY(encode<BYTE>(result, name.size()));
		TRY(encode(result, name));
		nfonts++;
	}

	xListFontsReply reply {
		.type = X_Reply,
		.sequenceNumber = client_info.sequence,
		.length = static_cast<CARD32>((result.size() + 3) / 4),
		.nFonts = nfonts,
	};
	TRY(encode(client_info.output_buffer, reply));
	TRY(encode(client_info.output_buffer, result));

	return {};
}

struct WriteTextInfo
{
	uint32_t string_len;
	const uint8_t* string;
	bool wide;

	int32_t x;
	int32_t y;

	uint32_t* out_data_u32;
	uint32_t out_w, out_h;

	int32_t min_x;
	int32_t max_x;
	int32_t min_y;
	int32_t max_y;

	const PCFFont* font;
	const Object::GraphicsContext& gc;
};

static void write_text(WriteTextInfo& info)
{
	for (size_t i = 0; i < info.string_len; i++)
	{
		const uint16_t codepoint = info.wide ? (info.string[i * 2] << 8) | info.string[i * 2 + 1] : info.string[i];
		
		auto glyph_index = info.font->find_glyph(codepoint);
		if (!glyph_index.has_value())
			continue;

		const auto& glyph = info.font->glyphs[glyph_index.value()];
		const auto& ci = glyph.char_info;

		const auto width = ci.rightSideBearing - ci.leftSideBearing;
		const auto height = ci.ascent + ci.descent;

		const uint8_t* glyph_bitmap_base = info.font->bitmap.data() + glyph.bitmap_offset;
		for (auto rel_y = 0; rel_y < height; rel_y++)
		{
			const uint8_t* glyph_row_base = glyph_bitmap_base + (width + 7) / 8 * rel_y;
			for (auto rel_x = 0; rel_x < width; rel_x++)
			{
				const auto out_x = info.x + rel_x + ci.leftSideBearing;
				const auto out_y = info.y + rel_y - ci.ascent;
				if (out_x < 0 || out_y < 0 || out_x >= info.out_w || out_y >= info.out_h)
					continue;
				if (info.gc.is_clipped(out_x, out_y))
					continue;

				const auto color = (glyph_row_base[rel_x / 8] & (1 << (rel_x % 8)))
					? info.gc.foreground
					: info.gc.background;

				if (color != LibGUI::Texture::color_invisible)
					info.out_data_u32[out_y * info.out_w + out_x] = color;
			}
		}

		info.min_x = BAN::Math::min(info.min_x, info.x + ci.leftSideBearing);
		info.max_x = BAN::Math::max(info.max_x, info.x + ci.rightSideBearing);

		info.min_y = BAN::Math::min(info.min_y, info.y - ci.ascent);
		info.max_y = BAN::Math::max(info.max_y, info.y + ci.descent);

		info.x += ci.characterWidth;
	}
}

BAN::ErrorOr<void> poly_text(Client& client_info, BAN::ConstByteSpan packet, bool wide)
{
	const BYTE opcode = wide ? X_PolyText16 : X_PolyText8;

	auto request = decode<xPolyTextReq>(packet).value();

	dprintln("PolyText{}", wide ? "16" : "8");
	dprintln("  drawable: {}", request.drawable);
	dprintln("  gc:       {}", request.gc);
	dprintln("  x:        {}", request.x);
	dprintln("  y:        {}", request.y);

	auto [out_data_u32, out_w, out_h, _] = TRY(get_drawable_info(client_info, request.drawable, opcode));

	const auto& gc = TRY_REF(get_gc(client_info, request.gc, opcode));

	WriteTextInfo info {
		.string_len = 0,
		.string = nullptr,
		.wide = wide,
		.x = request.x,
		.y = request.y,
		.out_data_u32 = out_data_u32,
		.out_w = out_w,
		.out_h = out_h,
		.min_x = INT32_MAX,
		.max_x = INT32_MIN,
		.min_y = INT32_MAX,
		.max_y = INT32_MIN,
		.font = nullptr,
		.gc = gc,
	};

	while (!packet.empty())
	{
		if (packet[0] == 255)
		{
			if (packet.size() < 5)
				break;
			const CARD32 fid =
				packet[1] << 24 |
				packet[2] << 16 |
				packet[3] <<  8 |
				packet[4] <<  0;
			info.font = TRY(get_fontable(client_info, fid, opcode)).ptr();
			packet = packet.slice(5);
			continue;
		}

		if (packet.size() < 2 + packet[0] * (1 + wide))
			break;

		if (info.font == nullptr)
			info.font = TRY(get_fontable(client_info, gc.font, opcode)).ptr();

		info.string_len = packet[0];
		info.string = packet.data() + 2;

		write_text(info);

		info.x += packet[1];
		packet = packet.slice(2 + packet[0] * (1 + wide));
	}

	if (g_objects[request.drawable]->type == Object::Type::Window)
		invalidate_window(request.drawable, info.min_x, info.min_y, info.max_x - info.min_x + 1, info.max_y - info.min_y + 1);

	return {};
}

BAN::ErrorOr<void> image_text(Client& client_info, BAN::ConstByteSpan packet, bool wide)
{
	const BYTE opcode = wide ? X_ImageText16 : X_ImageText8;

	auto request = decode<xImageTextReq>(packet).value();

	dprintln("ImageText{}", wide ? "16" : "8");
	dprintln("  nChars:   {}", request.nChars);
	dprintln("  drawable: {}", request.drawable);
	dprintln("  gc:       {}", request.gc);
	dprintln("  x:        {}", request.x);
	dprintln("  y:        {}", request.y);

	auto [out_data_u32, out_w, out_h, _] = TRY(get_drawable_info(client_info, request.drawable, opcode));

	const auto& gc = TRY_REF(get_gc(client_info, request.gc, opcode));

	WriteTextInfo info {
		.string_len = request.nChars,
		.string = packet.data(),
		.wide = wide,
		.x = request.x,
		.y = request.y,
		.out_data_u32 = out_data_u32,
		.out_w = out_w,
		.out_h = out_h,
		.min_x = INT32_MAX,
		.max_x = INT32_MIN,
		.min_y = INT32_MAX,
		.max_y = INT32_MIN,
		.font = TRY(get_fontable(client_info, gc.font, opcode)).ptr(),
		.gc = gc,
	};
	write_text(info);

	if (g_objects[request.drawable]->type == Object::Type::Window)
		invalidate_window(request.drawable, info.min_x, info.min_y, info.max_x - info.min_x + 1, info.max_y - info.min_y + 1);

	return {};
}

BAN::ErrorOr<void> create_glyph_cursor(Client& client_info, BAN::ConstByteSpan packet)
{
	auto request = decode<xCreateGlyphCursorReq>(packet).value();

	dprintln("CreateGlyphCursor");
	dprintln("  cid:        {}", request.cid);
	dprintln("  source:     {}", request.source);
	dprintln("  mask:       {}", request.mask);
	dprintln("  sourceChar: {}", request.sourceChar);
	dprintln("  maskChar:   {}", request.maskChar);
	dprintln("  foreRed:    {}", request.foreRed);
	dprintln("  foreGreen:  {}", request.foreGreen);
	dprintln("  foreBlue:   {}", request.foreBlue);
	dprintln("  backRed:    {}", request.backRed);
	dprintln("  backGreen:  {}", request.backGreen);
	dprintln("  backBlue:   {}", request.backBlue);

	const uint32_t foreground =
		static_cast<uint32_t>(request.foreRed   >> 8) << 16 |
		static_cast<uint32_t>(request.foreBlue  >> 8) <<  8 |
		static_cast<uint32_t>(request.foreGreen >> 8) <<  0;
	const uint32_t background =
		static_cast<uint32_t>(request.backRed   >> 8) << 16 |
		static_cast<uint32_t>(request.backBlue  >> 8) <<  8 |
		static_cast<uint32_t>(request.backGreen >> 8) <<  0;

	const auto& source_font = TRY(get_fontable(client_info, request.source, X_CreateGlyphCursor));

	auto source_glyph_index = source_font->find_glyph(request.sourceChar);
	if (!source_glyph_index.has_value())
	{
		xError error {
			.type = X_Error,
			.errorCode = BadValue,
			.sequenceNumber = client_info.sequence,
			.resourceID = request.source,
			.minorCode = 0,
			.majorCode = X_CreateGlyphCursor,
		};
		TRY(encode(client_info.output_buffer, error));
		return {};
	}

	const auto& source_glyph = source_font->glyphs[source_glyph_index.value()];
	const auto& source_ci = source_glyph.char_info;
	const uint32_t source_width = source_ci.rightSideBearing - source_ci.leftSideBearing;
	const uint32_t source_height = source_ci.ascent + source_ci.descent;

	Object::Cursor cursor {
		.width = source_width,
		.height = source_height,
		.origin_x = -source_ci.leftSideBearing,
		.origin_y =  source_ci.ascent,
	};
	TRY(cursor.pixels.resize(cursor.width * cursor.height));

	for (uint32_t y = 0; y < source_height; y++)
	{
		const uint8_t* row_base = source_font->bitmap.data() + source_glyph.bitmap_offset + (source_width + 7) / 8 * y;
		for (uint32_t x = 0; x < source_width; x++)
			cursor.pixels[y * source_width + x] = 0xFF000000 | ((row_base[x / 8] & (1 << (x % 8))) ? foreground : background);
	}

	if (request.mask != None)
	{
		const auto& mask_font = TRY(get_fontable(client_info, request.mask, X_CreateGlyphCursor));

		auto mask_glyph_index = mask_font->find_glyph(request.maskChar);
		if (!mask_glyph_index.has_value())
		{
			xError error {
				.type = X_Error,
				.errorCode = BadValue,
				.sequenceNumber = client_info.sequence,
				.resourceID = request.mask,
				.minorCode = 0,
				.majorCode = X_CreateGlyphCursor,
			};
			TRY(encode(client_info.output_buffer, error));
			return {};
		}

		const auto& mask_glyph = mask_font->glyphs[source_glyph_index.value()];
		const auto& mask_ci = mask_glyph.char_info;
		const int32_t mask_width = mask_ci.rightSideBearing - mask_ci.leftSideBearing;
		const int32_t mask_height = mask_ci.ascent + mask_ci.descent;

		for (int32_t src_y = 0; src_y < source_height; src_y++)
		{
			for (int32_t src_x = 0; src_x < source_width; src_x++)
			{
				const auto mask_x = src_x + source_ci.leftSideBearing - mask_ci.leftSideBearing;
				const auto mask_y = src_y + source_ci.ascent          - mask_ci.ascent;
				if (mask_x < 0 || mask_y < 0 || mask_x >= mask_width || mask_y >= mask_height)
					continue;

				const uint8_t* row_base = mask_font->bitmap.data() + mask_glyph.bitmap_offset + (mask_width + 7) / 8 * mask_y;
				if (!(row_base[mask_x / 8] & (1 << (mask_x % 8))))
					cursor.pixels[src_y * source_width + src_x] = 0;
			}
		}
	}

	TRY(client_info.objects.insert(request.cid));
	TRY(g_objects.insert(
		request.cid,
		TRY(BAN::UniqPtr<Object>::create(Object {
			.type = Object::Type::Cursor,
			.object = BAN::move(cursor),
		}))
	));

	return {};
}

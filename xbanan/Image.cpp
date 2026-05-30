#include "Definitions.h"
#include "Image.h"

#include <X11/X.h>

uint32_t image_dwords(uint32_t width, uint32_t height, uint8_t depth)
{
	uint8_t bpp = 0;
	for (const auto& format : g_formats)
		if (format.depth == depth)
			bpp = format.bitsPerPixel;
	ASSERT(bpp && 32 % bpp == 0);
	return (width * bpp + 31) / 32 * height;
}

void put_image(const PutImageInfo& info)
{
	uint8_t in_bpp = 0;
	for (const auto& format : g_formats)
		if (format.depth == info.in_depth)
			in_bpp = format.bitsPerPixel;
	ASSERT(in_bpp && 32 % in_bpp == 0);

	const auto min_x = BAN::Math::max<int32_t>(0, -info.out_x);
	const auto min_y = BAN::Math::max<int32_t>(0, -info.out_y);

	const auto max_x = BAN::Math::min<int32_t>(info.w, info.out_w - info.out_x);
	const auto max_y = BAN::Math::min<int32_t>(info.h, info.out_h - info.out_y);

	auto* out_data_u32 = static_cast<uint32_t*>(info.out_data);
	const auto* in_data_u32 = static_cast<const uint32_t*>(info.in_data);

	ASSERT(info.format == XYBitmap || info.in_depth == info.out_depth);

	switch (info.format)
	{
		case XYBitmap:
			ASSERT(info.in_depth == 1);
			[[fallthrough]];
		case XYPixmap:
		{
			const auto dwords_per_plane = (info.left_pad + info.in_w + 31) / 32;
			for (int32_t y = min_y; y < max_y; y++)
			{
				const auto dst_off = (info.out_y + y) * info.out_w + info.out_x;
				for (int32_t x = min_x; x < max_x; x++)
				{
					const auto bit_index = info.left_pad + (info.in_y + y) * info.in_w + (info.in_x + x);
					const auto dword = bit_index / 32;
					const auto bit_mask = 1u << (bit_index % 32);
					uint32_t pixel = 0;
					for (size_t i = 0; i < info.in_depth; i++)
						if (in_data_u32[dword + i * dwords_per_plane]  & bit_mask)
							pixel |= 1u << i;
					if (!info.gc.is_clipped(x, y))
						out_data_u32[dst_off + x] = pixel;
				}
			}
			break;
		}
		case ZPixmap:
		{
			bool needs_clipping = true;

			if (info.gc.clip_mask == None)
			{
				needs_clipping = false;
			}
			else if (info.gc.clip_mask == UINT32_MAX)
			{
				const uint32_t clip_min_x = min_x - info.gc.clip_origin_x;
				const uint32_t clip_min_y = min_y - info.gc.clip_origin_y;

				const uint32_t clip_max_x = max_x - info.gc.clip_origin_x;
				const uint32_t clip_max_y = max_y - info.gc.clip_origin_y;

				for (const auto& rect : info.gc.clip_rects)
				{
					if (clip_min_x < rect.x || clip_max_x > rect.x + rect.width)
						continue;
					if (clip_min_y < rect.y || clip_max_y > rect.y + rect.height)
						continue;
					needs_clipping = false;
					break;
				}
			}

			ASSERT(info.left_pad == 0);
			if (in_bpp == 32 && !needs_clipping)
			{
				const auto bytes_per_row = (max_x - min_x) * 4;
				for (int32_t y = min_y; y < max_y; y++)
				{
					const auto dst_off = (info.out_y + y) * info.out_w + (info.out_x + min_x);
					const auto src_off = (info. in_y + y) * info. in_w + (info. in_x + min_y);
					memcpy(&out_data_u32[dst_off], &in_data_u32[src_off], bytes_per_row);
				}
			}
			else
			{
				const auto pixel_mask = (1u << info.in_depth) - 1;
				const auto bits_per_scanline = (info.in_w * in_bpp + 31) / 32 * 32;
				for (int32_t y = min_y; y < max_y; y++)
				{
					const auto dst_off = (info.out_y + y) * info.out_w + info.out_x;
					for (int32_t x = min_x; x < max_x; x++)
					{
						const auto bit_offset = (info.in_y + y) * bits_per_scanline + (info.in_x + x) * in_bpp;
						const auto dword = bit_offset / 32;
						const auto shift = bit_offset % 32;
						if (!info.gc.is_clipped(x, y))
							out_data_u32[dst_off + x] = (in_data_u32[dword] >> shift) & pixel_mask;
					}
				}
			}
			break;
		}
		default:
			ASSERT_NOT_REACHED();
	}
}

void get_image(const GetImageInfo& info)
{
	uint8_t out_bpp = 0;
	for (const auto& format : g_formats)
		if (format.depth == info.depth)
			out_bpp = format.bitsPerPixel;
	ASSERT(out_bpp && 32 % out_bpp == 0);

	ASSERT(info.in_x >= 0 && info.in_y >= 0);
	ASSERT(info.in_x + info.w <= info.in_w);
	ASSERT(info.in_y + info.h <= info.in_h);

	if (info.format != XYBitmap && info.format != ZPixmap)
		dwarnln("GetImage with format {}", info.format);

	auto* out_data_u32 = static_cast<uint32_t*>(info.out_data);
	const auto* in_data_u32 = static_cast<const uint32_t*>(info.in_data);

	if (out_bpp == 32)
	{
		for (int32_t y = 0; y < info.h; y++)
		{
			const size_t out_off = y * info.w;
			const size_t in_off = (info.in_y + y) * info.in_w + info.in_x;
			memcpy(&out_data_u32[out_off], &in_data_u32[in_off], info.w * 4);
		}
	}
	else
	{
		const auto dwords_per_scanline = (info.w * out_bpp + 31) / 32;

		memset(out_data_u32, 0, dwords_per_scanline * info.h * 4);

		for (int32_t y = 0; y < info.h; y++)
		{
			const auto in_row_off = (info.in_y + y) * info.in_w + info.in_x;
			for (int32_t x = 0; x < info.w; x++)
			{
				const auto bit_offset = x * out_bpp;
				const auto dword = bit_offset / 32;
				const auto shift = bit_offset % 32;
				const auto mask = (1u << out_bpp) - 1;
				out_data_u32[dword] |= (in_data_u32[in_row_off + x] & mask) << shift;
			}
		}
	}

}

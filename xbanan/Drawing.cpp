#include "Base.h"
#include "Drawing.h"
#include "SafeGetters.h"
#include "Utils.h"

#include <X11/X.h>

struct DrawSegmentInfo
{
	int32_t x1, y1;
	int32_t x2, y2;

	uint32_t* out_data_u32;
	uint32_t out_w, out_h;

	int32_t min_x;
	int32_t max_x;
	int32_t min_y;
	int32_t max_y;

	const Object::GraphicsContext& gc;
};

static void draw_segment(DrawSegmentInfo& info)
{
	const int32_t width = info.gc.line_width;

	const int32_t min_x = BAN::Math::max<int32_t>(BAN::Math::min(info.x1, info.x2) - width / 2, 0);
	const int32_t min_y = BAN::Math::max<int32_t>(BAN::Math::min(info.y1, info.y2) - width / 2, 0);
	const int32_t max_x = BAN::Math::min<int32_t>(BAN::Math::max(info.x1, info.x2) + width / 2, info.out_w);
	const int32_t max_y = BAN::Math::min<int32_t>(BAN::Math::max(info.y1, info.y2) + width / 2, info.out_h);

	// FIXME: support non solid lines

	if (width == 0)
	{
		const auto is_in_bounds =
			[&info](int32_t x, int32_t y) -> bool
			{
				return x >= 0 && y >= 0 && x < info.out_w && y < info.out_h;
			};

		int32_t x = info.x1;
		int32_t y = info.y1;

		const int32_t dx = +BAN::Math::abs(info.x2 - info.x1);
		const int32_t dy = -BAN::Math::abs(info.y2 - info.y1);
		const int32_t sx = info.x1 < info.x2 ? 1 : -1;
		const int32_t sy = info.y1 < info.y2 ? 1 : -1;
		int32_t err = dx + dy;

		for (;;)
		{
			if (is_in_bounds(x, y) && !info.gc.is_clipped(x, y))
				info.out_data_u32[y * info.out_w + x] = info.gc.foreground;

			const int32_t e2 = 2 * err;

			if (e2 >= dy)
			{
				if (x == info.x2)
					break;
				err += dy;
				x += sx;
			}

			if (e2 <= dx)
			{
				if (y == info.y2)
					break;
				err += dx;
				y += sy;
			}
		}
	}
	else
	{
		const int32_t dx = info.x2 - info.x1;
		const int32_t dy = info.y2 - info.y1;
		const int32_t x2y1 = info.x2 * info.y1;
		const int32_t y2x1 = info.y2 * info.x1;
		const int32_t den2 = dy * dy + dx * dx;

		for (int32_t y = min_y; y <= max_y; y++)
		{
			for (int32_t x = min_x; x <= max_x; x++)
			{
				const int32_t num = dy * x - dx * y + x2y1 - y2x1;
				if (width * width * den2 < 4 * num * num)
					continue;
				if (!info.gc.is_clipped(x, y))
					info.out_data_u32[y * info.out_w + x] = info.gc.foreground;
			}
		}
	}

	info.min_x = BAN::Math::min(info.min_x, min_x);
	info.min_y = BAN::Math::min(info.min_y, min_y);
	info.max_x = BAN::Math::max(info.max_x, max_x);
	info.max_y = BAN::Math::max(info.max_y, max_y);
}

BAN::ErrorOr<void> poly_line(Client& client_info, BAN::ConstByteSpan packet)
{
	auto request = decode<xPolyLineReq>(packet).value();

	dprintln("PolyLine");
	dprintln("  drawable: {}", request.drawable);
	dprintln("  gc:       {}", request.gc);

	if (packet.size() < sizeof(xPoint))
		return {};

	auto [out_data_u32, out_w, out_h, _] = TRY(get_drawable_info(client_info, request.drawable, X_PolyFillRectangle));

	const auto& gc = TRY_REF(get_gc(client_info, request.gc, X_PolyFillRectangle));

	DrawSegmentInfo info {
		.out_data_u32 = out_data_u32,
		.out_w = out_w,
		.out_h = out_h,
		.min_x = INT32_MAX,
		.max_x = INT32_MIN,
		.min_y = INT32_MAX,
		.max_y = INT32_MIN,
		.gc = gc,
	};

	auto prev = decode<xPoint>(packet).value();
	while (packet.size() >= sizeof(xPoint))
	{
		auto next = decode<xPoint>(packet).value();
		if (request.coordMode == CoordModePrevious)
		{
			next.x += prev.x;
			next.y += prev.y;
		}

		info.x1 = prev.x;
		info.y1 = prev.y;
		info.x2 = next.x;
		info.y2 = next.y;

		draw_segment(info);

		prev = next;
	}

	// TODO: should we invalidate bounding box or once per line?
	if (g_objects[request.drawable]->type == Object::Type::Window)
		invalidate_window(request.drawable, info.min_x, info.min_y, info.max_x - info.min_x + 1, info.max_y - info.min_y + 1);

	return {};
}

BAN::ErrorOr<void> poly_segment(Client& client_info, BAN::ConstByteSpan packet)
{
	auto request = decode<xPolySegmentReq>(packet).value();

	dprintln("PolySegment");
	dprintln("  drawable: {}", request.drawable);
	dprintln("  gc:       {}", request.gc);

	auto [out_data_u32, out_w, out_h, _] = TRY(get_drawable_info(client_info, request.drawable, X_PolyFillRectangle));

	const auto& gc = TRY_REF(get_gc(client_info, request.gc, X_PolyFillRectangle));

	DrawSegmentInfo info {
		.out_data_u32 = out_data_u32,
		.out_w = out_w,
		.out_h = out_h,
		.min_x = INT32_MAX,
		.max_x = INT32_MIN,
		.min_y = INT32_MAX,
		.max_y = INT32_MIN,
		.gc = gc,
	};

	while (packet.size() >= sizeof(xSegment))
	{
		auto segment = decode<xSegment>(packet).value();

		info.x1 = segment.x1;
		info.y1 = segment.y1;
		info.x2 = segment.x2;
		info.y2 = segment.y2;

		draw_segment(info);
	}

	// TODO: should we invalidate bounding box or once per line?
	if (g_objects[request.drawable]->type == Object::Type::Window)
		invalidate_window(request.drawable, info.min_x, info.min_y, info.max_x - info.min_x + 1, info.max_y - info.min_y + 1);

	return {};
}

BAN::ErrorOr<void> poly_fill_rectangle(Client& client_info, BAN::ConstByteSpan packet)
{
	auto request = decode<xPolyFillRectangleReq>(packet).value();

	dprintln("PolyFillRectangle");
	dprintln("  drawable: {}", request.drawable);
	dprintln("  gc:       {}", request.gc);

	auto [out_data_u32, out_w, out_h, _] = TRY(get_drawable_info(client_info, request.drawable, X_PolyFillRectangle));

	const auto& gc = TRY_REF(get_gc(client_info, request.gc, X_PolyFillRectangle));
	const auto foreground = gc.foreground;

	dprintln("  rects:");
	while (!packet.empty())
	{
		const auto rect = decode<xRectangle>(packet).value();

		dprintln("    rect");
		dprintln("      x, y: {},{}", rect.x, rect.y);
		dprintln("      w, h: {},{}", rect.width, rect.height);

		const int32_t min_x = BAN::Math::max<int32_t>(rect.x, 0);
		const int32_t min_y = BAN::Math::max<int32_t>(rect.y, 0);
		const int32_t max_x = BAN::Math::min<int32_t>(rect.x + rect.width, out_w);
		const int32_t max_y = BAN::Math::min<int32_t>(rect.y + rect.height, out_h);

		for (int32_t y = min_y; y < max_y; y++)
			for (int32_t x = min_x; x < max_x; x++)
				if (!gc.is_clipped(x, y))
					out_data_u32[y * out_w + x] = foreground;

		if (g_objects[request.drawable]->type == Object::Type::Window)
			invalidate_window(request.drawable, min_x, min_y, max_x - min_x, max_y - min_y);
	}

	return {};
}

BAN::ErrorOr<void> poly_fill_arc(Client& client_info, BAN::ConstByteSpan packet)
{
	auto request = decode<xPolyFillArcReq>(packet).value();

	dprintln("PolyFillArc");
	dprintln("  drawable: {}", request.drawable);
	dprintln("  gc:       {}", request.gc);

	auto [out_data_32, out_w, out_h, _] = TRY(get_drawable_info(client_info, request.drawable, X_PolyFillArc));

	auto& gc = TRY_REF(get_gc(client_info, request.gc, X_PolyFillArc));
	const auto foreground = gc.foreground;

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
		
		const int32_t max_x = BAN::Math::min<int32_t>(out_w, arc.x + arc.width);
		const int32_t max_y = BAN::Math::min<int32_t>(out_h, arc.y + arc.height);

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

				if (!gc.is_clipped(x, y))
					out_data_32[y * out_w + x] = foreground;
			}
		}

		if (g_objects[request.drawable]->type == Object::Type::Window)
			invalidate_window(request.drawable, min_x, min_y, max_x - min_x, max_y - min_y);
	}

	return {};
}

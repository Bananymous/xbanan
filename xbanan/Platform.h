#pragma once

#include "Types.h"

#include <BAN/UniqPtr.h>

struct PlatformWindow
{
	virtual ~PlatformWindow() = default;
};

struct PlatformCursor
{
	virtual ~PlatformCursor() = default;
};

enum class WindowType
{
	Normal,
	Utility,
	Popup,
};

enum class SystemCursorType
{
	Pointer,
	Text,
	Wait,
	Hand,
	Help,
	Move,
	Forbidden,
	ResizeN,
	ResizeE,
	ResizeS,
	ResizeW,
	ResizeNW,
	ResizeNE,
	ResizeSW,
	ResizeSE,
	ResizeVertical,
	ResizeHorizontal,
};

// initialize, poll_events, create_window and invalidate are required
struct PlatformOps
{
	/* Do platform initialization */
	bool (*initialize)(uint32_t* width, uint32_t* height);
	/* Handle pending events */
	void (*poll_events)(void*);
	/* Create a window with given size */
	BAN::ErrorOr<BAN::UniqPtr<PlatformWindow>> (*create_window)(PlatformWindow* parent, WindowType, WINDOW wid, int32_t x, int32_t y, uint32_t width, uint32_t height);
	/* Invaldate part of a window */
	void (*invalidate)(PlatformWindow*, const uint32_t* pixels, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
	/* Request resize of a window, can be async */
	void (*request_resize)(PlatformWindow*, uint32_t width, uint32_t height);
	/* Request window repositioning */
	void (*request_reposition)(PlatformWindow*, int32_t x, int32_t y);
	/* Request new fullscreen state, can be async */
	void (*request_fullscreen)(PlatformWindow*, bool fullscreen);
	/* Create a system cursor */
	BAN::ErrorOr<BAN::UniqPtr<PlatformCursor>> (*create_system_cursor)(SystemCursorType);
	/* Create cursor from custom bitmap */
	BAN::ErrorOr<BAN::UniqPtr<PlatformCursor>> (*create_bitmap_cursor)(const uint32_t* pixels, uint32_t width, uint32_t height, int32_t origin_x, int32_t origin_y);
	/* Set custom cursor */
	void (*set_cursor)(PlatformWindow*, PlatformCursor*);
};
extern PlatformOps g_platform_ops;

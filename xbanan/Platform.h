#pragma once

#include "Types.h"

#include <BAN/UniqPtr.h>

struct PlatformWindow
{
	virtual ~PlatformWindow() = default;
};

// initialize, poll_events, create_window and invalidate are required
struct PlatformOps
{
	/* Do platform initialization */
	bool (*initialize)(uint32_t* width, uint32_t* height);
	/* Handle pending events */
	void (*poll_events)(void*);
	/* Create a window with given size */
	BAN::ErrorOr<BAN::UniqPtr<PlatformWindow>> (*create_window)(WINDOW wid, uint32_t width, uint32_t height);
	/* Invaldate part of a window */
	void (*invalidate)(PlatformWindow*, const uint32_t* pixels, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
	/* Request resize of a window, can be async */
	void (*request_resize)(PlatformWindow*, uint32_t width, uint32_t height);
	/* Request new fullscreen state, can be async */
	void (*request_fullscreen)(PlatformWindow*, bool fullscreen);
	/* Set custom cursor */
	void (*set_cursor)(PlatformWindow*, const uint32_t* pixels, uint32_t width, uint32_t height, int32_t origin_x, int32_t origin_y);
};
extern PlatformOps g_platform_ops;

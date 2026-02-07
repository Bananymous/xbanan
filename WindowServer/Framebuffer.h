#pragma once

#include "Utils.h"

struct Framebuffer
{
	uint32_t* pixels;
	int32_t width;
	int32_t height;
	uint8_t bpp;

	Rectangle area() const { return { 0, 0, width, height }; }
};

Framebuffer open_framebuffer();

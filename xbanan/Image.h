#pragma once

#include <stdint.h>
#include <stddef.h>

struct PutImageInfo
{
	void* out_data;
	int32_t out_x;
	int32_t out_y;
	uint32_t out_w;
	uint32_t out_h;
	uint8_t out_depth;

	const void* in_data;
	int32_t in_x;
	int32_t in_y;
	uint32_t in_w;
	uint32_t in_h;
	uint8_t in_depth;

	uint32_t w;
	uint32_t h;

	uint32_t left_pad;
	uint8_t format;
};

struct GetImageInfo
{
	void* out_data;

	const void* in_data;
	int32_t in_x;
	int32_t in_y;
	uint32_t in_w;
	uint32_t in_h;

	// out matches these dimensions
	uint32_t w;
	uint32_t h;

	uint8_t depth;
	uint8_t format;
};

uint32_t image_dwords(uint32_t width, uint32_t height, uint8_t depth);
void put_image(const PutImageInfo& info);
void get_image(const GetImageInfo& info);

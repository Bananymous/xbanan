#include "Framebuffer.h"

#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_video.h>
#include <cstdint>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include <SDL2/SDL.h>

static constexpr size_t window_width { 1280 };
static constexpr size_t window_height { 800 };
static constexpr size_t window_bpp { 32 };

SDL_Renderer* g_renderer { nullptr };
SDL_Texture* g_texture { nullptr };
SDL_Window* g_window { nullptr };

Framebuffer open_framebuffer()
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == -1)
	{
		fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}

	g_window = SDL_CreateWindow("banan gui", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, SDL_WINDOW_SHOWN);
	if (g_window == nullptr)
	{
		fprintf(stderr, "Could not create SDL window: %s\n", SDL_GetError());
		exit(1);
	}

	g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
	if (g_renderer == nullptr)
	{
		fprintf(stderr, "Could not get SDL renderer: %s\n", SDL_GetError());
		exit(1);
	}

	g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, window_width, window_height);
	if (g_texture == nullptr)
	{
		fprintf(stderr, "Could not get SDL texture: %s\n", SDL_GetError());
		exit(1);
	}

	uint32_t* pixels = new uint32_t[window_width * window_height];
	memset(pixels, 0, window_width * window_height * 4);

	Framebuffer framebuffer;
	framebuffer.pixels = pixels;
	framebuffer.width = window_width;
	framebuffer.height = window_height;
	framebuffer.bpp = window_bpp;
	return framebuffer;
}

#pragma once

#include <BAN/Errors.h>

#include <stddef.h>
#include <stdint.h>

BAN::ErrorOr<void> initialize_keymap();

constexpr size_t g_keymap_min_keycode = 8;
constexpr size_t g_keymap_max_keycode = 255;
constexpr size_t g_keymap_layers = 4;
extern uint32_t g_keymap[0x100][g_keymap_layers];

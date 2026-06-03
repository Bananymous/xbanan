#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr size_t g_keymap_min_keycode = 8;
constexpr size_t g_keymap_max_keycode = 255;
constexpr size_t g_keymap_size = g_keymap_max_keycode - g_keymap_min_keycode + 1;

constexpr size_t g_keymap_layers = 4;
extern uint32_t g_keymap[g_keymap_size][g_keymap_layers];

// shift, capslock, control, alt, numlock, level5 shift, super, altgr with 2 keycodes per modifier
constexpr size_t g_modifier_layers = 2;
extern uint8_t g_modifier_map[8][g_modifier_layers];

extern uint8_t g_pressed_keys[32];

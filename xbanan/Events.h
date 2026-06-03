#pragma once

#include "Types.h"

void register_event_fd(int fd, void* data);
void unregister_event_fd(int fd);

void on_window_close_event(WINDOW wid);
void on_window_resize_event(WINDOW wid, uint32_t width, uint32_t height);
void on_window_move_event(WINDOW wid, int32_t x, int32_t y);
void on_window_focus_event(WINDOW wid, bool focused);
void on_window_fullscreen_event(WINDOW wid, bool is_fullscreen);
void on_window_leave_event(WINDOW wid);

void on_mouse_move_event(WINDOW wid, int32_t x, int32_t y);
void on_mouse_button_event(WINDOW wid, uint8_t xbutton, bool pressed);
void on_key_event(WINDOW wid, uint8_t scancode, uint8_t xmod, bool pressed);

void on_keymap_changed();

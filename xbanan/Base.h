#pragma once

#include "Definitions.h"

BAN::ErrorOr<void> setup_client_conneciton(Client& client_info, const xConnClientPrefix& client_prefix);
BAN::ErrorOr<void> handle_packet(Client& client_info, BAN::ConstByteSpan packet);

void invalidate_window(WINDOW wid, int32_t x, int32_t y, int32_t w, int32_t h);
void send_exposure_recursive(WINDOW wid);

BAN::ErrorOr<void> destroy_window(Client& client_info, WINDOW wid);

WINDOW find_child_window(WINDOW wid, int32_t& x, int32_t& y);

static constexpr uint32_t COLOR_INVISIBLE = 0x69000000;

extern CARD16 g_keymask;
extern CARD16 g_butmask;
extern WINDOW g_focus_window;

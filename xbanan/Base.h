#pragma once

#include "Definitions.h"

BAN::ErrorOr<void> setup_client_conneciton(Client& client_info, const xConnClientPrefix& client_prefix);
BAN::ErrorOr<void> handle_packet(Client& client_info, BAN::ConstByteSpan packet);

void invalidate_window(WINDOW wid, int32_t x, int32_t y, int32_t w, int32_t h);

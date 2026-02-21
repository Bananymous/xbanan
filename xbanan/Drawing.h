#pragma once

#include "Definitions.h"

BAN::ErrorOr<void> poly_line(Client& client_info, BAN::ConstByteSpan packet);
BAN::ErrorOr<void> poly_segment(Client& client_info, BAN::ConstByteSpan packet);
BAN::ErrorOr<void> poly_fill_rectangle(Client& client_info, BAN::ConstByteSpan packet);
BAN::ErrorOr<void> poly_fill_arc(Client& client_info, BAN::ConstByteSpan packet);

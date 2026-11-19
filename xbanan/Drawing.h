#pragma once

#include "Definitions.h"

BAN::ErrorOr<void> poly_point(Client& client_info, BAN::ConstByteSpan packet);
BAN::ErrorOr<void> poly_line(Client& client_info, BAN::ConstByteSpan packet);
BAN::ErrorOr<void> poly_segment(Client& client_info, BAN::ConstByteSpan packet);
BAN::ErrorOr<void> poly_rectangle(Client& client_info, BAN::ConstByteSpan packet);
BAN::ErrorOr<void> poly_arc(Client& client_info, BAN::ConstByteSpan packet);
BAN::ErrorOr<void> fill_poly(Client& client_info, BAN::ConstByteSpan packet);
BAN::ErrorOr<void> poly_fill_rectangle(Client& client_info, BAN::ConstByteSpan packet);
BAN::ErrorOr<void> poly_fill_arc(Client& client_info, BAN::ConstByteSpan packet);

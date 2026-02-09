#pragma once

#include "Definitions.h"

BAN::ErrorOr<void> extension_randr(Client& client_info, BAN::ConstByteSpan packet);

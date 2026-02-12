#pragma once

#include "Definitions.h"

#include <BAN/Array.h>

struct Extension
{
	BAN::StringView name;
	uint8_t major_opcode;
	uint8_t event_base;
	uint8_t error_base;
	BAN::ErrorOr<void> (*handler)(Client&, BAN::ConstByteSpan);
};

void install_extension(BAN::StringView name, uint8_t event_count, uint8_t error_count, BAN::ErrorOr<void> (*handler)(Client&, BAN::ConstByteSpan));

extern BAN::Array<Extension, 128> g_extensions;

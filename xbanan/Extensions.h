#pragma once

#include "Definitions.h"

#include <BAN/Array.h>

struct Extension
{
	BAN::StringView name;
	uint8_t major_opcode;
	uint8_t opcode_count;
	BAN::ErrorOr<void> (*handler)(Client&, BAN::ConstByteSpan);
};

void install_extension(BAN::StringView name, BAN::ErrorOr<void> (*handler)(Client&, BAN::ConstByteSpan));

extern BAN::Array<Extension, 128> g_extensions;

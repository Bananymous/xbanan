#include "Extensions.h"

static uint8_t g_extension_opcode { 128 };
BAN::Array<Extension, 128> g_extensions;

void install_extension(BAN::StringView name, BAN::ErrorOr<void> (*handler)(Client&, BAN::ConstByteSpan))
{
	g_extensions[g_extension_opcode - 128] = {
		.name = name,
		.major_opcode = g_extension_opcode,
		.handler = handler,
	};
	g_extension_opcode++;
}

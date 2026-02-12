#include "Extensions.h"

#include <X11/X.h>

static uint8_t g_extension_opcode { 128 };
static uint8_t g_extension_event_base { LASTEvent };
static uint8_t g_extension_error_base { FirstExtensionError };

BAN::Array<Extension, 128> g_extensions;

void install_extension(BAN::StringView name, uint8_t event_count, uint8_t error_count, BAN::ErrorOr<void> (*handler)(Client&, BAN::ConstByteSpan))
{
	g_extensions[g_extension_opcode - 128] = {
		.name = name,
		.major_opcode = g_extension_opcode,
		.event_base = g_extension_event_base,
		.error_base = g_extension_error_base,
		.handler = handler,
	};
	g_extension_opcode++;
	g_extension_event_base += event_count;
	g_extension_error_base += error_count;
}

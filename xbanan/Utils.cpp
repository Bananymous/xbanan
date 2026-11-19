#include "Definitions.h"
#include "Poller.h"
#include "Utils.h"

BAN::ErrorOr<void> encode_bytes(Client& client, const void* data, size_t size)
{
	const size_t old_size = client.output_buffer.size();
	TRY(client.output_buffer.resize(old_size + size));
	memcpy(client.output_buffer.data() + old_size, data, size);
	if (old_size == 0)
		poller_modify(client.fd, POLLMODE_OUT, 0);
	return {};
}

#include "Extensions.h"
#include "Utils.h"

#include <X11/X.h>
#include <X11/extensions/bigreqsproto.h>

BAN::ErrorOr<void> extension_bigrequests(Client& client_info, BAN::ConstByteSpan packet)
{
	switch (packet[1])
	{
		case X_BigReqEnable:
		{
			xGenericReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.data00 = (16 << 20) / 4, // 16 MiB
			};
			TRY(encode(client_info.output_buffer, reply));

			client_info.has_bigrequests = true;

			dprintln("client enabled big requests");

			break;
		}
		default:
			dwarnln("invalid BIG-REQUESTS minor opcode {}", packet[1]);
			return BAN::Error::from_errno(EINVAL);
	}

	return {};
}

static struct BigRegInstaller
{
	BigRegInstaller()
	{
		install_extension(XBigReqExtensionName, XBigReqNumberEvents, XBigReqNumberErrors, extension_bigrequests);
	}
} installer;

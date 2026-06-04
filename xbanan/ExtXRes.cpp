#include "Extensions.h"
#include "Utils.h"

#include <X11/X.h>
#include <X11/extensions/XResproto.h>

template<typename F>
static BAN::ErrorOr<void> for_each_client(uint32_t target_spec, const F& callback)
{
	for (auto [fd, thingy] : g_pollables)
	{
		if (thingy.type != Pollable::Type::Client)
			continue;

		Client& client_info = thingy.value.get<Client>();
		if (target_spec && (target_spec >> 20) != client_info.fd)
			continue;

		TRY(callback(client_info, target_spec ? target_spec : (client_info.fd << 20)));
	}
	return {};
}

BAN::ErrorOr<void> extension_xres(Client& client_info, BAN::ConstByteSpan packet)
{
	const uint8_t major_opcode = packet[0];
	const uint8_t minor_opcode = packet[1];

	switch (minor_opcode)
	{
		case X_XResQueryVersion:
		{
			auto request = decode<xXResQueryVersionReq>(packet).value();

			dprintln("XResQueryVersion");
			dprintln("  clientMajor: {}", request.client_major);
			dprintln("  clientMinor: {}", request.client_minor);
   
			xXResQueryVersionReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.server_major = 1,
				.server_minor = 2,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_XResQueryClientIds:
		{
			auto request = decode<xXResQueryClientIdsReq>(packet).value();

			dprintln("XResQueryClientIds");
			dprintln("  numSpecs: {}", request.numSpecs);

			uint32_t num_ids { 0 };
			BAN::Vector<uint8_t> query_result;

			for (size_t i = 0; i < request.numSpecs; i++)
			{
				auto spec = decode<xXResClientIdSpec>(packet).value();

				TRY(for_each_client(spec.client, [&](Client& client_info, uint32_t client_spec) -> BAN::ErrorOr<void> {
					if (spec.mask == None || (spec.mask & X_XResClientXIDMask))
					{
						xXResClientIdValue value {
							.spec = {
								.client = client_spec,
								.mask = X_XResClientXIDMask,
							},
							.length = 0,
						};
						TRY(encode(query_result, value));
						num_ids++;
					}
					if ((spec.mask == None || (spec.mask & X_XResLocalClientPIDMask)) && client_info.pid.has_value())
					{
						xXResClientIdValue value {
							.spec = {
								.client = client_spec,
								.mask = X_XResLocalClientPIDMask,
							},
							.length = 4,
						};
						TRY(encode(query_result, value));
						TRY(encode(query_result, client_info.pid.value()));
						num_ids++;
					}
					return {};
				}));	
			}

			xXResQueryClientIdsReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = static_cast<uint32_t>(query_result.size() / 4),
				.numIds = num_ids,
			};
			TRY(encode(client_info.output_buffer, reply));
			TRY(encode(client_info.output_buffer, query_result));

			break;
		}
		default:
			dwarnln("unsupported opcode XRes minor opcode {}", minor_opcode);
			break;
	}

	return {};
}

static struct XResInstaller
{
	XResInstaller()
	{
		install_extension(XRES_NAME, 6, 0, extension_xres);
	}
} installer;

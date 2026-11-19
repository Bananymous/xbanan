#include "Extensions.h"
#include "Utils.h"

#include <X11/X.h>
#include <X11/extensions/XResproto.h>

template<typename F>
static BAN::ErrorOr<void> for_each_client(uint32_t target_spec, const F& callback)
{
	if (const int fd = target_spec >> 20)
	{
		if (auto it = g_clients.find(fd); it != g_clients.end())
			TRY(callback(*it->value, fd << 20));
	}
	else
	{
		for (const auto& [fd, client] : g_clients)
			TRY(callback(*client, fd << 20));
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
			TRY(encode(client_info, reply));

			break;
		}
		case X_XResQueryClientIds:
		{
			auto request = decode<xXResQueryClientIdsReq>(packet).value();

			dprintln("XResQueryClientIds");
			dprintln("  numSpecs: {}", request.numSpecs);

			uint32_t num_ids { 0 };
			BAN::Vector<uint8_t> query_result;

			const auto result_append = [&query_result]<typename T>(const T& value) -> BAN::ErrorOr<void> {
				static_assert(BAN::is_pod_v<T>);
				const size_t old_size = query_result.size();
				TRY(query_result.resize(old_size + sizeof(T)));
				memcpy(query_result.data() + old_size, &value, sizeof(T));
				return {};
			};

			for (size_t i = 0; i < request.numSpecs; i++)
			{
				auto spec = decode<xXResClientIdSpec>(packet).value();

				TRY(for_each_client(spec.client, [&](const Client& client_info, uint32_t client_spec) -> BAN::ErrorOr<void> {
					if (spec.mask == None || (spec.mask & X_XResClientXIDMask))
					{
						xXResClientIdValue value {
							.spec = {
								.client = client_spec,
								.mask = X_XResClientXIDMask,
							},
							.length = 0,
						};
						TRY(result_append(value));
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
						TRY(result_append(value));
						TRY(result_append(client_info.pid.value()));
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
			TRY(encode(client_info, reply));
			TRY(encode(client_info, query_result));

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

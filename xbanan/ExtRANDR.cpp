#include "Extensions.h"
#include "Utils.h"

#include <X11/X.h>
#include <X11/extensions/randrproto.h>

#include <time.h>

struct RANDRDisplay
{
	CARD32 crtc_id;
	CARD32 output_id;
	CARD32 mode_id;
	ATOM name_atom;
	BAN::String output_str;
	BAN::String mode_str;
	const DisplayInfo& info;
};

static BAN::Vector<RANDRDisplay> s_randr_displays;
static CARD32 s_timestamp { 0 };

static void initialize_displays()
{
	for (size_t i = 0; i < g_displays.size(); i++)
	{
		auto name = MUST(BAN::String::formatted("B-OUT-{}", i));

		const auto name_atom = g_atom_value++;
		MUST(g_atoms_name_to_id.insert(name, name_atom));
		MUST(g_atoms_id_to_name.insert(name_atom, name));

		MUST(s_randr_displays.push_back({
			.crtc_id    = g_next_global_id++,
			.output_id  = g_next_global_id++,
			.mode_id    = g_next_global_id++,
			.name_atom  = name_atom,
			.output_str = BAN::move(name),
			.mode_str   = MUST(BAN::String::formatted("{}x{}", g_displays[i].w, g_displays[i].h)),
			.info       = g_displays[i],
		}));
	}

	s_timestamp = time(nullptr);
}

static const RANDRDisplay& find_display_by_output(CARD32 output)
{
	for (const auto& display : s_randr_displays)
		if (display.output_id == output)
			return display;
	ASSERT_NOT_REACHED();
}

static const RANDRDisplay& find_display_by_crtc(CARD32 crtc)
{
	for (const auto& display : s_randr_displays)
		if (display.crtc_id == crtc)
			return display;
	ASSERT_NOT_REACHED();
}

static BAN::ErrorOr<void> extension_randr(Client& client_info, BAN::ConstByteSpan packet)
{
	if (s_randr_displays.empty())
		initialize_displays();

	static xRenderTransform transform {
		1 << 16, 0,       0,
		0,       1 << 16, 0,
		0,       0,       1 << 16
	};

	switch (packet[1])
	{
		case X_RRQueryVersion:
		{
			auto request = decode<xRRQueryVersionReq>(packet).value();

			dprintln("RRQueryVersion");
			dprintln("  majorVersion: {}", request.majorVersion);
			dprintln("  minorVersion: {}", request.minorVersion);

			xRRQueryVersionReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.majorVersion = 1,
				.minorVersion = 6,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_RRSelectInput:
		{
			auto request = decode<xRRSelectInputReq>(packet).value();

			dprintln("RRSelectInput");
			dprintln("  window: {}", request.window);
			dprintln("  enable: {}", request.enable);

			break;
		}
		case X_RRGetScreenInfo:
		{
			auto request = decode<xRRGetScreenInfoReq>(packet).value();

			dprintln("RRGetScreenInfo");
			dprintln("  window: {}", request.window);

			xRRGetScreenInfoReply reply {
				.type = X_Reply,
				.setOfRotations = RR_Rotate_0,
				.sequenceNumber = client_info.sequence,
				.length = 3,
				.root = g_root.windowId,
				.timestamp = s_timestamp,
				.configTimestamp = s_timestamp,
				.nSizes = 1,
				.sizeID = 0,
				.rotation = RR_Rotate_0,
				.rate = 60,
				.nrateEnts = 1,
			};
			TRY(encode(client_info.output_buffer, reply));

			xScreenSizes screen_size {
				.widthInPixels = g_root.pixWidth,
				.heightInPixels = g_root.pixHeight,
				.widthInMillimeters = g_root.mmWidth,
				.heightInMillimeters = g_root.mmHeight,
			};
			TRY(encode(client_info.output_buffer, screen_size));

			TRY(encode<CARD16>(client_info.output_buffer, 60));
			TRY(encode<CARD16>(client_info.output_buffer, 0));

			break;
		}
		case X_RRGetScreenSizeRange:
		{
			auto request = decode<xRRGetScreenSizeRangeReq>(packet).value();

			dprintln("RRGetScreenSizeRange");
			dprintln("  window: {}", request.window);

			xRRGetScreenSizeRangeReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.minWidth = g_root.pixWidth,
				.minHeight = g_root.pixHeight,
				.maxWidth = g_root.pixWidth,
				.maxHeight = g_root.pixHeight,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_RRGetScreenResources:
		case X_RRGetScreenResourcesCurrent:
		{
			const bool current = packet[1] == X_RRGetScreenResourcesCurrent;

			auto request = decode<xRRGetScreenResourcesReq>(packet).value();

			dprintln("RRGetScreenResources{}", current ? "Current" : "");
			dprintln("  window: {}", request.window);

			size_t mode_name_bytes = 0;
			for (const auto& display : s_randr_displays)
				mode_name_bytes += display.mode_str.size();

			xRRGetScreenResourcesReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = static_cast<CARD32>(s_randr_displays.size() * (1 + 1 + 8) + (mode_name_bytes + 3) / 4),
				.timestamp = s_timestamp,
				.configTimestamp = s_timestamp,
				.nCrtcs = static_cast<CARD16>(s_randr_displays.size()),
				.nOutputs = static_cast<CARD16>(s_randr_displays.size()),
				.nModes = static_cast<CARD16>(s_randr_displays.size()),
				.nbytesNames = static_cast<CARD16>(mode_name_bytes),
			};
			TRY(encode(client_info.output_buffer, reply));

			for (const auto& display : s_randr_displays)
				TRY(encode(client_info.output_buffer, display.crtc_id));

			for (const auto& display : s_randr_displays)
				TRY(encode(client_info.output_buffer, display.output_id));

			for (const auto& display : s_randr_displays)
			{
				const CARD16 hsync_start = display.info.w;
				const CARD16 hsync_end   = display.info.w + 1;
				const CARD16 htotal = display.info.w + 1;

				const CARD16 vsync_start = display.info.h;
				const CARD16 vsync_end   = display.info.h + 1;
				const CARD16 vtotal = display.info.h + 1;

				const CARD32 clock = htotal * vtotal * 60;

				xRRModeInfo mode_info {
					.id = display.mode_id,
					.width = static_cast<CARD16>(display.info.w),
					.height = static_cast<CARD16>(display.info.h),
					.dotClock = clock,
					.hSyncStart = hsync_start,
					.hSyncEnd = hsync_end,
					.hTotal = htotal,
					.hSkew = 0,
					.vSyncStart = vsync_start,
					.vSyncEnd = vsync_end,
					.vTotal = vtotal,
					.nameLength = static_cast<CARD16>(display.mode_str.size()),
					.modeFlags = RR_HSyncPositive | RR_VSyncPositive,
				};
				TRY(encode(client_info.output_buffer, mode_info));
			}

			for (const auto& display : s_randr_displays)
				TRY(encode(client_info.output_buffer, display.mode_str));
			while (client_info.output_buffer.size() % 4)
				TRY(encode<BYTE>(client_info.output_buffer, 0));

			break;
		}
		case X_RRGetOutputInfo:
		{
			auto request = decode<xRRGetOutputInfoReq>(packet).value();

			dprintln("RRGetOutputInfo");
			dprintln("  output:          {}", request.output);
			dprintln("  configTimestamp: {}", request.configTimestamp);

			const auto& display = find_display_by_output(request.output);

			xRRGetOutputInfoReply reply {
				.type = X_Reply,
				.status = Success,
				.sequenceNumber = client_info.sequence,
				.length = static_cast<CARD32>(1 + 1 + 1 + (display.output_str.size() + 3) / 4),
				.timestamp = s_timestamp,
				.crtc = display.crtc_id,
				.mmWidth  = display.info.w * 254 / 960, // 96 DPI
				.mmHeight = display.info.h * 254 / 960, // 96 DPI
				.connection = RR_Connected,
				.subpixelOrder = SubPixelUnknown,
				.nCrtcs = 1,
				.nModes = 1,
				.nPreferred = 1,
				.nClones = 0,
				.nameLength = static_cast<CARD16>(display.output_str.size()),
			};
			TRY(encode(client_info.output_buffer, reply));
			TRY(encode(client_info.output_buffer, display.crtc_id));
			TRY(encode(client_info.output_buffer, display.mode_id));
			TRY(encode(client_info.output_buffer, display.output_str));
			while (client_info.output_buffer.size() % 4)
				TRY(encode<BYTE>(client_info.output_buffer, 0));

			break;
		}
		case X_RRListOutputProperties:
		{
			auto request = decode<xRRListOutputPropertiesReq>(packet).value();

			dprintln("RRListOutputProperties");
			dprintln("  output: {}", request.output);

			xRRListOutputPropertiesReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.nAtoms = 0,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_RRGetOutputProperty:
		{
			auto request = decode<xRRGetOutputPropertyReq>(packet).value();

			dprintln("RRGetOutputProperty");
			dprintln("  output:     {}", request.output);
			dprintln("  property:   {}", request.property);
			dprintln("  type:       {}", request.type);
			dprintln("  longOffset: {}", request.longOffset);
			dprintln("  longLength: {}", request.longLength);
			dprintln("  _delete:    {}", request._delete);
			dprintln("  pending:    {}", request.pending);

			xRRGetOutputPropertyReply reply {
				.type = X_Reply,
				.format = 0,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.propertyType = None,
				.bytesAfter = 0,
				.nItems = 0,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_RRGetCrtcInfo:
		{
			auto request = decode<xRRGetCrtcInfoReq>(packet).value();

			dprintln("RRGetCrtcInfo");
			dprintln("  crtc:            {}", request.crtc);
			dprintln("  configTimestamp: {}", request.configTimestamp);

			const auto& display = find_display_by_crtc(request.crtc);

			xRRGetCrtcInfoReply reply {
				.type = X_Reply,
				.status = Success,
				.sequenceNumber = client_info.sequence,
				.length = 2,
				.timestamp = s_timestamp,
				.x = static_cast<INT16>(display.info.x),
				.y = static_cast<INT16>(display.info.y),
				.width  = static_cast<CARD16>(display.info.w),
				.height = static_cast<CARD16>(display.info.h),
				.mode = display.mode_id,
				.rotation = RR_Rotate_0,
				.rotations = RR_Rotate_0,
				.nOutput = 1,
				.nPossibleOutput = 1,
			};
			TRY(encode(client_info.output_buffer, reply));
			TRY(encode(client_info.output_buffer, display.output_id));
			TRY(encode(client_info.output_buffer, display.output_id));

			break;
		}
		case X_RRSetCrtcConfig:
		{
			auto request = decode<xRRSetCrtcConfigReq>(packet).value();

			dprintln("RRSetCrtcConfig");
			dprintln("  crtc:            {}", request.crtc);
			dprintln("  timestamp:       {}", request.timestamp);
			dprintln("  configTimestamp: {}", request.configTimestamp);
			dprintln("  x:               {}", request.x);
			dprintln("  y:               {}", request.y);
			dprintln("  mode:            {}", request.mode);
			dprintln("  rotation:        {}", request.rotation);

			s_timestamp = time(nullptr);
			xRRSetCrtcConfigReply reply {
				.type = X_Reply,
				.status = Success,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.newTimestamp = s_timestamp,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_RRGetCrtcGammaSize:
		{
			auto request = decode<xRRGetCrtcGammaSizeReq>(packet).value();

			dwarnln("RRGetCrtcGammaSize");
			dwarnln("  crtc:            {}", request.crtc);

			xRRGetCrtcGammaSizeReply reply {
				.type = X_Reply,
				.status = Success,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.size = 256,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_RRGetCrtcGamma:
		{
			auto request = decode<xRRGetCrtcGammaReq>(packet).value();

			dprintln("RRGetCrtcGamma");
			dprintln("  crtc:            {}", request.crtc);

			xRRGetCrtcGammaReply reply {
				.type = X_Reply,
				.status = Success,
				.sequenceNumber = client_info.sequence,
				.length = (6 * 256) / 4,
				.size = 256,
			};
			TRY(encode(client_info.output_buffer, reply));
			for (size_t i = 0; i < 3; i++)
				for (size_t j = 0; j < 256; j++)
					TRY(encode<CARD16>(client_info.output_buffer, j * 65535 / 255));

			break;
		}
		case X_RRGetCrtcTransform:
		{
			auto request = decode<xRRGetCrtcTransformReq>(packet).value();

			dprintln("RRGetCrtcTransform");
			dprintln("  crtc:            {}", request.crtc);

			xRRGetCrtcTransformReply reply {
				.type = X_Reply,
				.status = Success,
				.sequenceNumber = client_info.sequence,
				.length = 16,
				.pendingTransform = transform,
				.hasTransforms = xFalse,
				.currentTransform = transform,
				.pendingNbytesFilter = 0,
				.pendingNparamsFilter = 0,
				.currentNbytesFilter = 0,
				.currentNparamsFilter = 0,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_RRGetPanning:
		{
			auto request = decode<xRRGetPanningReq>(packet).value();

			dprintln("RRGetPanning");
			dprintln("  crtc: {}", request.crtc);

			xRRGetPanningReply reply {
				.type = X_Reply,
				.status = Success,
				.sequenceNumber = client_info.sequence,
				.length = 1,
				.timestamp = s_timestamp,
				.left = 0,
				.top = 0,
				.width = 0,
				.height = 0,
				.track_left = 0,
				.track_top = 0,
				.track_width = 0,
				.track_height = 0,
				.border_left = 0,
				.border_top = 0,
				.border_right = 0,
				.border_bottom = 0,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_RRGetOutputPrimary:
		{
			auto request = decode<xRRGetOutputPrimaryReq>(packet).value();

			dprintln("RRGetOutputPrimary");
			dprintln("  window: {}", request.window);

			xRRGetOutputPrimaryReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.output = s_randr_displays.front().output_id,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_RRGetProviders:
		{
			auto request = decode<xRRGetProvidersReq>(packet).value();

			dprintln("RRGetProviders");
			dprintln("  window: {}", request.window);

			xRRGetProvidersReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.timestamp = s_timestamp,
				.nProviders = 0,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_RRGetMonitors:
		{
			auto request = decode<xRRGetMonitorsReq>(packet).value();

			dprintln("RRGetMonitors");
			dprintln("  window:     {}", request.window);
			dprintln("  get_active: {}", request.get_active);

			xRRGetMonitorsReply reply {
				.type = X_Reply,
				.status = Success,
				.sequenceNumber = client_info.sequence,
				.length = static_cast<CARD32>(6 * s_randr_displays.size() + s_randr_displays.size()),
				.timestamp = s_timestamp,
				.nmonitors = static_cast<CARD32>(s_randr_displays.size()),
				.noutputs = static_cast<CARD32>(s_randr_displays.size()),
			};
			TRY(encode(client_info.output_buffer, reply));

			for (const auto& display : s_randr_displays)
			{
				xRRMonitorInfo monitor {
					.name = display.name_atom,
					.primary = (&display == &s_randr_displays.front()),
					.automatic = xTrue,
					.noutput = 1,
					.x = static_cast<INT16>(display.info.x),
					.y = static_cast<INT16>(display.info.y),
					.width  = static_cast<CARD16>(display.info.w),
					.height = static_cast<CARD16>(display.info.h),
					.widthInMillimeters  = display.info.w * 254 / 96, // 96 DPI
					.heightInMillimeters = display.info.h * 254 / 96, // 96 DPI
				};
				TRY(encode(client_info.output_buffer, monitor));
				TRY(encode(client_info.output_buffer, display.output_id));
			}

			break;
		}
		default:
			dwarnln("unsupported randr minor opcode {}", packet[1]);
			break;
	}

	return {};
}

static struct RANDRInstaller
{
	RANDRInstaller()
	{
		install_extension(RANDR_NAME, RRNumberEvents, RRNumberErrors, extension_randr);
	}
} installer;

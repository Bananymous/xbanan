#include "Extensions.h"
#include "Utils.h"

#include <X11/X.h>
#include <X11/extensions/randrproto.h>

#include <time.h>

static BAN::ErrorOr<void> extension_randr(Client& client_info, BAN::ConstByteSpan packet)
{
	static CARD32 crtc_id   = 5;
	static CARD32 output_id = 6;
	static CARD32 mode_id   = 7;
	static CARD32 timestamp = time(nullptr);

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
    			.timestamp = timestamp,
    			.configTimestamp = timestamp,
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

			const auto mode_name = TRY(BAN::String::formatted("{}x{}", g_root.pixWidth, g_root.pixHeight));

			xRRGetScreenResourcesReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = static_cast<CARD32>(1 + 1 + 8 + (mode_name.size() + 3) / 4),
				.timestamp = timestamp,
				.configTimestamp = timestamp,
				.nCrtcs = 1,
				.nOutputs = 1,
				.nModes = 1,
				.nbytesNames = static_cast<CARD16>(mode_name.size()),
			};
			TRY(encode(client_info.output_buffer, reply));

			TRY(encode(client_info.output_buffer, crtc_id));

			TRY(encode(client_info.output_buffer, output_id));

			const CARD16 hsync_start = g_root.pixWidth;
			const CARD16 hsync_end   = g_root.pixWidth + 1;
			const CARD16 htotal = g_root.pixWidth + 1;

			const CARD16 vsync_start = g_root.pixHeight;
			const CARD16 vsync_end   = g_root.pixHeight + 1;
			const CARD16 vtotal = g_root.pixHeight + 1;

			const CARD32 clock = htotal * vtotal * 60;

			xRRModeInfo mode_info {
    			.id = mode_id,
    			.width = g_root.pixWidth,
    			.height = g_root.pixHeight,
    			.dotClock = clock,
    			.hSyncStart = hsync_start,
    			.hSyncEnd = hsync_end,
    			.hTotal = htotal,
    			.hSkew = 0,
    			.vSyncStart = vsync_start,
    			.vSyncEnd = vsync_end,
    			.vTotal = vtotal,
    			.nameLength = static_cast<CARD16>(mode_name.size()),
    			.modeFlags = RR_HSyncPositive | RR_VSyncPositive,
			};
			TRY(encode(client_info.output_buffer, mode_info));

			TRY(encode(client_info.output_buffer, mode_name));
			for (size_t i = 0; (mode_name.size() + i) % 4; i++)
				TRY(encode(client_info.output_buffer, '\0'));

			break;
		}
		case X_RRGetOutputInfo:
		{
			auto request = decode<xRRGetOutputInfoReq>(packet).value();

			dprintln("RRGetOutputInfo");
			dprintln("  output:          {}", request.output);
			dprintln("  configTimestamp: {}", request.configTimestamp);

			xRRGetOutputInfoReply reply {
    			.type = X_Reply,
    			.status = Success,
    			.sequenceNumber = client_info.sequence,
    			.length = 1 + 1 + 1 + 2,
    			.timestamp = timestamp,
    			.crtc = crtc_id,
    			.mmWidth = g_root.mmWidth,
    			.mmHeight = g_root.mmHeight,
    			.connection = RR_Connected,
    			.subpixelOrder = SubPixelUnknown,
    			.nCrtcs = 1,
    			.nModes = 1,
    			.nPreferred = 1,
    			.nClones = 0,
    			.nameLength = 5,
			};
			TRY(encode(client_info.output_buffer, reply));
			TRY(encode(client_info.output_buffer, crtc_id));
			TRY(encode(client_info.output_buffer, mode_id));
			TRY(encode(client_info.output_buffer, "B-OUT\0\0\0"_sv));

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

			xRRGetCrtcInfoReply reply {
				.type = X_Reply,
				.status = Success,
				.sequenceNumber = client_info.sequence,
				.length = 2,
				.timestamp = timestamp,
				.x = 0,
				.y = 0,
				.width = g_root.pixWidth,
				.height = g_root.pixHeight,
				.mode = mode_id,
				.rotation = RR_Rotate_0,
				.rotations = RR_Rotate_0,
				.nOutput = 1,
				.nPossibleOutput = 1,
			};
			TRY(encode(client_info.output_buffer, reply));
			TRY(encode(client_info.output_buffer, output_id));
			TRY(encode(client_info.output_buffer, output_id));

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

			timestamp = time(nullptr);
			xRRSetCrtcConfigReply reply {
				.type = X_Reply,
				.status = Success,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.newTimestamp = timestamp,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_RRGetCrtcGammaSize:
		{
			auto request = decode<xRRGetCrtcGammaSizeReq>(packet).value();

			dprintln("RRGetCrtcGammaSize");
			dprintln("  crtc:            {}", request.crtc);

			xRRGetCrtcGammaSizeReply reply {
    			.type = X_Reply,
    			.status = Success,
    			.sequenceNumber = client_info.sequence,
    			.length = 0,
				.size = 1,
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
    			.length = 2,
				.size = 1,
			};
			TRY(encode(client_info.output_buffer, reply));
			TRY(encode<CARD16>(client_info.output_buffer, 1));
			TRY(encode<CARD16>(client_info.output_buffer, 1));
			TRY(encode<CARD16>(client_info.output_buffer, 1));
			TRY(encode<CARD16>(client_info.output_buffer, 0));

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
    			.timestamp = timestamp,
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
				.output = output_id,
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
				.timestamp = timestamp,
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
    			.length = 6 + 1,
				.timestamp = timestamp,
				.nmonitors = 1,
				.noutputs = 1,
			};
			TRY(encode(client_info.output_buffer, reply));

			xRRMonitorInfo monitor {
				.name = None,
				.primary = xTrue,
				.automatic = xTrue,
				.noutput = 1,
				.x = 0,
				.y = 0,
				.width = g_root.pixWidth,
				.height = g_root.pixHeight,
				.widthInMillimeters = g_root.mmWidth,
				.heightInMillimeters = g_root.mmHeight,
			};
			TRY(encode(client_info.output_buffer, monitor));

			TRY(encode(client_info.output_buffer, output_id));

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

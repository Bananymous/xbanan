#include "Extensions.h"
#include "Utils.h"

#include <GL/glxproto.h>
#include <GL/glxtokens.h>

using BOOL32 = CARD32;

CARD32 g_fb_configs[2][24][2] {
	{
		{ GLX_FBCONFIG_ID,         1 },
		{ GLX_VISUAL_ID,           g_visual.visualID },
		{ GLX_BUFFER_SIZE,         32 },
		{ GLX_LEVEL,               0 },
		{ GLX_DOUBLEBUFFER,        xTrue },
		{ GLX_STEREO,              xFalse },
		{ GLX_RENDER_TYPE,         GLX_RGBA_BIT },
		{ GLX_DRAWABLE_TYPE,       GLX_WINDOW_BIT },
		{ GLX_X_RENDERABLE,        xTrue },
		{ GLX_X_VISUAL_TYPE,       GLX_TRUE_COLOR },
		{ GLX_CONFIG_CAVEAT,       GLX_NONE },
		{ GLX_TRANSPARENT_TYPE,    GLX_NONE },
		{ GLX_RED_SIZE,            8 },
		{ GLX_GREEN_SIZE,          8 },
		{ GLX_BLUE_SIZE,           8 },
		{ GLX_ALPHA_SIZE,          8 },
		{ GLX_DEPTH_SIZE,          24 },
		{ GLX_STENCIL_SIZE,        8 },
		{ GLX_ACCUM_RED_SIZE,      0 },
		{ GLX_ACCUM_GREEN_SIZE,    0 },
		{ GLX_ACCUM_BLUE_SIZE,     0 },
		{ GLX_ACCUM_ALPHA_SIZE,    0 },
		{ GLX_SAMPLE_BUFFERS_SGIS, 0 },
		{ GLX_SAMPLES_SGIS,        0 },
	},
	{
		{ GLX_FBCONFIG_ID,         2 },
		{ GLX_VISUAL_ID,           g_visual.visualID },
		{ GLX_BUFFER_SIZE,         32 },
		{ GLX_LEVEL,               0 },
		{ GLX_DOUBLEBUFFER,        xFalse },
		{ GLX_STEREO,              xFalse },
		{ GLX_RENDER_TYPE,         GLX_RGBA_BIT },
		{ GLX_DRAWABLE_TYPE,       GLX_WINDOW_BIT },
		{ GLX_X_RENDERABLE,        xTrue },
		{ GLX_X_VISUAL_TYPE,       GLX_TRUE_COLOR },
		{ GLX_CONFIG_CAVEAT,       GLX_NONE },
		{ GLX_TRANSPARENT_TYPE,    GLX_NONE },
		{ GLX_RED_SIZE,            8 },
		{ GLX_GREEN_SIZE,          8 },
		{ GLX_BLUE_SIZE,           8 },
		{ GLX_ALPHA_SIZE,          8 },
		{ GLX_DEPTH_SIZE,          24 },
		{ GLX_STENCIL_SIZE,        8 },
		{ GLX_ACCUM_RED_SIZE,      0 },
		{ GLX_ACCUM_GREEN_SIZE,    0 },
		{ GLX_ACCUM_BLUE_SIZE,     0 },
		{ GLX_ACCUM_ALPHA_SIZE,    0 },
		{ GLX_SAMPLE_BUFFERS_SGIS, 0 },
		{ GLX_SAMPLES_SGIS,        0 },
	},
};

struct MyGLXContext
{
	CARD32 fbconfig;
	BOOL is_direct;
};

static BYTE s_glx_event_base;
static BYTE s_glx_error_base;
static BYTE s_glx_major_opcode;

BAN::ErrorOr<void> extension_glx(Client& client_info, BAN::ConstByteSpan packet)
{
	const uint8_t major_opcode = packet[0];
	const uint8_t minor_opcode = packet[1];

	const auto get_glx_context =
		[&client_info, minor_opcode, major_opcode](CARD32 context) -> BAN::ErrorOr<MyGLXContext&>
		{
			auto it = g_objects.find(context);
			if (it != g_objects.end() && it->value->type == Object::Type::Extension)
			{
				auto& ext = it->value->object.get<Object::Extension>();
				if (ext.type_major == s_glx_major_opcode && ext.type_minor == GLXBadContext)
					return *static_cast<MyGLXContext*>(ext.c_private);
			}

			xError error {
				.type = X_Error,
				.errorCode = static_cast<BYTE>(s_glx_error_base + GLXBadContext),
				.sequenceNumber = client_info.sequence,
				.resourceID = context,
				.minorCode = minor_opcode,
				.majorCode = major_opcode,
			};
			TRY(encode(client_info.output_buffer, error));
			return BAN::Error::from_errno(ENOENT);
		};

	switch (minor_opcode)
	{
		case X_GLXCreateContext:
		{
			auto request = decode<xGLXCreateContextReq>(packet).value();

			dprintln("GLXCreateContext");
			dprintln("  context:   {}", request.context);
			dprintln("  visual:    {}", request.visual);
			dprintln("  screen:    {}", request.screen);
			dprintln("  shareList: {}", request.shareList);
			dprintln("  isDirect:  {}", request.isDirect);

			auto* object = new MyGLXContext({
				.fbconfig = 1,
				.is_direct = request.isDirect,
			});
			ASSERT(object);

			TRY(client_info.objects.insert(request.context));
			TRY(g_objects.insert(
				request.context,
				TRY(BAN::UniqPtr<Object>::create(Object {
					.type = Object::Type::Extension,
					.object = Object::Extension {
						.type_major = s_glx_major_opcode,
						.type_minor = GLXBadContext,
						.c_private = object,
						.destructor = [](Object::Extension& ext) { delete static_cast<MyGLXContext*>(ext.c_private); },
					},
				}))
			));

			break;
		}
		case X_GLXDestroyContext:
		{
			auto request = decode<xGLXDestroyContextReq>(packet).value();

			dprintln("GLXDestroyContext");
			dprintln("  context:   {}", request.context);

			delete &TRY_REF(get_glx_context(request.context));
			client_info.objects.remove(request.context);
			g_objects.remove(request.context);

			break;
		}
		case X_GLXIsDirect:
		{
			auto request = decode<xGLXIsDirectReq>(packet).value();

			dprintln("GLXIsDirect");
			dprintln("  context: {}", request.context);

			const auto& context = TRY_REF(get_glx_context(request.context));

			xGLXIsDirectReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.isDirect = context.is_direct,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_GLXQueryVersion:
		{
			auto request = decode<xGLXQueryVersionReq>(packet).value();

			dprintln("GLXQueryVersion");
			dprintln("  majorVersion: {}", request.majorVersion);
			dprintln("  minorVersion: {}", request.minorVersion);

			xGLXQueryVersionReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = 0,
				.majorVersion = 1,
				.minorVersion = 4,
			};
			TRY(encode(client_info.output_buffer, reply));

			break;
		}
		case X_GLXGetVisualConfigs:
		{
			auto request = decode<xGLXGetVisualConfigsReq>(packet).value();

			dprintln("GLXGetVisualConfigs");
			dprintln("  screen: {}", request.screen);

			xGLXGetVisualConfigsReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = 1 * 18,
				.numVisuals = 1,
				.numProps = 18,
			};
			TRY(encode(client_info.output_buffer, reply));

			TRY(encode<VISUALID>(client_info.output_buffer, g_visual.visualID));
			TRY(encode<CARD32>(client_info.output_buffer, g_visual.c_class));
			TRY(encode<BOOL32>(client_info.output_buffer, xTrue)); // rgba
			TRY(encode<CARD32>(client_info.output_buffer, 8)); // red size
			TRY(encode<CARD32>(client_info.output_buffer, 8)); // green size
			TRY(encode<CARD32>(client_info.output_buffer, 8)); // blue size
			TRY(encode<CARD32>(client_info.output_buffer, 8)); // alpha size
			TRY(encode<CARD32>(client_info.output_buffer, 0)); // accum red size
			TRY(encode<CARD32>(client_info.output_buffer, 0)); // accum green size
			TRY(encode<CARD32>(client_info.output_buffer, 0)); // accum blue size
			TRY(encode<CARD32>(client_info.output_buffer, 0)); // accum alpha size
			TRY(encode<CARD32>(client_info.output_buffer, xTrue)); // double buffer
			TRY(encode<BOOL32>(client_info.output_buffer, xFalse)); // stereo
			TRY(encode<CARD32>(client_info.output_buffer, 32)); // buffer size
			TRY(encode<CARD32>(client_info.output_buffer, 24)); // depth size
			TRY(encode<CARD32>(client_info.output_buffer, 8)); // stencil size
			TRY(encode<CARD32>(client_info.output_buffer, 0)); // aux buffers
			TRY(encode<INT32>(client_info.output_buffer, 0)); // level

			break;
		}
		case X_GLXQueryServerString:
		{
			auto request = decode<xGLXQueryServerStringReq>(packet).value();

			dprintln("GLXQueryServerString");
			dprintln("  screen: {}", request.screen);
			dprintln("  name:   {}", request.name);

			BAN::StringView string;
			switch (request.name)
			{
				case GLX_VENDOR:
					string = "XBANAN";
					break;
				case GLX_VERSION:
					string = "1.4";
					break;
				case GLX_EXTENSIONS:
					string = "";
					break;
				default:
					ASSERT_NOT_REACHED();
			}

			xGLXQueryServerStringReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = static_cast<CARD32>((string.size() + 3) / 4),
				.n = static_cast<CARD16>(string.size()),
			};
			TRY(encode(client_info.output_buffer, reply));

			TRY(encode(client_info.output_buffer, string));
			for (size_t i = 0; (string.size() + i) % 4; i++)
				TRY(encode(client_info.output_buffer, '\0'));

			break;
		}
		case X_GLXClientInfo:
		{
			auto request = decode<xGLXClientInfoReq>(packet).value();

			dprintln("GLXClientInfo");
			dprintln("  major:     {}", request.major);
			dprintln("  minor:     {}", request.minor);
			dprintln("  extension: {}", BAN::StringView((char*)packet.data(), request.numbytes));

			break;
		}
		case X_GLXGetFBConfigs:
		{
			auto request = decode<xGLXGetFBConfigsReq>(packet).value();

			dprintln("GLXGetFBConfigs");
			dprintln("  screen: {}", request.screen);

			constexpr size_t fbconfigs = sizeof(g_fb_configs) / sizeof(*g_fb_configs);
			constexpr size_t attribs = sizeof(*g_fb_configs) / sizeof(**g_fb_configs);

			xGLXGetFBConfigsReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = 2 * fbconfigs * attribs,
				.numFBConfigs = fbconfigs,
				.numAttribs = attribs,
			};
			TRY(encode(client_info.output_buffer, reply));

			TRY(encode(client_info.output_buffer, g_fb_configs));

			break;
		}
		case X_GLXCreateNewContext:
		{
			auto request = decode<xGLXCreateNewContextReq>(packet).value();

			dprintln("GLXCreateNewContext");
			dprintln("  context:    {}", request.context);
			dprintln("  fbconfig:   {}", request.fbconfig);
			dprintln("  screen:     {}", request.screen);
			dprintln("  renderType: {}", request.renderType);
			dprintln("  shareList:  {}", request.shareList);
			dprintln("  isDirect:   {}", request.isDirect);

			auto* object = new MyGLXContext({
				.fbconfig = request.fbconfig,
				.is_direct = request.isDirect,
			});
			ASSERT(object);

			TRY(client_info.objects.insert(request.context));
			TRY(g_objects.insert(
				request.context,
				TRY(BAN::UniqPtr<Object>::create(Object {
					.type = Object::Type::Extension,
					.object = Object::Extension {
						.type_major = s_glx_major_opcode,
						.type_minor = GLXBadContext,
						.c_private = object,
						.destructor = [](Object::Extension& ext) { delete static_cast<MyGLXContext*>(ext.c_private); },
					},
				}))
			));

			break;
		}
		case X_GLXGetDrawableAttributes:
		{
			auto request = decode<xGLXGetDrawableAttributesReq>(packet).value();

			dprintln("GLXGetDrawableAttributes");
			dprintln("  drawable: {}", request.drawable);

			const auto& object = g_objects[request.drawable];
			ASSERT(object->type == Object::Type::Window);
			const auto& window = object->object.get<Object::Window>();

			xGLXGetDrawableAttributesReply reply {
				.type = X_Reply,
				.sequenceNumber = client_info.sequence,
				.length = 2 * 6,
				.numAttribs = 6,
			};
			TRY(encode(client_info.output_buffer, reply));

			TRY(encode<CARD32>(client_info.output_buffer, GLX_WIDTH));
			TRY(encode<CARD32>(client_info.output_buffer, window.width));

			TRY(encode<CARD32>(client_info.output_buffer, GLX_HEIGHT));
			TRY(encode<CARD32>(client_info.output_buffer, window.height));

			TRY(encode<CARD32>(client_info.output_buffer, GLX_PRESERVED_CONTENTS));
			TRY(encode<CARD32>(client_info.output_buffer, xTrue));

			TRY(encode<CARD32>(client_info.output_buffer, GLX_LARGEST_PBUFFER));
			TRY(encode<CARD32>(client_info.output_buffer, window.width * window.height));

			TRY(encode<CARD32>(client_info.output_buffer, GLX_FBCONFIG_ID));
			TRY(encode<CARD32>(client_info.output_buffer, 1));

			TRY(encode<CARD32>(client_info.output_buffer, GLX_EVENT_MASK));
			TRY(encode<CARD32>(client_info.output_buffer, 0));

			break;
		}
		case X_GLXCreateWindow:
		{
			auto request = decode<xGLXCreateWindowReq>(packet).value();

			dprintln("GLXCreateWindow");
			dprintln("  screen:     {}", request.screen);
			dprintln("  fbconfig:   {}", request.fbconfig);
			dprintln("  window:     {}", request.window);
			dprintln("  glxwindow:  {}", request.glxwindow);
			dprintln("  numAttribs: {}", request.numAttribs);

			for (size_t i = 0; i < request.numAttribs; i++)
			{
				const auto key = decode<CARD32>(packet).value();
				const auto value = decode<CARD32>(packet).value();
				dprintln("  {} = {}", key, value);
			}

			TRY(client_info.objects.insert(request.glxwindow));
			TRY(g_objects.insert(
				request.glxwindow,
				TRY(BAN::UniqPtr<Object>::create(Object {
					.type = Object::Type::Extension,
					.object = Object::Extension {
						.type_major = s_glx_major_opcode,
						.type_minor = GLXBadWindow,
						.c_private = nullptr,
						.destructor = [](Object::Extension&) { },
					},
				}))
			));

			break;
		}
		default:
			dwarnln("unsupported glx minor opcode {}", packet[1]);
			break;
	}

	return {};
}

static struct GLXInstaller
{
	GLXInstaller()
	{
		install_extension(GLX_EXTENSION_NAME, __GLX_NUMBER_EVENTS, __GLX_NUMBER_ERRORS, extension_glx);
		for (const auto& extension : g_extensions)
		{
			if (extension.name != GLX_EXTENSION_NAME)
				continue;
			s_glx_event_base = extension.event_base;
			s_glx_error_base = extension.error_base;
			s_glx_major_opcode = extension.major_opcode;
			break;
		}
	}
} installer;

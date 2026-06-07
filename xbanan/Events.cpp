#include "Base.h"
#include "Definitions.h"
#include "Keymap.h"
#include "Utils.h"

#include <X11/X.h>
#include <X11/Xatom.h>

#include <time.h>
#include <sys/epoll.h>

void register_event_fd(int fd, void* data)
{
	epoll_event event { .events = EPOLLIN, .data = { .fd = fd } };
	epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, fd, &event);

	MUST(g_epoll_thingies.insert(fd, {
		.type = EpollThingy::Type::Event,
		.value = data,
	}));
}

void unregister_event_fd(int fd)
{
	epoll_ctl(g_epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
	g_epoll_thingies.remove(fd);
}

void on_window_close_event(WINDOW wid)
{
	static CARD32 WM_PROTOCOLS     = g_atoms_name_to_id["WM_PROTOCOLS"_sv];
	static CARD32 WM_DELETE_WINDOW = g_atoms_name_to_id["WM_DELETE_WINDOW"_sv];

	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);
	auto& window = object.object.get<Object::Window>();

	const bool supports_wm_delete_winow =
		[&window]
		{
			auto wm_protocols_it = window.properties.find(WM_PROTOCOLS);
			if (wm_protocols_it == window.properties.end())
				return false;

			const auto& wm_protocols = wm_protocols_it->value;
			if (wm_protocols.type != XA_ATOM || wm_protocols.format != 32)
				return false;

			const auto atoms = BAN::ConstByteSpan(wm_protocols.data.span()).as_span<const CARD32>();
			for (auto atom : atoms)
				if (atom == WM_DELETE_WINDOW)
					return true;

			return false;
		}();

	if (!supports_wm_delete_winow)
		MUST(destroy_window(window.owner, wid));
	else
	{
		xEvent event { .u = {
			.clientMessage = {
				.window = wid,
				.u = { .l = {
					.type = WM_PROTOCOLS,
					.longs0 = static_cast<INT32>(WM_DELETE_WINDOW),
					.longs1 = static_cast<INT32>(time(nullptr)),
				}}
			}
		}};
		event.u.u.type = ClientMessage;
		event.u.u.detail = 32;
		event.u.u.sequenceNumber = window.owner.sequence;
		MUST(encode(window.owner.output_buffer, event));
	}
}

void on_window_resize_event(WINDOW wid, uint32_t new_width, uint32_t new_height)
{
	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);
	auto& window = object.object.get<Object::Window>();

	window.width  = new_width;
	window.height = new_height;

	MUST(window.pixels.resize(new_width * new_height));
	for (auto& pixel : window.pixels)
		pixel = window.background;

	MUST(window.double_buffer.resize(new_width * new_height));
	for (auto& pixel : window.double_buffer)
		pixel = window.background;

	MUST(send_configure_notify(wid));

	MUST(send_exposure_events_recursive(wid));
}

void on_window_move_event(WINDOW wid, int32_t x, int32_t y)
{
	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);
	auto& window = object.object.get<Object::Window>();

	window.x = x;
	window.y = y;

	MUST(send_configure_notify(wid));
}

void on_window_focus_event(WINDOW wid, bool focused)
{
	if (focused)
		g_focus_window = wid;

	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);

	auto& window = object.object.get<Object::Window>();
	if (window.focused == focused)
		return;

	window.focused = focused;

	// FIXME: handle children

	xEvent event { .u = {
		.focus = {
			.window = wid,
			.mode = NotifyNormal,
		}
	}};
	event.u.u.type = focused ? FocusIn : FocusOut,
	event.u.u.detail = NotifyNonlinear;
	MUST(window.send_event(event, FocusChangeMask));
}

void on_window_fullscreen_event(WINDOW wid, bool is_fullscreen)
{
	static CARD32 _NET_WM_STATE            = g_atoms_name_to_id["_NET_WM_STATE"_sv];
	static CARD32 _NET_WM_STATE_FULLSCREEN = g_atoms_name_to_id["_NET_WM_STATE_FULLSCREEN"_sv];

	auto window_it = g_objects.find(wid);
	if (window_it == g_objects.end() || window_it->value->type != Object::Type::Window)
		return;

	auto& window = window_it->value->object.get<Object::Window>();

	window.fullscreen = is_fullscreen;

	auto it = window.properties.find(_NET_WM_STATE);
	if (it == window.properties.end())
		it = MUST(window.properties.emplace(_NET_WM_STATE));

	auto& _net_wm_state = it->value;
	if (_net_wm_state.type != XA_ATOM || _net_wm_state.format != 32)
		_net_wm_state = {};

	_net_wm_state.type = XA_ATOM;
	_net_wm_state.format = 32;

	for (size_t i = 0; i + 4 <= _net_wm_state.data.size(); i += 4)
	{
		const auto atom = *reinterpret_cast<const CARD32*>(_net_wm_state.data.data() + i);
		if (atom != _NET_WM_STATE_FULLSCREEN)
			continue;
		_net_wm_state.data.remove(i);
		_net_wm_state.data.remove(i);
		_net_wm_state.data.remove(i);
		_net_wm_state.data.remove(i);
		i -= 4;
	}

	if (is_fullscreen)
	{
		const size_t atom_count = _net_wm_state.data.size() / 4;
		MUST(_net_wm_state.data.resize(_net_wm_state.data.size() + 4));
		reinterpret_cast<CARD32*>(_net_wm_state.data.data())[atom_count] = _NET_WM_STATE_FULLSCREEN;
	}

	xEvent event = { .u = {
		.property = {
			.window = wid,
			.atom = _NET_WM_STATE,
			.time = static_cast<CARD32>(time(nullptr)),
			.state = PropertyNewValue,
		}
	}};
	event.u.u.type = PropertyNotify;
	MUST(window.send_event(event, PropertyChangeMask));
}

static void send_key_button_pointer_event(WINDOW root_wid, BYTE detail, uint32_t event_mask, BYTE event_type, KeyButMask state)
{
	int32_t event_x, event_y;

	{
		auto& object = *g_objects[root_wid];
		ASSERT(object.type == Object::Type::Window);
		auto& window = object.object.get<Object::Window>();
		event_x = window.cursor_x;
		event_y = window.cursor_y;
	}

	const auto child_wid = find_child_window(root_wid, event_x, event_y);

	auto wid = child_wid;
	while (wid != None)
	{
		auto& object = *g_objects[wid];
		ASSERT(object.type == Object::Type::Window);
		auto& window = object.object.get<Object::Window>();
		if (window.full_event_mask() & event_mask)
			break;

		event_x += window.x;
		event_y += window.y;
		wid = window.parent;
	}

	if (wid == None)
	{
		if (event_type == MotionNotify && g_butmask == 0)
			return;
		wid = root_wid;
	}

	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);
	auto& window = object.object.get<Object::Window>();

	const auto [root_x, root_y] = get_window_position(wid);
	xEvent event { .u = {
		.keyButtonPointer = {
			.time = static_cast<CARD32>(time(nullptr)),
			.root = g_root.windowId,
			.event = wid,
			.child = static_cast<CARD32>(child_wid == wid ? None : child_wid),
			.rootX = static_cast<INT16>(root_x + event_x),
			.rootY = static_cast<INT16>(root_y + event_y),
			.eventX = static_cast<INT16>(event_x),
			.eventY = static_cast<INT16>(event_y),
			.state = state,
			.sameScreen = xTrue,
		}
	}};
	event.u.u.type = event_type,
	event.u.u.detail = detail;
	MUST(window.send_event(event, event_mask));
}

static void update_cursor_position_recursive(WINDOW wid, int32_t new_x, int32_t new_y)
{
	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);
	auto& window = object.object.get<Object::Window>();

	window.cursor_x = new_x;
	window.cursor_y = new_y;

	for (auto child_wid : window.children)
	{
		auto& child_object = *g_objects[child_wid];
		ASSERT(child_object.type == Object::Type::Window);
		auto& child_window = child_object.object.get<Object::Window>();
		update_cursor_position_recursive(child_wid, new_x - child_window.x, new_y - child_window.y);
	}
}

static BAN::Vector<WINDOW> get_path_to_child(WINDOW wid, int32_t x, int32_t y)
{
	BAN::Vector<WINDOW> result;

	const auto window_contains =
		[](const Object::Window& window, int32_t x, int32_t y) -> bool
		{
			return x >= 0 && y >= 0 && x < window.width && y < window.height;
		};

	for (;;)
	{
		const auto& object = *g_objects[wid];
		ASSERT(object.type == Object::Type::Window);

		const auto& window = object.object.get<Object::Window>();
		if (!window_contains(window, x, y))
			break;

		MUST(result.push_back(wid));

		const WINDOW old_wid = wid;
		for (auto child_wid : window.children)
		{
			const auto& child_object = *g_objects[child_wid];
			ASSERT(child_object.type == Object::Type::Window);

			const auto& child_window = child_object.object.get<Object::Window>();
			if (!window_contains(child_window, x - child_window.x, y - child_window.y))
				continue;
			wid = child_wid;
			break;
		}

		if (old_wid == wid)
			break;
	}

	return result;
}

static void send_enter_and_leave_events(WINDOW old_wid, int32_t old_x, int32_t old_y, WINDOW new_wid, int32_t new_x, int32_t new_y)
{
	// FIXME: correct event_x and event_y values in events

	const auto old_child_path = get_path_to_child(old_wid, old_x, old_y);
	const auto new_child_path = get_path_to_child(new_wid, new_x, new_y);

	size_t common_ancestors = 0;
	while (common_ancestors < old_child_path.size() && common_ancestors < new_child_path.size())
	{
		if (old_child_path[common_ancestors] != new_child_path[common_ancestors])
			break;
		common_ancestors++;
	}

	if (old_child_path.size() == common_ancestors && new_child_path.size() == common_ancestors)
		return;

	const bool linear = (common_ancestors == old_child_path.size() || common_ancestors == new_child_path.size());

	const auto get_flags =
		[](WINDOW wid) -> BYTE
		{
			return ELFlagSameScreen | (g_focus_window == wid ? ELFlagFocus : 0);
		};

	size_t leave_events = old_child_path.size() - common_ancestors;
	if (linear && common_ancestors && old_child_path.size() < new_child_path.size())
		leave_events++;

	size_t enter_events = new_child_path.size() - common_ancestors;
	if (linear && common_ancestors && new_child_path.size() < old_child_path.size())
		enter_events++;

	for (size_t i = 0; i < leave_events; i++)
	{
		const bool first = (i == 0);
		const BYTE detail =
			[&]() -> BYTE {
				if (!linear)
					return first ? NotifyNonlinear : NotifyNonlinearVirtual;
				if (!first)
					return NotifyVirtual;
				return (old_child_path.size() > new_child_path.size())
					? NotifyAncestor
					: NotifyInferior;
			}();

		const auto& wid = old_child_path[old_child_path.size() - i - 1];

		auto& object = *g_objects[wid];
		ASSERT(object.type == Object::Type::Window);
		auto& window = object.object.get<Object::Window>();

		const auto [root_x, root_y] = get_window_position(wid);
		xEvent event { .u = {
			.enterLeave = {
				.time = static_cast<CARD32>(time(nullptr)),
				.root = g_root.windowId,
				.event = wid,
				.child = first ? static_cast<WINDOW>(None) : old_child_path.back(),
				.rootX = static_cast<INT16>(root_x + old_x),
				.rootY = static_cast<INT16>(root_y + old_y),
				.eventX = static_cast<INT16>(old_x),
				.eventY = static_cast<INT16>(old_y),
				.state = static_cast<KeyButMask>(g_keymask | g_butmask),
				.mode = NotifyNormal,
				.flags = get_flags(wid),
			}
		}};
		event.u.u.type = LeaveNotify,
		event.u.u.detail = detail;
		MUST(window.send_event(event, LeaveWindowMask));
	}

	for (size_t i = 0; i < enter_events; i++)
	{
		const bool last = (i == enter_events - 1);
		const BYTE detail =
			[&]() -> BYTE {
				if (!linear)
					return last ? NotifyNonlinear : NotifyNonlinearVirtual;
				if (!last)
					return NotifyVirtual;
				return (old_child_path.size() > new_child_path.size())
					? NotifyInferior
					: NotifyAncestor;
			}();

		const auto& wid = new_child_path[new_child_path.size() - enter_events + i];

		auto& object = *g_objects[wid];
		ASSERT(object.type == Object::Type::Window);
		auto& window = object.object.get<Object::Window>();

		const auto [root_x, root_y] = get_window_position(wid);
		xEvent event { .u = {
			.enterLeave = {
				.time = static_cast<CARD32>(time(nullptr)),
				.root = g_root.windowId,
				.event = wid,
				.child = last ? static_cast<WINDOW>(None) : new_child_path.back(),
				.rootX = static_cast<INT16>(root_x + new_x),
				.rootY = static_cast<INT16>(root_y + new_y),
				.eventX = static_cast<INT16>(new_x),
				.eventY = static_cast<INT16>(new_y),
				.state = static_cast<KeyButMask>(g_keymask | g_butmask),
				.mode = NotifyNormal,
				.flags = get_flags(wid),
			}
		}};
		event.u.u.type = EnterNotify,
		event.u.u.detail = detail;
		MUST(window.send_event(event, EnterWindowMask));
	}

	for (const auto wid : old_child_path)
		g_objects[wid]->object.get<Object::Window>().hovered = false;
	for (const auto wid : new_child_path)
		g_objects[wid]->object.get<Object::Window>().hovered = true;
}

static WINDOW s_current_top_level_window = None;

void on_window_leave_event(WINDOW wid)
{
	if (s_current_top_level_window != wid)
		return;

	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);
	auto& window = object.object.get<Object::Window>();

	send_enter_and_leave_events(wid, window.cursor_x, window.cursor_y, wid, -1, -1);

	s_current_top_level_window = None;
}

void on_mouse_move_event(WINDOW wid, int32_t x, int32_t y)
{
	auto& object = *g_objects[wid];
	ASSERT(object.type == Object::Type::Window);
	auto& window = object.object.get<Object::Window>();

	update_cursor(wid, x, y);

	if (auto it = g_objects.find(s_current_top_level_window); it == g_objects.end())
		send_enter_and_leave_events(wid, -1, -1, wid, x, y);
	else
	{
		ASSERT(it->value->type == Object::Type::Window);
		auto& old_window = it->value->object.get<Object::Window>();
		send_enter_and_leave_events(s_current_top_level_window, old_window.cursor_x, old_window.cursor_y, wid, x, y);
	}

	s_current_top_level_window = wid;

	update_cursor_position_recursive(wid, x, y);

	// TODO: optimize with PointerMotionHint
	uint32_t event_mask = PointerMotionMask | PointerMotionHintMask;
	if (g_butmask) event_mask |= ButtonMotionMask;
	if (g_butmask & Button1Mask) event_mask |= Button1MotionMask;
	if (g_butmask & Button2Mask) event_mask |= Button2MotionMask;
	if (g_butmask & Button3Mask) event_mask |= Button3MotionMask;
	if (g_butmask & Button4Mask) event_mask |= Button4MotionMask;
	if (g_butmask & Button5Mask) event_mask |= Button5MotionMask;
	send_key_button_pointer_event(wid, NotifyNormal, event_mask, MotionNotify, g_keymask | g_butmask);
}

void on_mouse_button_event(WINDOW wid, uint8_t xbutton, bool pressed)
{
	uint16_t mask = 0;
	switch (xbutton)
	{
		case Button1: mask = Button1Mask; break;
		case Button2: mask = Button2Mask; break;
		case Button3: mask = Button3Mask; break;
		case Button4: mask = Button4Mask; break;
		case Button5: mask = Button5Mask; break;
	}

	const auto old_state = g_keymask | g_butmask;

	if (pressed)
		g_butmask |= mask;
	else
		g_butmask &= ~mask;

	send_key_button_pointer_event(
		wid,
		xbutton,
		pressed ? ButtonPressMask : ButtonReleaseMask,
		pressed ? ButtonPress     : ButtonRelease,
		old_state
	);
}

void on_key_event(WINDOW wid, uint8_t scancode, uint8_t xmod, bool pressed)
{
	const uint8_t xkeycode = scancode + g_keymap_min_keycode;
	if (xkeycode < g_keymap_min_keycode || xkeycode > g_keymap_max_keycode)
		return;

	{
		const uint8_t byte =       xkeycode / 8;
		const uint8_t mask = 1 << (xkeycode % 8);
		if (pressed)
			g_pressed_keys[byte] |= mask;
		else
			g_pressed_keys[byte] &= ~mask;
	}

	const auto old_state = g_keymask | g_butmask;

	g_keymask = xmod;

	send_key_button_pointer_event(
		wid,
		xkeycode,
		pressed ? KeyPressMask : KeyReleaseMask,
		pressed ? KeyPress     : KeyRelease,
		old_state
	);
}

void on_keymap_changed()
{
	for (auto& [id, object] : g_objects)
	{
		if (object->type != Object::Type::Window)
			continue;

		auto& client_info = object->object.get<Object::Window>().owner;

		xEvent event {};
		event.u.u.type = MappingNotify;
		event.u.u.sequenceNumber = client_info.sequence;

		event.u.mappingNotify.request = MappingKeyboard,
		event.u.mappingNotify.firstKeyCode = g_keymap_min_keycode,
		event.u.mappingNotify.count = g_keymap_size,
		MUST(encode(client_info.output_buffer, event));

		event.u.mappingNotify.request = MappingModifier;
		MUST(encode(client_info.output_buffer, event));
	}
}

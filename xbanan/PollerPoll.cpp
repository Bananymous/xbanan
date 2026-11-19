#include "Poller.h"

#include <BAN/Vector.h>

#include <poll.h>

static BAN::Vector<Pollable> s_pollables;
static BAN::Vector<pollfd>   s_pollfds;

static short get_poll_events(uint32_t event_mask)
{
	short result { 0 };
	if (event_mask & POLLMODE_IN)
		result |= POLLIN;
	if (event_mask & POLLMODE_OUT)
		result |= POLLOUT;
	return result;
}

void poller_initialize()
{
}

void poller_add(int fd, uint32_t event_mask, Pollable pollable)
{
	if (fd >= s_pollables.size())
		MUST(s_pollables.resize(fd + 1, {}));
	s_pollables[fd] = pollable;

	if (fd >= s_pollfds.size())
		MUST(s_pollfds.resize(fd + 1, { .fd = -1 }));
	s_pollfds[fd] = pollfd {
		.fd = fd,
		.events = get_poll_events(event_mask),
		.revents = 0,
	};
}

void poller_modify(int fd, uint32_t add_mask, uint32_t remove_mask)
{
	s_pollfds[fd].events |=  get_poll_events(add_mask);
	s_pollfds[fd].events &= ~get_poll_events(remove_mask);
}

void poller_remove(int fd)
{
	s_pollables[fd] = {};
	s_pollfds[fd] = { .fd = -1 };
}

size_t poller_wait(PollEvent events[], size_t events_count)
{
	if (poll(s_pollfds.data(), s_pollfds.size(), -1) == -1)
	{
		fprintf(stderr, "poll: %s\n", strerror(errno));
		exit(1);
	}

	size_t result = 0;
	for (int fd = 0; fd < s_pollfds.size() && result < events_count; fd++)
	{
		const auto revents = s_pollfds[fd].revents;
		if (revents == 0)
			continue;

		uint32_t event_mask { 0 };
		if (revents & POLLIN)
			event_mask |= POLLMODE_IN;
		if (revents & POLLOUT)
			event_mask |= POLLMODE_OUT;
		if (revents & POLLHUP)
			event_mask |= POLLMODE_HUP;
		if (revents & POLLERR)
			event_mask |= POLLMODE_ERR;

		events[result++] = {
			.fd         = fd,
			.event_mask = event_mask,
			.pollable   = s_pollables[fd],
		};
	}

	return result;
}

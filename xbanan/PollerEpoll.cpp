#include "Poller.h"

#include <BAN/HashMap.h>
#include <BAN/UniqPtr.h>
#include <BAN/Vector.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>

static BAN::HashMap<int, BAN::UniqPtr<PollEvent>> s_pollables;

static int s_epoll_fd { -1 };

static uint32_t get_epoll_events(uint32_t event_mask)
{
	uint32_t result { 0 };
	if (event_mask & POLLMODE_IN)
		result |= EPOLLIN;
	if (event_mask & POLLMODE_OUT)
		result |= EPOLLOUT;
	return result;
}

void poller_initialize()
{
	s_epoll_fd = epoll_create1(0);
	if (s_epoll_fd == -1)
	{
		fprintf(stderr, "epoll_create1: %s\n", strerror(errno));
		exit(1);
	}
}

void poller_add(int fd, uint32_t event_mask, Pollable pollable_)
{
	event_mask &= POLLMODE_IN | POLLMODE_OUT;

	auto pollable = MUST(BAN::UniqPtr<PollEvent>::create(fd, event_mask, pollable_));

	epoll_event event {
		.events = get_epoll_events(event_mask),
		.data = { .ptr = pollable.ptr() },
	};
	if (epoll_ctl(s_epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1)
	{
		fprintf(stderr, "epoll_ctl ADD: %s\n", strerror(errno));
		exit(1);
	}

	MUST(s_pollables.insert(fd, BAN::move(pollable)));
}

void poller_modify(int fd, uint32_t add_mask, uint32_t remove_mask)
{
	add_mask    &= POLLMODE_IN | POLLMODE_OUT;
	remove_mask &= POLLMODE_IN | POLLMODE_OUT;

	auto& pollable = s_pollables[fd];

	const auto new_event_mask = (pollable->event_mask | add_mask) & ~remove_mask;
	if (pollable->event_mask == new_event_mask)
		return;

	epoll_event event {
		.events = get_epoll_events(new_event_mask),
		.data = { .ptr = pollable.ptr() },
	};
	if (epoll_ctl(s_epoll_fd, EPOLL_CTL_MOD, fd, &event) == -1)
	{
		fprintf(stderr, "epoll_ctl MOD: %s\n", strerror(errno));
		exit(1);
	}

	pollable->event_mask = new_event_mask;
}

void poller_remove(int fd)
{
	if (epoll_ctl(s_epoll_fd, EPOLL_CTL_DEL, fd, nullptr) == -1)
	{
		fprintf(stderr, "epoll_ctl DEL: %s\n", strerror(errno));
		exit(1);
	}

	s_pollables.remove(fd);
}

size_t poller_wait(PollEvent events[], size_t events_count)
{
	static BAN::Vector<epoll_event> epoll_events;
	if (epoll_events.size() < events_count)
		MUST(epoll_events.resize(events_count));

	const int result = epoll_wait(s_epoll_fd, epoll_events.data(), epoll_events.size(), -1);
	if (result == -1)
	{
		fprintf(stderr, "epoll_wait: %s\n", strerror(errno));
		exit(1);
	}

	for (int i = 0; i < result; i++)
	{
		const auto epoll_event = epoll_events[i];

		uint32_t event_mask { 0 };
		if (epoll_event.events & EPOLLIN)
			event_mask |= POLLMODE_IN;
		if (epoll_event.events & EPOLLOUT)
			event_mask |= POLLMODE_OUT;
		if (epoll_event.events & EPOLLHUP)
			event_mask |= POLLMODE_HUP;
		if (epoll_event.events & EPOLLERR)
			event_mask |= POLLMODE_ERR;

		const auto& pollable = *static_cast<const PollEvent*>(epoll_event.data.ptr);
		events[i] = PollEvent {
			.fd         = pollable.fd,
			.event_mask = event_mask,
			.pollable   = pollable.pollable,
		};
	}

	return result;
}

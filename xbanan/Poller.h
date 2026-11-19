#pragma once

#include <stddef.h>
#include <stdint.h>

struct Pollable
{
	enum class Type {
		Server,
		Client,
		Event,
	};
	Type type;
	void* data;
};

enum PollMode : uint32_t
{
	POLLMODE_IN  = 1 << 0,
	POLLMODE_OUT = 1 << 1,
	POLLMODE_HUP = 1 << 2,
	POLLMODE_ERR = 1 << 3,
};

struct PollEvent
{
	int fd;
	uint32_t event_mask;
	Pollable pollable;
};

void poller_initialize();
void poller_add(int fd, uint32_t event_mask, Pollable pollable);
void poller_modify(int fd, uint32_t add_mask, uint32_t remove_mask);
void poller_remove(int fd);
size_t poller_wait(PollEvent events[], size_t events_count);

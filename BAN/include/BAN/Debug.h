#pragma once

#if __is_kernel

#include <kernel/Debug.h>

#else

#include <BAN/Formatter.h>
#include <stdio.h>
#include <time.h>

#define __debug_putchar [](int c) { putc_unlocked(c, stddbg); }

inline uint64_t _ban_init_start_ms()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

inline uint64_t _ban_start_ms = _ban_init_start_ms();

#define __print_timestamp() \
	do { \
		timespec ts; \
		clock_gettime(CLOCK_MONOTONIC, &ts); \
		const auto ms = ts.tv_sec * 1000 + ts.tv_nsec / 1000000 - _ban_start_ms; \
		BAN::Formatter::print(__debug_putchar, "[{}.{03}] ", ms / 1000, ms % 1000); \
	} while (false)

#define dprintln(...)											\
	do {														\
		flockfile(stddbg);										\
		__print_timestamp();									\
		BAN::Formatter::print(__debug_putchar, __VA_ARGS__);	\
		BAN::Formatter::print(__debug_putchar,"\n");			\
		fflush(stddbg);											\
		funlockfile(stddbg);									\
	} while (false)

#define dwarnln(...)											\
	do {														\
		flockfile(stddbg);										\
		BAN::Formatter::print(__debug_putchar, "\e[33m");		\
		__print_timestamp();									\
		BAN::Formatter::print(__debug_putchar, __VA_ARGS__);	\
		BAN::Formatter::print(__debug_putchar, "\e[m\n");		\
		fflush(stddbg);											\
		funlockfile(stddbg);									\
	} while(false)

#define derrorln(...)											\
	do {														\
		flockfile(stddbg);										\
		BAN::Formatter::print(__debug_putchar, "\e[31m");		\
		__print_timestamp();									\
		BAN::Formatter::print(__debug_putchar, __VA_ARGS__);	\
		BAN::Formatter::print(__debug_putchar, "\e[m\n");		\
		fflush(stddbg);											\
		funlockfile(stddbg);									\
	} while(false)

#define dprintln_if(cond, ...)		\
	do {							\
		if constexpr(cond)			\
			dprintln(__VA_ARGS__);	\
	} while(false)

#define dwarnln_if(cond, ...)		\
	do {							\
		if constexpr(cond)			\
			dwarnln(__VA_ARGS__);	\
	} while(false)

#define derrorln_if(cond, ...)		\
	do {							\
		if constexpr(cond)			\
			derrorln(__VA_ARGS__);	\
	} while(false)

#endif

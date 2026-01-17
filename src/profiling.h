#ifndef INCLUDE_SRC_PROFILING_H_
#define INCLUDE_SRC_PROFILING_H_

#ifdef PROFILING
#include "../vendor/spall.h"
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <sys/mman.h>
#include <immintrin.h>
#endif //PROFILING

#ifdef PROFILING
#define BEGIN_FUNC()                                                                                                \
	do {                                                                                                        \
		spall_buffer_begin(&spall_ctx, &spall_buffer, __FUNCTION__, sizeof(__FUNCTION__) - 1, get_clock()); \
	} while (0)

#define BEGIN_SECT(name)                                                                                          \
	do {                                                                                                      \
		const char __sect_name[] = name;                                                                  \
		spall_buffer_begin(&spall_ctx, &spall_buffer, __sect_name, sizeof(__sect_name) - 1, get_clock()); \
	} while (0)
#define END_FUNC()                                                        \
	do {                                                              \
		spall_buffer_end(&spall_ctx, &spall_buffer, get_clock()); \
	} while (0)
#define END_SECT(name)                                                    \
	do {                                                              \
		spall_buffer_end(&spall_ctx, &spall_buffer, get_clock()); \
	} while (0)
#else
#define BEGIN_FUNC() (void)0;
#define END_FUNC() (void)0;
#define BEGIN_SECT(name) (void)0;
#define END_SECT(name) (void)0;
#endif // PROFILING

#ifdef PROFILING
extern SpallProfile spall_ctx;
extern _Thread_local SpallBuffer spall_buffer;

static inline uint64_t get_clock(void)
{
	return __rdtsc();
}

#endif // PROFILING

#endif // INCLUDE_SRC_PROFILING_H_

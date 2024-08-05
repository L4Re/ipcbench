#pragma once

#if defined(__amd64__) || defined(__i686__)
#include <l4/util/rdtsc.h>
#define UNIT_TYPE l4_uint64_t
#define UNIT_NAME "cpu-cycles"
#define PREPARE() do {} while (0)
//#define TAKE_TIME(v) v = l4_rdpmc((1 << 30) | 2)
#define TAKE_TIME(v) v = l4_rdtsc()
#define DIFF(start, end) ((end) - (start))

#elif defined(__aarch64__)

#define UNIT_TYPE l4_uint64_t
#define UNIT_NAME "cpu-cycles"
#define PREPARE() do { asm volatile ("msr PMCCFILTR_EL0, %0" : : "r" (1UL << 27)); } while (0) // Enable counting in EL2 too
#define TAKE_TIME(v) asm volatile ("mrs %0, PMCCNTR_EL0" : "=r"(v))
#define DIFF(start, end) ((end) - (start))

#elif defined(__arm__)

#define UNIT_TYPE l4_uint64_t
#define UNIT_NAME "cpu-cycles"
#define PREPARE() do { asm volatile ("mcr p15, 0, %0, c14, c15, 3" : : "r" (1UL << 27)); } while (0) // Enable counting in EL2 too
#define TAKE_TIME(v) asm volatile ("mrc p15, 0, %0, c9, c13, 0" : "=r" (v))
#define DIFF(start, end) ((end) - (start))

#else
/* Generic */
#define UNIT_TYPE struct timespec
#define UNIT_NAME "ns"
#define PREPARE() do {} while (0)
#define TAKE_TIME(v) clock_gettime(CLOCK_REALTIME, &v)
#define DIFF(start, end) (end.tv_sec * 1000000000 + end.tv_nsec \
                           - start.tv_sec * 1000000000 + start.tv_nsec)
#endif


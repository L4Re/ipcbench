#pragma once

#if defined(__amd64__) || defined(__i686__)
#include <l4/util/rdtsc.h>
#include <l4/util/cpu.h>
#if 1
#define NUMBERS 1
#define UNIT_NAME(x) "cpu-cycles"
#define TAKE_TIME(v) v[0] = l4_rdtsc()
#else
#define NUMBERS 3
#define UNIT_NAME(x) ({ char *__nn[NUMBERS] = {"insnret-units", "clk-units", "tsc-units" }; __nn[x]; })
#define TAKE_TIME(v) do { v[0] = l4_rdpmc((1 << 30) | 0); v[1] = l4_rdpmc((1 << 30) | 1); v[2] = l4_rdpmc((1 << 30) | 2); } while (0)
#endif
#define UNIT_TYPE(t) l4_uint64_t t[NUMBERS]
#define PREPARE() do {} while (0)
#define DIFF(idx, start, end) (end[idx] - start[idx])
#define SYNC() l4util_cpu_capabilities()
#elif defined(__aarch64__)

#define NUMBERS 1
#define UNIT_TYPE(x) l4_uint64_t x
#define UNIT_NAME(x) "cpu-cycles"
#define PREPARE() do { asm volatile ("msr PMCCFILTR_EL0, %0" : : "r" (1UL << 27)); } while (0) // Enable counting in EL2 too
#define TAKE_TIME(v) asm volatile ("mrs %0, PMCCNTR_EL0" : "=r"(v))
#define DIFF(x, start, end) ((end) - (start))
#define SYNC() do {} while (0)

#elif defined(__arm__)

#define NUMBERS 1
#define UNIT_TYPE(x) l4_uint64_t x
#define UNIT_NAME(x) "cpu-cycles"
#define PREPARE() do { asm volatile ("mcr p15, 0, %0, c14, c15, 3" : : "r" (1UL << 27)); } while (0) // Enable counting in EL2 too
#define TAKE_TIME(v) asm volatile ("mrc p15, 0, %0, c9, c13, 0" : "=r" (v))
#define DIFF(x, start, end) ((end) - (start))
#define SYNC() do {} while (0)

#else
/* Generic */
#include <time.h>

#define NUMBERS 1
#define UNIT_TYPE(x) l4_uint64_t x
#define UNIT_NAME(x) "ns"
#define PREPARE() do {} while (0)
#define TAKE_TIME(v) ({ struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); v = ts.tv_sec * 1000000000 + ts.tv_nsec;})
#define DIFF(x, start, end) ((end) - (start))
#define SYNC() do {} while (0)
#endif

#define PRINT_RESULT(start, end, opname) \
  for (unsigned n = 0; n < NUMBERS; ++n) \
    printf("done %d " opname "s in %lld %s, %lld %s/" opname "\n", \
           Num_IPCs, DIFF(n, start, end), UNIT_NAME(n), \
           DIFF(n, start, end) / Num_IPCs, UNIT_NAME(n)); \

enum { Num_rounds = 300000, Num_IPCs = Num_rounds * 2 };


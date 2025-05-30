#pragma once

#include <l4/util/kip.h>
#include <stdio.h>
#include <stdlib.h>

static inline void check_kernel_features(void)
{
  if (!l4_kip_kernel_has_feature(l4_kip(), "perf_cnt_user"))
    {
      printf("You need to enable CONFIG_PERF_CNT_USER in the microkernel.\n");
      printf("This is required to use the performance measurement facilities\n"
             "of the processor in this benchmark program.\n");
      exit(1);
    }
}


#if defined(__amd64__) || defined(__i686__)
#include <l4/util/rdtsc.h>
#include <l4/util/cpu.h>
#if 0
#define NUMBERS 1
#define UNIT_NAME(x) "cpu-cycles"
#define TAKE_TIME(v) v[0] = l4_rdtsc()
#define PREPARE() do {} while (0)
#else
#define NUMBERS 3
#define UNIT_NAME(x) ({ char *__nn[NUMBERS] = {"insnret-units", "clk-units", "tsc-units" }; __nn[x]; })
#define TAKE_TIME(v) do { v[0] = l4_rdpmc((1 << 30) | 0); v[1] = l4_rdpmc((1 << 30) | 1); v[2] = l4_rdpmc((1 << 30) | 2); } while (0)
#define PREPARE() do { check_kernel_features(); } while (0)
#endif
#define UNIT_TYPE(t) l4_uint64_t t[NUMBERS]
#define DIFF(idx, start, end) (end[idx] - start[idx])
#define SYNC() l4util_cpu_capabilities()
#elif defined(__aarch64__)

#if 1 // < PMUv3, just cpu-cycles
#define NUMBERS 1
#define UNIT_NAME(x) "cpu-cycles"
#define TAKE_TIME(v) asm volatile ("mrs %0, PMCCNTR_EL0" : "=r"(v))
#define PREPARE() do { check_kernel_features(); asm volatile ("msr PMCCFILTR_EL0, %0" : : "r" (1UL << 27)); } while (0) // Enable counting in EL2 too
#else
#define NUMBERS 3
#define UNIT_NAME(x) ({ char *__nn[NUMBERS] = {"CPU_CYCLES", "INST_RETIRED", "EXC_TAKEN" }; __nn[x]; })
#define SET_EVENT(eventreg, eventtype) do { asm volatile ("msr " eventreg ", %0" : : "r" ((1UL << 27) | eventtype));  } while (0) // Enable counting in EL2 too
#define READ_CNT(eventreg) ({ l4_uint64_t v; asm volatile("mrs %0, " eventreg : "=r"(v)); v; })
#define PREPARE() do { check_kernel_features(); asm volatile("msr PMCNTENSET_EL0, %0" : : "r" (7 | (1UL << 31))); SET_EVENT("PMEVTYPER0_EL0", 0x11); SET_EVENT("PMEVTYPER1_EL0", 8); SET_EVENT("PMEVTYPER2_EL0", 9);  } while (0)
#define TAKE_TIME(v) do { v[0] = READ_CNT("PMEVCNTR0_EL0"); v[1] = READ_CNT("PMEVCNTR1_EL0"); v[2] = READ_CNT("PMEVCNTR2_EL0"); } while (0)
#endif

#define UNIT_TYPE(t) l4_uint64_t t[NUMBERS]
#define DIFF(idx, start, end) (end[idx] - start[idx])
#define SYNC() do {} while (0)

#elif defined(__arm__)

#define NUMBERS 1
#define UNIT_TYPE(x) l4_uint64_t x
#define UNIT_NAME(x) "cpu-cycles"
#define PREPARE() do { check_kernel_features(); asm volatile ("mcr p15, 0, %0, c14, c15, 7" : : "r" (1UL << 27)); } while (0) // Enable counting in EL2 too
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

#define PRINT_RESULT(cpu, start, end, opname, factor) \
  for (unsigned n = 0; n < NUMBERS; ++n) \
    printf("CPU %2u done %d " opname "s in %lld %s, %lld %s/" opname "\n", \
           cpu, Num_rounds * factor, DIFF(n, start, end), UNIT_NAME(n), \
           DIFF(n, start, end) / (Num_rounds * factor), UNIT_NAME(n)); \

enum { Num_rounds = 300000 };

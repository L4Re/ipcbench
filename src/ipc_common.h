/* SPDX-License-Identifier: MIT */
/* Small and simple L4Re syscall benchmark program */
/* by Adam Lackorzynski <adam@l4re.org> */

#pragma once

#include <l4/sys/types.h>

struct Caller_params
{
  l4_cap_idx_t responder_cap;
  unsigned cpu;
};

void check_pthr_err(int r, char const *msg);

void enumerate_cpus(void (*cb)(unsigned cpu, void *arg), void *arg);
unsigned count_cpus(void);
long run_thread(l4_cap_idx_t thread, unsigned cpu);

void *fn_caller(void *cp);
void *fn_responder(void *ignore);

void syscall_bench(l4_cap_idx_t thread, unsigned cpu);

void wait_for_start(void);
void start(void);
void set_completion_counter(unsigned val);
void finished(void);

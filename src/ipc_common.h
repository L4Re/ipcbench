/* SPDX-License-Identifier: MIT */
/* Small and simple L4Re syscall benchmark program */
/* by Adam Lackorzynski <adam@l4re.org> */

#pragma once

struct Caller_params
{
  l4_cap_idx_t responder_cap;
  unsigned cpu;
};

void check_pthr_err(int r, char const *msg);

void enumerate_cpus(void (*cb)(unsigned cpu, void *arg), void *arg);
unsigned count_cpus(void);

void *fn_caller(void *cp);
void *fn_responder(void *ignore);

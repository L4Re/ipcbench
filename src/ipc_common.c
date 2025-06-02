/* SPDX-License-Identifier: MIT */
/* Small and simple L4Re syscall benchmark program */
/* by Adam Lackorzynski <adam@l4re.org> */

#include <pthread-l4.h>
#include <stdio.h>
#include <string.h>

#include <l4/sys/ipc.h>
#include <l4/sys/rcv_endpoint.h>
#include <l4/sys/thread.h>
#include <l4/re/env.h>

#include "ipc_common.h"
#include "measure.h"

enum { Enable_return_checks = 1 };

void check_pthr_err(int r, char const *msg)
{
  if (r != 0)
    {
      printf("error: %s: %s (%d)\n", msg, strerror(r), r);
      exit(1);
    }
}

void enumerate_cpus(void (*cb)(unsigned cpu, void *arg), void *arg)
{
  enum { Map_size = sizeof(l4_umword_t) * 8 };

  l4_cap_idx_t sched = l4re_env()->scheduler;
  l4_umword_t cpu_max = 0;
  unsigned offset = 0;

  do
    {
      l4_sched_cpu_set_t cpus = l4_sched_cpu_set(offset, 0, 0);
      if (l4_error(l4_scheduler_info(sched, &cpu_max, &cpus)))
        {
          printf("Error enumerating CPUs!\n");
          return;
        }

      for (unsigned i = 0; i < Map_size; i++)
        if (cpus.map & (1UL << i))
          cb(offset + i, arg);

      offset += Map_size;
    }
  while (offset < cpu_max);
}

static unsigned start_sync_val;

void wait_for_start()
{
  while (!start_sync_val)
    asm volatile("" : : : "memory");
}

void start(void)
{
  start_sync_val = 1;
}

static unsigned completion_counter = 1;

void set_completion_counter(unsigned val)
{
  completion_counter = val;
}

void finished(void)
{
  unsigned n = __atomic_sub_fetch(&completion_counter, 1, __ATOMIC_SEQ_CST);

  if (n == 0)
    start_sync_val = ~0u;
  else
    while (start_sync_val != ~0u)
      asm volatile("" : : : "memory");
}

static void count_cpus_cb(unsigned cpu, void *arg)
{
  (void)cpu;
  unsigned *num = (unsigned *)arg;
  (*num)++;
}

unsigned count_cpus(void)
{
  unsigned num_cpus = 0;
  enumerate_cpus(&count_cpus_cb, &num_cpus);
  return num_cpus;
}

long run_thread(l4_cap_idx_t thread, unsigned cpu)
{
  l4_sched_param_t sp = l4_sched_param(2, 0);
  sp.affinity = l4_sched_cpu_set(cpu, 0, 1);
  return l4_error(l4_scheduler_run_thread(l4re_env()->scheduler, thread, &sp));
}

/* This thread is initiating IPC */
void *fn_caller(void *cp)
{
  struct Caller_params *params = (struct Caller_params *)cp;

  l4_utcb_t *utcb = l4_utcb();
  l4_msgtag_t tag = l4_msgtag(0, 0, 0, 0);
  l4_cap_idx_t responder_cap = params->responder_cap;

  wait_for_start();

  PREPARE();

  l4_msgtag_t r;
  UNIT_TYPE(start);
  UNIT_TYPE(end);
  TAKE_TIME(start);
  SYNC();
  for (int i = 0; i < Num_rounds; ++i)
    {
      r = l4_ipc_call(responder_cap, utcb, tag, L4_IPC_NEVER);
      if (Enable_return_checks && l4_ipc_error(r, utcb))
        printf("caller: ipc err\n");
    }
  SYNC();
  TAKE_TIME(end);

  finished();

  PRINT_RESULT(params->cpu, start, end, "IPC", 2);

  return NULL;
}

/* This thread is replying to IPC */
void *fn_responder(void *ignore)
{
  (void)ignore;
  l4_utcb_t *utcb = l4_utcb();
  l4_umword_t label;

  // Allow cancellation of thread at end of test
  check_pthr_err(pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL),
                 "pthread_setcanceltype");
  check_pthr_err(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL),
                 "pthread_setcancelstate");

  l4_msgtag_t r = l4_ipc_wait(utcb, &label, L4_IPC_NEVER);
  if (Enable_return_checks && l4_ipc_error(r, utcb))
    printf("responder: ipc err (wait)\n");

  l4_msgtag_t tag = l4_msgtag(0, 0, 0, 0);
  while (1)
    {
      r = l4_ipc_reply_and_wait(utcb, tag, &label, L4_IPC_NEVER);
      if (Enable_return_checks && l4_ipc_error(r, utcb))
        printf("responder: ipc err (reply+wait)\n");
    }

  return NULL;
}

void syscall_bench(l4_cap_idx_t thread, unsigned cpu)
{
  l4_msgtag_t tag = l4_msgtag(L4_PROTO_THREAD, 1, 0, 0);
  l4_utcb_t *utcb = l4_utcb();

  PREPARE();

  l4_utcb_mr_u(utcb)->mr[L4_THREAD_CONTROL_MR_IDX_FLAGS] = L4_THREAD_CONTROL_OP;

  UNIT_TYPE(start);
  UNIT_TYPE(end);
  TAKE_TIME(start);
  SYNC();
  for (int i = 0; i < Num_rounds; ++i)
    {
      // This will go to the thread's control function and bail out
      // early there.
      l4_msgtag_t r = l4_ipc_call(thread, utcb, tag, L4_IPC_NEVER);
      if (Enable_return_checks && l4_ipc_error(r, utcb))
        printf("IPC error\n");
    }
  SYNC();
  TAKE_TIME(end);

  finished();

  PRINT_RESULT(cpu, start, end, "syscall", 1);
}

/* SPDX-License-Identifier: MIT */
/* Small and simple L4Re IPC benchmark program */
/* by Adam Lackorzynski <adam@l4re.org> */

#include <pthread-l4.h>
#include <stdio.h>
#include <string.h>

#include <time.h>

#include <l4/sys/ipc.h>
#include <l4/sys/rcv_endpoint.h>
#include <l4/re/env.h>

#include "measure.h"

struct Caller_params
{
  l4_cap_idx_t responder_cap;
  unsigned cpu;
};

struct Pair
{
  pthread_t caller_thread;
  struct Caller_params caller_params;
  pthread_t responder_thread;
};

struct Spawn_param
{
  unsigned idx;
  unsigned num_cpus;
  struct Pair *pairs;
};

enum { Enable_return_checks = 1 };

static void enumerate_cpus(void (*cb)(unsigned cpu, void *arg), void *arg)
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

static void count_cpus(unsigned cpu, void *arg)
{
  (void)cpu;
  unsigned *num = (unsigned *)arg;
  (*num)++;
}

static void check_pthr_err(int r, char const *msg)
{
  if (r != 0)
    {
      printf("error: %s: %s (%d)\n", msg, strerror(r), r);
      exit(1);
    }
}

static long run_thread(l4_cap_idx_t thread, unsigned cpu)
{
  l4_sched_param_t sp = l4_sched_param(2, 0);
  sp.affinity = l4_sched_cpu_set(cpu, 0, 1);
  return l4_error(l4_scheduler_run_thread(l4re_env()->scheduler, thread, &sp));
}

/* This thread is initiating IPC */
static void *fn_caller(void *pv)
{
  struct Caller_params *params = (struct Caller_params *)pv;

  l4_utcb_t *utcb = l4_utcb();
  l4_msgtag_t tag = l4_msgtag(0, 0, 0, 0);
  l4_cap_idx_t responder_cap = params->responder_cap;

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

  PRINT_RESULT(start, end, "IPC", 2);

  return NULL;
}

/* This thread is replying to IPC */
static void *fn_responder(void *ignore)
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

static void spawn_pair(unsigned cpu, void *arg)
{
  struct Spawn_param *param = (struct Spawn_param *)arg;

  if (param->idx >= param->num_cpus)
    {
      printf("CPU %u showed up late! Ignoring...\n", cpu);
      return;
    }

  unsigned idx = param->idx++;
  struct Pair *pair = &param->pairs[idx];

  // Create threads without starting them. We want to put them directly on the
  // right CPU...
  pthread_attr_t attr;
  check_pthr_err(pthread_attr_init(&attr), "pthread_attr_init");
  attr.create_flags |= PTHREAD_L4_ATTR_NO_START;

  // Create responder first
  check_pthr_err(pthread_create(&pair->responder_thread, &attr, fn_responder,
                                NULL),
                 "create responder thread");

  long err = run_thread(pthread_l4_cap(pair->responder_thread), cpu);
  if (err)
    {
      printf("Error starting responder on CPU %u: %ld\n", cpu, err);
      exit(1);
    }

  // Wait for responder to be ready
  l4_utcb_t *utcb = l4_utcb();
  if (l4_ipc_error(l4_ipc_call(pthread_l4_cap(pair->responder_thread), utcb,
                               l4_msgtag(0, 0, 0, 0), L4_IPC_NEVER),
                   utcb))
    printf("Error syncing with responder thread!\n");

  // Now we can create the caller
  pair->caller_params.responder_cap = pthread_l4_cap(pair->responder_thread);
  pair->caller_params.cpu = cpu;
  check_pthr_err(pthread_create(&pair->caller_thread, &attr, fn_caller,
                                &pair->caller_params),
                 "create caller thread");

  err = run_thread(pthread_l4_cap(pair->caller_thread), cpu);
  if (err)
    {
      printf("Error starting caller on CPU %u: %ld\n", cpu, err);
      exit(1);
    }
}

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  unsigned num_cpus = 0;
  enumerate_cpus(&count_cpus, &num_cpus);
  printf("Found %u CPUs. Measuring core-local IPC latency on all CPUs.\n",
         num_cpus);

  // Spawn IPC benchmark pairs on all CPUs.
  struct Pair pairs[num_cpus];
  struct Spawn_param sp = {0, num_cpus, pairs};
  enumerate_cpus(&spawn_pair, &sp);

  // Wait for tests to finish
  for (unsigned i = 0; i < sp.idx; i++)
    {
      check_pthr_err(pthread_join(pairs[i].caller_thread, NULL),
                     "join caller thread");
      check_pthr_err(pthread_cancel(pairs[i].responder_thread),
                     "cancel responder thread");
      check_pthr_err(pthread_join(pairs[i].responder_thread, NULL),
                     "join responder thread");
    }

  return 0;
}

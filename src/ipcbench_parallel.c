/* SPDX-License-Identifier: MIT */
/* Small and simple L4Re IPC benchmark program */
/* by Adam Lackorzynski <adam@l4re.org> */

#include <pthread-l4.h>
#include <stdio.h>
#include <string.h>

#include <l4/re/env.h>

#include "ipc_common.h"

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

  unsigned num_cpus = count_cpus();
  printf("Found %u CPUs. Measuring core-local IPC latency on all CPUs.\n",
         num_cpus);

  // Spawn IPC benchmark pairs on all CPUs.
  struct Pair pairs[num_cpus];
  struct Spawn_param sp = {0, num_cpus, pairs};
  enumerate_cpus(&spawn_pair, &sp);

  start();

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

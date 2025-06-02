/* SPDX-License-Identifier: MIT */
/* Small and simple L4Re syscall benchmark program */
/* by Adam Lackorzynski <adam@l4re.org> */

#include <stdio.h>

#include <pthread-l4.h>

#include "ipc_common.h"


struct Thread
{
  pthread_t thread;
  unsigned cpu;
};

struct Spawn_param
{
  unsigned idx;
  unsigned num_cpus;
  struct Thread *threads;
};

static void *fn_syscall(void *arg)
{
  struct Thread *thread = (struct Thread *)arg;
  wait_for_start();
  syscall_bench(pthread_l4_cap(thread->thread), thread->cpu);
  return NULL;
}

static void spawn_threads(unsigned cpu, void *sp)
{
  struct Spawn_param *param = (struct Spawn_param *)sp;
  if (param->idx >= param->num_cpus)
    {
      printf("CPU %u showed up late! Ignoring...\n", cpu);
      return;
    }

  unsigned idx = param->idx++;
  struct Thread *thread = &param->threads[idx];

  thread->cpu = cpu;

  // Create thread without starting. We want to put it directly on the right
  // CPU...
  pthread_attr_t attr;
  check_pthr_err(pthread_attr_init(&attr), "pthread_attr_init");
  attr.create_flags |= PTHREAD_L4_ATTR_NO_START;
  check_pthr_err(pthread_create(&thread->thread, &attr, fn_syscall,
                                thread),
                 "create thread");

  long err = run_thread(pthread_l4_cap(thread->thread), cpu);
  if (err)
    {
      printf("Error starting responder on CPU %u: %ld\n", cpu, err);
      exit(1);
    }
}

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  unsigned num_cpus = count_cpus();
  printf("Found %u CPUs. Measuring syscall latency on all CPUs.\n",
         num_cpus);

  // Spawn IPC benchmark pairs on all CPUs.
  struct Thread threads[num_cpus];
  struct Spawn_param sp = {0, num_cpus, threads};
  enumerate_cpus(&spawn_threads, &sp);

  set_completion_counter(sp.idx);

  start();

  // Wait for tests to finish
  for (unsigned i = 0; i < sp.idx; i++)
    check_pthr_err(pthread_join(threads[i].thread, NULL), "join thread");

  return 0;
}

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

enum { Enable_return_checks = 1 };

static l4_cap_idx_t th_cap_b = L4_INVALID_CAP;

static void check_pthr_err(int r, char const *msg)
{
  if (r != 0)
    {
      printf("error: %s: %s (%d)\n", msg, strerror(r), r);
      exit(1);
    }
}

/* This thread is initiating IPC */
static void *fn_a(void *ignore)
{
  (void)ignore;
  /* Wait for partner thread to be ready */
  while (!l4_is_valid_cap(th_cap_b))
    l4_ipc_sleep_ms(1);

  l4_utcb_t *utcb = l4_utcb();
  l4_msgtag_t tag = l4_msgtag(0, 0, 0, 0);

  PREPARE();

  l4_msgtag_t r;
  UNIT_TYPE(start);
  UNIT_TYPE(end);
  TAKE_TIME(start);
  SYNC();
  for (int i = 0; i < Num_rounds; ++i)
    {
      r = l4_ipc_call(th_cap_b, utcb, tag, L4_IPC_NEVER);
      if (Enable_return_checks && l4_ipc_error(r, utcb))
        printf("fn_a: ipc err\n");
    }
  SYNC();
  TAKE_TIME(end);

  PRINT_RESULT(start, end, "IPC", 2);

  return NULL;
}

/* This thread is replying to IPC */
static void *fn_b(void *ignore)
{
  (void)ignore;
  l4_utcb_t *utcb = l4_utcb();
  l4_umword_t label;

  th_cap_b = pthread_l4_cap(pthread_self());

  l4_msgtag_t r = l4_ipc_wait(utcb, &label, L4_IPC_NEVER);
  if (Enable_return_checks && l4_ipc_error(r, utcb))
    printf("fn_b: ipc err (wait)\n");

  l4_msgtag_t tag = l4_msgtag(0, 0, 0, 0);
  while (1)
    {
      r = l4_ipc_reply_and_wait(utcb, tag, &label, L4_IPC_NEVER);
      if (Enable_return_checks && l4_ipc_error(r, utcb))
        printf("fn_b: ipc err (reply+wait)\n");
    }

  return NULL;
}

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  pthread_t thread_a;
  pthread_t thread_b;
  pthread_attr_t attr;

  check_pthr_err(pthread_attr_init(&attr), "pthread_attr_init");

  check_pthr_err(pthread_create(&thread_a, &attr, fn_a, NULL),
                 "create thread a");
  check_pthr_err(pthread_create(&thread_b, &attr, fn_b, NULL),
                 "create thread b");

  void *retval;
  check_pthr_err(pthread_join(thread_a, &retval), "join thread a");

  return 0;
}

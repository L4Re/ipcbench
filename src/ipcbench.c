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
};

enum { Enable_return_checks = 1 };

static void check_pthr_err(int r, char const *msg)
{
  if (r != 0)
    {
      printf("error: %s: %s (%d)\n", msg, strerror(r), r);
      exit(1);
    }
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

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  pthread_t thread_caller;
  pthread_t thread_responder;

  check_pthr_err(pthread_create(&thread_responder, NULL, fn_responder, NULL),
                 "create responder thread");

  // Wait for responder to be ready
  l4_utcb_t *utcb = l4_utcb();
  if (l4_ipc_error(l4_ipc_call(pthread_l4_cap(thread_responder), utcb,
                               l4_msgtag(0, 0, 0, 0), L4_IPC_NEVER),
                   utcb))
    printf("Error syncing with responder thread!\n");

  struct Caller_params cp;
  cp.responder_cap = pthread_l4_cap(thread_responder);
  check_pthr_err(pthread_create(&thread_caller, NULL, fn_caller, &cp),
                 "create caller thread");

  check_pthr_err(pthread_join(thread_caller, NULL), "join caller thread");
  check_pthr_err(pthread_cancel(thread_responder), "cancel responder thread");
  check_pthr_err(pthread_join(thread_responder, NULL), "join responder thread");

  return 0;
}

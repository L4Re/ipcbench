/* SPDX-License-Identifier: MIT */
/* Small and simple L4Re IPC benchmark program */
/* by Adam Lackorzynski <adam@l4re.org> */

#include <pthread-l4.h>
#include <stdio.h>
#include <string.h>

#include "ipc_common.h"

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  pthread_t thread_responder;
  check_pthr_err(pthread_create(&thread_responder, NULL, fn_responder, NULL),
                 "create responder thread");

  // Wait for responder to be ready
  l4_utcb_t *utcb = l4_utcb();
  if (l4_ipc_error(l4_ipc_call(pthread_l4_cap(thread_responder), utcb,
                               l4_msgtag(0, 0, 0, 0), L4_IPC_NEVER),
                   utcb))
    printf("Error syncing with responder thread!\n");

  struct Caller_params cp = {pthread_l4_cap(thread_responder), 0};
  start();
  fn_caller(&cp);

  check_pthr_err(pthread_cancel(thread_responder), "cancel responder thread");
  check_pthr_err(pthread_join(thread_responder, NULL), "join responder thread");

  return 0;
}

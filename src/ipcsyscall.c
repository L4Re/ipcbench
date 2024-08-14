/* SPDX-License-Identifier: MIT */
/* Small and simple L4Re syscall benchmark program */
/* by Adam Lackorzynski <adam@l4re.org> */

#include <assert.h>
#include <stdio.h>

#include <l4/re/env.h>
#include <l4/sys/thread.h>

#include "measure.h"

enum { Enable_return_checks = 1 };

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

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
      l4_msgtag_t r = l4_ipc_call(l4re_env()->main_thread, utcb, tag,
                                  L4_IPC_NEVER);
      if (Enable_return_checks && l4_ipc_error(r, utcb))
        printf("IPC error\n");
    }
  SYNC();
  TAKE_TIME(end);

  PRINT_RESULT(start, end, "syscall");

  return 0;
}

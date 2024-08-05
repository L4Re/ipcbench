/* SPDX-License-Identifier: MIT */
/* Small and simple L4Re IPC benchmark program -- Server side */
/* by Adam Lackorzynski <adam@l4re.org> */

#include <assert.h>
#include <stdio.h>

#include <l4/sys/ipc.h>
#include <l4/sys/rcv_endpoint.h>
#include <l4/re/env.h>

#include "measure.h"

enum { Enable_return_checks = 1 };

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  l4_cap_idx_t server = l4re_env_get_cap("comm");
  assert(l4_is_valid_cap(server));

  l4_msgtag_t r = l4_rcv_ep_bind_thread(server, l4re_env()->main_thread, 0x10);
  assert(l4_error(r) == 0);

  l4_utcb_t *utcb = l4_utcb();
  l4_umword_t label;

  r = l4_ipc_wait(utcb, &label, L4_IPC_NEVER);
  if (Enable_return_checks && l4_ipc_error(r, utcb))
    printf("IPC error (wait)\n");

  l4_msgtag_t tag = l4_msgtag(0, 0, 0, 0);
  while (1)
    {
      r = l4_ipc_reply_and_wait(utcb, tag, &label, L4_IPC_NEVER);
      if (Enable_return_checks && l4_ipc_error(r, utcb))
        printf("IPC error (reply+wait)\n");
    }

  return 0;
}

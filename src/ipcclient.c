/* SPDX-License-Identifier: MIT */
/* Small and simple L4Re IPC benchmark program -- Client side */
/* by Adam Lackorzynski <adam@l4re.org> */

#include <assert.h>
#include <stdio.h>

#include <l4/re/env.h>

#include "measure.h"

enum { Enable_return_checks = 1 };

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  l4_cap_idx_t server = l4re_env_get_cap("comm");
  assert(l4_is_valid_cap(server));

  l4_utcb_t *utcb = l4_utcb();
  l4_msgtag_t tag = l4_msgtag(0, 0, 0, 0);

  l4_msgtag_t r = l4_ipc_call(server, utcb, tag, L4_IPC_NEVER);
  if (l4_ipc_error(r, utcb))
    printf("IPC error on initial rendevouz\n");

  PREPARE();

  UNIT_TYPE(start);
  UNIT_TYPE(end);
  TAKE_TIME(start);
  SYNC();
  for (int i = 0; i < Num_rounds; ++i)
    {
      r = l4_ipc_call(server, utcb, tag, L4_IPC_NEVER);
      if (Enable_return_checks && l4_ipc_error(r, utcb))
        printf("IPC error\n");
    }
  SYNC();
  TAKE_TIME(end);

  PRINT_RESULT(0, start, end, "IPC", 2);

  return 0;
}

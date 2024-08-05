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

  PREPARE();

  l4_msgtag_t r;
  UNIT_TYPE start, end;
  TAKE_TIME(start);
  for (int i = 0; i < Num_rounds; ++i)
    {
      r = l4_ipc_call(server, utcb, tag, L4_IPC_NEVER);
      if (Enable_return_checks && l4_ipc_error(r, utcb))
        printf("IPC error\n");
    }
  TAKE_TIME(end);

  UNIT_TYPE diff = DIFF(start, end);

  printf("done %d IPCs in %lld " UNIT_NAME ", %lld " UNIT_NAME "/IPC\n",
         Num_IPCs, diff, diff / Num_IPCs);

  return 0;
}
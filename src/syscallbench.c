/* SPDX-License-Identifier: MIT */
/* Small and simple L4Re syscall benchmark program */
/* by Adam Lackorzynski <adam@l4re.org> */

#include <l4/re/env.h>

#include "ipc_common.h"

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  syscall_bench(l4re_env()->main_thread, 0);

  return 0;
}

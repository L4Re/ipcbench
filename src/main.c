/* Small and simple L4Re IPC benchmark program */
/* by Adam Lackorzynski <adam@l4re.org> */
/* License: MIT */

#include <pthread-l4.h>
#include <assert.h>
#include <stdio.h>

#include <time.h>

#include <l4/sys/ipc.h>

#if defined(__amd64__) || defined(__i686__)
#include <l4/util/rdtsc.h>
#define UNIT_TYPE l4_uint64_t
#define UNIT_NAME "cpu-cycles"
#define PREPARE() do {} while (0)
#define TAKE_TIME_(v) v = l4_rdpmc((1 << 30) | 2)
#define DIFF(start, end) ((end) - (start))

#elif defined(__aarch64__)

#define UNIT_TYPE l4_uint64_t
#define UNIT_NAME "cpu-cycles"
#define PREPARE() do { asm volatile ("msr PMCCFILTR_EL0, %0" : : "r" (1 << 27)); } while (0) // Enable counting in EL2 too
#define TAKE_TIME(v) asm volatile ("mrs %0, PMCCNTR_EL0" : "=r"(v))
#define DIFF(start, end) ((end) - (start))

#elif defined(__arm__)

#define UNIT_TYPE l4_uint64_t
#define UNIT_NAME "cpu-cycles"
#define PREPARE() do { asm volatile ("mcr p15, 0, %0, c14, c15, 3" : : "r" (1 << 27)); } while (0) // Enable counting in EL2 too
#define TAKE_TIME(v) asm volatile ("mrc p15, 0, %0, c9, c13, 0" : "=r" (v))
#define DIFF(start, end) ((end) - (start))

#else
/* Generic */
#define UNIT_TYPE struct timespec
#define UNIT_NAME "ns"
#define PREPARE() do {} while (0)
#define TAKE_TIME(v) clock_gettime(CLOCK_REALTIME, &v)
#define DIFF(start, end) (end.tv_sec * 1000000000 + end.tv_nsec \
                           - start.tv_sec * 1000000000 + start.tv_nsec)
#endif

enum { Enable_return_checks = 1 };

static l4_cap_idx_t th_cap_b = L4_INVALID_CAP;

/* This thread is initiating IPC */
static void *fn_a(void *)
{
  /* Wait for partner thread to be ready */
  while (!l4_is_valid_cap(th_cap_b))
    l4_ipc_sleep_ms(1);

  l4_utcb_t *utcb = l4_utcb();
  l4_msgtag_t tag = l4_msgtag(0, 0, 0, 0);

  enum { Num_rounds = 300000, Num_IPCs = Num_rounds * 2 };

  PREPARE();

  l4_msgtag_t r;
  UNIT_TYPE start, end;
  TAKE_TIME(start);
  for (int i = 0; i < Num_rounds; ++i)
    {
      r = l4_ipc_call(th_cap_b, utcb, tag, L4_IPC_NEVER);
      if (Enable_return_checks && l4_ipc_error(r, utcb))
        printf("fn_a: ipc err\n");
    }
  TAKE_TIME(end);

  UNIT_TYPE diff = DIFF(start, end);

  printf("done %d IPCs in %lld " UNIT_NAME ", %lld " UNIT_NAME "/IPC\n",
         Num_IPCs, diff, diff / Num_IPCs);

  return NULL;
}

/* This thread is replying to IPC */
static void *fn_b(void *)
{
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

  int r;

  r = pthread_attr_init(&attr);
  assert(r == 0);

  r = pthread_create(&thread_a, &attr, fn_a, NULL);
  assert(r == 0);
  r = pthread_create(&thread_b, &attr, fn_b, NULL);
  assert(r == 0);

  void *retval;
  r = pthread_join(thread_a, &retval);
  assert(r == 0);

  return 0;
}

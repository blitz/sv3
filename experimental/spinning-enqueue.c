
/* A bit of benchmarking code to see how expensive concurrent enqueue
 *   into vring.used is.
 */

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#define UNLIKELY(x) __builtin_expect(!!(x), 0)

static int reservation      = 0;
static int globally_visible = 0;

int enqueue(unsigned data)
{
  /* Reserve a slot */
  int local = __atomic_add_fetch(&reservation, 1,
                                 __ATOMIC_ACQUIRE);

  /* Do something with our reservation, e.g. fill in the
     descriptor. */
  asm volatile ("nop"
                :
                : "rm" (local), "rm" (data)
                : "memory");

  /* Spin until previous enqueuers are done. */
  while (UNLIKELY(local-1 != __atomic_load_n(&globally_visible,
                                             __ATOMIC_RELAXED)))
    /* RELAX */;

  /* Update globally visible value. */
  __atomic_store_n(&globally_visible, local,
                   __ATOMIC_RELEASE);
}

int main()
{
  const unsigned repeat = 10000;

  for (int j = 10; j > 0; j--) {
    uint64_t start = __builtin_ia32_rdtsc();
    for (int i = repeat; i > 0; i--) {
      enqueue(i);

    } 
    uint64_t end = __builtin_ia32_rdtsc();

    printf("%f\n", (double)(end - start) / repeat);
  }

}

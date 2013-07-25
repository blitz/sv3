/* Copyright (C) 2013, Julian Stecklina <jsteckli@os.inf.tu-dresden.de> */
/* Economic rights: Technische Universitaet Dresden (Germany) */

/* This file is part of sv3. */

/* sv3 is free software: you can redistribute it and/or modify it */
/* under the terms of the GNU General Public License version 2 as */
/* published by the Free Software Foundation. */

/* sv3 is distributed in the hope that it will be useful, but */
/* WITHOUT ANY WARRANTY; without even the implied warranty of */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU */
/* General Public License version 2 for more details. */


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

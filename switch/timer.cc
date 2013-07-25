// Copyright (C) 2013, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
// Economic rights: Technische Universitaet Dresden (Germany)

// This file is part of sv3.

// sv3 is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

// sv3 is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License version 2 for more details.


#include <timer.hh>
#include <cstdio>
#include <cstdlib>

#include <cpuid.h>

namespace Switch {

  uint64_t cycles_per_second()
  {
    static uint64_t cps = 0;

    if (cps) return cps;

    FILE *f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r");
    unsigned long long freq;
    if (not f or (1 != fscanf(f, "%llu", &freq))) {
      perror("unable to get CPU frequency");
      abort();
    }
    freq *= 1000;		// It's in kHz.

    printf("CPU frequency %llu\n", freq);
    fclose(f);

    unsigned level = 0x80000007;
    unsigned eax, ebx, ecx, edx = 0;
    __get_cpuid(level, &eax, &ebx, &ecx, &edx);
    if (not (edx & (1 << 8 /* invariant TSC */))) {
      fprintf(stderr, "TSC is not invariant!\n");
      abort();
    }

    return (cps = freq);
  }

}

// EOF

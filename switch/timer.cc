
#include <timer.hh>
#include <cstdio>
#include <cstdlib>

#include <cpuid.h>

namespace Switch {

  uint64_t cycles_per_second()
  {
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

    return freq;
  }

}

// EOF


#pragma once


#include <util.hh>
#include <cinttypes>
#include <cstdio>

namespace Switch {

  uint64_t cycles_per_second();

  /// A cheap timer that is intended to be used to measure rough time
  /// differences. This timer is not completely precise, so don't use
  /// this for benchmarking.
  class Timer {
    uint64_t _cycles_per_tick;

    uint64_t _start_time;

  public:

    void arm() { _start_time = rdtsc(); }

    bool elapsed(unsigned periods) {
      return (rdtsc() - _start_time) >= _cycles_per_tick*periods;
    }

    Timer(uint64_t ticks_per_second)
      : _cycles_per_tick(double(cycles_per_second()) / ticks_per_second)
    {
      arm();
    }
  };

}

// EOF


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
    uint64_t _period_cycles;
    uint64_t _end_time;

  public:

    void arm(uint64_t now = rdtsc())
    { 
      _end_time = now + _period_cycles;
    }

    bool elapsed(uint64_t now = rdtsc()) {
      return now > _end_time;
    }

    Timer(unsigned periods, unsigned frequency)
      : _period_cycles(double(cycles_per_second()) * periods / frequency),
        _end_time(0)
    {}
  };

}

// EOF

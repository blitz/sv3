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
      : _period_cycles((double(cycles_per_second()) * periods) / frequency),
        _end_time(0)
    {}
  };

}

// EOF

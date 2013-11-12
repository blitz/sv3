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
#include <fstream>
#include <sstream>
#include <boost/algorithm/string.hpp>

#include <cpuid.h>

namespace Switch {

  static bool tsc_invariant()
  {
    unsigned level = 0x80000007;
    unsigned eax, ebx, ecx, edx = 0;
    __get_cpuid(level, &eax, &ebx, &ecx, &edx);
    return (edx & (1 << 8 /* invariant TSC */));
 }

  uint64_t cycles_per_second()
  {
    static uint64_t cps = 0;
    if (cps) return cps;

    if (not tsc_invariant()) {
      fprintf(stderr, "TSC is not invariant!\n");
      abort();
    }

    // XXX Linux specific
    std::ifstream cpuinfo_file("/proc/cpuinfo");

    std::string line;
    while (std::getline(cpuinfo_file, line)) {
      std::stringstream line_str(line);
      std::string key, value;

      // Generates scary bloated code and depends on input locale ...
      std::getline(line_str, key, ':');
      boost::algorithm::trim(key);

      std::getline(line_str, value);
      boost::algorithm::trim(value);

      if (key.compare("cpu MHz") == 0) {
	float v = std::stof(value);
	cps = v * 1000000;
	break;
      }
    }

    if (cps == 0) {
      perror("unable to get CPU frequency");
      abort();
    }

    printf("CPU frequency %" PRIu64 "\n", cps);

    return cps;
  }

}

// EOF

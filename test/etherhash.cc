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


#include <hash/ethernet.hh>
#include <util.hh>

#include <cstdlib>
#include <cstdio>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <set>

using namespace Ethernet;
using std::set;

int main()
{
  constexpr unsigned tries = 1024*10;
  set<uint32_t> hashset;
  set<uint32_t> hashrset;

  int       devr = open("/dev/urandom", O_RDONLY);

  if (devr < 0) {
    perror("open");
    return EXIT_FAILURE;
  }

  Address a;
  for (unsigned i = 0; i < tries; i++) {
    if (read(devr, a.byte, sizeof(a.byte)) != sizeof(a.byte)) {
      perror("read");
      return EXIT_FAILURE;
    }
    uint32_t h = hash(a);
    hashset.insert(h);
    hashrset.insert(h % (2 << 16));
  }

  printf("full    %2.4lf\n", 1 - double(hashset.size())/tries); 
  printf("reduced %2.4lf\n", 1 - double(hashrset.size())/tries); 

  uint64_t start = rdtsc();
  for (unsigned i = 0; i < 1024; i++) {
    UNUSED volatile uint32_t h;
    asm ("" : "+m" (a));
    h = hash(a);
    asm ("" : "+m" (a));
    h = hash(a);
    asm ("" : "+m" (a));
    h = hash(a);
    asm ("" : "+m" (a));
    h = hash(a);
  }
  uint64_t end = rdtsc();
  printf("%.4lf cycles\n", double(end - start)/(4*1024));

  return 0;
}

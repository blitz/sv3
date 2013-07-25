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


#include <header/ethernet.hh>
#include <cstdio>

namespace Ethernet {

  const char *Address::to_str() const
  {
    static __thread char buf[24];
    uint8_t const *b = byte;
    snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
             b[0], b[1], b[2], b[3], b[4], b[5]);
    return buf;
  }

  uint32_t hash(Address const &addr)
  {
    uint32_t r1 = 0;
    asm ("crc32l %1, %0\n"
         "crc32w %2, %0\n"
         : "+&r" (r1)
         : "m" (addr._w1), "m" (addr._w2));
    return r1;
  }
}

// EOF

#pragma once

#include <header/ethernet.hh>
#include <util.hh>

#include <cstdio>
namespace Ethernet {

  // Hash a MAC address by creatively using CRC32.
  static inline uint32_t hash(Address const &addr) {
    uint32_t r1 = 0;
    asm ("crc32w  %1, %0\n"
         "crc32l  %2, %0\n"
         : "+&r" (r1)
         : "m" (addr),
           // This could be more straightforward if the "o" constraint
           // wasn't broken.
           "m" (addr.byte[2]));
    return r1;
  }

}

// EOF

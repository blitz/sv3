#pragma once

#include <header/ethernet.hh>
#include <util.hh>

namespace Ethernet {

  // Hash a MAC address by creatively using CRC32.
  static inline uint32_t hash(Address const &addr) {
    uint32_t r1, r2;
    asm ("crc32l  (%2), %0\n"
         "crc32l 2(%2), %1\n"
         : "=r" (r1), "=r" (r2)
         : "r" (addr.byte));
    return r1 ^ r2;
  }

}

// EOF

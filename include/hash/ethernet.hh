#pragma once

#include <header/ethernet.hh>
#include <util.hh>

#include <cstdio>
namespace Ethernet {

  // Hash a MAC address by creatively using CRC32.
  static inline uint32_t hash(Address const &addr) {
    uint32_t r1;
    asm ("crc32w  (%1), %0\n"
         "crc32l 2(%1), %0\n"
         : "=r" (r1)
           // Need reference to content of addr otherwise the compiler
           // thinks we only depend on the poiner value.
         : "r" (addr.byte), "m" (addr));
    return r1;
  }

}

// EOF

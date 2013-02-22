#pragma once

#include <header/ethernet.hh>
#include <util.hh>

#include <cstdio>
namespace Ethernet {

  // Hash a MAC address by creatively using CRC32.
  static inline uint32_t hash(Address const &addr) {
    uint32_t r1 = 0;
    asm ("crc32l %1, %0\n" : "+r" (r1) : "m" (addr._w1));
    asm ("crc32w %1, %0\n" : "+r" (r1) : "m" (addr._w2));
    return r1;
  }

}

// EOF

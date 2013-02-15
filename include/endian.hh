#pragma once

#include <cstdint>

namespace Endian {

  static inline uint16_t bswap16(uint16_t value) { asm ("xchg %b0, %h0" : "+Q"(value)); return value; }
  static inline uint32_t bswap(uint32_t value) { asm ("bswap %0" : "+r"(value)); return value; }
  static inline uint64_t bswap(uint64_t value) {
    return static_cast<uint64_t>(bswap(static_cast<uint32_t>(value)))<<32
      | bswap(static_cast<uint32_t>(value >> 32));
  }

  static inline unsigned long bswapl(unsigned long value) {
    #if    __SIZEOF_LONG__ == 8
    return bswap(static_cast<uint64_t>(value));
    #elif  __SIZEOF_LONG__ == 4
    return bswap(static_cast<uint32_t>(value));
    #else
    #error Long?
    #endif
  }

}

// EOF

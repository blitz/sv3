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

  // For use in headers

  constexpr uint16_t const_hton16(uint16_t v) {
    return (v >> 8) | (v << 8);
  }

}

// EOF

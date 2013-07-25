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
#include <compiler.h>

#include <hash/onescomplement.hh>
#include <header/ipv4.hh>

namespace TCP {

  struct PACKED Header {
    uint16_t src;
    uint16_t dst;
    uint32_t seq;
    uint32_t ack;

    uint8_t  ns  : 1;
    uint8_t  rsv : 3;
    uint8_t  off : 4;

    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;

    template<class IP>
    bool checksum_ok(IP const *ip) const {
      uint16_t      plen  = ip->payload_length(); // Helping the common subexpression elimination along...
      unsigned long state = ip->pseudo_checksum();
      uint16_t res = ~OnesComplement::fold(OnesComplement::add(state, OnesComplement::checksum(reinterpret_cast<uint8_t const *>(this),
                                                                                               plen)));
      return res == 0;
    }
  };

}

// EOF

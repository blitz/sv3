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
#include <header/tcp.hh>

namespace IPv6 {

  struct PACKED Address {
    uint8_t byte[16];
  };

  enum struct Proto : uint8_t {
    ICMP = 0x01,
    TCP  = 0x06,
    UDP  = 0x11,
  };

  struct PACKED Header {
    uint32_t version_class_label;

    uint16_t _payload_length;
    Proto    next_header;
    uint8_t  hop_limit;
    
    Address  src;
    Address  dst;

    union Payload {
      TCP::Header tcp;
    };

    Payload _payload[];

    // Accessor functions to make IPv6 header compatible to IPv4 one.

    uint8_t        version() const        { return version_class_label >> 4; }
    Payload       *payload()              { return _payload; }
    Payload const *payload() const        { return _payload; }
    uint16_t       payload_length() const { return Endian::bswap16(_payload_length); }

    unsigned long  pseudo_checksum() const {
      // XXX Wrong with option headers present, can be greatly optimized!
      unsigned long state = OnesComplement::checksum(src.byte, sizeof(Address));
      state = OnesComplement::add(state, OnesComplement::checksum(dst.byte, sizeof(Address)));
      state = OnesComplement::add(state, Endian::bswapl(static_cast<unsigned long>(payload_length() + uint8_t(next_header))));
      return state;
    }

  };

  static_assert(sizeof(Header) == 40, "IPv6 header layout broken");

}

// EOF

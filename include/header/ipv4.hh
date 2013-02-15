#pragma once

#include <cstdint>
#include <compiler.h>

#include <checksum/onescomplement.hh>

namespace IPv4 {

  struct PACKED Address {
    uint8_t byte[4];
  };

  enum struct Proto : uint8_t {
    ICMP = 0x01,
    TCP  = 0x06,
    UDP  = 0x11,
  };

  struct PACKED Header {
    // GCC bit order weirdness.
    uint8_t  ihl     : 4;
    uint8_t  version : 4;

    uint8_t  tos;
    uint16_t len;
    uint16_t id;
    
    uint16_t frag_offset : 13;
    uint16_t flags       : 3;

    uint8_t  ttl;
    Proto    proto;
    uint16_t checksum;

    Address  src;
    Address  dst;
    
    bool checksum_ok() const {
      uint8_t const *buf = reinterpret_cast<uint8_t const *>(this);
      uint16_t sum       = ~OnesComplement::fold(OnesComplement::checksum(buf, ihl * 4));
      return 0 == sum;
    }
  };

}

// EOF

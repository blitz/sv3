#pragma once

#include <cstdint>
#include <compiler.h>

#include <checksum/onescomplement.hh>

namespace TCP {
  class Header;
}

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

    uint32_t options[];
    
    bool checksum_ok() const {
      uint8_t const *buf = reinterpret_cast<uint8_t const *>(this);
      uint16_t sum       = ~OnesComplement::fold(OnesComplement::checksum(buf, ihl * 4));
      return 0 == sum;
    }

    // Returns payload length in host byte order.
    uint16_t payload_length() const {
      return Endian::bswap16(len) - ihl*4;
    }

    /// Returns an unfolded checksum for the TCP pseudo header.
    unsigned long checksum_tcp_pseudo() const {
      // Initial state covers TCP length and IP protocol.
      unsigned long state = static_cast<unsigned long>(Endian::bswap(static_cast<uint32_t>(Proto::TCP) + payload_length()));
      // Now add both IP addresses
#if      __SIZEOF_LONG__ == 8
      state = OnesComplement::add(state, *reinterpret_cast<unsigned long const *>(&src));
#elif __SIZEOF_LONG__ == 4
      state = OnesComplement::add(state,
                                  *reinterpret_cast<unsigned long const *>(&src),
                                  *reinterpret_cast<unsigned long const *>(&dst));
#else
#error long?
#endif
      return state;
    }

    TCP::Header       *tcp()       { return reinterpret_cast<TCP::Header       *>(options + ihl - 5); }
    TCP::Header const *tcp() const { return reinterpret_cast<TCP::Header const *>(options + ihl - 5); }
  };

}

// EOF

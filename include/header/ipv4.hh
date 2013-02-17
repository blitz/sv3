#pragma once

#include <cstdint>
#include <compiler.h>

#include <checksum/onescomplement.hh>
#include <header/tcp.hh>

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
    uint8_t  ihl      : 4;
    uint8_t  _version : 4;

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

    union Payload {
      TCP::Header tcp;
    };

    uint8_t        version() const { return _version; }
    Payload       *payload()       { return reinterpret_cast<Payload       *>(options + ihl - 5); }
    Payload const *payload() const { return reinterpret_cast<Payload const *>(options + ihl - 5); }

    /// Returns an unfolded checksum for the TCP/UDP pseudo header.
    unsigned long pseudo_checksum() const {
      // Initial state covers TCP length and IP protocol.
      unsigned long state = static_cast<unsigned long>(Endian::bswap(static_cast<uint32_t>(proto) + payload_length()));
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
  };

  static_assert(sizeof(Header) == 20, "IPv4 header layout broken");
}

// EOF

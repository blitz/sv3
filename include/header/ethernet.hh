#pragma once

#include <cstdint>
#include <cstring>
#include <compiler.h>

#include <endian.hh>
#include <header/ipv4.hh>
#include <header/ipv6.hh>

namespace Ethernet {

  struct PACKED Address {
    uint8_t byte[6];

    bool        is_multicast() const { return byte[0] & 1; }
    const char *to_str()       const;

    Address() : byte() {}
    Address(uint8_t a1, uint8_t a2, uint8_t a3, uint8_t a4, uint8_t a5, uint8_t a6) { 
      byte[0] = a1; byte[1] = a2;
      byte[2] = a3; byte[3] = a4;
      byte[4] = a5; byte[5] = a6;
    }
  };

  static inline bool operator==(Address const &a, Address const &b)
  {
    return 0 == memcmp(a.byte, b.byte, sizeof(Address::byte));
  }

  enum struct Ethertype : uint16_t {
    IPV4 = Endian::const_hton16(0x0800),
    IPV6 = Endian::const_hton16(0x86DD),
  };

  struct PACKED Header {
    Address   dst;
    Address   src;
    Ethertype type;

    union {
      IPv4::Header ipv4[];
      IPv6::Header ipv6[];
    };
  };

}

// EOF

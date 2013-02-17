#pragma once

#include <cstdint>
#include <compiler.h>

#include <endian.hh>
#include <header/ipv4.hh>
#include <header/ipv6.hh>

namespace Ethernet {

  struct PACKED Address {
    uint8_t byte[6];
  };

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

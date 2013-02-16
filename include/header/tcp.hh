#pragma once

#include <cstdint>
#include <compiler.h>

#include <checksum/onescomplement.hh>
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
      unsigned long state = ip->checksum_tcp_pseudo();
      uint16_t res = ~OnesComplement::fold(OnesComplement::add(state, OnesComplement::checksum(reinterpret_cast<uint8_t const *>(this),
                                                                                               ip->payload_length())));
      return res == 0;
    }
  };

}

// EOF

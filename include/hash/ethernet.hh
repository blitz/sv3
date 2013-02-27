#pragma once

#include <header/ethernet.hh>
#include <util.hh>

#include <cstdio>
namespace Ethernet {

  // Hash a MAC address by creatively using CRC32.
  uint32_t hash(Address const &addr);

}

// EOF

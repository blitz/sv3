
#include <header/ethernet.hh>
#include <cstdio>

namespace Ethernet {

  const char *Address::to_str() const
  {
    static __thread char buf[24];
    uint8_t const *b = byte;
    snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
             b[0], b[1], b[2], b[3], b[4], b[5]);
    return buf;
  }

  uint32_t hash(Address const &addr)
  {
    uint32_t r1 = 0;
    asm ("crc32l %1, %0\n"
         "crc32w %2, %0\n"
         : "+&r" (r1)
         : "m" (addr._w1), "m" (addr._w2));
    return r1;
  }
}

// EOF


#include <functional>
#include <header/ethernet.hh>

struct iovec;

namespace Switch {

  struct Packet {
    constexpr static unsigned MAX_FRAGMENTS = 16;

    uint8_t const *fragment[MAX_FRAGMENTS];
    uint16_t       fragment_length[MAX_FRAGMENTS];

    /// Length of packet in bytes.
    uint16_t packet_length;

    /// Number of fragments.
    uint8_t  fragments;

    std::function<void(Packet &p)> callback;

    Ethernet::Header const &ethernet_header() const
    {
      assert(fragments > 0 and
             fragment_length[0] >= sizeof(Ethernet::Header));

      return *reinterpret_cast<Ethernet::Header const *>(fragment[0]);
    }

    void to_iovec(struct iovec *iov) const;
  };

}

// EOF

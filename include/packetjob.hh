
#include <functional>
#include <header/ethernet.hh>

namespace Switch {

  class Port;

  struct Packet {
    constexpr static unsigned MAX_FRAGMENTS = 256;

    uint8_t  *fragment[MAX_FRAGMENTS];
    uint16_t  fragment_length[MAX_FRAGMENTS];

    uint16_t packet_length;	// Length of packet in bytes
    uint8_t  fragments;		// Number of fragments
    Port    *src_port;		// Port this packet originated

    // Port-private data. Set in src_port->poll() and read in
    // src_port->mark_done(), effectively implementing a poor man's
    // closure.
    union {
      struct {
	unsigned index;
      } virtio;
    };

    Ethernet::Header const &ethernet_header() const
    {
      assert(fragments > 0 and
             fragment_length[0] >= sizeof(Ethernet::Header));

      return *reinterpret_cast<Ethernet::Header const *>(fragment[0]);
    }

    void copy_from(Packet const &src);

    Packet(Port *src_port)
      : packet_length(0), fragments(0),
	src_port(src_port)
    { }
  };

}

// EOF

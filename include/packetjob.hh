
#include <functional>

namespace Switch {

  struct Packet {
    constexpr static unsigned MAX_FRAGMENTS = 8;

    uint8_t  *fragment[MAX_FRAGMENTS];
    uint16_t  fragment_length[MAX_FRAGMENTS];

    /// Length of packet in bytes.
    uint16_t packet_length;

    /// Number of fragments.
    uint8_t  fragments;    
  };

  /// A bunch of packets to the same destination.
  class PacketJob {

  public:
    typedef std::function<void(Packet const &)> PacketFn;
    virtual void do_packets(PacketFn f) const = 0;

    virtual Ethernet::Header const &ethernet_header() const = 0;

    virtual ~PacketJob() { }
  };


  /// A job consisting only of a single packet.
  class SinglePacketJob : public PacketJob {
  public:
    Packet packet;

    virtual void do_packets(PacketFn f) const override
    { f(packet); }

    virtual Ethernet::Header const &ethernet_header() const override {
      assert(packet.fragments >= 1 and 
             packet.fragment_length[0] > sizeof(Ethernet::Header));

      return *reinterpret_cast<Ethernet::Header const *>(packet.fragment[0]);
    }

    SinglePacketJob() {}
    SinglePacketJob(Packet const &p) : packet(p) {}
  };


}

// EOF

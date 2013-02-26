
#include <cstdio>
#include <algorithm>


#include <hash/ethernet.hh>
#include <switch.hh>
#include <listener.hh>
#include <tapport.hh>

class TestPort : public Switch::Port {
protected:
  constexpr static unsigned PACKET_LEN = 1512;
  Switch::SinglePacketJob _sp;
  bool _has_packet;
  union {
    uint8_t                 _data[PACKET_LEN];
    Ethernet::Header        _hdr;
  };

public:
  void receive(Port &src_port, Switch::PacketJob const &pj) override
  {
    logf("Packet job from '%s'.", src_port.name());

    pj.do_packets([&](Switch::Packet const &p) {
        if (p.fragments != 1) {
          logf("Complicated packet... Skipping.\n");
          return;
        }

        // Copy payload without destroying our header.
        memcpy(_data + sizeof(Ethernet::Header), 
               p.fragment[0] + sizeof(Ethernet::Header),
               std::min(PACKET_LEN - sizeof(Ethernet::Header),
                        p.fragment_length[0] - sizeof(Ethernet::Header)));
        _has_packet = true;
      });
  }

  Switch::PacketJob *poll() override
  {
    if (_has_packet) {
      _has_packet = false;
      return &_sp;
    } else {
      return nullptr;
    }
  }

  // GCC 4.8: using Port::Port;
  TestPort(Switch::Switch &sw, char const *name,
           Ethernet::Address src,
           Ethernet::Address dst,
           bool start)
    : Port(sw, name), _has_packet(start), _data()
  {
    _hdr.src = src;
    _hdr.dst = dst;

    _sp.packet.fragments          = 1;
    _sp.packet.fragment[0]        = _data;
    _sp.packet.fragment_length[0] = PACKET_LEN;
    _sp.packet.packet_length      = PACKET_LEN;
  }
};


int main()
{
  Switch::Switch   sv3;
  Switch::Listener listener(sv3);

  // Ethernet::Address a1(0xFE, 0x12, 0, 0, 0, 1);
  // Ethernet::Address a2(0xFE, 0x12, 0, 0, 0, 2);
  // TestPort  tport1(sv3, "test1", a1, a2, false);
  // TestPort  tport2(sv3, "test2", a2, a1, true);

  // Switch::TapPort egress(sv3, "/dev/tap4");

  sv3.loop();

  return 0;
}

// EOF

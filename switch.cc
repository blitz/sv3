
#include <switch.hh>
#include <hash/ethernet.hh>

#include <cstdio>

class TestPort : public Switch::Port {
  Switch::SinglePacketJob _sp;

  union {
    uint8_t                 _data[64];
    Ethernet::Header        _hdr;
  };

public:
  void receive(Port &src_port, Switch::PacketJob const &pj)
  {
    logf("Packet from '%s'. Drop.", src_port.name());
  }

  Switch::PacketJob *poll()
  {
    _sp.packet.packet_length = sizeof(_data);
    _sp.packet.fragments     = 1;

    _sp.packet.fragment[0]        = _data;
    _sp.packet.fragment_length[0] = _sp.packet.packet_length;

    return &_sp;
  }

  // GCC 4.8: using Port::Port;
  TestPort(Switch::Switch &sw, char const *name,
           Ethernet::Address src,
           Ethernet::Address dst)
    : Port(sw, name), _data()
  {
    _hdr.src = src;
    _hdr.dst = dst;
  }
};


int main()
{
  Switch::Switch sv3;

  Ethernet::Address a1(0xFE, 0x12, 0, 0, 0, 1);
  Ethernet::Address a2(0xFE, 0x12, 0, 0, 0, 2);
  TestPort  tport1(sv3, "test1", a1, a2);
  TestPort  tport2(sv3, "test2", a2, a1);

  sv3.loop();

  return 0;
}

// EOF

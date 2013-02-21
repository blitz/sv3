
#include <switch.hh>

namespace Switch {

  void BroadcastPort::receive(Port &src_port, PacketJob const &pj)
  {
    pj.do_packets([&](Packet const &p) {
        for (Port *dst_port : _switch.ports()) {
          if (dst_port != &src_port and
              dst_port != this)
            dst_port->receive(*this, SinglePacketJob(p));
        }
      });
  }

  PacketJob *BroadcastPort::poll()
  {
    return nullptr;
  }

}

// EOF

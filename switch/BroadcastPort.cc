
#include <switch.hh>

namespace Switch {

  void BroadcastPort::receive(PacketJob &pj)
  {
    pj.do_packets([&](Packet &p) { logf("Packet..."); });
  }

}

// EOF

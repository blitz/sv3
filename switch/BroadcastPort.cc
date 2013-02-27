
#include <switch.hh>

namespace Switch {

  void BroadcastPort::receive(Port &src_port, Packet &p)
  {
    for (Port *dst_port : _switch.ports()) {
      if (dst_port != &src_port and
          dst_port != this)
        dst_port->receive(*this, p);
    }
  }

  bool BroadcastPort::poll(Packet &p)
  {
    return false;
  }

}

// EOF

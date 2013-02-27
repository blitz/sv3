// 

#include <qpport.hh>
#include <listener.hh>
#include <sv3-client.h>

namespace Switch {

  void QpPort::receive(UNUSED Port &src_port, UNUSED PacketJob const &pj)
  {
  }

  PacketJob *QpPort::poll()
  {
    return nullptr;
  }

  QpPort::QpPort(Switch &sw, char const *name, Session &session, uint64_t qp)
    : Port(sw, name), _session(session)
  {
    this->_qp = session.translate_ptr<Sv3QueuePair>(qp);
    session._ports.push_front(this);
  }

}

// EOF

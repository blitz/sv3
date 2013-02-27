// 

#include <exception>
#include <switch.hh>

struct Sv3QueuePair;

namespace Switch {

  class Session;

  class QpPort : public Port {
    // Client session. Includes memory map.
    Session      &_session;

    Sv3QueuePair *_qp;

  public:

    virtual void receive(Port &src_port, PacketJob const &pj) override;
    virtual PacketJob *poll() override;


    QpPort(Switch &sw, char const *name, Session &session, uint64_t qp);
  };

}

// EOF

// 

#include <exception>
#include <switch.hh>
#include <sv3-client.h>

namespace Switch {

  struct Session;

  class QpPort : public Port {
    // Client session. Includes memory map.
    Session      &_session;
    Sv3QueuePair *_qp;

    // Trigger the clients event fd, if necessary.
    void notify();

  public:

    virtual void receive(Port &src_port, Packet &p) override;
    virtual bool poll(Packet &p) override;


    QpPort(Switch &sw, char const *name, Session &session, uint64_t qp);
  };

}

// EOF

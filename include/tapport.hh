// 

#include <exception>

#include <switch.hh>

namespace Switch {

  class TapPort : public Port {
    int             _fd;
    int             _header_size;
    SinglePacketJob _sp;
    uint8_t         _data[MAX_MTU];

  public:

    virtual void receive(Port &src_port, PacketJob const &pj) override;
    virtual PacketJob *poll() override;


    TapPort(Switch &sw, char const *devn);
  };

}

// EOF

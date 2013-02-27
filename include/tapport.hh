// 

#include <exception>

#include <switch.hh>

namespace Switch {

  class TapPort : public Port {
    static const size_t _buf_size = 2048;

    int      _fd;
    unsigned _header_size;

    uint8_t *_buf;

  public:

    virtual void receive(Port &src_port, Packet &p) override;
    virtual bool poll(Packet &p) override;


    TapPort(Switch &sw, char const *devn);
  };

}

// EOF

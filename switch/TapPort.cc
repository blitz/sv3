// 

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/if_tun.h>

#include <errno.h>

#include <tapport.hh>
#include <system_error>

namespace Switch {

  void TapPort::receive(Port &src_port, PacketJob const &pj)
  {
    struct iovec iov[Packet::MAX_FRAGMENTS];
    pj.do_packets([&](Packet const &p) {
        p.to_iovec(iov);
        ssize_t r = writev(_fd, iov, p.fragments);
        if (r < 0) {
          throw std::system_error(errno, std::system_category());
        }
      });

  }

  PacketJob *TapPort::poll()
  {
    ssize_t r = read(_fd, _data, sizeof(_data));
    if (r < 0) {
      if (errno == EWOULDBLOCK) return nullptr;
      throw std::system_error(errno, std::system_category());
    }

    logf("Read %zd bytes.", r);
    assert(r > _header_size); 
    _sp.from_buffer(_data + _header_size, r - _header_size);
    return &_sp;
  }

  TapPort::TapPort(Switch &sw, char const *dev)
    : Port(sw, dev), _data()
  {
    _fd = open(dev, O_RDWR | O_NONBLOCK);
    if (_fd < 0)
      throw std::system_error(errno, std::system_category());
   
    if (0 != ioctl(_fd, TUNGETVNETHDRSZ, &_header_size))
      throw std::system_error(errno, std::system_category());
  }
}

// EOF

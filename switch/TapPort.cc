// 

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/if_tun.h>

#define class _class
#include <linux/virtio_net.h>
#undef class

#include <errno.h>

#include <tapport.hh>
#include <system_error>

namespace Switch {

  void TapPort::receive(UNUSED Port &src_port, Packet &p)
  {
    struct iovec iov[Packet::MAX_FRAGMENTS];
    p.to_iovec(iov);

    // XXX Write virtio header?
    ssize_t r = writev(_fd, iov, p.fragments);
    if (r < 0) {
      throw std::system_error(errno, std::system_category());
    }
  }

  bool TapPort::poll(Packet &p)
  {
    if (not _buf)
      _buf = new uint8_t[_buf_size];

    ssize_t r = read(_fd, _buf, _buf_size);
    if (r < 0) {
      if (errno == EWOULDBLOCK) return false;
      throw std::system_error(errno, std::system_category());
    }

    assert(r > _header_size); 

    p.fragments          = 1;
    p.fragment[0]        = reinterpret_cast<uint8_t *>(_buf) + _header_size;
    p.fragment_length[0] = r - _header_size;
    p.packet_length      = r - _header_size;

    _buf = nullptr;
    p.callback = [=](Packet &p) { delete[] (p.fragment[0] - _header_size); };

    return true;
  }

  TapPort::TapPort(Switch &sw, char const *dev)
    : Port(sw, dev), _buf(nullptr)
  {
    _fd = open(dev, O_RDWR | O_NONBLOCK);
    if (_fd < 0)
      throw std::system_error(errno, std::system_category());

#ifdef NO_GETVNETHDRSZ
    _header_size = sizeof(virtio_net_hdr);
#else
    if (0 != ioctl(_fd, TUNGETVNETHDRSZ, &_header_size))
      throw std::system_error(errno, std::system_category());
#endif
    logf("Virtio header is %d bytes.", _header_size);
    if (_header_size < sizeof(virtio_net_hdr))
      logf("Virtio header is too small. Should be at least %zu bytes. Bad things will happen!",
           sizeof(virtio_net_hdr));
  }
}

// EOF

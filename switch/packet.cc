#include <switch.hh>
#include <sys/uio.h>

namespace Switch {

  void Packet::to_iovec(struct iovec *iov) const
  {
    for (unsigned i = 0; i < fragments; i++) {
      iov[i].iov_base = const_cast<uint8_t *>(fragment[i]);
      iov[i].iov_len  = fragment_length[i];
    }
  }

}

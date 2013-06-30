#include <packetjob.hh>

namespace Switch {

  void
  Packet::copy_from(Packet const &src)
  {
    Packet  &dst = *this;
    size_t   dst_i   = 0;
    uint8_t *dst_ptr = dst.fragment[0];
    size_t   dst_space = dst.fragment_length[0]; /* Space left in current
						    destination segment. */

    for (unsigned src_i = 0; src_i < src.fragments; src_i++) {
      uint8_t *src_ptr   = src.fragment[src_i];
      size_t   src_space = src.fragment_length[src_i];

      do {
	size_t chunk = std::min(dst_space, src_space);

	memcpy(dst_ptr, src_ptr, chunk);

	src_ptr    += chunk;
	src_space  -= chunk;
	dst_ptr    += chunk;
	dst_space  -= chunk;

	if (dst_space == 0) {
	  dst_i += 1;
	  if (UNLIKELY(dst_i >= dst.fragments)) {
	    // XXX Packet cropped.
	    return;
	  }

	  dst_ptr   = dst.fragment[dst_i];
	  dst_space = dst.fragment_length[dst_i];
	}
      } while (src_space);
    }
  }

}

// EOF

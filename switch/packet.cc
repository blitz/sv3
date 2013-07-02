#include <packetjob.hh>

namespace Switch {

  void
  Packet::copy_from(Packet const &src, virtio_net_hdr const *hdr)
  {
    Packet  &dst = *this;

    // Copy header
    assert(dst.fragment_length[0] == sizeof(*hdr));
    memcpy(dst.fragment[0], hdr, sizeof(*hdr));

    size_t   dst_i     = 1;
    uint8_t *dst_ptr   = dst.fragment[1];
    size_t   dst_space = dst.fragment_length[1]; /* Space left in current
						    destination segment. */

    for (unsigned src_i = 1; src_i < src.fragments; src_i++) {
      uint8_t *src_ptr   = src.fragment[src_i];
      size_t   src_space = src.fragment_length[src_i];

      do {
	size_t chunk = std::min(dst_space, src_space);

#if defined(__x86_64__) or defined(__i386__)
	unsigned ecx = chunk;
	asm volatile ("rep; movsb"
		      : "+D" (dst_ptr),
			"+S" (src_ptr),
			"+c" (ecx)
		      :
		      : "memory");
#else
#warning No optimized memcpy available.
	memcpy(dst_ptr, src_ptr, chunk);
	src_ptr    += chunk;
	dst_ptr    += chunk;
#endif

	src_space  -= chunk;
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

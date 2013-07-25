// Copyright (C) 2013, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
// Economic rights: Technische Universitaet Dresden (Germany)

// This file is part of sv3.

// sv3 is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

// sv3 is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License version 2 for more details.

#include <packetjob.hh>
#include <util.hh>

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
      uint8_t const *src_ptr   = src.fragment[src_i];
      size_t         src_space = src.fragment_length[src_i];

      do {
	size_t chunk = std::min(dst_space, src_space);

        movs<uint8_t>(dst_ptr, src_ptr, chunk);

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

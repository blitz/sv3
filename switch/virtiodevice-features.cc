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

#include <virtiodevice.hh>
#include <sstream>

namespace Switch {

#define FNAME(n) { VIRTIO_NET_F_ ## n, "" # n }
  static struct {
    unsigned    bit;
    char const *name;
    
  } virtio_net_feature_names[] {
    FNAME(CSUM),
    FNAME(GUEST_CSUM),
    FNAME(MAC),
    FNAME(GSO),
    FNAME(GUEST_TSO4),
    FNAME(GUEST_TSO6),
    FNAME(GUEST_ECN),
    FNAME(GUEST_UFO),
    FNAME(HOST_TSO4),
    FNAME(HOST_TSO6),
    FNAME(HOST_ECN),
    FNAME(HOST_UFO),
    FNAME(MRG_RXBUF),
    FNAME(STATUS),
    FNAME(CTRL_VQ),
    FNAME(CTRL_RX),
    FNAME(CTRL_VLAN),
    FNAME(CTRL_RX_EXTRA),
    FNAME(MQ),
    FNAME(CTRL_MAC_ADDR),
  };


  std::string VirtioDevice::features_to_string(uint32_t features)
  {
    std::stringstream out;
    for (unsigned bit = 0; bit < 32; bit++) {
      if (features & (1 << bit)) {
	for (auto r : virtio_net_feature_names)
	  if (r.bit == bit) {
	    out << " " << r.name;
	    goto continue_bit;
	  }
	out << " " << bit;
      }
    continue_bit:
      ;
    }
    return out.str();
  }


}

// EOF

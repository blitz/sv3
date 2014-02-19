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
#include <switch.hh>
#include <session.hh>
#include <config.hh>

#include <sstream>

namespace Switch {


  VirtioDevice::VirtioDevice(Session &session)
    : Port(session._sw, std::string("VirtIO ") + std::to_string(session._fd)),
      _session(session), _irq_fd(), online(false)
  {
    // Always announce guest features. Doesn't harm.
    host_features = (1 << VIRTIO_NET_F_GUEST_CSUM) 
      | (1 << VIRTIO_NET_F_MRG_RXBUF)
      | (1 << VIRTIO_NET_F_GUEST_TSO4)
      | (1 << VIRTIO_NET_F_GUEST_TSO6)
      | (1 << VIRTIO_NET_F_CSUM)
      | (1 << VIRTIO_NET_F_HOST_TSO4)
      | (1 << VIRTIO_NET_F_HOST_TSO6);
  }

}

// EOF

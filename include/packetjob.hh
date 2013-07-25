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


#include <functional>
#include <header/ethernet.hh>

#include <virtio-constants.h>

namespace Switch {

  class Port;

  struct Packet {
    constexpr static unsigned MAX_FRAGMENTS = 256;

    uint8_t  *fragment[MAX_FRAGMENTS];
    uint16_t  fragment_length[MAX_FRAGMENTS];

    uint32_t packet_length;	// Length of packet in bytes
    uint8_t  fragments;		// Number of fragments
    Port    *src_port;		// Port this packet originated

    // Port-private data. Set in src_port->poll() and read in
    // src_port->mark_done(), effectively implementing a poor man's
    // closure.
    union {
      struct {
	unsigned index;
      } virtio;
    };

    Ethernet::Header const &ethernet_header() const
    {
      assert(fragments > 1);
      assert(fragment_length[0] == sizeof(struct virtio_net_hdr));
      assert(fragment_length[1] >= sizeof(Ethernet::Header));

      return *reinterpret_cast<Ethernet::Header const *>(fragment[1]);
    }

    void copy_from(Packet const &src, virtio_net_hdr const *hdr);

    Packet(Port *src_port)
      : packet_length(0), fragments(0),
	src_port(src_port)
    { }
  };

}

// EOF

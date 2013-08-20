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

#pragma once

#include <cstdint>
#include <algorithm>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>

// From QEMU source
#include <hw/misc/externalpci.h>

#include <virtiodevice.hh>

namespace Switch {

  struct Region {
    uint64_t addr;
    uint64_t size;

    uint8_t *mapping;

    Region(uint64_t addr, uint64_t size, uint8_t *mapping)
      : addr(addr), size(size), mapping(mapping) {}
  };

  class RegionList {
    std::list<Region> _list;

  public:
    bool insert(Region const &r);

    template<typename P>
    P *translate_ptr(uint64_t addr) {
      return reinterpret_cast<P *>(translate_ptr(addr, sizeof(P)));
    }

    uint8_t *translate_ptr(uint64_t addr, size_t size)
    {
      if (addr + size <= addr) return nullptr;

      for (auto &r : _list) {
	if (r.addr <= addr and
	    addr + size < r.addr + r.size)
	  return r.mapping + (addr - r.addr);
      }

      return nullptr;
    }

    ~RegionList() {
      for (auto &r : _list)
	munmap(r.mapping, r.size);
    }
  };

  class Session {
    // XXX Write proper accessors.
  public:

    /// Switch instance.
    Switch      &_sw;

    /// Session file descriptor.
    int          _fd;

    /// Client socket address.
    sockaddr_un  _sa;

    RegionList   _regions;

    /// File descriptors we accepted that must be cleaned up on session
    /// termination.
    std::list<int>    _file_descriptors;

    VirtioDevice      _device;

    template<typename P>
    P       *translate_ptr(uint64_t addr)
    { return _regions.translate_ptr<P>(addr); }

    uint8_t *translate_ptr(uint64_t addr, size_t size)
    { return _regions.translate_ptr(addr, size); }

    /// Check for a message. Shouldn't block.
    bool poll();

    /// Insert a region into the region list.
    bool insert_region(Region const &r);

    bool handle_request(externalpci_req const &req,
			externalpci_res &res);

    void close_fd(int fd)
    {
      close(fd);

      auto it = std::find(_file_descriptors.begin(),
			  _file_descriptors.end(),
			  fd);
      assert(it != _file_descriptors.end());
      _file_descriptors.erase(it);
    }

    Session(Switch &sw, int fd, sockaddr_un sa) :
      _sw(sw), _fd(fd), _sa(sa), _regions(),
      _file_descriptors(),
      _device(*this)
    { }

    virtual ~Session();
  };

}


// EOF

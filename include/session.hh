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
      // Do they want to fool us?
      size_t size = sizeof(P);

      if (addr + size <= addr) return nullptr;

      for (auto &r : _list) {
	if (r.addr <= addr and
	    addr + size < r.addr + r.size)
	  return reinterpret_cast<P *>(r.mapping + (addr - r.addr));
      }

      return nullptr;
    }

    ~RegionList() {
      for (auto &r : _list)
	munmap(r.mapping, r.size);
    }
  };

  struct Session {
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
    P *translate_ptr(uint64_t addr) { return _regions.translate_ptr<P>(addr); }

    /// Check for a message. Shouldn't block.
    bool poll();

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

    ~Session();
  };

}


// EOF

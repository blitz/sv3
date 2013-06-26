//

#pragma once

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>

#include <list>
#include <thread>

#include <switch.hh>

#include <hw/misc/externalpci.h>

namespace Switch {

  struct Region {
    uint64_t addr;
    uint64_t size;

    uint8_t *mapping;
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

    template<typename P>
    P *translate_ptr(uint64_t addr) { return _regions.translate_ptr<P>(addr); }

    /// Check for a message. Shouldn't block.
    bool poll();

    externalpci_res handle_request(externalpci_req const &req);

    Session(Switch &sw, int fd, sockaddr_un sa) :
      _sw(sw), _fd(fd), _sa(sa), _regions(), _file_descriptors() { }
    ~Session();
  };


  class Listener {
    Switch     &_sw;

    int         _sfd;
    sockaddr_un _local_addr;

    std::thread _thread;

    std::list<Session *> _sessions;

    void thread_fun();
    void accept_session();

  public:

    // Create a listening socket for the switch through which it can
    // be controlled by sv3-remote. If force is set, the unix file
    // socket is unlinked prior to creating a new one.
    Listener(Switch &sw, bool force = false);
    ~Listener();
  };

}

// EOF

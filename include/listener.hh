// 

#pragma once

#include <sys/socket.h>
#include <sys/un.h>

#include <list>
#include <thread>

#include <switch.hh>

#include <sv3-client.h>
#include <sv3-messages.h>

namespace Switch {

  struct Region {
    uint64_t addr;
    uint64_t size;

    uint8_t *mapping;
  };

  struct Session {
    int          _fd;
    int          _event_fd;
    sockaddr_un  _sa;

    std::list<Region> _regions;
    std::list<Port *> _ports;

    template<typename P>
    P *translate_ptr(uint64_t p) {
      // XXX We don't care about length yet... p might cross a region
      // boundary.

      for (auto &r : _regions) {
        uint64_t tp = p - r.addr;
        if (tp < r.size)
          return reinterpret_cast<P *>(r.mapping + tp);
      }

      return nullptr;
    }

    Session() : _event_fd(0), _regions(), _ports() { }
  };


  class Listener {
    Switch     &_sw;

    int         _sfd;
    sockaddr_un _local_addr;

    std::thread _thread;

    std::list<Session> _sessions;

    void thread_fun();
    void close_session(Session &session);
    void accept_session();
    bool poll(Session &session);
    bool insert_region(Session &session, Region r);

    Sv3Response handle_request(Session &session, Sv3Request &req);

  public:

    // Pass a request over the given fd, which should be connected to
    // the switch instance.
    static Sv3Response call(int fd, Sv3Request const &req);

    // Create a listening socket for the switch through which it can
    // be controlled by sv3-remote. If force is set, the unix file
    // socket is unlnked prior to creating a new one.
    Listener(Switch &sw, bool force = false);
    ~Listener();
  };

}

// EOF

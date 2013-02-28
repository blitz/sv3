// 

#pragma once

#include <sys/socket.h>
#include <sys/un.h>

#include <list>

#include <switch.hh>


namespace Switch {

  struct ClientRequest {
    enum {
      PING,
      CREATE_PORT_TAP,
      MEMORY_MAP,
      CREATE_PORT_QP,
      EVENT_FD,
    } type;

    union {
      struct {
        // nothing...
      } ping;
      struct {
        char buf[32];
      } create_port_tap;
      struct {
        int      fd;

        uint64_t addr;
        uint64_t size;
        off_t    offset;
      } memory_map;
      struct {
        uint64_t qp;            // pointer
      } create_port_qp;
      struct {
        // fd needs to be at same offset as in memory_map
        int fd;
      } event_fd;
    };
  };

  struct ServerResponse {
    enum {
      STATUS,
      PONG,
    } type;
    union {
      struct {
        bool success;
      } status;
      struct {
      } pong;
    };
  };

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

    pthread_t   _thread;

    std::list<Session> _sessions;

    static void *thread_enter(void *);
    void thread_fun();
    void close_session(Session &session);
    void accept_session();
    bool poll(Session &session);
    bool insert_region(Session &session, Region r);

    ServerResponse handle_request(Session &session, ClientRequest &req);

  public:

    // Pass a request over the given fd, which should be connected to
    // the switch instance.
    static ServerResponse call(int fd, ClientRequest const &req);

    Listener(Switch &sw);
    ~Listener();
  };

}

// EOF

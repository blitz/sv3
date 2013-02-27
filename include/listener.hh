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
    } type;

    union {
      struct {
        // nothing...
      } ping;
      struct {
        char buf[32];
      } create_port_tap;
      struct {
        uint64_t addr;
        uint64_t size;

        int      fd;
        off_t    offset;
      } memory_map;
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
    sockaddr_un  _sa;

    std::list<Region> _regions;
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

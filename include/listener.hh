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
    } type;

    union {
      struct {
        // nothing...
      } ping;
      struct {
        char buf[32];
      } create_port_tap;
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


  class Listener {
    Switch     &_sw;

    int         _sfd;
    sockaddr_un _local_addr;

    pthread_t   _thread;

    struct Session {
      int         _fd;
      sockaddr_un _sa; 
    };

    std::list<Session> _sessions;

    static void *thread_enter(void *);
    void thread_fun();
    void close_session(Session &session);
    void accept_session();
    bool poll(Session &session);
    ServerResponse handle_request(Session &session, ClientRequest &req);

  public:

    static ServerResponse call(int fd, ClientRequest const &req);

    Listener(Switch &sw);
    ~Listener();
  };

}

// EOF

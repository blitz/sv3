// 

#pragma once

#include <switch.hh>

#include <list>

namespace Switch {

  class Listener {
    Switch   &_sw;
    int       _sfd;
    pthread_t _thread;

    struct Session {
      int         _fd;
      sockaddr_un _sa; 
    };

    std::list<Session> _sessions;

    static void *thread_enter(void *);
    void thread_fun();
    void accept_session();
    void poll(Session &session);

  public:

    Listener(Switch &sw);
    ~Listener();
  };

}

// EOF

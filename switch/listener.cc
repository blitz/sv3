

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <unistd.h>

#include <listener.hh>
#include <system_error>
#include <pthread.h>

namespace Switch {


  void *Listener::thread_enter(void *arg)
  {
    Listener *l = reinterpret_cast<Listener *>(arg);
    l->thread_fun();
    return nullptr;
  }

  void Listener::accept_session()
  {
    Session   new_s;
    socklen_t si = sizeof(new_s._sa);
    int     res = accept(_sfd, reinterpret_cast<sockaddr *>(&new_s._sa), &si);
    if (res < 0) {
      perror("accept");
      return;
    }

    if (si > sizeof(new_s._sa)) {
      fprintf(stderr, "sockaddr too large? %u vs %lu", si, sizeof(new_s._sa));
      return;
    }

    new_s._fd = res;
    _sessions.push_back(new_s);

    printf("New connection established.\n");
  }

  void Listener::poll(Session &session)
  {
    // Handle requests
  }

  void Listener::thread_fun()
  {

    do {
      fd_set fdset;
      FD_ZERO(&fdset);
      FD_SET(_sfd, &fdset);
      for (auto &s : _sessions)
        FD_SET(s._fd, &fdset);

      int res = select(1 + _sessions.size(), &fdset, NULL, NULL, NULL);
      if (res < 0) {
        perror("select");
        return;
      }

      if (FD_ISSET(_sfd, &fdset)) accept_session();

      for (auto &s : _sessions)
        if (FD_ISSET(s._fd, &fdset))
          poll(s);

    } while (true);
  }

  Listener::Listener(Switch &sw) : _sw(sw)
  {
    _sfd = socket(AF_LOCAL, SOCK_SEQPACKET, 0);
    if (_sfd < 0) throw std::system_error(errno, std::system_category());

    sockaddr_un addr;
    addr.sun_family = AF_LOCAL;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "/tmp/switch-%u", getpid());

    if (0 != bind(_sfd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)))
      throw std::system_error(errno, std::system_category());

    if (0 != listen(_sfd, 5))
      throw std::system_error(errno, std::system_category());

    int ret;
    if (0 != (ret = pthread_create(&_thread, NULL, &thread_enter, this)))
      throw std::system_error(ret, std::system_category());
  }

  Listener::~Listener()
  {
    close(_sfd);

    void *ret;
    pthread_join(_thread, &ret);
  }

}

// EOF

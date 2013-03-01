

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <unistd.h>

#include <listener.hh>
#include <system_error>
#include <pthread.h>
#include <tapport.hh>
#include <qpport.hh>

namespace Switch {


  void *Listener::thread_enter(void *arg)
  {
    Listener *l = reinterpret_cast<Listener *>(arg);
    l->thread_fun();
    return nullptr;
  }

  void Listener::close_session(Session &session)
  {
    close(session._fd);


    for (auto &p : session._ports)
      delete p;

    for (auto &r : session._regions)
      munmap(r.mapping, r.size);

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
      _sw.logf("sockaddr too large? %u vs %lu", si, sizeof(new_s._sa));
      return;
    }

    new_s._fd = res;
    _sessions.push_back(new_s);

    // _sw.logf("Incoming connection. Client %d.", new_s._fd);
  }

  bool Listener::insert_region(Session &session, Region r)
  {
    _sw.logf("Client %u mapped %016llx+%llx to %p.", session._fd, r.addr, r.size, r.mapping);

    // Overflow or empty region
    if (r.addr + r.size <= r.addr) return false;

    uint64_t r_end  = r .addr + r .size;
    for (auto &lr : session._regions) {
      uint64_t lr_end = lr.addr + lr.size;
      if (r.addr < lr_end and lr.addr < r_end) {
        _sw.logf("Incoming region [%016llx,%016llx] overlaps with [%016llx,%016llx]",
                 r.addr,  r.addr  + r.size  - 1,
                 lr.addr, lr.addr + lr.size - 1);
        return false;
      }
    }

    // Sorted insert
    auto it = session._regions.begin();
    for (; it != session._regions.end() and (*it).addr < r_end; ++it) {}
    session._regions.insert(it, r);

    return true;
  }

  Sv3Response Listener::handle_request(Session &session, Sv3Request &req)
  {
    Sv3Response resp;
    memset(&resp, 0, sizeof(resp));
    resp.type = SV3_RESP_STATUS;
    resp.status.success = true;

    switch (req.type) {
    case SV3_REQ_PING:
      break;
    case SV3_REQ_CREATE_PORT_TAP: {
      char name[sizeof(req.create_port_tap.buf) + 1];
      name[sizeof(req.create_port_tap.buf)] = 0;
      strncpy(name, req.create_port_tap.buf, sizeof(name));

      _sw.logf("Client %d wants to create a tap port for '%s'.", session._fd, name);        

      try {
        Port *p = new TapPort(_sw, name);
        p->enable();
      } catch (std::system_error &e) {
        _sw.logf("Could not construct tap port for '%s': %s", name, e.what());
        resp.status.success = false;
      }

      break;
    }
    case SV3_REQ_MEMORY_MAP: {
      Region r = { req.memory_map.addr,
                   req.memory_map.size,
                   reinterpret_cast<uint8_t *>(mmap(nullptr, req.memory_map.size, PROT_READ | PROT_WRITE, MAP_SHARED,
                                                    req.memory_map.fd, req.memory_map.offset)) };
      if (r.mapping == MAP_FAILED) {
        char err[128];
        strerror_r(errno, err, sizeof(err));
        _sw.logf("Could not map shared memory: %d %s", errno, err);
        resp.status.success = false;
        break;
      }

      resp.status.success = insert_region(session, r);
      break;
    }
    case SV3_REQ_CREATE_PORT_QP: {
      char buf[32];
      snprintf(buf, 32, "c%d qp", session._fd);
      Port *p = new QpPort(_sw, buf, session, req.create_port_qp.qp);
      p->enable();
    };
      break;
    case SV3_REQ_EVENT_FD:
      if (session._event_fd) close(session._event_fd);
      session._event_fd = req.event_fd.fd;
      break;
    default:
      resp.status.success = false;
      break;
    };

    return resp;
  }

  Sv3Response Listener::call(int fd, Sv3Request const &req)
  {
    Sv3Response resp;

    if (0 != sv3_call(fd, const_cast<Sv3Request *>(&req), &resp))
      throw std::system_error(errno, std::system_category());

    return resp;
  }

  bool Listener::poll(Session &session)
  {
    Sv3Request    req;
    struct msghdr hdr;
    struct iovec  iov = { &req, sizeof(req) };
    union {
      struct cmsghdr chdr;
      char           chdr_data[CMSG_SPACE(sizeof(int))];
    };

    hdr.msg_name       = NULL;
    hdr.msg_namelen    = 0;
    hdr.msg_iov        = &iov;
    hdr.msg_iovlen     = 1;
    hdr.msg_flags      = 0;
    hdr.msg_control    = &chdr;
    hdr.msg_controllen = CMSG_SPACE(sizeof(int));

    int res = recvmsg(session._fd, &hdr, 0);
    if (res < 0) {
      char err[128];
      strerror_r(errno, err, sizeof(err));
      _sw.logf("Got error from connection to client %d: %s", session._fd, err);
      goto do_close;
    }

    // Is our connection closed?
    if (res == 0) {
      // _sw.logf("Goodbye, client %u!", session._fd);
      goto do_close;
    }
    
    if (res != sizeof(req)) {
      _sw.logf("Client %3d violated protocol. %u %zu", session._fd, res, sizeof(req));
      goto do_close;
    }

    {
      cmsghdr *incoming_chdr = CMSG_FIRSTHDR(&hdr);
      if (incoming_chdr) {
        _sw.logf("Received file descriptor from client %d.", session._fd);
        if (req.type == SV3_REQ_MEMORY_MAP or
            req.type == SV3_REQ_EVENT_FD) {
          req.memory_map.fd = *reinterpret_cast<int *>(CMSG_DATA(&chdr));
        } else {
          _sw.logf("... but we didn't expect one!\n");
          close(*reinterpret_cast<int *>(CMSG_DATA(&chdr)));
          goto do_close;
        }
      }
    }

    {
      Sv3Response resp = handle_request(session, req);
      res = send(session._fd, &resp, sizeof(resp), MSG_EOR | MSG_NOSIGNAL);
      if (res != sizeof(resp)) {
        char err[128];
        strerror_r(errno, err, sizeof(err));
        _sw.logf("Error sending response to client %d: %s", session._fd, err);
        goto do_close;
      }
    }

    return true;
  do_close:
    close_session(session);
    return false;             // Remove!
  }

  void Listener::thread_fun()
  {
    do {
      fd_set fdset;
      int    nfds = 0;
      FD_ZERO(&fdset);
      FD_SET(_sfd, &fdset); if (_sfd >= nfds) nfds = _sfd + 1;
      for (auto &s : _sessions) {
        FD_SET(s._fd, &fdset);
        if (s._fd >= nfds) nfds = s._fd + 1;
      }

      int res = select(nfds, &fdset, NULL, NULL, NULL);
      if (res < 0) {
        perror("select");
        return;
      }

      if (FD_ISSET(_sfd, &fdset)) accept_session();

      for (auto s = _sessions.begin(); s != _sessions.end(); ++s)
        if (FD_ISSET((*s)._fd, &fdset))
          if (not poll(*s))
            s = _sessions.erase(s);

    } while (true);
  }

  Listener::Listener(Switch &sw) : _sw(sw)
  {
    _sfd = socket(AF_LOCAL, SOCK_SEQPACKET, 0);
    if (_sfd < 0) throw std::system_error(errno, std::system_category());

    _local_addr.sun_family = AF_LOCAL;
    snprintf(_local_addr.sun_path, sizeof(_local_addr.sun_path),
             "/tmp/sv3" /* "-%u", getpid() */);

    _sw.logf("Listening for clients on %s.", _local_addr.sun_path);

    if (0 != bind(_sfd, reinterpret_cast<sockaddr *>(&_local_addr), sizeof(_local_addr)))
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
    unlink(_local_addr.sun_path);

    void *ret;
    pthread_join(_thread, &ret);
  }

}

// EOF

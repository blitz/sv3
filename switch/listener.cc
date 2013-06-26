

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <unistd.h>

#include <listener.hh>
#include <system_error>

namespace Switch {

  Session::~Session() {
    close(_fd);

    for (int fd : _file_descriptors)
      close(fd);
  }

  void Listener::accept_session()
  {
    sockaddr_un sa;
    socklen_t   si = sizeof(sa);

    int     res = accept(_sfd, reinterpret_cast<sockaddr *>(&sa), &si);
    if (res < 0) {
      perror("accept");
      return;
    }

    if (si > sizeof(sa)) {
      _sw.logf("sockaddr too large? %u vs %lu", si, sizeof(sa));
      return;
    }

    _sessions.push_back(new Session(_sw, res, sa));
  }

  bool RegionList::insert(Region const &r)
  {
    // Overflow or empty region
    if (r.addr + r.size <= r.addr) return false;

    uint64_t r_end  = r .addr + r .size;
    for (auto &lr : _list) {
      uint64_t lr_end = lr.addr + lr.size;
      if (r.addr < lr_end and lr.addr < r_end) {
	return false;
      }
    }

    // Sorted insert
    auto it = _list.begin();
    for (; it != _list.end() and (*it).addr < r_end; ++it) {}
    _list.insert(it, r);

    return true;
  }

  externalpci_res Session::handle_request(externalpci_req const &req)
  {
    externalpci_res resp;
    memset(reinterpret_cast<void *>(&resp), 0, sizeof(resp));

    return resp;
  }


  bool Session::poll()
  {
    externalpci_req req;
    externalpci_res resp;
    struct msghdr   hdr;
    struct iovec    iov = { &req, sizeof(req) };
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

    int res = recvmsg(_fd, &hdr, 0);
    if (res < 0) {
      char err[128];
      strerror_r(errno, err, sizeof(err));
      _sw.logf("Got error from connection to client %d: %s", _fd, err);
      goto do_close;
    }

    // Is our connection closed?
    if (res == 0) {
      // _sw.logf("Goodbye, client %u!", session._fd);
      goto do_close;
    }

    if (res != sizeof(req)) {
      _sw.logf("Client %3d violated protocol. %u %zu", _fd, res, sizeof(req));
      goto do_close;
    }

    {
      cmsghdr *incoming_chdr = CMSG_FIRSTHDR(&hdr);
      if (incoming_chdr) {
	int incoming_fd;
	memcpy(&incoming_fd, CMSG_DATA(&chdr), sizeof(int));

	_sw.logf("Received file descriptor %d from client %d.", incoming_fd, _fd);
	if (req.type == EXTERNALPCI_REQ_REGION) {
	  req.region.fd = incoming_fd;
	} else if (req.type == EXTERNALPCI_REQ_IRQ) {
	  req.irq_req.fd = incoming_fd;
	} else {
	  _sw.logf("... but we didn't expect one!\n");
	  close(*reinterpret_cast<int *>(CMSG_DATA(&chdr)));
	  goto do_close;
	}
      }
    }

    resp = handle_request(req);

    // Prepare sending response
    hdr.msg_name       = NULL;
    hdr.msg_namelen    = 0;
    iov.iov_base       = &resp;
    iov.iov_len        = sizeof(resp);
    hdr.msg_control    = NULL;
    hdr.msg_controllen = 0;

    if (resp.type == EXTERNALPCI_REQ_PCI_INFO) {
      // Pass file descriptor
      hdr.msg_control    = &chdr;
      hdr.msg_controllen = CMSG_LEN(sizeof(int));
      chdr.cmsg_len     = CMSG_LEN(sizeof(int));
      chdr.cmsg_level   = SOL_SOCKET;
      chdr.cmsg_type    = SCM_RIGHTS;

      memcpy(CMSG_DATA(&chdr), &resp.pci_info.hotspot_fd, sizeof(int));
    }

    res  = sendmsg(_fd, &hdr, MSG_EOR | MSG_NOSIGNAL);
    if (res != sizeof(resp)) {
      char err[128];
      strerror_r(errno, err, sizeof(err));
      _sw.logf("Error sending response to client %d: %s", _fd, err);
      goto do_close;
    }

    return true;
  do_close:
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
	FD_SET(s->_fd, &fdset);
	if (s->_fd >= nfds) nfds = s->_fd + 1;
      }

      struct timeval to;
      to.tv_sec = 0;
      to.tv_usec = 100000;
      int res = select(nfds, &fdset, NULL, NULL, &to);
      if (res < 0) {
	// When our destructor closes _sfd, select() will exit with an
	// error and we exit the thread.

	// XXX Delete all sessions.

	return;
      }

      if (FD_ISSET(_sfd, &fdset)) accept_session();

      for (auto s = _sessions.begin(); s != _sessions.end(); ++s) {
	Session *session = *s;
	if (FD_ISSET(session->_fd, &fdset))
	  if (not session->poll()) {
	    // XXX Use unique_ptr

	    s = _sessions.erase(s);
	    delete session;
	  }
      }

    } while (true);
  }

  Listener::Listener(Switch &sw, bool force) : _sw(sw)
  {
    _sfd = socket(AF_LOCAL, SOCK_SEQPACKET, 0);
    if (_sfd < 0) throw std::system_error(errno, std::system_category());

    _local_addr.sun_family = AF_LOCAL;
    snprintf(_local_addr.sun_path, sizeof(_local_addr.sun_path),
	     "/tmp/sv3" /* "-%u", getpid() */);

    _sw.logf("Listening for clients on %s.", _local_addr.sun_path);

    if (force) {
      // Try to unlink the file. We don't care about the exit value,
      // because bind() will catch all errors anyway.
      unlink(_local_addr.sun_path);
    }

    if (0 != bind(_sfd, reinterpret_cast<sockaddr *>(&_local_addr), sizeof(_local_addr)))
      throw std::system_error(errno, std::system_category());

    if (0 != listen(_sfd, 5))
      throw std::system_error(errno, std::system_category());

    _thread = std::thread(&Listener::thread_fun, this);
  }

  Listener::~Listener()
  {
    close(_sfd);
    unlink(_local_addr.sun_path);

    _thread.join();
  }

}

// EOF

// Copyright (C) 2013, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
// Economic rights: Technische Universitaet Dresden (Germany)

// This file is part of sv3.

// sv3 is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

// sv3 is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License version 2 for more details.



#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cinttypes>

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

  bool Session::handle_request(externalpci_req const &req,
			       externalpci_res &res)
  {
    memset(reinterpret_cast<void *>(&res), 0, sizeof(res));
    res.type = req.type;

    switch (req.type) {
    case EXTERNALPCI_REQ_PCI_INFO: {

      _device.get_device_info(res.pci_info.vendor_id,    res.pci_info.device_id,
			      res.pci_info.subsystem_id, res.pci_info.subsystem_vendor_id);

      for (unsigned i = 0; i < 6; i++)
	_device.get_bar_info(i, res.pci_info.bar[i].size);

      _device.get_irq_info(res.pci_info.msix_vectors);

      _device.get_hotspot(res.pci_info.hotspot_bar,
			  res.pci_info.hotspot_addr,
			  res.pci_info.hotspot_size,
			  res.pci_info.hotspot_fd);

      break;
    }
    case EXTERNALPCI_REQ_REGION: {
      Region r(req.region.phys_addr,
	       req.region.size,
	       reinterpret_cast<uint8_t *>(mmap(NULL, req.region.size,
						PROT_READ | PROT_WRITE,
						MAP_SHARED,
						req.region.fd,
						req.region.offset)));
      close(req.region.fd);
      if (r.mapping == MAP_FAILED) {
	_sw.logf("mmap failed.");
	return false;
      }
      _sw.logf("Inserting region %016" PRIx64 "+%08" PRIx64 " at %p.",
	       r.addr, r.size, r.mapping);
      return _regions.insert(r);
    }
    case EXTERNALPCI_REQ_RESET: {
      _device.reset();
      break;
    }
    case EXTERNALPCI_REQ_IOT: {
      bool irqs_changed = false;

      if (req.iot_req.type == externalpci_iot_req::IOT_READ)
        res.iot_res.value = _device.io_read(req.iot_req.bar,
					    req.iot_req.hwaddr, req.iot_req.size);
      else {
        _device.io_write(req.iot_req.bar, req.iot_req.hwaddr,
			 req.iot_req.size, req.iot_req.value, irqs_changed);
      }

      /* Notify qemu if it can fetch IRQ info. */
      if (irqs_changed)
	res.flags |= EXTERNALPCI_RES_FLAG_FETCH_IRQS;

      break;
    }
    case EXTERNALPCI_REQ_IRQ:
      _device.get_msix_info(req.irq_req.fd,    req.irq_req.idx,
			    res.irq_res.valid, res.irq_res.more);
      break;
    case EXTERNALPCI_REQ_EXIT:
    default:
      _sw.logf("Didn't understand message %u from client %d",
	       req.type, _fd);
      return false;
    }

    return true;
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
      _sw.logf("Goodbye, client %u!", _fd);
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

	_file_descriptors.push_back(incoming_fd);

	_sw.logf("Received file descriptor %d from client %d.", incoming_fd, _fd);
	if (req.type == EXTERNALPCI_REQ_REGION) {
	  req.region.fd = incoming_fd;
	} else if (req.type == EXTERNALPCI_REQ_IRQ) {
	  req.irq_req.fd = incoming_fd;
	} else {
	  _sw.logf("... but we didn't expect one!\n");
	  goto do_close;
	}
      } else {
	if (req.type == EXTERNALPCI_REQ_REGION or
	    req.type ==  EXTERNALPCI_REQ_IRQ) {
	  _sw.logf("Expected file descriptor.");
	  goto do_close;
	}

      }
    }

    if (not handle_request(req, resp))
      goto do_close;

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
      _sw.logf("Transmitting fd %d to client %d.", resp.pci_info.hotspot_fd, _fd);
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
    rcu_register_thread();

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
      rcu_thread_offline();
      int res = select(nfds, &fdset, NULL, NULL, &to);
      rcu_thread_online();
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

    rcu_unregister_thread();
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
      // Try to unlink the file. We don't care about failures,
      // because bind() will catch them anyway.
      
      int res = unlink(_local_addr.sun_path);
      if (res == 0) _sw.logf("Cleaned up stale socket.");
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

    _sw.logf("Waiting for listener thread to join.");
    _thread.join();
    _sw.logf("Listener thread joined.");
  }

}

// EOF

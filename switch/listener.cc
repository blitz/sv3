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

#include <vhost-user.hh>

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

  bool Session::insert_region(Region const &r)
  {
    _sw.logf("Inserting region %016" PRIx64 "+%08" PRIx64 " at %p.",
	     r.addr, r.size, r.mapping);
    bool s =  _regions.insert(r);
    if (s) _sw.announce_dma_memory(_device, r.mapping, r.size);

    return s;
  }

  static std::unique_ptr<vhost_request> new_vhost_response(size_t payload)
  {
    void *mem = new char[sizeof(vhost_request) + payload];
    auto r    = std::unique_ptr<vhost_request>(new (mem) vhost_request);
    r->hdr.size  = payload;
    r->hdr.flags = 1;
    return r;
  }

  std::unique_ptr<vhost_request> Session::handle_request(vhost_request const &req,
                                                         int *fds)
  {
    switch (req.hdr.request) {
    case VHOST_USER_SET_OWNER:
      _sw.logf("VHOST_USER_SET_OWNER");
      break;
    case VHOST_USER_GET_FEATURES:
      {
        auto res = new_vhost_response(sizeof(req.u64));
        res->hdr.request = req.hdr.request;
        res->u64 = _device.vhost_get_features();
        return res;
      }

    // case EXTERNALPCI_REQ_PCI_INFO: {

    //   _device.get_device_info(res.pci_info.vendor_id,    res.pci_info.device_id,
    //     		      res.pci_info.subsystem_id, res.pci_info.subsystem_vendor_id);

    //   for (unsigned i = 0; i < 6; i++)
    //     _device.get_bar_info(i, res.pci_info.bar[i].size);

    //   _device.get_irq_info(res.pci_info.msix_vectors);

    //   _device.get_hotspot(res.pci_info.hotspot_bar,
    //     		  res.pci_info.hotspot_addr,
    //     		  res.pci_info.hotspot_size,
    //     		  res.pci_info.hotspot_fd);

    //   break;
    // }
    // case EXTERNALPCI_REQ_REGION: {
    //   // We need this hint, because otherwise are pretty certain to
    //   // get virtual addresses for which we cannot create 1:1 DMA
    //   // mappings. Don't worry about races here, mmap takes care of
    //   // that.
    //   static uintptr_t address_hint = (1ULL << 32);
    //   Region r(req.region.phys_addr,
    //            req.region.size,
    //            reinterpret_cast<uint8_t *>(mmap((void *)address_hint, req.region.size,
    //     					PROT_READ | PROT_WRITE,
    //     					MAP_SHARED,
    //     					req.region.fd,
    //     					req.region.offset)));
    //   close(req.region.fd);
    //   if (r.mapping == MAP_FAILED) {
    //     _sw.logf("mmap failed. Did you use -mem-path for qemu?");
    //     return false;
    //   }
    //   address_hint += req.region.size;
    //   return insert_region(r);
    // }
    // case EXTERNALPCI_REQ_RESET: {
    //   _device.reset();
    //   break;
    // }
    // case EXTERNALPCI_REQ_IOT: {
    //   bool irqs_changed = false;

    //   if (req.iot_req.type == externalpci_iot_req::IOT_READ)
    //     res.iot_res.value = _device.io_read(req.iot_req.bar,
    //     				    req.iot_req.hwaddr, req.iot_req.size);
    //   else {
    //     _device.io_write(req.iot_req.bar, req.iot_req.hwaddr,
    //     		 req.iot_req.size, req.iot_req.value, irqs_changed);
    //   }

    //   /* Notify qemu if it can fetch IRQ info. */
    //   if (irqs_changed)
    //     res.flags |= EXTERNALPCI_RES_FLAG_FETCH_IRQS;

    //   break;
    // }
    // case EXTERNALPCI_REQ_IRQ:
    //   _device.get_msix_info(req.irq_req.fd,    req.irq_req.idx,
    //     		    res.irq_res.valid, res.irq_res.more);
    //   break;
    // case EXTERNALPCI_REQ_EXIT:
    default:
      _sw.logf("Didn't understand message %u from client %d",
	       req.hdr.request, _fd);
    }

    return nullptr;
  }


  bool Session::poll()
  {
    vhost_request   req;
    struct msghdr   hdr;
    struct iovec    iov = { &req, sizeof(req.hdr) };
    union {
      struct cmsghdr chdr;
      char           chdr_data[CMSG_SPACE(vhost_user_memory::max_regions * sizeof(int))];
    };

    memset(&req,      0, sizeof(req));
    memset(chdr_data, 0, sizeof(chdr_data));

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
      return false;
    }

    // Is our connection closed?
    if (res == 0) {
      _sw.logf("Goodbye, client %u!", _fd);
      return false;
    }
    {
      cmsghdr *incoming_chdr = CMSG_FIRSTHDR(&hdr);
      if (incoming_chdr) {
	int fds = (incoming_chdr->cmsg_len - sizeof(*incoming_chdr)) / sizeof(int);
        for (unsigned i = 0; i < fds; i++) {
          int fd;
          memcpy(&fd, CMSG_DATA(&chdr) + i * sizeof(int), sizeof(int));
          _sw.logf("Client %d sent file descriptor %d.", _fd, fd);
          _file_descriptors.push_back(fd);
        }
      }
    }

    if (req.hdr.size + sizeof(req.hdr) > sizeof(req))
      return false;

    // Read payload.
    if (req.hdr.size) {
      res = recv(_fd, (char *)&req + sizeof(req.hdr), req.hdr.size, MSG_NOSIGNAL);
      if (res != req.hdr.size) {
        _sw.logf("Read %d bytes, but expected %" PRIu32 " from client %d.",
                 res, req.hdr.size, _fd);
        return false;
      }
    }

    auto resp = handle_request(req, (int *)(CMSG_DATA(&chdr)));
    if (resp) {
      res  = send(_fd, resp.get(), sizeof(resp->hdr) + resp->hdr.size, MSG_EOR | MSG_NOSIGNAL);
      if (res != sizeof(resp->hdr) + resp->hdr.size) {
        char err[128];
        strerror_r(errno, err, sizeof(err));
        _sw.logf("Error sending response to client %d: %s", _fd, err);
        return false;
      }
    }

    return true;
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
	    // XXX Use shared_ptr

	    s = _sessions.erase(s);
	    delete session;
	  }
      }

    } while (true);

    rcu_unregister_thread();
  }

  Listener::Listener(Switch &sw, bool force) : _sw(sw)
  {
    _sfd = socket(AF_LOCAL, SOCK_STREAM, 0);
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

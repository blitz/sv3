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

#include <cstring>
#include <unistd.h>

#include <thread>

#include <vfio.hh>
#include <switch.hh>

namespace Switch {

  class PollTimeout { };

  template <class T>
  void poll_for(int us, T closure)
  {
    do {
      if (closure()) return;
      usleep(1);
    } while (us > 0);

    throw PollTimeout();
  }

  class Intel82599 : public VfioDevice {

    uint32_t volatile *_reg;

    struct desc { uint64_t hi; uint64_t lo; };
  
    desc              *_rx_desc;
    desc              *_tx_desc;
    uint32_t volatile *_tx_writeback;

    int _rxtx_eventfd;
    int _misc_eventfd;

    uint64_t receive_address(unsigned idx);


    // Stop issuing master requests.
    void master_disable();

    /// Allocates page aligned and zeroed memory and makes it visible to
    /// the device..
    template <class T>
    T *alloc_dma_mem(size_t size = sizeof(T))
    {
      T *p;
      size_t alloc_size = (size + 0xFFF) & ~0xFFF;
      if (0 != posix_memalign((void **)&p, 0x1000, alloc_size))
	throw "Allocation failure";
      memset(p, 0, alloc_size);
      map_memory_to_device(p, alloc_size, true, true);
      return p;
    }

  public:

    int         misc_event_fd() const { return _misc_eventfd; }
    int         rxtx_event_fd() const { return _rxtx_eventfd; }
    void        unmask_misc_irq();
    std::string status();
    void        reset();

    Intel82599(VfioGroup group, int fd, int rxtx_eventfd);
  };

  class Intel82599Port : public Intel82599,
			 public Port
  {
    std::thread _misc_thread;

    void misc_thread_fn();

  public:

    void receive(Packet &p) override;
    bool poll(Packet &p, bool enable_notifications) override;
    void mark_done(Packet &p) override;
    void poll_irq() override;

    Intel82599Port(VfioGroup group, int fd,
		   Switch &sw, std::string name);

  };

}

// EOF

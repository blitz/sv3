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

  protected:

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
	throw Exception("posix_memalign failed");
      memset(p, 0, alloc_size);
      map_memory_to_device(p, alloc_size, true, true);
      return p;
    }

    enum {
      RX_BUFFER_SIZE   = 4096,
      QUEUE_LEN        = 4096,	// 8K is the highest we can use here
				// according to the manual.
    };

  public:

    int         misc_event_fd() const { return _misc_eventfd; }
    int         rxtx_event_fd() const { return _rxtx_eventfd; }
    void        unmask_rxtx_irq();
    void        unmask_misc_irq();
    std::string status();
    void        reset();

    Intel82599(VfioGroup group, int fd, int rxtx_eventfd);
  };

  class Intel82599Port : public Intel82599,
			 public Port
  {
    // Configuration

    enum {
      RX_BUFFER_MEMORY = 7 << 20,
      RX_BUFFERS       = RX_BUFFER_MEMORY / RX_BUFFER_SIZE,

    };

    static_assert(unsigned(RX_BUFFERS) < unsigned(QUEUE_LEN), "Too many RX buffers");

    struct rx_buffer {
      uint8_t data[RX_BUFFER_SIZE];
    };

    std::thread _misc_thread;

    uint16_t    _shadow_rdt0;
    uint16_t    _shadow_rdh0;

    uint16_t    _shadow_tdt0;
    uint16_t    _shadow_tdh0;
    uint16_t    _tx0_inflight;


    // Remembers which buffer we stored in an RX queue entry.
    struct {
      rx_buffer *buffer;

      // We need space to construct a header. Here is as good as any.
      virtio_net_hdr_mrg_rxbuf hdr;

      // 82599's buffer chaining for LRO is weird. We need to remember
      // where our buffer chain started.

      // False if this is the first packet in a chain. This is stored
      // as a complemented value to be able to easily initialize
      // everything with zero.
      bool       not_first;

      // Index of previous buffer.
      uint16_t   rsc_last;

      // How many buffers do we have until now not including this?
      uint16_t   rsc_number;
    } _rx_buffers[QUEUE_LEN];

    // Information for cleaning TX buffers
    struct {
      bool need_completion;

      // Only valid if need_completion is true;
      Packet::CompletionInfo info;
    } _tx_buffers[QUEUE_LEN];

    uint16_t advance_qp(uint16_t idx) {
      assert(idx < QUEUE_LEN);
      idx += 1;
      if (idx == QUEUE_LEN) idx = 0;
      return idx;
    }

    // Is there space for a single TX descriptor?
    bool tx_has_room();

    void misc_thread_fn();
    desc populate_rx_desc(uint8_t *data);
    desc populate_tx_desc(uint8_t *data, uint16_t len, uint16_t total_len,
			  bool first, uint64_t first_flags,
			  bool eop);
  public:

    void receive(Packet &p) override;
    bool poll(Packet &p, bool enable_notifications) override;
    void mark_done(Packet::CompletionInfo &p) override;

    Intel82599Port(VfioGroup group, int fd,
		   Switch &sw, std::string name);

  };

}

// EOF

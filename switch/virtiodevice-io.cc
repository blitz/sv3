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

// Part of this code is based on code from Qemu, which is also GPL 2.

#include <algorithm>
#include <cinttypes>
#include <virtiodevice.hh>
#include <switch.hh>
#include <session.hh>
#include <tracing.hh>

namespace Switch {

  void
  VirtioDevice::receive(Packet &src)
  {
    assert(src.completion_info.src_port != this);

    if (UNLIKELY(not (status & VIRTIO_CONFIG_S_DRIVER_OK))) return;

    trace(PACKET_RX, _session._fd, 0);

    // Deal with offloads. Check whether the guest can receive all our
    // offloads, if not always use the slow path (which is not implemented...).

    const uint32_t fast_path_features = 0
      | (1 << VIRTIO_NET_F_MRG_RXBUF)
      | (1 << VIRTIO_NET_F_GUEST_CSUM)
      | (1 << VIRTIO_NET_F_GUEST_TSO4)
      | (1 << VIRTIO_NET_F_GUEST_TSO6);

    if (UNLIKELY((guest_features & fast_path_features) != fast_path_features)) {
      throw PortBrokenException(*this, "XXX implement slow path");
    }

    // Points to src fragment currently in-use.
    unsigned       src_fragment = 0;

    // We'll remember where we need to update the header, when we're
    // done. If this is set, we have processed the header.
    uint16_t      *num_buffers  = nullptr;

    uint8_t const *src_ptr   = src.fragment[src_fragment];
    uint32_t       src_space = src.fragment_length[src_fragment];

    // How many bytes are left in the source packet to copy.
    uint32_t       tot_space = src.packet_length;

    // logf("Packet is %u bytes long.", tot_space);

    auto c = [&]
      (uint8_t *dst_ptr, uint32_t dst_space) {

      while (dst_space) {
	uint32_t chunk = std::min(dst_space, src_space);
	assert(chunk <= tot_space);

	if (num_buffers) {
	  // Plain data
	  movs(dst_ptr, src_ptr, chunk);
	} else {
	  // Special case for header.
	  virtio_net_hdr_mrg_rxbuf       *dst_hdr = reinterpret_cast<virtio_net_hdr_mrg_rxbuf       *>(dst_ptr);
	  virtio_net_hdr_mrg_rxbuf const *src_hdr = reinterpret_cast<virtio_net_hdr_mrg_rxbuf const *>(src_ptr);

	  if (UNLIKELY(chunk < sizeof(struct virtio_net_hdr_mrg_rxbuf)))
	    throw PortBrokenException(*this, "no space for contiguous header");
	  chunk = sizeof(struct virtio_net_hdr_mrg_rxbuf);

	  memcpy(dst_ptr, src_ptr, chunk);
	  dst_ptr += chunk;
	  src_ptr += chunk;

	  // XXX The source port needs to translate header! In this
	  // case virtiodevice needs to translate
	  // VIRTIO_NET_HDR_F_NEEDS_CSUM into
	  // VIRTIO_NET_HDR_F_DATA_VALID. intel82599 does this
	  // correctly by synthesizing a new virtio header in poll()!
	  // Seeing VIRTIO_NET_HDR_F_NEEDS_CSUM here should be
	  // considered a bug!
	  dst_hdr->flags = (src_hdr->flags & (VIRTIO_NET_HDR_F_NEEDS_CSUM | VIRTIO_NET_HDR_F_DATA_VALID))
 	  ? VIRTIO_NET_HDR_F_DATA_VALID : 0;

	  // We'll update this later. Don't know how many yet.
	  num_buffers = &dst_hdr->num_buffers;
	}

	src_space -= chunk;
	dst_space -= chunk;
	tot_space -= chunk;

	if (src_space == 0) {
	  if (tot_space == 0) {
	    // Packet completely copied.
	    return true;
	  }

	  // Otherwise, fetch a new src fragment.
	  src_fragment += 1;
	  src_ptr       = src.fragment[src_fragment];
	  src_space     = src.fragment_length[src_fragment];
	}
      }

      return false;           // We want more.
    };

    // Now we have our closure that fills a single RX descriptor
    // chain, let's start popping RX descriptor chains until our
    // packet is consumed.

    uint32_t last_tot_space  = tot_space;
    unsigned num_descriptors = 0; // Descriptors we consumed

    while (tot_space) {
      unsigned head = vq_pop_generic(rx_vq(), true, c);
      if (head == INVALID_DESC_ID) break;

      uint32_t bytes_consumed = last_tot_space - tot_space;
      vq_fill(rx_vq(), head, bytes_consumed, num_descriptors);
      num_descriptors += 1;
      last_tot_space   = tot_space;
    }

    // Update the header with the actual number of descriptors we
    // consumed. This might still be a null pointer, when the guest
    // ran out of RX descriptors.
    if (num_buffers) *num_buffers = num_descriptors;

    vq_flush(rx_vq(), num_descriptors);
  }

  void
  VirtioDevice::vq_irq(VirtQueue &vq)
  {
    unsigned vector = vq.vector;
    if (UNLIKELY(vq.pending_irq) and LIKELY(_irq_fd[vector])) {
      vq.pending_irq = false;

      if (not (__atomic_load_n(&vq.vring.avail->flags, __ATOMIC_ACQUIRE) &
	       VRING_AVAIL_F_NO_INTERRUPT)) {
	// Guest asks to be interrupted.
	isr.store(1, std::memory_order_release);
	trace(IRQ, _session._fd);
	uint64_t val = 1;
	write(_irq_fd[vector], &val, sizeof(val));
      }
    }
  }

  void
  VirtioDevice::poll_irq()
  {
    vq_irq(rx_vq());
    vq_irq(tx_vq());
  }

  int
  VirtioDevice::vq_num_heads(VirtQueue &vq, unsigned idx)
  {
    /* Callers read a descriptor at vq->last_avail_idx.  Make sure
     * descriptor read does not bypass avail index read. */
    unsigned avail     = __atomic_load_n(&vq.vring.avail->idx, __ATOMIC_ACQUIRE);
    uint16_t num_heads = avail - idx;

    /* Check if guest isn't doing very strange things with descriptor
       numbers. */
    if (UNLIKELY(num_heads > QUEUE_ELEMENTS))
      throw PortBrokenException(*this, "avail->idx b0rken");

    return num_heads;
  }

  unsigned
  VirtioDevice::vq_get_head(VirtQueue &vq, unsigned idx)
  {
    unsigned int head;

    /* Grab the next descriptor number they're advertising, and increment
     * the index we've seen. */
    head = __atomic_load_n(&vq.vring.avail->ring[idx % QUEUE_ELEMENTS],
                           __ATOMIC_ACQUIRE);

    /* If their number is silly, that's a fatal mistake. */
    if (UNLIKELY(head >= QUEUE_ELEMENTS))
      throw PortBrokenException(*this, "head b0rken");

    return head;
  }

  unsigned
  VirtioDevice::vq_next_desc(VRingDesc *desc)
  {
    unsigned int next;

    /* If this descriptor says it doesn't chain, we're done. */
    if (not (desc->flags & VRING_DESC_F_NEXT))
      return INVALID_DESC_ID;

    /* Check they're not leading us off end of descriptors. */
    next = __atomic_load_n(&desc->next, __ATOMIC_ACQUIRE);
    if (UNLIKELY(next >= QUEUE_ELEMENTS))
      throw PortBrokenException(*this, "next beyond bounds");

    return next;
  }

  template <typename T>
  int
  VirtioDevice::vq_pop_generic(VirtQueue &vq, bool writeable_bufs, T closure)
  {
    VRingDesc *desc = vq.vring.desc;

    if (!vq_num_heads(vq, vq.last_avail_idx))
      return INVALID_DESC_ID;

    unsigned head;
    unsigned i = head = this->vq_get_head(vq, vq.last_avail_idx++);

    // Collect all the descriptors
    do {
      // We need to load this only once. Otherwise, the guest may
      // fool pointer validation.
      uint32_t flen = __atomic_load_n(&desc[i].len, __ATOMIC_RELAXED);
      uint8_t *data = _session.translate_ptr(desc[i].addr, flen);

      // We either collect only writeable or readable buffers. Check
      // for fragment list overflow and pointer translation failures
      // as well.
      bool buf_writeable = (desc[i].flags & VRING_DESC_F_WRITE);
      if (UNLIKELY((writeable_bufs xor buf_writeable) or
                   (data == nullptr)))
        throw PortBrokenException(*this, "mixed read/write descriptors");

      // logf("Popped %p+%zx", data, size_t(flen));
      if (closure(data, flen))
        break;

    } while ((i = vq_next_desc(&desc[i])) != INVALID_DESC_ID);

    vq.inuse++;
    return head;
  }


  int
  VirtioDevice::vq_pop(VirtQueue &vq, Packet &p,
                       bool writeable_bufs)
  {
    assert(p.fragments     == 0 and
           p.packet_length == 0);

    // Called for each buffer in the chain. Collect them in Packet p.
    auto c = [&] (uint8_t *data, uint32_t flen) {
      assert(data);
      p.fragment[p.fragments]        = data;
      p.fragment_length[p.fragments] = flen;

      p.packet_length += flen;

      if (UNLIKELY(p.fragments + 1U > Packet::MAX_FRAGMENTS))
        throw PortBrokenException(*this, "too many fragments");

      p.fragments     += 1;

      return false; // We want more
    };

    p.completion_info.virtio.index = vq_pop_generic(vq, writeable_bufs, c);
    return p.fragments;
  }

  void
  VirtioDevice::vq_fill(VirtQueue &vq, unsigned head,
                        uint32_t len, unsigned idx)
  {
    idx = (idx + vq.vring.used->idx) % QUEUE_ELEMENTS;

    /* Get a pointer to the next entry in the used ring. */
    VRingUsedElem &el = vq.vring.used->ring[idx];
    el.id  = head;
    el.len = len;
  }


  void
  VirtioDevice::vq_flush(VirtQueue &vq, unsigned count)
  {
    uint16_t oldv, newv;
    VRingUsed  *used  = vq.vring.used;

    oldv = __atomic_load_n(&used->idx, __ATOMIC_ACQUIRE);
    newv = oldv + count;
    __atomic_store_n(&used->idx, newv, __ATOMIC_RELEASE);
    vq.inuse -= count;

    // Guest may need to be interrupted. Check this in poll_irq.
    vq.pending_irq = true;
  }


  void VirtioDevice::vq_push(VirtQueue &vq, unsigned head, uint32_t len)
  {
    vq_fill(vq, head, len, 0);
    vq_flush(vq, 1);
  }


  bool
  VirtioDevice::poll(Packet &p, bool enable_notifications)
  {
    VirtQueue &vq = tx_vq();

    vq.vring.used->flags = enable_notifications ? 0 : VRING_USED_F_NO_NOTIFY;

    if (not (status & VIRTIO_CONFIG_S_DRIVER_OK) or
        not vq_pop(vq, p, false /* readable buffers */))
      return false;

    if (UNLIKELY(p.fragment_length[0] != sizeof(struct virtio_net_hdr_mrg_rxbuf)))
      throw PortBrokenException(*this, "invalid header size");

    trace(PACKET_TX, _session._fd, p.packet_length);

    // XXX Do something with the packet.

    return true;
  }


  void
  VirtioDevice::mark_done(Packet::CompletionInfo &c)
  {
    vq_push(tx_vq(), c.virtio.index, 0);
  }

  static inline void
  enable_notification(VirtQueue &vq)
  {
    __atomic_and_fetch(&vq.vring.used->flags, ~VRING_USED_F_NO_NOTIFY,
                       __ATOMIC_RELEASE);
  }

  static inline void
  disable_notification(VirtQueue &vq)
  {
    // XXX Probably overkill to use an atomic access here.
    __atomic_or_fetch(&vq.vring.used->flags, VRING_USED_F_NO_NOTIFY,
                      __ATOMIC_RELEASE);
  }


  static inline void *
  vnet_vring_align(void *addr,
                   unsigned long align)
  {
    return (void *)(((uintptr_t)addr + align - 1) & ~(align - 1));
  }



  void
  VirtioDevice::vq_set_addr(VirtQueue &vq, uint64_t addr)
  {
    vq.pa = addr;

    // XXX Use correct size here.
    char *va = _session.translate_ptr<char>(vq.pa);
    if (va != nullptr) {
      vq.vring.desc  = reinterpret_cast<VRingDesc *>(va);
      vq.vring.avail = reinterpret_cast<VRingAvail *>(va + QUEUE_ELEMENTS * sizeof(VRingDesc));
      vq.vring.used  = reinterpret_cast<VRingUsed *>(vnet_vring_align(reinterpret_cast<char *>(vq.vring.avail) +
                                                                       offsetof(VRingAvail, ring[QUEUE_ELEMENTS]),
                                                                       VIRTIO_PCI_VRING_ALIGN));
    } else {
      vq.vring.desc  = nullptr;
      vq.vring.avail = nullptr;
      vq.vring.used  = nullptr;

      logf("Cannot translate address %016" PRIx64 ".", vq.pa);
    }
  }
}

// EOF

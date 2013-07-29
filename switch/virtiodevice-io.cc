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

namespace Switch {

  void
  VirtioDevice::receive(Packet &src)
  {
    assert(src.src_port != this);

    if (UNLIKELY(not (status & VIRTIO_CONFIG_S_DRIVER_OK))) return;

    // Deal with offloads. Check whether the guest can receive all our
    // offloads, if not always use the slow path (which is not implemented...).

    const uint32_t fast_path_features = 0
      | (1 << VIRTIO_NET_F_GUEST_CSUM)
      | (1 << VIRTIO_NET_F_GUEST_TSO4)
      | (1 << VIRTIO_NET_F_GUEST_TSO6)
      | (1 << VIRTIO_NET_F_GUEST_UFO)
      | (1 << VIRTIO_NET_F_GUEST_ECN);

    if (UNLIKELY((guest_features & fast_path_features) != fast_path_features)) {
      // XXX Implement slow path.
      abort();
    }

    // The header in fragment[0] is handled specially in the closure
    // below. Thus we can start with the first data fragment here.
    unsigned       src_fragment = 1; // Points to src fragment currently in-use.
    unsigned       dst_fragment = 0; // How many dst fragments did we consume?

    uint8_t const *src_ptr   = src.fragment[src_fragment];
    uint32_t       src_space = src.fragment_length[src_fragment];

    uint32_t       tot_space = src.packet_length;

    auto c = [&]
      (uint8_t *dst_ptr, uint32_t dst_space) {
      if (dst_fragment == 0) {
        // Header
        if (UNLIKELY(dst_space != sizeof(struct virtio_net_hdr)))
          throw PortBrokenException(*this);

        virtio_net_hdr  dst_hdr; memset(&dst_hdr, 0, sizeof(dst_hdr));
        virtio_net_hdr *src_hdr = reinterpret_cast<virtio_net_hdr *>(src.fragment[0]);
        dst_hdr.flags = (src_hdr->flags & VIRTIO_NET_HDR_F_NEEDS_CSUM) ? VIRTIO_NET_HDR_F_DATA_VALID : 0;

        dst_hdr.gso_type = src_hdr->gso_type;
        dst_hdr.hdr_len  = src_hdr->hdr_len;
        dst_hdr.gso_size = src_hdr->gso_size;

        memcpy(dst_ptr, &dst_hdr, sizeof(dst_hdr));
        tot_space -= sizeof(dst_hdr);
      } else {
        // Packet
        while (dst_space) {
          uint32_t chunk = std::min(dst_space, src_space);
          assert(chunk <= tot_space);

          movs(dst_ptr, src_ptr, chunk);

          src_space -= chunk;
          dst_space -= chunk;
          tot_space -= chunk;

          if (src_space == 0) {
            if (tot_space == 0) {
              // Packet completely copied.
              dst_fragment += 1;
              return true;
            }

            // Otherwise, fetch a new src fragment.
            src_fragment += 1;
            src_ptr       = src.fragment[src_fragment];
            src_space     = src.fragment_length[src_fragment];
          }
        }

      }

      dst_fragment += 1;
      return false;           // We want more.
    };

    unsigned head = vq_pop_generic(rx_vq(), true, c);
    if (UNLIKELY(head == 0))
      // No space. Packet dropped.
      return;

    // Calculate packet length. We might have truncated it, because of
    // missing buffer space.
    uint32_t plen = src.packet_length - tot_space;
    vq_push(rx_vq(), head, plen);
  }

  void
  VirtioDevice::vq_irq(VirtQueue &vq)
  {
    unsigned vector = vq.vector;
    if (UNLIKELY(vq.pending_irq) and LIKELY(_irq_fd[vector])) {
      vq.pending_irq = false;

      uint64_t val = 1;
      write(_irq_fd[vector], &val, sizeof(val));
    }
  }

  void
  VirtioDevice::poll_irq()
  {
    vq_irq(rx_vq());
    vq_irq(tx_vq());
  }

  int
  VirtioDevice::vq_num_heads(VirtQueue &vq, unsigned int idx)
  {
    /* Callers read a descriptor at vq->last_avail_idx.  Make sure
     * descriptor read does not bypass avail index read. */
    uint16_t num_heads = __atomic_load_n(&vq.vring.avail->idx,
                                         __ATOMIC_ACQUIRE) - idx;

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
  VirtioDevice::vq_next_desc(VRingDesc *desc, unsigned max)
  {
    unsigned int next;

    /* If this descriptor says it doesn't chain, we're done. */
    if (not (desc->flags & VRING_DESC_F_NEXT))
      return max;

    /* Check they're not leading us off end of descriptors. */
    next = __atomic_load_n(&desc->next, __ATOMIC_ACQUIRE);
    if (UNLIKELY(next >= max)) throw PortBrokenException(*this, "next beyond bounds");

    return next;
  }

  template <typename T>
  int
  VirtioDevice::vq_pop_generic(VirtQueue &vq, bool writeable_bufs,
                               T closure)
  {
    VRingDesc *desc = vq.vring.desc;

    if (!vq_num_heads(vq, vq.last_avail_idx)) return 0;

    unsigned max = QUEUE_ELEMENTS;
    unsigned head;
    unsigned i = head = this->vq_get_head(vq, vq.last_avail_idx++);

    /* Collect all the descriptors */
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

      if (closure(data, flen))
        break;

    } while ((i = vq_next_desc(&desc[i], max)) != max);

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

    p.virtio.index = vq_pop_generic(vq, writeable_bufs, c);
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
    VRingAvail *avail = vq.vring.avail;

    oldv = __atomic_load_n(&used->idx, __ATOMIC_ACQUIRE);
    newv = oldv + count;
    __atomic_store_n(&used->idx, newv, __ATOMIC_RELEASE);
    vq.inuse -= count;

    // XXX This might also work with just assigning the result of the
    // test to pending_irq and destroying earlier scheduled IRQs?
    if (not (__atomic_load_n(&avail->flags, __ATOMIC_ACQUIRE) &
             VRING_AVAIL_F_NO_INTERRUPT)) {
      // Guest asks to be interrupted.
      vq.pending_irq = true;
      isr.store(1, std::memory_order_release);
    }
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

    // XXX Do something with the packet.

    return true;
  }


  void
  VirtioDevice::mark_done(Packet &p)
  {
    vq_push(tx_vq(), p.virtio.index, 0);
  }

  uint64_t
  VirtioDevice::io_read (unsigned bar_no,
                         uint64_t addr,
                         unsigned size)
  {
    uint64_t val = ~0ULL;
    if (UNLIKELY(bar_no != 0)) return val;

    switch (addr) {
    case VIRTIO_PCI_HOST_FEATURES:
      logf("Reading host features %x", host_features);
      val = host_features;  break;
    case VIRTIO_PCI_GUEST_FEATURES:
      logf("Reading guest features %x", guest_features);
      val = guest_features; break;
    case VIRTIO_PCI_QUEUE_PFN:
      if (queue_sel < VIRT_QUEUES)
        val = vq[queue_sel].pa >> VIRTIO_PCI_QUEUE_ADDR_SHIFT;
      break;
    case VIRTIO_PCI_QUEUE_NUM:
      if (queue_sel < VIRT_QUEUES)
        val = QUEUE_ELEMENTS;
      break;
    case VIRTIO_PCI_QUEUE_SEL:
      val = queue_sel;
      break;
    case VIRTIO_PCI_STATUS:
      val = status;
      break;
    case VIRTIO_PCI_ISR:
      /* reading from the ISR also clears it. */
      val = isr.exchange(0);
      break;
    case VIRTIO_MSI_CONFIG_VECTOR:
      val = config_vector;
      break;
    case VIRTIO_MSI_QUEUE_VECTOR:
      if (queue_sel < VIRT_QUEUES)
        val = vq[queue_sel].vector;
      break;
    default:
      logf("Unimplemented register %x read.", addr);
      break;
    }

    logf("io read  %04" PRIx64 "+%x -> %" PRIx64, addr, size,
         val & ((1 << 8*size) - 1));
    return val;
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

  void VirtioDevice::io_write(unsigned bar_no,
                              uint64_t addr,
                              unsigned size,
                              uint64_t val,
                              bool &irqs_changed)
  {
    switch (addr) {
    case VIRTIO_PCI_QUEUE_NOTIFY:
      if (val >= VIRT_QUEUES) {
        logf("Guest notified non-existent queue %u.", (unsigned)val);
        break;
      }

      disable_notification(vq[val]);
      _session._sw.schedule_poll();
      break;
    case VIRTIO_PCI_QUEUE_SEL:
      if (val >= VIRT_QUEUES) {
        logf("Guest selected non-existent queue %u.", (unsigned)val);
      } else
        queue_sel = val;
      break;
    case VIRTIO_PCI_QUEUE_PFN:
      if (queue_sel >= VIRT_QUEUES) {
        logf("Guest set address on non-existent queue %u.", queue_sel);
      } else
        vq_set_addr(vq[queue_sel], val << VIRTIO_PCI_QUEUE_ADDR_SHIFT);

      if (not online and rx_vq().vring.desc and tx_vq().vring.desc)
        enable();

      // Take us offline, if the guest has screwed up queue
      // configuration.
      if (online and not (rx_vq().vring.desc and tx_vq().vring.desc))
        disable();

      break;
    case VIRTIO_PCI_GUEST_FEATURES: {
      /* Guest does not negotiate properly?  We have to assume nothing. */
      if (val & (1 << VIRTIO_F_BAD_FEATURE)) {
        logf("BAD FEATURE set. Guest broken.");
      }

      uint32_t supported_features = host_features; /* What do we support? */
      uint32_t bad = (val & ~supported_features) != 0;
      if (bad)
        logf("Guest features we don't support: %x.\n", bad);


      val &= supported_features;
      guest_features = val;
      logf("Negotiated features: %08x", guest_features);
      break;
    }
    case VIRTIO_PCI_STATUS:
      status = val & 0xFF;
      break;
    case VIRTIO_MSI_CONFIG_VECTOR:
      logf("Configuration change vector is %u.", (unsigned)val);
      config_vector = val;
      irqs_changed  = true;
      break;
    case VIRTIO_MSI_QUEUE_VECTOR:
      if (queue_sel >= VIRT_QUEUES) {
        logf("Guest configured IRQ on non-existent queue %u.", queue_sel);
        break;
      }

      if (val >= MSIX_VECTORS) {
        logf("Guest configured non-existent MSI-X vector %u.", val);
        break;
      }

      logf("Queue %u configured to trigger MSI-X vector %u.", queue_sel, (unsigned)val);
      vq[queue_sel].vector = val;
      irqs_changed  = true;
      break;

    default:
      logf("Unimplemented register %x.", addr);
      break;
    }
    logf("io write %04" PRIx64 "+%x <- %" PRIx64, addr, size, val);
  }

  void VirtioDevice::reset()
  {
    logf("Resetting port.");

    // Take port offline.
    disable();

    status         = 0;
    guest_features = 0;
    queue_sel      = 0;
    isr            = 0;
    config_vector  = VIRTIO_MSI_NO_VECTOR;

    memset(vq, 0, sizeof(vq));
    for (VirtQueue &q : vq) {
      q.vector    = VIRTIO_MSI_NO_VECTOR;
    }

    // We reattach to the switch, when the guest has pointed us to RX and TX queues.
  }

}

// EOF

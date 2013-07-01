#include <algorithm>
#include <cinttypes>
#include <virtiodevice.hh>
#include <switch.hh>
#include <session.hh>

namespace Switch {

  void
  VirtioDevice::receive(Packet &p)
  {
    assert(p.src_port != this);

    logf("Receiving %u bytes in %u fragments.",
	 p.packet_length,
	 p.fragments);

    if (UNLIKELY(not (status & VIRTIO_CONFIG_S_DRIVER_OK))) return;

    Packet dst_p(nullptr);
    if (UNLIKELY(not vq_pop(rx_vq(), dst_p, true))) {
      logf("RX queue empty. Packet dropped.\n");
      return;
    }

    logf("Popped %u bytes in %u fragments.",
	 dst_p.packet_length,
	 dst_p.fragments);

    dst_p.copy_from(p);

    vq_push(rx_vq(), dst_p,
	    std::min(p.packet_length, dst_p.packet_length));
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
    if (UNLIKELY(num_heads > vq.vring.num))
      throw PortBrokenException(*this);

    return num_heads;
  }

  unsigned
  VirtioDevice::vq_get_head(VirtQueue &vq, unsigned idx)
  {
    unsigned int head;

    /* Grab the next descriptor number they're advertising, and increment
     * the index we've seen. */
    head = __atomic_load_n(&vq.vring.avail->ring[idx % vq.vring.num],
			   __ATOMIC_ACQUIRE);

    /* If their number is silly, that's a fatal mistake. */
    if (UNLIKELY(head >= vq.vring.num))
      throw PortBrokenException(*this);

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
    if (UNLIKELY(next >= max)) throw PortBrokenException(*this);

    return next;
  }

  int
  VirtioDevice::vq_pop(VirtQueue &vq, Packet &p,
		       bool writeable_bufs)
  {
    VRingDesc *desc = vq.vring.desc;

    if (!vq_num_heads(vq, vq.last_avail_idx))
      return 0;

    assert(p.fragments     == 0 and
	   p.packet_length == 0);

    unsigned max = vq.vring.num;
    unsigned head;
    unsigned i = head = vq_get_head(vq, vq.last_avail_idx++);
    unsigned fragments = 0;

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
		   (fragments >= Packet::MAX_FRAGMENTS) or
		   (data == nullptr)))
	throw PortBrokenException(*this);

      p.fragment[fragments]        = data;
      p.fragment_length[fragments] = flen;

      p.packet_length += flen;
      fragments       += 1;
    } while ((i = vq_next_desc(&desc[i], max)) != max);

    p.fragments = fragments;
    p.virtio.index = head;

    vq.inuse++;

    logf("Sending %u bytes in %u fragments.",
	 p.packet_length,
	 p.fragments);

    return fragments;
  }

  void
  VirtioDevice::vq_fill(VirtQueue &vq, Packet const &p,
			unsigned len, unsigned idx)
  {
    idx = (idx + vq.vring.used->idx) % vq.vring.num;

    /* Get a pointer to the next entry in the used ring. */
    VRingUsedElem &el = vq.vring.used->ring[idx];
    el.id  = p.virtio.index;
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


  void VirtioDevice::vq_push(VirtQueue &vq,
			     Packet const &p,
			     unsigned len)
  {
    vq_fill(vq, p, len, 0);
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

    if (UNLIKELY(p.fragment_length[0] != sizeof(struct virtio_net_hdr)))
      throw PortBrokenException(*this);

    // XXX Do something with the packet.

    return true;
  }


  void
  VirtioDevice::mark_done(Packet &p)
  {
    vq_push(tx_vq(), p, 0);
  }

  uint64_t
  VirtioDevice::io_read (unsigned bar_no,
			 uint64_t addr,
			 unsigned size)
  {
    uint64_t val = ~0ULL;
    if (UNLIKELY(bar_no != 0)) return val;

    switch (addr) {
    case VIRTIO_PCI_HOST_FEATURES:  val = host_features;  break;
    case VIRTIO_PCI_GUEST_FEATURES: val = guest_features; break;
    case VIRTIO_PCI_QUEUE_PFN:
      if (queue_sel < VIRT_QUEUES)
	val = vq[queue_sel].pa >> VIRTIO_PCI_QUEUE_ADDR_SHIFT;
      break;
    case VIRTIO_PCI_QUEUE_NUM:
      if (queue_sel < VIRT_QUEUES)
	val = vq[queue_sel].vring.num;
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
      vq.vring.avail = reinterpret_cast<VRingAvail *>(va + vq.vring.num * sizeof(VRingDesc));
      vq.vring.used  = reinterpret_cast<VRingUsed *>(vnet_vring_align(reinterpret_cast<char *>(vq.vring.avail) +
								       offsetof(VRingAvail, ring[vq.vring.num]),
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

      uint32_t supported_features = 0; /* What do we support? */
      uint32_t bad = (val & ~supported_features) != 0;
      if (bad)
	logf("Guest features we don't support: %x.\n", bad);

      val &= supported_features;
      guest_features = val;
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
      q.vector = VIRTIO_MSI_NO_VECTOR;
      q.vring.num = 512;	// 512 elements per queuee
    }

    // We reattach to the switch, when the guest has pointed us to RX and TX queues.
  }

}

// EOF
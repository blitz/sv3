#include <cinttypes>
#include <virtiodevice.hh>
#include <switch.hh>
#include <session.hh>

namespace Switch {

  void VirtioDevice::receive(Port &src_port, Packet &p)
  {
    logf("%s: not implemented", __func__);
  }

  bool VirtioDevice::poll(Packet &p)
  {
    logf("%s: not implemented", __func__);
    return false;
  }


  uint64_t VirtioDevice::io_read (unsigned bar_no,
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

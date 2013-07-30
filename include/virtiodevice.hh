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

#pragma once

#include <atomic>
#include <string>

#include <externaldevice.hh>
#include <switch.hh>
#include <virtio-constants.hh>

namespace Switch {

  enum {
    INVALID_DESC_ID = ~0U,
  };

  struct VRingDesc
  {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
  };

  struct VRingAvail
  {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
  };

  struct VRingUsedElem
  {
    uint32_t id;
    uint32_t len;
  };

  struct VRingUsed
  {
    uint16_t flags;
    uint16_t idx;
    VRingUsedElem ring[];
  };

  struct VRing
  {
    // num is always equal QUEUE_ELEMENTS
    // unsigned int num;

    VRingDesc  *desc;
    VRingAvail *avail;
    VRingUsed  *used;
  };


  struct VirtQueue
  {
    VRing vring;
    uint64_t pa;
    uint16_t last_avail_idx;
    int inuse;
    uint16_t vector;

    // An entry was added to the used list and IRQs were enabled.
    bool pending_irq;
  };


  class VirtioDevice final : public ExternalDevice,
			     public Port
  {
    enum {
      MSIX_VECTORS = 3,
      VIRT_QUEUES  = 3,
      QUEUE_ELEMENTS = 1024,
    };

    int _irq_fd[MSIX_VECTORS];

    /* virtio */
    bool                 online; // Are we attached to the switch?

    uint8_t              status;
    std::atomic<uint8_t> isr;
    uint16_t             queue_sel;
    uint16_t             config_vector;

    uint32_t             guest_features;
    uint32_t             host_features;

    VirtQueue  vq[VIRT_QUEUES];

    VirtQueue &rx_vq()   { return vq[0]; }
    VirtQueue &tx_vq()   { return vq[1]; }
    VirtQueue &ctrl_vq() { return vq[2]; }

    void     vq_set_addr (VirtQueue &vq,   uint64_t addr);
    int      vq_num_heads(VirtQueue &vq,   unsigned idx);
    unsigned vq_get_head (VirtQueue &vq,   unsigned idx);
    unsigned vq_next_desc(VRingDesc *desc);

    /// Pop a set of descriptors. If last_desc is set, it will be set
    /// to the last descriptor in the chain that was popped. If
    /// chain_desc is set, it will be made to chain to the first
    /// descriptor that is popped.
    template <typename T>
    int      vq_pop_generic(VirtQueue &vq, bool writeable_bufs, T closure);
    int      vq_pop      (VirtQueue &vq, Packet &elem, bool writeable_bufs);

    /// Fill a single entry in the used ring. idx is used to fill
    /// multiple entries and is added to the current index.
    void     vq_fill     (VirtQueue &vq, unsigned head, uint32_t len, unsigned idx);

    /// Tell the guest that count entries appeared in the used ring,
    /// i.e. count descriptor chains are consumed.
    void     vq_flush    (VirtQueue &vq, unsigned count);

    /// This function combines vq_flush and vq_fill when only a single
    /// chain needs to be handled.
    void     vq_push     (VirtQueue &vq, unsigned head, uint32_t len);

    void     vq_irq      (VirtQueue &vq);

    /// Return a string describing the feature bit mask.
    static std::string features_to_string(uint32_t features);

  public:

    // ExternalDevice methods

    virtual void get_device_info(uint16_t &vendor_id,    uint16_t &device_id,
				 uint16_t &subsystem_id, uint16_t &subsystem_vendor_id)
      override;

    virtual void get_bar_info   (uint8_t   bar_no, uint32_t &size)
      override;

    virtual void get_irq_info   (uint8_t &msix_vectors)
      override;

    virtual void get_hotspot    (uint8_t  &bar_no,
				 uint16_t &addr,
				 uint8_t  &size,
				 int      &fd)
      override;

    virtual void get_msix_info  (int fd, int index,
				 bool &valid, bool &more)
      override;


    virtual uint64_t io_read (unsigned bar_no,
			      uint64_t addr,
			      unsigned size)
      override;

    virtual void io_write(unsigned bar_no,
			  uint64_t addr,
			  unsigned size,
			  uint64_t val,
			  bool &irqs_changed)
      override;

    virtual void reset() override;

    // Port methods

    virtual void enable()  override { Port::enable();  online = true; }
    virtual void disable() override { Port::disable(); online = false; }


    virtual bool poll     (Packet &p, bool enable_notifications) override;
    virtual void receive  (Packet &p) override;
    virtual void mark_done(Packet &p) override;
    virtual void poll_irq ()          override;

    VirtioDevice(Session &session);
  };

}

// EOF

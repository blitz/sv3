#pragma once

#include <atomic>

#include <externaldevice.hh>
#include <switch.hh>
#include <virtio-constants.h>

namespace Switch {

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
    unsigned vq_next_desc(VRingDesc *desc, unsigned max);

    template <typename T>
    int      vq_pop_generic(VirtQueue &vq, bool writeable_bufs, T closure);

    int      vq_pop      (VirtQueue &vq, Packet &elem, bool writeable_bufs);
    void     vq_fill     (VirtQueue &vq, unsigned head, uint32_t len, unsigned idx);
    void     vq_flush    (VirtQueue &vq, unsigned count);
    void     vq_push     (VirtQueue &vq, unsigned head, uint32_t len);
    void     vq_irq      (VirtQueue &vq);

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

#pragma once

#include <externaldevice.hh>
#include <switch.hh>

namespace Switch {

  class VirtioDevice final : public ExternalDevice,
			     public Port
  {
    #include "virtio-constants.h"

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

    virtual void receive(Port &src_port, Packet &p) override;
    virtual bool poll(Packet &p) override;

    VirtioDevice(Session &session);
  };

}

// EOF

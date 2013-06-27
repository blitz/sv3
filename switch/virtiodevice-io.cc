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

    logf("io read  %04" PRIx64 "+%x -> %" PRIx64, addr, size,
	 val & ((1 << 8*size) - 1));
    return val;
  }

  void VirtioDevice::io_write(unsigned bar_no,
			      uint64_t addr,
			      unsigned size,
			      uint64_t val,
			      bool &irqs_changed)
  {
    logf("io write %04" PRIx64 "+%x <- %" PRIx64, addr, size, val);
  }

  void VirtioDevice::reset()
  {
    disable();

    logf("Reset!");
  }

}

// EOF

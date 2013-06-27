
#include <virtiodevice.hh>
#include <switch.hh>
#include <session.hh>

namespace Switch {





  uint64_t VirtioDevice::io_read (unsigned bar_no,
				  uint64_t addr,
				  unsigned size)
  {

    return ~0ULL;
  }

  void VirtioDevice::io_write(unsigned bar_no,
			      uint64_t addr,
			      unsigned size,
			      uint64_t val,
			      bool &irqs_changed)
  {

  }

  void VirtioDevice::reset()
  {

  }

}

// EOF

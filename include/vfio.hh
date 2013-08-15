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

#include <cstdlib>
#include <cstdint>
#include <sys/ioctl.h>
#include <linux/vfio.h>

#include <vector>
#include <string>
#include <exceptions.hh>

namespace Switch {

  class VfioGroup {
    int _container;
    int _group;

  public:

    template <class T, class ...ARG>
    T *get_device(std::string device_id, ARG... arg)
    {
      int device = ioctl(_group, VFIO_GROUP_GET_DEVICE_FD, device_id.c_str());
      if (device < 0) throw SystemError("ioctl VFIO_GROUP_GET_DEVICE_FD failed.");

      return new T(*this, device, arg...);
    }

    // Map a piece of virtual memory into the device address
    // space. Mappings a are 1-to-1.
    void map_memory_to_device(void *m, size_t len, bool read, bool write);

    VfioGroup(std::string groupdev);
  };

  class VfioDevice {
    friend class VfioGroup;

    VfioGroup _group;
    int       _device;

    std::vector<vfio_region_info> _region_info;
    std::vector<vfio_irq_info>    _irq_info;

  public:

    void     map_memory_to_device(void *m, size_t len, bool read, bool write);
    void    *map_bar(int bar, size_t *size);
    void     set_irq_eventfd(unsigned idx, unsigned start, int event_fd);
    uint32_t read_config(int reg, int width);
    void     write_config(int reg, uint32_t val, int width);

    VfioDevice(VfioGroup group, int fd);
  };



}

// EOF

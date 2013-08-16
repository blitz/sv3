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

#include <cassert>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <compiler.h>
#include <vfio.hh>

namespace Switch {

  // VfioGroup

  VfioGroup::VfioGroup(std::string groupdev)
  {

    struct vfio_group_status group_status =
      { .argsz = sizeof(group_status) };
    struct vfio_iommu_type1_dma_map dma_map = { .argsz = sizeof(dma_map) };

    _container = open("/dev/vfio/vfio", O_RDWR);
    if (_container < 0)
      throw SystemError("Opening /dev/vfio/vfio failed.");

    int version;
    if ((version = ioctl(_container, VFIO_GET_API_VERSION)) != VFIO_API_VERSION)
      throw SystemError("VFIO API version mismatch.");

    if (!ioctl(_container, VFIO_CHECK_EXTENSION, VFIO_TYPE1_IOMMU))
      throw SystemError("VFIO does not understand TYPE1_IOMMU.");

    _group = open(groupdev.c_str(), O_RDWR);
    if (_group < 0) throw SystemError("Opening VFIO group device failed.");

    ioctl(_group, VFIO_GROUP_GET_STATUS, &group_status);
    if (not (group_status.flags & VFIO_GROUP_FLAGS_VIABLE))
      throw SystemError("VFIO group not usable. Did you bind all devices to vfio-pci?");

    ioctl(_group, VFIO_GROUP_SET_CONTAINER, &_container);
    ioctl(_container, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU);

  }

  void VfioGroup::map_memory_to_device(void *m, size_t len, bool read, bool write)
  {
    // We don't need mlock. VFIO_IOMMU_MAP_DMA will do this for us.

    struct vfio_iommu_type1_dma_map dma_map = { .argsz = sizeof(dma_map) };

    dma_map.flags = 0
      | (read ? VFIO_DMA_MAP_FLAG_READ : 0)
      | (write ? VFIO_DMA_MAP_FLAG_WRITE : 0);
    dma_map.vaddr = (uintptr_t)m;
    dma_map.iova  = (uintptr_t)m;
    dma_map.size  = len;

    if (0 != ioctl(_container, VFIO_IOMMU_MAP_DMA, &dma_map))
      throw SystemError("Could not map DMA memory.");
  }

  // VfioDevice

  VfioDevice::VfioDevice(VfioGroup group, int fd)
    : _group(group), _device(fd)
  {
    struct vfio_device_info device_info = { .argsz = sizeof(device_info) };

    ioctl(_device, VFIO_DEVICE_GET_INFO, &device_info);

    for (unsigned i = 0; i < device_info.num_regions; i++) {
      struct vfio_region_info reg = { .argsz = sizeof(reg) };

      // XXX http://gcc.gnu.org/bugzilla/show_bug.cgi?id=55606
      reg.index = i;

      ioctl(_device, VFIO_DEVICE_GET_REGION_INFO, &reg);
      _region_info.push_back(reg);
    }

    for (unsigned i = 0; i < device_info.num_irqs; i++) {
      struct vfio_irq_info irq = { .argsz = sizeof(irq) };

      // XXX http://gcc.gnu.org/bugzilla/show_bug.cgi?id=55606
      irq.index = i;

      ioctl(_device, VFIO_DEVICE_GET_IRQ_INFO, &irq);
      _irq_info.push_back(irq);
    }


  }

  void VfioDevice::map_memory_to_device(void *m, size_t len, bool read, bool write)
  {
    _group.map_memory_to_device(m, len, read, write);
  }

  void VfioDevice::write_config(int reg, uint32_t val, int width)
  {
    off_t config_offset = _region_info[VFIO_PCI_CONFIG_REGION_INDEX].offset;
    int res = pwrite(_device, &val, width, config_offset + reg);
    if (res != width)
      throw SystemError("Could not write config space at %x+%x.", reg, width);
  }

  uint32_t VfioDevice::read_config(int reg, int width)
  {
    uint32_t ret = 0;
    assert(reg < 0x100);

    off_t config_offset = _region_info[VFIO_PCI_CONFIG_REGION_INDEX].offset;
    int res = pread(_device, &ret, width, config_offset + reg);
    if (res != width)
      throw SystemError("Could not read config space at %x+%x.", reg, width);
    return ret;
  }

  void VfioDevice::set_irq_eventfd(unsigned idx, unsigned start, int event_fd)
  {
    constexpr size_t argsz = sizeof(vfio_irq_set) + sizeof(int);
    union {
      // Allocate backing store for the data part of the struct.
      char buf[argsz];
      struct vfio_irq_set irq_set;
    };

    irq_set.argsz = argsz;
    irq_set.flags = VFIO_IRQ_SET_DATA_EVENTFD | VFIO_IRQ_SET_ACTION_TRIGGER;
    irq_set.index = idx;
    irq_set.start = start;
    irq_set.count = 1;

    // Use memcpy here to avoid aliasing warning.
    memcpy(irq_set.data, &event_fd, sizeof(event_fd));

    if (0 != ioctl(_device, VFIO_DEVICE_SET_IRQS, &irq_set))
      throw SystemError("Could not set VFIO IRQ properties.");
  }

  void *VfioDevice::map_bar(int bar, size_t *size)
  {
    if (not (_region_info[bar].flags & VFIO_REGION_INFO_FLAG_MMAP)) {
      return nullptr;
    }

    void * res = mmap(nullptr, _region_info[bar].size, PROT_READ | PROT_WRITE, MAP_SHARED, _device,
		      _region_info[bar].offset);
    if (res == MAP_FAILED)
      throw SystemError("Mapping BAR %u of VFIO device failed.", bar);

    return res;
  }

}

// EOF

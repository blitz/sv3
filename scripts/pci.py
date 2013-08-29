# -*- Mode: Python -*-

import os
import struct

def readlink_base(name):
    return os.path.basename(os.readlink(name))

def get_driver(pciid):
    return readlink_base("/sys/bus/pci/devices/%s/driver" % pciid)

def get_iommu_group(pciid):
    return readlink_base("/sys/bus/pci/devices/%s/iommu_group" % pciid)

def get_iommu_group_devices(group):
    """Returns a list of all devices in an IOMMU group"""
    return os.listdir("/sys/kernel/iommu_groups/%s/devices/" % group)

def device_config(pciid):
    with open("/sys/bus/pci/devices/%s/config" % pciid) as c:
        config = c.read()
        (vendor, device, b, c, d, subclasscode, classcode) = struct.unpack("HHIBBBB", config[0:12])
        return {'vendor'   : vendor,
                'device'   : device,
                'class'    : classcode,
                'subclass' : subclasscode }

def unbind(pciid):
    """Unbind a PCI device from its device driver."""
    if os.path.exists("/sys/bus/pci/devices/%s/driver" % pciid):
        with open("/sys/bus/pci/devices/%s/driver/unbind" % pciid, "w") as u:
            u.write(pciid)

def bind(driver, pciid):
    """Bind a PCI device to a device driver."""
    with open("/sys/bus/pci/drivers/%s/new_id" % driver, "w") as u:
        c = device_config(pciid)
        u.write("%x %x" % (c['vendor'], c['device']))

def device_is_bridge(pciid):
    return device_config(pciid)['class'] == 0x06

def bind_to_vfio(pciid):
    """Bind a PCI device to VFIO. Makes sure to attach all devices in the same IOMMU group."""
    if os.system("modprobe vfio-pci") != 0:
        raise "Could not load vfio-pci."
    group = get_iommu_group(pciid)
    devs  = get_iommu_group_devices(group)
    assert(pciid in devs)
    for d in devs:
        if device_is_bridge(d):
            continue
        unbind(d)
        bind("vfio-pci", d)

# EOF

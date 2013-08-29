#!/usr/bin/env python2

from __future__ import print_function
import pci
import sys

if len(sys.argv) != 2:
    print("Usage: vfio-rebind.py pci-id")

dev = sys.argv[1]
group = pci.get_iommu_group(dev)
print("Device belongs to IOMMU %s." % group)
print("All devices in this group are:")
for d in pci.get_iommu_group_devices(group):
    if not pci.device_is_bridge(d):
        print("%s" % d)

raw_input("Press ENTER to bind those devices to vfio-pci... ")
pci.bind_to_vfio(dev)

# EOF

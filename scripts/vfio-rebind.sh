#!/bin/sh

if [ $# -ne 2 ]; then
    echo Usage: $0 pciid user
    exit 1
fi

if [ -z "$SUDO_COMMAND" ]; then
    echo You probably want to run this with sudo.
fi

echo PCI ID is $1
echo User is $2

modprobe vfio-pci || exit 1

if [ ! -d /sys/bus/pci/devices/0000:02:00.0/iommu_group ]; then
    echo IO-MMU not enabled or PCI ID wrong?
    exit 1
fi

IOMMU_GROUP=$(basename $(readlink /sys/bus/pci/devices/0000:02:00.0/iommu_group))

echo IOMMU group $IOMMU_GROUP

PCI_VENDOR_DEVICE=$(lspci -n -s 0000:02:00.0 | sed -E 's/.*: (....):(....).*/\1 \2/')

echo $1                 > /sys/bus/pci/devices/$1/driver/unbind
echo $PCI_VENDOR_DEVICE > /sys/bus/pci/drivers/vfio-pci/new_id

chown $2 /dev/vfio/$IOMMU_GROUP

echo "You need to bind the following devices to vfio-pci: (you can ignore PCI bridges)"
for id in `ls /sys/bus/pci/devices/0000:02:00.0/iommu_group/devices`; do
    if [ ! -e /sys/bus/pci/drivers/vfio-pci/$id ]; then
	lspci -s $id
    fi
done

# EOF

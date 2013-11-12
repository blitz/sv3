#!/bin/sh

echo "Read instructions in this file."
# Remove this when you have read the rest of this file.
exit 1


# Step 0 Use kernel with patch applied: ../contrib/linux-*-latency-experiment.diff

# Step 1 Checkout github.com/blitz/NUL latency-experiment branch and
# point NUL_PATH there.
NUL_PATH=$HOME/src/nul

# Build it...
scons -C $NUL_PATH/build -j8 || exit 1

# Step 2 Configure a macvtap device using
# ip link add link eth0 name latency0 type macvtap mode bridge
# ip link set dev latency0 up
# This will create a /dev/tapX device. Change this to fit:
TAP_DEVICE=/dev/tap6


# Step 3 Run qemu:
QEMU=../contrib/qemu/x86_64-softmmu/qemu-system-x86_64


# Now either run the vhost version ...
#exec $QEMU -enable-kvm -machine q35 -netdev type=tap,id=guest,vhost=on,fd=3 -device virtio-net-pci,netdev=guest -kernel $NUL_PATH/build/bin/apps/hypervisor -append serial -initrd "$NUL_PATH/build/bin/apps/sigma0.nul S0_DEFAULT hostserial hostvirtionet:0" -serial stdio 3<>$TAP_DEVICE | tee microbenchmark-vhost.log

# ... or the sv3 version:
exec $QEMU -enable-kvm -mem-path /tmp -machine q35 -device externalpci,socket=/tmp/sv3 -kernel $NUL_PATH/build/bin/apps/hypervisor -append serial -initrd "$NUL_PATH/build/bin/apps/sigma0.nul S0_DEFAULT hostserial hostvirtionet:0" -serial stdio | tee microbenchmark-sv3.log

# EOF

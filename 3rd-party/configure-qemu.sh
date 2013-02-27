#!/bin/sh

cd qemu-kvm && ./configure --disable-virtfs --disable-seccomp --disable-guest-agent --disable-usb-redir --disable-smartcard --disable-libiscsi --disable-spice --disable-bluez --disable-glusterfs --disable-xen --disable-brlapi --disable-vnc --target-list=x86_64-softmmu

#!/bin/sh

if [ $# -ne 2 ]; then
    echo "Usage: $0 netif macvtap-name"
    exit 1
fi

ip link add link $1 name $2 type macvtap || exit 1
ip link show $2 || exit 1

basename /sys/class/net/$2/tap*



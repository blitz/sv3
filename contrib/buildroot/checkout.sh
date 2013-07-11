#!/bin/sh -x

if [ $# -ne 1 ]; then
    echo "Usage: $0 output-directory"
    exit 1
fi

OUTPUTDIR=$1
CLONEURL=git://git.buildroot.net/buildroot

git clone --depth 100 "$CLONEURL" "$OUTPUTDIR" || exit 1

LINUX_CONFIG=$PWD/linux-guest-config

cat buildroot-config | sed 's,^.*BR2_LINUX_KERNEL_CUSTOM_CONFIG_FILE.*$,BR2_LINUX_KERNEL_CUSTOM_CONFIG_FILE="'$LINUX_CONFIG'",' > $OUTPUTDIR/.config


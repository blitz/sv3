#!/bin/sh

for gov in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo performance > $gov
done

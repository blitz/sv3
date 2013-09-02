# -*- Mode: Python -*-
# Measure CPU utilization

STAT_USER    = 0
STAT_NICE    = 1
STAT_SYSTEM  = 2
STAT_IDLE    = 3
STAT_IRQ     = 5
STAT_SOFTIRQ = 6
STAT_GUEST   = 8

def list_subtract(l1, l2):
    assert len(l1) == len(l2)
    return [a-b for (a, b) in zip(l1,l2)]

def read_proc_stat():
    res = {}
    with open("/proc/stat") as stat:
        for line in stat:
            splitted = line.split()
            res[splitted[0]] = [int(i) for i in splitted[1:]]
    return res

def relative_proc(old, new):
    res = {}
    for key in old:
        res[key] = list_subtract(new[key], old[key])
    return res

# EOF

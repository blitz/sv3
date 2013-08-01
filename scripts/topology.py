# -*- Mode: Python -*-

import glob

# Utilities

def read_int(fn):
    return int(open(fn).read())

def id_set(glob_name):
    s = set()
    for cpu_name in glob.glob(glob_name):
        s.add(read_int(cpu_name))
    return s

# Topology data structures

class Thread:
    def __repr__(self):
        return "<T%u>" % self.id

    def __init__(self, t_id, fn):
        self.id = t_id
        self.filename = fn

class Core:
    def __repr__(self):
        return "<%s>" % self.thread

    def __init__(self, c_id):
        self.id = c_id
        self.thread = []
        for n in glob.glob("/sys/devices/system/cpu/cpu[0-9]*/topology/core_id"):
            core_id = read_int(n)
            if core_id == c_id:
                t_id = int(n.split("/")[5][3:])
                self.thread.append(Thread(t_id, n))

class Package:
    def __init__(self, p_id):
        self.id = p_id
        n = "/sys/devices/system/cpu/cpu[0-9]*/topology/core_id"
        self.core = [Core(i) for i in id_set(n)]

def get_topology():
    n = "/sys/devices/system/cpu/cpu[0-9]*/topology/physical_package_id"
    return [Package(i) for i in id_set(n)]
        

def main():
    pass

if __name__ == "__main__":
    main()

# EOF

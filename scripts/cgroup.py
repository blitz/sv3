# -*- Mode: Python -*-

# This is not a generic cgroup library.

import os
import pexpect
import pwd

USER_HZ = os.sysconf(os.sysconf_names['SC_CLK_TCK'])

def cgcreate(name):
    user = pwd.getpwuid(os.getuid())[0]
    return os.system("cgcreate -a %s:%s -t %s:%s -g cpuacct:/%s" % (user, user, user, user, name))

def cgdelete(name):
    return os.system("cgdelete cpuacct:/%s" % name)

def cgexec_cmd(name):
    return "cgexec --sticky -g cpuacct:/%s " % name


# XXX Collect cpuacct.usage instead. No sys/user distinction but more accurate (nanoseconds).
class CpuUsage:

    def delta(old, new, time):
        """Returns tuple with user and system cpu usage (as fraction between 0 and 1)."""
        return (new.cpu - old.cpu) / time

    def __repr__(self):
        return "<CPU %.2fs>" % (self.cpu)

    def __init__(self, cpu):
        self.cpu = cpu

def cgget_usage(name):
    with open('/sys/fs/cgroup/cpuacct/%s/cpuacct.usage' % name, 'r') as s:
       cpu = float(s.read()) / 1000000000
       return CpuUsage(cpu)


# EOF

#!/usr/bin/env python2
# -*- coding: utf-8 -*-
from __future__ import print_function

import time
import datetime
import os
import sys
import glob
import itertools
import time
import pexpect as e
import csv
import argparse
import random
import platform
import json
import gzip

# Local "libraries"
import topology
import cgroup
import pci
import cpuutil
from stddev import StdDev

# XXX TODO Interrupt affinity

def thread_list_to_str(tl):
    return ",".join([str(t.id) for t in tl])

# Connection to remote machine that will be our load generator
loadgen = e.spawn("ssh testuser@cosel")

# The PCI ID of our network device. We need this to bind it to VFIO.
net_if_pciid  = '0000:02:00.1'

# Original driver. We need this to reattach the driver after the
# benchmark run.
net_if_driver = 'ixgbe'

# This is only used for vhost benchmarks. We attach macvtap interfaces
# to this external NIC.
net_if    = "p33p2"

# Load modules
if os.system("modprobe vhost-net") != 0:
    print("Couldn't load vhost-net?")
    exit(1)

# Remove vfio-pci driver. This frees up all devices it has bound to
# it. We can attach the original network drivers without problems.
if pci.driver_exists("vfio-pci") and os.system("rmmod vfio-pci") != 0:
    print("Couldn't unload vfio-pci?")
    exit(1)

cur_driver = pci.get_driver(net_if_pciid)
if cur_driver != net_if_driver:
    print("Rebinding NIC to original kernel driver.")
    pci.bind(net_if_driver, net_if_pciid)

        
qemu_bin  = "../contrib/qemu/x86_64-softmmu/qemu-system-x86_64"
linux_bin = "../contrib/buildroot/buildroot/output/images/bzImage"
qemu_generic = "-enable-kvm -m 2048 -nographic -kernel " + linux_bin + " -initrd ../contrib/benchcat/benchcat.cpio -append loglevel=0\ quiet\ console=ttyS0\ clocksource=tsc"

all_cgroups = ["js-switch", "js-server", "js-client"]

def check_setup():
    for cpu in os.listdir("/sys/devices/system/cpu"):
        gov = ("/sys/devices/system/cpu/%s/cpufreq/scaling_governor" % cpu)
        if os.path.exists(gov):
            with open(gov, "a+") as c:
                old = c.read().strip()
                new = "performance"
                c.write(new)
                if old != new:
                    print("%s: %s -> performance" % (cpu, old))
    
    if not os.path.exists(qemu_bin):
        print("Qemu not found at: " + qemu_bin)
        exit(1)


def qemu_quit(p):
    p.sendcontrol('a')
    p.send('x')
    p.expect(e.EOF)
    p.close(force = True)

def clear_line():
    print("[2K\r", end="")


# A list of all experiments we ran.
experiments = []

class Run:
    def remember(self, key, value):
        self.details[key] = value

    def elapsed(self):
        return time.time() - self.details['time_start']

    def __enter__(self):
        assert self.start_proc == None
        clear_line()
        print("Running %s (%d) ..." % (self.experiment.name, len(self.experiment.runs)+1), end="")
        sys.stdout.flush()
        self.details = {}
        self.details['time_start'] = time.time()
        self.start_proc = cpuutil.read_proc_stat()
        return self

    def __exit__(self, etype, value, trace):
        if (etype):
            return False
        end_proc = cpuutil.read_proc_stat()
        self.details['procstat'] = cpuutil.relative_proc(self.start_proc, end_proc)
        self.details['time_end'] = time.time()
        self.experiment.runs.append(self.details)

        return False

    def __init__(self, experiment):
        self.experiment = experiment
        self.start_proc = None

class Experiment:

    def __str__(self):
        return "<EXP %s (%s): %s>" % (self.name, self.parameters, [i['value'] for i in self.runs])

    def done(self):
        experiments.append({'name'  : self.name,
                            'date'  : self.date,
                            'uname' : platform.uname(),
                            'parameters' : self.parameters,
                            'runs' : self.runs })
    def run(self):
        return Run(self)

    def __enter__(self):
        self.start = time.time()
        return self

    def __exit__(self, etype, value, trace):
        self.done()
        duration = time.time() - self.start
        clear_line()
        print("Finished %s in %dmin %02ds." % (self.name, duration / 60, duration % 60))
        print(self)
        return False

    def __init__(self, name, parameters):
        self.name  = name
        self.date  = time.time()
        self.parameters = parameters
        self.runs  = []

def create_and_configure(qemu_cmd, is_server, cpus):
    print("Starting %s VM..." % ("server" if is_server else "client"))
    qemu_cmd = cgroup.cgexec_cmd("js-%s" % ("server" if is_server else "client")) + " taskset -c %s sh -c '%s'" % (thread_list_to_str(cpus), qemu_cmd)
    #print(qemu_cmd)
    p = e.spawn(qemu_cmd, timeout=20)
    p.setecho(False)
    p.expect_exact("buildroot login: ")
    p.sendline("root")
    p.expect("\r\n")
    p.sendline("ifconfig eth0 up %s && echo return $?" % ("192.168.1.100" if is_server else "192.168.1.200"))
    r = p.expect (["return 0", "No such device"])
    if r != 0:
        print("Network configuration failed: %d" % r, file=sys.stderr)
        exit(1)
    if is_server:
        p.sendline("netserver")
        p.expect_exact(r"Starting netserver with host 'IN(6)ADDR_ANY' port '12865' and family AF_UNSPEC")
        # p.sendline("iperf -s")
        # p.expect_exact("Server listening on TCP port 5001")
    return p

def netperf_rr_like(client, test):
    client.sendline("netperf -t %s -H 192.168.1.100" % test)
    client.expect_exact("Socket Size   Request  Resp.   Elapsed  Trans.")
    client.expect_exact("Send   Recv   Size     Size    Time     Rate")
    client.expect_exact("bytes  Bytes  bytes    bytes   secs.    per sec")
    client.expect(r"\d+\s+\d+\s+\d+\s+\d+\s+\d+\.\d+\s+(\d+\.\d+)\s*\r\n")
    return float(client.match.group(1))

def netperf_stream_like(client, test):
    client.sendline("netperf -t %s -H 192.168.1.100" % test)
    client.expect_exact("Recv   Send    Send")
    client.expect_exact("Socket Socket  Message  Elapsed")
    client.expect_exact("Size   Size    Size     Time     Throughput")
    client.expect_exact("bytes  bytes   bytes    secs.    10^6bits/sec")
    client.expect(r"\d+\s+\d+\s+\d+\s+\d+\.\d+\s+(\d+\.\d+)\s*\r\n")
    return float(client.match.group(1))

def benchcat(client, server, target_mbit, connections, active):
    #client.sendline("benchcat ")
    print("Benchcat benchmark not implemented yet...")

def repeat_benchmark(name, repeat, parameters, thunk):
    with Experiment(name, parameters) as e:
        for i in range(repeat):
            with e.run() as run:
                start_usage = dict([ (cg, cgroup.cgget_usage(cg)) for cg in all_cgroups ])
                run.remember("value", thunk())
                end_usage   = dict([ (cg, cgroup.cgget_usage(cg)) for cg in all_cgroups ])
                for g in all_cgroups:
                    run.remember(g + "-usage", cgroup.CpuUsage.delta(start_usage[g], end_usage[g], run.elapsed()))

def set_tso(session, enable):
    on_off = "on" if enable else "off"
    ethtool_cmd = "ethtool -K eth0 tx-tcp-segmentation %s tx-tcp-ecn-segmentation %s tx-tcp6-segmentation %s"
    session.sendline(ethtool_cmd % ((on_off,)*3))
    # Check whether we configured correctly
    session.sendline("ethtool -k eth0")
    session.expect_exact("tcp-segmentation-offload: %s" % on_off)


def repeat_benchcat(prefix, client, server, repeat, parameters):
    for target_mbit in range(1000,9001,1000):
        for connections in [1, 4, 8]:
            for active in [True, False]:
                repeat_benchmark(prefix + "_benchcat", repeat, dict({"target_mbit" : mbit,
                                                                     "benchcat_connections": connections,
                                                                     "active": active},
                                                                    **parameters),
                                 lambda: benchcat(client, server, target_mbit, connections, active))

def repeat_stream_benchmarks(prefix, client, server, repeat, parameters):
    repeat_benchmark(prefix + "_tcp_stream", repeat, tso_parameters,
                     lambda: netperf_stream_like(client, "TCP_STREAM"))
    repeat_benchcat(prefix, client, server, tso_parameters)

repeat = 2
def run_benchmark(prefix, client, server, parameters = {}):

    repeat_benchmark(prefix + "_udp_rr", repeat, parameters, lambda: 1000000/netperf_rr_like(client, "UDP_RR"))
    # repeat_benchmark(prefix + "_tcp_rr", repeat, lambda: 1000000/netperf_rr_like(client, "TCP_RR"))
    # repeat_benchmark(prefix + "_tcp_cc",  repeat, lambda: 1000000/netperf_rr_like(client, "TCP_CC"))
    # repeat_benchmark(prefix + "_tcp_crr", repeat, lambda: 1000000/netperf_rr_like(client, "TCP_CRR"))

    for tso in [True, False]:
        set_tso(client, tso)
        set_tso(server, tso)
        repeat_stream_benchmarks(prefix, client, server, repeat, dict({"tso" : tso}, **parameters))

def run_externalpci_benchmark(switch_cpus, server_cpus, client_cpus, poll_us, batch_size):
    print("--- externalpci ---")
    print("Switch on %s, server on %s, client on %s." % (switch_cpus, server_cpus, client_cpus))
    qemu_externalpci_args = "-mem-path /tmp -device externalpci,socket=/tmp/sv3 -net none "
    qemu_cmd = "%s %s %s" % (qemu_bin, qemu_externalpci_args, qemu_generic)

    switch_cmd = cgroup.cgexec_cmd("js-switch")
    switch_cmd += " taskset -c %s ../sv3 -f --poll-us %u --batch-size %u" % (thread_list_to_str(switch_cpus), poll_us, batch_size)
    #print(switch_cmd)
    switch = e.spawn(switch_cmd)
    r = switch.expect_exact(["Built with optimal compiler flags.", "Do not use for benchmarking"])
    if (r != 0):
        print("Switch was not built for benchmarking!")
        raise CommandFailed()
    print("Switch is running.")

    server = None
    client = None

    try:
        server = create_and_configure(qemu_cmd, True, server_cpus)
        client = create_and_configure(qemu_cmd, False, client_cpus)
        run_benchmark("epci", client, server, {'poll_us' : poll_us, 'batch_size' : batch_size })
    except:
	print("Unexpected error:", sys.exc_info()[0])

    if client:
        qemu_quit(client)

    if server:
        qemu_quit(server)

    switch.close(force = True)

class CommandFailed(BaseException):
    pass

def create_macvtap(netif, macvtap, mac):
    """Create a macvtap devices connected to netif. Returns the name of the 
    corresponding tap device."""
    output, ex = e.run("ip link add link %s name %s address %s type macvtap mode bridge" % (netif, macvtap, mac),
                       withexitstatus = True)
    if ex != 0:
        print(("Creating '%s' failed:" % macvtap) + output, end="")
        raise CommandFailed()
    e.run("ip link set dev %s up" % macvtap)
    tap_files = glob.glob("/sys/class/net/%s/tap*" % macvtap)
    assert len(tap_files) == 1
    devnode = "/dev/" + os.path.basename(tap_files[0])
    return devnode

def delete_macvtap(macvtap):
    e.run("ip link delete %s type macvtap" % macvtap)

def run_vhost_benchmark(server_cpus, client_cpus):
    print("\n--- vhost ---")
    print("Server on %s, client on %s." % (server_cpus, client_cpus))
    cleanup_fn = []
    try:
        def mt(name, tapname, mac):
            tap = create_macvtap(net_if, tapname, mac)
            cleanup_fn.append(lambda: delete_macvtap(tapname))
            return tap

        client_mac = "1a:54:0b:ca:bc:10"
        client_macvtap = "macvtap0"
        qemu_vhost_args = " -netdev type=tap,id=guest,vhost=on,fd=3 -device virtio-net-pci,netdev=guest,mac=%s  3<>%s"
        qemu_vhost_client_args = qemu_vhost_args % (client_mac, mt(net_if, client_macvtap, client_mac))
        client = create_and_configure("%s %s %s" % (qemu_bin, qemu_generic, qemu_vhost_client_args), False, server_cpus)

        server_mac = "1a:54:0b:ca:bc:20"
        server_macvtap = "macvtap1"
        qemu_vhost_server_args = qemu_vhost_args % (server_mac, mt(net_if, server_macvtap, server_mac))
        server = create_and_configure("%s %s %s" % (qemu_bin, qemu_generic, qemu_vhost_server_args), True, client_cpus)

        try:
            run_benchmark("vhost", client, server)
        finally:
            qemu_quit(client)
            qemu_quit(server)
    finally:
        cleanup_fn.reverse()
        for f in cleanup_fn:
            f()

def main(args):
    check_setup()

    print("Setting up control groups...")
    for cg in all_cgroups:
        cgroup.cgdelete(cg)
        if 0 != cgroup.cgcreate(cg):
            print("Could not create control group %s." % cg)
            exit(1)

    topo = topology.get_topology()
    print("Topology: %u Package x %u Cores x %u Threads" % (len(topo), len(topo[0].core), len(topo[0].core[0].thread)))

    if (len(topo[-1].core) < 3):
        print("ERROR: We need 3 cores in a package for optimal thread pinning.")
        if not "--ignore-warnings" in args:
            exit(1)
        any_cpu = "1-64"
        switch_cpus = any_cpu
        server_cpus = any_cpu
        client_cpus = any_cpu
    else:
        core_list = topo[-1].core[-4:]
        switch_cpus = [core_list[0].thread[-1], core_list[1].thread[-1]]
        server_cpus = [core_list[2].thread[-1]]
        client_cpus = [core_list[3].thread[-1]]
    
    print("Let the benchmarking commence!")

    try:
        run_vhost_benchmark([switch_cpus[0]] + server_cpus, [switch_cpus[1]] + client_cpus)

        poll_v  = [0, 5, 10, 50, 100, 1000]
        batch_v = [1, 16, 32]

        configurations = [ x for x in itertools.product(poll_v, batch_v)]
        random.shuffle(configurations)

        for n in range(len(configurations)):
            poll_us, batch_size = configurations[n]
            run_externalpci_benchmark(switch_cpus[:1], server_cpus, client_cpus, poll_us, batch_size)
    finally:
        global experiments
        if len(experiments) > 0:
            filename = datetime.datetime.today().strftime("results-%Y%m%d-%H%m%S.json.gz")
            with gzip.open(filename, "wb") as f:
                f.write(json.dumps(experiments))
            print("Written %s." % filename)

if __name__ == "__main__":
    main(sys.argv)

# EOF

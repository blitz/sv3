#!/usr/bin/env python2
# -*- coding: utf-8 -*-
from __future__ import print_function

# I am obviously not good at this. Improvements welcome. :)

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
import re
import traceback

# Local "libraries"
import topology
import cgroup
import pci
import cpuutil
from stddev import StdDev

# XXX TODO Interrupt affinity

def thread_list_to_str(tl):
    return ",".join([str(t.id) for t in tl])

# How often do we want to repeat individual measurements
rr_repeat     = 3
stream_repeat = 5

# We want to do stream measurements with what number of connections?
connection_numbers = [1]

# Connection to remote machine that will be our load generator
loadgen_host = "testuser@cosel"

# The PCI ID of our network device. We need this to bind it to VFIO.
net_if_pciid  = '0000:01:00.1'

# Original driver. We need this to reattach the driver after the
# benchmark run.
net_if_driver = 'ixgbe'

# This is only used for vhost benchmarks. We attach macvtap interfaces
# to this external NIC.
net_if    = "p1p2"

# XXX Check that $net_if is a network device provided by $net_if_pciid

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
qemu_generic = "-enable-kvm -m 2048 -nographic -kernel " + linux_bin + " -append loglevel=0\ quiet\ console=ttyS0\ clocksource=tsc"

all_cgroups = ["js-switch", "js-server", "js-client"]

def check_setup():
    for service in ["cups", "sendmail", "crashplan", "irqbalance", "gdm", "btsync"]:
        os.system("systemctl stop %s" % service)

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
        #print(self)
        return False

    def __init__(self, name, parameters):
        print("Running '%s': %s" % (name, parameters))
        self.name  = name
        self.date  = time.time()
        self.parameters = parameters
        self.runs  = []

def create_and_configure(qemu_cmd, is_server):
    print("Starting %s VM..." % ("server" if is_server else "client"))
    cqemu_cmd  = cgroup.cgexec_cmd("js-%s" % ("server" if is_server else "client"))
    #cqemu_cmd += " taskset -c %s " % (thread_list_to_str(cpus))
    cqemu_cmd += " sh -c '%s'" % qemu_cmd

    p = e.spawn(cqemu_cmd, timeout=20)
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
    client.sendline("netperf -l 5 -t %s -H 192.168.1.100" % test)
    client.expect_exact("Socket Size   Request  Resp.   Elapsed  Trans.", timeout=20)
    client.expect_exact("Send   Recv   Size     Size    Time     Rate")
    client.expect_exact("bytes  Bytes  bytes    bytes   secs.    per sec")
    client.expect(r"\d+\s+\d+\s+\d+\s+\d+\s+\d+\.\d+\s+(\d+\.\d+)\s*\r\n")
    return float(client.match.group(1))

def netperf_stream_like(client, test):
    client.sendline("netperf -l 20 -t %s -H 192.168.1.100" % test)
    client.expect_exact("Recv   Send    Send", timeout = 30)
    client.expect_exact("Socket Socket  Message  Elapsed")
    client.expect_exact("Size   Size    Size     Time     Throughput")
    client.expect_exact("bytes  bytes   bytes    secs.    10^6bits/sec")
    client.expect(r"\d+\s+\d+\s+\d+\s+\d+\.\d+\s+(\d+\.\d+)\s*\r\n")
    return float(client.match.group(1))

def repeat_benchmark(name, repeat, parameters, thunk):
    with Experiment(name, parameters) as e:
        for i in range(repeat):
            with e.run() as run:
                start_usage = dict([ (cg, cgroup.cgget_usage(cg)) for cg in all_cgroups ])
                run.remember("value", thunk())
                end_usage   = dict([ (cg, cgroup.cgget_usage(cg)) for cg in all_cgroups ])
                for g in all_cgroups:
                    run.remember(g + "-usage", cgroup.CpuUsage.delta(start_usage[g], end_usage[g], run.elapsed()))

def get_nic(session):
    # Pipe via cat otherwise GNU grep will use colors. We cannot use
    # --color=never, because busybox grep doesn't understand that...
    session.sendline("ip route get 192.168.1.254 | grep ' dev ' | cat")
    session.expect(r".*dev ([a-z0-9]+)\s.*\r\n", timeout=4)
    return session.match.group(1)

def set_tso(session, enable, nic = None):
    try:
        on_off = "on" if enable else "off"
        netdev = nic if nic else get_nic(session)
        ethtool_cmd = "sudo ethtool -K %s tx-tcp-segmentation %s tx-tcp-ecn-segmentation %s tx-tcp6-segmentation %s lro %s"
        session.sendline(ethtool_cmd % (netdev, on_off, on_off, on_off, on_off))
        # Check whether we configured correctly
        session.sendline("ethtool -k %s" % netdev)
        session.expect_exact("tcp-segmentation-offload: %s" % on_off)
    except Exception, e:
        print(e)
        session.interact()

def set_irq_rate(session, rate, nic = None):
    try:
        netdev = nic if nic else get_nic(session)
	print("Setting IRQ usecs to %u." % int(1000000.0 / rate))
        ethtool_cmd = "sudo ethtool -C %s rx-usecs %u" % (netdev, int(1000000.0 / rate))
        session.sendline(ethtool_cmd)
    except Exception, e:
        print(e)
        session.interact()


def nuttcp(client, server, target_mbit, connections, active, bw_reached_fn):
    timeout = 30
    client.sendline("nuttcp -fparse %s -T%d -Inuttcp -N%d %s 192.168.1.100" % ("-t" if active else "-r",
                                                                               timeout, connections,
                                                                               ("-R%dM" % target_mbit) if target_mbit != 0 else ""))
    client.expect(r"nuttcp: (.*)\r\n", timeout = timeout + 5)
    results = {}
    for [key, value] in [x.split("=") for x in client.match.group(1).split()]:
        results[key] = float(value)
    relative_error = abs(target_mbit - results['rate_Mbps'])/target_mbit if target_mbit != 0 else 0
    if (relative_error > 0.02):
        print("Requested target bitrate of %dMBit/s not achieved. Got %dMBit/s." % (target_mbit, results['rate_Mbps']))
        bw_reached_fn(False)
    else:
        bw_reached_fn(connections)
    return results

def repeat_nuttcp(client, server, repeat, parameters):
    connections = set(connection_numbers)
    for target_mbit in range(0,10000,1000) + range(10000, 100000, 2000):
        # Continue as long as we somehow reach the target bandwidth
        reached = set()
        def bw_reached(b):
            if b:
                reached.add(b)
        for c in connections:
            for active in [True]: #[True, False]
                print("nuttcp: %sMBit/s N%s A%s" % (target_mbit, c, active))
                repeat_benchmark("nuttcp", repeat, dict({"target_mbit" : target_mbit,
                                                         "nuttcp_connections": c,
                                                         "active": active},
                                                        **parameters),
                                 lambda: nuttcp(client, server, target_mbit, c, active, bw_reached))
        if len(reached) == 0:
            print("Skipped remaining bandwidth tests at %dMBit/s." % target_mbit)
            break
        if len(reached) < len(connections):
            print("We only try %s parallel connections." % reached)
        connections = reached


def repeat_stream_benchmarks(client, server, repeat, parameters):
    repeat_benchmark("tcp_stream", repeat, parameters,
                     lambda: netperf_stream_like(client, "TCP_STREAM"))
    repeat_nuttcp(client, server, repeat, parameters)

def run_benchmark(vm_client, vm_server, default_parameters = {}):
    loadgen_client = e.spawn("ssh %s" % loadgen_host)
    for use_loadgen in [True, False]:
        parameters = dict(**default_parameters)
        if use_loadgen:
            parameters['loadgenerator'] = "external"
            bench_client = loadgen_client
            bench_client.sendline("sudo arp -d -a")
            #vm_client.sendline("sleep 1 && ifconfig eth0 down && sleep 1")
        else:
            parameters['loadgenerator'] = "VM"
            bench_client = vm_client
            #vm_client.sendline("sleep 1 && ifconfig eth0 up && sleep 1")
        print("Checking whether client is up...")
        bench_client.sendline("ping -c 2 192.168.1.100")
        bench_client.expect("bytes from", timeout=10)
        bench_client.expect("bytes from", timeout=2)
        print("It's alive!")

        #repeat_stream_benchmarks(bench_client, vm_server, stream_repeat, parameters)
        repeat_benchmark("tcp_rr", rr_repeat, parameters, lambda: 1000000/netperf_rr_like(bench_client, "TCP_RR"))
        repeat_benchmark("udp_rr", rr_repeat, parameters, lambda: 1000000/netperf_rr_like(bench_client, "UDP_RR"))


def run_externalpci_benchmark(switch_cpus, tso, poll_us, batch_size, irq_rate):
    print("--- externalpci ---")
    print("Switch on %s." % (switch_cpus))
    qemu_externalpci_args = "-mem-path /tmp -device externalpci,socket=/tmp/sv3 -net none "
    qemu_cmd = "%s %s %s" % (qemu_bin, qemu_externalpci_args, qemu_generic)

    switch = None
    server = None
    client = None

    try:
        print("Configuring vfio ...")
        pci.bind_to_vfio(net_if_pciid)
        iommu_group = pci.get_iommu_group(net_if_pciid)

        switch_cmd  = cgroup.cgexec_cmd("js-switch")
        switch_cmd += " taskset -c %s " % thread_list_to_str(switch_cpus)
        switch_cmd += "../sv3 -f --upstream-port %s,device=/dev/vfio/%s,pciid=%s,tso=%s,irq_rate=%s --poll-us %u --batch-size %u" % (net_if_driver, iommu_group, net_if_pciid, 1 if tso else 0,irq_rate, poll_us, batch_size)
        #print(switch_cmd)
        print("Starting switch ...")
        switch = e.spawn(switch_cmd)
        r = switch.expect_exact(["Built with optimal compiler flags.", "Do not use for benchmarking"])
        if (r != 0):
            print("Switch was not built for benchmarking!")
            raise CommandFailed()
        print("Waiting for link to come up ...")
        switch.expect_exact("upstream: Link is UP at 10 GBit/s")
        print("Switch is running.")

        # IRQ affinity is set by sv3 itself, when it sees that it is
        # pinned to one CPU.

        try:
            server = create_and_configure(qemu_cmd, True)
            client = create_and_configure(qemu_cmd, False)

            for s in [client, server]:
                set_tso(s, tso)
                # This will only work on the external system, but it is okay.
                set_irq_rate(s, irq_rate)

            run_benchmark(client, server, {'switch': 'sv3', 'poll_us' : poll_us, 'batch_size' : batch_size, 'tso' : tso, 'irq_rate' : irq_rate })
        except:
            print("Unexpected error:", sys.exc_info()[0])
            print(traceback.format_exc())

    finally:
        if switch:
            try:
                switch.close()
            except:
                pass
        if client:
            qemu_quit(client)

        if server:
            qemu_quit(server)
        print("Rebinding device to original driver ...")
        pci.bind(net_if_driver, net_if_pciid)

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

def run_vhost_benchmark(tso, irqr):
    print("\n--- vhost ---")
    cleanup_fn = []
    try:
        def mt(name, tapname, mac):
            tap = create_macvtap(net_if, tapname, mac)
            cleanup_fn.append(lambda: delete_macvtap(tapname))
            return tap

        host = e.spawn("sh")
        set_tso(host, tso, net_if)
	set_irq_rate(host, irqr, net_if)
        host.close()

        client_mac = "1a:54:0b:ca:bc:10"
        client_macvtap = "macvtap0"
        qemu_vhost_args = " -netdev type=tap,id=guest,vhost=on,fd=3 -device virtio-net-pci,netdev=guest,mac=%s  3<>%s"
        qemu_vhost_client_args = qemu_vhost_args % (client_mac, mt(net_if, client_macvtap, client_mac))
        client = create_and_configure("%s %s %s" % (qemu_bin, qemu_generic, qemu_vhost_client_args), False)

        server_mac = "1a:54:0b:ca:bc:20"
        server_macvtap = "macvtap1"
        qemu_vhost_server_args = qemu_vhost_args % (server_mac, mt(net_if, server_macvtap, server_mac))
        server = create_and_configure("%s %s %s" % (qemu_bin, qemu_generic, qemu_vhost_server_args), True)

        for s in [client, server]:
            set_tso(s, tso)
            set_irq_rate(s, irqr)

        try:
            run_benchmark(client, server, {'switch' : 'vhost', 'tso' : tso, 'irq_rate' : irqr})
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
        switch_cpu = core_list[1].thread[-1]
        print("Switch CPU is %s." % switch_cpu)

    print("Let the benchmarking commence!")

    try:
        start_time = time.time()

        tso     = [False, True]
        irqr_v  = [10000, 50000]

        configurations = [ x for x in itertools.product(tso, irqr_v)]
        random.shuffle(configurations)
        
        for conf in configurations:
            tsov, irqr = conf
            run_vhost_benchmark(tsov, irqr)

        tso     = [True, False]
        poll_v  = [0]
        batch_v = [16]


        configurations = [ x for x in itertools.product(tso, poll_v, batch_v, irqr_v)]
        random.shuffle(configurations)

        for n in range(len(configurations)):
            remaining_experiments = len(configurations) - n
            elapsed = time.time() - start_time
            # We count the vhost experiment here as well.
            print("\n=== Remaining time %.2fh. ===\n" % ((remaining_experiments * (elapsed / (n+1))) / (60*60)))

            tso, poll_us, batch_size, irq_rate = configurations[n]
            print("TSO %s, poll %d, batch %d, irqr %u" % (tso, poll_us, batch_size, irq_rate))
            run_externalpci_benchmark([switch_cpu], tso, poll_us, batch_size, irq_rate)

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

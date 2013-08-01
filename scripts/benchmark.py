#!/usr/bin/env python2
# -*- coding: utf-8 -*-
from __future__ import print_function

import datetime
import os
import sys
import math
import glob
import itertools
import time
import pexpect as e
import csv
import argparse
import random

import topology
import cgroup

def thread_list_to_str(tl):
    return ",".join([str(t.id) for t in tl])

net_if    = "eth0"
qemu_bin  = "../contrib/qemu/x86_64-softmmu/qemu-system-x86_64"
linux_bin = "../contrib/buildroot/buildroot/output/images/bzImage"
qemu_generic = "-enable-kvm -m 2048 -nographic -kernel " + linux_bin + " -append loglevel=3\ quiet\ console=ttyS0\ clocksource=tsc"

all_cgroups = ["js-switch", "js-server", "js-client"]

def check_setup():
    try:
        scaling_governor =  open("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor").read()
        if scaling_governor != "performance\n":
            print("CPU frequency scaling in effect! Scaling governor is set to: " + scaling_governor, end="")
            exit(1)
        else:
            print("CPU frequency scaling is set to performance. Good.")
    except IOError as e:
        print("Could not check your frequency scaling settings!")
        print(str(e))
    
    if not os.path.exists(qemu_bin):
        print("Qemu not found at: " + qemu_bin)
        exit(1)

    try:
        with open('/dev/vhost-net'): pass
    except IOError as e:
        print(str(e))
        print("Is the vhost_net module loaded?")
        print("If it is a permission problem, add yourself to the kvm group and add this udev rule:")
        print(' SUBSYSTEM=="misc", KERNEL=="vhost-net", GROUP="kvm", MODE="0660"')
        exit(1)

class StdDev:
    def update(self, value):
        self.n      += 1
        self.sum    += value
        self.sum_sq += value*value

    def mean(self):
        if self.n == 0:
            return None
        else:
            return self.sum / self.n

    def stddev(self):
        if self.n < 2:
            return None
        else:
            n  = self.n
            s  = self.sum
            sq = self.sum_sq
            return math.sqrt((sq - s*s/n)/(n-1))

    def format(self):
        res = ""
        mean   = self.mean()
        stddev = self.stddev()
        if mean:
            res += " mean=%.2f" % mean
        if stddev:
            er = 1.96 * stddev / math.sqrt(self.n)
            res += "±%.2f  95%% in [%.2f,%.2f]" % (stddev, mean - er, mean + er)
        return res

    def __init__(self):
        self.n      = 0
        self.sum    = 0
        self.sum_sq = 0

def qemu_quit(p):
    p.sendcontrol('a')
    p.send('x')
    p.expect(e.EOF)
    p.close(force = True)

def create_and_configure(qemu_cmd, is_server, cpus):
    print("Starting %s VM..." % ("server" if is_server else "client"))
    qemu_cmd = cgroup.cgexec_cmd("js-%s" % ("server" if is_server else "client")) + " taskset -c %s sh -c '%s'" % (thread_list_to_str(cpus), qemu_cmd)
    #print(qemu_cmd)
    p = e.spawn(qemu_cmd, timeout=20)
    p.setecho(False)
    p.expect_exact("buildroot login: ")
    p.sendline("root")
    p.expect("\r\n")
    p.sendline("ifconfig eth0 up %s && echo return $?" % ("10.0.0.1" if is_server else "10.0.0.2"))
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
    client.sendline("netperf -t %s -H 10.0.0.1" % test)
    client.expect_exact("Socket Size   Request  Resp.   Elapsed  Trans.")
    client.expect_exact("Send   Recv   Size     Size    Time     Rate")
    client.expect_exact("bytes  Bytes  bytes    bytes   secs.    per sec")
    client.expect(r"\d+\s+\d+\s+\d+\s+\d+\s+\d+\.\d+\s+(\d+\.\d+)\s*\r\n")
    return float(client.match.group(1))

def netperf_stream_like(client, test):
    client.sendline("netperf -t %s -H 10.0.0.1" % test)
    client.expect_exact("Recv   Send    Send")
    client.expect_exact("Socket Socket  Message  Elapsed")
    client.expect_exact("Size   Size    Size     Time     Throughput")
    client.expect_exact("bytes  bytes   bytes    secs.    10^6bits/sec")
    client.expect(r"\d+\s+\d+\s+\d+\s+\d+\.\d+\s+(\d+\.\d+)\s*\r\n")
    return float(client.match.group(1))

def clear_line():
    print("[2K\r", end="")

def account(user_list, sys_list, start, end, elapsed):
    (cpu_user, cpu_sys) = cgroup.CpuUsage.delta(start, end, elapsed)
    user_list.append(cpu_user)
    sys_list.append(cpu_sys)

import pdb    

def repeat_benchmark(results, name, repeat, thunk):
    bench_results   = [name + "_res"]
    switch_user = [name + "_switch_user"]
    switch_sys  = [name + "_switch_sys"]
    client_user = [name + "_client_user"]
    client_sys  = [name + "_client_sys"]
    server_user = [name + "_server_user"]
    server_sys  = [name + "_server_sys"]

    stddev_gen = StdDev()
    for i in range(repeat):
        clear_line()
        print("Running %s (%d/%d). %s" % (name, i+1, repeat, stddev_gen.format()), end="")
        sys.stdout.flush()

        start_time  = time.time()
        start_usage = dict([ (cg, cgroup.cgget_usage(cg)) for cg in all_cgroups ])
        v = thunk()
        elapsed_time = time.time() - start_time
        end_usage   = dict([ (cg, cgroup.cgget_usage(cg)) for cg in all_cgroups ])

        account(switch_user, switch_sys, start_usage["js-switch"], end_usage["js-switch"], elapsed_time)
        account(client_user, client_sys, start_usage["js-client"], end_usage["js-client"], elapsed_time)
        account(server_user, server_sys, start_usage["js-server"], end_usage["js-server"], elapsed_time)

        stddev_gen.update(v)
        bench_results.append(v)
    clear_line()
    print("%s: %s" % (name, stddev_gen.format()))

    for r in [bench_results, switch_user, switch_sys, client_user, client_sys, server_user, server_sys]:
        results.append(r)

def set_tso(session, enable):
    on_off = "on" if enable else "off"
    ethtool_cmd = "ethtool -K eth0 tx-tcp-segmentation %s tx-tcp-ecn-segmentation %s tx-tcp6-segmentation %s"
    session.sendline(ethtool_cmd % ((on_off,)*3))
    # Check whether we configured correctly
    session.sendline("ethtool -k eth0")
    session.expect_exact("tcp-segmentation-offload: %s" % on_off)


repeat = 10
def run_benchmark(prefix, client, server):
    results = []
    repeat_benchmark(results, prefix + "_udp_rr", repeat, lambda: 1000000/netperf_rr_like(client, "UDP_RR"))
    #repeat_benchmark(results, prefix + "_tcp_rr", repeat, lambda: 1000000/netperf_rr_like(client, "TCP_RR"))
    #repeat_benchmark(results, prefix + "_tcp_cc", repeat, lambda: 1000000/netperf_rr_like(client, "TCP_CC"))
    repeat_benchmark(results, prefix + "_tcp_crr", repeat, lambda: 1000000/netperf_rr_like(client, "TCP_CRR"))

    set_tso(client, True)
    set_tso(server, True)
    repeat_benchmark(results, prefix + "_tcp_stream_tso", repeat, lambda: netperf_stream_like(client, "TCP_STREAM"))

    set_tso(client, False)
    set_tso(server, False)
    repeat_benchmark(results, prefix + "_tcp_stream", repeat, lambda: netperf_stream_like(client, "TCP_STREAM"))

    return results

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

    results = []
    try:
        server = create_and_configure(qemu_cmd, True, server_cpus)
        client = create_and_configure(qemu_cmd, False, client_cpus)
        results = run_benchmark("epci", client, server)
    except:
	print("Unexpected error:", sys.exc_info()[0])

    if client:
        qemu_quit(client)

    if server:
        qemu_quit(server)

    switch.close(force = True)

    return results

class CommandFailed(BaseException):
    pass

def create_macvtap(netif, macvtap, mac):
    """Create a macvtap devices connected to netif. Returns the name of the 
    corresponding tap device."""
    output, ex = e.run("sudo ip link add link %s name %s address %s type macvtap mode bridge" % (netif, macvtap, mac),
                       withexitstatus = True)
    if ex != 0:
        print(("Creating '%s' failed:" % macvtap) + output, end="")
        raise CommandFailed()
    e.run("sudo ip link set dev %s up" % macvtap)
    tap_files = glob.glob("/sys/class/net/%s/tap*" % macvtap)
    assert len(tap_files) == 1
    devnode = "/dev/" + os.path.basename(tap_files[0])
    print(e.run("ls -l %s" % devnode), end="")
    print("If it looks like you don't have permissions to access this, use this udev rule:")
    print('SUBSYSTEM=="macvtap", GROUP="kvm", MODE="0660"')
    time.sleep(1)
    return devnode

def delete_macvtap(macvtap):
    e.run("sudo ip link delete %s type macvtap" % macvtap)

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
            results = run_benchmark("vhost", client, server)
        finally:
            qemu_quit(client)
            qemu_quit(server)

        return results

    finally:
        cleanup_fn.reverse()
        for f in cleanup_fn:
            f()

def write_csv(name, results):
    with open(name, 'wb') as csvfile:
        writer = csv.writer(csvfile)
        for r in results:
            writer.writerow(r)


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

    dstr = datetime.datetime.today().strftime("%Y%m%d-%H%m%S")

    write_csv("results-vhost-%s.csv" % dstr, run_vhost_benchmark([switch_cpus[0]] + server_cpus, [switch_cpus[1]] + client_cpus))

    poll_v  = [0,4,8,12,16,18,24,32,40]
    batch_v = [1,4,8,16,24,32]

    experiments = [ x for x in itertools.product(poll_v, batch_v)]
    random.shuffle(experiments)

    for n in range(len(experiments)):
        poll_us, batch_size = experiments[n]
        print("\nExperiment %4u/%4u: poll=%u batch=%u" % (n, len(experiments), poll_us, batch_size))
        write_csv("results-sv3-%s-poll%u-batch%u.csv" % (dstr, poll_us, batch_size),
                  run_externalpci_benchmark(switch_cpus[:1], server_cpus, client_cpus,
                                            poll_us, batch_size))


if __name__ == "__main__":
    main(sys.argv)

# EOF

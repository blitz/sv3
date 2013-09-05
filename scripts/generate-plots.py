#!/usr/bin/env python2

import os
import itertools
import json
import gzip
import sys
import numpy

import cpuutil

USER_HZ = os.sysconf(os.sysconf_names['SC_CLK_TCK'])

def match_dict(match, full):
    for k,v in match.items():
        if not k in full or full[k] != v:
            return False
    return True

def parameter_values(experiments, parameter):
    v = set()
    for e in experiments:
        v.add(e['parameters'][parameter])
    return list(v)

def select(data, name, parameters):
    """Returns a list of experiments that match fields"""
    res = []
    for exp in data:
        if exp['name'] == name and match_dict(parameters, exp['parameters']):
            res.append(exp)
    return res

def analyze(values):
    return numpy.mean(values), numpy.std(values)

def run_wall_time(run):
    return run['time_end'] - run['time_start']

def proc_cpu_usage(run):
    stat = run['procstat']
    cpus = 0
    for k in stat.keys():
        if len(k) > len("cpu") and k[0:3] == "cpu":
            cpus = cpus + 1
    
    return cpus - stat['cpu'][cpuutil.STAT_IDLE]/(USER_HZ*run_wall_time(run))

def bw_cpu_plot(data):
    data_vhost = select(data, "nuttcp", {'switch': 'vhost', 'active': True, 'tso' : True})
    connections = parameter_values(data_vhost, 'nuttcp_connections')
    target_mbit = parameter_values(data_vhost, 'target_mbit')
    for c, mbit in itertools.product(connections, target_mbit):
        exp_list = select(data_vhost, "nuttcp", {'nuttcp_connections': c, 'target_mbit': mbit})
        if len(exp_list) == 0:
            continue

        if len(exp_list) != 1:
            print(exp_list[0]['parameters'])
            print(exp_list[1]['parameters'])
            exit(1)
        runs = exp_list[0]['runs']
        if len(runs) < 2:
            continue

        m, std = analyze([proc_cpu_usage(r) for r in runs])
        print("%d %d %.3f %.4f" % (c, mbit, m, std))

def generate_plots(json_results_file):
    with gzip.open(json_results_file) as f:
        data = json.loads(f.read())
        
        bw_cpu_plot(data)

def main(args):
    for arg in args[1:]:
        generate_plots(arg)

if __name__ == "__main__":
    main(sys.argv)


# EOF

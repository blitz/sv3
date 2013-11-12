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
    if (len(values) > 1):
        return numpy.mean(values), numpy.std(values)
    else:
        # XXX
        return numpy.mean(values), 0

def run_wall_time(run):
    return run['time_end'] - run['time_start']

def proc_cpu_usage(run):
    stat = run['procstat']
    cpus = 0
    for k in stat.keys():
        if len(k) > len("cpu") and k[0:3] == "cpu":
            cpus = cpus + 1
    
    return cpus - stat['cpu'][cpuutil.STAT_IDLE]/(USER_HZ*run_wall_time(run))

def latency_plot(data):
    data = select(data, "udp_rr", {'irq_rate' : 50000, 'tso' : False})
    switches    = parameter_values(data, 'switch')
    loadgenerators = parameter_values(data, 'loadgenerator')
    with open("latency.csv", "w") as f:
        for (switch, loadgen) in itertools.product(switches, loadgenerators):
            exp_list = select(data, "udp_rr", {'switch' : switch,
                                               'loadgenerator' : loadgen })

            if switch == 'sv3':
                exp_list = select(exp_list, "nuttcp", {'poll_us' : 0})

            if (len(exp_list) == 0):
                continue
            if (len(exp_list) > 1):
                print(exp_list[0]['parameters'])
                print(exp_list[1]['parameters'])
                assert(False)

            rtt, rtt_stddev = analyze([r['value'] for r in exp_list[0]['runs']])
            f.write("%s-%s %.4f %.4f\n" % (switch, loadgen, rtt, rtt_stddev))

def max_bw_plot(data):
    for tso in [True, False]:
        with open("bw_max_%s" % ("tso" if tso else "notso"), "w") as f:
            data = select(data, "nuttcp", {'active': True, 'target_mbit': 0, 'tso': tso})
            connections = parameter_values(data, 'nuttcp_connections')
            switches    = parameter_values(data, 'switch')
            loadgenerators = parameter_values(data, 'loadgenerator')

            f.write("# configuration connections mbit stddev cpu-usage stddev switch-usage stddev\n")
            for (connections, switch, loadgenerator) in itertools.product(connections, switches, loadgenerators):
                exp_list = select(data, "nuttcp", {'switch' : switch, 'nuttcp_connections': connections, 'loadgenerator': loadgenerator})
                if switch == 'sv3':
                    exp_list = select(exp_list, "nuttcp", {'poll_us' : 0})
                if (len(exp_list) == 0):
                    continue
                if (len(exp_list) > 1):
                    print(exp_list[0]['parameters'])
                    print(exp_list[1]['parameters'])
                    exit(1)
                    
                runs = exp_list[0]['runs']
                mbit, mstd   = analyze([r['value']['rate_Mbps'] for r in runs])
                usage, ustd  = analyze([proc_cpu_usage(r) for r in runs])
                susage, sstd = analyze([r['js-switch-usage'] for r in runs])

                f.write("%s-%s %d %.04f %.04f %.04f %.04f %.04f %.04f \n" % (switch, loadgenerator, connections, 
                                                                             mbit, mstd, usage, ustd, susage, sstd))

def bw_cpu_plot(data):
    data = select(data, "nuttcp", {'active': True})
    connections = parameter_values(data, 'nuttcp_connections')
    switches    = parameter_values(data, 'switch')
    target_mbit = parameter_values(data, 'target_mbit')
    loadgenerators = parameter_values(data, 'loadgenerator')
    tso = parameter_values(data, 'tso')
    for (tso, switch, loadgen) in itertools.product(tso, switches, loadgenerators):
        with open("bw_cpu_%s_%s_%s.csv" % ("tso" if tso else "notso", switch, loadgen), "w") as f:
            for c, mbit in itertools.product(connections, target_mbit):
                exp_list = select(data, "nuttcp", {'switch' : switch, 'nuttcp_connections': c, 'target_mbit': mbit, 'loadgenerator': loadgen, 'tso' : tso})
                if switch == 'sv3':
                    exp_list = select(exp_list, "nuttcp", {'poll_us' : 0})

                if len(exp_list) == 0:
                    continue
                
                if len(exp_list) != 1:
                    print(exp_list[0]['parameters'])
                    print(exp_list[1]['parameters'])
                    exit(1)
                runs = exp_list[0]['runs']
                if len(runs) < 2:
                    continue

                invalid = False
                if mbit != 0:
                    for r in runs:
                        relative_error = abs(mbit - r['value']['rate_Mbps'])/mbit
                        if (relative_error > 0.02):
                            invalid = True

                if invalid:
                    print("Skipped %s" % exp_list[0]['parameters'])
                    continue
                m, std   = analyze([proc_cpu_usage(r) for r in runs])
                cu, cstd = analyze([r['js-switch-usage'] for r in runs])
                f.write("%d %d %.4f %.4f %.4f %.4f\n" % (c, mbit, m, std, cu, cstd))

def generate_plots(json_results_file):
    with gzip.open(json_results_file) as f:
        data = json.loads(f.read())
        
        bw_cpu_plot(data)
        latency_plot(data)
        max_bw_plot(data)

def main(args):
    for arg in args[1:]:
        generate_plots(arg)

if __name__ == "__main__":
    main(sys.argv)


# EOF

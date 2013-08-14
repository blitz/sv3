#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import csv
import re
import sys
import numpy

def get_prop(prop, name):
    r = re.escape(prop) + r"([\d]+)"
    m = re.search(r, name)
    return int(m.group(1))

def main(benchmarks, files):
    # We assume file names to be like: results-sv3-DATE-pollN-batchM
    # Collect poll and batch values first.
    
    print("#poll-us batch %s" % benchmarks)

    for f in files:
        poll  = get_prop("poll",   f)
        batch = get_prop("batch",  f)

        print("%s %s " % (poll, batch), end="")

        for benchmark in benchmarks:
            with open(f, 'r') as csvfile:
                reader = csv.reader(csvfile)
                for row in reader:
                    if row[0] == benchmark:
                        r = [float(x) for x in row[1:]]
                        m  = numpy.mean(r)
                        sd = numpy.std(r)
                        print("%s %s %s " % (m, m - sd, m + sd), end="")
                        break
                else:
                    print("0 0 0 ", end="")
        print("")

if __name__ == "__main__":
    sep_idx = sys.argv.index("--")
    main(sys.argv[1:sep_idx], sys.argv[(sep_idx+1):])

# EOF

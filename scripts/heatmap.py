#!/usr/bin/env python2
# -*- coding: utf-8 -*-
from __future__ import print_function

import csv
import re
import sys
import numpy

def get_prop(prop, name):
    r = re.escape(prop) + r"([\d]+)"
    m = re.search(r, name)
    return int(m.group(1))

def main(benchmark, files):
    # We assume file names to be like: results-sv3-DATE-pollN-batchM
    # Collect poll and batch values first.
    
    print("#poll-us batch %s" % benchmark)

    for f in files:
        poll  = get_prop("poll",   f)
        batch = get_prop("batch",  f)

        with open(f, 'rb') as csvfile:
            reader = csv.reader(csvfile)
            for row in reader:
                if row[0] == benchmark:
                    r = [float(x) for x in row[1:]]
                    m  = numpy.mean(r)
                    sd = numpy.std(r)
                    print("%s %s %s %s %s" % (poll, batch, m, m - sd, m + sd))

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2:])

# EOF

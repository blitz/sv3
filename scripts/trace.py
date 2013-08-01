#!/usr/bin/env python3

import struct
import sys

trace_event_struct = 'QBBHI'

def print_event(time_rel, time, event, port, length, dummy):
    if (time != 0):
        print("%016x +%08x: %12s port=%03s length=%04x" %
              (time, time_rel,
               ["BLOCK", "WAKEUP", "PACKET_RX", "PACKET_TX", "WENT_IDLE",
                "IRQ", "QUIESCENT"][event],
               port, length))

def main(tracefile):
    events = []
    with open(tracefile, 'rb') as tf:
        while True:
            bytes_read = tf.read(struct.calcsize(trace_event_struct))
            if not bytes_read: break
            event = struct.unpack(trace_event_struct, bytes_read)
            if event[0] != 0:
                events.append(event)
    last_time = None
    for e in sorted(events, key = lambda t: t[0]):
        print_event(e[0] - last_time if last_time else 0,
                    *e)
        last_time = e[0]

if __name__ == "__main__":
    main(*sys.argv[1:])

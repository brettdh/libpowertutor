#!/usr/bin/env python

import time, sys, math
from subprocess import Popen, PIPE

def get_lines(filename):
    #return open(filename).readlines()
    proc = Popen(['adb', 'shell', 'cat', filename], stdout=PIPE)
    lines = proc.communicate()[0].split('\r\n')[:-1]
    return lines

def get_ip_hex(iface):
    ints = [int(octet) for octet in iface.split(".")]
    ints.reverse()
    return "%02X%02X%02X%02X" % tuple(ints)

def get_counts(ifaces):
    counts = {}
    for iface in ifaces:
        counts[iface] = {
            "packets" : {
                "down" : -1, "up" : -1
            },
            "bytes" : {
                "down" : -1, "up" : -1
            }
        }
    
    filename = '/proc/net/dev'
    lines = [line.strip() for line in get_lines(filename)]
    lines = lines[2:] # strip the header lines
    for line in lines:
        #  Receive                      ...        Transmit
        #  0      1          2                       9        10
        # iface  bytes    packets (...6 fields...) bytes    packets ...
        blocks = line.split()
        local_iface_name = blocks[0][:-1] # trim trailing ':'
        if local_iface_name not in counts:
            continue
        
        block = blocks[4]
        bytes_down, bytes_up = int(blocks[1]), int(blocks[9])
        packets_down, packets_up = int(blocks[2]), int(blocks[10])
        counts[local_iface_name]["bytes"]["down"] += bytes_down
        counts[local_iface_name]["bytes"]["up"] += bytes_up
        counts[local_iface_name]["packets"]["down"] += packets_down
        counts[local_iface_name]["packets"]["up"] += packets_up
    
    return counts

def get_ifaces():
    ifaces = []
    proc = Popen(['adb', 'shell', 'netcfg'], stdout=PIPE)
    lines = proc.communicate()[0].split('\r\n')[:-1]

    for line in lines:
        blocks = line.split()
        ifname = blocks[0]
        up = (blocks[1] == "UP")
        if up and ifname != "lo":
            ifaces.append(ifname)
    return ifaces

def main():
    sample_rate = 1.0
    last_counts = {}
    last_diffs = {}
    sys.stdout.write("%10s | " % "")
    for iface in get_ifaces():
        last_counts[iface] = {
            "packets" : {
                "down" : -1, "up" : -1
            },
            "bytes" : {
                "down" : -1, "up" : -1
            }
        }
        
        sys.stdout.write("%-25s" % iface)
    print ""
    
    init = False
    last_update = time.time()
    while True:
        ifaces = get_ifaces()
        counts = get_counts(ifaces)

        if not init:
            init = True
            last_counts = dict(counts)
            last_update = time.time()
            time.sleep(sample_rate)
            continue

        now = time.time()
        diffs = {}
        for i in counts:
            diffs[i] = {}
            for j in counts[i]:
                diffs[i][j] = {}
                for k in counts[i][j]:
                    diffs[i][j][k] = counts[i][j][k] - last_counts[i][j][k]
                    diffs[i][j][k] = math.ceil(diffs[i][j][k] /
                                               (now - last_update))
        
        last_counts = dict(counts)
        last_update = now
        
        if last_diffs == diffs:
            time.sleep(sample_rate)
            continue
        
        last_diffs = diffs

        msgs = {}
        for iface in diffs:
            for category in diffs[iface]:
                if category not in msgs:
                    msgs[category] = ("%-10s | " % category)

                for direction in diffs[iface][category]:
                    val = diffs[iface][category][direction]
                    field = "%s: %d" % (direction, val)
                    msgs[category] += ("%-13s " % field)

        for category in msgs:
            print msgs[category]
        print ""

        time.sleep(sample_rate)

if __name__ == '__main__':
    main()

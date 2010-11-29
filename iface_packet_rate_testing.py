#!/usr/bin/env python

import time, sys
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

def get_rates(ifaces):
    rates = {}
    for iface in ifaces:
        iface_hex = get_ip_hex(iface)
        rates[iface_hex] = {
            "bytes_down" : -1, "bytes_up" : -1,
            "packets_down" : -1, "packets_up" : -1
        }
    
    filename = '/proc/net/dev'
    lines = [line.strip() for line in get_lines(filename)]
    lines = lines[2:] # strip the header lines
    for line in lines:
        #  0      1          2                       9        10
        # iface  bytes    packets (...6 fields...) bytes    packets ...
        blocks = line.split()
        local_iface_hex = blocks[0].split(':')[0]
        if local_iface_hex not in rates:
            continue
        
        block = blocks[4]
        bytes_down, bytes_up = [int(field) for field in block.split(':')]
        rates[local_iface_hex]["down_bytes"] += down_q
        rates[local_iface_hex]["up_bytes"] += up_q
    
    return rates

def get_ifaces():
    ifaces = []
    proc = Popen(['adb', 'shell', 'netcfg'], stdout=PIPE)
    lines = proc.communicate()[0].split('\r\n')[:-1]

    for line in lines:
        blocks = line.split()
        ifname = blocks[0]
        up = (blocks[1] == "UP")
        ifip = blocks[2]
        if up and ifname != "lo":
            ifaces.append(ifip)
    return ifaces

def main():
    sample_rate = 1.0
    last_rates = {}
    for iface in get_ifaces():
        last_rates[get_ip_hex(iface)] = {"down_bytes":-1, "up_bytes":-1}
        
        sys.stdout.write("%-25s" % iface)
    print ""
    
    while True:
        ifaces = get_ifaces()
        rates = get_rates(ifaces)
        if rates != last_rates:
            for key in rates.keys():
                down = rates[key]["down_bytes"]
                up = rates[key]["up_bytes"]
                sys.stdout.write("down: %d   up: %d       " % (down, up))
                last_rates[key]["down_bytes"] = down
                last_rates[key]["up_bytes"] = up
            print ""
                
        time.sleep(sample_rate)

if __name__ == '__main__':
    main()
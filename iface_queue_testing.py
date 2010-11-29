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

def get_queues(ifaces):
    queues = {}
    for iface in ifaces:
        iface_hex = get_ip_hex(iface)
        queues[iface_hex] = {}
        queues[iface_hex]["down_bytes"] = 0
        queues[iface_hex]["up_bytes"] = 0
    
    for suffix in ('tcp', 'udp', 'raw'):
        filename = '/proc/net/' + suffix
        lines = [line.strip() for line in get_lines(filename)]
        lines = lines[1:] # strip the header line
        for line in lines:
            #  0              1           2    3                 4
            # sl  local_address rem_address   st tx_queue:rx_queue ...
            # 0:  0100007F:13AD 00000000:0000 0A 00000000:00000000 ...
            blocks = line.split()
            local_iface_hex = blocks[1].split(':')[0]
            if local_iface_hex not in queues:
                continue
            
            block = blocks[4]
            down_q, up_q = [int(field, 16) for field in block.split(':')]
            queues[local_iface_hex]["down_bytes"] += down_q
            queues[local_iface_hex]["up_bytes"] += up_q
    
    return queues

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
    last_queues = {}
    for iface in get_ifaces():
        last_queues[get_ip_hex(iface)] = {"down_bytes":-1, "up_bytes":-1}
        
        sys.stdout.write("%-25s" % iface)
    print ""
    
    while True:
        ifaces = get_ifaces()
        queues = get_queues(ifaces)
        if queues != last_queues:
            for key in queues.keys():
                down = queues[key]["down_bytes"]
                up = queues[key]["up_bytes"]
                sys.stdout.write("down: %d   up: %d       " % (down, up))
                last_queues[key]["down_bytes"] = down
                last_queues[key]["up_bytes"] = up
            print ""
                
        time.sleep(0.5)

if __name__ == '__main__':
    main()
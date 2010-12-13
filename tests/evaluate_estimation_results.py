#!/usr/bin/env python

import sys, os, shutil
from subprocess import Popen, PIPE

def get_most_recent_of(glob):
    cmd = ["adb", "shell", "ls", glob]
    inpipe = Popen(cmd, stdout=PIPE)
    in_data = inpipe.communicate()[0]
    items = in_data.split()
    if len(items) == 0:
        raise Exception("Couldn't find anything matching %s" % glob)
    
    # return the item with the greatest timestamp
    items.sort()
    print "Choosing among ", items
    return items[-1]
    
def sanity_check(power_trace_lines, libpt_log_lines):
    power_trace_start = 0
    power_trace_end = 0
    for line in power_trace_lines:
        if line[0:4] == "time":
            power_trace_start = int(line.split()[1])
            power_trace_end = power_trace_start
            break
    
    for line in reversed(power_trace_lines):
        if line[0:5] == "begin":
            power_trace_end += 1000 * int(line.split()[1])
            break
    
    print "Trace begins at %d" % power_trace_start
    try:
        if len(libpt_log_lines) == 0:
            raise Exception("Error: no events in libpt_log")
        
        for line in libpt_log_lines:
            sys.stdout.write("  %s" % line)
            fields = line.split()
            if fields[0][0] == "[":
                # timestamps are floating-point seconds
                fields[0] = fields[0][1:-1]
                
            timestamp = int(float(fields[0]) * 1000)
            if timestamp < power_trace_start or timestamp > power_trace_end:
                raise Exception("Error: libpt_log falls outside of power trace")
    except Exception, e:
        print str(e)
        raise
    finally:
        print "Trace ends at %d" % power_trace_end
    
def main():
    tmpdir = '/tmp/powertutor_eval'
    power_trace = tmpdir + "/power_trace.log"
    libpt_log = tmpdir + "/libpt.log"
    if os.access(tmpdir, os.F_OK):
        shutil.rmtree(tmpdir)
    os.mkdir(tmpdir)
    
    dev_power_trace = get_most_recent_of("/sdcard/PowerTrace*.log")
    dev_libpt_log = get_most_recent_of("/sdcard/libpowertutor_testing/*")
    print "Using %s and %s" % (power_trace, libpt_log)
    
    os.system("adb pull %s %s" % (dev_power_trace, power_trace))
    os.system("adb pull %s %s" % (dev_libpt_log, libpt_log))
    
    power_trace_lines = open(power_trace).readlines()
    libpt_log_lines = open(libpt_log).readlines()
    sanity_check(power_trace_lines, libpt_log_lines)
    
    

if __name__ == '__main__':
    main()

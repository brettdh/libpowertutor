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
    
class PowerTrace:
    def __init__(self, power_trace_lines):
        self.__trace = []
        cur_slice = None
        for line in power_trace_lines:
            if line[0:4] == "time":
                self.__trace_start = int(line.split()[1])
                self.__trace_offset = self.__trace_start % 1000
            elif line[0:5] == "begin":
                if cur_slice:
                    self.__trace.append(cur_slice)
                cur_slice = {}
                step = int(line.split()[1])
                if step != len(self.__trace):
                    raise Exception("expected slice %d, got slice %d" % 
                                    (len(self.__trace), step))
            else:
                if cur_slice:
                    fields = line.split()
                    cur_slice[fields[0]] = fields[1:]
                    
        self.__trace_end = self.__trace_start + (len(self.__trace) * 1000)
    
    def sanity_check(self, libpt_log_lines):
        print "Trace begins at %d" % self.__trace_start
        try:
            if len(libpt_log_lines) == 0:
                raise Exception("Error: no events in libpt_log")

            for line in libpt_log_lines:
                sys.stdout.write("  %s" % line)
                fields = line.split()

                # timestamps are floating-point seconds; store as milliseconds
                timestamp = int(float(fields[0]) * 1000)
                if timestamp < self.__trace_start or
                   timestamp > self.__trace_end:
                    raise Exception("Error: libpt_log falls outside " + 
                                    "of power trace")
        except Exception, e:
            print str(e)
            raise
        finally:
            print "Trace ends at %d" % self.__trace_end
            
    def calculate_energy(self, begin, end, type):
        '''Calculate the energy used for this transfer, which
           began at <begin> and ended at <end>.
           begin and end are UNIX timestamps in milliseconds.
           Returns energy in mJ.'''
        if type != PowerTrace.TYPE_WIFI and type != PowerTrace.TYPE_MOBILE:
            raise Exception("Unknown transfer type: %s" % type)
        
        begin_step = (begin - trace_start) / 1000
        end_step = (end - trace_start) / 1000
        # TODO: take a fresh look at this and figure out how to calculate it.
        # TODO:   focus on wifi only to start.

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
    
    trace = PowerTrace(power_trace_lines)
    trace.sanity_check(libpt_log_lines)
    
    action_pending = False
    begin = None
    for line in libpt_log_lines:
        fields = line.split()
        timestamp = int(float(fields[0]) * 1000) # milliseconds
        if action_pending:
            end = timestamp
            duration = end - begin
            
            energy = trace.calculate_energy(begin, end)
            print ""
            
            begin = None
            end = None
            action_pending = False
        else:
            begin = timestamp
            action_pending = True
            

if __name__ == '__main__':
    main()

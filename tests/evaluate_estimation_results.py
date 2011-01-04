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
    # print "Choosing among ", items
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
                if cur_slice != None:
                    self.__trace.append(cur_slice)
                cur_slice = {}
                step = int(line.split()[1])
                cur_slice["begin"] = step
                if step != len(self.__trace):
                    raise Exception("expected slice %d, got slice %d" % 
                                    (len(self.__trace), step))
            else:
                if cur_slice != None:
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
                if (timestamp < self.__trace_start or
                    timestamp > self.__trace_end):
                    raise Exception("Error: libpt_log falls outside " + 
                                    "of power trace")
        except Exception, e:
            print str(e)
            raise
        finally:
            print "Trace ends at %d" % self.__trace_end
            
    TYPE_WIFI = "wifi"
    TYPE_MOBILE = "mobile"
    
    def calculate_energy(self, begin, end, type):
        '''Calculate the energy used for this transfer, which
           began at <begin> and ended at <end>.
           begin and end are UNIX timestamps in milliseconds.
           Returns energy in mJ.'''
        if type != PowerTrace.TYPE_WIFI: #TODO: and type != TYPE_MOBILE:
            raise Exception("Unknown transfer type: %s" % type)
        
        begin_step = (begin - self.__trace_start) / 1000
        end_step = (end - self.__trace_start) / 1000
        
        energy = 0
        if type == PowerTrace.TYPE_WIFI:
            # if begin_step == end_step:
            #     return (end - begin) * self.__trace[begin_step]["Wifi"]
            # else:
            # TODO: tune this a bit more; make sure it's not overestimating.
            for i in xrange(begin_step, end_step + 1):
                energy += int(self.__trace[i]["Wifi"][0])
        else:
            # TODO: this should be done by adding up the total 3G usage
            # TODO:  over the entire trace and then comparing this to
            # TODO:  the total estimated usage
            # TODO: This approach simplifies the consideration of 
            # TODO:  any amortization of ramp and tail periods.
            assert False
            
        return energy
    
    def total_mobile_energy(self, begin, end):
        # begin and end are milliseconds.
        print "Considering trace section [%d, %d]" % (begin, end)
        mobile_energy = 0
        for cur_slice in self.__trace:
            if "3G-state" in cur_slice and cur_slice["3G-state"][0] != "IDLE":
                timestamp = self.__trace_start + (cur_slice["begin"] * 1000)
                if timestamp < begin or timestamp > end:
                    continue
                    
                print ("3G in %s state at step %d" % 
                       (cur_slice["3G-state"][0], timestamp))
                mobile_energy += int(cur_slice["3G"][0])
        return mobile_energy
        
    def get_mobile_active_intervals(self):
        '''Returns the time intervals during which the 3G interface is active.
        
        Result is a list of 2-tuples: (begin, end).  Timestamps are in
        milliseconds, so they can be passed to total_mobile_energy().'''
        activity_start = None
        intervals = []
        idle = False
        for cur_slice in self.__trace:
            if "3G-state" not in cur_slice:
                continue
            
            timestamp = self.__trace_start + (cur_slice["begin"] * 1000)
            if cur_slice["3G-state"][0] == "IDLE":
                if not idle:
                    if activity_start != None:
                        intervals.append((activity_start, timestamp))
                        activity_start = None
                idle = True
            else:
                if idle:
                    assert activity_start == None
                    activity_start = timestamp
                idle = False
        
        if not idle:
            print "Warning: 3G active at end of trace; ignoring last period"
        
        return intervals

class PredictionTest:
    def __init__(self, libpt_log_lines, trace):
        action_pending = False
        begin = None
        action_line = None
        action_fields = None
        mobile_energy = 0
        first_mobile = None
        last_mobile = None
        big_mobile_send = None
        self.__mobile_events = []
        self.__wifi_events = []
        for line in libpt_log_lines:
            fields = line.split()
            timestamp = int(float(fields[0]) * 1000) # milliseconds

            if "Starting 3G power tests" in line:
                first_mobile = timestamp
                continue
            elif "Finished 3G power tests" in line:
                last_mobile = timestamp
                continue

            if action_pending:
                end = timestamp
                duration = end - begin
                action_type = fields[1]
                event = {}
                event["begin"] = begin
                event["duration"] = duration
                event["type"] = action_type
                event["bytes"] = int(action_fields[2])
                event["bandwidth"] = int(action_fields[4])
                event["energy"] = int(action_fields[6])
                if action_type == PowerTrace.TYPE_WIFI:
                    energy = trace.calculate_energy(begin, end, action_type)
                    print ("%s ; PowerTutor says %d mJ" % 
                           (action_line.strip(), energy))
                    self.__wifi_events.append(event)
                else:
                    bytes = int(action_line.split()[2])
                    energy = int(action_line.split()[6])
                    mobile_energy += energy
                    if bytes == 40000:
                        big_mobile_send = begin - 10 # milliseconds
                        big_send_mobile_energy = energy
                    print ("%s ; total estimated %d mJ" % 
                           (action_line.strip(), mobile_energy))
                    self.__mobile_events.append(event)
                    
                begin = None
                end = None
                action_pending = False
                action_line = None
                action_fields = None
            else:
                begin = timestamp
                action_pending = True
                action_line = line
                action_fields = fields

    def get_mobile_events(self, begin, end):
        func = lambda event : event["begin"] >= begin and event["begin"] <= end
        return filter(func, self.__mobile_events)

def main():
    tmpdir = '/tmp/powertutor_eval'
    power_trace = tmpdir + "/power_trace.log"
    libpt_log = tmpdir + "/libpt.log"
    if os.access(tmpdir, os.F_OK):
        shutil.rmtree(tmpdir)
    os.mkdir(tmpdir)
    
    print "Waiting for device..."
    os.system("adb wait-for-device")
    dev_power_trace = get_most_recent_of("/sdcard/PowerTrace*.log")
    dev_libpt_log = get_most_recent_of("/sdcard/libpowertutor_testing/*")
    print "Using %s and %s" % (power_trace, libpt_log)
    
    os.system("adb pull %s %s" % (dev_power_trace, power_trace))
    os.system("adb pull %s %s" % (dev_libpt_log, libpt_log))
    
    power_trace_lines = open(power_trace).readlines()
    libpt_log_lines = open(libpt_log).readlines()
    
    trace = PowerTrace(power_trace_lines)
    trace.sanity_check(libpt_log_lines)
    
    prediction_test = PredictionTest(libpt_log_lines, trace)
    
    # [(a,b), (c,d), ...]
    active_intervals = trace.get_mobile_active_intervals()
    print "Total estimated mobile energy:"
    for interval in active_intervals:
        # XXX: This is failing to capture the first event in each 
        # XXX:  3G activity interval, due to rounding and the fact that
        # XXX:  PowerTutor doesn't detect the radio active until the
        # XXX:  current second of waiting has passed.
        # TODO: FIX IT.
        (begin, end) = interval 
        begin -= 3000 # active interval starts just after event
        events = prediction_test.get_mobile_events(begin, end)
        if len(events) == 0:
            pass #continue
        total_energy = sum([event["energy"] for event in events])
        pt_total_energy = trace.total_mobile_energy(begin, end)
        print ("   [%d,%d] %d events: %d mJ (PowerTutor: %d mJ)" %
               (begin, end, len(events), total_energy, pt_total_energy))
        
    #if first_mobile != None:
        #pt_total_mobile = trace.total_mobile_energy(first_mobile, last_mobile)
        #pt_small_total = trace.total_mobile_energy(first_mobile, 
        #                                           big_mobile_send)
        #pt_big_total = trace.total_mobile_energy(big_mobile_send, last_mobile)
        #print ("Total estimated mobile energy: %d mJ (PowerTutor: %d mJ)" %
        #       (mobile_energy, pt_total_mobile))
        #print ("Total small est mobile energy: %d mJ (PowerTutor: %d mJ)" %
        #       (mobile_energy - big_send_mobile_energy, pt_small_total))
        #print ("Total big est mobile energy: %d mJ (PowerTutor: %d mJ)" %
        #       (big_send_mobile_energy, pt_big_total))

if __name__ == '__main__':
    main()

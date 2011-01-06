#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <functional>

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <assert.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <pthread.h>
#include "pthread_util.h"

#include "libpowertutor.h"
#include "timeops.h"
#include "utils.h"

#define LOG_TAG "libpowertutor"
#include <cutils/log.h>

using std::ifstream; using std::hex; using std::string;
using std::istringstream;
using std::min; using std::max;

// PowerTutor [1] power model for the HTC Dream.
// [1] Zhang et al. "Accurate Online Power Estimation and Automatic Battery
// Behavior Based Power Model Generation for Smartphones," CODES+ISSS '10.

static const int TCP_HDR_SIZE = 32;
static const int IP_HDR_SIZE = 20;

// all power vals in mW
static const int WIFI_TRANSMIT_POWER = 1000;
static const int WIFI_HIGH_POWER_BASE = 710;
static const int WIFI_LOW_POWER = 20;

enum MobileState {
    MOBILE_POWER_STATE_IDLE=0,
    MOBILE_POWER_STATE_FACH,
    MOBILE_POWER_STATE_DCH
};

static const char *mobile_state_str[3] = {
    "IDLE", "FACH", "DCH"
};

// XXX: These are for 3G only; careful when testing!
static const int MOBILE_IDLE_POWER = 10;
static const int MOBILE_FACH_POWER = 401;
static const int MOBILE_DCH_POWER = 570;

static const int power_coeffs[MOBILE_POWER_STATE_DCH + 1] = {
    MOBILE_IDLE_POWER,
    MOBILE_FACH_POWER,
    MOBILE_DCH_POWER
};

static inline int
power_coeff(MobileState state)
{
    return power_coeffs[state];
}

// byte queue thresholds for transition to DCH state
static const int MOBILE_DCH_THRESHOLD_DOWN = 119;
static const int MOBILE_DCH_THRESHOLD_UP = 151;

static inline int
get_dch_threshold(bool downlink)
{
    return downlink ? MOBILE_DCH_THRESHOLD_DOWN : MOBILE_DCH_THRESHOLD_UP;
}

// in seconds
static const double MOBILE_FACH_INACTIVITY_TIMER = 6.0;
static const double MOBILE_DCH_INACTIVITY_TIMER = 4.0;

/* Information needed to make calculations (based on power state inference):
 * 1) Uplink/downlink queue size
 *    - Look in /proc/net/{tcp,udp,raw} and add up tx_queue and rx_queue
 * 2) Packet rate and data rate (bytes/sec)
 *    - Look in /proc/net/dev ; sample over time to get rates
 */
/* Problem: the server doesn't have this information, and furthermore,
 *     it changes so rapidly that it probably doesn't make sense for the 
 *     client to spend energy sending the info to the server.
 *     So, we should just make do without it.
 *   The power estimates will be fuzzy as a result,
 *     but we'll just have to see if it's close enough to make
 *     good decisions between networks.
 *
 * Solution: jklol, calculate this information on the mobile device
 *     and send it to the server as often as is feasible/necessary.
 *     Possible strategies:
 *     1) Passive - only piggyback on ACKs or other IntNW messages.
 *     2) Active - send separately every so often
 *        a) When conditions change significantly
 *        b) After a timeout expires
 *     ...probably more a) than b).
 *
 *     The server will sometimes operate on stale information,
 *       but there's no way around it; only the mobile device
 *       can make those calculations.
 */

enum dir {DOWN=0, UP};
pthread_mutex_t mobile_state_lock = PTHREAD_MUTEX_INITIALIZER;
static MobileState mobile_state = MOBILE_POWER_STATE_IDLE;
static struct timeval last_mobile_activity = {0, 0};
static int mobile_last_bytes[2] = {-1, -1};
static int mobile_last_delta_bytes[2] = {0, 0};

static inline MobileState
get_mobile_state()
{
    PthreadScopedLock lock(&mobile_state_lock);
    return mobile_state;
}

void
get_mobile_queue_len(int *down_queue, int *up_queue)
{
    size_t up_bytes = 0, down_bytes = 0;
#if 0
    const char *iface = "rmnet0";
    struct in_addr mobile_addr;
    if (get_ip_addr(iface, &mobile_addr) != 0) {
        LOGE("Failed to get ip addr of %s\n", iface);
        return -1;
    }
    
    char filename[50];
    strcpy(filename, "/proc/net/");
    
    char *suffix = filename + strlen(filename);
    
    const char *suffixes[] = {"tcp", "udp", "raw"};
    for (int i = 0; i < 3; ++i) {
        strcpy(suffix, suffixes[i]);
        ifstream infile(filename);
        string junk, line;
        getline(infile, junk); // skip the header line
        while (getline(infile, line)) {
            //  0              1           2    3                 4
            // sl  local_address rem_address   st tx_queue:rx_queue ...
            // 0:  0100007F:13AD 00000000:0000 0A 00000000:00000000 ...
            in_addr_t addr;
            istringstream in(line);
            in >> junk >> hex >> addr;
            if (!in) {
                LOGE("Failed to parse ip addr\n");
                return -1;
            }
            if (addr != mobile_addr.s_addr) {
                continue;
            }
            int tx_bytes = 0, rx_bytes = 0;
            in >> junk >> junk >> junk;
            in >> hex >> tx_bytes;
            in.ignore(1);
            in >> hex >> rx_bytes;
            if (!in) {
                LOGE("Failed to parse queue size\n");
                return -1;
            }
            //getline(in, junk);
            
            up_bytes += tx_bytes;
            down_bytes += rx_bytes;
        }
        infile.close();
    }
    
    return 0;
#endif

    //  Two ways to do this:
    //  1) Return the delta_bytes from the last sample.
    //   (+) Simple
    //   (-) The queue might have filled more since then
    //  2) Estimate the delta_bytes from the last second.
    //   (+) Might grab a more up-to-date estimate of the queues
    //   (-) Trickier to implement
    // XXX: doing (1) for now.  might be naive.
    // TODO: could put the two together.  (PICK UP HERE)
    PthreadScopedLock lock(&mobile_state_lock);
    down_bytes = mobile_last_delta_bytes[DOWN];
    up_bytes = mobile_last_delta_bytes[UP];
    
    if (down_queue) {
        *down_queue = down_bytes;
    }
    if (up_queue) {
        *up_queue = up_bytes;
    }
}

static inline double
time_since_last_mobile_activity(bool already_locked=false)
{
    struct timeval dur, now;
    TIME(now);
    {
        PthreadScopedLock lock;
        if (!already_locked) { 
            lock.acquire(&mobile_state_lock);
        }
        TIMEDIFF(last_mobile_activity, now, dur);
    }
    return dur.tv_sec + (((double)dur.tv_usec) / 1000000.0);
}

static bool
power_model_is_remote()
{
    // XXX: HACKTASTIC.
    // XXX:   The point of this is to decide whether I'm estimating
    // XXX:   power usage based on local or remote information.
    // XXX:   In the present world, I don't have any mobile-to-mobile
    // XXX:   communication, so it's sufficient to ask whether I'm
    // XXX:   on an Android device.
    // XXX:   Later, I'll need to differentiate between minimizing
    // XXX:   local vs. remote power consumption, but this is okay for now.
#ifdef ANDROID
    return false;
#else
    return true;
#endif
}

int update_mobile_state();

static int 
estimate_mobile_energy_cost(int datalen, size_t bandwidth)
{
    // XXX: this doesn't account for the impact that the large
    // XXX:  RTT of the 3G connection will have on the time it takes
    // XXX:  to stop sending all the TCP ACKs.
    // XXX: a higher-level problem is that in order to make this guess
    // XXX:  accurate, I have to accurately guess what TCP is going to do
    // XXX:  with the data.
    // TODO: make it better.
    
    // by updating here, we get the most up-to-date state for this estimate.
    //int rc = update_mobile_state();
    //if (rc < 0) {
    //    LOGE("Warning: failed to update mobile state\n");
    //}
    
     // add in TCP/IP headers
     // XXX: this ignores the possibility of multiple sends
     // XXX:  being coalesced and sent with only one TCP packet.
    datalen += (TCP_HDR_SIZE + IP_HDR_SIZE);
    
    bool downlink = power_model_is_remote();
    
    MobileState old_state = get_mobile_state();
    MobileState new_state = MOBILE_POWER_STATE_FACH;
    
    int queue_len = 0, ack_dir_queue_len = 0;
    get_mobile_queue_len(&queue_len, &ack_dir_queue_len);
    
    int threshold = get_dch_threshold(downlink);
    LOGD("DCH threshold %d -- predicted queue length %d\n",
         threshold, queue_len + datalen);
    if (old_state == MOBILE_POWER_STATE_DCH ||
        (queue_len + datalen) >= threshold) {
        new_state = MOBILE_POWER_STATE_DCH;
    }
    
    int ack_dir_threshold = get_dch_threshold(!downlink);
    // XXX: WRONG.  don't assume an arbitrary number of bytes 
    // XXX:  only generates one TCP ack.
    // TODO: fix.
    int ack_dir_queue_pred = ack_dir_queue_len + TCP_HDR_SIZE + IP_HDR_SIZE;
    LOGD("DCH ack-dir threshold %d -- predicted queue length %d\n",
         ack_dir_threshold, ack_dir_queue_len);
    if (ack_dir_queue_pred >= ack_dir_threshold) {
        new_state = MOBILE_POWER_STATE_DCH;
    }
    
    // XXX: confirmed: the TCP ACK appears to arrive around 800ms after
    // XXX:  the data left the device.  That seems like way too long, though.
    // TODO: understand and account for this delay.  It extends the
    // TODO:  inactivity timer, and that amount contributes to the 
    // TODO:  power cost of the send.
    // Preliminary explanation of this: the RTT for the 3G connection
    //   is just wildly variable.  I've worked around the Nagle/delayed acks
    //   issue, though, to tighten up the test, which appears to have helped.
    
    double duration = ((double)datalen) / bandwidth;
    int fach_energy = 0, dch_energy = 0;
    if (old_state == MOBILE_POWER_STATE_IDLE) {
        if (new_state == MOBILE_POWER_STATE_DCH) {
            // FACH inactivity timeout only; data transferred in DCH
            fach_energy = MOBILE_FACH_POWER * MOBILE_FACH_INACTIVITY_TIMER;
            
            duration += MOBILE_DCH_INACTIVITY_TIMER;
            dch_energy = MOBILE_DCH_POWER * duration;
            LOGD("Now in DCH: dch_energy %d fach_energy %d\n",
                 dch_energy, fach_energy);
        } else {
            // cost of transferring data in FACH state + FACH timeout
            duration += MOBILE_FACH_INACTIVITY_TIMER;
            fach_energy = MOBILE_FACH_POWER * duration;
            LOGD("Now in FACH: fach_energy %d\n", fach_energy);
        }
    } else if (old_state == MOBILE_POWER_STATE_FACH) {
        if (new_state == MOBILE_POWER_STATE_DCH) {
            // DCH + extending FACH time by postponing inactivity timeout
            duration += MOBILE_DCH_INACTIVITY_TIMER;
            dch_energy = MOBILE_DCH_POWER * duration;
            
            // extending FACH time
            double fach_duration = min(time_since_last_mobile_activity(), 
                                       MOBILE_FACH_INACTIVITY_TIMER);
            fach_energy = fach_duration * MOBILE_FACH_POWER;
            LOGD("Now in DCH: extending FACH time by %f seconds "
                 "(costs %d mJ)\n",
                 fach_duration, fach_energy);
        } else {
            // extending FACH time
            double fach_duration = min(time_since_last_mobile_activity(), 
                                       MOBILE_FACH_INACTIVITY_TIMER);
            duration += fach_duration;
            
            fach_energy = duration * MOBILE_FACH_POWER;
            int extend_energy = fach_duration * MOBILE_FACH_POWER;
            LOGD("Extending FACH time by %f seconds (costs %d mJ)\n",
                 fach_duration, extend_energy);
        }
    } else {
        assert(old_state == MOBILE_POWER_STATE_DCH);
        // extending DCH time by postponing inactivity timeout
        double dch_duration = min(time_since_last_mobile_activity(), 
                                  MOBILE_DCH_INACTIVITY_TIMER);
        duration += dch_duration;
        dch_energy = duration * MOBILE_DCH_POWER;
        int extend_energy = dch_duration * MOBILE_DCH_POWER;
        LOGD("Extending DCH time by %f seconds (costs %d mJ)\n",
             dch_duration, extend_energy);
    }
    // XXX: the above can probably be simplified by calculating
    // XXX:  fach_ and dch_ as if old_state was IDLE, then subtracting
    // XXX:  to account for the cases where it's not, in which cases
    // XXX:  we only pay a portion of the FACH/DCH energy with this
    // XXX:  transfer, having paid the full amount in previous transfers.
    
    // account for possible floating-point math errors
    return max(0, fach_energy) + max(0, dch_energy);
}

#include "wifi.h"

int
wifi_channel_rate()
{
    /* Adapted from 
     * $(MY_DROID)/frameworks/base/core/jni/android_net_wifi_Wifi.cpp 
     */
    char reply[256];
    memset(reply, 0, sizeof(reply));
    int linkspeed = -1;
    
    size_t reply_len = sizeof(reply) - 1;
    
    int rc = wifi_connect_to_supplicant();
    if (rc < 0) {
        LOGE("Failed to connect to supplicant\n");
        return rc;
    }
    if (wifi_command("DRIVER LINKSPEED", reply, &reply_len) != 0) {
        LOGE("DRIVER LINKSPEED command failed\n");
        wifi_close_supplicant_connection();
        return -1;
    } else {
        // Strip off trailing newline
        if (reply_len > 0 && reply[reply_len-1] == '\n')
            reply[reply_len-1] = '\0';
        else
            reply[reply_len] = '\0';
    }
    wifi_close_supplicant_connection();
    
    sscanf(reply, "%*s %u", &linkspeed);
    return linkspeed;
}

pthread_mutex_t wifi_state_lock = PTHREAD_MUTEX_INITIALIZER;
static int wifi_last_bytes[2] = {-1, -1};
static int wifi_last_packets[2] = {-1, -1};
static int wifi_data_rates[2] = {-1, -1};
static int wifi_packet_rates[2] = {-1, -1};
static struct timeval last_wifi_observation = {0, 0};

// caller must hold wifi_state_lock
static void calc_rates(int *wifi_rates, int *wifi_last, int *current,
                       double duration)
{
    for (int i = DOWN; i <= UP; ++i) {
        wifi_rates[i] = (int)ceil((current[i] - wifi_last[i]) / duration);
    }
}

// caller must hold wifi_state_lock
static void store_counts(int *wifi_last, int *current)
{
    for (int i = DOWN; i <= UP; ++i) {
        wifi_last[i] = current[i];
    }
}

int
get_net_dev_stats(const char *iface, int bytes[2], int packets[2])
{
    size_t iface_len = strlen(iface);
    
    char filename[] = "/proc/net/dev";
    
    ifstream infile(filename);
    string junk, line;
    
    // skip the header lines
    getline(infile, junk);
    getline(infile, junk);
    
    int rc = -1;
    while (getline(infile, line)) {
        //  Receive                      ...        Transmit
        //  0      1          2                       9        10
        // iface  bytes    packets (...6 fields...) bytes    packets ...
        string cur_iface;
        istringstream in(line);
        in >> cur_iface;
        if (!in) {
            LOGE("Failed to parse ip addr\n");
            break;
        }
        if (cur_iface.compare(0, iface_len, iface) == 0) {
            in >> bytes[DOWN] >> packets[DOWN];
            in >> junk >> junk >> junk
               >> junk >> junk >> junk;
            in >> bytes[UP] >> packets[UP];
            if (!in) {
                LOGE("Failed to parse byte/packet counts\n");
            } else {
                rc = 0;
            }
            break;
        }
    }
    infile.close();
    return rc;
}

int
update_wifi_estimated_rates()
{
    struct timeval now;
    int bytes[2] = {0,0}, packets[2] = {0,0};
    int rc = get_net_dev_stats("tiwlan0", bytes, packets);
    if (rc < 0) {
        return rc;
    }

    TIME(now);
    PthreadScopedLock lock(&wifi_state_lock);
    if (wifi_last_bytes[DOWN] == -1) {
        assert(wifi_last_bytes[UP] == -1 &&
               wifi_last_packets[DOWN] == -1 &&
               wifi_last_packets[UP] == -1 &&
               wifi_data_rates[DOWN] == -1 &&
               wifi_data_rates[UP] == -1 &&
               wifi_packet_rates[DOWN] == -1 &&
               wifi_packet_rates[UP] == -1);
    } else {
        struct timeval diff;
        TIMEDIFF(last_wifi_observation, now, diff);
        double dur = diff.tv_sec + ((double)diff.tv_usec / 1000000.0);
        calc_rates(wifi_data_rates, wifi_last_bytes, bytes, dur);
        calc_rates(wifi_packet_rates, wifi_last_packets, packets, dur);
    }
    store_counts(wifi_last_bytes, bytes);
    store_counts(wifi_last_packets, packets);
    last_wifi_observation = now;
    
    return 0;
}

int
update_mobile_state()
{
    int bytes[2] = {0,0}, dummy[2] = {0,0};
    int rc = get_net_dev_stats("rmnet0", bytes, dummy);
    if (rc < 0) {
        return rc;
    }
    
    bool state_change = false;
    
    PthreadScopedLock lock(&mobile_state_lock);
    if (bytes[DOWN] > mobile_last_bytes[DOWN] ||
        bytes[UP] > mobile_last_bytes[UP]) {
        // there's been activity recently,
        //  and the power state might have changed
        if (mobile_last_bytes[DOWN] != -1) {
            // Make sure we have at least one prior observation
            
            gettimeofday(&last_mobile_activity, NULL);
            
            // Hypothesis: PowerTutor checks the queue length simply by
            //  checking the number of bytes transferred in the last second.
            //  We should do likewise; adding up the socket buffer queues appears
            //  to be missing some downstream bytes.  This will catch them.
            int delta_bytes[2] = {0, 0};
            for (int i = DOWN; i <= UP; ++i) {
                delta_bytes[i] = bytes[i] - mobile_last_bytes[i];
                if ((state_change || mobile_state != MOBILE_POWER_STATE_DCH) &&
                    delta_bytes[i] >= get_dch_threshold(i == DOWN)) {
                    LOGD("%s bytecount (%d) >= threshold (%d) ; "
                         "switching to DCH\n",
                         ((i == DOWN) ? "downlink" : "uplink"),
                         delta_bytes[i], get_dch_threshold(i == DOWN));
                    mobile_state = MOBILE_POWER_STATE_DCH;
                    state_change = true;
                }
                mobile_last_delta_bytes[i] = delta_bytes[i];
            }
            int down_queue = delta_bytes[DOWN], up_queue = delta_bytes[UP];
            if (mobile_state == MOBILE_POWER_STATE_IDLE) {
                state_change = true;
                mobile_state = MOBILE_POWER_STATE_FACH;
            }
            if (down_queue > MOBILE_DCH_THRESHOLD_DOWN ||
                up_queue > MOBILE_DCH_THRESHOLD_UP) {
                if (mobile_state != MOBILE_POWER_STATE_DCH) {
                    state_change = true;
                }
                mobile_state = MOBILE_POWER_STATE_DCH;
            }
        } else {
            for (int i = DOWN; i <= UP; ++i) {
                mobile_last_delta_bytes[i] = 0;
            }
        }
        
        mobile_last_bytes[DOWN] = bytes[DOWN];
        mobile_last_bytes[UP] = bytes[UP];
    } else {
        // idle for the past second
        // check to see if a timeout expired
        double idle_time = time_since_last_mobile_activity(true);
        if (mobile_state == MOBILE_POWER_STATE_DCH) {
            if (idle_time >= MOBILE_DCH_INACTIVITY_TIMER) {
                state_change = true;
                mobile_state = MOBILE_POWER_STATE_FACH;
                
                // mark this state change as an "activity,"
                //   so that we wait for the full FACH timeout
                //   before going to IDLE.
                gettimeofday(&last_mobile_activity, NULL);
            }
        } else if (mobile_state == MOBILE_POWER_STATE_FACH) {
            if (idle_time >= MOBILE_FACH_INACTIVITY_TIMER) {
                state_change = true;
                mobile_state = MOBILE_POWER_STATE_IDLE;
            }
        }
    }
    if (state_change) {
        LOGD("mobile state changed to %s\n", 
             mobile_state_str[mobile_state]);
    }
    return 0;
}

#ifdef BUILDING_SHLIB
static pthread_t update_thread;
static pthread_mutex_t update_thread_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t update_thread_cv = PTHREAD_COND_INITIALIZER;
static bool running = true;
static void *
NetworkStatsUpdateThread(void *)
{
    //struct timespec interval = {0, 100 * 1000 * 1000}; // sample every 100ms
    struct timespec interval = {1, 0}; // sample every 1s
    struct timespec wait_time;
    
    PthreadScopedLock lock(&update_thread_lock);
    while (running) {
        int rc = update_wifi_estimated_rates();
        if (rc < 0) {
            LOGE("Warning: failed to update wifi stats\n");
        }
        rc = update_mobile_state();
        if (rc < 0) {
            LOGE("Warning: failed to update mobile state\n");
        }
        
        wait_time = abs_time(interval);
        pthread_cond_timedwait(&update_thread_cv, &update_thread_lock, 
                               &wait_time);
    }
    return NULL;
}

static void libpowertutor_init() __attribute__((constructor));
static void libpowertutor_init()
{
    LOGD("In libpowertutor_init\n");
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    int rc = pthread_create(&update_thread, &attr, 
                            NetworkStatsUpdateThread, NULL);
    if (rc != 0) {
        LOGE("Warning: failed to create update thread!\n");
    }
}

static void libpowertutor_fin() __attribute__((destructor));
static void libpowertutor_fin()
{
    LOGD("In libpowertutor_fin\n");
    PthreadScopedLock lock(&update_thread_lock);
    running = false;
    pthread_cond_signal(&update_thread_cv);
}
#endif

int
wifi_uplink_data_rate()
{
    PthreadScopedLock lock(&wifi_state_lock);
    return wifi_data_rates[UP];
}

int
wifi_packet_rate()
{
    PthreadScopedLock lock(&wifi_state_lock);
    return wifi_packet_rates[DOWN] + wifi_packet_rates[UP];
}

static inline int 
wifi_channel_rate_component()
{
    // These are both in Mbps.
    int channel_rate = wifi_channel_rate();
    double uplink_data_rate = int(wifi_uplink_data_rate() * 8.0 / 1000000.0);
    int channel_rate_component = (48 - 0.768 * channel_rate)*uplink_data_rate;
    LOGD("channel rate %d uplink data rate %f chrate component %d\n",
         channel_rate, uplink_data_rate, channel_rate_component);
    return channel_rate_component;
}

static inline int
wifi_mtu()
{
    // could get from ifreq ioctl, but it's 1500, so not bothering for now.
    const int WIFI_DEV_MTU = 1500;
    return (WIFI_DEV_MTU - TCP_HDR_SIZE - IP_HDR_SIZE);
}

static inline bool
wifi_high_state(size_t datalen)
{
    // estimate whether this transfer will push the radio 
    //  into the high-power state. (pkts = datalen/MTU)
    // TODO: figure out if this estimation is accurate based 
    // TODO:  only on data size and the MTU, or if I need to be more careful.
    // TODO:  my guess is that this will underestimate the number of packets
    // TODO:  required for a transmission, because it doesn't include
    // TODO:  e.g. TCP control messages.
    int cur_packets = datalen / wifi_mtu();
    cur_packets += (datalen % wifi_mtu() > 0) ? 1 : 0; // round up
    int packet_rate = wifi_packet_rate();
    LOGD("Wifi packet rate: %d  cur_packets: %d\n", packet_rate, cur_packets);
    return (cur_packets + packet_rate) > 15;
}

static int 
estimate_wifi_energy_cost(size_t datalen, size_t bandwidth)
{
    // update wifi stats here to get more current estimate.
    //int rc = update_wifi_estimated_rates();
    //if (rc < 0) {
    //    LOGE("Warning: failed to update wifi stats\n");
    //}
    
    int power = 0;
    
    // The wifi radio is only in the transmit state for a very short time,
    //  and that power consumption is factored into the high/low states'
    //  power calculation.
    if (wifi_high_state(datalen)) {
        power = WIFI_HIGH_POWER_BASE + wifi_channel_rate_component();
    } else {
        power = WIFI_LOW_POWER;
    }
    
    // TODO: finish
    
    return (((double)datalen) / bandwidth) * power;
}

// XXX: It may be useful/necessary to factor into these calculations how
// XXX:  energy cost is amortized over several transmissions.
// XXX:  For example, if the cellular interface is idle,
// XXX:  the initial state transition is a large jump in power,
// XXX:  but then the radio will remain in that state for a while, 
// XXX:  during which it makes sense to keep sending to amortize the
// XXX:  "tail energy" over several transmissions.  Something like that.

// TODO: consider adding a function that estimates how much it will cost
// TODO:  to receive N bytes.

int estimate_energy_cost(NetworkType type, // bool downlink, 
                         size_t datalen, size_t bandwidth)
{
    if (type == TYPE_MOBILE) {
        return estimate_mobile_energy_cost(datalen, bandwidth);
    } else if (type == TYPE_WIFI) {
        return estimate_wifi_energy_cost(datalen, bandwidth);
    } else assert(false);
    
    return -1;
}

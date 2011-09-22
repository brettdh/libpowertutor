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
#include "power_model.h"
#include "timeops.h"
#include "utils.h"

#include "debug.h"

#define MOBILE_IFACE "rmnet0"

using std::ifstream; using std::hex; using std::string;
using std::istringstream;
using std::min; using std::max;

static const int TCP_HDR_SIZE = 32;
static const int IP_HDR_SIZE = 20;

const char *mobile_state_str[3] = {
    "IDLE", "FACH", "DCH"
};

static PowerModel *powerModel;

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
static struct timeval last_mobile_sample_time = {0, 0};
static int mobile_last_bytes[2] = {-1, -1};
static int mobile_last_delta_bytes[2] = {0, 0};

#ifdef ANDROID
static activity_callback_t mobile_activity_callback = NULL;

#  ifdef BUILDING_SHLIB
static activity_callback_t
get_activity_callback(void)
{
    PthreadScopedLock lock(&mobile_state_lock);
    return mobile_activity_callback;
}
#  endif
#endif

// remote power model state: only what's needed for decent accuracy
static pthread_mutex_t remote_power_state_lock = PTHREAD_MUTEX_INITIALIZER;
// TODO: support multiple remotes.
static struct timeval remote_last_mobile_activity = {0, 0};
static struct remote_power_state remote_state = {
    MOBILE_POWER_STATE_IDLE, {0, 0}, 0
};


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

static inline MobileState
get_mobile_state()
{
    if (power_model_is_remote()) {
        PthreadScopedLock lock(&remote_power_state_lock);
        return (MobileState) remote_state.mobile_state;
    } else {
        PthreadScopedLock lock(&mobile_state_lock);
        return mobile_state;
    }
}

int get_net_dev_stats(const char *iface, int bytes[2], int packets[2]);

void
get_mobile_queue_len(int *down_queue, int *up_queue)
{
    if (power_model_is_remote()) {
        PthreadScopedLock lock(&remote_power_state_lock);
        if (down_queue) {
            *down_queue = remote_state.mobile_queue_len[DOWN];
        }
        if (up_queue) {
            *up_queue = remote_state.mobile_queue_len[UP];
        }
        return;
    }

    // here, we know we're on the handset.
    size_t up_bytes = 0, down_bytes = 0;

    //  Two ways to do this:
    //  1) Return the delta_bytes from the last sample.
    //   (+) Simple
    //   (-) The queue might have filled more since then
    //  2) Estimate the delta_bytes from the last second.
    //   (+) Might grab a more up-to-date estimate of the queues
    //   (-) Trickier to implement
    //  3) Combine the previous sample and the time since the past sample.
    //   (+) Easy to implement
    //   (+) Counts the most recent queued bytes
    //   (-) Might overestimate the queue
    // Right now we're experimenting with (3).
    int bytes[2] = {-1, -1}, dummy[2];
    int recent_bytes[2] = {0, 0};
    int rc = get_net_dev_stats(MOBILE_IFACE, bytes, dummy);

    PthreadScopedLock lock(&mobile_state_lock);
    struct timeval now, diff;
    gettimeofday(&now, NULL);
    TIMEDIFF(last_mobile_sample_time, now, diff);
    down_bytes = mobile_last_delta_bytes[DOWN];
    up_bytes = mobile_last_delta_bytes[UP];

    if (rc == 0 && mobile_last_bytes[DOWN] != -1) {
        recent_bytes[DOWN] = bytes[DOWN] - mobile_last_bytes[DOWN];
        recent_bytes[UP] = bytes[UP] - mobile_last_bytes[UP];
    }
    down_bytes += recent_bytes[DOWN];
    up_bytes += recent_bytes[UP];

    LOGD("Queue length estimation:\n");
    LOGD("   last sample time: %lu.%06lu seconds ago\n",
         diff.tv_sec, diff.tv_usec);
    LOGD("   downlink: %d bytes from last sample period; %d bytes since\n", 
         mobile_last_delta_bytes[DOWN], recent_bytes[DOWN]);
    LOGD("   uplink: %d bytes from last sample period; %d bytes since\n", 
         mobile_last_delta_bytes[UP], recent_bytes[UP]);
    
    if (down_queue) {
        *down_queue = down_bytes;
    }
    if (up_queue) {
        *up_queue = up_bytes;
    }
}

static inline double
time_since(struct timeval then)
{
    struct timeval dur, now;
    TIME(now);
    TIMEDIFF(then, now, dur);
    return dur.tv_sec + (((double)dur.tv_usec) / 1000000.0);
}

static inline double
time_since_last_mobile_activity(bool already_locked=false)
{
    pthread_mutex_t *the_lock = NULL;
    struct timeval *activity = NULL;
    if (power_model_is_remote()) {
        the_lock = &remote_power_state_lock;
        activity = &remote_last_mobile_activity;
    } else {
        the_lock = &mobile_state_lock;
        activity = &last_mobile_activity;
    }
    
    PthreadScopedLock lock;
    if (!already_locked) { 
        lock.acquire(the_lock);
    }
    return time_since(*activity);
}


int 
estimate_mobile_energy_cost(size_t datalen, size_t bandwidth, size_t rtt_ms)
{
    // XXX: this doesn't account for the impact that the large
    // XXX:  RTT of the 3G connection will have on the time it takes
    // XXX:  to stop sending all the TCP ACKs.
    // XXX: a higher-level problem is that in order to make this guess
    // XXX:  accurate, I have to accurately guess what TCP is going to do
    // XXX:  with the data.
    // TODO: make it better.
    
     // add in TCP/IP headers
     // XXX: this ignores the possibility of multiple sends
     // XXX:  being coalesced and sent with only one TCP packet.
    datalen += (TCP_HDR_SIZE + IP_HDR_SIZE);
    
    bool downlink = power_model_is_remote();
    
    MobileState old_state = get_mobile_state();
    MobileState new_state = MOBILE_POWER_STATE_FACH;
    
    int queue_len = 0, ack_dir_queue_len = 0;
    if (downlink) {
        // the handset's downlink queue is the queue we're sending into.
        //  it will send acks into its uplink queue.
        get_mobile_queue_len(&queue_len, &ack_dir_queue_len);
    } else {
        // the handset is sending into its uplink queue.
        //  acks will arrive in its downlink queue.
        get_mobile_queue_len(&ack_dir_queue_len, &queue_len);
    }
    
    int threshold = powerModel->get_dch_threshold(downlink);
    LOGD("DCH threshold %d -- queue length %d (%d predicted)\n",
         threshold, queue_len, queue_len + datalen);
    if (old_state == MOBILE_POWER_STATE_DCH ||
        (queue_len + datalen) >= (size_t) threshold) {
        new_state = MOBILE_POWER_STATE_DCH;
    }
    
    int ack_dir_threshold = powerModel->get_dch_threshold(!downlink);
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
    duration += rtt_ms / 1000.0;  // accounting for the last TCP ack
    
    int fach_energy = 0, dch_energy = 0;
    if (old_state == MOBILE_POWER_STATE_IDLE) {
        if (new_state == MOBILE_POWER_STATE_DCH) {
            // FACH inactivity timeout only; data transferred in DCH
            fach_energy = (powerModel->MOBILE_FACH_POWER * 
                           powerModel->MOBILE_FACH_INACTIVITY_TIMER);
            
            duration += powerModel->MOBILE_DCH_INACTIVITY_TIMER;
            dch_energy = powerModel->MOBILE_DCH_POWER * duration;
            LOGD("Now in DCH: dch_energy %d fach_energy %d\n",
                 dch_energy, fach_energy);
        } else {
            // cost of transferring data in FACH state + FACH timeout
            duration += powerModel->MOBILE_FACH_INACTIVITY_TIMER;
            fach_energy = powerModel->MOBILE_FACH_POWER * duration;
            LOGD("Now in FACH: fach_energy %d\n", fach_energy);
        }
    } else if (old_state == MOBILE_POWER_STATE_FACH) {
        if (new_state == MOBILE_POWER_STATE_DCH) {
            // DCH + extending FACH time by postponing inactivity timeout
            duration += powerModel->MOBILE_DCH_INACTIVITY_TIMER;
            dch_energy = powerModel->MOBILE_DCH_POWER * duration;
            
            // extending FACH time
            double fach_duration = min(time_since_last_mobile_activity(), 
                                       powerModel->MOBILE_FACH_INACTIVITY_TIMER);
            fach_energy = fach_duration * powerModel->MOBILE_FACH_POWER;
            LOGD("Now in DCH: extending FACH time by %f seconds "
                 "(costs %d mJ)\n",
                 fach_duration, fach_energy);
        } else {
            // extending FACH time
            double fach_duration = min(time_since_last_mobile_activity(), 
                                       powerModel->MOBILE_FACH_INACTIVITY_TIMER);
            duration += fach_duration;
            
            fach_energy = duration * powerModel->MOBILE_FACH_POWER;
            int extend_energy = fach_duration * powerModel->MOBILE_FACH_POWER;
            LOGD("Extending FACH time by %f seconds (costs %d mJ)\n",
                 fach_duration, extend_energy);
        }
    } else {
        assert(old_state == MOBILE_POWER_STATE_DCH);
        // extending DCH time by postponing inactivity timeout
        double dch_duration = min(time_since_last_mobile_activity(), 
                                  powerModel->MOBILE_DCH_INACTIVITY_TIMER);
        duration += dch_duration;
        dch_energy = duration * powerModel->MOBILE_DCH_POWER;
        int extend_energy = dch_duration * powerModel->MOBILE_DCH_POWER;
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

const int MAX_80211G_CHANNEL_RATE = 54;

int
wifi_channel_rate()
{
#ifdef ANDROID
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
        return -1;
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
#else
    // TOD: IMPL (get from remote)
    return MAX_80211G_CHANNEL_RATE;
#endif
}


pthread_mutex_t wifi_state_lock = PTHREAD_MUTEX_INITIALIZER;
static int wifi_data_rates[2] = {-1, -1};
static int wifi_packet_rates[2] = {-1, -1};

#ifdef ANDROID
static int wifi_last_bytes[2] = {-1, -1};
static int wifi_last_packets[2] = {-1, -1};
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
#endif

int
get_net_dev_stats(const char *iface, int bytes[2], int packets[2])
{
    size_t iface_len = strlen(iface);
    
    // XXX: might be faster to just read the integers from
    // XXX:   /sys/devices/virtual/net/<iface>/statistics/
    // XXX:    [rx_bytes, tx_bytes, rx_packets, tx_packets]
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

#ifdef ANDROID
int
update_wifi_estimated_rates(bool& fire_callback)
{
    struct timeval now, diff;
    int bytes[2] = {0,0}, packets[2] = {0,0};
    int rc = get_net_dev_stats(powerModel->wifi_iface(), bytes, packets);
    if (rc < 0) {
        return rc;
    }

    TIME(now);
    PthreadScopedLock lock(&wifi_state_lock);
    TIMEDIFF(last_wifi_observation, now, diff);
    if (diff.tv_sec == 0) {
        // too soon to sample again; wait until a second has passed
        return 0;
    }
    
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
    
    // hack: only notify if the packet rate gets high enough that
    //  it might have an impact on small, frequent packet streams.
    //  This avoids reporting incessantly on the packets that 
    //  are being sent to inform about the remote power model itself.
    if ((wifi_packet_rates[DOWN] + wifi_packet_rates[UP]) > 5) {
        fire_callback = true;
    }
    
    return 0;
}

int
update_mobile_state(bool& fire_callback)
{
    int bytes[2] = {0,0}, dummy[2] = {0,0};
    int rc = get_net_dev_stats(MOBILE_IFACE, bytes, dummy);
    if (rc < 0) {
        return rc;
    }
    
    bool state_change = false;
    bool mobile_activity = false;
    
    PthreadScopedLock lock(&mobile_state_lock);
    struct timeval now, diff;
    gettimeofday(&now, NULL);
    TIMEDIFF(last_mobile_sample_time, now, diff);
    if (diff.tv_sec >= 1 && // only sample this data every second
        (bytes[DOWN] > mobile_last_bytes[DOWN] ||
         bytes[UP] > mobile_last_bytes[UP])) {
        // there's been activity recently,
        //  and the power state might have changed
        gettimeofday(&last_mobile_sample_time, NULL);
        if (mobile_last_bytes[DOWN] != -1) {
            // Make sure we have at least one prior observation
            
            gettimeofday(&last_mobile_activity, NULL);
            mobile_activity = true;
            
            // Hypothesis: PowerTutor checks the queue length simply by
            //  checking the number of bytes transferred in the last second.
            //  We should do likewise; adding up the socket buffer queues appears
            //  to be missing some downstream bytes.  This will catch them.
            int delta_bytes[2] = {0, 0};
            for (int i = DOWN; i <= UP; ++i) {
                delta_bytes[i] = bytes[i] - mobile_last_bytes[i];
                if ((state_change || mobile_state != MOBILE_POWER_STATE_DCH) &&
                    delta_bytes[i] >= powerModel->get_dch_threshold(i == DOWN)) {
                    LOGD("%s bytecount (%d) >= threshold (%d) ; "
                         "switching to DCH\n",
                         ((i == DOWN) ? "downlink" : "uplink"),
                         delta_bytes[i], powerModel->get_dch_threshold(i == DOWN));
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
            if (down_queue > powerModel->MOBILE_DCH_THRESHOLD_DOWN ||
                up_queue > powerModel->MOBILE_DCH_THRESHOLD_UP) {
                if (mobile_state != MOBILE_POWER_STATE_DCH) {
                    state_change = true;
                }
                mobile_state = MOBILE_POWER_STATE_DCH;
            }
        }
        
        mobile_last_bytes[DOWN] = bytes[DOWN];
        mobile_last_bytes[UP] = bytes[UP];
    } else {
        // This still gets checked every 100ms or however often.

        // no activity recently; reset these to zero
        for (int i = DOWN; i <= UP; ++i) {
            mobile_last_delta_bytes[i] = 0;
        }

        // check to see if a timeout expired
        double idle_time = time_since_last_mobile_activity(true);
        if (mobile_state == MOBILE_POWER_STATE_DCH) {
            if (idle_time >= powerModel->MOBILE_DCH_INACTIVITY_TIMER) {
                state_change = true;
                mobile_state = MOBILE_POWER_STATE_FACH;
                
                // mark this state change as an "activity,"
                //   so that we wait for the full FACH timeout
                //   before going to IDLE.
                gettimeofday(&last_mobile_activity, NULL);

                // We don't fire the activity callback as a result of this,
                //   because it is the remote code's responsibility to 
                //   watch for the timeouts, and that is separate from this
                //   update code, which only runs on the handset.
                // It is the remote code's responsibility beacuse, 
                //   in the absence of activity, the timeouts can be detected
                //   without using the network.
            }
        } else if (mobile_state == MOBILE_POWER_STATE_FACH) {
            if (idle_time >= powerModel->MOBILE_FACH_INACTIVITY_TIMER) {
                state_change = true;
                mobile_state = MOBILE_POWER_STATE_IDLE;
            }
        }
    }

    if (state_change) {
        LOGD("mobile state changed to %s\n", 
             mobile_state_str[mobile_state]);
    }

    if (mobile_activity) {
        fire_callback = true;
    }
    return 0;
}
#endif // ANDROID

#ifndef ANDROID
static void
update_remote_power_model()
{
    bool state_change = false;

    PthreadScopedLock lock(&remote_power_state_lock);
    double idle_time = time_since(remote_last_mobile_activity);
    if (remote_state.mobile_state == MOBILE_POWER_STATE_DCH) {
        if (idle_time >= MOBILE_DCH_INACTIVITY_TIMER) {
            state_change = true;
            remote_state.mobile_state = MOBILE_POWER_STATE_FACH;
            
            // mark this state change as an "activity,"
            //   so that we wait for the full FACH timeout
            //   before going to IDLE.
            gettimeofday(&remote_last_mobile_activity, NULL);
        }
    } else if (remote_state.mobile_state == MOBILE_POWER_STATE_FACH) {
        if (idle_time >= MOBILE_FACH_INACTIVITY_TIMER) {
            state_change = true;
            remote_state.mobile_state = MOBILE_POWER_STATE_IDLE;
        }
    }
    
    // the handset's 1-second polling rate, plus a fudge factor to
    //   cover the rtt.  If we don't hear an update for this long,
    //   we assume that nothing has happened; the queues have emptied
    //   and the packet rate has dropped off.
    const double REMOTE_POWER_STATE_INVALID_TIMEOUT = 1.5;
    if (idle_time >= REMOTE_POWER_STATE_INVALID_TIMEOUT) {
        remote_state.mobile_queue_len[DOWN] = 0;
        remote_state.mobile_queue_len[UP] = 0;
        remote_state.wifi_packet_rate = 0;
    }
    
    if (state_change) {
        LOGD("Remote mobile state changed to %s\n",
             mobile_state_str[remote_state.mobile_state]);
    }
}
#endif

int wifi_packet_rate();

#ifdef BUILDING_SHLIB
static pthread_t update_thread;
static pthread_mutex_t update_thread_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t update_thread_cv = PTHREAD_COND_INITIALIZER;
static bool running = true;
static void *
NetworkStatsUpdateThread(void *)
{
    struct timespec interval = {0, 100 * 1000 * 1000}; // sample every 100ms
    //struct timespec interval = {1, 0}; // sample every 1s
    struct timespec wait_time;
    
    PthreadScopedLock lock(&update_thread_lock);
    while (running) {
#ifdef ANDROID
        bool fire_callback = false;
        int rc = update_wifi_estimated_rates(fire_callback);
        if (rc < 0) {
            LOGE("Warning: failed to update wifi stats\n");
        }
        rc = update_mobile_state(fire_callback);
        if (rc < 0) {
            LOGE("Warning: failed to update mobile state\n");
        }
        
        if (fire_callback) {
            activity_callback_t callback = get_activity_callback();
            if (callback) {
                struct remote_power_state state;
                state.mobile_state = get_mobile_state();
                get_mobile_queue_len(&state.mobile_queue_len[DOWN],
                                     &state.mobile_queue_len[UP]);
                state.wifi_packet_rate = wifi_packet_rate();
                
                callback(state);
            }
        }
#else
        // TODO: This could work on Android, too (handset-to-handset)
        // TODO:  (but doesn't right now)
        update_remote_power_model();
#endif
        
        wait_time = abs_time(interval);
        pthread_cond_timedwait(&update_thread_cv, &update_thread_lock, 
                               &wait_time);
    }
    return NULL;
}

static void libpowertutor_init() __attribute__((constructor));
static void libpowertutor_init()
{
    // hard-coded power model choice for now; could guess from phone info
    powerModel = PowerModel::get(NEXUS_ONE);
    
    LOGD("Starting update thread\n");
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
#endif // BUILDING_SHLIB

int
wifi_uplink_data_rate()
{
    PthreadScopedLock lock(&wifi_state_lock);
    return wifi_data_rates[UP];
}

int
wifi_packet_rate()
{
    if (power_model_is_remote()) {
        // keeping track of these stats from the handset
        //  only marginally improves accuracy.
        // I might add them later. (TODO?)
        PthreadScopedLock lock(&remote_power_state_lock);
        return remote_state.wifi_packet_rate;
    } else {
        PthreadScopedLock lock(&wifi_state_lock);
        return wifi_packet_rates[DOWN] + wifi_packet_rates[UP];
    }
}

static inline int 
wifi_channel_rate_component()
{
    if (power_model_is_remote()) {
        // keeping track of these stats from the handset
        //  only marginally improves accuracy.
        // I might add them later. (TODO?)
        return 0;
    } else {
        // These are both in Mbps.
        // XXX: wifi_channel_rate only succeeds if we have root perms.
        // TODO: make this work for non-root apps calling this function.
        int channel_rate = wifi_channel_rate();
        if (channel_rate == -1) {
            LOGE("Warning: failed to get channel rate; using default\n");
            channel_rate = MAX_80211G_CHANNEL_RATE;
        }
        double uplink_data_rate = int(wifi_uplink_data_rate() * 8.0 / 1000000.0);
        int channel_rate_component = (48 - 0.768 * channel_rate)*uplink_data_rate;
        LOGD("channel rate %d uplink data rate %f chrate component %d\n",
             channel_rate, uplink_data_rate, channel_rate_component);
        return channel_rate_component;
    }
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
    return (cur_packets + packet_rate) > powerModel->WIFI_PACKET_RATE_THRESHOLD;
}

int 
estimate_wifi_energy_cost(size_t datalen, size_t bandwidth, size_t rtt_ms)
{
    int power = 0;
    
    // The wifi radio is only in the transmit state for a very short time,
    //  and that power consumption is factored into the high/low states'
    //  power calculation.
    if (wifi_high_state(datalen)) {
        power = powerModel->WIFI_HIGH_POWER_BASE + wifi_channel_rate_component();
    } else {
        power = powerModel->WIFI_LOW_POWER;
    }
    
    // slightly hacky, but here's what's going on here:
    // 1) PowerTutor's calculation for the WiFi power model:
    //    a) Figure out when the wifi is in the high-power state (pkt rate)
    //    b) Calculate the power draw of that state
    //    c) Model the wifi as uniform consumption over each second
    //       that it's in the high-power state.
    // 2) As a result, even if it was only briefly in the high-power state,
    //    PowerTutor models the entire second as high-power.
    // 3) I emulate this by rounding up to the next second before
    //    multiplying by power.
    return ceil((((double)datalen) / bandwidth) + (rtt_ms / 1000.0)) * power;
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
// Considered and rejected.  The receiver doesn't usually know how many
//   bytes it will receive; the remote sender does, though.
// TODO: reconsider this, as there are definitely applications for which
// TODO: this makes sense. (IMAP is one.)

int estimate_energy_cost(NetworkType type, // bool downlink, 
                         size_t datalen, size_t bandwidth, size_t rtt_ms)
{
    if (type == TYPE_MOBILE) {
        return estimate_mobile_energy_cost(datalen, bandwidth, rtt_ms);
    } else if (type == TYPE_WIFI) {
        return estimate_wifi_energy_cost(datalen, bandwidth, rtt_ms);
    } else assert(false);
    
    return -1;
}


// only on handsets.
void register_mobile_activity_callback(activity_callback_t callback)
{
#ifdef ANDROID
    PthreadScopedLock lock(&mobile_state_lock);
    mobile_activity_callback = callback;
#endif
}

void report_remote_mobile_activity(/* struct in_addr ip_addr, */ 
                                   struct remote_power_state state)
{
    PthreadScopedLock lock(&remote_power_state_lock);

    // TODO: support multiple remotes.
    remote_state = state;
    if ((state.mobile_queue_len[DOWN] + state.mobile_queue_len[UP]) > 0) {
        // only update if the mobile queue lengths have changed,
        //  which indicates activity on the 3G interface.
        // If they are zero, this is just a wifi packet rate update.
        gettimeofday(&remote_last_mobile_activity, NULL);
    }
}

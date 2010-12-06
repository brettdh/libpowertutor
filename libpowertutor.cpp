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

#define LOG_TAG "libpowertutor"
#include <cutils/log.h>

using std::ifstream; using std::hex; using std::string;
using std::istringstream;
using std::min; using std::max;

// PowerTutor [1] power model for the HTC Dream.
// [1] Zhang et al. "Accurate Online Power Estimation and Automatic Battery
// Behavior Based Power Model Generation for Smartphones," CODES+ISSS '10.

// all power vals in mW
static const int WIFI_TRANSMIT_POWER = 1000;
static const int WIFI_HIGH_POWER_BASE = 710;
static const int WIFI_LOW_POWER = 20;

enum MobileState {
    MOBILE_POWER_STATE_IDLE=0,
    MOBILE_POWER_STATE_FACH,
    MOBILE_POWER_STATE_DCH
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

pthread_mutex_t mobile_state_lock = PTHREAD_MUTEX_INITIALIZER;
static MobileState mobile_state = MOBILE_POWER_STATE_IDLE;
static struct timeval last_mobile_activity;

static inline MobileState
get_mobile_state()
{
    PthreadScopedLock lock(&mobile_state_lock);
    return mobile_state;
}

static int 
get_ip_addr(const char *ifname, struct in_addr *ip_addr)
{
    if (strlen(ifname) > IF_NAMESIZE) {
        LOGE("Error: ifname too long (longer than %d)\n", IF_NAMESIZE);
        return -1;
    }

    struct ifreq ifr;
    strcpy(ifr.ifr_name, ifname);
    ifr.ifr_addr.sa_family = AF_INET;

    int sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        LOGE("socket: %s\n", strerror(errno));
        return sock;
    }
    int rc = ioctl(sock, SIOCGIFADDR, &ifr);
    if (rc == 0) {
        struct sockaddr_in *inaddr = (struct sockaddr_in*)&ifr.ifr_addr;
        memcpy(ip_addr, &inaddr->sin_addr, sizeof(ip_addr));
    } else {
        LOGE("getting ip addr: ioctl: %s\n", strerror(errno));
    }

    close(sock);
    return rc;
}

int
get_mobile_queue_len(bool downlink)
{
    const char *iface = "rmnet0";
    struct in_addr mobile_addr;
    if (get_ip_addr(iface, &mobile_addr) != 0) {
        LOGE("Failed to get ip addr of %s\n", iface);
        return -1;
    }
    
    char filename[50];
    strcpy(filename, "/proc/net/");
    
    char *suffix = filename + strlen(filename);
    
    size_t up_bytes = 0, down_bytes = 0;
    
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
    
    return downlink ? down_bytes : up_bytes;
}

static inline double
time_since_last_mobile_activity()
{
    struct timeval dur, now;
    TIME(now);
    {
        PthreadScopedLock lock(&mobile_state_lock);
        TIMEDIFF(last_mobile_activity, now, dur);
    }
    return dur.tv_sec + (((double)dur.tv_usec) / 1000000.0);
}

static int 
estimate_mobile_energy_cost(bool downlink, int datalen, size_t bandwidth)
{
    MobileState old_state = get_mobile_state();
    MobileState new_state = MOBILE_POWER_STATE_FACH;
    
    int queue_len = get_mobile_queue_len(downlink);
    int threshold = get_dch_threshold(downlink);
    if (old_state == MOBILE_POWER_STATE_DCH ||
        (queue_len + datalen) >= threshold) {
        new_state = MOBILE_POWER_STATE_DCH;
    }
    
    double duration = ((double)datalen) / bandwidth;
    int fach_energy = 0, dch_energy = 0;
    if (old_state == MOBILE_POWER_STATE_IDLE) {
        if (new_state == MOBILE_POWER_STATE_DCH) {
            // FACH inactivity timeout only; data transferred in DCH
            fach_energy = MOBILE_FACH_POWER * MOBILE_FACH_INACTIVITY_TIMER;
            
            duration += MOBILE_DCH_INACTIVITY_TIMER;
            dch_energy = MOBILE_DCH_POWER * duration;
        } else {
            // cost of transferring data in FACH state + FACH timeout
            duration += MOBILE_FACH_INACTIVITY_TIMER;
            fach_energy = MOBILE_FACH_POWER * duration;
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
        } else {
            // extending FACH time
            double fach_duration = min(time_since_last_mobile_activity(), 
                                       MOBILE_FACH_INACTIVITY_TIMER);
            duration += fach_duration;
            
            fach_energy = duration * MOBILE_FACH_POWER;
        }
    } else {
        assert(old_state == MOBILE_POWER_STATE_DCH);
        // extending DCH time by postponing inactivity timeout
        double dch_duration = min(time_since_last_mobile_activity(), 
                                  MOBILE_DCH_INACTIVITY_TIMER);
        duration += dch_duration;
        dch_energy = duration * MOBILE_DCH_POWER;
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
enum dir {DOWN=0, UP};
static int wifi_last_bytes[2] = {-1, -1};
static int wifi_last_packets[2] = {-1, -1};
static int wifi_data_rates[2] = {-1, -1};
static int wifi_packet_rates[2] = {-1, -1};
struct timeval last_wifi_observation = {0, 0};

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
update_wifi_estimated_rates()
{
    const char *iface = "tiwlan0";
    size_t iface_len = strlen(iface);
    
    char filename[] = "/proc/net/dev";
    
    struct timeval now;
    
    ifstream infile(filename);
    string junk, line;
    
    // skip the header lines
    getline(infile, junk);
    getline(infile, junk);
    
    while (getline(infile, line)) {
        //  Receive                      ...        Transmit
        //  0      1          2                       9        10
        // iface  bytes    packets (...6 fields...) bytes    packets ...
        string cur_iface;
        istringstream in(line);
        in >> cur_iface;
        if (!in) {
            LOGE("Failed to parse ip addr\n");
            return -1;
        }
        if (cur_iface.compare(0, iface_len, iface) != 0) {
            continue;
        }
        
        int bytes[2] = {0,0}, packets[2] = {0,0};
        in >> bytes[DOWN] >> packets[DOWN];
        in >> junk >> junk >> junk
           >> junk >> junk >> junk;
        in >> bytes[UP] >> packets[UP];
        if (!in) {
            LOGE("Failed to parse byte/packet counts\n");
            return -1;
        }
        //getline(in, junk);

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
    }
    infile.close();
    
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
    struct timespec interval = {1, 0};
    struct timespec wait_time;
    
    PthreadScopedLock lock(&update_thread_lock);
    while (running) {
        int rc = update_wifi_estimated_rates();
        if (rc < 0) {
            LOGE("Warning: failed to update network stats\n");
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
    return (48 - 0.768 * wifi_channel_rate()) * wifi_uplink_data_rate();
}

static inline int
wifi_mtu()
{
    // could get from ifreq ioctl, but it's 1500, so not bothering for now.
    return 1500;
}

static inline bool
wifi_high_state(size_t datalen)
{
    /* TODO - IMPL */
    (void)downlink;
    (void)datalen;
    // TODO: pick up here.  estimate whether this transfer will push
    // TODO:  the radio into the high-power state. (pkts = datalen/MTU)
    // TODO: also, figure out if this estimation is accurate based 
    // TODO:  only on data size and the MTU, or if I need to be more careful.
    return false;
}

static int 
estimate_wifi_energy_cost(bool downlink, size_t datalen, size_t bandwidth)
{
    int power = 0;
    
    // The wifi radio is only in the transmit state for a very short time,
    //  and that power consumption is factored into the high/low states'
    //  power calculation.
    if (wifi_high_state(downlink, datalen)) {
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

int estimate_energy_cost(NetworkType type, bool downlink, 
                         size_t datalen, size_t bandwidth)
{
    if (type == TYPE_MOBILE) {
        return estimate_mobile_energy_cost(downlink, datalen, bandwidth);
    } else if (type == TYPE_WIFI) {
        return estimate_wifi_energy_cost(downlink, datalen, bandwidth);
    } else assert(false);
    
    return -1;
}

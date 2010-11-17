#include "libpowertutor.h"

// PowerTutor [1] power model for the HTC Dream.
// [1] Zhang et al. "Accurate Online Power Estimation and Automatic Battery
// Behavior Based Power Model Generation for Smartphones," CODES+ISSS '10.

// all power vals in mW
static const int WIFI_TRANSMIT_POWER = 1000;
static const int WIFI_HIGH_POWER_BASE = 710;
static const int WIFI_LOW_POWER = 20;

// XXX: These are for 3G only; careful when testing!
static const int MOBILE_IDLE_POWER = 10;
static const int MOBILE_FACH_POWER = 401;
static const int MOBILE_DCH_POWER = 570;

// in seconds
static const int MOBILE_FACH_INACTIVITY_TIMER = 6;
static const int MOBILE_DCH_INACTIVITY_TIMER = 4;

/* Information needed to make calculations (based on power state inference):
 * 1) Uplink/downlink queue size
 *    - Look in /proc/net/{tcp,udp,raw} and add up tx_queue and rx_queue
 * 2) Packet rate and data rate (bytes/sec)
 *    - Look in /proc/net/dev ; sample over time to get rates
 */

static inline int estimate_mobile_energy_cost(int sock, size_t datalen)
{
    int power = 0;
    if (MOBILE_POWER_STATE_IDLE) {
        power = MOBILE_IDLE_POWER;
    } else if (MOBILE_POWER_STATE_FACH) {
        power = MOBILE_FACH_POWER;
    } else if (MOBILE_POWER_STATE_DCH) {
        power = MOBILE_DCH_POWER;
    } else assert(0);
    
    // TODO: calculate projected energy cost of this transfer,
    //  based on current power state and expected power state(s)
    //  throughout the transfer.  Different for cellular vs. wifi
    return calculate_cellular_energy(sock, datalen, power);
}

static inline int wifi_channel_rate_component()
{
    return (48 - 0.768 * wifi_channel_rate) * wifi_data_rate;
}

static inline int estimate_wifi_energy_cost(int sock, size_t datalen)
{
    int power = 0;
    
    if (wifi_transmitting) {
        power = WIFI_TRANSMIT_POWER;
    } else {
        if (wifi_high_state) {
            power = WIFI_HIGH_POWER_BASE + wifi_channel_rate_component();
        } else {
            power = WIFI_LOW_POWER;
        }
    }
    
    // TODO: calculate projected energy cost of this transfer,
    //  based on current power state and expected power state(s)
    //  throughout the transfer.  Different for cellular vs. wifi
    return calculate_wifi_energy(sock, datalen, power);
}

// XXX: It may be useful/necessary to factor into these calculations how
// XXX:  energy cost is amortized over several transmissions.
// XXX:  For example, if the cellular interface is idle,
// XXX:  the initial state transition is a large jump in power,
// XXX:  but then the radio will remain in that state for a while, 
// XXX:  during which it makes sense to keep sending to amortize the
// XXX:  "tail energy" over several transmissions.  Something like that.

int estimate_energy_cost(NetworkType type, int sock, size_t datalen)
{
    if (type == TYPE_MOBILE) {
        return estimate_mobile_power_cost(sock, datalen);
    } else if (type == TYPE_WIFI) {
        return estimate_wifi_power_cost(sock, datalen);
    } else assert(false);
}

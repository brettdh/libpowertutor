#include "libpowertutor.h"

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

// in seconds
static const int MOBILE_FACH_INACTIVITY_TIMER = 6;
static const int MOBILE_DCH_INACTIVITY_TIMER = 4;

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

static inline int
calculate_energy(size_t datalen, size_t bandwidth, int power)
{
    // rough estimation of how long the radio will remain active
    //   when sending this chunk of data.
    // This formulation is naive and may or may not work.
    assert(bandwidth > 0);
    double duration = ((double)datalen) / bandwidth;
    int energy = duration * power;
    return energy;
}

static inline int 
estimate_mobile_energy_cost(bool downlink, size_t datalen, size_t bandwidth)
{
    int power = 0;
    MobileState mobile_state = get_mobile_state();
    MobileState new_state = MOBILE_POWER_STATE_FACH;
    
    int queue_len = get_queue_len(downlink);
    int threshold = get_dch_threshold(downlink);
    if ((queue_len + datalen) >= threshold) {
        new_state = MOBILE_POWER_STATE_DCH;
    }
    
    int fach_energy = 0, dch_energy = 0;
    if (mobile_state == MOBILE_POWER_STATE_IDLE) {
        if (new_state == MOBILE_POWER_STATE_DCH) {
            // FACH inactivity timeout only; data transferred in DCH
            fach_energy = MOBILE_FACH_POWER * MOBILE_FACH_INACTIVITY_TIMER;
            
            double duration = ((double)datalen) / bandwidth;
            duration += MOBILE_DCH_INACTIVITY_TIMER;
            dch_energy = MOBILE_DCH_POWER * duration;
        } else {
            // cost of transferring data in FACH state + FACH timeout
            double duration = ((double) datalen) / bandwidth;
            duration += MOBILE_FACH_INACTIVITY_TIMER;
            fach_energy = MOBILE_FACH_POWER * duration;
        }
    } else if (mobile_state == MOBILE_POWER_STATE_FACH) {
        if (new_state == MOBILE_POWER_STATE_DCH) {
            // DCH + extending FACH time by postponing inactivity timeout
            // TODO
        } else {
            // extending FACH time
            // TODO
        }
    } else {
        assert(mobile_state == MOBILE_POWER_STATE_DCH);
        // extending DCH time by postponing inactivity timeout
        // TODO
    }
    // XXX: the above can probably be simplified by calculating
    // XXX:  fach_ and dch_ as if mobile_state was IDLE, then subtracting
    // XXX:  to account for the cases where it's not, in which cases
    // XXX:  we only pay a portion of the FACH/DCH energy with this
    // XXX:  transfer, having paid the full amount in previous transfers.
    
    // account for possible floating-point math errors
    return max(0, fach_energy) + max(0, dch_energy);
}

static inline int 
wifi_channel_rate_component()
{
    return (48 - 0.768 * wifi_channel_rate()) * wifi_data_rate();
}

static inline int 
estimate_wifi_energy_cost(bool downlink, size_t datalen, size_t bandwidth)
{
    int power = 0;
    
    // The wifi radio is only in the transmit state for a very short time,
    //  and that power consumption is factored into the high/low states'
    //  power calculation.
    if (wifi_high_state) {
        power = WIFI_HIGH_POWER_BASE + wifi_channel_rate_component();
    } else {
        power = WIFI_LOW_POWER;
    }
    
    return calculate_energy(datalen, bandwidth, power);
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
        return estimate_mobile_power_cost(downlink, datalen, bandwidth);
    } else if (type == TYPE_WIFI) {
        return estimate_wifi_power_cost(downlink, datalen, bandwidth);
    } else assert(false);
}

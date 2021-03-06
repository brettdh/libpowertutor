#ifndef _POWER_MODEL_H_
#define _POWER_MODEL_H_

#include "libpowertutor.h"

#include <map>

enum PowerModelType {
    HTC_DREAM, NEXUS_ONE,

    NUM_POWER_MODELS
};

struct PowerModel {
    static PowerModel *get(PowerModelType type);

    // all power vals in mW
    //static const int WIFI_TRANSMIT_POWER = 1000;
    int WIFI_HIGH_POWER_BASE;
    int WIFI_LOW_POWER;

    // XXX: These are for 3G only; careful when testing!
    int MOBILE_IDLE_POWER;
    int MOBILE_FACH_POWER;
    int MOBILE_DCH_POWER;

    int power_coeff(MobileState state) {
        return power_coeffs[state];
    }

    // get the power coefficient for the current cpu frequency.
    double cpu_power_coeff(int freq);
    
    // return the estimated energy 
    double estimate_pegged_cpu_energy(double seconds) {
        // power coefficient (mW) = frequency coefficient * utilization (100).
        // energy = power * time
        return (max_cpu_freq_coeff * 100) * seconds;
    }

    int min_cpu_freq;

    // byte queue thresholds for transition to DCH state
    int MOBILE_DCH_THRESHOLD_DOWN;
    int MOBILE_DCH_THRESHOLD_UP;

    // in seconds
    double MOBILE_FACH_INACTIVITY_TIMER;
    double MOBILE_DCH_INACTIVITY_TIMER;

    // threshold for transitioning to wifi high-power state
    int WIFI_PACKET_RATE_THRESHOLD; // packets/sec

    int get_dch_threshold(bool downlink) {
        return downlink ? MOBILE_DCH_THRESHOLD_DOWN : MOBILE_DCH_THRESHOLD_UP;
    }

    virtual const char *wifi_iface() = 0;

    virtual ~PowerModel() {}

protected:
    PowerModel() {}
    virtual void init() = 0;

    int power_coeffs[MOBILE_POWER_STATE_DCH + 1];
    void set_power_state_coeffs(int idle_mw, int fach_mw, int dch_mw) {
        MOBILE_IDLE_POWER = idle_mw;
        MOBILE_FACH_POWER = fach_mw;
        MOBILE_DCH_POWER = dch_mw;
        power_coeffs[MOBILE_POWER_STATE_IDLE] = MOBILE_IDLE_POWER;
        power_coeffs[MOBILE_POWER_STATE_FACH] = MOBILE_FACH_POWER;
        power_coeffs[MOBILE_POWER_STATE_DCH] = MOBILE_DCH_POWER;
    }

    std::map<int, double> cpu_freq_power_coeffs;
    double max_cpu_freq_coeff;

private:
    static PowerModel *powerModels[NUM_POWER_MODELS];
    static void initArray();

    class static_initer {
    public:
        static_initer();
        ~static_initer();
    };
    static static_initer initer;
};

#endif /* _POWER_MODEL_H_ */

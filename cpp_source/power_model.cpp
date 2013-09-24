#include <assert.h>
#include "power_model.h"

#include <stdlib.h>

PowerModel * PowerModel::powerModels[NUM_POWER_MODELS];
PowerModel::static_initer PowerModel::initer;

// PowerTutor [1] power model for the HTC Dream.
// [1] Zhang et al. "Accurate Online Power Estimation and Automatic Battery
// Behavior Based Power Model Generation for Smartphones," CODES+ISSS '10.
struct HTCDreamPowerModel : public PowerModel {
    virtual void init() {
        WIFI_HIGH_POWER_BASE = 710;
        WIFI_LOW_POWER = 20;

        set_power_state_coeffs(10, 401, 570);

        MOBILE_DCH_THRESHOLD_DOWN = 119;
        MOBILE_DCH_THRESHOLD_UP = 151;

        MOBILE_FACH_INACTIVITY_TIMER = 6.0;
        MOBILE_DCH_INACTIVITY_TIMER = 4.0;
    
        WIFI_PACKET_RATE_THRESHOLD = 15; // packets/sec

        // freqs in kHz and rounded down.
        cpu_freq_power_coeffs.clear();
        cpu_freq_power_coeffs[245] = 3.4169;
        cpu_freq_power_coeffs[383] = 4.3388;
        min_cpu_freq = 245;
        max_cpu_freq_coeff = 4.3388;
    }

    virtual const char *wifi_iface() {
        static const char *iface = "tiwlan0";
        return iface;
    }
};

// PowerTutor model for the Nexus One.
// Values pulled from PowerTutor's log report file,
//  since its values differ somewhat from the published numbers.
struct NexusOnePowerModel : public PowerModel {
    virtual void init() {
        WIFI_HIGH_POWER_BASE = 405;
        WIFI_LOW_POWER = 34;

        set_power_state_coeffs(10, 406, 902);

        MOBILE_DCH_THRESHOLD_DOWN = 119;
        MOBILE_DCH_THRESHOLD_UP = 151;

        MOBILE_FACH_INACTIVITY_TIMER = 6.0;
        MOBILE_DCH_INACTIVITY_TIMER = 4.0;
    
        WIFI_PACKET_RATE_THRESHOLD = 15; // packets/sec

        // freqs in kHz.
        cpu_freq_power_coeffs.clear();
        cpu_freq_power_coeffs[245] = 1.1273;
        cpu_freq_power_coeffs[384] = 1.5907;
        cpu_freq_power_coeffs[460] = 1.8736;
        cpu_freq_power_coeffs[499] = 2.1745;
        cpu_freq_power_coeffs[576] = 2.6031;
        cpu_freq_power_coeffs[614] = 2.9612;
        cpu_freq_power_coeffs[653] = 3.1373;
        cpu_freq_power_coeffs[691] = 3.4513;
        cpu_freq_power_coeffs[768] = 3.9073;
        cpu_freq_power_coeffs[806] = 4.1959;
        cpu_freq_power_coeffs[845] = 4.6492;
        cpu_freq_power_coeffs[998] = 5.4818;

        min_cpu_freq = 245;
        max_cpu_freq_coeff = 5.4818;
    }

    virtual const char *wifi_iface() {
        static const char *iface = "eth0";
        return iface;
    }
};

PowerModel::static_initer::static_initer()
{
    for (int i = 0; i < NUM_POWER_MODELS; ++i) {
        powerModels[i] = NULL;
    }
}

void
PowerModel::initArray()
{
    if (powerModels[0] == NULL) {
        powerModels[HTC_DREAM] = new HTCDreamPowerModel();
        powerModels[NEXUS_ONE] = new NexusOnePowerModel();
        for (int i = 0; i < NUM_POWER_MODELS; ++i) {
            powerModels[i]->init();
        }
    }
}

PowerModel::static_initer::~static_initer()
{
    for (int i = 0; i < NUM_POWER_MODELS; ++i) {
        delete powerModels[i];
        powerModels[i] = NULL;
    }
}

PowerModel *
PowerModel::get(PowerModelType type)
{
    initArray();
    assert(type >= 0 && type < NUM_POWER_MODELS);
    return powerModels[type];
}

// get the power coefficient for the current cpu frequency.
// as per PowerTutor source, linear interpolation between
// nearest two frequencies' coefficients.
double 
PowerModel::cpu_power_coeff(int freq)
{
    assert(!cpu_freq_power_coeffs.empty());
    auto it = cpu_freq_power_coeffs.find(freq);
    if (it != cpu_freq_power_coeffs.end()) {
        return it->second;
    }
    
    it = cpu_freq_power_coeffs.upper_bound(freq);
    if (it == cpu_freq_power_coeffs.end()) {
        it = cpu_freq_power_coeffs.lower_bound(freq);
        assert(it != cpu_freq_power_coeffs.end());
        return it->second;
    } else if (it == cpu_freq_power_coeffs.begin()) {
        return it->second;
    }
    
    // 
    double upper_freq = it->first;
    double upper_coeff = it->second;
    --it;
    double lower_freq = it->first;
    double lower_coeff = it->second;
    
    double slope = (upper_coeff - lower_coeff) / (upper_freq - lower_freq);
    return lower_coeff + ((freq - lower_freq) * slope);
}

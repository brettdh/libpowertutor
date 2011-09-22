#include <assert.h>
#include "power_model.h"

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

// simple JNI wrappers.

#include <jni.h>
#include "libpowertutor.h"

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jint JNICALL
Java_edu_umich_libpowertutor_EnergyEstimates_estimateMobileEnergyCost
(JNIEnv *jenv, jclass jclass, jint datalen, jint bandwidth, jint rtt_ms)
{
    return estimate_mobile_energy_cost(datalen, bandwidth, rtt_ms);
}

JNIEXPORT jint JNICALL
Java_edu_umich_libpowertutor_EnergyEstimates_estimateMobileEnergyCostAverage
(JNIEnv *jenv, jclass jclass, jint datalen, jint avg_bandwidth, jint avg_rtt_ms)
{
    return estimate_mobile_energy_cost_average(datalen, avg_bandwidth, avg_rtt_ms);
}

JNIEXPORT jint JNICALL
Java_edu_umich_libpowertutor_EnergyEstimates_estimateWifiEnergyCost
(JNIEnv *jenv, jclass jclass, jint datalen, jint bandwidth, jint rtt_ms)
{
    return estimate_wifi_energy_cost(datalen, bandwidth, rtt_ms);
}

JNIEXPORT jint JNICALL
Java_edu_umich_libpowertutor_EnergyEstimates_energyConsumedSinceReset
(JNIEnv *jenv, jclass jclass)
{
    return energy_consumed_since_reset();
}

JNIEXPORT jint JNICALL
Java_edu_umich_libpowertutor_EnergyEstimates_averagePowerConsumptionSinceReset
(JNIEnv *jenv, jclass jclass)
{
    return average_power_consumption_since_reset();
}

#ifdef __cplusplus
}
#endif

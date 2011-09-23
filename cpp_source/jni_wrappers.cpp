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
Java_edu_umich_libpowertutor_EnergyEstimates_estimateMobileEnergyCostFromIdle
(JNIEnv *jenv, jclass jclass, jint datalen, jint bandwidth, jint rtt_ms)
{
    return estimate_mobile_energy_cost_from_idle(datalen, bandwidth, rtt_ms);
}

JNIEXPORT jint JNICALL
Java_edu_umich_libpowertutor_EnergyEstimates_estimateWifiEnergyCost
(JNIEnv *jenv, jclass jclass, jint datalen, jint bandwidth, jint rtt_ms)
{
    return estimate_wifi_energy_cost(datalen, bandwidth, rtt_ms);
}

#ifdef __cplusplus
}
#endif

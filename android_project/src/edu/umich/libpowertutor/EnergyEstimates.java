package edu.umich.libpowertutor;

public class EnergyEstimates {
    public static native int estimateMobileEnergyCost(int datalen, int bandwidth, int rtt_ms);
    public static native int estimateMobileEnergyCostAverage(int datalen, int bandwidth, int rtt_ms);
    public static native int estimateWifiEnergyCost(int datalen, int bandwidth, int rtt_ms);
    
    // TODO: rename, since there's no longer a 'reset' operation.
    static native int energyConsumedSinceReset();
    static native int averagePowerConsumptionSinceReset();
    
    static {
        System.loadLibrary("powertutor");
    }
}

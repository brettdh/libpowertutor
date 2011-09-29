package edu.umich.libpowertutor;

public class EnergyEstimates {
    public static native int estimateMobileEnergyCost(int datalen, int bandwidth, int rtt_ms);
    public static native int estimateMobileEnergyCostAverage(int datalen, int bandwidth, int rtt_ms);
    public static native int estimateWifiEnergyCost(int datalen, int bandwidth, int rtt_ms);
    
    public static native int energyConsumedSinceReset();
    public static native int averagePowerConsumptionSinceReset();
    public static native void resetStats();
    
    static {
        System.loadLibrary("powertutor");
    }
}

package edu.umich.libpowertutor;

public class EnergyEstimates {
    public static native int estimateMobileEnergyCost(int datalen, int bandwidth, int rtt_ms);
    public static native int estimateMobileEnergyCostFromIdle(int datalen, int bandwidth, int rtt_ms);
    public static native int estimateWifiEnergyCost(int datalen, int bandwidth, int rtt_ms);
    
    static {
        System.loadLibrary("powertutor");
    }
}

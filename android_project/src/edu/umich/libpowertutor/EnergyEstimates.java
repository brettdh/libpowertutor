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

    public static double convertBatteryPercentToJoules(double energyBudgetBatteryPercent) {
        // TODO: sanity-check.
        double charge = 1400.0 * energyBudgetBatteryPercent / 100.0; // mAh
        double energy = charge * 4.0; // average voltage 4V;  mAh*V, or mWh
        energy *= 3600; // mWs or mJ
        return energy / 1000.0; // mJ to J
    }
}

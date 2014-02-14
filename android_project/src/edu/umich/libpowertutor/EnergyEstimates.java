package edu.umich.libpowertutor;

public class EnergyEstimates {
    public static native int estimateMobileEnergyCost(int datalen, int bandwidth, int rtt_ms);
    public static native int estimateMobileEnergyCostAverage(int datalen, int bandwidth, int rtt_ms);
    public static native int estimateWifiEnergyCost(int datalen, int bandwidth, int rtt_ms);
    
    // TODO: rename, since there's no longer a 'reset' operation.
    static int energyConsumedSinceReset() {
        return energyConsumedSinceReset(EnergyComponent.ALL_ENERGY_COMPONENTS);
    }
    static int energyConsumedSinceReset(EnergyComponent component) {
        return energyConsumedSinceReset(component.ordinal());
    }
    static int averagePowerConsumptionSinceReset() {
        return averagePowerConsumptionSinceReset(EnergyComponent.ALL_ENERGY_COMPONENTS);
    }
    static int averagePowerConsumptionSinceReset(EnergyComponent component) {
        return averagePowerConsumptionSinceReset(component.ordinal());
    }
    static native int energyConsumedSinceReset(int component);
    static native int averagePowerConsumptionSinceReset(int component);
    
    static {
        System.loadLibrary("powertutor");
    }

    public static double convertBatteryPercentToJoules(double energyBudgetBatteryPercent) {
        // TODO: sanity-check.
        double charge = 1400.0 * energyBudgetBatteryPercent / 100.0; // mAh
        double energy = charge * 3.7; // average voltage 3.7V for Li-Ion battery;  mAh*V, or mWh
        energy *= 3600; // mWs or mJ
        return energy / 1000.0; // mJ to J
    }
}

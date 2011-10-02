package edu.umich.libpowertutor;

import java.util.Date;

public class EnergyUsage {
    private int energyUsageAtStart;
    private Date start;
    
    public EnergyUsage() {
        reset();
    }
    
    public void reset() {
        start = new Date();
        energyUsageAtStart = EnergyEstimates.energyConsumedSinceReset();
    }
    
    public int energyConsumed() {
        int energyUsedNow = EnergyEstimates.energyConsumedSinceReset();
        assert(energyUsedNow >= energyUsageAtStart);
        return energyUsedNow - energyUsageAtStart;
    }
}

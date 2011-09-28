package edu.umich.libpowertutor;

import android.test.InstrumentationTestCase;

public class LibPowerTutorTest extends InstrumentationTestCase {
    @Override
    protected void setUp() {
        try {
            Thread.sleep(3000);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }
    
    public void testSmoke() {
        double cost = EnergyEstimates.estimateMobileEnergyCost(500000, 10000, 200);
        assertTrue(cost > 0.0);
        cost = EnergyEstimates.estimateMobileEnergyCostFromIdle(500000, 10000, 200);
        assertTrue(cost > 0.0);
        cost = EnergyEstimates.estimateWifiEnergyCost(500000, 10000, 200);
        assertTrue(cost > 0.0);
    }
    
    public void testEnergyEstimatesAreSane() throws InterruptedException {
        Thread.sleep(3000);
        int energyConsumed = EnergyEstimates.energyConsumedSinceReset();
        assertTrue(energyConsumed > 0);
        Thread.sleep(3000);
        int energyConsumedLater = EnergyEstimates.energyConsumedSinceReset();
        assertTrue(energyConsumedLater > energyConsumed);
        
        int power = EnergyEstimates.averagePowerConsumptionSinceReset();
        assertTrue(power > 0);
        
        EnergyEstimates.resetStats();
        energyConsumedLater = EnergyEstimates.energyConsumedSinceReset();
        assertTrue(energyConsumedLater < energyConsumed);
    }
    
    static {
        System.loadLibrary("powertutor");
    }
}

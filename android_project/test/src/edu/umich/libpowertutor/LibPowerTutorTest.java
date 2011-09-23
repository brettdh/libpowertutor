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
    
    static {
        System.loadLibrary("powertutor");
    }
}

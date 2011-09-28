package edu.umich.libpowertutor;

import java.io.IOException;
import java.io.InputStream;
import java.net.Socket;
import java.net.UnknownHostException;

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
        EnergyEstimates.resetStats();
        
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
    
    public void testEnergyConsumptionTracked() throws InterruptedException, IOException {
        EnergyEstimates.resetStats();
        
        Thread.sleep(3000);
        int energyStart = EnergyEstimates.energyConsumedSinceReset();
        assertTrue(energyStart > 0);
        
        Socket socket = new Socket("141.212.110.132", 4321);
        InputStream in = socket.getInputStream();
        byte[] buf = new byte[4096];
        long begin_ms = System.currentTimeMillis();
        while (in.read(buf) > 0 && 
               (System.currentTimeMillis() - begin_ms) < 3000) {
            // nothing; read lots of bytes
        }
        in.close();
        socket.close();
        int transferEnergy = EnergyEstimates.energyConsumedSinceReset() - energyStart;
        assertTrue(transferEnergy > 1000);
        
        int transferPower = EnergyEstimates.averagePowerConsumptionSinceReset();
        assertTrue(transferPower > 300);
    }
    
    static {
        System.loadLibrary("powertutor");
    }
}

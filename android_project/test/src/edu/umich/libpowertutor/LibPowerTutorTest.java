package edu.umich.libpowertutor;

import java.io.IOException;
import java.io.InputStream;
import java.net.Socket;
import java.net.UnknownHostException;

import android.test.InstrumentationTestCase;
import android.util.Log;

public class LibPowerTutorTest extends InstrumentationTestCase {
    private static final String SPEEDTEST_SERVER_IP = "141.212.113.120";
    private static final String TAG = LibPowerTutorTest.class.getName();

    @Override
    protected void setUp() {
        try {
            // force the library to be loaded
            EnergyEstimates.estimateMobileEnergyCost(1, 1, 1);
            
            Thread.sleep(12000);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }
    
    public void testSmoke() throws InterruptedException {
        double cost = EnergyEstimates.estimateMobileEnergyCost(500000, 10000, 200);
        assertTrue(cost > 0.0);
        cost = EnergyEstimates.estimateMobileEnergyCostAverage(500000, 10000, 200);
        assertTrue(cost > 0.0);
        cost = EnergyEstimates.estimateWifiEnergyCost(500000, 10000, 200);
        assertTrue(cost > 0.0);
    }
    
    public void testEnergyEstimatesAreSane() throws InterruptedException {
        EnergyUsage usage = new EnergyUsage();
        
        Thread.sleep(3000);
        int energyConsumed = usage.energyConsumed();
        assertTrue(energyConsumed > 0);
        Thread.sleep(3000);
        int energyConsumedLater = usage.energyConsumed();
        assertTrue(energyConsumedLater > energyConsumed);
        
        int power = EnergyEstimates.averagePowerConsumptionSinceReset();
        assertTrue(power > 0);
        
        usage.reset();
        energyConsumedLater = usage.energyConsumed();
        assertTrue(energyConsumedLater < energyConsumed);
    }
    
    public void testEnergyConsumptionTracked() throws InterruptedException, IOException {
        Thread.sleep(3000);
        int energyStart = EnergyEstimates.energyConsumedSinceReset();
        assertTrue(energyStart > 0);
        
        downloadBytesForDuration(3000);
        int transferEnergy = EnergyEstimates.energyConsumedSinceReset() - energyStart;
        assertTrue(transferEnergy > 1000); // XXX: this depends on the data being sent on 3G.
        
        int transferPower = EnergyEstimates.averagePowerConsumptionSinceReset();
        assertTrue(transferPower > 300);
    }

    private void downloadBytesForDuration(int durationMillis) throws UnknownHostException,
            IOException {
        Socket socket = new Socket(SPEEDTEST_SERVER_IP, 4321);
        InputStream in = socket.getInputStream();
        byte[] buf = new byte[4096];
        long begin_ms = System.currentTimeMillis();
        while (in.read(buf) > 0 && 
               (System.currentTimeMillis() - begin_ms) < durationMillis) {
            // nothing; read lots of bytes
        }
        in.close();
        socket.close();
    }
    
    public void testAverageEnergyEstimate() throws InterruptedException, IOException {
        int energy, avgEnergy;
        
        energy = EnergyEstimates.estimateMobileEnergyCost(1, 1000, 1);
        avgEnergy = EnergyEstimates.estimateMobileEnergyCostAverage(1, 1000, 1);
        Log.d(TAG, String.format("energy: %d  avgEnergy: %d", energy, avgEnergy));
        assertEquals(energy, avgEnergy, 30);
        
        Thread.sleep(5000);
        downloadBytes(4096);
        
        Thread.sleep(1000);
        
        // avg should still reflect IDLE state
        avgEnergy = EnergyEstimates.estimateMobileEnergyCostAverage(1, 1000, 1);
        assertEquals(energy, avgEnergy, 500);
        
        // current energy estimate should not contain tail time,
        //  so it will be much less
        energy = EnergyEstimates.estimateMobileEnergyCost(1, 1000, 1);
        assertTrue(avgEnergy > energy); // XXX: this depends on the data being sent on 3G.
        assertTrue((avgEnergy - energy) > 1000);
        
        downloadBytesForDuration(8000);
        
        int oldAvgEnergy = avgEnergy;
        avgEnergy = EnergyEstimates.estimateMobileEnergyCostAverage(1, 1000, 1);
        assertTrue(avgEnergy < oldAvgEnergy);
        assertTrue((oldAvgEnergy - avgEnergy) > 200);
        
        Thread.sleep(25000); // avg energy should go up with more idle time
        oldAvgEnergy = avgEnergy;
        avgEnergy = EnergyEstimates.estimateMobileEnergyCostAverage(1, 1000, 1);
        assertTrue(avgEnergy > oldAvgEnergy);
    }
    
    public void testLogPowerConsumptionOverTime() throws IOException, InterruptedException {
        logMobileEnergyEstimate(10, 1000, 10, false);
        logMobileEnergyEstimate(100, 1000, 10, false);
        logMobileEnergyEstimate(1000, 1000, 10, false);
        
        logMobileEnergyEstimate(10, 1000, 10, true);
        logMobileEnergyEstimate(100, 1000, 10, true);
        logMobileEnergyEstimate(1000, 1000, 10, true);
        
        downloadBytes(4096);
        
        Thread.sleep(2000);
        
        logMobileEnergyEstimate(10, 1000, 10, false);
        logMobileEnergyEstimate(100, 1000, 10, false);
        logMobileEnergyEstimate(1000, 1000, 10, false);
        
        logMobileEnergyEstimate(10, 1000, 10, true);
        logMobileEnergyEstimate(100, 1000, 10, true);
        logMobileEnergyEstimate(1000, 1000, 10, true);
    }
    
    public void testIsolatedEnergyUsageTracking() throws IOException, InterruptedException {
        EnergyUsage usage1 = new EnergyUsage();
        EnergyUsage usage2 = new EnergyUsage();
        
        downloadBytes(4096);
        Thread.sleep(2000);

        assertTrue(usage1.energyConsumed() > 0);
        assertTrue(usage2.energyConsumed() > 0);
        
        usage2.reset();
        assertTrue(usage2.energyConsumed() < usage1.energyConsumed());
    }

    private void downloadBytes(int bytes) throws UnknownHostException, IOException {
        Socket socket = new Socket(SPEEDTEST_SERVER_IP, 4321);
        InputStream in = socket.getInputStream();
        byte[] buf = new byte[bytes];
        in.read(buf);
        in.close();
        socket.close();
    }
    
    private void logMobileEnergyEstimate(int datalen, int bandwidth, int rtt_ms, boolean average) {
        int energy = 0;
        if (average) {
            energy = EnergyEstimates.estimateMobileEnergyCost(datalen, bandwidth, rtt_ms);
        } else {
            energy = EnergyEstimates.estimateMobileEnergyCostAverage(datalen, bandwidth, rtt_ms);
        }
        Log.d(TAG, String.format("%s energy estimate (datalen %d bw %d rtt_ms %d): %d mJ",
                                 average ? "average" : "spot", datalen, bandwidth, rtt_ms, energy));
    }

    static {
        System.loadLibrary("powertutor");
    }
}

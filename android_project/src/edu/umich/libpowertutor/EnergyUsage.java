package edu.umich.libpowertutor;

import java.util.Date;

public class EnergyUsage {
    private int[] energyUsageAtStart = new int[EnergyComponent.values().length];
    private Date start;
    
    public EnergyUsage() {
        reset();
    }
    
    public void reset() {
        start = new Date();
        getEnergyConsumedAllComponents(energyUsageAtStart);
    }

    private void getEnergyConsumedAllComponents(int[] components) {
        for (EnergyComponent component : EnergyComponent.values()) {
            components[component.ordinal()] = EnergyEstimates.energyConsumedSinceReset(component);
        }
    }
    
    public int energyConsumed() {
        return energyConsumed(EnergyComponent.ALL_ENERGY_COMPONENTS);
    }
    
    public int energyConsumed(EnergyComponent component) {
        int index = component.ordinal();
        int energyUsedNow = EnergyEstimates.energyConsumedSinceReset(component);
        assert(energyUsedNow >= energyUsageAtStart[index]);
        return energyUsedNow - energyUsageAtStart[index];
    }
    
    public int[] energyConsumedAllComponents() {
        int[] energyConsumed = new int[EnergyComponent.values().length];
        getEnergyConsumedAllComponents(energyConsumed);
        return energyConsumed;
    }
    
    public static String asString(int[] components) {
        StringBuffer s = new StringBuffer();
        for (EnergyComponent component : EnergyComponent.values()) {
            int index = component.ordinal();
            s.append(component.name()).append(": ")
             .append(components[index]).append(" ");
        }
        return s.toString();
    }
}

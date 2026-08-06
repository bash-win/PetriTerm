#pragma once

namespace petriterm::organisms {

/// Immutable per-species tuning. Climate tolerances define the survivable band
/// around each ideal; energy fields drive feeding, metabolism, and reproduction;
/// movement and dispersal govern spatial behavior. Tolerance ranges default to a
/// nonzero value so environmental fitness never divides by zero.
struct TraitProfile {
    double idealTemperatureCelsius = 0.0;
    double temperatureToleranceRange = 1.0;
    double idealHumidityPercent = 0.0;
    double humidityToleranceRange = 1.0;
    double energyGainedPerFeeding = 0.0;
    double energyConsumedPerTick = 0.0;
    double energyRequiredToReproduce = 0.0;
    double reproductionEnergyCost = 0.0;
    int reproductionCooldownTicks = 0;
    int maximumAgeInTicks = 0;
    int movementRangeInTiles = 0;
    double seedDispersalProbabilityPerTick = 0.0;
};

}

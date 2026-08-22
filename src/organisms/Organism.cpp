#include "petriterm/organisms/Organism.hpp"

#include "petriterm/organisms/Species.hpp"

namespace petriterm::organisms {

namespace {

/// Fraction of a species' reproduction threshold a newly created organism starts
/// with: enough to survive and act for a while, but not to reproduce immediately.
constexpr double kStartingEnergyFractionOfReproduce = 0.5;

}

Organism::Organism(const Species* species, int tileColumnIndex, int tileRowIndex)
    : species(species),
      tileColumnIndex(tileColumnIndex),
      tileRowIndex(tileRowIndex),
      remainingEnergyUnits(species->traits.energyRequiredToReproduce *
                           kStartingEnergyFractionOfReproduce),
      ticksUntilCanReproduce(species->traits.reproductionCooldownTicks) {}

void Organism::applyMetabolismAndAgingForOneTick() {
    remainingEnergyUnits -= species->traits.energyConsumedPerTick;
    ++ageInTicks;
    if (ticksUntilCanReproduce > 0) {
        --ticksUntilCanReproduce;
    }
    if (remainingEnergyUnits <= 0.0 || ageInTicks > species->traits.maximumAgeInTicks) {
        isAlive = false;
    }
}

bool Organism::isReadyToReproduce() const {
    return isAlive && ticksUntilCanReproduce <= 0 &&
           remainingEnergyUnits >= species->traits.energyRequiredToReproduce;
}

void Organism::payReproductionCostAndResetCooldown() {
    remainingEnergyUnits -= species->traits.reproductionEnergyCost;
    ticksUntilCanReproduce = species->traits.reproductionCooldownTicks;
}

}

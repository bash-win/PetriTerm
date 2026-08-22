#pragma once

namespace petriterm::organisms {

struct Species;

/// A living instance of a species at a tile position, with the mutable state the
/// simulation advances each tick. Non-copyable: each organism is owned by its
/// tile via a unique_ptr and referenced elsewhere by raw pointer.
struct Organism {
    const Species* species;
    int tileColumnIndex;
    int tileRowIndex;
    double remainingEnergyUnits;
    int ageInTicks = 0;
    int ticksUntilCanReproduce;
    bool isAlive = true;

    /// Constructs an organism of the given species at the given tile, seeded with
    /// the species' starting energy and reproduction cooldown.
    Organism(const Species* species, int tileColumnIndex, int tileRowIndex);

    Organism(const Organism&) = delete;
    Organism& operator=(const Organism&) = delete;

    /// Decrements energy by the species' per-tick consumption, ages by one tick,
    /// counts down the reproduction cooldown, and sets isAlive to false if energy
    /// is depleted or the maximum age is exceeded.
    void applyMetabolismAndAgingForOneTick();

    /// Returns true if the organism is alive, off reproduction cooldown, and has
    /// at least the energy its species needs to reproduce.
    bool isReadyToReproduce() const;

    /// Charges the species' reproduction energy cost and restarts the cooldown.
    /// Called on the parent once an offspring has been placed, so paying the cost
    /// and going back on cooldown can never drift apart.
    void payReproductionCostAndResetCooldown();
};

}

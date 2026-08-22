# PetriTerm

A terminal ecosystem simulation game in C++20 and ncurses. The player seeds a
procedurally generated world with species and keeps a food web alive against
weather, seasons, and their own eco-credit budget.

## Read this first

**[ROADMAP.md](ROADMAP.md) is the plan of record.** It lists every remaining
pull request, in dependency order, with the design decisions already settled for
each. Start there rather than re-deriving what to build next.

## Build, test, run

```sh
cmake -S . -B build            # Debug by default
cmake --build build -j
./build/petriterm_tests        # Catch2, or: ctest --test-dir build
./build/petriterm             # needs a terminal at least 80x24
```

CI runs `clang-format --dry-run --Werror`, `clang-tidy -p build`, and gcc + clang
builds. Run those locally before pushing; `-Wall -Wextra -Wpedantic -Werror` is
on, so a warning is a build failure.

## Layout

`include/petriterm/<layer>/` headers and `src/<layer>/` sources, one type per
file pair, mirrored by `tests/test_*.cpp`.

- **`engine/`** — terminal, rendering, input, scenes, RNG, timing. Ecology-
  agnostic and reusable; ncurses appears nowhere outside it.
- **`world/`** — noise, biomes, tiles, world generation, climate.
- **`organisms/`** — categories, traits, species, the species registry, and the
  living `Organism`.
- **`simulation/`** — per-tick ecology. Does not exist yet; see ROADMAP PR 01.
- **`game/`** — player-facing glue: viewport, placement, and (later) HUD,
  scenarios, tools.

`data/species.txt` holds the species definitions, parsed at startup and copied
next to the binary by a post-build step.

## Conventions

These are consistently followed across the merged history — match them.

- **Names are spelled out.** `remainingEnergyUnits`, `tileColumnIndex`,
  `advanceWeatherAndApplyToWorld`. No abbreviations, and units in the name where
  a bare number would be ambiguous (`...Celsius`, `...InTiles`, `...Ticks`).
- **Doc comments explain why, not what.** Every public type and function gets
  one, and it earns its place by recording the reasoning a reader could not
  recover from the signature — see `SimulationClock.hpp` for the standard.
- **Testability is a design constraint.** Anything needing a live terminal is
  split so the pure logic can be tested without one: `decodeRawKeyRead` is a free
  function for exactly this reason, and `SimulationClock` exists separately from
  `GameLoop` because `GameLoop` sleeps and cannot be unit tested.
- **Determinism is a hard requirement.** One seeded `RandomNumberGenerator` is
  threaded by reference through everything that needs randomness, so a seed
  reproduces an entire run. Playback speed must never change what a tick
  computes. Preserve this — save/load and the balance tests both depend on it.
- **Tiles own their organisms** via `unique_ptr`; everything else refers to them
  by raw pointer. `Species` pointers from the registry are stable for the
  program's lifetime.
- One milestone per branch, one branch per PR, merged into `main`.

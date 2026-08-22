# Roadmap

My plan from here to a finished game. One branch and one PR per numbered item,
each sized to review on its own.

## Status

Merged through PR #14. Engine (terminal, palette, renderer, text measure, input,
grid, RNG, scenes, clock, loop), world (noise, biomes, tiles, generation,
climate), organisms (categories, traits, species, registry, `Organism`), game
(viewport, placement). 102 tests. CI runs clang-format, clang-tidy, gcc, clang.

## What's actually broken or missing

- Nothing simulates. `Organism::applyMetabolismAndAgingForOneTick()` is called
  only by its own tests. `WorldViewScene::update()` advances weather and nothing
  else, so organisms never eat, move, breed, or die.
- Eco-credits can only be spent, never earned. No objectives, no failure state.
- Resize is decoded and dropped. `InputManager` maps `KEY_RESIZE` to
  `KeyCode::Resize`, but nothing handles it, `resizeterm()` is never called, and
  `Viewport` plus the help-bar row are fixed at construction. Any resize —
  including opening a tmux split — corrupts the layout.
- `panelw` is a `REQUIRED` dependency in `CMakeLists.txt:17` and the code never
  calls it. Glyphs are Unicode-only with no ASCII fallback. The monochrome path
  in `ColorPalette` is unexercised. CI is Linux-only.

## Checklist before opening any PR

- clang-format and clang-tidy clean; no warnings under
  `-Wall -Wextra -Wpedantic -Werror` on gcc and clang.
- Catch2 coverage for new behavior. Split anything needing a live terminal so the
  pure logic tests without one, the way `decodeRawKeyRead` and `SimulationClock`
  already do.
- Doc comments on public types and functions, recording the reasoning rather than
  restating the signature.

---

# Milestone A — Make it simulate

Nothing downstream means much until this lands.

## 01. `simulation-engine`

The central missing piece. `SimulationClock.hpp:63` already names it: the engine
owns the authoritative tick index a save file records, as distinct from the
clock's playback counter.

New: `include/petriterm/simulation/SimulationEngine.hpp`,
`src/simulation/SimulationEngine.cpp`, `tests/test_simulation_engine.cpp`.
Touches: `Organism` (add `payReproductionCostAndResetCooldown()`),
`CMakeLists.txt`, `main.cpp`.

Design I settled on:

- Engine owns the `WorldGrid` and the `ClimateSystem`, borrows the shared RNG.
  The scene stops owning simulation state and renders `engine.world()`. Save/load
  (PR 13) needs the engine to be the sole authority.
- Four tick phases, and the order is the contract:
  1. Climate — re-derive every tile's temperature and humidity so the whole tick
     reads one consistent set of conditions.
  2. Metabolism — upkeep, aging, breeding cooldown. Starvation and old-age deaths
     land here.
  3. Behavior — survivors feed, move, breed.
  4. Cleanup — remove the dead, take the census.
- Phases 2 and 3 each walk a snapshot of raw organism pointers taken at the start
  of the phase. Tiles own organisms via `unique_ptr`, so moving one between tiles
  keeps the pointee's address stable. This is what stops an organism that moves
  mid-phase from acting twice, and stops one born this tick from acting on the
  tick it appeared.
- `environmentalFitness(traits, temperature, humidity)` → [0, 1]: 1.0 at the
  ideal, falling linearly to 0.0 at the tolerance edge, the two axes multiplied so
  being outside either one is fatal alone. Fitness scales feeding yield and gates
  breeding instead of killing directly, so a badly placed organism starves over
  several ticks and stays visible while it happens.
- Plants get grazed, animals get killed. A herbivore takes part of a plant's
  energy and the plant survives unless drained; a carnivore kills outright.
  Without that asymmetry a handful of rabbits strips the map in a few dozen ticks.
- `movementRangeInTiles` is reach, not stride: how far an organism finds and
  strikes at prey. Nothing edible in reach means wander one tile. Document it,
  because the trait name suggests otherwise.
- Cap energy at a multiple of the species' reproduction threshold, or a well-fed
  predator banks unbounded energy and goes immortal between meals.
- Per-tile carrying capacity (`Tile::hasCapacityForCategory`, already written)
  gates both births and moves.
- `TickReport`: living counts by category, total, births, deaths, feedings. The
  HUD and every later statistics feature read this.

Done when: plants plus rabbits grow, plateau at carrying capacity, and crash when
the weather turns. Two engines with the same seed and starting world produce
identical trajectories over 1000 ticks.

## 02. `nutrient-cycle`

Touches `Tile` (add `detritusLevel`), `SimulationEngine`, tests.

Death currently produces nothing. A dead organism deposits detritus proportional
to body mass (use its species' reproduction threshold as the stand-in);
decomposers consume detritus and convert most of it to `soilNutrientLevel`,
respiring the rest; plant photosynthesis scales with `soilNutrientLevel` and
draws it down. Gives decomposers a reason to cost credits.

Done when: a sealed plot with plants and no decomposers loses fertility and stops
supporting plants, and adding decomposers recovers it.

## 03. `simulation-tuning-harness`

New: `--headless --ticks N --seed S` printing the census as CSV without a
terminal; `tests/test_ecosystem_soak.cpp`.

Balancing 18 species by watching a 30fps terminal will not converge. Headless
runs make tuning empirical and turn "stable ecosystem" into a test: seeded runs
asserting a starter ecosystem neither goes extinct nor explodes. Then do the
balance pass on `data/species.txt` and the engine constants.

Done when: the starter scenario survives 5000 ticks across 10 seeds with all five
trophic categories present.

## 04. `active-organism-index`

A full 160x48 scan per phase is 7680 tile visits, and `GameLoop` allows 8 ticks
per frame at 30fps — ~1.8M tile visits a second before any organism does
anything. Replace full-grid scans with an index of tiles that hold organisms,
maintained on birth, move, and death.

Wait for PR 03's soak runs to make the cost measurable. Include a benchmark.

Done when: the fastest speed step holds 30fps at 5000+ organisms, and trajectories
stay bit-identical for a given seed.

---

# Milestone B — Turn it into a game

## 05. `event-log`

Ring buffer of notable events (births, mass deaths, extinctions, weather shifts,
objective progress) with a scrollable panel and severity colors.

Early, because a collapse is currently invisible unless I happen to be watching
the right tile, and every later feature has something to report.

## 06. `eco-credit-economy`

Credits can only be spent. Add income scaled by biodiversity, population
stability, and trophic completeness, so a healthy web funds expansion and a
monoculture stagnates. Show the rate and its inputs in the HUD. This is what
makes placement a decision with consequences instead of a sandbox.

## 07. `inspector-panel`

Select a tile: biome, climate, soil, detritus, and every organism on it with
energy, age, species. Browse the palette with full traits, diet, tolerances,
cost. Eighteen species with a dozen traits each are invisible to the player
right now — this is the difference between "the rabbits died" and "the rabbits
died 4°C below their tolerance band".

## 08. `hud-layout`

Replace the row-by-row debug HUD in `main.cpp` with a composed layout: map pane,
sidebar (status, selection, log), status/help bar, minimap for the 160x48 world.
Lay out from the real terminal size, on top of PR 15's resize handling. Needs 05
and 07 for content.

## 09. `scenarios-and-objectives`

Data-driven scenario definitions, same spirit as `species.txt`: starting seed and
size, starting credits, available palette, objectives, failure conditions.
Objectives like sustaining three trophic levels for 500 ticks, or restoring
fertility to a desert basin. Failure on total extinction or bankruptcy. Win and
lose screens. A free-play scenario with no objectives. Also gives the tutorial
somewhere to live.

## 10. `disasters-and-events`

Drought, wildfire spreading along dry tiles, disease spreading through one
species' population, invasive arrivals. Random in free play, scripted in
scenarios. Telegraph them a few ticks ahead through the event log so they can be
responded to rather than only watched.

## 11. `player-tools`

More verbs than placement, each costing credits with a cooldown: cull, irrigate
or drain, fertilize, quarantine an outbreak, terraform brush to shift a tile's
biome. Plus a tutorial scenario introducing them one at a time. Needs 06
(something to spend), 09 (somewhere to teach), 10 (something to respond to).

## 12. `menus-and-shell`

Title screen, new game (seed, size, scenario), controls screen, pause menu, quit
confirmation. Right now the only exit is `q` and the only world is hard-coded
seed 42.

## 13. `save-and-load`

Serialize world, organisms, climate state, RNG state, scenario, and the engine's
tick index to a versioned file under the XDG data directory. `SimulationClock`'s
comments already anticipate this. Round-trip test: save, load, confirm the
resumed run is bit-identical to the uninterrupted one. Needs PR 01.

## 14. `statistics-and-graphs`

Population history per category as sparklines and a full-screen graph, plus
trophic-pyramid and biodiversity readouts, fed by `TickReport`. Makes
predator-prey oscillation legible instead of inferred.

---

# Milestone C — Run in every terminal

## 15. `resize-and-relayout`

Fix the dropped resize. Call `resizeterm()`, add a relayout hook to `Scene`, give
`Viewport` a `setScreenRegion()`, stop capturing layout constants at
construction, and have `SceneManager` propagate the new size through the stack.
A bug fix, not a feature.

Done when: resizing between 80x24 and full-screen, and crossing the too-small
threshold both ways, always leaves a correct layout.

## 16. `curses-portability`

- Drop the unused `panelw` `REQUIRED` lookup. It is a configure-time hard failure
  for a library nothing calls.
- Fall back across ncursesw, ncurses, and BSD curses; pkg-config before
  `find_package`; no assumption that wide support is a separate lib.
- Make the macOS build work, both system curses and Homebrew ncurses.

Done when: configures and builds on Fedora, Debian/Ubuntu, Alpine (musl), Arch,
and macOS.

## 17. `glyph-themes`

Biome glyphs (`^ ∙ " ≈ *`) and species glyphs (`♣ ♠ ‡ † ∴ ~`) are Unicode-only.
On the Linux console, `TERM=vt100`, or a font missing those code points, the map
is unreadable boxes. Add a glyph theme layer — one Unicode set, one pure ASCII —
picked from locale charset and `TERM`, overridable with `--glyphs=ascii`. Extend
`species.txt` with an optional `asciiGlyph`, defaulting by category.
`TextMeasure` already handles display width; availability is a separate problem.

Done when: fully legible under `LC_ALL=C TERM=linux`.

## 18. `color-modes`

Detect color count and pick a palette: monochrome (emphasis and glyphs only),
8/16, or 256 with distinct per-species hues. Honor `NO_COLOR` and
`--color=auto|never|always|256`. Add a colorblind-safe palette and high contrast,
and stop leaning on color alone to convey trophic level. `ColorPalette` already
degrades when `has_colors()` is false, but nothing exercises that path and the
game is unreadable in it.

Done when: playable at 0, 8, and 256 colors, and with `NO_COLOR=1`.

## 19. `input-robustness`

Handle `SIGTSTP`/`SIGCONT` so Ctrl+Z and `fg` restore the screen — the existing
`SIGINT`/`SIGTERM` handling is good but incomplete. Handle `SIGWINCH` alongside
PR 15. Support Alt/meta chords. Optional mouse (click to select, scroll to pan)
with full keyboard parity so a mouse is never required.

## 20. `terminal-compatibility-ci`

A job driving the game through a pty across `TERM=xterm-256color`, `xterm`,
`screen`, `tmux-256color`, `linux`, and `vt100`, at several sizes and under
`LC_ALL=C`, asserting it starts, renders, takes input, and exits cleanly. Add a
macOS build-and-test job. "Runs in every terminal" is only true if something
checks it every commit.

## 21. `startup-diagnostics`

`--check-terminal` reporting detected size, color count, UTF-8 support, and the
glyph and color modes chosen, with advice when something is off. Turn the
remaining hard failures into clear messages — `initscr` failing and the too-small
notice already read well, so extend the same care to locale and color problems.

---

# Milestone D — Ship it

## 22. `cli-and-config`

`--seed`, `--width`, `--height`, `--scenario`, `--glyphs`, `--color`, `--fps`,
`--save`, `--headless`, `--help`, `--version`. Config file and save/log locations
under the XDG base directories. Seed 42 and 160x48 are compiled in today.

## 23. `docs`

README with an asciinema recording, per-platform install, controls, the ecology
model, and the `species.txt` format documented so players can add species. Man
page.

## 24. `packaging-and-release`

CMake `install` target, tagged release workflow producing Linux x86_64 and
aarch64 plus macOS binaries, source tarball, packaging metadata. Maybe an
AppImage and a Homebrew formula.

## 25. `windows-pdcurses` — stretch

Native Windows via PDCurses on Windows Terminal. WSL already works, so this is
optional; only worth it if "every terminal" has to include `cmd.exe`. Last,
because it will surface every remaining ncurses assumption in the engine.

---

## Order

01 → 02 → 03, then 15 → 16 → 17 → 18, then 05 → 06 → 07 → 08, then 09 → 10 → 11,
then 04 if the soak runs call for it, then 12 → 13 → 14, then 19 → 20 → 21 → 22 →
23 → 24 → 25.

The one trap: 15 through 18 are cheap now and expensive later. Every panel built
in Milestone B without a relayout hook and a glyph/color abstraction is a panel
to retrofit with one.

# Roadmap

My plan from here to a finished game. One branch and one PR per numbered item,
each sized to review on its own.

## Status

Merged through PR #16. Engine (terminal, palette, renderer, text measure, input,
grid, RNG, scenes, clock, loop), world (noise, biomes, tiles, generation,
climate), organisms (categories, traits, species, registry, `Organism`),
simulation (`SimulationEngine`, its four-phase tick, the nutrient cycle, and the
census), game (viewport, placement, starter ecosystem, headless runs). 138 tests.
CI runs clang-format, clang-tidy, gcc, clang.

## What's actually broken or missing

- The food web collapses to plants and decomposers. Carnivores, herbivores, and
  omnivores die out within a few hundred ticks of every seeded start, so three of
  the five trophic levels do not currently persist at all. Measured, not guessed;
  the numbers are under PR 03b.
- Eco-credits can only be spent, never earned. No objectives, no failure state.
- The nutrient cycle is simulated but invisible. Soil and detritus move every
  tick and the HUD shows neither, so a plot going barren looks like plants dying
  for no reason. PR 07 is where that readout belongs.
- Resize is decoded and dropped. `InputManager` maps `KEY_RESIZE` to
  `KeyCode::Resize`, but nothing handles it, `resizeterm()` is never called, and
  `Viewport` plus the help-bar row are fixed at construction. Any resize —
  including opening a tmux split — corrupts the layout.
- `panelw` is a `REQUIRED` dependency in `CMakeLists.txt:17` and the code never
  calls it. Glyphs are Unicode-only with no ASCII fallback. The monochrome path
  in `ColorPalette` is unexercised. CI is Linux-only.

## Checklist before opening any PR

- `scripts/check.sh` passes: clang-format, clang-tidy, the build under
  `-Wall -Wextra -Wpedantic -Werror`, and the tests.
- Catch2 coverage for new behavior. Split anything needing a live terminal so the
  pure logic tests without one, the way `decodeRawKeyRead` and `SimulationClock`
  already do.
- Doc comments on public types and functions, recording the reasoning rather than
  restating the signature.

---

# Milestone A — Make it simulate

## 01. `simulation-engine` — done, PR #16

`SimulationEngine` owns the world and the climate, borrows the shared RNG, and
runs the four-phase tick. The reasoning behind the phase order, the snapshot
walk, `environmentalFitness`, and the graze-versus-kill asymmetry now lives in
`SimulationEngine.hpp` rather than here.

## 02. `nutrient-cycle` — done

Deaths deposit detritus on the tile in the cleanup phase, decomposers work
detritus rather than corpses and mineralize part of it into `soilNutrientLevel`,
and photosynthesis scales with the soil and draws it down. The rates are in
`SimulationEngine.cpp` and the reasoning for each is on the constant.

Two of them - the mineralization rate and the drawdown rate - are a matched pair
set so a corpse roughly repays what that organism drew over a lifetime. That
balance is the thing PR 03's sweep should check first, because it decides whether
a closed plot trends fertile or barren over thousands of ticks.

## 03. `simulation-tuning-harness` — done

`--headless --ticks N --seed S --census-every N` runs the simulation with no
terminal and writes the census to stdout as CSV: populations by category, births,
deaths, feedings, mean soil, mean detritus, mean temperature, season, and weather.
`StarterEcosystem` stocks a world with a trophic pyramid, `HeadlessRun` drives it
and hands each sample to a callback, and `CommandLineOptions` parses the flags.

The climate columns were not in the original plan and turned out to be the ones
that mattered. A population crash looks identical whether the food web failed on
its own terms or a heatwave put the whole map outside every tolerance band at
once, and only the weather column tells them apart.

The balance pass this was built for is 03b. The measurements it produced are
recorded there rather than here.

## 03b. `ecosystem-balance-pass`

Touches `data/species.txt`, the engine's tuning constants, and
`StarterDensityPerHundredTiles`. New: `tests/test_ecosystem_soak.cpp`.

What the harness measured, across seeds 1, 7, 42, and 99 — the same failures
every time, so none of this is one unlucky world:

- **Carnivores never eat once.** Zero of them alive by tick 20 in every seed.
  With predators seeded at 0.25 per hundred tiles and herbivores thinning to
  roughly one per 450 tiles, a hunter with `moveRange` 3 sees 49 tiles and starves
  at 3 energy a tick before it ever finds prey. Either prey density, hunter reach,
  or the wander walk has to change; a predator that cannot find food is not a
  balance problem so much as a search problem.
- **Herbivores starve surrounded by plants**, gone by tick 140 with 1,315 of them
  on the map. This is the trade-off already flagged on
  `kMaximumGrazedFractionPerFeeding`: half of a seedling's energy is not a meal.
- **Omnivores go the same way as the herbivores**, and for the same reason.
- **Weather is a mass-casualty event.** Seed 42 loses plants 2730 → 613 to a
  heatwave at tick 140, 3576 → 1224 to a cold snap at tick 300, and the rest to a
  second heatwave at 400. A pattern shifts temperature by around 11°C against
  tolerance bands of ±7 to ±16, so one event can put most of the map outside every
  species' range at the same moment. Worth deciding whether the fix is smaller
  weather offsets, wider bands, or a gentler fitness curve — fitness multiplies
  two linear falloffs and gates feeding and breeding both, which is what makes an
  event this sharp.
- **Plants then boom-bust with nothing grazing them**: seed 1 reached 10,109 and
  was still climbing at tick 600, seed 42 crashed to zero.
- **The nutrient cycle is not implicated.** Mean soil held between 0.59 and 0.90
  in every run and detritus turned over the whole time, so PR 02's mineralization
  and drawdown pair can be left alone. That question is settled; it was the one
  thing this was most expected to catch.

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

03b, then 15 → 16 → 17 → 18, then 05 → 06 → 07 → 08, then 09 → 10 → 11, then 04 if
the soak runs call for it, then 12 → 13 → 14, then 19 → 20 → 21 → 22 → 23 → 24 →
25.

03b is lettered rather than numbered because the balance pass turned out to be a
PR of its own rather than the tail of the harness, and renumbering everything
below it would break the correspondence between these numbers and the branch
names already merged.

Two traps. 15 through 18 are cheap now and expensive later: every panel built in
Milestone B without a relayout hook and a glyph/color abstraction is a panel to
retrofit with one.

And 03 should come before anything else that sets an ecology constant. It was
listed after 02 and that was the wrong way round — 02's rates had to be picked
against actual trajectories, so the harness got rebuilt as throwaway probes to
finish the PR that was supposed to precede it. Any later PR that touches a rate
(06's income curve, 10's disaster severities, the balance pass itself) has the
same shape. Build the measuring tool first and use it.

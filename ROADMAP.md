# Roadmap

My plan from here to a finished game. One branch and one PR per numbered item,
each sized to review on its own.

## Status

Merged through PR #18. Engine (terminal, palette, renderer, text measure, input,
grid, RNG, scenes, clock, loop), world (noise, biomes, tiles, generation,
climate), organisms (categories, traits, species, registry, `Organism`),
simulation (`SimulationEngine`, its four-phase tick, the nutrient cycle, and the
census), game (viewport, placement, starter ecosystem, headless runs). 145 tests,
one of them a soak run over whole simulated worlds.
CI runs clang-format, clang-tidy, gcc, clang.

## What's actually broken or missing

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

## 03b. `ecosystem-balance-pass` — done

Touched `data/species.txt`, the engine's tuning constants,
`StarterDensityPerHundredTiles`, and the weather profiles. New:
`tests/test_ecosystem_soak.cpp`.

Done: 5000 ticks across seeds 1-7, 42, 99 and 123, with all five trophic categories
standing the whole way. The numbers that matter are the troughs, since a category
that touches zero anywhere is gone for good — the lowest each fell to, worst seed of
the ten: plants 16,340, herbivores 583, carnivores 84, omnivores 18, decomposers
1,562. Standing populations at tick 5000 run 21,000-24,000 plants, 3,700-7,700
herbivores, 550-820 carnivores, 1,000-2,700 omnivores and 4,400-6,800 decomposers,
and every tier oscillates rather than settling — the predator-prey cycle is visible
in the census output now instead of being a thing the model was supposed to have.

The one number still thin is seed 123's omnivores at 18. That is a real risk of loss
on a longer run and the honest place to look first if this regresses.

Most of what the harness originally reported turned out to be a symptom rather
than a cause, and the causes were not the ones the measurements suggested. Worth
keeping, because the same misreadings are available to any later PR that touches a
rate:

- **Nothing was starting grown.** `Organism` gives every new organism half its
  reproduction threshold, which is right for a birth and wrong for a founding
  population. Every plant in a seeded world was therefore a seedling with nothing
  on it above the root stock a grazer can take, so the herbivores placed beside
  them starved in what looked like a meadow. That one line explained the
  herbivores, the omnivores, and most of the carnivores. Fixed by seeding a starter
  world grown rather than newborn.
- **The producer tier was seeded at an eightieth of its standing crop** — 8 per
  hundred tiles against the ~20,000 plants the world settles at. The forage the
  founding herd needed did not exist until tick 100 and the herd was dead by 50.
- **"Carnivores never find prey" was backwards.** They find it immediately and eat
  all of it: 129 herbivores down to 5 by tick 11. A hunter foraged whenever it had
  room to store more energy, which is every tick of its life. The fix is satiation
  (`kForagingSatiationFractionOfEnergyCap`) — a fed animal ignores food, so one
  meal buys a dozen ticks of not hunting. Reach was never the problem, and the
  reach increases tried first made it worse.
- **Grazing scaled the wrong way round.** Taking a fraction of a plant's remaining
  energy made a mature plant an easy meal and a seedling a mouthful too small to
  live on. Replaced with a floor: a graze leaves the root stock, so maturity is
  what makes a plant food, and a browsed plant is held below its own breeding
  threshold rather than killed. Grazing now regulates the meadow through plant
  reproduction instead of plant death.
- **Diet is now ordered, and where preference applies decided stability.** An
  omnivore given a free choice farms the herbivores to extinction: it is never
  short of plants, so nothing limits how many rabbits it takes. But preference
  applied across the whole reach is worse in the other direction — a predator then
  hunts its favourite prey hardest exactly when that prey is rarest. Ranking within
  a search ring, so distance still wins and preference only breaks ties, gives both
  the grazing omnivore and genuine prey switching. This was the single change that
  turned a diverging oscillation into a stable one.
- **Weather was a mass-casualty event, and the amplitude was only half of it.**
  Offsets came down, but the load-bearing change was pairing each pattern's
  severity against its *duration*: an organism carries a couple of dozen ticks of
  upkeep in reserve, and a sharp pattern lasting longer than that reserve does not
  stress a population, it removes one. The two sharp patterns are now the two short
  ones. Fitness also combines its two axes as a geometric mean rather than a
  product, which stops one bad tile counting twice.
- **Nothing eats a carnivore, so the top of the web needed a brake of its own.**
  This was the last failure standing and the hardest to see, because it looks like
  the predators being too weak: they were hanging on at troughs of five to thirty
  and going out on half the seeds. They were in fact too strong. Every other tier is
  limited from outside — plants by the soil, grazers by the crop and by being eaten
  — but a predator's only limit is prey, and prey limits it late, because a fed
  animal cannot tell that the herd it is eating is the last of it. So the predators
  bred at full rate straight through each prey crash, and every cycle's trough came
  in below the last. Making them individually weaker never worked, and could not
  have: it lowers the peak and the trough together. Territory — a carnivore will not
  breed with another within one tile — costs a crowded predator population
  everything and a sparse one nothing, which is the asymmetry the problem needed.
  Carnivore troughs went from 0–34 to 114–177 across the same six seeds, and the
  herbivores under them from 0–476 to 991–1800.
- **Coexistence would not tune.** Two species in the same niche with slightly
  different numbers is a knife-edge: whichever is marginally better excludes the
  other, and the winner flips on small changes. The omnivore was the case that made
  this obvious — in temperate grassland it is a worse rabbit that also eats rabbits,
  so it either loses to the grazers or removes them, and no setting of its numbers
  avoids both. What worked was moving it: the jungle and the wetland are the parts of
  a generated map no herbivore has its ideal near, and the two carnivores went to
  opposite ends of the temperature range for the same reason. Niche separation, not
  parameter balance.
- **A trophic level resting on one species is one bad seed from empty**, and two is
  not enough if both fail the same way. The omnivores were the last category with a
  single species in it and the last still going extinct — on four of six seeds,
  while everything around them held. A second omnivore fixed most of those, but both
  were in humid ground, so a seed whose map came out dry still took the jungle and
  the wetland together. The third sits in the tundra, which fails under the opposite
  conditions, and also gives the wolf a prey base in its own climate — nothing else
  a wolf eats has its ideal anywhere near freezing, so the cold half of the map was
  a predator hunting prey that only reached it at the edge of its tolerance.
  Uncorrelated niches per category, not just several of them.
- **The nutrient cycle is not implicated**, as originally measured. Mean soil holds
  between 0.5 and 0.9 throughout. PR 02's mineralization and drawdown pair are left
  alone.

One bug found on the way: the `species.txt` copy beside the binary was a
`POST_BUILD` step, which only runs when the target relinks. Editing a species and
rebuilding therefore ran the *old* data with the build reporting success — which
silently invalidates the one workflow the file exists for. It is a keyed build
input now.

`tests/test_ecosystem_soak.cpp` guards this in two tiers: a 600-tick run over two
seeds on every build, which covers every failure listed above, and the full
criterion — 5000 ticks across 10 seeds — behind the `[.soak-full]` tag, to be run
whenever a tuning constant or anything in `species.txt` changes.

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

15 → 16 → 17 → 18, then 05 → 06 → 07 → 08, then 09 → 10 → 11, then 04 if the soak
runs call for it, then 12 → 13 → 14, then 19 → 20 → 21 → 22 → 23 → 24 → 25.

04 is now called for. The balance pass left the world carrying 25,000 to 35,000
organisms in steady state rather than the few thousand it collapsed to before, and
a 5000-tick soak run takes around two minutes a seed — which is why the full
ten-seed soak is tagged out of the default test run rather than in it.

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

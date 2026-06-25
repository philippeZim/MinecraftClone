# Agent guide — minecraft_clone

Modular C voxel game on sokol. **Work on one module at a time**: read that module's
`AGENTS.md` + its `.h` + the headers of its declared deps. You rarely need the whole tree.

## Module map & dependency rule

Dependencies point one way only. Never add a back-edge (e.g. `world` must not include
render or sokol). Enforced by CMake `target_link_libraries`.

```
core ← world      core, world ← player      core, world ← render      all ← main
```

- `src/core`   — `hmath.h` (vec/mat), `config.h` (dimensions), sokol impl unit.
- `src/world`  — `world.h`: voxel grid, edits, raycast. Pure data, no graphics. Internal
  `terrain.{h,c}`: stateless per-column generator (continents, mountains, rivers/lakes, biomes).
- `src/player` — `player.h`: FPS controller (physics, collision, fly); host feeds `player_input`.
- `src/render` — `render.h`: chunked indexed meshing + frustum culling.
- `src/main.c`     — window, input, block edits, HUD, frame loop.
- `src/main_bench.c` — autonomous perf harness: 4-phase autopilot (Spawn+Ascend,
  Look-Around, Cruise, Stop+Look), no user input, writes `bench.csv` on exit.

## Conventions

- C11, `-O3 -march=native -ffast-math`. Optimize for performance; favour flat arrays
  and static-inline hot paths. Apply YAGNI — build only what the slice needs.
- One public header per module; keep internals `static` in the `.c`.
- Shared constants go in `config.h`, never re-declared per module.

## Build

`cmake -S . -B build && cmake --build build -j && ./build/game`

## Bench harness

```sh
cmake -S . -B build && cmake --build build -j
./build/bench          # writes ./bench.csv, then exits
```

**Must be launched from the project root.** Texture paths in `src/render/render.c`
are CWD-relative (`textures/grass.png`, etc.); `stbi_load` only resolves them if the
launching shell's CWD contains `textures/`. Run from any other directory and the
renderer silently falls back to procedural tiles — the world still renders but every
block looks like the noisy AI-generated fallback instead of the texturepack PNGs.

`bench.csv` columns: `Phase,N,avg_ms,min_ms,p50_ms,p95_ms,max_ms,fps`. Phase
budgets are in `src/main_bench.c` (`P0_MAX_FRAMES=1200`, `P1_FRAMES=480`,
`P2_FRAMES=4800`, `P3_FRAMES=480`). Cruise altitude is `CRUISE_ALTITUDE=255.5`
(eye y), chosen above the hard surface clamp `WORLD_SY-2 = 254` so phase-2 forward
flight never collides with terrain.

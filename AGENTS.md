# Agent guide — minecraft_clone

Modular C voxel game on sokol. **Work on one module at a time**: read that module's
`AGENTS.md` + its `.h` + the headers of its declared deps. You rarely need the whole tree.

## Module map & dependency rule

Dependencies point one way only. Never add a back-edge (e.g. `world` must not include
render or sokol). Enforced by CMake `target_link_libraries`.

```
core  ← world      core  ← player      core, world ← render      all ← main
```

- `src/core`   — `hmath.h` (vec/mat), `config.h` (dimensions), sokol impl unit.
- `src/world`  — `world.h`: voxel grid + terrain gen. Pure data, no graphics.
- `src/player` — `player.h`: fly camera; host feeds it `player_input`.
- `src/render` — `render.h`: meshes the world (face culling) and draws it.
- `src/main.c` — window, input plumbing, frame loop.

## Conventions

- C11, `-O3 -march=native -ffast-math`. Optimize for performance; favour flat arrays
  and static-inline hot paths. Apply YAGNI — build only what the slice needs.
- One public header per module; keep internals `static` in the `.c`.
- Shared constants go in `config.h`, never re-declared per module.

## Build

`cmake -S . -B build && cmake --build build -j && ./build/game`

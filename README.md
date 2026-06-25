# minecraft_clone

A minimal voxel game in C using [sokol](https://github.com/floooh/sokol) (GL backend).
First slice: procedurally generated terrain + a free-fly player camera.

## Build & run (Linux)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/game
```

Requires a C compiler, CMake ≥ 3.16, and X11 + GL dev headers
(`libX11-devel libXi-devel libXcursor-devel libglvnd-devel`, all standard on Fedora).

## Controls

| Input            | Action          |
|------------------|-----------------|
| `W A S D`        | move            |
| `Space` / `Ctrl` | up / down       |
| `Shift`          | sprint          |
| mouse            | look            |
| click            | re-capture mouse|
| `Esc`            | release mouse   |

## Architecture (modular by design)

Each module is a static library with a single public header and a one-directional
dependency, so an agent can work on one module knowing only its header + deps.

```
core    src/core    math (hmath.h), config.h, sokol implementation unit   deps: sokol
world   src/world   voxel storage + terrain generation (no graphics)      deps: core
player  src/player  input-agnostic fly camera                             deps: core
render  src/render  builds GPU mesh from world, draws with sokol_gfx       deps: core, world
main    src/main.c  window/lifecycle; the only place modules meet          deps: all
```

See each module's `AGENTS.md` for its contract. Tunables live in `src/core/config.h`.

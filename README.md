# minecraft_clone

A minimal voxel game in C using [sokol](https://github.com/floooh/sokol) (GL backend).

Features: procedural terrain, chunked indexed meshing with hidden-face culling and
frustum culling, walking physics (gravity, jumping, AABB-vs-voxel collision) plus a
creative fly mode, block break/place via voxel raycast, and an FPS + coordinates HUD.

## Build & run (Linux)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/game
```

Requires a C compiler, CMake ≥ 3.16, and X11 + GL dev headers
(`libX11-devel libXi-devel libXcursor-devel libglvnd-devel`, all standard on Fedora).

## Controls

| Input         | Action                         |
|---------------|--------------------------------|
| `W A S D`     | move                           |
| `Space`       | jump (or ascend while flying)  |
| `Shift`       | descend while flying           |
| `Ctrl`        | sprint                         |
| `F`           | toggle fly                     |
| mouse         | look                           |
| left click    | break block                    |
| right click   | place block                    |
| `Esc`         | release mouse (click to recapture) |

## Architecture (modular by design)

Each module is a static library with a single public header and a one-directional
dependency, so an agent can work on one module knowing only its header + deps.

```
core    src/core    math (hmath.h), config.h, sokol implementation unit   deps: sokol
world   src/world   voxel storage, terrain gen, edits, raycast (no gfx)   deps: core
player  src/player  FPS controller: physics, collision, fly mode          deps: core, world
render  src/render  chunked indexed meshing + frustum culling             deps: core, world
main    src/main.c  window/lifecycle, input, block edits, HUD             deps: all
```

See each module's `AGENTS.md` for its contract. Tunables live in `src/core/config.h`.

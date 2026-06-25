# world module

Voxel storage, terrain generation, edits and picking. **Pure data — no sokol, no graphics.**

Public contract: `world.h`
- `world_init(seed)` — set the seed (no terrain yet).
- `world_update(center)` — (re)generate the `LOAD_RADIUS` ring of chunks around `center`; call each frame.
- `world_block` / `world_solid` — queries, AIR/0 out of bounds or unloaded. `world_solid` is false for water.
- `world_set_block(x,y,z,b)` — edit a cell (renderer must re-mesh the affected chunk).
- `world_raycast(origin,dir,max)` → `world_hit` (first solid cell + the empty cell before it; passes through water).
- `BLOCK_*` enum — block ids incl. `WATER/WOOD/LEAVES` (renderer maps these to textures).

**Infinite world**: a `GRID×GRID` ring of chunks slides with the player (`slot = (cx&MASK, cz&MASK)`);
revisiting regenerates identical noise-only terrain (no save). In-chunk index `(lx*WORLD_SY + y)*CHUNK_SZ + lz`,
global→chunk/local via `CX_OF/LX_OF` (chunk side is a power of two). Deps: `core` (config dims, hmath vec3).

Likely next work: biomes, caves, chunk save/load, async generation.

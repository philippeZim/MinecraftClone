# world module

Voxel storage, terrain generation, edits and picking. **Pure data — no sokol, no graphics.**

Public contract: `world.h`
- `world_init(seed)` — fill the grid.
- `world_block` / `world_solid` — queries, AIR/0 out of bounds.
- `world_set_block(x,y,z,b)` — edit a cell (renderer must re-mesh the affected chunk).
- `world_raycast(origin,dir,max)` → `world_hit` (first solid cell + the empty cell before it).
- `BLOCK_*` enum — block ids (renderer maps these to colours).

Deps: `core` (config dims, hmath vec3). Grid is one flat `uint8_t` array, index
`(x*WORLD_SY + y)*WORLD_SZ + z`. Keep this header stable — `render` and `player` use it.

Likely next work: chunked storage, biomes, save/load.

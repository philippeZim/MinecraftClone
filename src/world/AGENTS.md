# world module

Voxel storage + terrain generation. **Pure data — no sokol, no graphics, no I/O.**

Public contract: `world.h`
- `world_init(seed)` — fill the grid.
- `world_block(x,y,z)` / `world_solid(x,y,z)` — queries, return AIR/0 out of bounds.
- `BLOCK_*` enum — block ids (renderer maps these to colours).

Deps: `core` (config.h dimensions) only. Grid is one flat `uint8_t` array, index
`(x*WORLD_SY + y)*WORLD_SZ + z`. Keep this header stable — `render` reads through it.

Likely next work: chunking, biomes, save/load, block add/remove.

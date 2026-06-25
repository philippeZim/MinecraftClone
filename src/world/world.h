// World module: voxel storage + terrain generation. Pure data, zero rendering deps.
// Depends only on core (config.h, hmath.h). Renderer/player read through these queries.
#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>
#include "hmath.h"

enum { BLOCK_AIR = 0, BLOCK_GRASS, BLOCK_DIRT, BLOCK_STONE, BLOCK_SAND,
       BLOCK_WATER, BLOCK_WOOD, BLOCK_LEAVES, BLOCK_SNOW, BLOCK_COUNT };

// Result of a voxel raycast. `hit` block is the first solid cell; `pre` is the empty
// cell just before it (where a placed block goes).
typedef struct { int hit; int bx,by,bz; int px,py,pz; } world_hit;

void    world_init(uint32_t seed);                 // set the seed; chunks stream in via world_update
void    world_update(vec3 center);                 // (re)generate the loaded-chunk window around `center`
uint8_t world_block(int x, int y, int z);          // BLOCK_AIR when out of bounds / unloaded
int     world_solid(int x, int y, int z);          // 1 only for collidable blocks (air & water are not solid)
void    world_set_block(int x, int y, int z, uint8_t b); // edit a cell (no-op if unloaded / out of bounds)
int     world_chunk_ymax(int cx, int cz);          // highest non-air y in a loaded chunk; lets the mesher skip empty sky
world_hit world_raycast(vec3 origin, vec3 dir, float max_dist); // DDA voxel pick (passes through water)

#endif // WORLD_H

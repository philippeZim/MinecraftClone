// World module: voxel storage + terrain generation. Pure data, zero rendering deps.
// Depends only on core (config.h, hmath.h). Renderer/player read through these queries.
#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>
#include "hmath.h"

enum { BLOCK_AIR = 0, BLOCK_GRASS, BLOCK_DIRT, BLOCK_STONE, BLOCK_SAND, BLOCK_COUNT };

// Result of a voxel raycast. `hit` block is the first solid cell; `pre` is the empty
// cell just before it (where a placed block goes).
typedef struct { int hit; int bx,by,bz; int px,py,pz; } world_hit;

void    world_init(uint32_t seed);                 // generate terrain into the voxel grid
uint8_t world_block(int x, int y, int z);          // BLOCK_AIR when out of bounds
int     world_solid(int x, int y, int z);          // 1 if a solid (non-air) block sits there
void    world_set_block(int x, int y, int z, uint8_t b); // edit a cell (no-op out of bounds)
world_hit world_raycast(vec3 origin, vec3 dir, float max_dist); // DDA voxel pick

#endif // WORLD_H

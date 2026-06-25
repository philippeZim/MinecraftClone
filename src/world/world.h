// World module: voxel storage + terrain generation. Pure data, zero rendering deps.
// Depends only on core/config.h. Renderer reads blocks through world_block().
#ifndef WORLD_H
#define WORLD_H

#include <stdint.h>

enum { BLOCK_AIR = 0, BLOCK_GRASS, BLOCK_DIRT, BLOCK_STONE, BLOCK_SAND, BLOCK_COUNT };

void    world_init(uint32_t seed);              // generate terrain into the voxel grid
uint8_t world_block(int x, int y, int z);       // BLOCK_AIR when out of bounds
int     world_solid(int x, int y, int z);       // 1 if a solid (non-air) block sits there

#endif // WORLD_H

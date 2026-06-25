// Terrain generation: a pure per-column function of (x,z,seed). Internal to the world module
// (not a public game header) — world.c assembles voxels from these samples. Stateless by design:
// no cross-column state means chunks regenerate identically and gen parallelises trivially.
#ifndef TERRAIN_H
#define TERRAIN_H

#include <stdint.h>

// Everything world.c needs to fill one column: the solid surface height, the water surface
// (or -1 if dry), the top/sub-surface block ids, and whether the biome may grow a tree.
typedef struct { int surface, water; uint8_t top, sub; int tree; } terrain_col;

void        terrain_init(uint32_t seed);
terrain_col terrain_at(int x, int z);
float       terrain_hash(int x, int z, uint32_t salt); // shared deterministic RNG for feature placement

#endif // TERRAIN_H

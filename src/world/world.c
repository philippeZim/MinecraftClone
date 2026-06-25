#include "world.h"
#include "config.h"
#include <math.h>

// Flat voxel grid, indexed [x][y][z]. ~1MB at default dims — fits in cache-friendly
// linear memory, no per-chunk indirection for the first slice (YAGNI).
static uint8_t g_blocks[WORLD_SX * WORLD_SY * WORLD_SZ];

static inline int idx(int x, int y, int z) { return (x * WORLD_SY + y) * WORLD_SZ + z; }
static inline int in_bounds(int x, int y, int z) {
    return x >= 0 && y >= 0 && z >= 0 && x < WORLD_SX && y < WORLD_SY && z < WORLD_SZ;
}

// Cheap deterministic hash -> [0,1). Good enough for value-noise terrain.
static float hash2(int x, int z, uint32_t seed) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)z * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)(h & 0xFFFFFF) / (float)0x1000000;
}

// Smoothed value noise: bilinearly interpolate hashed lattice corners.
static float value_noise(float x, float z, uint32_t seed) {
    int xi = (int)floorf(x), zi = (int)floorf(z);
    float fx = x - xi, fz = z - zi;
    float u = fx*fx*(3.0f - 2.0f*fx), v = fz*fz*(3.0f - 2.0f*fz);
    float a = hash2(xi, zi, seed),     b = hash2(xi+1, zi, seed);
    float c = hash2(xi, zi+1, seed),   d = hash2(xi+1, zi+1, seed);
    return (a*(1-u) + b*u)*(1-v) + (c*(1-u) + d*u)*v;
}

void world_init(uint32_t seed) {
    for (int x = 0; x < WORLD_SX; ++x)
    for (int z = 0; z < WORLD_SZ; ++z) {
        // Two octaves of value noise -> rolling hills around mid height.
        float n = value_noise(x*0.06f, z*0.06f, seed) * 0.7f
                + value_noise(x*0.15f, z*0.15f, seed) * 0.3f;
        int height = (int)(WORLD_SY*0.35f + n * WORLD_SY*0.30f);
        if (height >= WORLD_SY) height = WORLD_SY - 1;
        for (int y = 0; y <= height; ++y) {
            uint8_t b = BLOCK_STONE;
            if (y == height)      b = (height < WORLD_SY*0.38f) ? BLOCK_SAND : BLOCK_GRASS;
            else if (y > height-4) b = BLOCK_DIRT;
            g_blocks[idx(x, y, z)] = b;
        }
    }
}

uint8_t world_block(int x, int y, int z) {
    return in_bounds(x, y, z) ? g_blocks[idx(x, y, z)] : BLOCK_AIR;
}

int world_solid(int x, int y, int z) {
    return world_block(x, y, z) != BLOCK_AIR;
}

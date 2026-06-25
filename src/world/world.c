#include "world.h"
#include "config.h"
#include <math.h>
#include <string.h>

// Infinite world: a fixed GRID×GRID ring of chunks slides with the player. Each slot holds
// whichever chunk currently maps onto it (cx&MASK, cz&MASK); a stale slot reads as air until
// regenerated. Chunks are pure noise so revisiting regenerates identical terrain (no save).
typedef struct { int cx, cz, loaded; uint8_t blocks[CHUNK_SX * WORLD_SY * CHUNK_SZ]; } chunk_t;
static chunk_t  g_chunks[GRID * GRID];
static uint32_t g_seed;

static inline int      lidx(int lx, int y, int lz) { return (lx * WORLD_SY + y) * CHUNK_SZ + lz; }
static inline chunk_t *slot(int cx, int cz) { return &g_chunks[(cx & GRID_MASK) * GRID + (cz & GRID_MASK)]; }

// Cheap deterministic hash -> [0,1). Good enough for value-noise terrain + feature placement.
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

static int height_at(int gx, int gz) {
    float n = value_noise(gx*0.012f, gz*0.012f, g_seed) * 0.6f   // broad rolling hills
            + value_noise(gx*0.05f,  gz*0.05f,  g_seed) * 0.3f   // medium detail
            + value_noise(gx*0.13f,  gz*0.13f,  g_seed) * 0.1f;  // fine roughness
    int h = (int)(SEA_LEVEL - 5 + n * 30.0f);
    return h < 1 ? 1 : h >= WORLD_SY ? WORLD_SY-1 : h;
}

// Fill one chunk's blocks straight from noise: stone/dirt/grass columns, sand beaches,
// water up to sea level, and the odd tree (kept fully inside the chunk so no cross-chunk writes).
static void gen_chunk(chunk_t *c, int cx, int cz) {
    memset(c->blocks, BLOCK_AIR, sizeof c->blocks);
    int ox = cx*CHUNK_SX, oz = cz*CHUNK_SZ;
    for (int lx = 0; lx < CHUNK_SX; ++lx)
    for (int lz = 0; lz < CHUNK_SZ; ++lz) {
        int gx = ox+lx, gz = oz+lz, h = height_at(gx, gz), land = h > SEA_LEVEL;
        for (int y = 0; y <= h; ++y)
            c->blocks[lidx(lx,y,lz)] = (y == h)   ? (land ? BLOCK_GRASS : BLOCK_SAND)
                                     : (y > h-4)  ? (land ? BLOCK_DIRT  : BLOCK_SAND) : BLOCK_STONE;
        for (int y = h+1; y <= SEA_LEVEL; ++y) c->blocks[lidx(lx,y,lz)] = BLOCK_WATER;

        if (land && h+1 <= WORLD_SY-7 && lx >= 2 && lx < CHUNK_SX-2 && lz >= 2 && lz < CHUNK_SZ-2
            && hash2(gx, gz, g_seed ^ 0x9e37u) > 0.985f) {              // ~1.5% of land columns
            int th = 4 + (int)(hash2(gx, gz, g_seed ^ 0x1234u) * 3), top = h + th;
            for (int t = 1; t <= th; ++t) c->blocks[lidx(lx, h+t, lz)] = BLOCK_WOOD;
            for (int dy = -2; dy <= 1; ++dy)
            for (int dx = -2; dx <= 2; ++dx)
            for (int dz = -2; dz <= 2; ++dz)
                if (dx*dx + dz*dz + dy*dy <= 6) {                       // round-ish canopy blob
                    int j = lidx(lx+dx, top+dy, lz+dz);
                    if (c->blocks[j] == BLOCK_AIR) c->blocks[j] = BLOCK_LEAVES;
                }
        }
    }
}

void world_init(uint32_t seed) {
    g_seed = seed;
    for (int i = 0; i < GRID*GRID; ++i) g_chunks[i].loaded = 0;
}

void world_update(vec3 center) {
    int ccx = CX_OF((int)floorf(center.x)), ccz = CX_OF((int)floorf(center.z));
    for (int dz = -LOAD_RADIUS; dz <= LOAD_RADIUS; ++dz)
    for (int dx = -LOAD_RADIUS; dx <= LOAD_RADIUS; ++dx) {
        int cx = ccx+dx, cz = ccz+dz;
        chunk_t *c = slot(cx, cz);
        if (c->loaded && c->cx == cx && c->cz == cz) continue;   // already the right chunk
        gen_chunk(c, cx, cz);
        c->cx = cx; c->cz = cz; c->loaded = 1;
    }
}

uint8_t world_block(int x, int y, int z) {
    if ((unsigned)y >= (unsigned)WORLD_SY) return BLOCK_AIR;
    int cx = CX_OF(x), cz = CX_OF(z);
    chunk_t *c = slot(cx, cz);
    if (!c->loaded || c->cx != cx || c->cz != cz) return BLOCK_AIR;
    return c->blocks[lidx(LX_OF(x), y, LX_OF(z))];
}

int world_solid(int x, int y, int z) {
    uint8_t b = world_block(x, y, z);
    return b != BLOCK_AIR && b != BLOCK_WATER;   // water is non-collidable (swimmable)
}

void world_set_block(int x, int y, int z, uint8_t b) {
    if ((unsigned)y >= (unsigned)WORLD_SY) return;
    int cx = CX_OF(x), cz = CX_OF(z);
    chunk_t *c = slot(cx, cz);
    if (c->loaded && c->cx == cx && c->cz == cz) c->blocks[lidx(LX_OF(x), y, LX_OF(z))] = b;
}

// Amanatides & Woo voxel DDA: step the integer cell along `dir` until a solid hit.
world_hit world_raycast(vec3 origin, vec3 dir, float max_dist) {
    world_hit h = {0};
    int x = (int)floorf(origin.x), y = (int)floorf(origin.y), z = (int)floorf(origin.z);
    int px = x, py = y, pz = z;
    int sx = dir.x > 0 ? 1 : -1, sy = dir.y > 0 ? 1 : -1, sz = dir.z > 0 ? 1 : -1;
    float tdx = dir.x != 0 ? fabsf(1.0f/dir.x) : 1e30f;
    float tdy = dir.y != 0 ? fabsf(1.0f/dir.y) : 1e30f;
    float tdz = dir.z != 0 ? fabsf(1.0f/dir.z) : 1e30f;
    float tx = (dir.x > 0 ? (x+1-origin.x) : (origin.x-x)) * tdx;
    float ty = (dir.y > 0 ? (y+1-origin.y) : (origin.y-y)) * tdy;
    float tz = (dir.z > 0 ? (z+1-origin.z) : (origin.z-z)) * tdz;
    for (float t = 0; t <= max_dist; ) {
        if (world_solid(x, y, z)) {
            h.hit = 1; h.bx=x; h.by=y; h.bz=z; h.px=px; h.py=py; h.pz=pz; return h;
        }
        px = x; py = y; pz = z; // remember last empty cell for placement
        if (tx <= ty && tx <= tz)      { x += sx; t = tx; tx += tdx; }
        else if (ty <= tz)             { y += sy; t = ty; ty += tdy; }
        else                           { z += sz; t = tz; tz += tdz; }
    }
    return h; // h.hit == 0
}

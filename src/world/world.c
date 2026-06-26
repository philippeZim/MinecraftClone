#include "world.h"
#include "terrain.h"
#include "config.h"
#include <math.h>
#include <string.h>

// Infinite world: a fixed GRID×GRID ring of chunks slides with the player. Each slot holds
// whichever chunk currently maps onto it (cx&MASK, cz&MASK); a stale slot reads as air until
// regenerated. Chunks are pure noise so revisiting regenerates identical terrain (no save).
// `ymax` caches the tallest non-air cell so the mesher can cap its vertical sweep over empty sky.
typedef struct { int cx, cz, loaded, ymax; uint8_t blocks[CHUNK_SX * WORLD_SY * CHUNK_SZ]; } chunk_t;
static chunk_t  g_chunks[GRID * GRID];

static inline int      lidx(int lx, int y, int lz) { return (lx * WORLD_SY + y) * CHUNK_SZ + lz; }
static inline chunk_t *slot(int cx, int cz) { return &g_chunks[(cx & GRID_MASK) * GRID + (cz & GRID_MASK)]; }

// Fill one chunk from the terrain sampler: stone/sub/top columns, water up to each column's
// water level, and the odd tree (kept fully inside the chunk so no cross-chunk writes). Caches
// the tallest non-air cell in ymax so the mesher can skip the empty sky above.
static void gen_chunk(chunk_t *c, int cx, int cz) {
    memset(c->blocks, BLOCK_AIR, sizeof c->blocks);
    int ox = cx*CHUNK_SX, oz = cz*CHUNK_SZ, ymax = 1;
    for (int lx = 0; lx < CHUNK_SX; ++lx)
    for (int lz = 0; lz < CHUNK_SZ; ++lz) {
        int gx = ox+lx, gz = oz+lz;
        terrain_col t = terrain_at(gx, gz);
        int h = t.surface;
        for (int y = 0; y <= h; ++y)
            c->blocks[lidx(lx,y,lz)] = (y == h) ? t.top : (y >= h-3) ? t.sub : BLOCK_STONE;
        for (int y = h+1; y <= t.water; ++y) c->blocks[lidx(lx,y,lz)] = BLOCK_WATER;
        int col_top = t.water > h ? t.water : h;

        if (t.tree && h+7 <= WORLD_SY-1 && lx >= 2 && lx < CHUNK_SX-2 && lz >= 2 && lz < CHUNK_SZ-2
            && terrain_hash(gx, gz, 0x9e37u) > 0.985f) {               // ~1.5% of forest columns
            int th = 4 + (int)(terrain_hash(gx, gz, 0x1234u) * 3), top = h + th;
            for (int s = 1; s <= th; ++s) c->blocks[lidx(lx, h+s, lz)] = BLOCK_WOOD;
            for (int dy = -2; dy <= 1; ++dy)
            for (int dx = -2; dx <= 2; ++dx)
            for (int dz = -2; dz <= 2; ++dz)
                if (dx*dx + dz*dz + dy*dy <= 6) {                       // round-ish canopy blob
                    int j = lidx(lx+dx, top+dy, lz+dz);
                    if (c->blocks[j] == BLOCK_AIR) c->blocks[j] = BLOCK_LEAVES;
                }
            if (top+1 > col_top) col_top = top+1;
        }
        if (col_top > ymax) ymax = col_top;
    }
    c->ymax = ymax;
}

void world_init(uint32_t seed) {
    terrain_init(seed);
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
    if (c->loaded && c->cx == cx && c->cz == cz) {
        c->blocks[lidx(LX_OF(x), y, LX_OF(z))] = b;
        if (b != BLOCK_AIR && y >= c->ymax) c->ymax = y+1;   // grow the mesh cap to cover a block placed up high
    }
}

int world_chunk_ymax(int cx, int cz) {
    chunk_t *c = slot(cx, cz);
    return (c->loaded && c->cx == cx && c->cz == cz) ? c->ymax : WORLD_SY-1;
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

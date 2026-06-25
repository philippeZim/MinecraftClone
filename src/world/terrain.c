#include "terrain.h"
#include "world.h"
#include "config.h"
#include <math.h>

// Altitude bands (block coords). With WORLD_SY=256, SEA_LEVEL=63 there is plenty of
// vertical room: bare rock starts well above sea, snow caps start even higher.
#define ROCK_LINE 150
#define SNOW_LINE 200

static uint32_t g_seed;

// Cheap deterministic hash -> [0,1). Good enough for value-noise terrain + feature placement.
static float hash2(int x, int z, uint32_t seed) {
    uint32_t h = (uint32_t)x*374761393u + (uint32_t)z*668265263u + seed*2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)(h & 0xFFFFFF) / (float)0x1000000;
}
// Smoothed value noise: bilinearly interpolate hashed lattice corners.
static float value_noise(float x, float z, uint32_t seed) {
    int xi = (int)floorf(x), zi = (int)floorf(z);
    float fx = x-xi, fz = z-zi, u = fx*fx*(3-2*fx), v = fz*fz*(3-2*fz);
    float a = hash2(xi,zi,seed),   b = hash2(xi+1,zi,seed);
    float c = hash2(xi,zi+1,seed), d = hash2(xi+1,zi+1,seed);
    return (a*(1-u)+b*u)*(1-v) + (c*(1-u)+d*u)*v;
}
// Fractal sum: octaves at doubling frequency / halving amplitude -> smooth rolling field in [0,1].
static float fbm(float x, float z, int oct, uint32_t seed) {
    float a = 0, amp = 1, f = 1, norm = 0;
    for (int i = 0; i < oct; ++i) { a += value_noise(x*f, z*f, seed+i*1013u)*amp; norm += amp; amp *= 0.5f; f *= 2.0f; }
    return a/norm;
}
// Ridged fractal: folds noise at its midline (1-|2n-1|) and squares it -> sharp mountain crests.
static float ridged(float x, float z, int oct, uint32_t seed) {
    float a = 0, amp = 1, f = 1, norm = 0;
    for (int i = 0; i < oct; ++i) { float n = 1 - fabsf(2*value_noise(x*f,z*f,seed+i*2017u)-1); a += n*n*amp; norm += amp; amp *= 0.5f; f *= 2.0f; }
    return a/norm;
}
static float smoothstep(float e0, float e1, float x) { float t = (x-e0)/(e1-e0); t = t<0?0:t>1?1:t; return t*t*(3-2*t); }

void  terrain_init(uint32_t seed)                  { g_seed = seed; }
float terrain_hash(int x, int z, uint32_t salt)    { return hash2(x, z, g_seed ^ salt); }

terrain_col terrain_at(int x, int z) {
    float fx = (float)x, fz = (float)z;

    // Continentalness in [-1, 1]: negative = ocean, positive = land.
    float cont = fbm(fx*0.0028f, fz*0.0028f, 5, g_seed^0xC0u) * 2.0f - 1.0f;
    // Erosion in [0, 1]: low = mountainous, high = flat plains.
    float ero  = fbm(fx*0.0055f, fz*0.0055f, 3, g_seed^0xE0u);

    // Base elevation: deep oceans (down to SEA_LEVEL-60) vs. land plateaus (up to SEA_LEVEL+45).
    float base = cont < 0.0f ? SEA_LEVEL + cont * 60.0f       // ocean
                            : SEA_LEVEL + 1.0f + cont * 45.0f; // land

    // Mountain peaks: tall ridges where land is high AND erosion is low.
    float land_mask = smoothstep(-0.10f, 0.30f, cont);
    float rough     = 1.0f - smoothstep(0.25f, 0.65f, ero);
    base += land_mask * rough * ridged(fx*0.011f, fz*0.011f, 5, g_seed^0xC4u) * 180.0f;

    // Hills + occasional small mountains everywhere except deep ocean and pure flat plains.
    // `land_mask * rough` doubles as a "small mountain" field: low-erosion inland gets a
    // mountain-ish bump on top of the hill noise, so forest biome ends up lumpy not pancake.
    float bumpy = land_mask * rough;
    float hill_mask = smoothstep(-0.55f, 0.05f, cont) * (1.0f - smoothstep(0.92f, 0.99f, ero));
    base += hill_mask * (fbm(fx*0.022f, fz*0.022f, 3, g_seed^0xA1u) - 0.5f) * 55.0f;
    base += smoothstep(0.20f, 0.55f, bumpy)
          * (fbm(fx*0.030f, fz*0.030f, 3, g_seed^0xA2u) - 0.5f) * 40.0f;
    base += (fbm(fx*0.10f, fz*0.10f, 2, g_seed^0xA3u) - 0.5f) * 6.0f;

    // Surface roughness (one block of bumpiness).
    base += (fbm(fx*0.08f, fz*0.08f, 2, g_seed^0xD0u) - 0.5f) * 2.0f;

    int surface = (int)base;
    if (surface < 1) surface = 1;
    if (surface > WORLD_SY-2) surface = WORLD_SY-2;

    int water_level = surface < SEA_LEVEL ? SEA_LEVEL : -1;

    terrain_col c = { surface, water_level, 0, 0, 0 };
    if (c.water >= 0) {                                                          // ocean floor
        // Minecraft-style sea bed: a thin sand cap near the shore, dirt through the middle
        // depths, then bare stone in the abyss. The sub layer mirrors the top so the
        // "soft" cover is a few blocks thick before solid stone begins underneath.
        int depth = c.water - surface;
        if      (depth <= 4)  { c.top = BLOCK_SAND;  c.sub = BLOCK_SAND;  }       // shore shallows
        else if (depth <= 15) { c.top = BLOCK_DIRT;  c.sub = BLOCK_DIRT;  }       // mid-depth
        else                  { c.top = BLOCK_STONE; c.sub = BLOCK_STONE; }       // abyss
    } else {
        float j = value_noise(fx*0.3f, fz*0.3f, g_seed^0x9u)*6.0f - 3.0f;        // ragged biome transitions
        if      (surface > SNOW_LINE + j) { c.top = BLOCK_SNOW;  c.sub = BLOCK_STONE; }   // snow caps
        else if (surface > ROCK_LINE + j) { c.top = BLOCK_STONE; c.sub = BLOCK_STONE; }   // bare mountain rock
        else if (surface <= SEA_LEVEL + 7) {
            // Near-water land. Beaches are common patches (most shores get one) but still
            // rarer around shallow enclosed water (lakes) and the width varies — sometimes a
            // 1-block strip, sometimes several blocks inland.
            float beach_signal = fbm(fx*0.008f, fz*0.008f, 3, g_seed^0xB0u);
            float beach_width  = fbm(fx*0.008f, fz*0.008f, 3, g_seed^0xB1u);
            float threshold    = cont < 0.15f ? 0.65f : 0.50f;     // ~25% lakeshore, ~50% oceanshore
            if (beach_signal > threshold) {
                int max_h = SEA_LEVEL + 1 + (int)(beach_width * 5.0f);   // up to ~6 blocks inland
                if (surface <= max_h) c.top = c.sub = BLOCK_SAND;
                else                  { c.top = BLOCK_GRASS; c.sub = BLOCK_DIRT; c.tree = 1; }
            } else {
                c.top = BLOCK_GRASS; c.sub = BLOCK_DIRT; c.tree = 1;
            }
        } else {
            c.top = BLOCK_GRASS; c.sub = BLOCK_DIRT; c.tree = 1;
        }
    }
    return c;
}
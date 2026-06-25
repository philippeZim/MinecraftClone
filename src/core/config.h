// Global compile-time tunables. Shared by every module so dimensions stay in sync.
#ifndef CONFIG_H
#define CONFIG_H

#define CHUNK_BITS 5                 // chunk side = 2^bits; power-of-two lets coord<->chunk use shift/mask
#define CHUNK_SX (1 << CHUNK_BITS)   // 32  chunk width  (x)
#define CHUNK_SZ (1 << CHUNK_BITS)   // 32  chunk depth  (z)
#define CHUNK_SY 64                  // chunk/world height (y)
#define WORLD_SY CHUNK_SY

#define LOAD_RADIUS   7   // chunks generated around the player in every direction (streaming window)
#define RENDER_RADIUS 6   // chunks meshed/drawn — one less than load so meshed chunks always have neighbours
#define GRID 16           // ring size: power-of-two ≥ 2*LOAD_RADIUS+1, indexes the loaded-chunk window
#define GRID_MASK (GRID - 1)

#define SEA_LEVEL 28      // water fills air at/below this height

// Global block coord -> owning chunk coord / in-chunk local coord (works for negatives via 2's-complement).
#define CX_OF(x) ((x) >> CHUNK_BITS)
#define LX_OF(x) ((x) & (CHUNK_SX - 1))

#endif // CONFIG_H

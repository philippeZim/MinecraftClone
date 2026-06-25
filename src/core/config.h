// Global compile-time tunables. Shared by every module so dimensions stay in sync.
#ifndef CONFIG_H
#define CONFIG_H

#define CHUNK_SX 32   // chunk width  (x) — bigger chunks = fewer draw calls + better greedy merging
#define CHUNK_SY 64   // chunk height (y)
#define CHUNK_SZ 32   // chunk depth  (z)

#define WORLD_CHUNKS_X 4   // world size in chunks along x
#define WORLD_CHUNKS_Z 4   // world size in chunks along z

#define WORLD_SX (CHUNK_SX * WORLD_CHUNKS_X)
#define WORLD_SZ (CHUNK_SZ * WORLD_CHUNKS_Z)
#define WORLD_SY CHUNK_SY

#endif // CONFIG_H

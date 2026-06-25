// Render module: turns the voxel world into per-chunk GPU meshes and draws them with
// frustum culling. The only module that touches the GPU; world/player stay graphics-free.
// Depends on core (hmath, sokol_gfx) + world (reads blocks).
#ifndef RENDER_H
#define RENDER_H

#include "hmath.h"

void render_init(void);                 // build all chunk meshes + pipeline (needs sg_setup + world_init)
void render_frame(mat4 view_proj);      // draw visible chunks into the current pass
void render_after_edit(int x, int z);   // rebuild the chunk at (x,z) and any border neighbours
void render_shutdown(void);

#endif // RENDER_H

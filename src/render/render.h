// Render module: turns the voxel world into a GPU mesh and draws it with sokol_gfx.
// Depends on sokol_gfx, world (reads blocks) and core/hmath. The only module that
// touches the GPU — world/player stay graphics-free.
#ifndef RENDER_H
#define RENDER_H

#include "hmath.h"

void render_init(void);            // build world mesh + create pipeline (needs sg_setup + world_init done)
void render_frame(mat4 view_proj); // draw one frame into the default pass
void render_shutdown(void);

#endif // RENDER_H

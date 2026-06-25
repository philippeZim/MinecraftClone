# render module

The only module that touches the GPU (sokol_gfx). Builds one static mesh from the
world (hidden-face culling) and draws it.

Public contract: `render.h`
- `render_init()` — call after `sg_setup` + `world_init`; meshes world, makes pipeline.
- `render_frame(view_proj)` — draw into the current default pass.
- `render_shutdown()`.

Deps: `core` (hmath, sokol_gfx) + `world` (reads blocks). Vertex = pos3+normal3+color3.
Shaders are embedded GLSL 410 strings (no shader-compiler toolchain). `FACES[6]` holds
CCW cube faces; `BLOCK_COLOR` maps block ids to colour.

Likely next work: per-chunk meshes + frustum culling, index buffers, texture atlas,
mesh rebuild on block edits.

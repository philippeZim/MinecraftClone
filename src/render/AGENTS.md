# render module

The only module that touches the GPU (sokol_gfx). The world is split into
`WORLD_CHUNKS_X * WORLD_CHUNKS_Z` chunk columns; each gets its own indexed mesh,
frustum-culled per frame.

Public contract: `render.h`
- `render_init()` — after `sg_setup` + `world_init`; builds every chunk + the pipeline.
- `render_frame(view_proj)` — draws visible chunks into the current pass.
- `render_after_edit(x,z)` — rebuild the edited chunk (+ border neighbours) after a block change.
- `render_shutdown()`.

Deps: `core` (hmath, sokol_gfx) + `world` (reads blocks). Vertex = pos3+normal3+color3,
uint32 indices. Shaders are embedded GLSL 410. `build_chunk` uses **greedy meshing**
(coplanar same-block faces merge into one quad via a signed face mask; winding fixed by a
cross-product test against the face normal). Frustum planes via Gribb-Hartmann.

Likely next work: texture atlas, async/threaded chunk builds, transparency, packed vertices.

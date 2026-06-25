# render module

The only module that touches the GPU (sokol_gfx). Mirrors world's `GRID×GRID` chunk ring; each
slot owns an indexed mesh that streams in via `render_update` and is frustum-culled per frame.

Public contract: `render.h`
- `render_init()` — after `sg_setup`; builds the two pipelines + the procedural block texture array.
- `render_update(center)` — mesh chunks within `RENDER_RADIUS`, nearest-first, budgeted per frame (anti-hitch).
- `render_frame(view_proj)` — draw visible chunks: opaque pass, then a translucent water pass.
- `render_after_edit(x,z)` — rebuild the edited chunk (+ border neighbours) after a block change.
- `render_shutdown()`.

Deps: `core` (hmath, sokol_gfx) + `world` (reads blocks). Vertex = pos3 float + tiled UV2 float +
packed `info` (shade byte, block-layer byte) = 24 bytes; per-face shading baked in, no normals. uint32
indices. **Textures** are a procedurally generated `sg` array image (one light/friendly tile per block
id), sampled NEAREST/REPEAT so each greedy quad tiles by its `w×h` extent. `mesh(cx,cz,pass)` does
**greedy meshing** twice: opaque blocks, then water (a water face shows only against AIR, so it's hidden
under terrain/itself). Water draws in `pip_water` (alpha blend, depth-write off, cull none). Frustum via
Gribb-Hartmann.

Likely next work: mipmaps/AO, async/threaded chunk builds, byte-packed positions, distance fog.

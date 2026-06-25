#include "render.h"
#include "world.h"
#include "config.h"
#include "sokol_gfx.h"
#include <stdlib.h>
#include <math.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

// Interleaved vertex: position(3 floats) + tiled UV(2 floats) + packed info(shade,layer) = 24 bytes.
// UVs run 0..quad-extent and the sampler repeats, so one greedy quad tiles its texture per block.
typedef struct { float px,py,pz,u,v; uint32_t info; } vertex;
static inline int opaque(int b) { return b != BLOCK_AIR && b != BLOCK_WATER; }

// One chunk's GPU meshes: opaque geometry + a separate translucent water mesh (drawn after, no depth write).
typedef struct {
    int cx, cz, have;
    sg_buffer vb, ib;  int icount;
    sg_buffer wvb, wib; int wcount;
} rchunk;

static struct {
    sg_pipeline pip, pip_water;
    sg_view     tex_view;
    sg_sampler  smp;
    int         ccx, ccz;          // current centre chunk (set by render_update)
    rchunk      chunks[GRID * GRID];
} g_r;

static inline rchunk *rslot(int cx, int cz) { return &g_r.chunks[(cx & GRID_MASK) * GRID + (cz & GRID_MASK)]; }

// --- block textures: load real PNGs from ./textures if present, else procedural friendly tiles ----
#define TEX 16            // tile resolution (Minecraft default); non-16² PNGs nearest-resample to fit
#define LIGHTEN 0.10f     // gentle "lighten up": lerp every texel this far toward white (0 = textures as-is)

// Texture layers: the first BLOCK_COUNT match the BLOCK_* ids 1:1, then extra layers for the
// per-face blocks (grass has a grass→dirt transition on its sides; a log shows end-grain on top/bottom).
enum { TX_GRASS_SIDE = BLOCK_COUNT, TX_LOG_TOP, TX_COUNT };

// Drop PNGs here to use real textures. Missing → procedural fallback tile.
static const char *TEX_FILE[TX_COUNT] = {
    0, "textures/grass.png", "textures/dirt.png", "textures/stone.png",
    "textures/sand.png", "textures/water.png", "textures/wood.png", "textures/leaves.png",
    "textures/grass_side.png", "textures/log_top.png" };
// Per-layer tint for loaded textures (white = texture's own colour). Minecraft ships grass/leaves/water
// greyscale and tints them by biome at runtime — we bake a friendly plains-style tint here instead.
static const float TINT[TX_COUNT][3] = {
    {1,1,1},                  // AIR
    {0.57f,0.74f,0.35f},      // GRASS  plains green
    {1,1,1},                  // DIRT
    {1,1,1},                  // STONE
    {1,1,1},                  // SAND
    {0.30f,0.52f,0.92f},      // WATER  blue
    {1,1,1},                  // WOOD
    {0.45f,0.72f,0.32f},      // LEAVES foliage green
    {1,1,1},                  // GRASS_SIDE (green fringe already baked in this pack)
    {1,1,1},                  // LOG_TOP
};
static const float BASE[TX_COUNT][4] = {          // procedural-fallback colours (already light/friendly)
    {0,0,0,0},                  // AIR (unused)
    {0.49f,0.78f,0.42f,1.0f},   // GRASS  fresh light green
    {0.64f,0.48f,0.34f,1.0f},   // DIRT   warm light brown
    {0.70f,0.70f,0.74f,1.0f},   // STONE  light grey
    {0.94f,0.88f,0.69f,1.0f},   // SAND   pale
    {0.40f,0.69f,0.95f,0.62f},  // WATER  light sky-blue, translucent
    {0.68f,0.53f,0.37f,1.0f},   // WOOD   light bark
    {0.55f,0.81f,0.49f,1.0f},   // LEAVES soft light green
    {0.64f,0.48f,0.34f,1.0f},   // GRASS_SIDE fallback → dirt
    {0.80f,0.65f,0.45f,1.0f},   // LOG_TOP    fallback → pale wood end-grain
};
static uint32_t h32(uint32_t x) { x^=x>>16; x*=0x7feb352du; x^=x>>15; x*=0x846ca68bu; x^=x>>16; return x; }
static uint8_t  clampu8(float v) { v = v<0?0:v>1?1:v; return (uint8_t)(v*255.0f + 0.5f); }
static void lighten_put(uint8_t *o, float r, float g, float b, float a) {
    o[0] = clampu8(r + (1-r)*LIGHTEN); o[1] = clampu8(g + (1-g)*LIGHTEN);
    o[2] = clampu8(b + (1-b)*LIGHTEN); o[3] = clampu8(a);
}

static void make_textures(void) {
    static uint8_t px[TX_COUNT * TEX * TEX * 4];
    for (int b = 0; b < TX_COUNT; ++b) {
        uint8_t *layer = &px[b * TEX*TEX*4];
        int sw, sh;
        unsigned char *img = TEX_FILE[b] ? stbi_load(TEX_FILE[b], &sw, &sh, &(int){0}, 4) : NULL;
        if (img) {                                  // real texture: tint, lighten, nearest-fit to TEX²
            int S = sw < sh ? sw : sh;              // top square → first frame of animated strips (water)
            for (int y = 0; y < TEX; ++y) for (int x = 0; x < TEX; ++x) {
                const unsigned char *s = &img[((y*S/TEX)*sw + (x*S/TEX)) * 4];
                float a = (b == BLOCK_WATER) ? BASE[b][3] : s[3]/255.0f;   // keep water translucent
                lighten_put(&layer[(y*TEX + x)*4], s[0]/255.0f*TINT[b][0], s[1]/255.0f*TINT[b][1],
                            s[2]/255.0f*TINT[b][2], a);
            }
            stbi_image_free(img);
        } else {                                    // procedural fallback tile
            for (int y = 0; y < TEX; ++y) for (int x = 0; x < TEX; ++x) {
                float n = (h32((uint32_t)((b*257 + y)*263 + x)) & 0xFFFF) / 65535.0f * 2.0f - 1.0f;
                float var = (b == BLOCK_GRASS || b == BLOCK_LEAVES) ? 0.10f : 0.06f, d = b == BLOCK_WATER ? 0 : n*var;
                if (b == BLOCK_WOOD && (x % 5) == 0) d -= 0.10f;          // vertical bark grain
                lighten_put(&layer[(y*TEX + x)*4], BASE[b][0]+d, BASE[b][1]+d, BASE[b][2]+d, BASE[b][3]);
            }
        }
    }
    sg_image img = sg_make_image(&(sg_image_desc){
        .type = SG_IMAGETYPE_ARRAY, .width = TEX, .height = TEX, .num_slices = TX_COUNT,
        .pixel_format = SG_PIXELFORMAT_RGBA8, .data.mip_levels[0] = { px, sizeof px }, .label = "block-atlas" });
    g_r.tex_view = sg_make_view(&(sg_view_desc){ .texture.image = img });
    g_r.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST, .mag_filter = SG_FILTER_NEAREST,
        .wrap_u = SG_WRAP_REPEAT, .wrap_v = SG_WRAP_REPEAT });
}

// --- reusable CPU scratch for the chunk currently being meshed (no per-build malloc) -------------
static vertex  *sv; static uint32_t *si; static int scap, ssvlen, ssilen;
static void reserve(int verts) {
    if (ssvlen + verts <= scap) return;
    scap = scap ? scap*2 : 1<<15;
    while (ssvlen + verts > scap) scap *= 2;
    sv = realloc(sv, scap*sizeof(vertex));
    si = realloc(si, scap*6/4*sizeof(uint32_t)); // 6 indices per 4 verts
}

#define MASK_MAX (WORLD_SY * (CHUNK_SX > CHUNK_SZ ? CHUNK_SX : CHUNK_SZ))
static int g_mask[MASK_MAX];

// Greedy-mesh one pass of chunk (cx,cz) into the scratch buffers. pass 0 = opaque blocks;
// pass 1 = water, where a face shows only against AIR (so water hidden under terrain/itself).
static void mesh(int cx, int cz, int water) {
    int dims[3] = { CHUNK_SX, WORLD_SY, CHUNK_SZ };
    int off[3]  = { cx*CHUNK_SX, 0, cz*CHUNK_SZ };
    for (int d = 0; d < 3; ++d) {
        int u = (d+1)%3, v = (d+2)%3, wdim = dims[u], hdim = dims[v], p[3];
        for (int s = -1; s < dims[d]; ++s) {
            int n = 0;
            for (p[v] = 0; p[v] < hdim; ++p[v])
            for (p[u] = 0; p[u] < wdim; ++p[u], ++n) {
                p[d] = s;   int A = world_block(off[0]+p[0], off[1]+p[1], off[2]+p[2]);
                p[d] = s+1; int B = world_block(off[0]+p[0], off[1]+p[1], off[2]+p[2]);
                if (!water) {
                    int As = opaque(A), Bs = opaque(B);
                    g_mask[n] = (As && !Bs) ? (s >= 0        ?  A : 0)
                              : (Bs && !As) ? (s < dims[d]-1 ? -B : 0) : 0;
                } else {
                    int wa = (A == BLOCK_WATER), wb = (B == BLOCK_WATER);
                    g_mask[n] = (wa && B == BLOCK_AIR) ? (s >= 0        ?  BLOCK_WATER : 0)
                              : (wb && A == BLOCK_AIR) ? (s < dims[d]-1 ? -BLOCK_WATER : 0) : 0;
                }
            }
            n = 0;
            for (int j = 0; j < hdim; ++j)
            for (int i = 0; i < wdim; ) {
                int c = g_mask[n];
                if (c == 0) { ++i; ++n; continue; }
                int w = 1; while (i+w < wdim && g_mask[n+w] == c) ++w;
                int hh = 1, stop = 0;
                while (j+hh < hdim) {
                    for (int k = 0; k < w; ++k) if (g_mask[n + k + hh*wdim] != c) { stop = 1; break; }
                    if (stop) break; ++hh;
                }
                int base[3], du[3] = {0,0,0}, dv[3] = {0,0,0};
                base[d] = s+1; base[u] = i; base[v] = j; du[u] = w; dv[v] = hh;
                vec3 c0 = v3(off[0]+base[0], off[1]+base[1], off[2]+base[2]);
                vec3 c1 = v3(c0.x+du[0], c0.y+du[1], c0.z+du[2]);
                vec3 c3 = v3(c0.x+dv[0], c0.y+dv[1], c0.z+dv[2]);
                vec3 c2 = v3(c1.x+dv[0], c1.y+dv[1], c1.z+dv[2]);
                float sgn = c > 0 ? 1.0f : -1.0f;
                vec3 N = v3(d==0?sgn:0, d==1?sgn:0, d==2?sgn:0);
                float sh = N.y>0 ? 1.0f : N.y<0 ? 0.55f : (N.x!=0 ? 0.8f : 0.65f);
                int blk = c<0 ? -c : c, layer = blk;       // per-face textures for grass + logs:
                if (blk == BLOCK_GRASS) layer = N.y>0 ? BLOCK_GRASS : N.y<0 ? BLOCK_DIRT : TX_GRASS_SIDE;
                else if (blk == BLOCK_WOOD) layer = N.y!=0 ? TX_LOG_TOP : BLOCK_WOOD;  // end-grain top/bottom
                uint32_t info = (uint32_t)(sh*255.0f) | ((uint32_t)layer << 8);

                vec3 cs[4] = { c0,c1,c2,c3 };
                // Orient UVs so oriented textures (grass-side fringe, log bark) stand upright: the vertical
                // in-plane axis (world Y) maps to tex-V with the image's top at the block's top; the other
                // axis tiles horizontally. Extents are in block units so REPEAT tiles once per block.
                float pu[4] = {0,(float)w,(float)w,0}, pv[4] = {0,0,(float)hh,(float)hh}, uv[4][2];
                for (int k = 0; k < 4; ++k) {
                    if (u == 1)      { uv[k][0] = pv[k]; uv[k][1] = (float)w  - pu[k]; }  // X-face: Y is the u-axis
                    else if (v == 1) { uv[k][0] = pu[k]; uv[k][1] = (float)hh - pv[k]; }  // Z-face: Y is the v-axis
                    else             { uv[k][0] = pu[k]; uv[k][1] = pv[k]; }              // top/bottom: any orient
                }
                if (v3_dot(v3_cross(v3_sub(c1,c0), v3_sub(c3,c0)), N) < 0) {     // fix winding to CCW
                    vec3 t = cs[1]; cs[1] = cs[3]; cs[3] = t;
                    float a = uv[1][0], b = uv[1][1]; uv[1][0]=uv[3][0]; uv[1][1]=uv[3][1]; uv[3][0]=a; uv[3][1]=b;
                }
                reserve(4);
                uint32_t bi = (uint32_t)ssvlen;
                for (int q = 0; q < 4; ++q) sv[ssvlen++] = (vertex){ cs[q].x,cs[q].y,cs[q].z, uv[q][0],uv[q][1], info };
                uint32_t qi[6] = { bi,bi+1,bi+2, bi,bi+2,bi+3 };
                for (int q = 0; q < 6; ++q) si[ssilen++] = qi[q];
                for (int l = 0; l < hh; ++l) for (int k = 0; k < w; ++k) g_mask[n+k+l*wdim] = 0;
                i += w; n += w;
            }
        }
    }
}

static sg_buffer upload_vb(void) { return sg_make_buffer(&(sg_buffer_desc){ .data = { sv, (size_t)ssvlen*sizeof(vertex) } }); }
static sg_buffer upload_ib(void) { return sg_make_buffer(&(sg_buffer_desc){ .usage.index_buffer = true, .data = { si, (size_t)ssilen*sizeof(uint32_t) } }); }

static void build_chunk(rchunk *ch, int cx, int cz) {
    if (ch->vb.id)  { sg_destroy_buffer(ch->vb);  sg_destroy_buffer(ch->ib);  ch->vb.id = ch->ib.id = 0; }
    if (ch->wvb.id) { sg_destroy_buffer(ch->wvb); sg_destroy_buffer(ch->wib); ch->wvb.id = ch->wib.id = 0; }
    ssvlen = ssilen = 0; mesh(cx, cz, 0); ch->icount = ssilen;
    if (ssilen) { ch->vb = upload_vb(); ch->ib = upload_ib(); }
    ssvlen = ssilen = 0; mesh(cx, cz, 1); ch->wcount = ssilen;
    if (ssilen) { ch->wvb = upload_vb(); ch->wib = upload_ib(); }
    ch->cx = cx; ch->cz = cz; ch->have = 1;
}

static const char *VS =
    "#version 410\n"
    "uniform mat4 mvp;\n in vec3 position;\n in vec2 texcoord0;\n in vec4 info;\n"
    "out vec2 uv;\n out float vlayer;\n out float vshade;\n"
    "void main(){ gl_Position = mvp*vec4(position,1.0); uv = texcoord0; vlayer = info.y*255.0; vshade = info.x; }\n";
static const char *FS =
    "#version 410\n"
    "uniform sampler2DArray tex;\n in vec2 uv;\n in float vlayer;\n in float vshade;\n out vec4 frag_color;\n"
    "void main(){ vec4 t = texture(tex, vec3(uv, floor(vlayer+0.5))); frag_color = vec4(t.rgb*vshade, t.a); }\n";

void render_init(void) {
    make_textures();
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func   = { .source = VS },
        .fragment_func = { .source = FS },
        .attrs = { [0].glsl_name="position", [1].glsl_name="texcoord0", [2].glsl_name="info" },
        .uniform_blocks[0] = { .stage = SG_SHADERSTAGE_VERTEX, .size = sizeof(mat4),
                               .glsl_uniforms[0] = { .type=SG_UNIFORMTYPE_MAT4, .glsl_name="mvp" } },
        .views[0].texture = { .stage = SG_SHADERSTAGE_FRAGMENT, .image_type = SG_IMAGETYPE_ARRAY,
                              .sample_type = SG_IMAGESAMPLETYPE_FLOAT },
        .samplers[0] = { .stage = SG_SHADERSTAGE_FRAGMENT, .sampler_type = SG_SAMPLERTYPE_FILTERING },
        .texture_sampler_pairs[0] = { .stage = SG_SHADERSTAGE_FRAGMENT, .view_slot = 0, .sampler_slot = 0,
                                      .glsl_name = "tex" },
        .label = "world-shader",
    });
    sg_vertex_layout_state layout = { .attrs = { [0].format=SG_VERTEXFORMAT_FLOAT3,
                                                 [1].format=SG_VERTEXFORMAT_FLOAT2,
                                                 [2].format=SG_VERTEXFORMAT_UBYTE4N } };
    g_r.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd, .layout = layout, .index_type = SG_INDEXTYPE_UINT32,
        .depth = { .compare = SG_COMPAREFUNC_LESS_EQUAL, .write_enabled = true },
        .cull_mode = SG_CULLMODE_BACK, .face_winding = SG_FACEWINDING_CCW, .label = "opaque-pipeline",
    });
    g_r.pip_water = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd, .layout = layout, .index_type = SG_INDEXTYPE_UINT32,
        .depth = { .compare = SG_COMPAREFUNC_LESS_EQUAL, .write_enabled = false },
        .colors[0].blend = { .enabled = true,
            .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA, .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .src_factor_alpha = SG_BLENDFACTOR_ONE, .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA },
        .cull_mode = SG_CULLMODE_NONE, .label = "water-pipeline",   // see water surface from below
    });
}

// Stream meshes for chunks within RENDER_RADIUS, nearest first, capped per frame to avoid hitches.
void render_update(vec3 center) {
    g_r.ccx = CX_OF((int)floorf(center.x));
    g_r.ccz = CX_OF((int)floorf(center.z));
    int budget = 8;
    for (int r = 0; r <= RENDER_RADIUS; ++r)
    for (int dz = -r; dz <= r; ++dz)
    for (int dx = -r; dx <= r; ++dx) {
        if (abs(dx) != r && abs(dz) != r) continue;       // only this ring's shell
        int cx = g_r.ccx+dx, cz = g_r.ccz+dz;
        rchunk *ch = rslot(cx, cz);
        if (ch->have && ch->cx == cx && ch->cz == cz) continue;
        build_chunk(ch, cx, cz);
        if (--budget == 0) return;
    }
}

// Gribb-Hartmann frustum planes from a column-major view_proj (m[col*4+row]).
static void extract_planes(mat4 vp, float pl[6][4]) {
    #define M(r,c) vp.m[(c)*4+(r)]
    for (int i = 0; i < 6; ++i) {
        int r = i >> 1; float s = (i & 1) ? -1.0f : 1.0f;
        for (int c = 0; c < 4; ++c) pl[i][c] = M(3,c) + s*M(r,c);
    }
    #undef M
}
static int aabb_visible(float pl[6][4], float mnx,float mny,float mnz, float mxx,float mxy,float mxz) {
    for (int i = 0; i < 6; ++i) {
        float a=pl[i][0], b=pl[i][1], c=pl[i][2], d=pl[i][3];
        float px = a>0?mxx:mnx, py = b>0?mxy:mny, pz = c>0?mxz:mnz;
        if (a*px + b*py + c*pz + d < 0) return 0;
    }
    return 1;
}

void render_frame(mat4 view_proj) {
    float pl[6][4]; extract_planes(view_proj, pl);
    sg_range u = SG_RANGE(view_proj);
    // pass 1: opaque
    sg_apply_pipeline(g_r.pip);
    sg_apply_uniforms(0, &u);
    for (int i = 0; i < GRID*GRID; ++i) {
        rchunk *ch = &g_r.chunks[i];
        if (!ch->have || !ch->icount) continue;
        if (abs(ch->cx-g_r.ccx) > RENDER_RADIUS || abs(ch->cz-g_r.ccz) > RENDER_RADIUS) continue; // stale far slot
        if (!aabb_visible(pl, ch->cx*CHUNK_SX,0,ch->cz*CHUNK_SZ, (ch->cx+1)*CHUNK_SX,WORLD_SY,(ch->cz+1)*CHUNK_SZ)) continue;
        sg_apply_bindings(&(sg_bindings){ .vertex_buffers[0]=ch->vb, .index_buffer=ch->ib,
                                          .views[0]=g_r.tex_view, .samplers[0]=g_r.smp });
        sg_draw(0, ch->icount, 1);
    }
    // pass 2: translucent water (after opaque so it blends over the scene; depth-write off)
    sg_apply_pipeline(g_r.pip_water);
    sg_apply_uniforms(0, &u);
    for (int i = 0; i < GRID*GRID; ++i) {
        rchunk *ch = &g_r.chunks[i];
        if (!ch->have || !ch->wcount) continue;
        if (abs(ch->cx-g_r.ccx) > RENDER_RADIUS || abs(ch->cz-g_r.ccz) > RENDER_RADIUS) continue;
        if (!aabb_visible(pl, ch->cx*CHUNK_SX,0,ch->cz*CHUNK_SZ, (ch->cx+1)*CHUNK_SX,WORLD_SY,(ch->cz+1)*CHUNK_SZ)) continue;
        sg_apply_bindings(&(sg_bindings){ .vertex_buffers[0]=ch->wvb, .index_buffer=ch->wib,
                                          .views[0]=g_r.tex_view, .samplers[0]=g_r.smp });
        sg_draw(0, ch->wcount, 1);
    }
}

void render_after_edit(int x, int z) {
    int cx = CX_OF(x), cz = CX_OF(z);
    build_chunk(rslot(cx, cz), cx, cz);
    // A border edit exposes/hides faces in the adjacent chunk too — rebuild neighbours.
    if (LX_OF(x) == 0)          build_chunk(rslot(cx-1, cz), cx-1, cz);
    if (LX_OF(x) == CHUNK_SX-1) build_chunk(rslot(cx+1, cz), cx+1, cz);
    if (LX_OF(z) == 0)          build_chunk(rslot(cx, cz-1), cx, cz-1);
    if (LX_OF(z) == CHUNK_SZ-1) build_chunk(rslot(cx, cz+1), cx, cz+1);
}

void render_shutdown(void) { free(sv); free(si); }

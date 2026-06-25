#include "render.h"
#include "world.h"
#include "config.h"
#include "sokol_gfx.h"
#include <stdlib.h>
#include <math.h>

// Interleaved vertex: position(3 floats) + packed RGBA8 colour = 16 bytes. Face shading
// is baked into the colour at mesh time, so no per-vertex normal/lighting is needed.
typedef struct { float px,py,pz; uint32_t rgba; } vertex;
static inline uint32_t pack_rgb(float r, float g, float b) {
    return (uint32_t)(r*255) | (uint32_t)(g*255)<<8 | (uint32_t)(b*255)<<16 | 0xFF000000u;
}

#define NCX WORLD_CHUNKS_X
#define NCZ WORLD_CHUNKS_Z

typedef struct { sg_buffer vb, ib; int index_count; } chunk;

static struct {
    sg_pipeline pip;
    chunk       chunks[NCX][NCZ];
} g_r;

// Base RGB per block type, indexed by the BLOCK_* enum (greedy mesher shades per face).
static const float BLOCK_COLOR[BLOCK_COUNT][3] = {
    {0,0,0}, {0.35f,0.62f,0.25f}, {0.55f,0.40f,0.25f}, {0.50f,0.50f,0.52f}, {0.85f,0.78f,0.55f},
};

// Reusable CPU scratch buffers for the chunk currently being meshed (no per-build malloc).
static vertex  *sv; static uint32_t *si; static int scap, ssvlen, ssilen;
static void reserve(int verts) {
    if (ssvlen + verts <= scap) return;
    scap = scap ? scap*2 : 1<<15;
    while (ssvlen + verts > scap) scap *= 2;
    sv = realloc(sv, scap*sizeof(vertex));
    si = realloc(si, scap*6/4*sizeof(uint32_t)); // 6 indices per 4 verts
}

// Greedy-mesh scratch mask (largest u*v face plane for a chunk column).
#define MASK_MAX (WORLD_SY * (CHUNK_SX > CHUNK_SZ ? CHUNK_SX : CHUNK_SZ))
static int g_mask[MASK_MAX];

// Build (or rebuild) the mesh for chunk (cx,cz) with greedy meshing: coplanar same-block
// faces merge into single quads, collapsing flat spans (terrain top, world walls, floor)
// from thousands of quads into a handful. Mask value sign encodes the face direction.
static void build_chunk(int cx, int cz) {
    chunk *ch = &g_r.chunks[cx][cz];
    if (ch->vb.id) { sg_destroy_buffer(ch->vb); sg_destroy_buffer(ch->ib); ch->vb.id = ch->ib.id = 0; }
    ssvlen = ssilen = 0;
    int dims[3] = { CHUNK_SX, WORLD_SY, CHUNK_SZ };
    int off[3]  = { cx*CHUNK_SX, 0, cz*CHUNK_SZ };

    for (int d = 0; d < 3; ++d) {
        int u = (d+1)%3, v = (d+2)%3, wdim = dims[u], hdim = dims[v], p[3];
        for (int s = -1; s < dims[d]; ++s) {
            // Mask the boundary between layer s and s+1: +id if the lower cell owns the
            // face, -id if the upper cell does, 0 if no visible face (or owned by a neighbour chunk).
            int n = 0;
            for (p[v] = 0; p[v] < hdim; ++p[v])
            for (p[u] = 0; p[u] < wdim; ++p[u], ++n) {
                p[d] = s;   int A = world_block(off[0]+p[0], off[1]+p[1], off[2]+p[2]);
                p[d] = s+1; int B = world_block(off[0]+p[0], off[1]+p[1], off[2]+p[2]);
                int As = A != BLOCK_AIR, Bs = B != BLOCK_AIR;
                g_mask[n] = (As && !Bs) ? (s >= 0          ?  A : 0)
                          : (Bs && !As) ? (s < dims[d]-1   ? -B : 0) : 0;
            }
            // Merge equal-value mask cells into maximal rectangles, one quad each.
            n = 0;
            for (int j = 0; j < hdim; ++j)
            for (int i = 0; i < wdim; ) {
                int c = g_mask[n];
                if (c == 0) { ++i; ++n; continue; }
                int w = 1; while (i+w < wdim && g_mask[n+w] == c) ++w;
                int h = 1, stop = 0;
                while (j+h < hdim) {
                    for (int k = 0; k < w; ++k) if (g_mask[n + k + h*wdim] != c) { stop = 1; break; }
                    if (stop) break; ++h;
                }
                int base[3], du[3] = {0,0,0}, dv[3] = {0,0,0};
                base[d] = s+1; base[u] = i; base[v] = j; du[u] = w; dv[v] = h;
                vec3 c0 = v3(off[0]+base[0], off[1]+base[1], off[2]+base[2]);
                vec3 c1 = v3(c0.x+du[0], c0.y+du[1], c0.z+du[2]);
                vec3 c3 = v3(c0.x+dv[0], c0.y+dv[1], c0.z+dv[2]);
                vec3 c2 = v3(c1.x+dv[0], c1.y+dv[1], c1.z+dv[2]);
                float sgn = c > 0 ? 1.0f : -1.0f;
                vec3 N = v3(d==0?sgn:0, d==1?sgn:0, d==2?sgn:0);
                if (v3_dot(v3_cross(v3_sub(c1,c0), v3_sub(c3,c0)), N) < 0) { vec3 t=c1; c1=c3; c3=t; }
                float sh = N.y>0 ? 1.0f : N.y<0 ? 0.55f : (N.x!=0 ? 0.8f : 0.65f);
                const float *col = BLOCK_COLOR[c<0 ? -c : c];
                uint32_t rgba = pack_rgb(col[0]*sh, col[1]*sh, col[2]*sh);
                reserve(4);
                uint32_t bi = (uint32_t)ssvlen;
                vec3 cs[4] = { c0,c1,c2,c3 };
                for (int q=0;q<4;++q) sv[ssvlen++] = (vertex){ cs[q].x,cs[q].y,cs[q].z, rgba };
                uint32_t qi[6] = { bi,bi+1,bi+2, bi,bi+2,bi+3 };
                for (int q=0;q<6;++q) si[ssilen++] = qi[q];
                for (int l=0;l<h;++l) for (int k=0;k<w;++k) g_mask[n+k+l*wdim] = 0;
                i += w; n += w;
            }
        }
    }
    ch->index_count = ssilen;
    if (ssilen == 0) return; // empty chunk (all air) — nothing to upload
    ch->vb = sg_make_buffer(&(sg_buffer_desc){ .data = { sv, (size_t)ssvlen*sizeof(vertex) } });
    ch->ib = sg_make_buffer(&(sg_buffer_desc){ .usage.index_buffer = true,
                                               .data = { si, (size_t)ssilen*sizeof(uint32_t) } });
}

static const char *VS =
    "#version 410\n"
    "uniform mat4 mvp;\n in vec3 position;\n in vec4 color0;\n out vec3 v_color;\n"
    "void main(){ gl_Position = mvp * vec4(position,1.0); v_color = color0.rgb; }\n";
static const char *FS =
    "#version 410\n"
    "in vec3 v_color;\n out vec4 frag_color;\n"
    "void main(){ frag_color = vec4(v_color, 1.0); }\n";

void render_init(void) {
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func   = { .source = VS },
        .fragment_func = { .source = FS },
        .attrs = { [0].glsl_name="position", [1].glsl_name="color0" },
        .uniform_blocks[0] = { .stage = SG_SHADERSTAGE_VERTEX, .size = sizeof(mat4),
                               .glsl_uniforms[0] = { .type=SG_UNIFORMTYPE_MAT4, .glsl_name="mvp" } },
        .label = "world-shader",
    });
    g_r.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .layout.attrs = { [0].format=SG_VERTEXFORMAT_FLOAT3, [1].format=SG_VERTEXFORMAT_UBYTE4N },
        .index_type = SG_INDEXTYPE_UINT32,
        .depth = { .compare = SG_COMPAREFUNC_LESS_EQUAL, .write_enabled = true },
        .cull_mode = SG_CULLMODE_BACK,
        .face_winding = SG_FACEWINDING_CCW,
        .label = "world-pipeline",
    });
    for (int cx = 0; cx < NCX; ++cx)
        for (int cz = 0; cz < NCZ; ++cz)
            build_chunk(cx, cz);
}

// Gribb-Hartmann frustum planes from a column-major view_proj (m[col*4+row]).
static void extract_planes(mat4 vp, float pl[6][4]) {
    #define M(r,c) vp.m[(c)*4+(r)]
    for (int i = 0; i < 6; ++i) {
        int r = i >> 1; float s = (i & 1) ? -1.0f : 1.0f; // +/- of row r against row 3
        for (int c = 0; c < 4; ++c) pl[i][c] = M(3,c) + s*M(r,c);
    }
    #undef M
}
static int aabb_visible(float pl[6][4], float mnx,float mny,float mnz, float mxx,float mxy,float mxz) {
    for (int i = 0; i < 6; ++i) {
        float a=pl[i][0], b=pl[i][1], c=pl[i][2], d=pl[i][3];
        float px = a>0?mxx:mnx, py = b>0?mxy:mny, pz = c>0?mxz:mnz; // positive vertex
        if (a*px + b*py + c*pz + d < 0) return 0; // fully behind a plane
    }
    return 1;
}

void render_frame(mat4 view_proj) {
    float pl[6][4]; extract_planes(view_proj, pl);
    sg_apply_pipeline(g_r.pip);
    sg_range u = SG_RANGE(view_proj);
    sg_apply_uniforms(0, &u);
    for (int cx = 0; cx < NCX; ++cx)
    for (int cz = 0; cz < NCZ; ++cz) {
        chunk *ch = &g_r.chunks[cx][cz];
        if (!ch->index_count || !ch->vb.id) continue; // skip empty / failed-alloc chunks
        if (!aabb_visible(pl, cx*CHUNK_SX,0,cz*CHUNK_SZ, (cx+1)*CHUNK_SX,WORLD_SY,(cz+1)*CHUNK_SZ)) continue;
        sg_apply_bindings(&(sg_bindings){ .vertex_buffers[0]=ch->vb, .index_buffer=ch->ib });
        sg_draw(0, ch->index_count, 1);
    }
}

void render_after_edit(int x, int z) {
    int cx = x / CHUNK_SX, cz = z / CHUNK_SZ;
    build_chunk(cx, cz);
    // A border edit exposes/hides faces in the adjacent chunk too — rebuild neighbours.
    if (x % CHUNK_SX == 0          && cx > 0)     build_chunk(cx-1, cz);
    if (x % CHUNK_SX == CHUNK_SX-1 && cx < NCX-1) build_chunk(cx+1, cz);
    if (z % CHUNK_SZ == 0          && cz > 0)     build_chunk(cx, cz-1);
    if (z % CHUNK_SZ == CHUNK_SZ-1 && cz < NCZ-1) build_chunk(cx, cz+1);
}

void render_shutdown(void) { free(sv); free(si); }

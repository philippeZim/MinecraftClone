#include "render.h"
#include "world.h"
#include "config.h"
#include "sokol_gfx.h"
#include <stdlib.h>
#include <math.h>

// Interleaved vertex: position(3) + normal(3) + color(3) = 9 floats.
typedef struct { float px,py,pz, nx,ny,nz, r,g,b; } vertex;

#define NCX WORLD_CHUNKS_X
#define NCZ WORLD_CHUNKS_Z

typedef struct { sg_buffer vb, ib; int index_count; } chunk;

static struct {
    sg_pipeline pip;
    chunk       chunks[NCX][NCZ];
} g_r;

// 6 cube faces: neighbour offset to test for culling + 4 CCW corners (outward normal).
static const struct { int dx,dy,dz; float c[4][3]; } FACES[6] = {
    {  1, 0, 0, {{1,0,1},{1,0,0},{1,1,0},{1,1,1}} }, // +X
    { -1, 0, 0, {{0,0,0},{0,0,1},{0,1,1},{0,1,0}} }, // -X
    {  0, 1, 0, {{0,1,0},{0,1,1},{1,1,1},{1,1,0}} }, // +Y (top)
    {  0,-1, 0, {{0,0,0},{1,0,0},{1,0,1},{0,0,1}} }, // -Y (bottom)
    {  0, 0, 1, {{0,0,1},{1,0,1},{1,1,1},{0,1,1}} }, // +Z
    {  0, 0,-1, {{1,0,0},{0,0,0},{0,1,0},{1,1,0}} }, // -Z
};
static const float SHADE[6] = { 0.8f,0.8f, 1.0f,0.55f, 0.65f,0.65f };
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

// Build (or rebuild) the mesh for chunk (cx,cz) into GPU buffers.
static void build_chunk(int cx, int cz) {
    chunk *ch = &g_r.chunks[cx][cz];
    if (ch->vb.id) { sg_destroy_buffer(ch->vb); sg_destroy_buffer(ch->ib); ch->vb.id = ch->ib.id = 0; }
    ssvlen = ssilen = 0;
    int x0 = cx*CHUNK_SX, z0 = cz*CHUNK_SZ;
    for (int x = x0; x < x0+CHUNK_SX; ++x)
    for (int y = 0;  y < WORLD_SY;      ++y)
    for (int z = z0; z < z0+CHUNK_SZ; ++z) {
        uint8_t b = world_block(x,y,z);
        if (b == BLOCK_AIR) continue;
        const float *col = BLOCK_COLOR[b];
        for (int f = 0; f < 6; ++f) {
            if (world_solid(x+FACES[f].dx, y+FACES[f].dy, z+FACES[f].dz)) continue;
            reserve(4);
            uint32_t base = (uint32_t)ssvlen;
            float s = SHADE[f];
            for (int i = 0; i < 4; ++i)
                sv[ssvlen++] = (vertex){ x+FACES[f].c[i][0], y+FACES[f].c[i][1], z+FACES[f].c[i][2],
                                         (float)FACES[f].dx,(float)FACES[f].dy,(float)FACES[f].dz,
                                         col[0]*s, col[1]*s, col[2]*s };
            uint32_t q[6] = { base,base+1,base+2, base,base+2,base+3 };
            for (int i = 0; i < 6; ++i) si[ssilen++] = q[i];
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
    "uniform mat4 mvp;\n in vec3 position;\n in vec3 normal;\n in vec3 color0;\n out vec3 v_color;\n"
    "void main(){\n"
    "  gl_Position = mvp * vec4(position,1.0);\n"
    "  float d = max(dot(normalize(normal), normalize(vec3(0.5,1.0,0.3))), 0.0);\n"
    "  v_color = color0 * (0.5 + 0.5*d);\n"
    "}\n";
static const char *FS =
    "#version 410\n"
    "in vec3 v_color;\n out vec4 frag_color;\n"
    "void main(){ frag_color = vec4(v_color, 1.0); }\n";

void render_init(void) {
    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func   = { .source = VS },
        .fragment_func = { .source = FS },
        .attrs = { [0].glsl_name="position", [1].glsl_name="normal", [2].glsl_name="color0" },
        .uniform_blocks[0] = { .stage = SG_SHADERSTAGE_VERTEX, .size = sizeof(mat4),
                               .glsl_uniforms[0] = { .type=SG_UNIFORMTYPE_MAT4, .glsl_name="mvp" } },
        .label = "world-shader",
    });
    g_r.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .layout.attrs = { [0].format=SG_VERTEXFORMAT_FLOAT3, [1].format=SG_VERTEXFORMAT_FLOAT3, [2].format=SG_VERTEXFORMAT_FLOAT3 },
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
        if (!ch->index_count) continue;
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

#include "render.h"
#include "world.h"
#include "config.h"
#include "sokol_gfx.h"
#include <stdlib.h>
#include <string.h>

// Interleaved vertex: position(3) + normal(3) + color(3) = 9 floats.
typedef struct { float px,py,pz, nx,ny,nz, r,g,b; } vertex;

static struct {
    sg_pipeline pip;
    sg_bindings bind;
    int         vertex_count;
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

// Base RGB per block type, indexed by the BLOCK_* enum.
static const float BLOCK_COLOR[BLOCK_COUNT][3] = {
    {0,0,0},            // AIR (unused)
    {0.35f,0.62f,0.25f},// GRASS
    {0.55f,0.40f,0.25f},// DIRT
    {0.50f,0.50f,0.52f},// STONE
    {0.85f,0.78f,0.55f},// SAND
};

// Growable CPU-side vertex buffer used only during mesh build.
static vertex *g_buf; static int g_cap, g_len;
static void push(vertex v) {
    if (g_len == g_cap) { g_cap = g_cap ? g_cap*2 : 1<<16; g_buf = realloc(g_buf, g_cap*sizeof(vertex)); }
    g_buf[g_len++] = v;
}

// Emit the two triangles of one block face with baked directional-ambient shade.
static void emit_face(int x, int y, int z, int f, const float col[3]) {
    const float shade[6] = { 0.8f,0.8f, 1.0f,0.55f, 0.65f,0.65f }; // per-face brightness
    float r=col[0]*shade[f], g=col[1]*shade[f], b=col[2]*shade[f];
    float nx=FACES[f].dx, ny=FACES[f].dy, nz=FACES[f].dz;
    vertex q[4];
    for (int i=0;i<4;i++) q[i] = (vertex){ x+FACES[f].c[i][0], y+FACES[f].c[i][1], z+FACES[f].c[i][2], nx,ny,nz, r,g,b };
    int order[6] = {0,1,2, 0,2,3};
    for (int i=0;i<6;i++) push(q[order[i]]);
}

static void build_mesh(void) {
    for (int x=0; x<WORLD_SX; ++x)
    for (int y=0; y<WORLD_SY; ++y)
    for (int z=0; z<WORLD_SZ; ++z) {
        uint8_t b = world_block(x,y,z);
        if (b == BLOCK_AIR) continue;
        for (int f=0; f<6; ++f)
            if (!world_solid(x+FACES[f].dx, y+FACES[f].dy, z+FACES[f].dz)) // skip hidden faces
                emit_face(x,y,z,f, BLOCK_COLOR[b]);
    }
}

static const char *VS =
    "#version 410\n"
    "uniform mat4 mvp;\n"
    "in vec3 position;\n in vec3 normal;\n in vec3 color0;\n"
    "out vec3 v_color;\n"
    "void main(){\n"
    "  gl_Position = mvp * vec4(position,1.0);\n"
    "  vec3 L = normalize(vec3(0.5,1.0,0.3));\n"
    "  float d = max(dot(normalize(normal), L), 0.0);\n"
    "  v_color = color0 * (0.5 + 0.5*d);\n"
    "}\n";

static const char *FS =
    "#version 410\n"
    "in vec3 v_color;\n out vec4 frag_color;\n"
    "void main(){ frag_color = vec4(v_color, 1.0); }\n";

void render_init(void) {
    build_mesh();
    g_r.vertex_count = g_len;

    g_r.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = (sg_range){ g_buf, (size_t)g_len * sizeof(vertex) },
        .label = "world-vertices",
    });
    free(g_buf); g_buf = NULL; g_cap = g_len = 0; // mesh is static; drop CPU copy

    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func   = { .source = VS },
        .fragment_func = { .source = FS },
        .attrs = {
            [0].glsl_name = "position",
            [1].glsl_name = "normal",
            [2].glsl_name = "color0",
        },
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size  = sizeof(mat4),
            .glsl_uniforms[0] = { .type = SG_UNIFORMTYPE_MAT4, .glsl_name = "mvp" },
        },
        .label = "world-shader",
    });

    g_r.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .layout.attrs = {
            [0].format = SG_VERTEXFORMAT_FLOAT3, // position
            [1].format = SG_VERTEXFORMAT_FLOAT3, // normal
            [2].format = SG_VERTEXFORMAT_FLOAT3, // color
        },
        .depth = { .compare = SG_COMPAREFUNC_LESS_EQUAL, .write_enabled = true },
        .cull_mode = SG_CULLMODE_BACK,
        .face_winding = SG_FACEWINDING_CCW,
        .label = "world-pipeline",
    });
}

void render_frame(mat4 view_proj) {
    sg_apply_pipeline(g_r.pip);
    sg_apply_bindings(&g_r.bind);
    sg_range u = SG_RANGE(view_proj);
    sg_apply_uniforms(0, &u);
    sg_draw(0, g_r.vertex_count, 1);
}

void render_shutdown(void) { /* sg_shutdown() in host frees GPU resources */ }

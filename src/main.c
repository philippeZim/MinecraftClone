// Host/entry: owns the window + sokol lifecycle and wires world + render + player.
// Per-module separation means this file is the only place they meet.
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "sokol_time.h"

#include "config.h"
#include "hmath.h"
#include "world.h"
#include "render.h"
#include "player.h"

static struct {
    player_input in;     // accumulates held keys + mouse delta between frames
    uint64_t     last;   // stm tick stamp of previous frame
    int          locked; // pointer-lock / mouse-look active
} g;

static void init(void) {
    stm_setup();
    sg_setup(&(sg_desc){ .environment = sglue_environment(), .logger.func = slog_func });
    world_init(1337);
    render_init();
    // Drop the player above the centre of the world looking out over the terrain.
    player_init(v3(WORLD_SX*0.5f, WORLD_SY*0.7f, WORLD_SZ*0.5f));
    g.last = stm_now();
    sapp_lock_mouse(true);
    g.locked = 1;
}

static void frame(void) {
    float dt = (float)stm_sec(stm_laptime(&g.last));
    if (dt > 0.1f) dt = 0.1f; // clamp huge hitches (e.g. after a breakpoint)

    player_update(&g.in, dt);
    g.in.mouse_dx = g.in.mouse_dy = 0.0f; // consume per-frame mouse delta

    float aspect = sapp_widthf() / sapp_heightf();
    mat4 proj = m4_perspective(1.2f, aspect, 0.1f, 1000.0f);
    mat4 view_proj = m4_mul(proj, player_view());

    sg_begin_pass(&(sg_pass){
        .action.colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = {0.52f,0.70f,0.95f,1.0f} },
        .swapchain = sglue_swapchain(),
    });
    render_frame(view_proj);
    sg_end_pass();
    sg_commit();
}

static void event(const sapp_event *e) {
    switch (e->type) {
    case SAPP_EVENTTYPE_KEY_DOWN:
    case SAPP_EVENTTYPE_KEY_UP: {
        int down = (e->type == SAPP_EVENTTYPE_KEY_DOWN);
        switch (e->key_code) {
        case SAPP_KEYCODE_W:           g.in.forward = down; break;
        case SAPP_KEYCODE_S:           g.in.back    = down; break;
        case SAPP_KEYCODE_A:           g.in.left    = down; break;
        case SAPP_KEYCODE_D:           g.in.right   = down; break;
        case SAPP_KEYCODE_SPACE:       g.in.up      = down; break;
        case SAPP_KEYCODE_LEFT_CONTROL:g.in.down    = down; break;
        case SAPP_KEYCODE_LEFT_SHIFT:  g.in.sprint  = down; break;
        case SAPP_KEYCODE_ESCAPE:      if (down) { g.locked = 0; sapp_lock_mouse(false); } break;
        default: break;
        }
        break;
    }
    case SAPP_EVENTTYPE_MOUSE_MOVE:
        if (g.locked) { g.in.mouse_dx += e->mouse_dx; g.in.mouse_dy += e->mouse_dy; }
        break;
    case SAPP_EVENTTYPE_MOUSE_DOWN: // click to re-capture the pointer
        if (!g.locked) { g.locked = 1; sapp_lock_mouse(true); }
        break;
    default: break;
    }
}

static void cleanup(void) {
    render_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init, .frame_cb = frame, .event_cb = event, .cleanup_cb = cleanup,
        .width = 1280, .height = 720,
        .sample_count = 4,
        .window_title = "minecraft_clone",
        .logger.func = slog_func,
    };
}

// Host/entry: owns the window + sokol lifecycle and wires world + render + player + HUD.
// Per-module separation means this file is the only place they meet.
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
#include "sokol_time.h"
#include "sokol_debugtext.h"

#include "config.h"
#include "hmath.h"
#include "world.h"
#include "render.h"
#include "player.h"

static struct {
    player_input in;
    uint64_t     last;
    int          locked;
    float        fps;   // smoothed frames/second for the HUD
} g;

// Drop the player onto the surface at the centre of the world.
static vec3 spawn_point(void) {
    int x = WORLD_SX/2, z = WORLD_SZ/2;
    int y = WORLD_SY-1; while (y > 0 && !world_solid(x, y, z)) --y;
    return v3(x + 0.5f, (float)(y + 1), z + 0.5f);
}

static void init(void) {
    stm_setup();
    sg_setup(&(sg_desc){ .environment = sglue_environment(), .logger.func = slog_func });
    sdtx_setup(&(sdtx_desc_t){ .fonts[0] = sdtx_font_c64(), .logger.func = slog_func });
    world_init(1337);
    render_init();
    player_init(spawn_point());
    g.last = stm_now();
    sapp_lock_mouse(true);
    g.locked = 1;
}

static void hud(void) {
    sdtx_canvas(sapp_widthf()*0.5f, sapp_heightf()*0.5f); // 2x-scaled glyphs
    sdtx_origin(1.0f, 1.0f);
    sdtx_font(0);
    sdtx_color3b(255, 255, 255);
    vec3 p = player_eye();
    sdtx_printf("FPS %.0f\n", g.fps);
    sdtx_printf("XYZ %.1f %.1f %.1f", p.x, p.y, p.z);
}

static void frame(void) {
    float dt = (float)stm_sec(stm_laptime(&g.last));
    if (dt > 0.1f) dt = 0.1f;
    g.fps += ((dt > 0 ? 1.0f/dt : 0.0f) - g.fps) * 0.1f; // exponential smoothing

    player_update(&g.in, dt);
    g.in.mouse_dx = g.in.mouse_dy = 0.0f;

    float aspect = sapp_widthf() / sapp_heightf();
    mat4 view_proj = m4_mul(m4_perspective(1.2f, aspect, 0.1f, 1000.0f), player_view());
    hud();

    sg_begin_pass(&(sg_pass){
        .action.colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = {0.52f,0.70f,0.95f,1.0f} },
        .swapchain = sglue_swapchain(),
    });
    render_frame(view_proj);
    sdtx_draw();
    sg_end_pass();
    sg_commit();
}

// Break (left) or place (right) the block the player is looking at.
static void edit_block(int place) {
    world_hit h = world_raycast(player_eye(), player_forward(), 8.0f);
    if (!h.hit) return;
    if (place) { world_set_block(h.px, h.py, h.pz, BLOCK_STONE); render_after_edit(h.px, h.pz); }
    else       { world_set_block(h.bx, h.by, h.bz, BLOCK_AIR);   render_after_edit(h.bx, h.bz); }
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
        case SAPP_KEYCODE_LEFT_SHIFT:  g.in.down    = down; break;
        case SAPP_KEYCODE_LEFT_CONTROL:g.in.sprint  = down; break;
        case SAPP_KEYCODE_F:           if (down) player_toggle_fly(); break;
        case SAPP_KEYCODE_ESCAPE:      if (down) { g.locked = 0; sapp_lock_mouse(false); } break;
        default: break;
        }
        break;
    }
    case SAPP_EVENTTYPE_MOUSE_MOVE:
        if (g.locked) { g.in.mouse_dx += e->mouse_dx; g.in.mouse_dy += e->mouse_dy; }
        break;
    case SAPP_EVENTTYPE_MOUSE_DOWN:
        if (!g.locked) { g.locked = 1; sapp_lock_mouse(true); }
        else edit_block(e->mouse_button == SAPP_MOUSEBUTTON_RIGHT);
        break;
    default: break;
    }
}

static void cleanup(void) {
    render_shutdown();
    sdtx_shutdown();
    sg_shutdown();
}

sapp_desc sokol_main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init, .frame_cb = frame, .event_cb = event, .cleanup_cb = cleanup,
        .width = 1280, .height = 720,
        .sample_count = 4,
        .window_title = "MinecraftClone",
        .logger.func = slog_func,
    };
}

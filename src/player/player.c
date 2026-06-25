#include "player.h"

#define MOVE_SPEED   12.0f   // blocks/second
#define SPRINT_MULT  2.5f
#define LOOK_SENS    0.0025f // radians per mouse pixel
#define PITCH_LIMIT  1.553f  // ~89 degrees, avoid gimbal flip

static struct { vec3 pos; float yaw, pitch; } g_cam;

static vec3 forward_dir(void) {
    float cp = cosf(g_cam.pitch);
    return v3(cosf(g_cam.yaw)*cp, sinf(g_cam.pitch), sinf(g_cam.yaw)*cp);
}

void player_init(vec3 start_pos) {
    g_cam.pos = start_pos;
    g_cam.yaw = -1.57f; // face -z
    g_cam.pitch = -0.3f;
}

void player_update(const player_input *in, float dt) {
    g_cam.yaw   += in->mouse_dx * LOOK_SENS;
    g_cam.pitch -= in->mouse_dy * LOOK_SENS;
    if (g_cam.pitch >  PITCH_LIMIT) g_cam.pitch =  PITCH_LIMIT;
    if (g_cam.pitch < -PITCH_LIMIT) g_cam.pitch = -PITCH_LIMIT;

    vec3 fwd = forward_dir();
    vec3 right = v3_norm(v3_cross(fwd, v3(0, 1, 0)));
    vec3 wish = v3(0, 0, 0);
    wish = v3_add(wish, v3_scale(fwd,   (float)(in->forward - in->back)));
    wish = v3_add(wish, v3_scale(right, (float)(in->right - in->left)));
    wish = v3_add(wish, v3_scale(v3(0,1,0), (float)(in->up - in->down)));

    float speed = MOVE_SPEED * (in->sprint ? SPRINT_MULT : 1.0f);
    g_cam.pos = v3_add(g_cam.pos, v3_scale(v3_norm(wish), speed * dt));
}

mat4 player_view(void) {
    return m4_lookat(g_cam.pos, v3_add(g_cam.pos, forward_dir()), v3(0, 1, 0));
}

vec3 player_eye(void) { return g_cam.pos; }

#include "player.h"
#include "world.h"

#define HW         0.3f    // player half-width (x/z)
#define PH         1.8f    // player height
#define EYE        1.62f   // eye height above feet
#define WALK       5.0f    // blocks/second
#define SPRINT     1.8f    // sprint multiplier
#define BOOST      4.0f    // Q-held burst multiplier (fast fly / fast run for exploring)
#define FLY_SPEED  14.0f
#define GRAVITY    28.0f
#define JUMP_VEL   8.4f    // ~1.25 block jump
#define SWIM_SPEED 4.0f    // vertical swim / max sink speed in water
#define LOOK_SENS  0.0025f
#define PITCH_LIM  1.553f

static struct {
    vec3 pos;            // feet position (x/z centred, y at the bottom)
    vec3 vel;
    float yaw, pitch;
    int on_ground, fly;
} p;

static vec3 look_dir(void) {
    float cp = cosf(p.pitch);
    return v3(cosf(p.yaw)*cp, sinf(p.pitch), sinf(p.yaw)*cp);
}

// True if the player AABB at feet-position `q` overlaps any solid voxel.
static int collide(vec3 q) {
    for (int x = (int)floorf(q.x-HW); x <= (int)floorf(q.x+HW); ++x)
    for (int y = (int)floorf(q.y);    y <= (int)floorf(q.y+PH); ++y)
    for (int z = (int)floorf(q.z-HW); z <= (int)floorf(q.z+HW); ++z)
        if (world_solid(x, y, z)) return 1;
    return 0;
}

void player_init(vec3 feet_start) {
    p.pos = feet_start; p.vel = v3(0,0,0);
    p.yaw = -1.57f; p.pitch = -0.2f; p.on_ground = 0; p.fly = 0;
}

void player_toggle_fly(void) { p.fly = !p.fly; p.vel.y = 0; }

void player_update(const player_input *in, float dt) {
    p.yaw   += in->mouse_dx * LOOK_SENS;
    p.pitch -= in->mouse_dy * LOOK_SENS;
    if (p.pitch >  PITCH_LIM) p.pitch =  PITCH_LIM;
    if (p.pitch < -PITCH_LIM) p.pitch = -PITCH_LIM;

    // Horizontal wish direction is yaw-only so looking up/down never slows you.
    vec3 flat = v3_norm(v3(cosf(p.yaw), 0, sinf(p.yaw)));
    vec3 right = v3_norm(v3_cross(flat, v3(0,1,0)));
    vec3 wish = v3_add(v3_scale(flat, (float)(in->forward - in->back)),
                       v3_scale(right, (float)(in->right - in->left)));
    wish = v3_norm(wish);

    // Submerged when a solid block of water sits at chest height — swim instead of walk.
    int wet = world_block((int)floorf(p.pos.x), (int)floorf(p.pos.y + 1.0f), (int)floorf(p.pos.z)) == BLOCK_WATER;

    float boost = in->boost ? BOOST : 1.0f;
    float speed = (p.fly ? FLY_SPEED : WALK) * (in->sprint ? SPRINT : 1.0f) * boost;
    if (wet && !p.fly) speed *= 0.6f;     // water resistance
    p.vel.x = wish.x * speed;
    p.vel.z = wish.z * speed;

    if (p.fly) {
        p.vel.y = (float)(in->up - in->down) * FLY_SPEED * boost;
    } else if (wet) {
        p.vel.y -= GRAVITY * 0.18f * dt;                       // buoyant: slow sink
        if (in->up)   p.vel.y = SWIM_SPEED;                    // swim up
        if (in->down) p.vel.y = -SWIM_SPEED;                   // dive
        p.vel.y *= 0.82f;                                      // drag
        if (p.vel.y < -SWIM_SPEED) p.vel.y = -SWIM_SPEED;
    } else {
        p.vel.y -= GRAVITY * dt;
        if (in->up && p.on_ground) { p.vel.y = JUMP_VEL; p.on_ground = 0; }
    }

    // Integrate per-axis, reverting the axis on collision (cheap swept AABB).
    vec3 np = p.pos;
    np.x += p.vel.x * dt; if (collide(np)) { np.x = p.pos.x; p.vel.x = 0; }
    np.z += p.vel.z * dt; if (collide(np)) { np.z = p.pos.z; p.vel.z = 0; }
    np.y += p.vel.y * dt;
    if (collide(np)) { p.on_ground = (p.vel.y <= 0); np.y = p.pos.y; p.vel.y = 0; }
    else if (!p.fly) p.on_ground = 0;
    p.pos = np;
}

vec3 player_eye(void)     { return v3(p.pos.x, p.pos.y + EYE, p.pos.z); }
vec3 player_forward(void) { return look_dir(); }
mat4 player_view(void)    { vec3 e = player_eye(); return m4_lookat(e, v3_add(e, look_dir()), v3(0,1,0)); }

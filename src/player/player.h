// Player module: walking FPS controller with gravity, jumping and voxel collision,
// plus a creative-style fly toggle. Input-agnostic — host fills player_input each frame.
// Depends on core (hmath) + world (collision queries).
#ifndef PLAYER_H
#define PLAYER_H

#include "hmath.h"

typedef struct {
    int   forward, back, left, right; // movement keys held (0/1)
    int   up, down;                   // jump / ascend, crouch / descend
    int   sprint;                     // speed multiplier
    float mouse_dx, mouse_dy;         // mouse delta accumulated this frame
} player_input;

void player_init(vec3 feet_start);
void player_update(const player_input *in, float dt);
void player_toggle_fly(void);
mat4 player_view(void);     // view matrix from the eye
vec3 player_eye(void);      // eye world position (for raycasting + HUD)
vec3 player_forward(void);  // look direction (for raycasting)

#endif // PLAYER_H

// Player module: free-fly FPS camera. Input-agnostic — the host fills player_input
// each frame so this stays decoupled from sokol/windowing. Depends only on core/hmath.
#ifndef PLAYER_H
#define PLAYER_H

#include "hmath.h"

typedef struct {
    int   forward, back, left, right, up, down; // movement keys currently held (0/1)
    int   sprint;                               // speed multiplier toggle
    float mouse_dx, mouse_dy;                   // mouse delta accumulated this frame
} player_input;

void player_init(vec3 start_pos);
void player_update(const player_input *in, float dt);
mat4 player_view(void);   // current view matrix
vec3 player_eye(void);    // current eye position

#endif // PLAYER_H

// Minimal column-major (OpenGL-style) vector/matrix math. Header-only, all static
// inline so it inlines fully and adds zero link cost. Only what the game uses.
#ifndef HMATH_H
#define HMATH_H

#include <math.h>

typedef struct { float x, y, z; } vec3;
typedef struct { float m[16]; } mat4; // column-major: m[col*4 + row]

static inline vec3 v3(float x, float y, float z) { return (vec3){ x, y, z }; }
static inline vec3 v3_add(vec3 a, vec3 b)   { return v3(a.x+b.x, a.y+b.y, a.z+b.z); }
static inline vec3 v3_sub(vec3 a, vec3 b)   { return v3(a.x-b.x, a.y-b.y, a.z-b.z); }
static inline vec3 v3_scale(vec3 a, float s){ return v3(a.x*s, a.y*s, a.z*s); }
static inline float v3_dot(vec3 a, vec3 b)  { return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline vec3 v3_cross(vec3 a, vec3 b)  { return v3(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x); }
static inline vec3 v3_norm(vec3 a) { float l = sqrtf(v3_dot(a, a)); return l > 1e-6f ? v3_scale(a, 1.0f/l) : a; }

static inline mat4 m4_mul(mat4 a, mat4 b) {
    mat4 r;
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row)
            r.m[c*4+row] = a.m[0*4+row]*b.m[c*4+0] + a.m[1*4+row]*b.m[c*4+1]
                         + a.m[2*4+row]*b.m[c*4+2] + a.m[3*4+row]*b.m[c*4+3];
    return r;
}

// Right-handed perspective, NDC depth -1..1 (GL convention). fov in radians.
static inline mat4 m4_perspective(float fov, float aspect, float n, float f) {
    float t = 1.0f / tanf(fov * 0.5f);
    mat4 r = {0};
    r.m[0] = t/aspect; r.m[5] = t; r.m[10] = (f+n)/(n-f);
    r.m[11] = -1.0f;   r.m[14] = (2.0f*f*n)/(n-f);
    return r;
}

// Right-handed look-at view matrix.
static inline mat4 m4_lookat(vec3 eye, vec3 center, vec3 up) {
    vec3 fwd = v3_norm(v3_sub(center, eye));
    vec3 s   = v3_norm(v3_cross(fwd, up));
    vec3 u   = v3_cross(s, fwd);
    mat4 r = {0};
    r.m[0]=s.x; r.m[4]=s.y; r.m[8]=s.z;
    r.m[1]=u.x; r.m[5]=u.y; r.m[9]=u.z;
    r.m[2]=-fwd.x; r.m[6]=-fwd.y; r.m[10]=-fwd.z;
    r.m[12]=-v3_dot(s,eye); r.m[13]=-v3_dot(u,eye); r.m[14]=v3_dot(fwd,eye); r.m[15]=1.0f;
    return r;
}

#endif // HMATH_H

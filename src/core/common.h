// Common types, math and utilities shared by every module.
#ifndef COMMON_H
#define COMMON_H
#define UC_VERSION "5.1.0"   // shown on the title screen, in F3, the window title and crash logs

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

typedef uint8_t u8;   typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef int8_t s8;    typedef int16_t s16;  typedef int32_t s32;  typedef int64_t s64;
typedef float f32;    typedef double f64;

#define TICKRATE 60
#define DT (1.0f / TICKRATE)
#define PI_F 3.14159265358979f
#define DEG2RAD(d) ((d) * (PI_F / 180.0f))
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#define MINI(a, b) ((a) < (b) ? (a) : (b))
#define MAXI(a, b) ((a) > (b) ? (a) : (b))

static inline f32 clampf(f32 v, f32 lo, f32 hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline f32 lerpf(f32 a, f32 b, f32 t) { return a + (b - a) * t; }
static inline f32 signf(f32 v) { return (f32)((v > 0) - (v < 0)); }
static inline int floordiv(int a, int b) { return (a >= 0) ? a / b : -((-a + b - 1) / b); }
static inline int floormod(int a, int b) { int m = a % b; return m < 0 ? m + b : m; }
static inline f32 wrap_angle(f32 a) { while (a > PI_F) a -= 2 * PI_F; while (a < -PI_F) a += 2 * PI_F; return a; }
static inline f32 smoothstepf(f32 a, f32 b, f32 x) { f32 t = clampf((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); }

// ---------------------------------------------------------------- vectors
typedef struct { f32 x, y, z; } vec3;
typedef struct { int x, y, z; } ivec3;
typedef struct { f32 x, y, z, w; } vec4;

static inline vec3 v3(f32 x, f32 y, f32 z) { return (vec3){x, y, z}; }
static inline vec3 v3_add(vec3 a, vec3 b) { return (vec3){a.x + b.x, a.y + b.y, a.z + b.z}; }
static inline vec3 v3_sub(vec3 a, vec3 b) { return (vec3){a.x - b.x, a.y - b.y, a.z - b.z}; }
static inline vec3 v3_scale(vec3 a, f32 s) { return (vec3){a.x * s, a.y * s, a.z * s}; }
static inline f32 v3_dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline vec3 v3_cross(vec3 a, vec3 b) { return (vec3){a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
static inline f32 v3_len(vec3 a) { return sqrtf(v3_dot(a, a)); }
static inline f32 v3_dist(vec3 a, vec3 b) { return v3_len(v3_sub(a, b)); }
static inline vec3 v3_norm(vec3 a) { f32 l = v3_len(a); return l > 1e-6f ? v3_scale(a, 1.0f / l) : (vec3){0, 0, 0}; }
static inline vec3 v3_lerp(vec3 a, vec3 b, f32 t) { return (vec3){lerpf(a.x, b.x, t), lerpf(a.y, b.y, t), lerpf(a.z, b.z, t)}; }
static inline vec3 v3_min(vec3 a, vec3 b) { return (vec3){fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z)}; }
static inline vec3 v3_max(vec3 a, vec3 b) { return (vec3){fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z)}; }
static inline ivec3 iv3(int x, int y, int z) { return (ivec3){x, y, z}; }
static inline ivec3 v3_floor(vec3 a) { return (ivec3){(int)floorf(a.x), (int)floorf(a.y), (int)floorf(a.z)}; }
static inline bool iv3_eq(ivec3 a, ivec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
static inline vec3 iv3_center(ivec3 p) { return v3(p.x + 0.5f, p.y + 0.5f, p.z + 0.5f); }

// ---------------------------------------------------------------- matrices (column-major, OpenGL style)
typedef struct { f32 m[16]; } mat4;
static inline mat4 m4_identity(void) { mat4 r = {{0}}; r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1; return r; }
static inline mat4 m4_mul(mat4 a, mat4 b) {
    mat4 r;
    for (int c = 0; c < 4; c++) for (int rr = 0; rr < 4; rr++) {
        f32 s = 0; for (int k = 0; k < 4; k++) s += a.m[k * 4 + rr] * b.m[c * 4 + k];
        r.m[c * 4 + rr] = s;
    }
    return r;
}
static inline mat4 m4_translate(vec3 t) { mat4 r = m4_identity(); r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z; return r; }
static inline mat4 m4_scale(vec3 s) { mat4 r = m4_identity(); r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z; return r; }
static inline mat4 m4_rot_x(f32 a) { mat4 r = m4_identity(); f32 c = cosf(a), s = sinf(a); r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c; return r; }
static inline mat4 m4_rot_y(f32 a) { mat4 r = m4_identity(); f32 c = cosf(a), s = sinf(a); r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c; return r; }
static inline mat4 m4_rot_z(f32 a) { mat4 r = m4_identity(); f32 c = cosf(a), s = sinf(a); r.m[0] = c; r.m[1] = s; r.m[4] = -s; r.m[5] = c; return r; }
static inline vec3 m4_mul_point(mat4 a, vec3 p) {
    return (vec3){a.m[0] * p.x + a.m[4] * p.y + a.m[8] * p.z + a.m[12],
                  a.m[1] * p.x + a.m[5] * p.y + a.m[9] * p.z + a.m[13],
                  a.m[2] * p.x + a.m[6] * p.y + a.m[10] * p.z + a.m[14]};
}
static inline vec3 m4_mul_dir(mat4 a, vec3 p) {
    return (vec3){a.m[0] * p.x + a.m[4] * p.y + a.m[8] * p.z,
                  a.m[1] * p.x + a.m[5] * p.y + a.m[9] * p.z,
                  a.m[2] * p.x + a.m[6] * p.y + a.m[10] * p.z};
}
static inline mat4 m4_perspective(f32 fovy, f32 aspect, f32 n, f32 f) {
    mat4 r = {{0}}; f32 t = 1.0f / tanf(fovy * 0.5f);
    r.m[0] = t / aspect; r.m[5] = t; r.m[10] = (f + n) / (n - f); r.m[11] = -1; r.m[14] = 2 * f * n / (n - f);
    return r;
}
static inline mat4 m4_ortho(f32 l, f32 r_, f32 b, f32 t, f32 n, f32 f) {
    mat4 r = m4_identity();
    r.m[0] = 2 / (r_ - l); r.m[5] = 2 / (t - b); r.m[10] = -2 / (f - n);
    r.m[12] = -(r_ + l) / (r_ - l); r.m[13] = -(t + b) / (t - b); r.m[14] = -(f + n) / (f - n);
    return r;
}
static inline mat4 m4_look_dir(vec3 eye, vec3 dir, vec3 up) {
    vec3 f = v3_norm(dir), s = v3_norm(v3_cross(f, up)), u = v3_cross(s, f);
    mat4 r = m4_identity();
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -v3_dot(s, eye); r.m[13] = -v3_dot(u, eye); r.m[14] = v3_dot(f, eye);
    return r;
}

// ---------------------------------------------------------------- axis aligned boxes
typedef struct { vec3 min, max; } AABB;
static inline bool aabb_overlap(AABB a, AABB b) {
    return a.min.x < b.max.x && a.max.x > b.min.x && a.min.y < b.max.y && a.max.y > b.min.y && a.min.z < b.max.z && a.max.z > b.min.z;
}
static inline AABB aabb_at(vec3 feet, f32 w, f32 h) {
    return (AABB){{feet.x - w * 0.5f, feet.y, feet.z - w * 0.5f}, {feet.x + w * 0.5f, feet.y + h, feet.z + w * 0.5f}};
}
static inline vec3 aabb_center(AABB a) { return v3_scale(v3_add(a.min, a.max), 0.5f); }
// Ray/box slab test; returns entry distance (0 if inside) or -1 when missed.
static inline f32 aabb_ray(AABB b, vec3 o, vec3 d) {
    f32 tmin = 0, tmax = 1e30f; f32 bo[3] = {o.x, o.y, o.z}, bd[3] = {d.x, d.y, d.z};
    f32 mn[3] = {b.min.x, b.min.y, b.min.z}, mx[3] = {b.max.x, b.max.y, b.max.z};
    for (int i = 0; i < 3; i++) {
        if (fabsf(bd[i]) < 1e-8f) { if (bo[i] < mn[i] || bo[i] > mx[i]) return -1; continue; }
        f32 t1 = (mn[i] - bo[i]) / bd[i], t2 = (mx[i] - bo[i]) / bd[i];
        if (t1 > t2) { f32 t = t1; t1 = t2; t2 = t; }
        if (t1 > tmin) tmin = t1;
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) return -1;
    }
    return tmin;
}

// ---------------------------------------------------------------- random numbers / hashing (deterministic, thread safe)
typedef struct { u64 s; } Rng;
static inline u32 rng_next(Rng *r) {
    r->s = r->s * 6364136223846793005ULL + 1442695040888963407ULL;
    u64 x = r->s; x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33;
    return (u32)(x >> 16);
}
static inline f32 rng_f(Rng *r) { return (rng_next(r) & 0xFFFFFF) / 16777216.0f; }
static inline f32 rng_range(Rng *r, f32 a, f32 b) { return a + (b - a) * rng_f(r); }
static inline int rng_int(Rng *r, int lo, int hi) { return hi <= lo ? lo : lo + (int)(rng_next(r) % (u32)(hi - lo + 1)); }
static inline u64 hash64(u64 x) { x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL; x ^= x >> 27; x *= 0x94d049bb133111ebULL; x ^= x >> 31; return x; }
static inline u64 hash3i(u64 seed, int x, int y, int z) {
    return hash64(seed ^ hash64((u64)(u32)x * 73856093ULL ^ hash64((u64)(u32)y * 19349663ULL ^ (u64)(u32)z * 83492791ULL)));
}
static inline f32 hashf(u64 seed, int x, int y, int z) { return (hash3i(seed, x, y, z) & 0xFFFFFF) / 16777216.0f; }

// Global gameplay RNG (main thread only).
extern Rng g_rng;
static inline f32 frand(void) { return rng_f(&g_rng); }
static inline f32 frand_range(f32 a, f32 b) { return rng_range(&g_rng, a, b); }
static inline int irand(int lo, int hi) { return rng_int(&g_rng, lo, hi); }

// ---------------------------------------------------------------- logging / memory
void log_msg(const char *level, const char *fmt, ...);
#define LOG_INFO(...) log_msg("INFO", __VA_ARGS__)
#define LOG_WARN(...) log_msg("WARN", __VA_ARGS__)
#define LOG_ERR(...) log_msg("ERROR", __VA_ARGS__)

void *xmalloc(size_t n);
void *xcalloc(size_t n, size_t sz);
void *xrealloc(void *p, size_t n);

#endif

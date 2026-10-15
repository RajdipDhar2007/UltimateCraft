#include "sky.h"
#include "rules.h"
#include "world.h"

static Texture tex_sun, tex_moon, tex_star, tex_clouds;
bool g_sky_clouds = true;   // Options -> Clouds

void sky_init(void) {
    tex_sun = texture_load("res/textures/sun.png");
    tex_moon = texture_load("res/textures/moon.png");
    tex_star = texture_load("res/textures/star.png");
    tex_clouds = texture_load("res/textures/clouds.png");
    glBindTexture(GL_TEXTURE_2D, tex_clouds.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

static f32 day_frac(void) { return (f32)(world_daytime() % DAY_TICKS) / DAY_TICKS; }

// smooth key-framed colours over the day (0 = sunrise, .25 noon, .5 sunset, .75 midnight)
static vec3 key_color(const vec3 *keys, f32 t) {
    static const f32 at[6] = {0.0f, 0.06f, 0.44f, 0.5f, 0.56f, 0.94f};
    for (int i = 0; i < 6; i++) {
        f32 a = at[i], b = i == 5 ? 1.0f : at[i + 1];
        if (t >= a && t <= b) return v3_lerp(keys[i], keys[(i + 1) % 6], (t - a) / (b - a));
    }
    return keys[0];
}
static const vec3 SKY_TOP[6] = {{0.36f, 0.42f, 0.66f}, {0.42f, 0.62f, 0.98f}, {0.42f, 0.62f, 0.98f}, {0.30f, 0.32f, 0.56f}, {0.02f, 0.02f, 0.06f}, {0.02f, 0.02f, 0.06f}};
static const vec3 SKY_HOR[6] = {{0.98f, 0.62f, 0.36f}, {0.70f, 0.82f, 1.0f}, {0.70f, 0.82f, 1.0f}, {0.98f, 0.52f, 0.28f}, {0.04f, 0.05f, 0.10f}, {0.04f, 0.05f, 0.10f}};

static vec3 rain_tint(vec3 c) {   // overcast: desaturate and darken toward grey
    f32 r = g_level.rain_level, th = g_level.thunder_level * r;
    f32 l = (c.x * 0.3f + c.y * 0.59f + c.z * 0.11f) * 0.6f;
    c = v3_lerp(c, v3(l, l, l), r * 0.75f);
    return v3_scale(c, 1.0f - th * 0.5f);
}
vec4 sky_fog_color(void) { vec3 c = rain_tint(key_color(SKY_HOR, day_frac())); return (vec4){c.x, c.y, c.z, 1}; }

static void celestial(vec3 cam, f32 angle, f32 size, f32 dist, GLuint tex, u32 col) {
    // a quad on a circle in the X/Y plane around the camera
    vec3 dir = v3(cosf(angle), sinf(angle), 0);
    vec3 c = v3_add(cam, v3_scale(dir, dist));
    vec3 u = v3(0, 0, size), r = v3_scale(v3(-sinf(angle), cosf(angle), 0), size);
    b3d_set_texture(tex);
    b3d_quad(v3_sub(v3_sub(c, r), u), v3_add(v3_sub(c, r), u), v3_add(v3_add(c, r), u), v3_sub(v3_add(c, r), u), 0, 0, 1, 1, col);
}

void sky_render(const mat4 *view, const mat4 *proj, vec3 cam, f32 far_dist) {
    f32 t = day_frac();
    vec3 top = rain_tint(key_color(SKY_TOP, t)), hor = rain_tint(key_color(SKY_HOR, t));
    {   // each biome tints the daytime sky with its own colour (blended smoothly while you walk)
        static vec3 bsky = {0.47f, 0.65f, 1.0f};
        int bi = world_biome_at((int)floorf(cam.x), (int)floorf(cam.y), (int)floorf(cam.z));
        if (bi >= 0) {
            u32 c = BIOMES[bi].sky;
            vec3 want = v3(((c >> 16) & 255) / 255.0f, ((c >> 8) & 255) / 255.0f, (c & 255) / 255.0f);
            bsky = v3_lerp(bsky, want, 0.02f);
        }
        f32 day = clampf((world_daylight() - 0.18f) / 0.82f, 0, 1) * (g_level.raining ? 0.5f : 1.0f);
        top = v3_lerp(top, v3_scale(bsky, 0.95f), 0.45f * day);
        hor = v3_lerp(hor, v3_lerp(bsky, v3(1, 1, 1), 0.55f), 0.25f * day);
    }
    mat4 vp = m4_mul(*proj, *view);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    b3d_begin(&vp, (vec4){0, 0, 0, 1}, 1e9f, 2e9f);
    b3d_set_texture(gfx_white_texture());
    // dome: 16 segments, horizon band to zenith and a dark band below the horizon
    f32 R = 400;
    u32 ctop = rgba_f(top.x, top.y, top.z, 1), chor = rgba_f(hor.x, hor.y, hor.z, 1), cbot = chor;
    for (int i = 0; i < 16; i++) {
        f32 a0 = i * PI_F / 8, a1 = (i + 1) * PI_F / 8;
        vec3 h0 = v3_add(cam, v3(cosf(a0) * R, 0, sinf(a0) * R)), h1 = v3_add(cam, v3(cosf(a1) * R, 0, sinf(a1) * R));
        vec3 m0 = v3_add(cam, v3(cosf(a0) * R * 0.7f, R * 0.35f, sinf(a0) * R * 0.7f)), m1 = v3_add(cam, v3(cosf(a1) * R * 0.7f, R * 0.35f, sinf(a1) * R * 0.7f));
        vec3 zen = v3_add(cam, v3(0, R, 0)), low0 = v3_add(h0, v3(0, -R, 0)), low1 = v3_add(h1, v3(0, -R, 0));
        vec3 q1[4] = {h0, h1, m1, m0}; f32 uv[8] = {0};
        Vert3D tmp; (void)tmp;
        // gradient: emit with per-quad colours approximated by two bands
        b3d_quad_uv(q1, uv, chor);
        vec3 q2[4] = {m0, m1, zen, zen};
        b3d_quad_uv(q2, uv, ctop);
        vec3 q3[4] = {low0, low1, h1, h0};
        b3d_quad_uv(q3, uv, cbot);
    }
    b3d_flush();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);   // additive celestial bodies
    f32 sun_angle = t * 2 * PI_F;
    f32 night = clampf((0.45f - world_daylight()) * 3.0f, 0, 1) * (1.0f - g_level.rain_level);
    if (night > 0.01f) {   // stars
        Rng r = {12345};
        b3d_set_texture(tex_star.id);
        for (int i = 0; i < 300; i++) {
            vec3 d = v3_norm(v3(rng_range(&r, -1, 1), rng_range(&r, 0.05f, 1), rng_range(&r, -1, 1)));
            mat4 rot = m4_rot_z(sun_angle);
            d = m4_mul_dir(rot, d);
            if (d.y < -0.05f) continue;
            vec3 c = v3_add(cam, v3_scale(d, 300));
            f32 s = rng_range(&r, 0.6f, 1.4f);
            vec3 up = v3_scale(v3_norm(v3_cross(d, v3(1, 0, 0))), s), rt = v3_scale(v3_norm(v3_cross(d, up)), s);
            f32 tw = 0.75f + 0.25f * sinf(g_world.time * 0.05f + i);
            b3d_quad(v3_sub(v3_sub(c, rt), up), v3_add(v3_sub(c, rt), up), v3_add(v3_add(c, rt), up), v3_sub(v3_add(c, rt), up), 0, 0, 1, 1, rgba_f(1, 1, 1, night * tw));
        }
    }
    f32 vis = 1.0f - g_level.rain_level;   // the sun and moon fade behind rain clouds
    celestial(cam, sun_angle, 30, 300, tex_sun.id, rgba_f(1, 1, 0.95f, vis));
    celestial(cam, sun_angle + PI_F, 22, 300, tex_moon.id, rgba_f(0.95f, 0.95f, 1, vis));
    b3d_flush();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // clouds: one big scrolling textured plane at y=110, faded with distance by fog (Options: Clouds ON/OFF)
    if (!g_sky_clouds) { glDepthMask(GL_TRUE); glEnable(GL_CULL_FACE); return; }
    glDepthMask(GL_TRUE);
    b3d_begin(&vp, sky_fog_color(), far_dist * 0.6f, far_dist * 1.6f);
    b3d_set_texture(tex_clouds.id);
    f32 cs = 1024, cy = MCY(192) + 0.33f, scroll = (f32)(g_world.time % (DAY_TICKS * 4)) / (DAY_TICKS * 4) * 4.0f;
    f32 u0 = cam.x / cs * 0.5f + scroll, v0 = cam.z / cs * 0.5f;
    vec3 cc = v3(cam.x, cy, cam.z);
    f32 dl = world_daylight();
    u32 ccol = rgba_f(0.35f + 0.65f * dl, 0.35f + 0.65f * dl, 0.4f + 0.6f * dl, 0.8f);
    vec3 p[4] = {v3_add(cc, v3(-cs, 0, -cs)), v3_add(cc, v3(cs, 0, -cs)), v3_add(cc, v3(cs, 0, cs)), v3_add(cc, v3(-cs, 0, cs))};
    f32 uv[8] = {u0, v0, u0 + 4, v0, u0 + 4, v0 + 4, u0, v0 + 4};
    b3d_quad_uv(p, uv, ccol);
    b3d_flush();
    glEnable(GL_CULL_FACE);
    (void)far_dist;
}

// ------------------------------------------------------------------------------- Space & the Sift
static void billboard(vec3 cam, vec3 dir, f32 dist, f32 size, u32 col) {
    vec3 c = v3_add(cam, v3_scale(dir, dist));
    vec3 ref = fabsf(dir.y) > 0.95f ? v3(1, 0, 0) : v3(0, 1, 0);
    vec3 up = v3_scale(v3_norm(v3_cross(dir, ref)), size), rt = v3_scale(v3_norm(v3_cross(dir, up)), size);
    b3d_quad(v3_sub(v3_sub(c, rt), up), v3_add(v3_sub(c, rt), up), v3_add(v3_add(c, rt), up), v3_sub(v3_add(c, rt), up), 0, 0, 1, 1, col);
}
void sky_render_space(const mat4 *view, const mat4 *proj, vec3 cam) {
    mat4 vp = m4_mul(*proj, *view);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    b3d_begin(&vp, (vec4){0, 0, 0, 1}, 1e9f, 2e9f);
    // faint nebula clouds
    Rng r = {777};
    b3d_set_texture(tex_clouds.id);
    for (int i = 0; i < 10; i++) {
        vec3 d = v3_norm(v3(rng_range(&r, -1, 1), rng_range(&r, -1, 1), rng_range(&r, -1, 1)));
        f32 hue = rng_f(&r);
        u32 col = rgba_f(0.35f + 0.4f * hue, 0.15f + 0.2f * (1 - hue), 0.55f + 0.3f * hue, 0.10f);
        billboard(cam, d, 320, rng_range(&r, 60, 140), col);
    }
    // stars in every direction (the sky is a full sphere in space), slowly twinkling
    b3d_set_texture(tex_star.id);
    for (int i = 0; i < 2200; i++) {   // tuned in v5: bigger and brighter, a few bright giants with a soft glow
        vec3 d = v3_norm(v3(rng_range(&r, -1, 1), rng_range(&r, -1, 1), rng_range(&r, -1, 1)));
        f32 s = rng_range(&r, 0.7f, 2.0f), tint = rng_f(&r);
        if (i % 18 == 0) { s = rng_range(&r, 2.6f, 4.0f); billboard(cam, d, 301, s * 3.2f, rgba_f(0.75f, 0.8f, 1, 0.22f)); }
        f32 tw = 0.85f + 0.15f * sinf(g_world.time * 0.03f + i * 1.7f);
        u32 col = tint < 0.15f ? rgba_f(1, 0.75f, 0.6f, tw) : tint < 0.3f ? rgba_f(0.7f, 0.8f, 1, tw) : rgba_f(1, 1, 1, tw);
        billboard(cam, d, 300, s, col);
    }
    // a distant ringed planet and a small blue moon
    b3d_set_texture(tex_moon.id);
    billboard(cam, v3_norm(v3(0.6f, 0.35f, -0.7f)), 300, 34, rgba_f(0.9f, 0.6f, 0.45f, 1));
    billboard(cam, v3_norm(v3(-0.5f, 0.55f, 0.4f)), 300, 9, rgba_f(0.55f, 0.75f, 1.0f, 1));
    b3d_set_texture(gfx_white_texture());
    {   vec3 pc = v3_add(cam, v3_scale(v3_norm(v3(0.6f, 0.35f, -0.7f)), 299));
        vec3 a = v3(52, 6, 14), b = v3(-14, 3, 52);
        for (int k = 0; k < 24; k++) {   // ring: a thin ellipse of quads
            f32 t0 = k * 2 * PI_F / 24, t1 = (k + 1) * 2 * PI_F / 24;
            vec3 p0 = v3_add(pc, v3_add(v3_scale(a, cosf(t0)), v3_scale(b, sinf(t0)))), p1 = v3_add(pc, v3_add(v3_scale(a, cosf(t1)), v3_scale(b, sinf(t1))));
            vec3 q0 = v3_add(pc, v3_scale(v3_sub(p0, pc), 0.8f)), q1 = v3_add(pc, v3_scale(v3_sub(p1, pc), 0.8f));
            vec3 q[4] = {p0, p1, q1, q0}; f32 uv[8] = {0};
            b3d_quad_uv(q, uv, rgba_f(0.8f, 0.7f, 0.55f, 0.35f));
        }
    }
    b3d_flush();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
}
void sky_render_sift(const mat4 *view, const mat4 *proj, vec3 cam, f32 far_dist) {
    mat4 vp = m4_mul(*proj, *view);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    b3d_begin(&vp, (vec4){0, 0, 0, 1}, 1e9f, 2e9f);
    b3d_set_texture(gfx_white_texture());
    f32 R = 400;
    u32 ctop = rgba_f(0.30f, 0.55f, 0.98f, 1), chor = rgba_f(0.86f, 0.90f, 0.98f, 1), cbot = rgba_f(1, 1, 1, 1);
    for (int i = 0; i < 16; i++) {
        f32 a0 = i * PI_F / 8, a1 = (i + 1) * PI_F / 8;
        vec3 h0 = v3_add(cam, v3(cosf(a0) * R, 0, sinf(a0) * R)), h1 = v3_add(cam, v3(cosf(a1) * R, 0, sinf(a1) * R));
        vec3 m0 = v3_add(cam, v3(cosf(a0) * R * 0.7f, R * 0.35f, sinf(a0) * R * 0.7f)), m1 = v3_add(cam, v3(cosf(a1) * R * 0.7f, R * 0.35f, sinf(a1) * R * 0.7f));
        vec3 zen = v3_add(cam, v3(0, R, 0)), low0 = v3_add(h0, v3(0, -R, 0)), low1 = v3_add(h1, v3(0, -R, 0));
        f32 uv[8] = {0};
        vec3 q1[4] = {h0, h1, m1, m0}; b3d_quad_uv(q1, uv, chor);
        vec3 q2[4] = {m0, m1, zen, zen}; b3d_quad_uv(q2, uv, ctop);
        vec3 q3[4] = {low0, low1, h1, h0}; b3d_quad_uv(q3, uv, cbot);
    }
    b3d_flush();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    b3d_set_texture(tex_sun.id);
    vec3 sd = v3_norm(v3(0.3f, 0.85f, -0.4f));
    billboard(cam, sd, 300, 46, rgba_f(1, 0.95f, 0.75f, 0.55f));   // a halo around a soft, never-setting sun
    billboard(cam, sd, 300, 24, rgba_f(1, 1, 0.92f, 1));
    b3d_flush();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // a slow cloud layer far below the islands
    glDepthMask(GL_TRUE);
    b3d_begin(&vp, (vec4){1, 1, 1, 1}, far_dist * 0.6f, far_dist * 1.6f);
    b3d_set_texture(tex_clouds.id);
    f32 cs = 1024, cy = 40.33f, scroll = (f32)(g_world.time % (DAY_TICKS * 8)) / (DAY_TICKS * 8) * 4.0f;
    f32 u0 = cam.x / cs * 0.5f + scroll, v0 = cam.z / cs * 0.5f;
    vec3 cc = v3(cam.x, cy, cam.z);
    vec3 p[4] = {v3_add(cc, v3(-cs, 0, -cs)), v3_add(cc, v3(cs, 0, -cs)), v3_add(cc, v3(cs, 0, cs)), v3_add(cc, v3(-cs, 0, cs))};
    f32 uv[8] = {u0, v0, u0 + 4, v0, u0 + 4, v0 + 4, u0, v0 + 4};
    b3d_quad_uv(p, uv, rgba_f(1, 1, 1, 0.9f));
    b3d_flush();
    glEnable(GL_CULL_FACE);
}

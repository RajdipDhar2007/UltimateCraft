#include "fx.h"
#include "../world/world.h"
#include "../entity/entity.h"

#define MAX_PARTICLES 4096
typedef struct {
    vec3 pos, prev, vel;
    f32 size, gravity, drag;
    int life, max_life;
    u32 color;
    u8 atlas;          // 0 = block atlas sub-rectangle, 1 = particles.png cell
    f32 u0, v0, du, dv;
    u8 anim;           // smoke: cell shrinks with age
    bool fade, collide;
} Particle;
static Particle P[MAX_PARTICLES];
static int np;
static f32 shake;

void fx_init(void) { np = 0; }
void fx_clear(void) { np = 0; shake = 0; }
void fx_shake(f32 a) { shake = fmaxf(shake, a); }
f32 fx_shake_amount(void) { return shake; }

static Particle *spawn(vec3 pos, vec3 vel, int life) {
    if (np >= MAX_PARTICLES) { P[irand(0, MAX_PARTICLES - 1)] = P[--np]; }
    Particle *p = &P[np++];
    memset(p, 0, sizeof *p);
    p->pos = p->prev = pos; p->vel = vel; p->life = p->max_life = life;
    p->size = 0.1f; p->color = COL_WHITE; p->drag = 0.98f; p->collide = true;
    return p;
}
static void set_cell(Particle *p, int cell) { p->atlas = 1; p->u0 = (cell % 8) / 8.0f; p->v0 = (cell / 8) / 8.0f; p->du = p->dv = 1 / 8.0f; }

static void block_debris(vec3 pos, vec3 vel, u16 block, f32 light) {
    int tile = BLOCKS[block].tex[FACE_S];
    if (BLOCKS[block].mesh == MESH_CUBE && BLOCKS[block].base == B_GRASS_BLOCK) tile = BLOCKS[block].tex[FACE_D];
    Particle *p = spawn(pos, vel, irand(20, 50));
    p->size = frand_range(0.06f, 0.12f);
    p->gravity = 20; p->atlas = 0;
    f32 sx = irand(0, 12) / 1024.0f, sy = irand(0, 12) / 1024.0f;
    p->u0 = (tile % ATLAS_TILES) / (f32)ATLAS_TILES + sx; p->v0 = (tile / ATLAS_TILES) / (f32)ATLAS_TILES + sy; p->du = p->dv = 4 / 1024.0f;
    p->color = rgba_f(light, light, light, 1);
}

static f32 light_at(vec3 p) {
    u16 l = world_get_light((int)floorf(p.x), (int)floorf(p.y), (int)floorf(p.z));
    f32 sky = L_SKY(l) / 15.0f * world_daylight();
    f32 blk = MAXI(L_R(l), L_G(l)) / 15.0f;
    return fmaxf(0.15f, fmaxf(sky, blk));
}

void fx_block_break(ivec3 b, u16 block) {
    if (!block) return;
    f32 l = light_at(iv3_center(b));
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) for (int k = 0; k < 4; k++) {
        if (frand() < 0.5f) continue;
        vec3 o = v3(b.x + (i + 0.5f) / 4, b.y + (j + 0.5f) / 4, b.z + (k + 0.5f) / 4);
        vec3 v = v3_scale(v3_sub(o, iv3_center(b)), frand_range(3, 6));
        v.y += frand_range(1, 3);
        block_debris(o, v, block, l);
    }
}
void fx_block_hit(ivec3 b, int face, u16 block) {
    if (!block) return;
    f32 l = light_at(iv3_center(b));
    ivec3 n = FACE_DIR[face];
    for (int i = 0; i < 2; i++) {
        vec3 o = v3(b.x + frand(), b.y + frand(), b.z + frand());
        if (n.x) o.x = b.x + (n.x > 0 ? 1.05f : -0.05f);
        if (n.y) o.y = b.y + (n.y > 0 ? 1.05f : -0.05f);
        if (n.z) o.z = b.z + (n.z > 0 ? 1.05f : -0.05f);
        block_debris(o, v3(n.x * 1.5f + frand_range(-0.5f, 0.5f), n.y * 1.5f + frand_range(0, 1), n.z * 1.5f + frand_range(-0.5f, 0.5f)), block, l);
    }
}
void fx_smoke(vec3 pos, int n, f32 spread, u32 color) {
    for (int i = 0; i < n; i++) {
        Particle *p = spawn(v3_add(pos, v3(frand_range(-spread, spread), frand_range(-spread, spread), frand_range(-spread, spread))),
                            v3(frand_range(-0.3f, 0.3f), frand_range(0.5f, 1.5f), frand_range(-0.3f, 0.3f)), irand(30, 70));
        p->size = frand_range(0.12f, 0.25f); p->gravity = -0.5f; p->anim = 1; p->collide = false;
        f32 g = frand_range(0.6f, 0.95f);
        p->color = rgba_u32((int)((color & 255) * g), (int)(((color >> 8) & 255) * g), (int)(((color >> 16) & 255) * g), 255);
        set_cell(p, 7);
    }
}
void fx_explosion(vec3 pos, f32 r) {
    for (int i = 0; i < (int)(r * 30); i++) {
        vec3 d = v3_norm(v3(frand_range(-1, 1), frand_range(-1, 1), frand_range(-1, 1)));
        Particle *p = spawn(v3_add(pos, v3_scale(d, frand() * r * 0.6f)), v3_scale(d, frand_range(2, 9)), irand(20, 60));
        p->size = frand_range(0.3f, 0.8f); p->drag = 0.9f; p->gravity = -1; p->anim = 1; p->collide = false;
        f32 g = frand_range(0.55f, 1.0f);
        p->color = rgba_f(g, g, g, 1);
        set_cell(p, i % 3 == 0 ? 15 : 7);
    }
}
void fx_crit(vec3 pos, int n) {
    for (int i = 0; i < n; i++) {
        vec3 d = v3_norm(v3(frand_range(-1, 1), frand_range(-0.2f, 1), frand_range(-1, 1)));
        Particle *p = spawn(pos, v3_scale(d, frand_range(3, 7)), irand(15, 30));
        p->size = 0.08f; p->gravity = 6; p->drag = 0.9f; p->color = rgba_f(1, 0.9f, 0.6f, 1); p->fade = true;
        set_cell(p, 8);
    }
}
void fx_damage(vec3 pos, int n) {
    for (int i = 0; i < n; i++) {
        Particle *p = spawn(v3_add(pos, v3(frand_range(-0.3f, 0.3f), 0, frand_range(-0.3f, 0.3f))), v3(frand_range(-1, 1), frand_range(1, 2.5f), frand_range(-1, 1)), irand(25, 40));
        p->size = 0.12f; p->gravity = 4; p->drag = 0.95f; p->collide = false;
        set_cell(p, 14);
    }
}
void fx_hearts(vec3 pos) {
    for (int i = 0; i < 5; i++) {
        Particle *p = spawn(v3_add(pos, v3(frand_range(-0.4f, 0.4f), frand_range(0, 0.4f), frand_range(-0.4f, 0.4f))), v3(0, 1, 0), 50);
        p->size = 0.12f; p->collide = false;
        set_cell(p, 9);
    }
}
void fx_bubbles(vec3 pos, int n) {
    for (int i = 0; i < n; i++) {
        Particle *p = spawn(v3_add(pos, v3(frand_range(-0.2f, 0.2f), 0, frand_range(-0.2f, 0.2f))), v3(frand_range(-0.2f, 0.2f), frand_range(1, 2), frand_range(-0.2f, 0.2f)), irand(20, 50));
        p->size = 0.05f; p->gravity = -1; p->collide = false;
        set_cell(p, 11);
    }
}
void fx_splash(vec3 pos, int n) {
    for (int i = 0; i < n; i++) {
        Particle *p = spawn(v3_add(pos, v3(frand_range(-0.5f, 0.5f), 0.1f, frand_range(-0.5f, 0.5f))), v3(frand_range(-2, 2), frand_range(2, 5), frand_range(-2, 2)), irand(15, 30));
        p->size = 0.06f; p->gravity = 25;
        set_cell(p, 12);
    }
}
void fx_flame(vec3 pos) {
    Particle *p = spawn(pos, v3(frand_range(-0.1f, 0.1f), frand_range(0.5f, 1.2f), frand_range(-0.1f, 0.1f)), irand(12, 24));
    p->size = frand_range(0.08f, 0.14f); p->gravity = -0.5f; p->fade = true; p->collide = false;
    set_cell(p, 10);
}
void fx_poof(vec3 pos, f32 w, f32 h) {
    for (int i = 0; i < 20; i++) {
        vec3 o = v3_add(pos, v3(frand_range(-w, w) * 0.6f, frand() * h, frand_range(-w, w) * 0.6f));
        Particle *p = spawn(o, v3(frand_range(-0.6f, 0.6f), frand_range(0.3f, 1.2f), frand_range(-0.6f, 0.6f)), irand(20, 40));
        p->size = frand_range(0.15f, 0.3f); p->gravity = -0.3f; p->anim = 1; p->collide = false;
        set_cell(p, 7);
    }
}

void fx_tick(void) {
    shake *= 0.9f;
    if (shake < 0.001f) shake = 0;
    for (int i = 0; i < np;) {
        Particle *p = &P[i];
        if (--p->life <= 0) { P[i] = P[--np]; continue; }
        p->prev = p->pos;
        p->vel.y -= p->gravity * DT;
        p->vel = v3_scale(p->vel, powf(p->drag, 60.0f * DT / 3.0f));
        vec3 np_ = v3_add(p->pos, v3_scale(p->vel, DT));
        if (p->collide && BLOCKS[world_get_block((int)floorf(np_.x), (int)floorf(np_.y), (int)floorf(np_.z))].solid) {
            p->vel = v3(p->vel.x * 0.3f, 0, p->vel.z * 0.3f);
        } else p->pos = np_;
        i++;
    }
}

void fx_render(const mat4 *vp, vec3 R, vec3 U, f32 alpha, vec4 fog, f32 fn, f32 ff) {
    if (!np) return;
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    b3d_begin(vp, fog, fn, ff);
    for (int pass = 0; pass < 2; pass++) {
        b3d_set_texture(pass == 0 ? g_world.atlas.id : entity_particles_texture());
        for (int i = 0; i < np; i++) {
            Particle *p = &P[i];
            if (p->atlas != pass) continue;
            vec3 c = v3_lerp(p->prev, p->pos, alpha);
            f32 t = 1.0f - (f32)p->life / p->max_life;
            f32 s = p->size;
            f32 u0 = p->u0, v0 = p->v0, du = p->du, dv = p->dv;
            if (p->anim) { int cell = 7 - (int)(t * 7.99f); u0 = (cell % 8) / 8.0f; v0 = 0; s *= 1.0f + t * 0.5f; }
            u32 col = p->color;
            if (p->fade) col = (col & 0x00FFFFFFu) | ((u32)((1.0f - t) * 255) << 24);
            vec3 q[4] = {v3_add(c, v3_scale(v3_add(R, U), -s)), v3_add(c, v3_scale(v3_sub(R, U), s)),
                         v3_add(c, v3_scale(v3_add(R, U), s)), v3_add(c, v3_scale(v3_sub(U, R), s))};
            f32 uv[8] = {u0, v0 + dv, u0 + du, v0 + dv, u0 + du, v0, u0, v0};
            b3d_quad_uv(q, uv, col);
        }
        b3d_flush();
    }
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
}

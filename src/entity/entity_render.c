#include "entity.h"
#include "../game/game.h"
#include "vehicle.h"
#include "../ui/screens.h"
#include "model.h"
#include "../world/world.h"
#include "../game/player.h"

vec3 g_cam_pos, g_cam_right, g_cam_up, g_cam_fwd;
static Texture mob_tex[MOB_COUNT], tex_arrow, tex_items, tex_particles;

void entities_render_init(void) {
    for (int i = 0; i < MOB_COUNT; i++) mob_tex[i] = texture_load(MOB_DEFS[i].texture);
    tex_arrow = texture_load("res/textures/entity/arrow.png");
    tex_items = texture_load("res/textures/items.png");
    tex_particles = texture_load("res/textures/particles.png");
}
GLuint entity_items_texture(void) { return tex_items.id; }
GLuint entity_particles_texture(void) { return tex_particles.id; }

static u32 lit_color(f32 light, f32 r, f32 g, f32 b, f32 a) { return rgba_f(light * r, light * g, light * b, a); }

static u32 mul_col(u32 c, u32 tint, f32 s) {
    f32 tr = ((tint >> 16) & 255) / 255.0f, tg = ((tint >> 8) & 255) / 255.0f, tb = (tint & 255) / 255.0f;
    return rgba_u32((int)((c & 255) * s * tr), (int)(((c >> 8) & 255) * s * tg), (int)(((c >> 16) & 255) * s * tb), (int)(c >> 24));
}
// default (plains-like) colours used when a tinted block is drawn outside the world
u32 block_default_tint(const BlockDef *d, int face) {
    if (d->tint == TINT_NONE || !(d->tint_faces & (1 << face))) return 0xFFFFFF;
    switch (d->tint) {
    case TINT_GRASS: return 0x91BD59 * 1;
    case TINT_FOLIAGE: return 0x77AB2F;
    case TINT_WATER: return 0x3F76E4;
    case TINT_SPRUCE: return 0x619961;
    case TINT_BIRCH: return 0x80A755;
    default: return 0x208030;
    }
}
static void tile_uv(int t, f32 *u0, f32 *v0, f32 *du) { *u0 = (t % ATLAS_TILES) / (f32)ATLAS_TILES; *v0 = (t / ATLAS_TILES) / (f32)ATLAS_TILES; *du = 1.0f / ATLAS_TILES; }

static const u8 BFC[6][4][3] = {
    {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}}, {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}},
    {{1, 0, 1}, {0, 0, 1}, {0, 0, 0}, {1, 0, 0}}, {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}},
    {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}}, {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}};
static const f32 BSH[6] = {0.78f, 0.78f, 0.5f, 1.0f, 0.64f, 0.64f};

// one box (1/16 units, 0..16) of a block, centred so the full block spans -0.5..0.5
static const u32 DYE_COLORS_RENDER[16] = {0xF9FFFE, 0xF9801D, 0xC74EBD, 0x3AB3DA, 0xFED83D, 0x80C71F, 0xF38BAA, 0x474F52, 0x9D9D97, 0x169C9C, 0x8932B8, 0x3C44AA, 0x835432, 0x5E7C16, 0xB02E26, 0x1D1D21};
static void draw_block_box(mat4 m, const BlockDef *d, const f32 b[6], u32 color) {
    for (int f = 0; f < 6; f++) {
        f32 u0, v0, du; tile_uv(d->tex[f], &u0, &v0, &du);
        vec3 p[4]; f32 uv[8];
        for (int i = 0; i < 4; i++) {
            const u8 *c = BFC[f][i];
            f32 px = c[0] ? b[3] : b[0], py = c[1] ? b[4] : b[1], pz = c[2] ? b[5] : b[2];
            f32 tu, tv;
            switch (f) {
            case FACE_U: case FACE_D: tu = px; tv = pz; break;
            case FACE_N: tu = 16 - px; tv = 16 - py; break;
            case FACE_S: tu = px; tv = 16 - py; break;
            case FACE_W: tu = pz; tv = 16 - py; break;
            default: tu = 16 - pz; tv = 16 - py; break;
            }
            p[i] = m4_mul_point(m, v3(px / 16 - 0.5f, py / 16 - 0.5f, pz / 16 - 0.5f));
            uv[i * 2] = u0 + clampf(tu / 16, 0.002f, 0.998f) * du; uv[i * 2 + 1] = v0 + clampf(tv / 16, 0.002f, 0.998f) * du;
        }
        b3d_quad_uv(p, uv, mul_col(color, block_default_tint(d, f), BSH[f]));
    }
}

// Renders any block (cube, shaped, fence, plant ...) centred on the origin with size 1, transformed by m.
void render_block_cube(mat4 m, u16 block, u32 color) {
    const BlockDef *d = &BLOCKS[block];
    b3d_set_texture(g_world.atlas.id);
    switch (d->mesh) {
    case MESH_CROSS: case MESH_TORCH: case MESH_CROP: case MESH_FLAT: case MESH_WIRE: case MESH_RAIL: case MESH_LIQUID: {
        f32 u0, v0, du; tile_uv(d->tex[d->mesh == MESH_FLAT || d->mesh == MESH_RAIL ? FACE_U : FACE_S], &u0, &v0, &du);
        vec3 p[4] = {m4_mul_point(m, v3(-0.5f, -0.5f, 0)), m4_mul_point(m, v3(0.5f, -0.5f, 0)), m4_mul_point(m, v3(0.5f, 0.5f, 0)), m4_mul_point(m, v3(-0.5f, 0.5f, 0))};
        f32 uv[8] = {u0, v0 + du, u0 + du, v0 + du, u0 + du, v0, u0, v0};
        b3d_quad_uv(p, uv, mul_col(color, block_default_tint(d, FACE_U), 1));
        return;
    }
    case MESH_BOXES: {
        const BlockShape *s = &SHAPES[d->shape];
        for (int i = 0; i < s->n; i++) { f32 b[6]; for (int k = 0; k < 6; k++) b[k] = s->b[i][k]; if (b[4] <= b[1]) b[4] = b[1] + 0.3f; if (b[3] <= b[0]) b[3] = b[0] + 0.3f; if (b[5] <= b[2]) b[5] = b[2] + 0.3f; draw_block_box(m, d, b, color); }
        return;
    }
    case MESH_CONNECT: {   // post with arms toward east and west
        const BlockShape *s = &SHAPES[d->shape];
        f32 b[6]; for (int k = 0; k < 6; k++) b[k] = s->b[0][k];
        draw_block_box(m, d, b, color);
        for (int a = 1; a < s->n; a++) for (int side = 0; side < 2; side++) {
            const u8 *q = s->b[a];   // north arm -> rotate to E (1) and W (3)
            f32 x0 = q[0], z0 = q[2], x1 = q[3], z1 = q[5];
            int k = side ? 3 : 1;
            for (int i = 0; i < k; i++) { f32 nx0 = 16 - z1, nz0 = x0, nx1 = 16 - z0, nz1 = x1; x0 = nx0; z0 = nz0; x1 = nx1; z1 = nz1; }
            f32 bb[6] = {fminf(x0, x1), q[1], fminf(z0, z1), fmaxf(x0, x1), q[4], fmaxf(z0, z1)};
            draw_block_box(m, d, bb, color);
        }
        return;
    }
    case MESH_CACTUS: { f32 b[6] = {1, 0, 1, 15, 16, 15}; draw_block_box(m, d, b, color); return; }
    default: { f32 b[6] = {0, 0, 0, 16, 16, 16}; draw_block_box(m, d, b, color); return; }
    }
}

static void item_icon_uv(int ic, f32 *u0, f32 *v0, f32 *d) { *u0 = (ic % 64) / 64.0f; *v0 = (ic / 64) / 64.0f; *d = 1 / 64.0f; }
static void sprite_quad(mat4 m, f32 u0, f32 v0, f32 du, u32 color) {
    vec3 p[4] = {m4_mul_point(m, v3(-0.5f, -0.5f, 0)), m4_mul_point(m, v3(0.5f, -0.5f, 0)), m4_mul_point(m, v3(0.5f, 0.5f, 0)), m4_mul_point(m, v3(-0.5f, 0.5f, 0))};
    f32 uv[8] = {u0, v0 + du, u0 + du, v0 + du, u0 + du, v0, u0, v0};
    b3d_quad_uv(p, uv, color);
}

// Flat item icon (double sided), centred, size 1. Block items use their block tile when flat.
void render_item_icon_sprite(mat4 m, ItemStack s, u32 color) {
    const ItemDef *it = &ITEMS[s.id];
    int ic = it->icon;
    if (ic == -2) {
        const BlockDef *d = &BLOCKS[it->block];
        f32 u0, v0, du; tile_uv(d->tex[d->mesh == MESH_FLAT || d->mesh == MESH_RAIL ? FACE_U : FACE_S], &u0, &v0, &du);
        b3d_set_texture(g_world.atlas.id);
        sprite_quad(m, u0, v0, du, mul_col(color, block_default_tint(d, FACE_U), 1));
        return;
    }
    if (ic < 0) { render_block_cube(m, it->block, color); return; }
    if (it->variants) ic += s.damage % it->variants;
    b3d_set_texture(tex_items.id);
    f32 u0, v0, du; item_icon_uv(ic, &u0, &v0, &du);
    sprite_quad(m, u0, v0, du, color);
}
void render_item_sprite(mat4 m, int item, u32 color) { render_item_icon_sprite(m, stack_of(item, 1), color); }

void render_stack(mat4 m, ItemStack s, u32 color) {
    if (ITEMS[s.id].icon == -1) render_block_cube(m4_mul(m, m4_scale(v3(0.5f, 0.5f, 0.5f))), ITEMS[s.id].block, color);
    else render_item_icon_sprite(m, s, color);
}

static void billboard(vec3 c, f32 s, ItemStack it, u32 col) {
    mat4 m = {{g_cam_right.x * s, g_cam_right.y * s, g_cam_right.z * s, 0, g_cam_up.x * s, g_cam_up.y * s, g_cam_up.z * s, 0,
               -g_cam_fwd.x, -g_cam_fwd.y, -g_cam_fwd.z, 0, c.x, c.y, c.z, 1}};
    render_item_icon_sprite(m, it, col);
}

static void shadow(vec3 p, f32 radius) {
    int x = (int)floorf(p.x), z = (int)floorf(p.z);
    for (int dy = 0; dy < 4; dy++) {
        int y = (int)floorf(p.y) - dy;
        u16 b = world_get_block(x, y - 1, z);
        if (BLOCKS[b].solid && BLOCKS[b].opaque) {
            f32 a = 0.45f * (1.0f - (p.y - y) / 4.0f);
            if (a <= 0) return;
            f32 gy = y + 0.015f;
            b3d_set_texture(tex_particles.id);
            f32 u0 = 7 / 8.0f, v0 = 0, d = 1 / 8.0f;
            vec3 q[4] = {v3(p.x - radius, gy, p.z + radius), v3(p.x + radius, gy, p.z + radius), v3(p.x + radius, gy, p.z - radius), v3(p.x - radius, gy, p.z - radius)};
            f32 uv[8] = {u0, v0 + d, u0 + d, v0 + d, u0 + d, v0, u0, v0};
            b3d_quad_uv(q, uv, rgba_f(0, 0, 0, a));
            return;
        }
    }
}

static inline f32 lerp_angle(f32 a, f32 b, f32 t) { return a + wrap_angle(b - a) * t; }

// pattern layers on placed banners (block entities): quads over both faces of the cloth
static void banners_render(const mat4 *vp, vec4 fog, f32 fn, f32 ff) {
    Entity *pl = player_ent();
    if (!pl) return;
    bool any = false;
    for (int i = 0; i < g_world.nbents; i++) {
        BlockEntity *be = &g_world.bents[i];
        if (be->type != BE_BANNER || stack_empty(be->slots[0]) || !be->slots[0].ench[0][0]) continue;
        if (v3_dist(iv3_center(be->pos), pl->pos) > 64) continue;
        Block b = world_get_block(be->pos.x, be->pos.y, be->pos.z);
        const BlockDef *d = &BLOCKS[b];
        if (d->be != BE_BANNER) continue;
        if (!any) { b3d_begin(vp, fog, fn, ff); b3d_set_texture(banner_pattern_texture()); any = true; }
        const BlockShape *sh = &SHAPES[d->shape];
        const u8 *c = sh->b[sh->n - 1];   // cloth box
        bool zthin = (c[5] - c[2]) < (c[3] - c[0]);
        Entity probe; memset(&probe, 0, sizeof probe); probe.kind = ENT_ITEM; probe.pos = v3(be->pos.x + 0.5f, (f32)be->pos.y, be->pos.z + 0.5f);
        f32 L = entity_light(&probe);
        for (int k = 0; k < MAX_ENCH; k++) {
            int pat = be->slots[0].ench[k][0], colr = be->slots[0].ench[k][1];
            if (!pat) continue;
            u32 dc = DYE_COLORS_RENDER[colr & 15];
            u32 col = rgba_f(((dc >> 16) & 255) / 255.0f * L, ((dc >> 8) & 255) / 255.0f * L, (dc & 255) / 255.0f * L, 1);
            f32 u0 = pat * 20 / 640.0f, u1 = (pat * 20 + 20) / 640.0f;
            for (int side = 0; side < 2; side++) {
                f32 off = (side ? 1 : -1) * (0.003f + k * 0.0008f);
                vec3 p[4]; f32 uv[8] = {u0, 1, u1, 1, u1, 0, u0, 0};
                f32 x0 = be->pos.x + c[0] / 16.0f, x1 = be->pos.x + c[3] / 16.0f, y0 = be->pos.y + c[1] / 16.0f, y1 = be->pos.y + c[4] / 16.0f;
                f32 z0 = be->pos.z + c[2] / 16.0f, z1 = be->pos.z + c[5] / 16.0f;
                if (zthin) {
                    f32 z = side ? z1 + off : z0 + off;
                    p[0] = v3(x0, y0, z); p[1] = v3(x1, y0, z); p[2] = v3(x1, y1, z); p[3] = v3(x0, y1, z);
                } else {
                    f32 x = side ? x1 + off : x0 + off;
                    p[0] = v3(x, y0, z0); p[1] = v3(x, y0, z1); p[2] = v3(x, y1, z1); p[3] = v3(x, y1, z0);
                }
                b3d_quad_uv(p, uv, col);
            }
        }
    }
    if (any) b3d_flush();
}

void entities_render(f32 alpha, const mat4 *vp, vec4 fog, f32 fn, f32 ff) {
    glDisable(GL_CULL_FACE);
    b3d_begin(vp, fog, fn, ff);
    Entity *pl = player_ent();
    // shadows first (blended)
    glDepthMask(GL_FALSE);
    ENT_FOREACH(e) {
        if (e->kind != ENT_MOB && e->kind != ENT_ITEM) continue;
        if (pl && v3_dist(e->pos, pl->pos) > 48) continue;
        if (e->kind == ENT_MOB && (mob_has(e->mob, MF_FLYING) || e->in_water)) continue;
        shadow(v3_lerp(e->prev_pos, e->pos, alpha), e->kind == ENT_MOB ? fminf(2.5f, e->width * 0.6f) : 0.18f);
    }
    b3d_flush();
    glDepthMask(GL_TRUE);
    ENT_FOREACH(e) {
        if (e->kind == ENT_PLAYER) continue;
        vec3 pos = v3_lerp(e->prev_pos, e->pos, alpha);
        if (v3_dist(pos, g_cam_pos) > g_world.render_dist * 16.0f) continue;
        if (!world_loaded((int)floorf(pos.x), (int)floorf(pos.z))) continue;
        f32 light = entity_light(e);
        switch (e->kind) {
        case ENT_MOB: {
            const MobDef *d = &MOB_DEFS[e->mob];
            if (effect_has(e, EFF_INVISIBILITY)) break;
            if (MOB_DEFS[e->mob].ai == AIK_GHOST || e->mob == MOB_SIFT_SOUL) break;   // translucent pass below
            f32 by = lerp_angle(e->prev_body_yaw, e->body_yaw, alpha);
            mat4 m = m4_mul(m4_translate(pos), m4_rot_y(by));
            f32 sc = mob_scale(e);
            if (e->mob == MOB_SHULKER) sc = 1;
            if (sc != 1) m = m4_mul(m, m4_scale(v3(sc, sc, sc)));
            if (e->death_time > 0 && e->mob != MOB_ENDER_DRAGON && e->mob != MOB_COSMIC_TYRANT) {
                f32 t = fminf(1.0f, (e->death_time + alpha) / 20.0f);
                m = m4_mul(m, m4_rot_z(sqrtf(t) * PI_F / 2));
            }
            if (MOB_DEFS[e->mob].ai == AIK_SLIME && e->swell > 0.05f) m = m4_mul(m, m4_scale(v3(1 - e->swell * 0.15f, 1 + e->swell * 0.3f, 1 - e->swell * 0.15f)));
            if ((d->ai == AIK_FISH || d->ai == AIK_SQUID) && !e->in_water && e->on_ground) m = m4_mul(m4_translate(v3(0, 0.1f, 0)), m4_mul(m, m4_rot_z(PI_F / 2)));
            if (e->mob == MOB_PUFFERFISH && e->swell > 0) { f32 s = 1 + e->swell * 0.8f; m = m4_mul(m, m4_scale(v3(s, s, s))); }
            if (e->mob == MOB_BAT && e->on_ground) m = m4_mul(m, m4_rot_x(PI_F));
            if (e->mob == MOB_SHULKER) m = m4_mul(m, m4_translate(v3(0, 0, 0)));
            f32 r = 1, g = 1, b = 1;
            if (e->mob == MOB_SHEEP && e->color) {
                static const u32 WOOL[16] = {0xF0F3F3, 0xF07613, 0xBD44B3, 0x3AAFD9, 0xF8C527, 0x70B919, 0xED8DAC, 0x3E4447, 0x8E8E86, 0x158991, 0x792AAC, 0x35399D, 0x724728, 0x546D1B, 0xA12722, 0x141519};
                u32 c = WOOL[e->color & 15];
                r = 0.25f + 0.75f * ((c >> 16) & 255) / 255.0f; g = 0.25f + 0.75f * ((c >> 8) & 255) / 255.0f; b = 0.25f + 0.75f * (c & 255) / 255.0f;
            }
            if (e->mob == MOB_SHEEP && e->sheared) { r *= 0.85f; g *= 0.75f; b *= 0.7f; }
            if (e->hurt_time > 0 || e->death_time > 0) { g *= 0.45f; b *= 0.45f; r = 1.25f; }
            if (e->mob == MOB_CREEPER && e->swell > 0) {
                f32 sw = lerpf(e->prev_swell, e->swell, alpha);
                f32 wob = 1.0f + sinf(sw * 100) * sw * 0.01f;
                f32 s = (1.0f + sw * 0.4f) * wob;
                m = m4_mul(m, m4_scale(v3(s, (1.0f + sw * 0.1f) / wob, s)));
                if (((int)(sw * 10)) % 2) { r = g = b = 2.5f; }
            }
            if (e->mob == MOB_WITHER && e->phase == 0 && (e->age / 5) % 2) { r = 0.6f; g = 0.7f; b = 1.6f; }
            ModelPose ps = {0};
            ps.head_yaw = wrap_angle(lerp_angle(e->prev_head_yaw, e->head_yaw, alpha) - by);
            ps.head_pitch = lerpf(e->prev_pitch, e->pitch, alpha);
            ps.limb_swing = e->limb_swing;
            ps.limb_amount = lerpf(e->prev_limb_amount, e->limb_amount, alpha);
            ps.attack = e->attack_anim > 0 ? 1.0f - e->attack_anim : 0;
            ps.age = e->age + alpha;
            int t = e->mob;
            ps.arms_forward = t == MOB_ZOMBIE || t == MOB_HUSK || t == MOB_DROWNED || t == MOB_ZOMBIFIED_PIGLIN || t == MOB_VINDICATOR || (t == MOB_VEX && e->target) || (d->ai == AIK_HOSTILE && d->model == MODEL_HUMANOID && e->target);
            ps.airborne = !e->on_ground || mob_has(t, MF_FLYING);
            ps.swimming = e->in_water;
            ps.bow_aim = d->ai == AIK_RANGED ? fmaxf(0.4f, e->swell) * (e->target != 0) : 0;
            ps.jaw = e->swell;
            const ModelDef *md = model_def(d->model);
            b3d_set_texture(mob_tex[e->mob].id);
            u32 col = lit_color(light, fminf(r, 1.0f), fminf(g, 1.0f), fminf(b, 1.0f), 1);
            if (r > 1.5f && g > 1.5f) col = rgba_f(1, 1, 1, 1);
            if (effect_has(e, EFF_GLOWING)) col = rgba_f(1, 1, 1, 1);
            model_draw(d->model, m, &ps, col, (f32)md->tw, (f32)md->th);
            // held trident for drowned
            if (e->item.id && e->mob == MOB_DROWNED) render_item_sprite(m4_mul(m, m4_mul(m4_translate(v3(-0.4f, 1.0f, 0.3f)), m4_scale(v3(0.7f, 0.7f, 0.7f)))), e->item.id, col);
            // guardian laser
            if ((t == MOB_GUARDIAN || t == MOB_ELDER_GUARDIAN) && e->swell > 0.02f) {
                vec3 a = v3_add(pos, v3(0, e->height * 0.5f, 0)), bpt = v3_add(e->goal, v3(0, 1.0f, 0));
                b3d_set_mode(GL_LINES); b3d_set_texture(tex_particles.id);
                u32 lc = rgba_f(0.3f + 0.7f * e->swell, 0.9f, 1.0f - 0.6f * e->swell, 1);
                b3d_vertex(a, 0.95f, 0.05f, lc); b3d_vertex(bpt, 0.95f, 0.05f, lc);
                b3d_flush(); b3d_set_mode(GL_TRIANGLES);
            }
            if (e->fire > 0 && e->death_time == 0 && !mob_has(t, MF_FIRE_IMMUNE)) {   // burning overlay
                b3d_set_texture(tex_particles.id);
                f32 u0 = 2 / 8.0f, v0 = 1 / 8.0f, dd = 1 / 8.0f;
                for (int k = 0; k < 3; k++) {
                    f32 a = (e->age * 0.1f) + k * 2.1f;
                    vec3 c = v3_add(pos, v3(cosf(a) * e->width * 0.4f, e->height * (0.3f + 0.3f * k), sinf(a) * e->width * 0.4f));
                    f32 s = e->width * 0.6f;
                    vec3 q[4] = {v3_add(c, v3_scale(v3_add(g_cam_right, g_cam_up), -s)), v3_add(c, v3_scale(v3_sub(g_cam_right, g_cam_up), s)),
                                 v3_add(c, v3_scale(v3_add(g_cam_right, g_cam_up), s)), v3_add(c, v3_scale(v3_sub(g_cam_up, g_cam_right), s))};
                    f32 uv[8] = {u0, v0 + dd, u0 + dd, v0 + dd, u0 + dd, v0, u0, v0};
                    b3d_quad_uv(q, uv, rgba_f(1, 1, 1, 0.9f));
                }
            }
        } break;
        case ENT_ITEM: {
            f32 t = e->age + alpha;
            f32 bob = sinf(t / 10.0f + e->color) * 0.1f + 0.15f;
            mat4 m = m4_mul(m4_translate(v3_add(pos, v3(0, bob, 0))), m4_rot_y(t / 20.0f + e->color));
            u32 col = lit_color(fmaxf(light, 0.25f), 1, 1, 1, 1);
            int copies = e->item.count > 32 ? 3 : e->item.count > 1 ? 2 : 1;
            bool block = ITEMS[e->item.id].icon == -1;
            for (int k = 0; k < copies; k++) {
                mat4 mk = m4_mul(m, m4_translate(v3(k * 0.06f, k * 0.04f, k * 0.05f)));
                if (block) render_block_cube(m4_mul(mk, m4_scale(v3(0.25f, 0.25f, 0.25f))), ITEMS[e->item.id].block, col);
                else render_item_icon_sprite(m4_mul(mk, m4_scale(v3(0.45f, 0.45f, 0.45f))), e->item, col);
            }
        } break;
        case ENT_XP: {
            f32 t = e->age + alpha;
            f32 s = 0.12f + fminf(0.15f, e->xp * 0.012f);
            vec3 c = v3_add(pos, v3(0, 0.15f, 0));
            f32 k = sinf(t * 0.15f + e->color) * 0.5f + 0.5f;
            b3d_set_texture(tex_particles.id);
            f32 u0 = 5 / 8.0f, v0 = 1 / 8.0f, dd = 1 / 8.0f;
            vec3 q[4] = {v3_add(c, v3_scale(v3_add(g_cam_right, g_cam_up), -s)), v3_add(c, v3_scale(v3_sub(g_cam_right, g_cam_up), s)),
                         v3_add(c, v3_scale(v3_add(g_cam_right, g_cam_up), s)), v3_add(c, v3_scale(v3_sub(g_cam_up, g_cam_right), s))};
            f32 uv[8] = {u0, v0 + dd, u0 + dd, v0 + dd, u0 + dd, v0, u0, v0};
            b3d_quad_uv(q, uv, rgba_f(0.6f + 0.4f * k, 1, 0.3f + 0.2f * k, 1));
        } break;
        case ENT_ARROW: {
            mat4 m = m4_mul(m4_translate(pos), m4_rot_y(e->yaw));
            m = m4_mul(m, m4_rot_x(-e->pitch));
            if (e->item.id == I_TRIDENT) {
                m = m4_mul(m, m4_mul(m4_rot_x(PI_F / 2), m4_rot_z(-PI_F / 4)));
                render_item_sprite(m4_mul(m, m4_scale(v3(1.4f, 1.4f, 1.4f))), I_TRIDENT, lit_color(light, 1, 1, 1, 1));
                break;
            }
            b3d_set_texture(tex_arrow.id);
            const ModelDef *am = model_def(MODEL_ARROW);
            u32 ac = lit_color(light, 1, 1, 1, 1);
            if (e->item.id == I_SPECTRAL_ARROW) ac = rgba_f(1, 0.9f, 0.4f, 1);
            if (e->item.id == I_TIPPED_ARROW) { u32 c = POTIONS[e->item.damage % POTION_COUNT].color; ac = rgba_u32((c >> 16) & 255, (c >> 8) & 255, c & 255, 255); }
            model_draw(MODEL_ARROW, m, NULL, ac, (f32)am->tw, (f32)am->th);
        } break;
        case ENT_PROJ: {
            static const u16 *PI_ITEMS;
            (void)PI_ITEMS;
            int it = 0; f32 s = 0.4f;
            switch (e->proj) {
            case PROJ_SNOWBALL: it = I_SNOWBALL; break;
            case PROJ_EGG: it = I_EGG; break;
            case PROJ_ENDER_PEARL: it = I_ENDER_PEARL; break;
            case PROJ_EYE: it = I_ENDER_EYE; break;
            case PROJ_SMALL_FIREBALL: it = I_FIRE_CHARGE; s = 0.35f; break;
            case PROJ_FIREBALL: it = I_FIRE_CHARGE; s = 1.0f; break;
            case PROJ_DRAGON_FIREBALL: it = I_DRAGON_BREATH; s = 1.0f; break;
            case PROJ_HOLY_WATER: it = I_HOLY_WATER; break;
            case PROJ_BOBBER: it = I_STRING; s = 0.35f; break;
            case PROJ_STAR_BOLT: it = I_NETHER_STAR; s = e->color ? 1.4f : 0.6f; break;
            case PROJ_SHULKER_BULLET: it = I_SHULKER_SHELL; s = 0.35f; break;
            case PROJ_LLAMA_SPIT: it = I_SNOWBALL; s = 0.2f; break;
            case PROJ_POTION: it = I_SPLASH_POTION; break;
            case PROJ_XP_BOTTLE: it = I_EXPERIENCE_BOTTLE; break;
            case PROJ_WIND_CHARGE: it = I_WIND_CHARGE; break;
            case PROJ_FIREWORK: it = I_FIREWORK_ROCKET; break;
            default: break;
            }
            if (e->proj == PROJ_WITHER_SKULL) { render_block_cube(m4_mul(m4_mul(m4_translate(pos), m4_rot_y(e->age * 0.2f)), m4_scale(v3(0.35f, 0.35f, 0.35f))), B_WITHER_SKELETON_SKULL, rgba_f(1, 1, 1, 1)); break; }
            if (it) { ItemStack st = stack_of(it, 1); if (e->proj == PROJ_POTION) st.damage = e->item.damage; billboard(v3_add(pos, v3(0, e->height * 0.5f, 0)), s, st, e->proj == PROJ_FIREBALL || e->proj == PROJ_SMALL_FIREBALL ? rgba_f(1, 1, 1, 1) : lit_color(light, 1, 1, 1, 1)); }
            if (e->proj == PROJ_BOBBER) {   // fishing line
                Entity *o = entity_get(e->owner);
                if (o) {
                    vec3 a = v3_add(entity_eye(o), v3_add(v3_scale(g_cam_right, 0.35f), v3(0, -0.3f, 0)));
                    b3d_set_mode(GL_LINES); b3d_set_texture(tex_particles.id);
                    for (int k = 0; k < 8; k++) {   // a slight sag
                        f32 t0 = k / 8.0f, t1 = (k + 1) / 8.0f;
                        vec3 p0 = v3_add(v3_lerp(a, e->pos, t0), v3(0, -0.6f * t0 * (1 - t0), 0)), p1 = v3_add(v3_lerp(a, e->pos, t1), v3(0, -0.6f * t1 * (1 - t1), 0));
                        b3d_vertex(p0, 0.95f, 0.05f, rgba_u32(30, 30, 30, 255)); b3d_vertex(p1, 0.95f, 0.05f, rgba_u32(30, 30, 30, 255));
                    }
                    b3d_flush(); b3d_set_mode(GL_TRIANGLES);
                }
            }
        } break;
        case ENT_FANGS: {
            if (e->fuse > 0) break;
            f32 up = fminf(1, (-e->fuse) / 5.0f) * 0.8f;
            for (int k = 0; k < 2; k++) {
                mat4 m = m4_mul(m4_translate(v3_add(pos, v3(k ? 0.2f : -0.2f, up * 0.5f, 0))), m4_rot_z(k ? -0.3f : 0.3f));
                render_block_cube(m4_mul(m, m4_scale(v3(0.18f, up, 0.5f))), B_BONE_BLOCK, lit_color(light, 1, 1, 1, 1));
            }
        } break;
        case ENT_DECOR: {
            vec3 n = v3((f32)FACE_DIR[e->size % 6].x, (f32)FACE_DIR[e->size % 6].y, (f32)FACE_DIR[e->size % 6].z);
            f32 L = e->vtype == DECOR_GLOW_FRAME ? 1.0f : light;
            if (e->vtype == DECOR_PAINTING) {   // procedural painting, 8 px per block, framed
                int k = e->color % PAINTING_COUNT; const PaintingDef *pd = &PAINTINGS[k];
                vec3 r = v3(n.z, 0, -n.x), up = v3(0, 1, 0), base = v3_add(pos, v3_scale(n, 0.035f));
                int W = pd->w * 8, H = pd->h * 8;
                b3d_set_texture(tex_particles.id);
                for (int j = 0; j < H; j++) for (int i = 0; i < W; i++) {
                    u32 c = (i == 0 || j == 0 || i == W - 1 || j == H - 1) ? rgba_u32(84, 56, 32, 255) : painting_color(k, (i + 0.5f) / W, (j + 0.5f) / H);
                    f32 x0 = ((f32)i / W - 0.5f) * pd->w, x1 = ((f32)(i + 1) / W - 0.5f) * pd->w, y0 = ((f32)j / H - 0.5f) * pd->h, y1 = ((f32)(j + 1) / H - 0.5f) * pd->h;
                    vec3 a = v3_add(base, v3_add(v3_scale(r, x0), v3_scale(up, y0))), b = v3_add(base, v3_add(v3_scale(r, x1), v3_scale(up, y0)));
                    vec3 cc = v3_add(base, v3_add(v3_scale(r, x1), v3_scale(up, y1))), d = v3_add(base, v3_add(v3_scale(r, x0), v3_scale(up, y1)));
                    b3d_quad(a, b, cc, d, 0.95f, 0.05f, 0.95f, 0.05f, rgba_f(((c >> 0) & 255) / 255.0f * L, ((c >> 8) & 255) / 255.0f * L, ((c >> 16) & 255) / 255.0f * L, 1));
                }
                break;
            }
            if (e->vtype == DECOR_FRAME || e->vtype == DECOR_GLOW_FRAME) {
                mat4 M = m4_translate(pos);
                if (n.y > 0.5f) M = m4_mul(M, m4_rot_x(-PI_F / 2)); else if (n.y < -0.5f) M = m4_mul(M, m4_rot_x(PI_F / 2)); else M = m4_mul(M, m4_rot_y(atan2f(n.x, n.z)));
                u32 fc = e->vtype == DECOR_GLOW_FRAME ? rgba_f(0.7f, 1.0f, 0.95f, 1) : lit_color(L, 1, 1, 1, 1);
                render_block_cube(m4_mul(M, m4_scale(v3(0.75f, 0.75f, 0.0625f))), B_SPRUCE_PLANKS, fc);
                render_block_cube(m4_mul(M, m4_mul(m4_translate(v3(0, 0, 0.01f)), m4_scale(v3(0.6f, 0.6f, 0.0625f)))), B_BROWN_WOOL, fc);
                if (!stack_empty(e->item)) render_stack(m4_mul(M, m4_mul(m4_translate(v3(0, 0, 0.07f)), m4_mul(m4_rot_z(-e->special_timer * PI_F / 4), m4_scale(v3(0.5f, 0.5f, 0.5f))))), e->item, lit_color(L, 1, 1, 1, 1));
                break;
            }
            {   // armour stand: wooden posts, posed arms, coloured armour
                static const f32 ARMS[6][2] = {{0, 0}, {-1.2f, -1.2f}, {-2.6f, 0}, {0.4f, -0.4f}, {-1.5f, 0.6f}, {-3.0f, -3.0f}};
                mat4 M = m4_mul(m4_translate(pos), m4_rot_y(-e->yaw));
                u32 wc = lit_color(L, 1, 1, 1, 1);
                #define PART(px, py, pz, sx, sy, sz, blk) render_block_cube(m4_mul(M, m4_mul(m4_translate(v3(px, py, pz)), m4_scale(v3(sx, sy, sz)))), blk, wc)
                PART(0, 0.03f, 0, 0.75f, 0.06f, 0.75f, B_SMOOTH_STONE);
                PART(0, 0.55f, 0, 0.12f, 0.95f, 0.12f, B_OAK_PLANKS);
                PART(0, 0.8f, 0, 0.5f, 0.1f, 0.15f, B_OAK_PLANKS);
                PART(0, 1.45f, 0, 0.75f, 0.12f, 0.15f, B_OAK_PLANKS);
                PART(0, 1.65f, 0, 0.1f, 0.3f, 0.1f, B_OAK_PLANKS);
                #undef PART
                for (int s2 = 0; s2 < 2; s2++) {
                    mat4 A = m4_mul(M, m4_mul(m4_translate(v3(s2 ? 0.33f : -0.33f, 1.45f, 0)), m4_mul(m4_rot_x(ARMS[e->pose % 6][s2]), m4_mul(m4_translate(v3(0, -0.35f, 0)), m4_scale(v3(0.1f, 0.7f, 0.1f))))));
                    render_block_cube(A, B_OAK_PLANKS, wc);
                }
                static const f32 EQ[4][6] = {{0, 1.85f, 0, 0.55f, 0.5f, 0.55f}, {0, 1.15f, 0, 0.6f, 0.65f, 0.32f}, {0, 0.6f, 0, 0.52f, 0.5f, 0.3f}, {0, 0.12f, 0, 0.52f, 0.22f, 0.32f}};
                for (int k = 0; k < 4; k++) {
                    ItemStack q = e->equip[k];
                    if (stack_empty(q)) continue;
                    mat4 B2 = m4_mul(M, m4_mul(m4_translate(v3(EQ[k][0], EQ[k][1], EQ[k][2])), m4_scale(v3(EQ[k][3], EQ[k][4], EQ[k][5]))));
                    if (ITEMS[q.id].armor_slot < 0 && ITEMS[q.id].block) { render_block_cube(B2, ITEMS[q.id].block, wc); continue; }
                    const char *key = ITEMS[q.id].key; u32 ac = 0xFFD8D8D8;
                    static const struct { const char *p; u32 c; } MAT[] = {{"leather", 0xFF355A9C}, {"chainmail", 0xFF9A9A9A}, {"iron", 0xFFD8D8D8}, {"golden", 0xFF3FD3F4},
                        {"diamond", 0xFFD9ED4A}, {"netherite", 0xFF45424A}, {"copper", 0xFF5F87D8}, {"turtle", 0xFF36A047}, {"divine", 0xFFA8E7F6}, {"tyrant", 0xFF40C0F0}, {"space", 0xFFE0E8F0}};
                    for (size_t mi = 0; mi < ARRAY_LEN(MAT); mi++) if (!strncmp(key, MAT[mi].p, strlen(MAT[mi].p))) ac = MAT[mi].c;
                    render_block_cube(B2, B_WHITE_CONCRETE, rgba_f(((ac >> 0) & 255) / 255.0f * L, ((ac >> 8) & 255) / 255.0f * L, ((ac >> 16) & 255) / 255.0f * L, 1));
                }
            }
        } break;
        case ENT_CRYSTAL: {
            f32 t = e->age + alpha;
            mat4 m = m4_mul(m4_translate(v3_add(pos, v3(0, 1.2f + sinf(t * 0.05f) * 0.2f, 0))), m4_mul(m4_rot_y(t * 0.05f), m4_rot_x(t * 0.03f)));
            render_block_cube(m4_mul(m, m4_scale(v3(0.8f, 0.8f, 0.8f))), B_GLASS, rgba_f(1, 1, 1, 1));
            render_block_cube(m4_mul(m, m4_scale(v3(0.45f, 0.45f, 0.45f))), B_PURPUR_BLOCK, rgba_f(1, 0.8f, 1, 1));
            render_block_cube(m4_mul(m4_translate(v3_add(pos, v3(0, 0.25f, 0))), m4_scale(v3(1.2f, 0.5f, 1.2f))), B_BEDROCK, lit_color(light, 1, 1, 1, 1));
        } break;
        case ENT_VEHICLE: {
            f32 yaw = e->body_yaw;
            f32 shake = e->hurt_time > 0 ? sinf(e->hurt_time * 1.5f) * 0.15f : 0;
            u32 col = lit_color(light, 1, 1, 1, 1);
            if (vehicle_is_cart(e->vtype)) {
                mat4 m = m4_mul(m4_translate(v3_add(pos, v3(0, 0.40f, 0))), m4_mul(m4_rot_y(yaw), m4_rot_z(shake)));
                m = m4_mul(m, m4_scale(v3(0.95f, 0.75f, 1.2f)));
                const BlockDef *iron = &BLOCKS[B_IRON_BLOCK];
                static const f32 CART[5][6] = {{0, 0, 0, 16, 2, 16}, {0, 2, 0, 1, 12, 16}, {15, 2, 0, 16, 12, 16}, {1, 2, 0, 15, 12, 1}, {1, 2, 15, 15, 12, 16}};
                for (int i = 0; i < 5; i++) draw_block_box(m, iron, CART[i], mul_col(col, 0xFFB0B0B0, 1));
                int inner = e->vtype == VEH_CHEST_MINECART ? B_CHEST : e->vtype == VEH_FURNACE_MINECART ? B_FURNACE : e->vtype == VEH_TNT_MINECART ? B_TNT : e->vtype == VEH_HOPPER_MINECART ? B_HOPPER : 0;
                if (inner) render_block_cube(m4_mul(m4_translate(v3_add(pos, v3(0, 0.75f, 0))), m4_mul(m4_rot_y(yaw), m4_scale(v3(0.75f, 0.75f, 0.75f)))), (u16)inner, col);
            } else {
                // wood of the boat item (oak_boat -> oak_planks, bamboo_raft -> bamboo_planks)
                Block planks = B_OAK_PLANKS;
                if (item_valid(e->item.id)) {
                    char k[64]; snprintf(k, sizeof k, "%s", ITEMS[e->item.id].key);
                    char *u = strstr(k, "_chest_boat"); if (!u) u = strstr(k, "_chest_raft"); if (!u) u = strstr(k, "_boat"); if (!u) u = strstr(k, "_raft");
                    if (u) { *u = 0; char pk[80]; snprintf(pk, sizeof pk, "%s_planks", k); Block b = block_by_key(pk); if (b) planks = b; }
                }
                mat4 m = m4_mul(m4_translate(v3_add(pos, v3(0, 0.22f, 0))), m4_mul(m4_rot_y(yaw), m4_rot_z(shake)));
                m = m4_mul(m, m4_scale(v3(1.35f, 0.45f, 1.9f)));
                static const f32 BOAT[5][6] = {{1, 0, 1, 15, 2, 15}, {0, 0, 1, 1, 10, 15}, {15, 0, 1, 16, 10, 15}, {1, 0, 0, 15, 10, 1}, {1, 0, 15, 15, 10, 16}};
                const BlockDef *pd = &BLOCKS[planks];
                for (int i = 0; i < 5; i++) draw_block_box(m, pd, BOAT[i], col);
                static const f32 SEAT[6] = {1, 5, 7, 15, 6.5f, 9};
                draw_block_box(m, pd, SEAT, col);
                if (e->vtype == VEH_CHEST_BOAT) render_block_cube(m4_mul(m4_translate(v3_add(pos, v3(-sinf(yaw) * 0.5f, 0.55f, -cosf(yaw) * 0.5f))), m4_mul(m4_rot_y(yaw), m4_scale(v3(0.65f, 0.65f, 0.65f)))), B_CHEST, col);
            }
        } break;
        case ENT_TNT: {
            f32 fl = (e->fuse / 8) % 2 ? 1.0f : 0.0f;
            mat4 m = m4_translate(v3_add(pos, v3(0, 0.49f, 0)));
            f32 sc = e->fuse < 10 ? 1.0f + (10 - e->fuse) * 0.02f : 1.0f;
            m = m4_mul(m, m4_scale(v3(sc * 0.98f, sc * 0.98f, sc * 0.98f)));
            render_block_cube(m, B_TNT, fl > 0 ? rgba_f(1, 1, 1, 1) : lit_color(light, 1, 1, 1, 1));
            if (fl > 0) render_block_cube(m4_mul(m, m4_scale(v3(1.01f, 1.01f, 1.01f))), B_SNOW_BLOCK, rgba_f(1, 1, 1, 0.5f));
        } break;
        case ENT_FALLING: {
            mat4 m = m4_translate(v3_add(pos, v3(0, 0.49f, 0)));
            m = m4_mul(m, m4_scale(v3(0.98f, 0.98f, 0.98f)));
            render_block_cube(m, (Block)e->xp, lit_color(light, 1, 1, 1, 1));
        } break;
        default: break;
        }
    }
    b3d_flush();
    banners_render(vp, fog, fn, ff);
    // translucent pass: ghosts (blended, no depth writes, drawn after everything opaque)
    b3d_begin(vp, fog, fn, ff);
    glDepthMask(GL_FALSE);
    ENT_FOREACH(e) {
        if (e->kind != ENT_MOB || (MOB_DEFS[e->mob].ai != AIK_GHOST && e->mob != MOB_SIFT_SOUL) || effect_has(e, EFF_INVISIBILITY)) continue;
        vec3 pos = v3_lerp(e->prev_pos, e->pos, alpha);
        if (v3_dist(pos, g_cam_pos) > g_world.render_dist * 16.0f) continue;
        const MobDef *d = &MOB_DEFS[e->mob];
        f32 by = lerp_angle(e->prev_body_yaw, e->body_yaw, alpha);
        mat4 m = m4_mul(m4_translate(v3_add(pos, v3(0, sinf((e->age + alpha) * 0.08f) * 0.08f, 0))), m4_rot_y(by));
        if (e->death_time > 0) { f32 t = fminf(1.0f, (e->death_time + alpha) / 20.0f); m = m4_mul(m, m4_scale(v3(1 + t, 1 - t * 0.8f, 1 + t))); }
        ModelPose ps = {0};
        ps.head_yaw = wrap_angle(lerp_angle(e->prev_head_yaw, e->head_yaw, alpha) - by);
        ps.head_pitch = lerpf(e->prev_pitch, e->pitch, alpha);
        ps.attack = e->attack_anim > 0 ? 1.0f - e->attack_anim : 0;
        ps.age = e->age + alpha; ps.airborne = true; ps.arms_forward = e->target != 0;
        f32 a = e->no_clip ? 0.22f : 0.5f + 0.08f * sinf((e->age + alpha) * 0.11f);
        if (e->death_time > 0) a *= fmaxf(0, 1.0f - e->death_time / 20.0f);
        u32 col = (e->hurt_time > 0) ? rgba_f(1, 0.55f, 0.55f, a) : rgba_f(0.85f, 0.95f, 1.0f, a);
        const ModelDef *md = model_def(d->model);
        b3d_set_texture(mob_tex[e->mob].id);
        model_draw(d->model, m, &ps, col, (f32)md->tw, (f32)md->th);
    }
    // the world border: a translucent striped wall that fades in within 24 blocks
    if (g_wb.size < 59999000) {
        f32 h = (f32)g_wb.size / 2, cx = (f32)g_wb.cx, cz = (f32)g_wb.cz;
        f32 d = h - fmaxf(fabsf(g_cam_pos.x - cx), fabsf(g_cam_pos.z - cz));
        if (d < 24) {
            f32 a = clampf(1 - d / 24, 0.15f, 0.8f), y0 = g_cam_pos.y - 32, y1 = g_cam_pos.y + 32;
            b3d_set_texture(tex_particles.id);
            glDisable(GL_CULL_FACE);
            for (int side = 0; side < 4; side++) {
                for (f32 t = -32; t < 32; t += 2) {
                    f32 s0 = (side < 2 ? g_cam_pos.z - cz : g_cam_pos.x - cx) + t, s1 = s0 + 1;
                    s0 = clampf(s0, -h, h); s1 = clampf(s1, -h, h); if (s1 <= s0) continue;
                    f32 fixed = side & 1 ? h : -h;
                    vec3 p0, p1, p2, p3;
                    if (side < 2) { p0 = v3(cx + fixed, y0, cz + s0); p1 = v3(cx + fixed, y0, cz + s1); p2 = v3(cx + fixed, y1, cz + s1); p3 = v3(cx + fixed, y1, cz + s0); }
                    else { p0 = v3(cx + s0, y0, cz + fixed); p1 = v3(cx + s1, y0, cz + fixed); p2 = v3(cx + s1, y1, cz + fixed); p3 = v3(cx + s0, y1, cz + fixed); }
                    b3d_quad(p0, p1, p2, p3, 0.95f, 0.05f, 0.95f, 0.05f, rgba_f(0.25f, 0.55f, 1.0f, a));
                }
            }
        }
    }
    b3d_flush();
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
}

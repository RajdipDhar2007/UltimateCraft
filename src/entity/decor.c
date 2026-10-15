// Decorations: item frames, glow item frames, paintings and armour stands (entity kind ENT_DECOR).
//  * subtype in e->vtype (DECOR_*), the side it hangs from in e->size (a FACE_*: the wall's outward normal),
//    frame rotation 0..7 in e->special_timer, painting variant in e->color, armour-stand pose in e->pose,
//    framed item in e->item, armour-stand equipment in e->equip[0..3] (head .. feet).
//  * Item frames: right-click puts in one of the held item, then rotates it (8 steps, click); hitting takes the
//    item out, hitting an empty frame breaks it. A comparator reading a frame (in the cell behind it, or on the
//    block behind it) outputs rotation + 1. Glow frames are always fully lit.
//  * Paintings: 16 original procedural paintings from 1x1 to 4x4; placing picks a random one among the largest
//    that fit the wall. Hitting drops the painting.
//  * Armour stands: right-click with armour (or a block / head) equips it; with an empty hand takes equipment back;
//    sneak + right-click cycles 6 poses. Hitting knocks it down (drops it and its equipment).
//  * Frames and paintings drop when the wall behind them goes away.
#include "entity.h"
#include "../world/world.h"
#include "../game/game.h"
#include "../game/player.h"
#include "../game/fx.h"
#include "../audio/audio.h"
#include "../world/rules.h"

const PaintingDef PAINTINGS[PAINTING_COUNT] = {
    {"Dawn over the Meadow", 2, 1, 0}, {"Twin Moons", 1, 1, 3}, {"The Delvari Lamps", 1, 2, 4}, {"Starfall", 2, 2, 3},
    {"Cloud Sea", 4, 2, 0}, {"Ember Wastes", 2, 1, 5}, {"Crystal Grove", 1, 1, 1}, {"Quiet Harbor", 2, 2, 0},
    {"Lantern Path", 1, 2, 4}, {"Crown of the Tyrant", 1, 1, 1}, {"Northern Lights", 4, 2, 6}, {"The Sift Gate", 3, 3, 1},
    {"Bloom", 1, 1, 1}, {"Orbit", 2, 2, 3}, {"Patchwork", 4, 3, 2}, {"The Long Road", 4, 4, 0},
};
static const char *DECOR_KEYS[DECOR_COUNT] = {"item_frame", "glow_item_frame", "painting", "armor_stand"};
const char *decor_key(const Entity *e) { return e->vtype < DECOR_COUNT ? DECOR_KEYS[e->vtype] : "unknown"; }
int decor_type_by_key(const char *k) { for (int i = 0; i < DECOR_COUNT; i++) if (!strcmp(k, DECOR_KEYS[i])) return i; return -1; }
static int decor_item(int type) { return type == DECOR_FRAME ? I_ITEM_FRAME : type == DECOR_GLOW_FRAME ? I_GLOW_ITEM_FRAME : type == DECOR_PAINTING ? I_PAINTING : I_ARMOR_STAND; }

// ---- procedural paintings: colour of pixel (u, v) in 0..1 (v up) of painting k
static u32 mix(u32 a, u32 b, f32 t) {
    t = clampf(t, 0, 1);
    int r = (int)(((a >> 0) & 255) * (1 - t) + ((b >> 0) & 255) * t), g = (int)(((a >> 8) & 255) * (1 - t) + ((b >> 8) & 255) * t), bl = (int)(((a >> 16) & 255) * (1 - t) + ((b >> 16) & 255) * t);
    return rgba_u32(r, g, bl, 255);
}
static f32 h01(int a, int b, int c) { return (f32)(hash3i(4242, a, b, c) % 10000) / 10000.0f; }
u32 painting_color(int k, f32 u, f32 v) {
    const PaintingDef *p = &PAINTINGS[k];
    f32 s = (f32)k * 1.7f, aspect = (f32)p->w / p->h;
    switch (p->style) {
    case 0: {   // landscape: sky, sun, two hill ranges, water
        f32 h1 = 0.42f + 0.07f * sinf(u * 6 * aspect + s), h2 = 0.28f + 0.06f * sinf(u * 9 * aspect + s * 2);
        if (v < 0.12f) return mix(rgba_u32(40, 90, 150, 255), rgba_u32(90, 150, 200, 255), v / 0.12f);
        if (v < h2) return mix(rgba_u32(40, 90, 40, 255), rgba_u32(70, 130, 60, 255), (v - 0.12f) / (h2 - 0.12f));
        if (v < h1) return mix(rgba_u32(90, 150, 80, 255), rgba_u32(150, 190, 110, 255), (v - h2) / fmaxf(0.01f, h1 - h2));
        f32 dx = (u - 0.72f) * aspect, dy = v - 0.72f;
        if (dx * dx + dy * dy < 0.008f) return rgba_u32(255, 230, 140, 255);
        return mix(rgba_u32(250, 190, 140, 255), rgba_u32(90, 140, 220, 255), (v - h1) / (1 - h1));
    }
    case 1: {   // radial: rings and petals
        f32 dx = (u - 0.5f) * aspect, dy = v - 0.5f, d = sqrtf(dx * dx + dy * dy), a = atan2f(dy, dx);
        static const u32 PAL[4] = {0xFF48C8F0, 0xFFA8B850, 0xFF7A58D8, 0xFFD8E8F0};
        int ring = (int)(d * 9 + 0.5f * sinf(a * (4 + k % 3))) % 4;
        return mix(PAL[ring], rgba_u32(30, 30, 50, 255), d * 0.9f);
    }
    case 2: {   // patchwork quilt
        int cx = (int)(u * p->w * 3), cy = (int)(v * p->h * 3);
        f32 r = h01(k, cx, cy);
        static const u32 PAL[6] = {0xFF3A5AC8, 0xFF5AB8E0, 0xFF6AA850, 0xFFD8D8D8, 0xFF8A4AA0, 0xFF4878C0};
        return PAL[(int)(r * 6) % 6];
    }
    case 3: {   // night sky with planets / moons
        u32 c = mix(rgba_u32(10, 12, 40, 255), rgba_u32(40, 30, 90, 255), v);
        if (h01(k, (int)(u * 64), (int)(v * 64)) > 0.97f) c = rgba_u32(240, 240, 255, 255);
        for (int m = 0; m < (k == 1 ? 2 : 1); m++) {
            f32 dx = (u - (0.35f + 0.3f * m)) * aspect, dy = v - (0.6f - 0.15f * m), r = 0.16f - 0.05f * m;
            if (dx * dx + dy * dy < r * r) c = mix(rgba_u32(230, 220, 190, 255), rgba_u32(120, 110, 150, 255), (dx + r) / (2 * r));
        }
        return c;
    }
    case 4: {   // lamplit street
        u32 c = mix(rgba_u32(20, 24, 36, 255), rgba_u32(50, 58, 82, 255), v);
        if (v < 0.15f) c = rgba_u32(60, 64, 80, 255);
        for (int l = 0; l < 2; l++) { f32 dx = (u - 0.5f) * aspect, dy = v - (0.35f + 0.4f * l); f32 g = expf(-(dx * dx + dy * dy) * 60); c = mix(c, rgba_u32(250, 190, 100, 255), g); }
        if (fabsf(u - 0.5f) < 0.04f && v < 0.8f) c = mix(c, rgba_u32(70, 76, 96, 255), 0.6f);
        return c;
    }
    case 5: {   // volcanic waste
        f32 m1 = 0.45f + 0.2f * fabsf(sinf(u * 4 * aspect + s));
        if (v < m1) { u32 c = rgba_u32(30, 18, 16, 255); if (fabsf(sinf(u * 30 + v * 8)) < 0.05f) c = rgba_u32(250, 120, 40, 255); return c; }
        return mix(rgba_u32(200, 70, 30, 255), rgba_u32(60, 20, 30, 255), (v - m1) / (1 - m1));
    }
    default: {  // aurora
        u32 c = mix(rgba_u32(8, 12, 30, 255), rgba_u32(20, 30, 60, 255), v);
        f32 b1 = expf(-powf(v - 0.62f - 0.1f * sinf(u * 8 * aspect), 2) * 90), b2 = expf(-powf(v - 0.45f - 0.08f * sinf(u * 5 * aspect + 2), 2) * 120);
        c = mix(c, rgba_u32(80, 240, 150, 255), b1); c = mix(c, rgba_u32(170, 90, 230, 255), b2 * 0.8f);
        if (v < 0.15f) c = rgba_u32(15, 20, 25, 255);
        return c;
    }
    }
}

// ---- geometry
static vec3 fdir(int f) { return v3((f32)FACE_DIR[f].x, (f32)FACE_DIR[f].y, (f32)FACE_DIR[f].z); }
AABB decor_aabb(const Entity *e) {
    if (e->vtype == DECOR_STAND) return entity_aabb(e);
    vec3 n = fdir(e->size), r = v3(n.z, 0, -n.x);
    f32 hw, hh, t = 0.04f;
    if (e->vtype == DECOR_PAINTING) { hw = PAINTINGS[e->color % PAINTING_COUNT].w * 0.5f; hh = PAINTINGS[e->color % PAINTING_COUNT].h * 0.5f; }
    else { hw = hh = 0.375f; }
    vec3 ext = (e->size == FACE_U || e->size == FACE_D) ? v3(hw, t, hh) : v3(fabsf(r.x) * hw + fabsf(n.x) * t, hh, fabsf(r.z) * hw + fabsf(n.z) * t);
    return (AABB){v3_sub(e->pos, ext), v3_add(e->pos, ext)};
}
static ivec3 decor_cell(const Entity *e) { return v3_floor(v3_add(e->pos, v3_scale(fdir(e->size), 0.2f))); }
static ivec3 decor_wall(const Entity *e) { return v3_floor(v3_sub(e->pos, v3_scale(fdir(e->size), 0.2f))); }
Entity *decor_spawn(int type, vec3 pos, int face, int variant) {
    Entity *e = entity_spawn(ENT_DECOR, pos);
    if (!e) return NULL;
    e->vtype = (u8)type; e->size = (u8)face; e->color = (u8)variant; e->persistent = true;
    e->no_gravity = true; e->health = e->max_health = 1;
    if (type == DECOR_STAND) { e->width = 0.5f; e->height = 1.975f; }
    else { AABB a = decor_aabb(e); e->width = fmaxf(a.max.x - a.min.x, a.max.z - a.min.z); e->height = a.max.y - a.min.y; }
    return e;
}
static bool decor_overlaps(AABB a) {
    ENT_FOREACH(o) if (o->kind == ENT_DECOR && !o->remove && o->vtype != DECOR_STAND && aabb_overlap(decor_aabb(o), a)) return true;
    return false;
}
// does painting k fit with its centre cell `cell` on the wall facing `face`?
static bool painting_fits(int k, ivec3 cell, int face, vec3 *center) {
    const PaintingDef *p = &PAINTINGS[k];
    ivec3 n = FACE_DIR[face], r = iv3(n.z, 0, -n.x);
    int x0 = -(p->w - 1) / 2, y0 = -(p->h - 1) / 2;
    for (int j = 0; j < p->h; j++) for (int i = 0; i < p->w; i++) {
        ivec3 c = iv3(cell.x + r.x * (x0 + i), cell.y + y0 + j, cell.z + r.z * (x0 + i));
        if (BLOCKS[world_get_block(c.x, c.y, c.z)].solid) return false;
        if (!BLOCKS[world_get_block(c.x - n.x, c.y, c.z - n.z)].solid) return false;
    }
    f32 cx = x0 + (p->w - 1) * 0.5f, cy = y0 + (p->h - 1) * 0.5f;
    *center = v3(cell.x + 0.5f + r.x * cx - n.x * 0.47f, cell.y + 0.5f + cy, cell.z + 0.5f + r.z * cx - n.z * 0.47f);
    Entity tmp = {0}; tmp.vtype = DECOR_PAINTING; tmp.size = (u8)face; tmp.color = (u8)k; tmp.pos = *center;
    return !decor_overlaps(decor_aabb(&tmp));
}
Entity *decor_place_at(int type, ivec3 cell, int face, f32 yaw) {   // face: outward normal of the wall
    if (type == DECOR_STAND) {
        if (BLOCKS[world_get_block(cell.x, cell.y, cell.z)].solid || BLOCKS[world_get_block(cell.x, cell.y + 1, cell.z)].solid) return NULL;
        Entity *e = decor_spawn(DECOR_STAND, v3(cell.x + 0.5f, (f32)cell.y, cell.z + 0.5f), FACE_U, 0);
        if (e) e->yaw = e->body_yaw = e->head_yaw = roundf(yaw / (PI_F / 4)) * (PI_F / 4);
        return e;
    }
    if (BLOCKS[world_get_block(cell.x, cell.y, cell.z)].solid) return NULL;
    ivec3 w = iv3(cell.x - FACE_DIR[face].x, cell.y - FACE_DIR[face].y, cell.z - FACE_DIR[face].z);
    if (!BLOCKS[world_get_block(w.x, w.y, w.z)].solid) return NULL;
    if (type == DECOR_PAINTING) {
        if (face == FACE_U || face == FACE_D) return NULL;
        int best = 0, cand[PAINTING_COUNT], nc = 0; vec3 ctr[PAINTING_COUNT];
        for (int k = 0; k < PAINTING_COUNT; k++) {
            if (!painting_fits(k, cell, face, &ctr[k])) continue;
            int area = PAINTINGS[k].w * PAINTINGS[k].h;
            if (area > best) { best = area; nc = 0; }
            if (area == best) cand[nc++] = k;
        }
        if (!nc) return NULL;
        int k = cand[irand(0, nc - 1)];
        return decor_spawn(DECOR_PAINTING, ctr[k], face, k);
    }
    vec3 pos = v3_sub(iv3_center(cell), v3_scale(fdir(face), 0.47f));
    Entity tmp = {0}; tmp.vtype = (u8)type; tmp.size = (u8)face; tmp.pos = pos;
    if (decor_overlaps(decor_aabb(&tmp))) return NULL;
    return decor_spawn(type, pos, face, 0);
}
bool decor_place(Entity *pl, ItemStack *h) {
    int type = h->id == I_ITEM_FRAME ? DECOR_FRAME : h->id == I_GLOW_ITEM_FRAME ? DECOR_GLOW_FRAME : h->id == I_PAINTING ? DECOR_PAINTING : h->id == I_ARMOR_STAND ? DECOR_STAND : -1;
    RayHit L = g_player.look;
    if (type < 0 || !L.hit) return false;
    if (type == DECOR_STAND && L.face != FACE_U) return false;
    Entity *e = decor_place_at(type, L.prev, L.face, pl->yaw + PI_F);
    if (!e) return false;
    if (g_player.gamemode != GM_CREATIVE) { if (--h->count == 0) *h = (ItemStack){0}; }
    sound_play_at(SND_DIG_WOOD, e->pos, 0.8f, 1.0f);
    rs_block_changed(decor_cell(e));
    if (type == DECOR_PAINTING) progress_grant("painter");
    return true;
}
static void drop(Entity *e, ItemStack s) { if (!stack_empty(s)) item_drop(v3_add(e->pos, v3_scale(fdir(e->size), 0.3f)), s, v3(frand_range(-1, 1), 2, frand_range(-1, 1)), 10); }
static void decor_break(Entity *e, bool drop_self) {
    if (rule(GR_DO_ENTITY_DROPS)) {   // gamerule doEntityDrops
        if (drop_self) drop(e, stack_of(decor_item(e->vtype), 1));
        drop(e, e->item);
        for (int i = 0; i < 4; i++) drop(e, e->equip[i]);
    }
    e->item = (ItemStack){0}; memset(e->equip, 0, sizeof e->equip);
    e->remove = true;
    sound_play_at(SND_DIG_WOOD, e->pos, 0.8f, 0.8f);
    fx_poof(e->pos, 0.3f, 0.3f);
    rs_block_changed(decor_cell(e));
}
void decor_hit(Entity *e, Entity *by) {
    bool creative = by && by->kind == ENT_PLAYER && g_player.gamemode == GM_CREATIVE;
    if ((e->vtype == DECOR_FRAME || e->vtype == DECOR_GLOW_FRAME) && !stack_empty(e->item)) {
        if (!creative) drop(e, e->item);
        e->item = (ItemStack){0}; e->special_timer = 0;
        sound_play_at(SND_POP, e->pos, 0.6f, 1.0f);
        rs_block_changed(decor_cell(e));
        return;
    }
    decor_break(e, !creative);
}
bool decor_interact(Entity *e, ItemStack *h) {
    bool creative = g_player.gamemode == GM_CREATIVE;
    if (e->vtype == DECOR_FRAME || e->vtype == DECOR_GLOW_FRAME) {
        if (stack_empty(e->item)) {
            if (stack_empty(*h)) return false;
            e->item = *h; e->item.count = 1; e->special_timer = 0;
            if (!creative && --h->count == 0) *h = (ItemStack){0};
            sound_play_at(SND_POP, e->pos, 0.6f, 1.2f);
        } else {
            e->special_timer = (e->special_timer + 1) % 8;
            sound_play_at(SND_CLICK, e->pos, 0.4f, 1.4f);
        }
        rs_block_changed(decor_cell(e));
        return true;
    }
    if (e->vtype != DECOR_STAND) return false;
    if (g_player.sneaking) { e->pose = (u8)((e->pose + 1) % 6); sound_play_at(SND_CLICK, e->pos, 0.4f, 1.0f); return true; }
    if (stack_empty(*h)) {   // take the topmost piece back
        for (int i = 0; i < 4; i++) if (!stack_empty(e->equip[i])) { player_give(e->equip[i]); e->equip[i] = (ItemStack){0}; sound_play_at(SND_POP, e->pos, 0.5f, 1.0f); return true; }
        return false;
    }
    int slot = ITEMS[h->id].armor_slot;
    if (slot < 0 || slot > 3) { if (ITEMS[h->id].block) slot = 0; else return false; }   // blocks and heads go on the head
    ItemStack old = e->equip[slot];
    e->equip[slot] = *h; e->equip[slot].count = 1;
    if (!creative) { if (--h->count == 0) *h = (ItemStack){0}; }
    if (!stack_empty(old)) player_give(old);
    sound_play_at(SND_DIG_WOOD, e->pos, 0.5f, 1.3f);
    return true;
}
void decor_tick(Entity *e) {
    if (e->vtype == DECOR_STAND) {   // stands rest on the ground
        ivec3 f = v3_floor(v3_add(e->pos, v3(0, -0.05f, 0)));
        if (!BLOCKS[world_get_block(f.x, f.y, f.z)].solid && e->pos.y > 1) e->pos.y = fmaxf(floorf(e->pos.y - 0.2f), e->pos.y - 0.3f);
        return;
    }
    if (e->age % 20) return;
    if (e->vtype == DECOR_PAINTING) {   // every wall block behind it must remain
        const PaintingDef *p = &PAINTINGS[e->color % PAINTING_COUNT];
        vec3 n = fdir(e->size), r = v3(n.z, 0, -n.x);
        for (int j = 0; j < p->h; j++) for (int i = 0; i < p->w; i++) {
            vec3 q = v3_add(v3_sub(e->pos, v3_scale(n, 0.2f)), v3(r.x * (i - (p->w - 1) * 0.5f), j - (p->h - 1) * 0.5f, r.z * (i - (p->w - 1) * 0.5f)));
            ivec3 c = v3_floor(q);
            if (!BLOCKS[world_get_block(c.x, c.y, c.z)].solid) { decor_break(e, true); return; }
        }
        return;
    }
    ivec3 w = decor_wall(e);
    if (!BLOCKS[world_get_block(w.x, w.y, w.z)].solid) decor_break(e, true);
}
// comparator reading: a frame in cell p, or hanging on block p
int decor_comparator(ivec3 p) {
    ENT_FOREACH(e) {
        if (e->kind != ENT_DECOR || e->remove || (e->vtype != DECOR_FRAME && e->vtype != DECOR_GLOW_FRAME)) continue;
        if (iv3_eq(decor_cell(e), p) || iv3_eq(decor_wall(e), p)) return stack_empty(e->item) ? 0 : e->special_timer + 1;
    }
    return -1;
}

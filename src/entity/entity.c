#include "../game/weather.h"
#include "../game/game.h"
#include "entity.h"
#include "../world/rules.h"
#include "../game/player.h"
#include "vehicle.h"
#include "../world/world.h"
#include "../game/player.h"
#include "../game/fx.h"
#include "../audio/audio.h"

EntityPool g_ents;

// ======================================================================== pool
Entity *entity_spawn(int kind, vec3 pos) {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = &g_ents.e[i];
        if (e->id) continue;
        memset(e, 0, sizeof *e);
        g_ents.gen++;
        e->id = ((g_ents.gen & 0x3FFFFF) << 10) | (u32)i;
        if (!e->id) e->id = (1u << 10) | (u32)i;
        e->kind = (u8)kind;
        e->pos = e->prev_pos = pos;
        e->width = 0.25f; e->height = 0.25f;
        e->health = e->max_health = 1;
        e->air = 300 * 3;
        return e;
    }
    return NULL;
}
Entity *entity_get(u32 id) {
    if (!id) return NULL;
    Entity *e = &g_ents.e[id & 1023];
    return (e->id == id && !e->remove) ? e : NULL;
}
void entities_clear(void) { memset(&g_ents, 0, sizeof g_ents); }

// ======================================================================== constructors
Entity *item_drop(vec3 pos, ItemStack s, vec3 vel, int delay) {
    if (stack_empty(s)) return NULL;
    Entity *e = entity_spawn(ENT_ITEM, pos);
    if (!e) return NULL;
    e->item = s; e->vel = vel; e->pickup_delay = delay;
    e->width = 0.25f; e->height = 0.25f;
    e->color = (u8)irand(0, 255);
    e->yaw = frand() * 6.28f;
    return e;
}
void item_drop_scatter(vec3 pos, ItemStack s) {
    item_drop(v3_add(pos, v3(frand_range(-0.25f, 0.25f), 0, frand_range(-0.25f, 0.25f))), s,
              v3(frand_range(-1.5f, 1.5f), frand_range(2.5f, 4.5f), frand_range(-1.5f, 1.5f)), 10);
}
void xp_drop(vec3 pos, int amount) {
    // split like Minecraft into a few orbs of common sizes
    while (amount > 0) {
        int v = amount >= 17 ? 17 : amount >= 7 ? 7 : amount >= 3 ? 3 : 1;
        amount -= v;
        Entity *e = entity_spawn(ENT_XP, v3_add(pos, v3(frand_range(-0.3f, 0.3f), 0.2f, frand_range(-0.3f, 0.3f))));
        if (!e) return;
        e->xp = v; e->width = e->height = 0.25f;
        e->vel = v3(frand_range(-2, 2), frand_range(3, 5), frand_range(-2, 2));
        e->color = (u8)irand(0, 255);
    }
}
Entity *arrow_spawn(vec3 pos, vec3 vel, u32 owner, f32 dmg, bool from_player) {
    Entity *e = entity_spawn(ENT_ARROW, pos);
    if (!e) return NULL;
    e->vel = vel; e->owner = owner; e->damage = dmg; e->from_player = from_player;
    e->width = e->height = 0.3f;
    e->yaw = atan2f(vel.x, vel.z);
    e->pitch = atan2f(vel.y, sqrtf(vel.x * vel.x + vel.z * vel.z));
    return e;
}
Entity *tnt_spawn(vec3 pos, int fuse) {
    Entity *e = entity_spawn(ENT_TNT, pos);
    if (!e) return NULL;
    e->fuse = fuse; e->width = e->height = 0.98f;
    e->vel = v3(frand_range(-1, 1), 3, frand_range(-1, 1));
    sound_play_at(SND_FUSE, pos, 1, 1);
    return e;
}

Entity *falling_spawn(vec3 pos, u16 block) {
    Entity *e = entity_spawn(ENT_FALLING, pos);
    if (!e) return NULL;
    e->xp = block; e->width = e->height = 0.98f;
    return e;
}
Entity *falling_try_spawn(vec3 pos, u16 block) {
    int sd = g_world.sim_dist > 0 ? g_world.sim_dist : 8;
    int cx = (int)floorf(pos.x) >> 4, cz = (int)floorf(pos.z) >> 4;
    if (abs(cx - g_world.sim_cx) > sd - 1 || abs(cz - g_world.sim_cz) > sd - 1) return NULL;   // frozen out there: let it drop instantly
    return falling_spawn(pos, block);
}

// ======================================================================== physics
static int boxes_for(Entity *e, vec3 d, AABB *out, int max) {
    AABB a = entity_aabb(e);
    AABB area = a;
    if (d.x < 0) area.min.x += d.x; else area.max.x += d.x;
    if (d.y < 0) area.min.y += d.y; else area.max.y += d.y;
    if (d.z < 0) area.min.z += d.z; else area.max.z += d.z;
    return world_collect_boxes(area, out, max);
}

// Minecraft-style axis clipping: move Y, then X, then Z.
static vec3 move_clipped(Entity *e, vec3 d) {
    AABB boxes[256];
    int n = boxes_for(e, d, boxes, 256);
    AABB a = entity_aabb(e);
    const f32 eps = 1e-4f;
    f32 dy = d.y;
    for (int i = 0; i < n; i++) {
        AABB b = boxes[i];
        if (a.max.x <= b.min.x + eps || a.min.x >= b.max.x - eps || a.max.z <= b.min.z + eps || a.min.z >= b.max.z - eps) continue;
        if (dy < 0 && b.max.y <= a.min.y + eps) dy = fmaxf(dy, b.max.y - a.min.y);
        else if (dy > 0 && b.min.y >= a.max.y - eps) dy = fminf(dy, b.min.y - a.max.y);
    }
    a.min.y += dy; a.max.y += dy;
    f32 dx = d.x;
    for (int i = 0; i < n; i++) {
        AABB b = boxes[i];
        if (a.max.y <= b.min.y + eps || a.min.y >= b.max.y - eps || a.max.z <= b.min.z + eps || a.min.z >= b.max.z - eps) continue;
        if (dx < 0 && b.max.x <= a.min.x + eps) dx = fmaxf(dx, b.max.x - a.min.x);
        else if (dx > 0 && b.min.x >= a.max.x - eps) dx = fminf(dx, b.min.x - a.max.x);
    }
    a.min.x += dx; a.max.x += dx;
    f32 dz = d.z;
    for (int i = 0; i < n; i++) {
        AABB b = boxes[i];
        if (a.max.y <= b.min.y + eps || a.min.y >= b.max.y - eps || a.max.x <= b.min.x + eps || a.min.x >= b.max.x - eps) continue;
        if (dz < 0 && b.max.z <= a.min.z + eps) dz = fmaxf(dz, b.max.z - a.min.z);
        else if (dz > 0 && b.min.z >= a.max.z - eps) dz = fminf(dz, b.min.z - a.max.z);
    }
    return v3(dx, dy, dz);
}

static void update_environment(Entity *e) {
    AABB a = entity_aabb(e);
    bool water = false, lava = false, cactus = false;
    vec3 wflow = v3(0, 0, 0), lflow = v3(0, 0, 0);   // summed current of the water / lava cells we touch
    int x0 = (int)floorf(a.min.x + 0.001f), x1 = (int)floorf(a.max.x - 0.001f);
    int y0 = (int)floorf(a.min.y + 0.001f), y1 = (int)floorf(a.max.y - 0.001f);
    int z0 = (int)floorf(a.min.z + 0.001f), z1 = (int)floorf(a.max.z - 0.001f);
    for (int y = y0; y <= y1; y++) for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++) {
        u16 b = world_get_block(x, y, z);
        if (block_is_water(b) && a.min.y < y + 0.875f) { water = true; wflow = v3_add(wflow, fluid_flow(x, y, z)); }
        if (block_is_lava(b)) { lava = true; lflow = v3_add(lflow, fluid_flow(x, y, z)); }
        if ((b == B_FIRE || b == B_SOUL_FIRE) && e->kind != ENT_XP && !effect_has(e, EFF_FIRE_RESISTANCE)) e->fire = MAXI(e->fire, 8 * TICKRATE);
        if (b == B_POWDER_SNOW && e->kind != ENT_ITEM) { e->vel.y = fmaxf(e->vel.y, -1.0f); e->vel.x *= 0.5f; e->vel.z *= 0.5f; if (e->age % (2 * TICKRATE) == 0 && g_player.inv.armor[3].id != I_LEATHER_BOOTS) entity_damage(e, 1, DMG_FREEZE, NULL); }
        if (b == B_COBWEB) { e->vel = v3_scale(e->vel, 0.25f); e->fall_dist = 0; }
        if (b == B_SWEET_BERRY_BUSH + 2 || b == B_SWEET_BERRY_BUSH + 3) { if (e->kind == ENT_PLAYER || e->kind == ENT_MOB) { if (e->age % 30 == 0 && (fabsf(e->vel.x) + fabsf(e->vel.z)) > 0.5f) entity_damage(e, 1, DMG_CACTUS, NULL); e->vel.x *= 0.8f; e->vel.z *= 0.8f; } }
        if (b == B_HONEY_BLOCK) { e->vel.x *= 0.6f; e->vel.z *= 0.6f; }
    }
    // standing on magma / campfires
    {
        u16 below = world_get_block((int)floorf(e->pos.x), (int)floorf(e->pos.y - 0.05f), (int)floorf(e->pos.z));
        if (BLOCKS[below].touch_damage && below != B_CACTUS && e->on_ground && !(e->kind == ENT_PLAYER && g_player.sneaking) && !(e->kind == ENT_MOB && mob_has(e->mob, MF_FIRE_IMMUNE)) && e->age % TICKRATE == 0 && e->kind != ENT_ITEM && e->kind != ENT_XP)
            entity_damage(e, BLOCKS[below].touch_damage, DMG_FIRE, NULL);
        if (below == B_SOUL_SAND && e->on_ground) { e->vel.x *= 0.6f; e->vel.z *= 0.6f; }
        if (below == B_SLIME_BLOCK && e->fall_dist > 0.5f && !(e->kind == ENT_PLAYER && g_player.sneaking)) { e->vel.y = sqrtf(2 * 32 * e->fall_dist) * 0.85f; e->fall_dist = 0; }
        if ((below == B_ICE || below == B_PACKED_ICE || below == B_BLUE_ICE) && e->on_ground) e->slippery = below == B_BLUE_ICE ? 0.989f : 0.98f; else e->slippery = 0;
    }
    // cactus pricks whoever touches its sides
    for (int y = y0; y <= y1 && !cactus; y++) for (int z = z0 - 1; z <= z1 + 1; z++) for (int x = x0 - 1; x <= x1 + 1; x++)
        if (world_get_block(x, y, z) == B_CACTUS) {
            AABB cb = {v3(x + 0.0625f - 0.02f, (f32)y, z + 0.0625f - 0.02f), v3(x + 0.9375f + 0.02f, y + 1.0f, z + 0.9375f + 0.02f)};
            if (aabb_overlap(a, cb)) { cactus = true; break; }
        }
    if (water && !e->in_water && e->vel.y < -4 && e->kind != ENT_XP) { sound_play_at(SND_SPLASH, e->pos, 0.6f, 1); fx_splash(e->pos, 12); }
    e->in_water = water; e->in_lava = lava;
    // Currents: flowing water (strongly) and lava (sluggishly) carry whatever floats in them, like Minecraft.
    // Boats read the flow themselves (vehicle.c); flyers, spectators and static things are not pushed.
    if ((water || lava) && !e->no_clip && !e->no_gravity && e->kind != ENT_VEHICLE && e->kind != ENT_DECOR && e->kind != ENT_ARROW && e->kind != ENT_PROJ && e->kind != ENT_CRYSTAL) {
        vec3 fl = water ? wflow : lflow;
        f32 len = sqrtf(fl.x * fl.x + fl.z * fl.z);
        if (len > 1e-4f) {
            f32 acc = water ? 3.0f : 1.2f;   // blocks/s^2: about 1.3 b/s of drift in water against its drag, like the real thing
            e->vel.x += fl.x / len * acc * DT;
            e->vel.z += fl.z / len * acc * DT;
        }
        if (water && fl.y < -0.1f && e->kind != ENT_PLAYER) e->vel.y -= 2.0f * DT;   // waterfalls drag things down
    }
    if (cactus && e->kind != ENT_ITEM && e->kind != ENT_XP) entity_damage(e, 1, DMG_CACTUS, NULL);
}

void entity_physics(Entity *e) {
    if (e->no_clip) {   // spectators pass through blocks
        e->pos = v3_add(e->pos, v3_scale(e->vel, DT));
        e->on_ground = false; e->in_water = e->in_lava = false; e->fall_dist = 0;
        return;
    }
    update_environment(e);
    if (!e->no_gravity) {
        if (e->in_water || e->in_lava) {
            e->vel.y -= (e->kind == ENT_ITEM || e->kind == ENT_XP ? 4.0f : 10.0f) * DT;
            f32 drag = e->in_lava ? 3.5f : 2.4f;
            e->vel = v3_scale(e->vel, 1.0f - drag * DT);
            if (e->kind == ENT_ITEM && e->in_water) e->vel.y += 6.0f * DT;   // items float
        } else {
            bool flutter = e->kind == ENT_MOB && (e->mob == MOB_CHICKEN || e->mob == MOB_PARROT) && e->vel.y < 0;
            f32 g = (flutter ? 12.0f : 32.0f) * dim_info(g_world.dim)->gravity;   // Space: low gravity
            if ((effect_has(e, EFF_SLOW_FALLING) || effect_has(e, EFF_BLESSING_OF_THE_SKY)) && e->vel.y < 0) g = 3.0f;
            if (effect_has(e, EFF_LEVITATION)) { e->vel.y = lerpf(e->vel.y, 0.9f * effect_level(e, EFF_LEVITATION), 0.1f); g = 0; }
            e->vel.y -= g * DT;
            e->vel.y *= g_world.dim == DIM_END ? 0.997f : 0.9933f;
            if (flutter && e->vel.y < -3.0f) e->vel.y = -3.0f;
            if ((effect_has(e, EFF_SLOW_FALLING) || effect_has(e, EFF_BLESSING_OF_THE_SKY)) && e->vel.y < -2.0f) e->vel.y = -2.0f;
            if (e->kind == ENT_MOB && mob_has(e->mob, MF_CLIMBS) && e->h_collision) e->fall_dist = 0;
        }
        if (e->vel.y < -78) e->vel.y = -78;
    }
    vec3 want = v3_scale(e->vel, DT);
    vec3 got = move_clipped(e, want);
    e->pos = v3_add(e->pos, got);
    bool hit_y = fabsf(got.y - want.y) > 1e-5f;
    e->on_ground = hit_y && want.y < 0;
    e->h_collision = fabsf(got.x - want.x) > 1e-5f || fabsf(got.z - want.z) > 1e-5f;
    if (hit_y) e->vel.y = 0;
    if (fabsf(got.x - want.x) > 1e-5f) e->vel.x = 0;
    if (fabsf(got.z - want.z) > 1e-5f) e->vel.z = 0;

    // fall damage bookkeeping
    if (e->in_water || e->in_lava || e->no_gravity) e->fall_dist = 0;
    else if (got.y < 0) e->fall_dist -= got.y;
    if (e->on_ground) {
        f32 fall = e->fall_dist * dim_info(g_world.dim)->gravity;   // low gravity means softer landings
        if (fall > 3.0f && (e->kind == ENT_MOB || e->kind == ENT_PLAYER)) {
            int dmg = (int)ceilf(fall - 3.0f);
            u16 land = world_get_block((int)floorf(e->pos.x), (int)floorf(e->pos.y - 0.2f), (int)floorf(e->pos.z));
            if (land == B_HAY_BLOCK) dmg = (int)(dmg * 0.2f);
            if (land == B_SLIME_BLOCK || land == B_CLOUD_BLOCK || block_is_water(world_get_block((int)floorf(e->pos.x), (int)floorf(e->pos.y), (int)floorf(e->pos.z)))) dmg = 0;
            if (BLOCKS[land].base >= B_WHITE_BED && BLOCKS[land].ui == UI_BED) dmg /= 2;
            dmg -= effect_level(e, EFF_JUMP_BOOST);
            if (dmg > 0 && !(e->kind == ENT_MOB && mob_has(e->mob, MF_NO_FALL)) && !effect_has(e, EFF_SLOW_FALLING) && !effect_has(e, EFF_BLESSING_OF_THE_SKY)) {
                entity_damage(e, (f32)dmg, DMG_FALL, NULL);
                u16 below = world_get_block((int)floorf(e->pos.x), (int)floorf(e->pos.y - 0.2f), (int)floorf(e->pos.z));
                sound_play_at(SND_LAND, e->pos, 0.6f, 1);
                if (below) fx_block_hit(iv3((int)floorf(e->pos.x), (int)floorf(e->pos.y - 0.2f), (int)floorf(e->pos.z)), FACE_U, below);
            }
        }
        e->fall_dist = 0;
    }
    // hazards
    if (e->in_lava && e->kind != ENT_XP) {
        if (e->kind == ENT_ITEM) { e->remove = true; fx_smoke(e->pos, 4, 0.2f, COL_WHITE); sound_play_at(SND_FIRE, e->pos, 0.4f, 1.5f); }
        else if (!(e->kind == ENT_MOB && mob_has(e->mob, MF_FIRE_IMMUNE)) && !effect_has(e, EFF_FIRE_RESISTANCE)) { entity_damage(e, 4, DMG_LAVA, NULL); e->fire = 15 * TICKRATE / 2; }
    }
    if (e->in_water) e->fire = 0;
    if (e->fire > 0 && e->kind != ENT_ITEM) {
        e->fire--;
        bool immune = (e->kind == ENT_MOB && mob_has(e->mob, MF_FIRE_IMMUNE)) || effect_has(e, EFF_FIRE_RESISTANCE);
        if (e->fire % TICKRATE == 0 && !immune) entity_damage(e, 1, DMG_FIRE, NULL);
        if (e->fire % 4 == 0) fx_flame(v3_add(e->pos, v3(frand_range(-e->width / 2, e->width / 2), frand() * e->height, frand_range(-e->width / 2, e->width / 2))));
    }
    if (e->slippery > 0 && e->on_ground) { e->vel.x *= 1.04f; e->vel.z *= 1.04f; }
    if (e->pos.y < -64) {
        if (e->kind == ENT_PLAYER || e->kind == ENT_MOB) entity_damage(e, 4, DMG_VOID, NULL);
        else e->remove = true;
    }
}

// ======================================================================== status effects
void effect_add(Entity *e, int eff, int amp, int ticks) {
    if (eff <= 0 || eff >= EFFECT_COUNT || !e) return;
    if (e->kind == ENT_MOB && mob_has(e->mob, MF_BOSS) && eff != EFF_INSTANT_DAMAGE) return;
    if (e->kind == ENT_MOB && mob_has(e->mob, MF_UNDEAD) && (eff == EFF_POISON || eff == EFF_REGENERATION)) return;
    if (e->kind == ENT_MOB && (e->mob == MOB_SPIDER || e->mob == MOB_CAVE_SPIDER) && eff == EFF_POISON) return;
    if (eff == EFF_INSTANT_HEALTH || eff == EFF_INSTANT_DAMAGE) {
        bool heal = (eff == EFF_INSTANT_HEALTH) != entity_is_undead(e);
        f32 amt = (f32)(4 << MINI(amp, 4));
        if (heal) entity_heal(e, amt); else { e->invuln = 0; entity_damage(e, amt * 1.5f, DMG_MAGIC, NULL); }
        return;
    }
    if (eff == EFF_SATURATION) { if (e->kind == ENT_PLAYER) { g_player.food = MINI(20, g_player.food + amp + 1); g_player.saturation = fminf((f32)g_player.food, g_player.saturation + 2.0f * (amp + 1)); } return; }
    if (eff == EFF_ABSORPTION) e->absorption = fmaxf(e->absorption, 4.0f * (amp + 1));
    if (amp > e->eff_amp[eff] || e->eff_time[eff] <= 0 || (amp == e->eff_amp[eff] && ticks > e->eff_time[eff])) {
        e->eff_amp[eff] = (u8)amp; e->eff_time[eff] = ticks;
    }
}
void effects_clear(Entity *e) { memset(e->eff_time, 0, sizeof e->eff_time); memset(e->eff_amp, 0, sizeof e->eff_amp); e->absorption = 0; }
void effects_tick(Entity *e) {
    for (int i = 1; i < EFFECT_COUNT; i++) {
        if (e->eff_time[i] <= 0) continue;
        int amp = e->eff_amp[i];
        int t = e->eff_time[i]--;
        switch (i) {
        case EFF_POISON: { int iv = MAXI(3, 75 >> amp); if (t % iv == 0 && e->health > 1) entity_damage(e, 1, DMG_POISON, NULL); } break;
        case EFF_WITHER: { int iv = MAXI(3, 120 >> amp); if (t % iv == 0) entity_damage(e, 1, DMG_WITHER, NULL); } break;
        case EFF_REGENERATION: { int iv = MAXI(3, 150 >> amp); if (t % iv == 0) entity_heal(e, 1); } break;
        case EFF_HUNGER: if (e->kind == ENT_PLAYER) player_exhaust(0.005f * (amp + 1) / 3.0f); break;
        case EFF_ABSORPTION: if (t == 1) e->absorption = 0; break;
        default: break;
        }
    }
}
void potion_apply(Entity *e, int potion, f32 scale) {
    if (potion < 0 || potion >= POTION_COUNT) return;
    const PotionDef *p = &POTIONS[potion];
    if (!p->effect) return;
    if (p->effect == EFF_INSTANT_HEALTH || p->effect == EFF_INSTANT_DAMAGE) {
        bool heal = (p->effect == EFF_INSTANT_HEALTH) != entity_is_undead(e);
        f32 amt = (f32)(4 << p->amplifier) * scale;
        if (heal) entity_heal(e, amt); else { e->invuln = 0; entity_damage(e, amt * 1.5f, DMG_MAGIC, NULL); }
        return;
    }
    effect_add(e, p->effect, p->amplifier, (int)(p->seconds * TICKRATE * scale));
}
void entity_heal(Entity *e, f32 amount) {
    if (!e || e->health <= 0) return;
    e->health = fminf(e->max_health, e->health + amount);
}

// ======================================================================== damage
static const char *damage_message(int type) {
    switch (type) {
    case DMG_FALL: return "hit the ground too hard";
    case DMG_LAVA: return "tried to swim in lava";
    case DMG_FIRE: return "burned to death";
    case DMG_DROWN: return "drowned";
    case DMG_VACUUM: return "could not breathe in the vacuum of space";
    case DMG_STAR: return "was struck down by starfire";
    case DMG_CRAMMING: return "was squished too much";
    case DMG_EXPLOSION: return "blew up";
    case DMG_VOID: return "fell out of the world";
    case DMG_STARVE: return "starved to death";
    case DMG_CACTUS: return "was pricked to death";
    case DMG_ARROW: return "was shot";
    case DMG_MAGIC: return "was killed by magic";
    case DMG_WITHER: return "withered away";
    case DMG_POISON: return "was poisoned";
    case DMG_FREEZE: return "froze to death";
    case DMG_SONIC: return "was obliterated by a sonically-charged shriek";
    case DMG_WALL: return "experienced kinetic energy";
    case DMG_FIREBALL: return "was fireballed";
    case DMG_SUFFOCATE: return "suffocated in a wall";
    case DMG_DRAGON: return "was scorched by wyrm's breath";
    default: return "died";
    }
}

static bool armor_applies(int type) { return type == DMG_MELEE || type == DMG_ARROW || type == DMG_EXPLOSION || type == DMG_CACTUS || type == DMG_FIREBALL || type == DMG_FIRE || type == DMG_LAVA; }

int g_blocks_shot, g_cramming_hits, g_effect_deaths, g_frozen_ents;
// on-death effects (Wind Charged, Weaving, Oozing) for mobs and players
void effects_on_death(Entity *e) {
    vec3 c = v3_add(e->pos, v3(0, e->height * 0.5f, 0));
    if (effect_has(e, EFF_WIND_CHARGED)) {   // a burst of wind throws everything nearby up and away
        g_effect_deaths++;
        ENT_FOREACH(o) if (o != e && (o->kind == ENT_MOB || o->kind == ENT_PLAYER || o->kind == ENT_ITEM) && v3_dist(o->pos, c) < 3.5f) { entity_knockback(o, c, 1.4f); o->vel.y += 10; }
        for (int i = 0; i < 16; i++) fx_smoke(v3_add(c, v3(frand_range(-1.5f, 1.5f), frand_range(-0.5f, 1.5f), frand_range(-1.5f, 1.5f))), 1, 0.2f, rgba_u32(220, 230, 255, 255));
    }
    if (effect_has(e, EFF_WEAVING)) {   // spins 2-3 cobwebs around where it died
        g_effect_deaths++;
        for (int i = 0, n = irand(2, 3), tries = 0; i < n && tries < 20; tries++) {
            ivec3 q = v3_floor(v3_add(e->pos, v3(frand_range(-1.5f, 1.5f), frand_range(0, 1.5f), frand_range(-1.5f, 1.5f))));
            if (block_is_air(world_get_block(q.x, q.y, q.z))) { world_set_block(q.x, q.y, q.z, B_COBWEB); i++; }
        }
    }
    if (effect_has(e, EFF_OOZING)) {   // two slimes spill out
        g_effect_deaths++;
        for (int i = 0; i < 2; i++) { Entity *s = mob_spawn(MOB_SLIME, v3_add(e->pos, v3(frand_range(-0.5f, 0.5f), 0.2f, frand_range(-0.5f, 0.5f)))); if (s) mob_set_size(s, 2); }
    }
}
bool entity_damage(Entity *e, f32 amount, int type, Entity *src) {
    if ((e->kind == ENT_MOB || e->kind == ENT_PLAYER) && effect_has(e, EFF_INFESTED) && e->health > 0 && type != DMG_CRAMMING && frand() < 0.1f) {   // Infested: silverfish burst out
        int n = irand(1, 2);
        for (int i = 0; i < n; i++) { Entity *s = mob_spawn(MOB_SILVERFISH, v3_add(e->pos, v3(frand_range(-0.5f, 0.5f), 0.3f, frand_range(-0.5f, 0.5f)))); if (s) g_effect_deaths++; }
    }
    if (e->kind == ENT_DECOR) { if (type == DMG_MELEE || type == DMG_ARROW || type == DMG_EXPLOSION || type == DMG_GENERIC) decor_hit(e, src); return true; }
    if (e->kind == ENT_MOB && MOB_DEFS[e->mob].ai == AIK_GOD && type != DMG_VOID) return false;   // the Luminaries cannot be harmed
    if (!e || e->remove || e->health <= 0 || amount <= 0) return false;
    if (e->kind == ENT_ITEM || e->kind == ENT_XP) {
        if (type == DMG_EXPLOSION || type == DMG_LAVA || type == DMG_FIRE) { if (!(e->kind == ENT_ITEM && (e->item.id == I_NETHER_STAR || (ITEMS[e->item.id].key && strstr(ITEMS[e->item.id].key, "netherite"))))) e->remove = true; }
        return false;
    }
    if (e->kind == ENT_CRYSTAL) { e->remove = true; explosion(e->pos, 6, e); return true; }
    if (e->kind == ENT_PROJ) {   // punching a ghast fireball sends it back
        if (src && (e->proj == PROJ_FIREBALL || e->proj == PROJ_SHULKER_BULLET)) {
            if (e->proj == PROJ_SHULKER_BULLET) { e->remove = true; return true; }
            vec3 d = src->kind == ENT_PLAYER ? v3(sinf(src->yaw) * cosf(src->pitch), sinf(src->pitch), cosf(src->yaw) * cosf(src->pitch)) : v3_norm(v3_sub(e->pos, src->pos));
            e->vel = v3_scale(d, 22); e->owner = src->id; e->from_player = src->kind == ENT_PLAYER;
            return true;
        }
        return false;
    }
    if (e->kind == ENT_ARROW || e->kind == ENT_TNT || e->kind == ENT_FANGS) return false;
    if (e->kind == ENT_PLAYER) {
        if (g_player.gamemode == GM_SPECTATOR) return false;
        if (g_player.gamemode == GM_CREATIVE && type != DMG_VOID) return false;
        if (type == DMG_FALL && !rule(GR_FALL_DAMAGE)) return false;
        if ((type == DMG_FIRE || type == DMG_LAVA) && !rule(GR_FIRE_DAMAGE)) return false;
        if (type == DMG_DROWN && !rule(GR_DROWNING_DAMAGE)) return false;
        if (type == DMG_FREEZE && !rule(GR_FREEZE_DAMAGE)) return false;
        // difficulty scales damage dealt to players by mobs (and their projectiles)
        Entity *att = src;
        if (att && (att->kind == ENT_ARROW || att->kind == ENT_PROJ)) att = entity_get(att->owner);
        if (att && att->kind == ENT_MOB) {
            switch (g_level.difficulty) {
            case DIFF_PEACEFUL: return false;
            case DIFF_EASY: amount = fminf(amount / 2 + 1, amount); break;
            case DIFF_HARD: amount *= 1.5f; break;
            default: break;
            }
        }
    }
    if (e->kind == ENT_MOB) {
        const MobDef *d = &MOB_DEFS[e->mob];
        if (mob_has(e->mob, MF_FIRE_IMMUNE) && (type == DMG_FIRE || type == DMG_LAVA || type == DMG_FIREBALL)) return false;
        if (e->mob == MOB_ENDERMAN && type == DMG_ARROW) return false;
        if (e->mob == MOB_WITHER && type == DMG_ARROW && e->health < e->max_health / 2) return false;
        if (e->mob == MOB_WITHER && e->phase == 0) return false;
        if (e->mob == MOB_CREAKING && !e->from_spawner && type != DMG_VOID) { e->hurt_time = 8; return false; }
        if (e->mob == MOB_SNOW_GOLEM && type == DMG_DROWN) amount = 1;
        if (MOB_DEFS[e->mob].ai == AIK_GHOST && type != DMG_VOID && type != DMG_MAGIC && type != DMG_GENERIC) amount *= entity_holy_source(src) ? 2.0f : 0.5f;
        if (src && src->kind == ENT_PROJ && src->proj == PROJ_SNOWBALL && e->mob == MOB_BLAZE) amount = 3;
        // natural mob armour: magic, void, starvation, wither and poison bypass it (like Minecraft)
        if (d->armor && type != DMG_MAGIC && type != DMG_VOID && type != DMG_STARVE && type != DMG_WITHER && type != DMG_POISON) amount *= 1.0f - fminf(20, (f32)d->armor) * 0.04f;
        if (e->tamed && src && src->kind == ENT_PLAYER) return false;
    }
    if (e->invuln > 0) {
        if (amount <= e->damage) return false;   // only the excess of a stronger hit lands
        f32 extra = amount - e->damage; e->damage = amount; amount = extra;
    } else e->damage = amount;
    if (e->kind == ENT_PLAYER) {
        // shield blocks frontal melee/projectile/explosion damage
        if (g_player.blocking && src && type != DMG_SONIC && (type == DMG_MELEE || type == DMG_ARROW || type == DMG_EXPLOSION || type == DMG_FIREBALL)) {
            vec3 look = v3(sinf(e->yaw), 0, cosf(e->yaw)), to = v3_norm(v3_sub(src->pos, e->pos));
            if (v3_dot(look, to) > 0.0f) {
                ItemStack *sh = g_player.inv.offhand.id == I_SHIELD ? &g_player.inv.offhand : inventory_held(&g_player.inv);
                if (sh->id == I_SHIELD) { sh->damage += (u16)(1 + amount); if (sh->damage >= ITEMS[I_SHIELD].durability) { *sh = (ItemStack){0}; sound_play(SND_BREAK_GLASS, 0.6f, 0.8f); } }
                sound_play(SND_HIT, 0.8f, 0.6f);
                if (src->kind == ENT_MOB) entity_knockback(src, e->pos, 0.5f);
                return false;
            }
        }
        if (armor_applies(type) && type != DMG_SONIC) {
            f32 armor = (f32)inventory_armor_points(&g_player.inv), tough = inventory_armor_toughness(&g_player.inv);
            f32 eff = fminf(20, fmaxf(armor / 5.0f, armor - 4.0f * amount / (tough + 8.0f)));
            amount *= 1.0f - eff / 25.0f;
            if (armor > 0) for (int i = 0; i < 4; i++) {
                ItemStack *a = &g_player.inv.armor[i];
                if (stack_empty(*a) || !ITEMS[a->id].durability) continue;
                int unb = ench_level(*a, ENCH_UNBREAKING);
                if (unb && frand() > 0.6f + 0.4f / (unb + 1)) continue;
                a->damage += (u16)fmaxf(1, amount / 4);
                if (a->damage >= ITEMS[a->id].durability) { *a = (ItemStack){0}; sound_play(SND_BREAK_GLASS, 0.6f, 0.8f); }
            }
        }
        {   // protection enchantments (enchantment protection factor, capped at 20)
            int epf = 0;
            for (int i = 0; i < 4; i++) {
                ItemStack a = g_player.inv.armor[i];
                if (type != DMG_VOID && type != DMG_STARVE && type != DMG_SONIC) epf += ench_level(a, ENCH_PROTECTION);
                if (type == DMG_FIRE || type == DMG_LAVA || type == DMG_FIREBALL) epf += 2 * ench_level(a, ENCH_FIRE_PROTECTION);
                if (type == DMG_EXPLOSION) epf += 2 * ench_level(a, ENCH_BLAST_PROTECTION);
                if (type == DMG_ARROW || type == DMG_FIREBALL) epf += 2 * ench_level(a, ENCH_PROJECTILE_PROTECTION);
                if (type == DMG_FALL) epf += 3 * ench_level(a, ENCH_FEATHER_FALLING);
            }
            amount *= 1.0f - MINI(20, epf) / 25.0f;
            // thorns
            if (src && (src->kind == ENT_MOB) && type == DMG_MELEE) for (int i = 0; i < 4; i++) {
                int th = ench_level(g_player.inv.armor[i], ENCH_THORNS);
                if (th && frand() < 0.15f * th) { entity_damage(src, (f32)irand(1, 4), DMG_MAGIC, NULL); g_player.inv.armor[i].damage += 2; }
            }
        }
    }
    if (type != DMG_VOID && type != DMG_STARVE) amount *= 1.0f - 0.2f * MINI(5, effect_level(e, EFF_RESISTANCE));
    if (e->absorption > 0) { f32 a = fminf(e->absorption, amount); e->absorption -= a; amount -= a; }
    if (e->kind == ENT_MOB && e->mob == MOB_VILLAGER && src && (src->kind == ENT_PLAYER || (entity_get(src->owner) && entity_get(src->owner)->kind == ENT_PLAYER))) villager_hurt_by_player(e, false);
    if (e->kind == ENT_PLAYER) g_stats[STAT_DMG_TAKEN] += (long long)ceilf(amount);
    else if (src && (src->kind == ENT_PLAYER || (entity_get(src->owner) && entity_get(src->owner)->kind == ENT_PLAYER))) g_stats[STAT_DMG_DEALT] += (long long)ceilf(amount);
    e->health -= amount;
    e->hurt_time = 18;
    e->invuln = 30;
    if (e->kind == ENT_MOB) {
        mob_on_hurt(e, src);
        fx_damage(v3_add(e->pos, v3(0, e->height, 0)), (int)fminf(8, amount / 2 + 1));
        if (e->health <= 0 && !e->death_time) {
            e->health = 0; e->death_time = 1;
            mob_on_death(e, src);
        }
    } else if (e->kind == ENT_PLAYER) {
        player_hurt_feedback(amount);
        player_exhaust(0.1f);
        if (e->health <= 0) {
            // totem of undying in either hand
            ItemStack *tot = g_player.inv.offhand.id == I_TOTEM_OF_UNDYING ? &g_player.inv.offhand : inventory_held(&g_player.inv)->id == I_TOTEM_OF_UNDYING ? inventory_held(&g_player.inv) : NULL;
            if (tot && type != DMG_VOID) {
                *tot = (ItemStack){0};
                e->health = 1; effects_clear(e);
                effect_add(e, EFF_REGENERATION, 1, 45 * TICKRATE); effect_add(e, EFF_ABSORPTION, 1, 5 * TICKRATE); effect_add(e, EFF_FIRE_RESISTANCE, 0, 40 * TICKRATE);
                fx_hearts(v3_add(e->pos, v3(0, 2, 0))); sound_play(SND_LEVELUP, 1, 1.4f);
                return true;
            }
            e->health = 0;
            char msg[128];
            Entity *who = src;
            if (who && (who->kind == ENT_ARROW || who->kind == ENT_PROJ)) who = entity_get(who->owner);
            if (who && who->kind == ENT_MOB) snprintf(msg, sizeof msg, "was %s by %s", type == DMG_ARROW ? "shot" : type == DMG_FIREBALL ? "fireballed" : type == DMG_EXPLOSION ? "blown up" : "slain", MOB_DEFS[who->mob].name);
            else snprintf(msg, sizeof msg, "%s", damage_message(type));
            player_on_death(msg);
        }
    }
    return true;
}

void entity_knockback(Entity *e, vec3 from, f32 strength) {
    if (e->kind == ENT_MOB) {
        if (mob_has(e->mob, MF_NO_KNOCKBACK)) return;
        strength *= 1.0f - MOB_DEFS[e->mob].kb_resist;
    }
    if (e->kind == ENT_PLAYER && g_player.inv.armor[1].id == I_NETHERITE_CHESTPLATE) strength *= 0.9f;
    vec3 d = v3_sub(e->pos, from); d.y = 0;
    d = v3_norm(d);
    if (v3_len(d) < 0.01f) d = v3(frand_range(-1, 1), 0, frand_range(-1, 1));
    e->vel.x = e->vel.x * 0.5f + d.x * strength * 7.0f;
    e->vel.z = e->vel.z * 0.5f + d.z * strength * 7.0f;
    if (e->on_ground || e->kind == ENT_PLAYER) e->vel.y = fminf(8.0f, e->vel.y * 0.5f + 5.5f * fminf(1.0f, strength + 0.3f));
}

f32 entity_light(const Entity *e) {
    if (e->kind == ENT_MOB && (mob_has(e->mob, MF_GLOWS) || e->mob == MOB_BLAZE || e->mob == MOB_ALLAY || e->mob == MOB_VEX || e->mob == MOB_GHOST)) return 1.0f;
    ivec3 p = v3_floor(v3_add(e->pos, v3(0, fminf(0.5f, e->height * 0.5f), 0)));
    u16 l = world_get_light(p.x, p.y, p.z);
    f32 day = g_world.dim == DIM_END ? 0.55f : world_daylight();
    f32 sky = L_SKY(l) ? powf(0.8f, 15.0f - L_SKY(l)) * day : 0;
    int bl = MAXI(L_R(l), MAXI(L_G(l), L_B(l)));
    f32 blk = bl ? powf(0.8f, 15.0f - bl) : 0;
    return fmaxf(dim_info(g_world.dim)->ambient, fmaxf(sky, blk));
}

// Holy sources hurt ghosts more: gold (the holy metal), Smite, and soul-fire items.
bool entity_holy_source(const Entity *src) {
    if (src && src->kind == ENT_PLAYER && effect_has(src, EFF_BLESSING_OF_LIGHT)) return true;   // blessed attackers strike with holy force
    if (!src) return false;
    if ((src->kind == ENT_ARROW || src->kind == ENT_PROJ) && src->fire > 0) return true;   // flaming arrows
    if (src->kind != ENT_PLAYER) return false;
    ItemStack h = *inventory_held((Inventory *)&g_player.inv);
    if (!h.id) return false;
    const char *k = ITEMS[h.id].key;
    if (ench_level(h, ENCH_SMITE) > 0) return true;
    return k && (!strncmp(k, "golden_", 7) || !strncmp(k, "divine_", 7) || !strncmp(k, "blessed_", 8) || strstr(k, "soul_") != NULL);
}

bool entity_can_see(const Entity *a, const Entity *b) {
    vec3 ea = entity_eye(a), eb = entity_eye(b);
    vec3 d = v3_sub(eb, ea);
    f32 len = v3_len(d);
    if (len < 0.01f) return true;
    RayHit h = world_raycast(ea, d, len, false);
    return !h.hit || (BLOCKS[h.block].opacity < 15 && !BLOCKS[h.block].opaque);
}

// ======================================================================== explosions
void explosion_ex(vec3 pos, f32 power, Entity *src, bool fire, bool breaks) {
    sculk_vibration(pos, 15, src && (src->kind == ENT_PLAYER || src->from_player));
    sound_play_at(SND_EXPLODE, pos, 1.6f, frand_range(0.8f, 1.0f));
    fx_explosion(pos, power);
    Entity *pl = player_ent();
    if (pl) fx_shake(fmaxf(0, 1.0f - v3_dist(pl->pos, pos) / 24.0f) * power * 0.35f);
    bool in_water = block_is_water(world_get_block((int)floorf(pos.x), (int)floorf(pos.y), (int)floorf(pos.z)));
    if (src && (src->kind == ENT_MOB || (src->kind == ENT_PROJ && entity_get(src->owner) && entity_get(src->owner)->kind == ENT_MOB)) && !rule(GR_MOB_GRIEFING)) breaks = false, fire = false;
    if (breaks && !in_water) {
        int r = (int)ceilf(power * 1.3f);
        ivec3 c = v3_floor(pos);
        for (int dy = -r; dy <= r; dy++) for (int dz = -r; dz <= r; dz++) for (int dx = -r; dx <= r; dx++) {
            f32 d = sqrtf((f32)(dx * dx + dy * dy + dz * dz));
            f32 reach = power * frand_range(0.7f, 1.25f);
            if (d > reach) continue;
            int x = c.x + dx, y = c.y + dy, z = c.z + dz;
            u16 b = world_get_block(x, y, z);
            if (!b || BLOCKS[b].hardness < 0 || BLOCKS[b].blast_resistance >= 20 * (1.0f - d / (reach + 0.01f)) + 18) continue;
            if (BLOCKS[b].liquid) continue;
            if (b == B_TNT) { world_set_block(x, y, z, B_AIR); tnt_spawn(v3(x + 0.5f, (f32)y, z + 0.5f), irand(10, 30)); continue; }
            world_set_block(x, y, z, B_AIR);
            // explosion drop decay game rules: blocks drop 1/power of the time, or always without decay
            bool decay = src && src->kind == ENT_TNT ? rule(GR_TNT_EXPLOSION_DROP_DECAY) : src && src->kind == ENT_MOB ? rule(GR_MOB_EXPLOSION_DROP_DECAY) : rule(GR_BLOCK_EXPLOSION_DROP_DECAY);
            if (!decay || frand() < 1.0f / power) game_hook_block_broken_naturally(iv3(x, y, z), b);
        }
        if (fire) for (int i = 0; i < (int)(power * 6); i++) {
            int x = c.x + irand(-r, r), y = c.y + irand(-r, r), z = c.z + irand(-r, r);
            if (block_is_air(world_get_block(x, y, z)) && BLOCKS[world_get_block(x, y - 1, z)].opaque && irand(0, 2) == 0) world_set_block(x, y, z, B_FIRE);
        }
    }
    ENT_FOREACH(e) {
        if (e == src || e->kind == ENT_XP || e->kind == ENT_ARROW || e->kind == ENT_FANGS) continue;
        vec3 ctr = v3_add(e->pos, v3(0, e->height * 0.5f, 0));
        f32 d = v3_dist(ctr, pos);
        if (d > power * 2) continue;
        f32 impact = 1.0f - d / (power * 2);
        if (!world_raycast(pos, v3_sub(ctr, pos), d, false).hit || d < 1.5f) {
            f32 dmg = (impact * impact + impact) * 0.5f * 7.0f * power + 1.0f;
            entity_damage(e, dmg, DMG_EXPLOSION, src);
            vec3 dir = v3_norm(v3_sub(ctr, pos));
            if (!(e->kind == ENT_MOB && mob_has(e->mob, MF_NO_KNOCKBACK))) e->vel = v3_add(e->vel, v3_scale(dir, impact * 14.0f));
        }
    }
}
void explosion(vec3 pos, f32 power, Entity *src) { explosion_ex(pos, power, src, false, true); }

// ======================================================================== constructors (projectiles & friends)
Entity *proj_spawn(int type, vec3 pos, vec3 vel, u32 owner) {
    Entity *e = entity_spawn(ENT_PROJ, pos);
    if (!e) return NULL;
    e->proj = (u8)type; e->vel = vel; e->owner = owner;
    e->width = e->height = (type == PROJ_FIREBALL || type == PROJ_DRAGON_FIREBALL) ? 1.0f : (type == PROJ_WITHER_SKULL || type == PROJ_STAR_BOLT ? 0.5f : 0.25f);
    Entity *o = entity_get(owner);
    e->from_player = o && o->kind == ENT_PLAYER;
    e->no_gravity = type == PROJ_FIREBALL || type == PROJ_SMALL_FIREBALL || type == PROJ_WITHER_SKULL || type == PROJ_DRAGON_FIREBALL || type == PROJ_SHULKER_BULLET || type == PROJ_WIND_CHARGE || type == PROJ_STAR_BOLT;
    if (type == PROJ_HOLY_WATER) e->width = e->height = 0.3f;
    if (type == PROJ_BOBBER) e->width = e->height = 0.25f;
    return e;
}
Entity *fangs_spawn(vec3 pos, int delay, u32 owner) {
    Entity *e = entity_spawn(ENT_FANGS, pos);
    if (!e) return NULL;
    e->owner = owner; e->fuse = delay; e->width = 0.5f; e->height = 0.8f;
    return e;
}
Entity *crystal_spawn(vec3 pos) {
    Entity *e = entity_spawn(ENT_CRYSTAL, pos);
    if (!e) return NULL;
    e->width = e->height = 2.0f; e->health = e->max_health = 1; e->persistent = true;
    return e;
}

static void proj_hit(Entity *e, Entity *victim, vec3 at) {
    Entity *owner = entity_get(e->owner);
    switch (e->proj) {
    case PROJ_SNOWBALL:
        if (victim) { entity_damage(victim, victim->kind == ENT_MOB && victim->mob == MOB_BLAZE ? 3 : 0.01f, DMG_ARROW, e); entity_knockback(victim, e->pos, 0.3f); }
        fx_poof(at, 0.3f, 0.3f);
        break;
    case PROJ_EGG:
        if (victim) entity_damage(victim, 0.01f, DMG_ARROW, e);
        if (irand(0, 7) == 0) { int n = irand(0, 31) == 0 ? 4 : 1; for (int i = 0; i < n; i++) { Entity *c = mob_spawn(MOB_CHICKEN, at); if (c) { c->baby = true; c->grow_age = -24000 * 3; mob_set_size(c, 1); } } }
        fx_poof(at, 0.3f, 0.3f);
        break;
    case PROJ_ENDER_PEARL:
        if (owner && owner->health > 0) {
            fx_poof(owner->pos, owner->width, owner->height);
            owner->pos = owner->prev_pos = v3_add(at, v3(0, 0.1f, 0));
            owner->vel = v3(0, 0, 0); owner->fall_dist = 0;
            entity_damage(owner, 5, DMG_FALL, NULL);
            if (frand() < 0.05f) mob_spawn(MOB_ENDERMITE, at);
            sound_play_at(SND_POP, at, 1, 0.5f);
        }
        break;
    case PROJ_SMALL_FIREBALL:
        if (victim) { if (entity_damage(victim, 5, DMG_FIREBALL, owner ? owner : e)) victim->fire = MAXI(victim->fire, 5 * TICKRATE); }
        else { ivec3 p = v3_floor(v3_sub(at, v3_scale(v3_norm(e->vel), 0.3f))); if (block_is_air(world_get_block(p.x, p.y, p.z))) world_set_block(p.x, p.y, p.z, B_FIRE); }
        break;
    case PROJ_FIREBALL:
        if (victim) entity_damage(victim, 6, DMG_FIREBALL, owner ? owner : e);
        explosion_ex(at, 1.0f, e, true, g_world.dim != DIM_END);
        break;
    case PROJ_WITHER_SKULL:
        if (victim) { if (entity_damage(victim, 8, DMG_WITHER, owner ? owner : e)) effect_add(victim, EFF_WITHER, 1, 10 * TICKRATE); }
        explosion_ex(at, 1.0f, e, false, true);
        break;
    case PROJ_DRAGON_FIREBALL:
        ENT_FOREACH(o) if ((o->kind == ENT_PLAYER || o->kind == ENT_MOB) && o != owner && v3_dist(o->pos, at) < 3.5f) entity_damage(o, 6, DMG_DRAGON, owner);
        for (int i = 0; i < 30; i++) fx_smoke(v3_add(at, v3(frand_range(-3, 3), frand_range(0, 1), frand_range(-3, 3))), 1, 0.1f, rgba_u32(200, 80, 220, 255));
        break;
    case PROJ_HOLY_WATER:   // blessed splash: burns undead and every ghost, lifts Haunted and puts out fires
        g_holy_splashes++;
        sound_play_at(SND_BREAK_GLASS, at, 1, 1.2f);
        for (int i = 0; i < 24; i++) fx_smoke(v3_add(at, v3(frand_range(-2, 2), frand_range(0, 1.5f), frand_range(-2, 2))), 1, 0.1f, i & 1 ? rgba_u32(255, 240, 170, 255) : rgba_u32(220, 240, 255, 255));
        ENT_FOREACH(o) {
            if ((o->kind != ENT_PLAYER && o->kind != ENT_MOB) || v3_dist(v3_add(o->pos, v3(0, o->height * 0.5f, 0)), at) > 3.5f) continue;
            if (o->kind == ENT_PLAYER) { o->eff_time[EFF_HAUNTED] = 0; o->fire = 0; continue; }
            if (MOB_DEFS[o->mob].ai == AIK_GHOST || entity_is_undead(o)) entity_damage(o, 8, DMG_MAGIC, owner ? owner : e);
        }
        break;
    case PROJ_STAR_BOLT: {   // Zorvath's starfire: a small burst; color 1 = falling meteor (bigger, hurts more)
        f32 dmg = e->damage > 0 ? e->damage : 6, r = e->color ? 3.0f : 1.6f;
        ENT_FOREACH(o) {
            if (o->kind != ENT_PLAYER && o->kind != ENT_MOB) continue;
            if (o == owner || (o->kind == ENT_MOB && (mob_is_space_minion(o->mob) || o->mob == MOB_COSMIC_TYRANT))) continue;
            if (v3_dist(v3_add(o->pos, v3(0, o->height * 0.5f, 0)), at) < r + o->width * 0.5f) entity_damage(o, dmg, DMG_STAR, owner ? owner : e);
        }
        for (int i = 0; i < (e->color ? 40 : 14); i++) fx_smoke(v3_add(at, v3(frand_range(-r, r), frand_range(0, r * 0.6f), frand_range(-r, r))), 1, 0.15f, i & 1 ? rgba_u32(255, 210, 80, 255) : rgba_u32(150, 110, 255, 255));
        if (e->color) { fx_explosion(at, 1); sound_play_at(SND_EXPLODE, at, 0.6f, 1.3f); }
    } break;
    case PROJ_SHULKER_BULLET:
        if (victim) { entity_damage(victim, 4, DMG_ARROW, owner ? owner : e); effect_add(victim, EFF_LEVITATION, 0, 10 * TICKRATE); }
        break;
    case PROJ_LLAMA_SPIT:
        if (victim) entity_damage(victim, 1, DMG_ARROW, owner ? owner : e);
        break;
    case PROJ_POTION: {
        int pot = e->item.damage;
        ENT_FOREACH(o) {
            if (o->kind != ENT_PLAYER && o->kind != ENT_MOB) continue;
            f32 d = v3_dist(v3_add(o->pos, v3(0, o->height * 0.5f, 0)), at);
            if (d < 4) potion_apply(o, pot, o == victim ? 1.0f : 1.0f - d / 4.0f);
        }
        u32 c = POTIONS[pot % POTION_COUNT].color;
        for (int i = 0; i < 24; i++) fx_smoke(v3_add(at, v3(frand_range(-1.5f, 1.5f), frand_range(0, 1), frand_range(-1.5f, 1.5f))), 1, 0.2f, rgba_u32((c >> 16) & 255, (c >> 8) & 255, c & 255, 255));
        sound_play_at(SND_BREAK_GLASS, at, 0.8f, 1.2f);
    } break;
    case PROJ_XP_BOTTLE:
        xp_drop(at, irand(3, 11));
        sound_play_at(SND_BREAK_GLASS, at, 0.8f, 1.2f);
        break;
    case PROJ_WIND_CHARGE:
        if (victim) entity_damage(victim, 1, DMG_ARROW, owner ? owner : e);
        ENT_FOREACH(o) {
            if (o->kind != ENT_PLAYER && o->kind != ENT_MOB && o->kind != ENT_ITEM) continue;
            f32 d = v3_dist(o->pos, at);
            if (d < 2.5f) { vec3 dir = v3_norm(v3_sub(v3_add(o->pos, v3(0, 0.5f, 0)), at)); o->vel = v3_add(o->vel, v3_scale(dir, 14.0f * (1 - d / 2.5f))); o->vel.y += 6; o->fall_dist = 0; }
        }
        fx_poof(at, 1.5f, 1.5f);
        sound_play_at(SND_SWING, at, 1, 0.5f);
        break;
    case PROJ_FIREWORK:
        explosion_ex(at, 0.3f, e, false, false);
        break;
    case PROJ_EYE: break;
    default: break;
    }
    e->remove = true;
}


// ------------------------------------------------------------------ fishing
// The bobber floats on water and waits for a bite: 5..30 s (Minecraft: 100..600 game ticks), minus 5 s per Lure
// level, a little faster in rain. A bite is a 1 s window (the bobber dips and splashes); reeling in during it
// catches something: junk 10 % - 2.5 %/Luck of the Sea, treasure 5 % + 2.1 %/level, otherwise fish.
// Reeling a hooked mob pulls it toward you. Rod wear: 1 per catch, 2 when snagged in the ground, 5 for a mob.
// Bobber fields: target = hooked entity, special_timer = wait, phase = 1 while biting, color = Lure, anger = Luck.
#ifndef MC
#define MC(t) ((t) * TICKRATE / 20)
#endif
int g_fish_caught, g_fish_junk, g_fish_treasure, g_fish_hooked;
static int bite_wait(Entity *b) {
    int w = irand(MC(100), MC(600)) - b->color * MC(100);
    if (weather_raining_at((int)floorf(b->pos.x), (int)floorf(b->pos.y) + 1, (int)floorf(b->pos.z))) w = w * 4 / 5;
    return MAXI(MC(20), w);
}
static bool water_at(vec3 p) { ivec3 q = v3_floor(p); return block_is_water(world_get_block(q.x, q.y, q.z)); }
static void bobber_tick(Entity *e) {
    Entity *owner = entity_get(e->owner);
    if (!owner || owner->health <= 0 || v3_dist(owner->pos, e->pos) > 32 || inventory_held(&g_player.inv)->id != I_FISHING_ROD) {
        e->remove = true; if (g_player.bobber == e->id) g_player.bobber = 0; return;
    }
    Entity *hk = e->target ? entity_get(e->target) : NULL;
    if (hk && hk->health > 0) { e->pos = v3_add(hk->pos, v3(0, hk->height * 0.8f, 0)); e->vel = v3(0, 0, 0); return; }
    e->target = 0;
    if (e->stuck) return;   // snagged in the ground
    if (water_at(e->pos) || water_at(v3_add(e->pos, v3(0, -0.15f, 0)))) {   // floating
        bool under = water_at(v3_add(e->pos, v3(0, 0.15f, 0)));
        e->vel.x *= 0.85f; e->vel.z *= 0.85f;
        e->vel.y = under ? 1.2f : -0.2f;
        if (e->phase) e->vel.y = -1.5f;   // a fish is pulling
        e->pos = v3_add(e->pos, v3_scale(e->vel, DT));
        if (e->special_timer <= 0 && !e->phase) e->special_timer = bite_wait(e);
        if (--e->special_timer == 0) {
            if (!e->phase) { e->phase = 1; e->special_timer = MC(20); sound_play_at(SND_SPLASH, e->pos, 0.6f, 1.3f);
                             for (int i = 0; i < 8; i++) fx_smoke(v3_add(e->pos, v3(frand_range(-0.4f, 0.4f), 0.1f, frand_range(-0.4f, 0.4f))), 1, 0.05f, rgba_u32(220, 235, 255, 255)); }
            else { e->phase = 0; e->special_timer = bite_wait(e); }   // it got away
        }
        return;
    }
    e->vel.y -= 12.0f * DT;
    vec3 step = v3_scale(e->vel, DT);
    f32 len = v3_len(step);
    if (len > 1e-5f) {
        RayHit h = world_raycast(e->pos, step, len, false);
        if (h.hit) { e->pos = v3_add(e->pos, v3_scale(v3_norm(step), fmaxf(0, h.dist - 0.05f))); e->vel = v3(0, 0, 0); e->stuck = true; return; }
    }
    ENT_FOREACH(o) {   // hook a mob on the way
        if (o->kind != ENT_MOB || o->health <= 0) continue;
        if (aabb_overlap(entity_aabb(o), (AABB){v3_sub(e->pos, v3(0.15f, 0.15f, 0.15f)), v3_add(e->pos, v3(0.15f, 0.15f, 0.15f))})) { e->target = o->id; g_fish_hooked++; return; }
    }
    e->pos = v3_add(e->pos, step);
}
static const char *FISH_LOOT[] = {"cod", "cod", "cod", "cod", "cod", "cod", "salmon", "salmon", "salmon", "pufferfish", "tropical_fish"};
static const char *JUNK_LOOT[] = {"lily_pad", "bowl", "leather", "leather_boots", "rotten_flesh", "stick", "string", "bone", "ink_sac", "tripwire_hook", "glass_bottle"};
static const char *TREASURE_LOOT[] = {"*bow", "*enchanted_book", "*fishing_rod", "name_tag", "nautilus_shell", "saddle"};
// reel in: returns rod durability to spend
int fishing_reel(Entity *player) {
    Entity *b = entity_get(g_player.bobber);
    g_player.bobber = 0;
    if (!b) return 0;
    b->remove = true;
    Entity *hk = b->target ? entity_get(b->target) : NULL;
    if (hk && hk->health > 0) {
        vec3 d = v3_sub(player->pos, hk->pos);
        hk->vel = v3_add(hk->vel, v3(d.x * 1.0f, d.y * 1.0f + sqrtf(v3_len(d)) * 0.8f, d.z * 1.0f));
        return 5;
    }
    if (b->stuck) return 2;
    if (!b->phase) return 0;
    int luck = b->anger + effect_level(player, EFF_LUCK);
    f32 r = frand(), junk = fmaxf(0, 0.10f - 0.025f * luck), treasure = 0.05f + 0.021f * luck;
    const char *key; bool ench = false;
    if (r < junk) { key = JUNK_LOOT[irand(0, (int)ARRAY_LEN(JUNK_LOOT) - 1)]; g_fish_junk++; }
    else if (r < junk + treasure) { key = TREASURE_LOOT[irand(0, (int)ARRAY_LEN(TREASURE_LOOT) - 1)]; g_fish_treasure++; }
    else key = FISH_LOOT[irand(0, (int)ARRAY_LEN(FISH_LOOT) - 1)];
    if (key[0] == '*') { key++; ench = true; }
    int id = item_by_key(key);
    if (id > 0) {
        ItemStack st = stack_of(id, 1);
        if (ench && id == I_ENCHANTED_BOOK) { for (int t = 0; t < 40; t++) { int en = irand(1, ENCH_COUNT - 1); if (ENCHANTS[en].curse) continue; ench_add(&st, en, irand(1, ENCHANTS[en].max_level)); break; } }
        else if (ench) { u8 out[MAX_ENCH][2]; memset(out, 0, sizeof out); int n = ench_select(id, irand(22, 30), (u32)irand(1, 1 << 30), out, MAX_ENCH); for (int i = 0; i < n; i++) ench_add(&st, out[i][0], out[i][1]); }
        if (id == I_LEATHER_BOOTS) st.damage = (u16)irand(0, ITEMS[id].durability * 9 / 10);
        vec3 d = v3_sub(player->pos, b->pos);
        item_drop(b->pos, st, v3(d.x * 1.0f, d.y * 1.0f + sqrtf(v3_len(d)) * 0.8f + 2, d.z * 1.0f), 0);
    }
    xp_drop(player->pos, irand(1, 6));
    g_fish_caught++; g_stats[STAT_FISH]++;
    sound_play_at(SND_SPLASH, b->pos, 0.7f, 1.0f);
    return 1;
}

static void proj_tick(Entity *e) {
    if (e->proj == PROJ_BOBBER) { bobber_tick(e); return; }
    Entity *owner = entity_get(e->owner);
    if (e->proj == PROJ_SHULKER_BULLET) {   // homing
        Entity *t = entity_get(e->target);
        if (t && t->health > 0) e->vel = v3_lerp(e->vel, v3_scale(v3_norm(v3_sub(v3_add(t->pos, v3(0, t->height * 0.5f, 0)), e->pos)), 8.0f), 0.08f);
        if (e->age > 30 * TICKRATE) e->remove = true;
    }
    if (e->proj == PROJ_EYE) {   // eye of ender floats toward the stronghold, then drops or shatters
        e->vel.y = e->age < 20 ? 4 : (e->age < 60 ? 0.5f : -2);
        if (e->age > 3 * TICKRATE / 2) {
            e->remove = true;
            if (frand() < 0.8f) item_drop(e->pos, stack_of(I_ENDER_EYE, 1), v3(0, 0, 0), 10);
            else { fx_poof(e->pos, 0.3f, 0.3f); sound_play_at(SND_BREAK_GLASS, e->pos, 0.7f, 1.5f); }
            return;
        }
        e->pos = v3_add(e->pos, v3_scale(e->vel, DT));
        if (e->age % 2 == 0) fx_smoke(e->pos, 1, 0.05f, rgba_u32(150, 60, 200, 255));
        return;
    }
    if (e->proj == PROJ_FIREWORK) {
        e->vel.y = fminf(30, e->vel.y + 30 * DT);
        e->pos = v3_add(e->pos, v3_scale(e->vel, DT));
        if (e->age % 2 == 0) fx_smoke(e->pos, 1, 0.05f, COL_WHITE);
        if (e->age > irand(TICKRATE, 2 * TICKRATE)) {
            e->remove = true;
            for (int i = 0; i < 40; i++) fx_smoke(v3_add(e->pos, v3(frand_range(-2, 2), frand_range(-2, 2), frand_range(-2, 2))), 1, 0.3f, rgba_u32(irand(100, 255), irand(100, 255), irand(100, 255), 255));
            sound_play_at(SND_EXPLODE, e->pos, 0.5f, 1.8f);
        }
        return;
    }
    vec3 step = v3_scale(e->vel, DT);
    f32 len = v3_len(step);
    if (len < 1e-5f) { e->pos.y -= 0.01f; return; }
    RayHit h = world_raycast(e->pos, step, len, e->proj == PROJ_POTION);
    f32 hit_t = h.hit ? h.dist : len;
    Entity *victim = NULL; f32 best = hit_t;
    ENT_FOREACH(o) {
        if ((o->kind != ENT_MOB && o->kind != ENT_PLAYER && o->kind != ENT_CRYSTAL) || o->health <= 0) continue;
        if (o == owner && e->age < 10) continue;
        if (owner && owner->kind == ENT_MOB && o->kind == ENT_MOB && o->mob == owner->mob && e->proj != PROJ_SNOWBALL) continue;
        AABB box = entity_aabb(o);
        f32 grow = e->width * 0.5f;
        box.min = v3_sub(box.min, v3(grow, grow, grow)); box.max = v3_add(box.max, v3(grow, grow, grow));
        f32 t = aabb_ray(box, e->pos, v3_norm(step));
        if (t >= 0 && t <= best) { best = t; victim = o; }
    }
    if (victim || h.hit) { proj_hit(e, victim, v3_add(e->pos, v3_scale(v3_norm(step), best))); return; }
    e->pos = v3_add(e->pos, step);
    if (!e->no_gravity) e->vel.y -= (e->proj == PROJ_POTION || e->proj == PROJ_XP_BOTTLE || e->proj == PROJ_HOLY_WATER ? 20.0f : 12.0f) * DT;
    e->vel = v3_scale(e->vel, e->no_gravity ? 1.0f : 0.997f);
    if (e->proj == PROJ_FIREBALL || e->proj == PROJ_SMALL_FIREBALL) { if (e->age % 3 == 0) fx_flame(e->pos); }
    if (e->proj == PROJ_WITHER_SKULL && e->age % 3 == 0) fx_smoke(e->pos, 1, 0.05f, rgba_u32(30, 30, 30, 255));
    if (e->proj == PROJ_DRAGON_FIREBALL && e->age % 2 == 0) fx_smoke(e->pos, 1, 0.1f, rgba_u32(200, 80, 220, 255));
    if (e->proj == PROJ_STAR_BOLT && e->age % 2 == 0) fx_smoke(e->pos, e->color ? 3 : 1, e->color ? 0.3f : 0.08f, e->age & 2 ? rgba_u32(255, 220, 110, 255) : rgba_u32(170, 130, 255, 255));
    if (e->pos.y < -64 || e->age > 60 * TICKRATE) e->remove = true;
}

static void fangs_tick(Entity *e) {
    if (--e->fuse > 0) return;
    if (e->fuse == 0) {
        Entity *owner = entity_get(e->owner);
        ENT_FOREACH(o) if ((o->kind == ENT_PLAYER || o->kind == ENT_MOB) && o != owner && !(o->kind == ENT_MOB && (o->mob == MOB_EVOKER || o->mob == MOB_VEX)) && aabb_overlap(entity_aabb(o), entity_aabb(e))) entity_damage(o, 6, DMG_MAGIC, owner);
        sound_play_at(SND_HIT, e->pos, 0.7f, 0.6f);
    }
    if (e->fuse < -20) e->remove = true;
}

static void crystal_tick(Entity *e) {
    if (e->age % 4 == 0 && frand() < 0.3f) fx_smoke(v3_add(e->pos, v3(0, 1, 0)), 1, 0.2f, rgba_u32(230, 150, 230, 255));
    // keeps fire burning beneath itself
    ivec3 p = v3_floor(e->pos);
    if (block_is_air(world_get_block(p.x, p.y, p.z)) && BLOCKS[world_get_block(p.x, p.y - 1, p.z)].solid) world_set_block(p.x, p.y, p.z, B_FIRE);
}

// ======================================================================== per-kind ticks
static void item_tick(Entity *e) {
    Entity *pl = player_ent();
    if (e->pickup_delay > 0) e->pickup_delay--;
    if (pl && pl->health > 0 && e->pickup_delay == 0 && !g_player.dead && g_player.gamemode != GM_SPECTATOR) {
        vec3 to = v3_sub(v3_add(pl->pos, v3(0, 0.6f, 0)), e->pos);
        f32 d = v3_len(to);
        if (d < 1.8f) e->vel = v3_add(e->vel, v3_scale(v3_norm(to), 30.0f * DT));
        if (d < 1.0f) {
            ItemStack s = e->item;
            int left = inventory_add(&g_player.inv, s);
            if (left < s.count) {
                sound_play(SND_POP, 0.35f, frand_range(1.4f, 2.0f));
                if (left == 0) e->remove = true; else e->item.count = (u8)left;
            }
        }
    }
    entity_physics(e);
    if (e->on_ground) { f32 f = e->slippery > 0 ? 0.98f : 0.85f; e->vel.x *= f; e->vel.z *= f; }
    else { e->vel.x *= 0.995f; e->vel.z *= 0.995f; }
    if (e->age % 20 == 0) ENT_FOREACH(o) {
        if (o <= e || o->kind != ENT_ITEM || !stack_can_merge(o->item, e->item)) continue;
        if (v3_dist(o->pos, e->pos) > 1.2f) continue;
        int room = stack_max(e->item) - e->item.count;
        int n = MINI(room, o->item.count);
        if (n <= 0) continue;
        e->item.count += n; o->item.count -= n;
        if (!o->item.count) o->remove = true;
    }
    if (e->age > 5 * 60 * TICKRATE) e->remove = true;
}

static void xp_tick(Entity *e) {
    Entity *pl = player_ent();
    if (pl && pl->health > 0 && e->age > 12) {
        vec3 to = v3_sub(v3_add(pl->pos, v3(0, 0.8f, 0)), e->pos);
        f32 d = v3_len(to);
        if (d < 8) { f32 pull = (1.0f - d / 8.0f); e->vel = v3_add(e->vel, v3_scale(v3_norm(to), pull * pull * 60.0f * DT)); }
        if (d < 1.1f) {
            // mending: experience repairs a damaged mending item in hand or armour first
            int xp = e->xp;
            ItemStack *cands[6] = {inventory_held(&g_player.inv), &g_player.inv.offhand, &g_player.inv.armor[0], &g_player.inv.armor[1], &g_player.inv.armor[2], &g_player.inv.armor[3]};
            for (int i = 0; i < 6 && xp > 0; i++) {
                ItemStack *m = cands[i];
                if (stack_empty(*m) || !m->damage || !ench_level(*m, ENCH_MENDING)) continue;
                int fix = MINI(m->damage, xp * 2);
                m->damage -= (u16)fix; xp -= (fix + 1) / 2;
            }
            if (xp > 0) player_add_xp(xp);
            sound_play(SND_XP, 0.25f, frand_range(0.8f, 1.3f)); e->remove = true; return;
        }
    }
    entity_physics(e);
    if (e->on_ground) { e->vel.x *= 0.85f; e->vel.z *= 0.85f; }
    if (e->age > 5 * 60 * TICKRATE) e->remove = true;
}

static void arrow_tick(Entity *e) {
    bool trident = e->item.id == I_TRIDENT;
    if (e->stuck && trident && e->phase && e->age > 10) {   // loyalty: fly back to the thrower
        Entity *pl = player_ent();
        if (pl && pl->id == e->owner) {
            vec3 to = v3_sub(entity_eye(pl), e->pos);
            f32 d = v3_len(to);
            if (d < 1.2f) { if (inventory_add(&g_player.inv, e->item) == 0) { sound_play(SND_POP, 0.4f, 1.2f); e->remove = true; return; } }
            e->pos = v3_add(e->pos, v3_scale(to, fminf(1.0f, (0.05f + 0.05f * e->phase) * 8 / fmaxf(d, 1))));
            e->yaw = atan2f(-to.x, -to.z);
            return;
        }
    }
    if (e->stuck) {
        Entity *pl = player_ent();
        if (e->from_player && pl && v3_dist(pl->pos, e->pos) < 1.5f && e->age > 20) {
            ItemStack give = stack_empty(e->item) ? stack_of(I_ARROW, 1) : e->item;
            if (g_player.gamemode == GM_CREATIVE && !trident) { e->remove = true; return; }
            if (inventory_add(&g_player.inv, give) == 0) { sound_play(SND_POP, 0.3f, 1.6f); e->remove = true; }
        }
        ivec3 b = v3_floor(v3_add(e->pos, v3_scale(v3_norm(v3(sinf(e->yaw), 0, cosf(e->yaw))), 0.1f)));
        if (!BLOCKS[world_get_block(b.x, b.y, b.z)].solid && !BLOCKS[world_get_block((int)floorf(e->pos.x), (int)floorf(e->pos.y), (int)floorf(e->pos.z))].solid) e->stuck = false;
        if (e->age > 60 * TICKRATE && !trident) e->remove = true;
        return;
    }
    vec3 step = v3_scale(e->vel, DT);
    f32 len = v3_len(step);
    RayHit h = world_raycast(e->pos, step, len, false);
    f32 hit_t = h.hit ? h.dist : len;
    Entity *victim = NULL; f32 best = hit_t;
    ENT_FOREACH(o) {
        if (o->kind != ENT_MOB && o->kind != ENT_PLAYER && o->kind != ENT_CRYSTAL) continue;
        if (o->health <= 0 || (o->id == e->owner && e->age < 10)) continue;
        AABB box = entity_aabb(o);
        box.min = v3_sub(box.min, v3(0.15f, 0.15f, 0.15f)); box.max = v3_add(box.max, v3(0.15f, 0.15f, 0.15f));
        f32 t = aabb_ray(box, e->pos, v3_norm(step));
        if (t >= 0 && t <= best) { best = t; victim = o; }
    }
    if (victim) {
        f32 speed = v3_len(e->vel);
        f32 dmg = trident ? 8 : ceilf(e->damage * speed / 45.0f * 3.0f);
        if (e->color == 1) dmg += irand(0, (int)(dmg / 2) + 2);   // critical (fully drawn bow)
        Entity *owner = entity_get(e->owner);
        if (trident) dmg = e->damage > 0 ? e->damage : 8;
        if (entity_damage(victim, fmaxf(1, dmg), DMG_ARROW, owner ? owner : e)) {
            entity_knockback(victim, v3_sub(victim->pos, v3_norm(e->vel)), 0.4f + 0.6f * e->size);
            sound_play_at(SND_ARROW_HIT, victim->pos, 0.8f, 1.2f);
            if (e->item.id == I_TIPPED_ARROW) potion_apply(victim, e->item.damage, 0.125f);
            if (e->item.id == I_SPECTRAL_ARROW) effect_add(victim, EFF_GLOWING, 0, 10 * TICKRATE);
            if (e->fire > 0) victim->fire = MAXI(victim->fire, 5 * TICKRATE);
        }
        if (trident) { e->vel = v3_scale(e->vel, -0.1f); e->vel.y = -2; if (e->phase) { e->stuck = true; e->age = 0; } return; }
        if (e->special_timer > 0) { e->special_timer--; e->pos = v3_add(e->pos, v3_scale(v3_norm(e->vel), victim->width + 0.3f)); return; }   // piercing
        e->remove = true;
        return;
    }
    if (h.hit) {
        int hk = BLOCKS[h.block].base;
        if (rule(GR_PROJECTILES_CAN_BREAK_BLOCKS) && (hk == B_DECORATED_POT || hk == B_CHORUS_FLOWER || hk == B_POINTED_DRIPSTONE)) {   // fragile blocks shatter
            world_set_block(h.pos.x, h.pos.y, h.pos.z, B_AIR); game_hook_block_broken_naturally(h.pos, h.block); g_blocks_shot++;
        }
        e->pos = v3_add(e->pos, v3_scale(v3_norm(step), h.dist - 0.05f));
        e->stuck = true; e->vel = v3(0, 0, 0); e->age = 0;
        sound_play_at(SND_ARROW_HIT, e->pos, 0.6f, 1.0f);
        if (BLOCKS[h.block].base == B_TARGET) fx_crit(e->pos, 6);
        return;
    }
    e->pos = v3_add(e->pos, step);
    e->vel.y -= 20.0f * DT;
    e->vel = v3_scale(e->vel, e->in_water ? (trident ? 0.99f : 0.97f) : 0.9983f);
    e->yaw = atan2f(e->vel.x, e->vel.z);
    e->pitch = atan2f(e->vel.y, sqrtf(e->vel.x * e->vel.x + e->vel.z * e->vel.z));
    u16 b = world_get_block((int)floorf(e->pos.x), (int)floorf(e->pos.y), (int)floorf(e->pos.z));
    e->in_water = block_is_water(b);
    if (block_is_lava(b) || b == B_FIRE) e->fire = 100;
    if (e->pos.y < -64 || e->age > 30 * TICKRATE) e->remove = true;
}

static void tnt_tick(Entity *e) {
    entity_physics(e);
    if (e->on_ground) { e->vel.x *= 0.7f; e->vel.z *= 0.7f; }
    if (e->age % 5 == 0) fx_smoke(v3_add(e->pos, v3(0, 1.0f, 0)), 1, 0.05f, COL_WHITE);
    if (--e->fuse <= 0) { e->remove = true; if (rule(GR_TNT_EXPLODES)) explosion(v3_add(e->pos, v3(0, 0.5f, 0)), 4.0f, e); }
}

// A falling block obeys gravity (and water drag), crushes anyone under an anvil, and turns back into a block when it lands.
static void falling_tick(Entity *e) {
    u16 b = (u16)e->xp;
    if (!b || b >= BLOCK_STATES) { e->remove = true; return; }
    f32 speed = -e->vel.y;
    entity_physics(e);
    if (e->in_water) { e->vel.x *= 0.9f; e->vel.z *= 0.9f; }
    const char *key = BLOCKS[b].key;
    if (key && strstr(key, "anvil") && speed > 3.0f) {   // anvils hurt: 2 per block fallen, up to 40
        AABB a = entity_aabb(e);
        ENT_FOREACH(t) {
            if (t == e || (t->kind != ENT_MOB && t->kind != ENT_PLAYER) || t->death_time > 0) continue;
            if (!aabb_overlap(a, entity_aabb(t))) continue;
            if (t->invuln > 0) continue;
            f32 fall = fminf(20.0f, speed * speed / 64.0f + 1.0f);
            entity_damage(t, fminf(40.0f, 2.0f * ceilf(fall)), DMG_GENERIC, NULL);
            t->invuln = 10;
        }
    }
    int x = (int)floorf(e->pos.x), z = (int)floorf(e->pos.z);
    if (e->age > 20 * 60 * 3 || e->pos.y < -8.0f) { e->remove = true; return; }
    if (!e->on_ground && !(e->h_collision && e->vel.y == 0)) return;
    // landed: settle into the cell we are standing in
    int y = (int)floorf(e->pos.y + 0.05f);
    e->remove = true;
    ivec3 p = iv3(x, y, z);
    u16 here = world_get_block(x, y, z);
    if (block_is_air(here) || BLOCKS[here].replaceable || BLOCKS[here].liquid) {
        world_set_block(x, y, z, b);
        fx_block_hit(p, FACE_U, b);
        if (key && strstr(key, "anvil")) sound_play_at(SND_ANVIL_LAND, v3(x + 0.5f, y + 0.5f, z + 0.5f), 1.0f, 1.0f);
        else sound_play_at(SND_DIG_STONE + MINI(BLOCKS[b].sound, SG_SNOW), v3(x + 0.5f, y + 0.5f, z + 0.5f), 0.7f, 0.8f);
    } else {
        game_hook_block_broken_naturally(p, b);   // nowhere to land (a torch, a slab...): it breaks into its drops
    }
}

void entities_tick(void) {
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = &g_ents.e[i];
        if (!e->id) continue;
        if (e->remove) {
            if (e->kind == ENT_MOB && g_ents.boss == e->id) g_ents.boss = 0;
            if (e->kind != ENT_PLAYER) memset(e, 0, sizeof *e);
            continue;
        }
        if (e->kind == ENT_PLAYER) continue;   // ticked by the player module
        e->prev_pos = e->pos;
        {   // beyond the simulation distance entities freeze (bosses always tick)
            int sd = g_world.sim_dist > 0 ? g_world.sim_dist : 8;
            int ecx = (int)floorf(e->pos.x) >> 4, ecz = (int)floorf(e->pos.z) >> 4;
            if ((abs(ecx - g_world.sim_cx) > sd || abs(ecz - g_world.sim_cz) > sd) && e->id != g_ents.boss) { g_frozen_ents++; continue; }
        }
        e->prev_body_yaw = e->body_yaw; e->prev_head_yaw = e->head_yaw; e->prev_pitch = e->pitch;
        e->prev_limb_amount = e->limb_amount; e->prev_swell = e->swell;
        e->age++;
        if (e->hurt_time > 0) e->hurt_time--;
        if (e->invuln > 0) e->invuln--;
        if (!world_loaded((int)floorf(e->pos.x), (int)floorf(e->pos.z))) continue;
        switch (e->kind) {
        case ENT_MOB: mob_tick(e); break;
        case ENT_ITEM: item_tick(e); break;
        case ENT_XP: xp_tick(e); break;
        case ENT_ARROW: arrow_tick(e); break;
        case ENT_TNT: tnt_tick(e); break;
        case ENT_FALLING: falling_tick(e); break;
        case ENT_PROJ: proj_tick(e); break;
        case ENT_FANGS: fangs_tick(e); break;
        case ENT_CRYSTAL: crystal_tick(e); break;
        case ENT_VEHICLE: vehicle_tick(e); break;
        case ENT_DECOR: decor_tick(e); break;
        default: break;
        }
        if (e->rider && !e->remove) ride_update_rider(e);
    }
}

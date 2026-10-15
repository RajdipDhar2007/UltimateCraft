// Mob behaviour for every Minecraft mob. Each MobDef (generated) names an AI kind; the
// behaviours below are composed from shared steering/attack helpers. Times written as
// "MC(n)" are Minecraft ticks (20/s) converted to engine ticks (60/s).
#include "entity.h"
#include "../game/raids.h"
#include "../world/rules.h"
#include "../world/structures.h"
#include "../game/game.h"
#include "vehicle.h"
#include "villager.h"
#include "../world/world.h"
#include "../game/player.h"
#include "../game/fx.h"
#include "../audio/audio.h"

#define MC(t) ((t) * TICKRATE / 20)

int mob_by_key(const char *key) {
    if (!key) return -1;
    if (!strncmp(key, "minecraft:", 10)) key += 10;
    for (int i = 0; i < MOB_COUNT; i++) if (!strcmp(MOB_DEFS[i].key, key)) return i;
    return -1;
}
bool mob_hostile(int t) {
    switch (MOB_DEFS[t].ai) {
    case AIK_HOSTILE: case AIK_RANGED: case AIK_CREEPER: case AIK_PHANTOM: case AIK_GUARDIAN: case AIK_SHULKER: case AIK_WARDEN:
    case AIK_CREAKING: case AIK_BOSS_WITHER: case AIK_BOSS_DRAGON: case AIK_EVOKER: case AIK_WITCH: case AIK_BREEZE: case AIK_BLAZE:
    case AIK_GHAST: case AIK_SILVERFISH: case AIK_SPIDER: case AIK_ENDERMAN: case AIK_GHOST: return true;
    case AIK_SLIME: return t != MOB_SULFUR_CUBE;
    case AIK_FLYER: return t == MOB_VEX;
    case AIK_NEUTRAL: return t == MOB_PIGLIN || t == MOB_ZOMBIFIED_PIGLIN;
    case AIK_FISH: return t == MOB_ZOMBIE_NAUTILUS;
    default: return false;
    }
}
bool entity_is_undead(const Entity *e) { return e->kind == ENT_MOB && mob_has(e->mob, MF_UNDEAD); }

f32 mob_scale(const Entity *e) {
    f32 s = MOB_DEFS[e->mob].scale;
    if (e->baby) s *= 0.5f;
    if (MOB_DEFS[e->mob].ai == AIK_SLIME) s *= e->size * 0.5f;
    return s;
}
void mob_set_size(Entity *e, int size) {
    const MobDef *d = &MOB_DEFS[e->mob];
    e->size = (u8)size;
    f32 s = mob_scale(e) / d->scale;
    e->width = d->width * s; e->height = d->height * s;
    if (d->ai == AIK_SLIME) { e->width = e->height = 0.52f * size; e->max_health = e->health = (f32)(size * size); }
}

// ======================================================================== sounds
// Every creature has its own voice kind (MobDef.sound, an index into the audio bank): idle, hurt and death clips.
static void mob_sound(Entity *e, int which) {
    int kind = MOB_DEFS[e->mob].sound;
    if (kind < 0 || kind >= MOB_SOUND_KINDS) kind = 0;
    if (which == 0 && (kind == 0 || kind == 3)) return;   // generic creatures and creepers make no idle noise
    f32 base = MOB_DEFS[e->mob].pitch;
    f32 pitch = (1.0f + (base - 1.0f) * 0.6f) * (e->baby ? 1.35f : 1.0f) * frand_range(0.93f, 1.07f);   // each voice already sits in its own register
    sound_play_at(SND_MOB(kind, which), e->pos, 1, pitch);
}

// ======================================================================== spawning one mob
static const u8 SHEEP_COLORS[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 7, 15, 12};
// difficulty scaling for UltimateCraft bosses (health and damage)
static f32 boss_diff_mul(void) { return g_level.difficulty == DIFF_EASY ? 0.75f : g_level.difficulty == DIFF_HARD ? 1.5f : 1.0f; }
bool mob_is_space_minion(int type) { return type == MOB_VOID_DRIFTER || type == MOB_ASTEROID_CRAWLER; }
int g_tyrant_pulls, g_tyrant_summons, g_tyrant_meteors;
Entity *mob_spawn(int type, vec3 pos) {
    if (type < 0 || type >= MOB_COUNT) return NULL;
    Entity *e = entity_spawn(ENT_MOB, pos);
    if (!e) return NULL;
    const MobDef *d = &MOB_DEFS[type];
    e->mob = (u8)type;
    e->width = d->width; e->height = d->height;
    e->health = e->max_health = d->health;
    e->yaw = e->body_yaw = e->head_yaw = frand_range(-PI_F, PI_F);
    e->ai = AI_IDLE; e->ai_timer = irand(20, 120);
    e->persistent = mob_has(type, MF_PERSIST);
    e->no_gravity = mob_has(type, MF_FLYING) || d->ai == AIK_SHULKER;
    e->no_clip = type == MOB_VEX;
    e->egg_timer = MC(irand(6000, 12000));
    e->anchor = pos;
    if (type == MOB_SHEEP) e->color = SHEEP_COLORS[irand(0, (int)ARRAY_LEN(SHEEP_COLORS) - 1)];
    if (d->ai == AIK_SLIME) mob_set_size(e, type == MOB_SULFUR_CUBE ? irand(1, 2) : (1 << irand(0, 2)));
    if ((type == MOB_ZOMBIE || type == MOB_HUSK || type == MOB_DROWNED || type == MOB_ZOMBIE_VILLAGER) && frand() < 0.05f) { e->baby = true; mob_set_size(e, 1); }
    if (type == MOB_DROWNED && frand() < 0.0625f) e->item = stack_of(I_TRIDENT, 1);
    if (type == MOB_VEX) e->special_timer = MC(irand(600, 2380));
    if (type == MOB_WITHER) { e->phase = 0; e->special_timer = MC(220); e->health = 1; }
    if (type == MOB_ENDER_DRAGON) { g_ents.boss = e->id; e->phase = 0; }
    if (type == MOB_COSMIC_TYRANT) { e->health = e->max_health = d->health * boss_diff_mul(); g_ents.boss = e->id; e->phase = 0; e->special_timer = 0; }
    if (type == MOB_WANDERING_TRADER) { e->special_timer = MC(irand(48000, 72000)); villager_set_profession(e, PROF_WANDERER); }
    if (type == MOB_DELVARI_MERCHANT) villager_set_profession(e, PROF_DELVARI);
    if (type == MOB_SIFT_SOUL) villager_set_profession(e, PROF_SOUL);
    if (type == MOB_GOD_ZEPHYRINE) villager_set_profession(e, PROF_ZEPHYR);
    if (type == MOB_THALOR_AVATAR || type == MOB_BRIMSTONE_REVENANT) { e->health = e->max_health = d->health * boss_diff_mul(); e->phase = 0; }
    if (type == MOB_DEEP_KEEPER) { e->health = e->max_health = d->health * boss_diff_mul(); e->phase = 0; }
    if (type == MOB_VILLAGER) villager_clear(e);
    return e;
}
static Entity *spawn_baby(int type, vec3 pos) {
    Entity *b = mob_spawn(type, pos);
    if (!b) return NULL;
    b->baby = true; b->grow_age = -MC(24000);
    mob_set_size(b, b->size ? b->size : 1);
    return b;
}

// ======================================================================== helpers
static f32 approach_angle(f32 a, f32 b, f32 max) { f32 d = wrap_angle(b - a); return a + clampf(d, -max, max); }
static f32 yaw_to(vec3 from, vec3 to) { return atan2f(to.x - from.x, to.z - from.z); }
static f32 hdist(vec3 a, vec3 b) { f32 dx = a.x - b.x, dz = a.z - b.z; return sqrtf(dx * dx + dz * dz); }
static Entity *player_target(void) {
    Entity *p = player_ent();
    if (!p || p->health <= 0 || g_player.gamemode == GM_CREATIVE || g_player.gamemode == GM_SPECTATOR || g_player.dead) return NULL;
    return p;
}
static bool wears_gold(void) {
    for (int i = 0; i < 4; i++) { int id = g_player.inv.armor[i].id; if (id == I_GOLDEN_HELMET || id == I_GOLDEN_CHESTPLATE || id == I_GOLDEN_LEGGINGS || id == I_GOLDEN_BOOTS) return true; }
    return false;
}
static f32 detect_range(Entity *e, Entity *t) {
    f32 r = 16;
    if (t && effect_has(t, EFF_INVISIBILITY)) r *= 0.3f;
    if (t && t->kind == ENT_PLAYER && g_player.sneaking) r *= 0.8f;
    if (e->mob == MOB_ENDER_DRAGON || e->mob == MOB_WITHER || e->mob == MOB_GHAST) r = 64;
    return r;
}
static bool is_dangerous(int x, int y, int z) {
    Block b = world_get_block(x, y, z), below = world_get_block(x, y - 1, z);
    if (block_is_lava(b) || block_is_lava(below) || b == B_FIRE || BLOCKS[b].touch_damage || BLOCKS[below].touch_damage) return true;
    if (!BLOCKS[below].solid && !BLOCKS[world_get_block(x, y - 2, z)].solid && !BLOCKS[world_get_block(x, y - 3, z)].solid && !block_is_water(below)) return true;
    return false;
}
static bool tempted(Entity *e, Entity **out) {
    Entity *p = player_ent();
    if (!p || p->health <= 0) return false;
    const MobDef *d = &MOB_DEFS[e->mob];
    int held = inventory_held(&g_player.inv)->id;
    if (!held) return false;
    for (int i = 0; i < 7 && d->food[i]; i++) if (d->food[i] == held) {
        if (v3_dist(p->pos, e->pos) < 10) { *out = p; return true; }
    }
    return false;
}

// ground steering: walk toward yaw at speed; jumps over 1-block obstacles; avoids cliffs/lava
static bool d_ok_for_paths(const Entity *e) {   // walkers use A*; climbers, swimmers and slimes keep their own movement
    int ai = MOB_DEFS[e->mob].ai;
    return !mob_has(e->mob, MF_CLIMBS) && ai != AIK_GHOST && ai != AIK_SHULKER && !e->mount;
}
static void steer(Entity *e, f32 yaw, f32 speed) {
    e->yaw = e->body_yaw = approach_angle(e->body_yaw, yaw, 0.25f);
    f32 sp = speed * (1.0f + 0.2f * effect_level(e, EFF_SPEED) - 0.15f * effect_level(e, EFF_SLOWNESS));
    if (e->baby && MOB_DEFS[e->mob].ai != AIK_PASSIVE) sp *= 1.5f;
    vec3 f = v3(sinf(e->yaw), 0, cosf(e->yaw));
    ivec3 ahead = v3_floor(v3_add(e->pos, v3_scale(f, e->width * 0.5f + 0.6f)));
    bool safe = e->ai == AI_CHASE || e->ai == AI_ATTACK || !is_dangerous(ahead.x, ahead.y, ahead.z);
    if (!safe) { sp = 0; e->ai_timer = 0; }
    f32 acc = e->on_ground ? 0.3f : (e->in_water ? 0.15f : 0.06f);
    e->vel.x = lerpf(e->vel.x, f.x * sp, acc);
    e->vel.z = lerpf(e->vel.z, f.z * sp, acc);
    if (e->h_collision && (e->on_ground || e->in_water) && e->jump_cooldown <= 0) {
        if (mob_has(e->mob, MF_CLIMBS)) e->vel.y = 3.5f;
        else { e->vel.y = 8.4f + 1.5f * effect_level(e, EFF_JUMP_BOOST); e->jump_cooldown = 10; }
    }
    if (e->h_collision && mob_has(e->mob, MF_CLIMBS)) e->vel.y = fmaxf(e->vel.y, 3.0f);
    // break leaves in the way (ravagers)
    if (e->mob == MOB_RAVAGER && e->h_collision) for (int y = 0; y < 3; y++) {
        Block b = world_get_block(ahead.x, (int)e->pos.y + y, ahead.z);
        if (block_is_leaves(b) || BLOCKS[b].mesh == MESH_CROSS) world_set_block(ahead.x, (int)e->pos.y + y, ahead.z, B_AIR);
    }
}
bool path_next(Entity *e, vec3 goal, vec3 *wp);   // path.c (A*)
static void walk_to(Entity *e, vec3 goal, f32 speed) {
    if (!mob_has(e->mob, MF_FLYING) && !e->in_water && d_ok_for_paths(e) && v3_dist(e->pos, goal) > 1.5f) {
        vec3 wp; if (path_next(e, goal, &wp)) { steer(e, yaw_to(e->pos, wp), speed); return; }
    }
    steer(e, yaw_to(e->pos, goal), speed);
}
static void stop(Entity *e) { e->vel.x *= 0.7f; e->vel.z *= 0.7f; }
static void look_at(Entity *e, vec3 p) {
    e->head_yaw = approach_angle(e->head_yaw, yaw_to(e->pos, p), 0.3f);
    vec3 d = v3_sub(p, entity_eye(e));
    e->pitch = atan2f(d.y, sqrtf(d.x * d.x + d.z * d.z));
}
static void fly_to(Entity *e, vec3 goal, f32 speed, f32 acc) {
    vec3 d = v3_sub(goal, e->pos);
    f32 len = v3_len(d);
    vec3 want = len > 0.3f ? v3_scale(d, speed / len) : v3(0, 0, 0);
    e->vel = v3_lerp(e->vel, want, acc);
    if (len > 0.3f && (fabsf(d.x) + fabsf(d.z)) > 0.1f) e->yaw = e->body_yaw = approach_angle(e->body_yaw, atan2f(d.x, d.z), 0.2f);
}
static vec3 random_goal(Entity *e, int r, int dy) {
    vec3 g = v3_add(e->pos, v3(frand_range(-r, r), frand_range(-dy, dy), frand_range(-r, r)));
    return g;
}
static bool in_water_at(vec3 p) { return block_is_water(world_get_block((int)floorf(p.x), (int)floorf(p.y), (int)floorf(p.z))); }
static f32 reach_to(Entity *e, Entity *t) { return e->width * 0.5f + t->width * 0.5f + 0.75f + (e->width > 1.5f ? 0.5f : 0); }

static void apply_hit_effect(Entity *e, Entity *t) {
    const MobDef *d = &MOB_DEFS[e->mob];
    if (d->effect && d->ai != AIK_RANGED) effect_add(t, d->effect, d->effect_amp, MC(d->effect_secs * 20));
}
static bool melee(Entity *e, Entity *t, f32 dmg) {
    if (e->attack_cooldown > 0 || !t || t->health <= 0) return false;
    if (v3_dist(v3_add(e->pos, v3(0, e->height * 0.5f, 0)), v3_add(t->pos, v3(0, t->height * 0.5f, 0))) > reach_to(e, t) + e->height * 0.25f) return false;
    dmg += 3 * effect_level(e, EFF_STRENGTH) - 4 * effect_level(e, EFF_WEAKNESS);
    if (e->baby && e->mob != MOB_ZOMBIE) dmg *= 0.5f;
    e->attack_cooldown = MC(20);
    e->attack_anim = 1;
    if (entity_damage(t, fmaxf(0, dmg), DMG_MELEE, e)) {
        f32 kb = 0.4f;
        if (e->mob == MOB_IRON_GOLEM || e->mob == MOB_RAVAGER || e->mob == MOB_HOGLIN || e->mob == MOB_ZOGLIN) { kb = 0.6f; t->vel.y += 8; }
        if (e->mob == MOB_WARDEN) kb = 1.0f;
        entity_knockback(t, e->pos, kb * (1.0f - MOB_DEFS[t->kind == ENT_MOB ? t->mob : 0].kb_resist * (t->kind == ENT_MOB)));
        apply_hit_effect(e, t);
        if (e->fire > 0 || e->mob == MOB_BLAZE) t->fire = MAXI(t->fire, MC(80));
        if (e->mob == MOB_BEE) { e->special_timer = MC(irand(600, 1200)); e->sheared = true; }   // stung: will die soon
        sound_play_at(SND_HIT, t->pos, 0.6f, 1);
    }
    return true;
}

static vec3 aim_velocity(vec3 from, vec3 to, f32 speed, f32 gravity) {
    vec3 d = v3_sub(to, from);
    f32 hd = sqrtf(d.x * d.x + d.z * d.z);
    f32 t = hd / speed;
    d.y += 0.5f * gravity * t * t;
    return v3_scale(v3_norm(d), speed);
}
static void shoot(Entity *e, Entity *t, int proj) {
    vec3 from = v3_add(entity_eye(e), v3_scale(v3(sinf(e->yaw), 0, cosf(e->yaw)), 0.4f));
    vec3 to = v3_add(t->pos, v3(0, t->height * 0.6f, 0));
    const MobDef *d = &MOB_DEFS[e->mob];
    if (proj == PROJ_ARROW) {
        vec3 v = aim_velocity(from, to, 38.0f, 20.0f);
        v = v3_add(v, v3(frand_range(-0.6f, 0.6f), frand_range(-0.4f, 0.4f), frand_range(-0.6f, 0.6f)));
        Entity *a = arrow_spawn(from, v, e->id, d->attack, false);
        if (a && d->effect) {
            int pot = d->effect == EFF_SLOWNESS ? POT_SLOWNESS : d->effect == EFF_POISON ? POT_POISON : d->effect == EFF_WEAKNESS ? POT_WEAKNESS : 0;
            if (pot) { a->item = stack_of(I_TIPPED_ARROW, 1); a->item.damage = (u16)pot; }
        }
        sound_play_at(SND_BOW, e->pos, 0.7f, frand_range(0.8f, 1.2f));
        return;
    }
    if (proj == PROJ_TRIDENT) {
        Entity *a = arrow_spawn(from, aim_velocity(from, to, 45.0f, 20.0f), e->id, 8, false);
        if (a) a->item = stack_of(I_TRIDENT, 1);
        return;
    }
    f32 speed = proj == PROJ_FIREBALL || proj == PROJ_WITHER_SKULL || proj == PROJ_DRAGON_FIREBALL ? 18.0f : proj == PROJ_SHULKER_BULLET ? 8.0f : 24.0f;
    f32 grav = (proj == PROJ_POTION || proj == PROJ_SNOWBALL || proj == PROJ_LLAMA_SPIT) ? 20.0f : 0;
    vec3 v = aim_velocity(from, to, speed, grav);
    if (proj == PROJ_SMALL_FIREBALL) v = v3_add(v, v3(frand_range(-2, 2), frand_range(-1, 1), frand_range(-2, 2)));
    Entity *p = proj_spawn(proj, from, v, e->id);
    if (!p) return;
    p->target = t->id;
    p->damage = d->attack;
    if (proj == PROJ_POTION) {   // witches pick a harmful potion
        f32 dist = v3_dist(e->pos, t->pos);
        p->item.damage = (u16)(dist > 8 && !effect_has(t, EFF_SLOWNESS) ? POT_SLOWNESS : (t->health >= 8 && !effect_has(t, EFF_POISON)) ? POT_POISON :
                               (dist <= 3 && !effect_has(t, EFF_WEAKNESS) && frand() < 0.25f) ? POT_WEAKNESS : POT_HARMING);
    }
    sound_play_at(proj == PROJ_SNOWBALL || proj == PROJ_POTION ? SND_SWING : SND_FIRE, e->pos, 0.8f, 1);
}

// target acquisition for hostiles: the player, or (golems/wolves/zoglins) other mobs
static Entity *find_mob_target(Entity *e, f32 range, bool (*pred)(Entity *self, Entity *o));
static bool pred_raid_victim(Entity *s, Entity *o) { (void)s; return o->mob == MOB_VILLAGER || o->mob == MOB_WANDERING_TRADER || o->mob == MOB_IRON_GOLEM; }
static Entity *find_target(Entity *e) {
    Entity *cur = entity_get(e->target);
    if (cur && cur->health > 0 && v3_dist(cur->pos, e->pos) < detect_range(e, cur) * 1.5f) {
        if (cur->kind != ENT_PLAYER || player_target()) return cur;
    }
    e->target = 0;
    Entity *p = player_target();
    if (p && v3_dist(p->pos, e->pos) < detect_range(e, p) && (entity_can_see(e, p) || v3_dist(p->pos, e->pos) < 4)) {
        e->target = p->id;
        return p;
    }
    if (e->raid_id && !(e->raid_id >> 31)) {   // raiders in a raid go after villagers and golems too
        Entity *v = find_mob_target(e, 32, pred_raid_victim);
        if (v) { e->target = v->id; return v; }
    }
    return NULL;
}
static Entity *find_mob_target(Entity *e, f32 range, bool (*pred)(Entity *self, Entity *o)) {
    Entity *best = NULL; f32 bd = range;
    ENT_FOREACH(o) {
        if (o == e || o->kind != ENT_MOB || o->health <= 0 || !pred(e, o)) continue;
        f32 dd = v3_dist(o->pos, e->pos);
        if (dd < bd && entity_can_see(e, o)) { bd = dd; best = o; }
    }
    return best;
}
static bool pred_hostile_not_creeper(Entity *s, Entity *o) { (void)s; return mob_hostile(o->mob) && o->mob != MOB_CREEPER && o->mob != MOB_ENDERMAN; }
static bool pred_wolf_prey(Entity *s, Entity *o) { (void)s; return o->mob == MOB_SHEEP || o->mob == MOB_RABBIT || o->mob == MOB_FOX || o->mob == MOB_SKELETON || o->mob == MOB_STRAY || o->mob == MOB_WITHER_SKELETON || o->mob == MOB_LLAMA; }
static bool pred_any_mob(Entity *s, Entity *o) { return o->mob != s->mob && o->mob != MOB_CREEPER && !mob_has(o->mob, MF_BOSS); }
static bool pred_small_slime(Entity *s, Entity *o) { (void)s; return (o->mob == MOB_SLIME || o->mob == MOB_MAGMA_CUBE) && o->size == 1; }
static bool pred_fish(Entity *s, Entity *o) { (void)s; return o->mob == MOB_COD || o->mob == MOB_SALMON || o->mob == MOB_TROPICAL_FISH || o->mob == MOB_PUFFERFISH || o->mob == MOB_DROWNED || o->mob == MOB_GUARDIAN || o->mob == MOB_ELDER_GUARDIAN || o->mob == MOB_SQUID || o->mob == MOB_GLOW_SQUID; }
static bool pred_villager(Entity *s, Entity *o) { (void)s; return o->mob == MOB_VILLAGER || o->mob == MOB_WANDERING_TRADER; }
static bool pred_zombie(Entity *s, Entity *o) { (void)s; return o->mob == MOB_ZOMBIE || o->mob == MOB_HUSK || o->mob == MOB_DROWNED || o->mob == MOB_ZOMBIE_VILLAGER || o->mob == MOB_VINDICATOR || o->mob == MOB_PILLAGER || o->mob == MOB_EVOKER || o->mob == MOB_VEX || o->mob == MOB_RAVAGER; }
static bool pred_cat(Entity *s, Entity *o) { (void)s; return o->mob == MOB_CAT || o->mob == MOB_OCELOT; }

// ======================================================================== behaviours
static void wander(Entity *e, f32 speed, int range) {
    if (e->raid_id && hdist(e->pos, e->anchor) > 6) {   // raiders march on the village; patrols follow their route
        e->ai = AI_WANDER; e->goal = e->anchor;
        walk_to(e, e->goal, speed * (e->raid_id >> 31 ? 0.5f : 0.8f));
        return;
    }
    if (--e->ai_timer <= 0) {
        if (e->ai == AI_WANDER) { e->ai = AI_IDLE; e->ai_timer = irand(MC(40), MC(160)); }
        else { e->ai = AI_WANDER; e->goal = random_goal(e, range, 0); e->ai_timer = irand(MC(40), MC(120)); }
    }
    if (e->ai == AI_WANDER && hdist(e->pos, e->goal) > 0.8f) walk_to(e, e->goal, speed * 0.55f);
    else stop(e);
    if (e->ai == AI_IDLE && frand() < 0.01f) e->head_yaw = approach_angle(e->head_yaw, e->body_yaw + frand_range(-1, 1), 0.5f);
}
static void flee_from(Entity *e, vec3 from, f32 speed) { steer(e, yaw_to(from, e->pos), speed); }

static void tamed_behaviour(Entity *e, f32 speed) {
    Entity *owner = player_ent();
    if (!owner) return;
    if (e->sitting) { stop(e); look_at(e, entity_eye(owner)); return; }
    f32 d = v3_dist(owner->pos, e->pos);
    if (d > 24 && owner->on_ground) { e->pos = v3_add(owner->pos, v3(frand_range(-1.5f, 1.5f), 0, frand_range(-1.5f, 1.5f))); e->prev_pos = e->pos; return; }
    Entity *t = entity_get(e->target);
    if (t && t->health > 0 && t != owner && e->mob == MOB_WOLF) {
        walk_to(e, t->pos, speed * 1.2f); melee(e, t, MOB_DEFS[e->mob].attack);
        return;
    }
    if (d > 4) walk_to(e, owner->pos, speed * (d > 10 ? 1.2f : 0.9f)); else wander(e, speed * 0.6f, 3);
}


// ---- villager day plan (Minecraft's schedule, in Minecraft ticks 0..24000 from sunrise): work 2000-9000 at the job
// site, meet at the nearest bell 9000-11000 (and swap gossip), free time until dusk, sleep in the nearest bed at night.
// e->phase = current activity (1 work, 2 meet, 3 sleep), e->anchor = the bell or bed found for it.
int g_villager_sleeps, g_gossip_swaps;
static bool find_near(Entity *e, int r, int dyr, bool bed, vec3 *out) {
    ivec3 c = v3_floor(e->pos); f32 bd = 1e9f; bool found = false;
    for (int dy = -dyr; dy <= dyr; dy++) for (int dz = -r; dz <= r; dz++) for (int dx = -r; dx <= r; dx++) {
        Block b = world_get_block(c.x + dx, c.y + dy, c.z + dz);
        if (bed ? BLOCKS[b].place != PL_BED : BLOCKS[b].base != B_BELL) continue;
        f32 d = (f32)(dx * dx + dy * dy + dz * dz);
        if (d < bd) { bd = d; *out = v3(c.x + dx + 0.5f, (f32)(c.y + dy + (bed ? 1 : 0)), c.z + dz + 0.5f); found = true; }
    }
    return found;
}
static bool villager_schedule(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    u64 t = (world_daytime() % DAY_TICKS) / MC_TICK_DIV;
    int act = t >= 12500 ? 3 : (t >= 9000 && t < 11000) ? 2 : (t >= 2000 && t < 9000) ? 1 : 0;
    if (act != e->phase || e->age % 400 == 13) {   // (re)choose where to go
        e->phase = (u8)act; e->anchor = v3(0, -9999, 0);
        vec3 p;
        if (act == 3 && find_near(e, 24, 3, true, &p)) e->anchor = p;
        if (act == 2 && find_near(e, 40, 6, false, &p)) e->anchor = p;
    }
    e->sleeping = 0;
    if (act == 3 && e->anchor.y > -9000) {
        f32 hx = e->pos.x - e->anchor.x, hz = e->pos.z - e->anchor.z;
        if (hx * hx + hz * hz > 1.6f * 1.6f || fabsf(e->pos.y - e->anchor.y) > 2.0f) walk_to(e, e->anchor, d->speed * 0.8f);
        else { stop(e); if (!e->sleeping) g_villager_sleeps++; e->sleeping = 1; }   // at the bed: asleep
        return true;
    }
    if (act == 2 && e->anchor.y > -9000) {
        if (v3_dist(e->pos, e->anchor) > 3.5f) walk_to(e, e->anchor, d->speed * 0.7f);
        else {
            stop(e);
            if (e->age % 600 == 31) ENT_FOREACH(o) if (o != e && o->kind == ENT_MOB && o->mob == MOB_VILLAGER && v3_dist(o->pos, e->pos) < 5 && o->rep != e->rep) {
                int avg = (e->rep + o->rep) / 2; e->rep = o->rep = (s8)avg; g_gossip_swaps++;   // gossip at the bell
            }
        }
        return true;
    }
    if (act == 1 && e->jobsite.y != -1000) {
        vec3 j = iv3_center(e->jobsite);
        if (v3_dist(e->pos, j) > 2.5f) walk_to(e, j, d->speed * 0.7f); else { stop(e); look_at(e, j); }
        return true;
    }
    return false;
}
void villager_hurt_by_player(Entity *v, bool killed) {   // gossip: the victim and everyone who saw it think less of you
    int self = killed ? 0 : -5, others = killed ? -10 : -2;
    v->rep = (s8)clampi(v->rep + self, -30, 30);
    ENT_FOREACH(o) if (o != v && o->kind == ENT_MOB && o->mob == MOB_VILLAGER && v3_dist(o->pos, v->pos) < 16) o->rep = (s8)clampi(o->rep + others, -30, 30);
}

static void passive_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    Entity *lure = NULL;
    if (e->tamed) { tamed_behaviour(e, d->speed); return; }
    if (e->ai == AI_PANIC) {
        if (--e->ai_timer <= 0) e->ai = AI_IDLE;
        steer(e, e->yaw + frand_range(-0.4f, 0.4f), d->speed * 1.4f);
        return;
    }
    // villagers & foxes & rabbits keep away from threats
    if (mob_has(e->mob, MF_FLEES_PLAYER) || d->ai == AIK_VILLAGER) {
        Entity *p = player_ent();
        Entity *threat = d->ai == AIK_VILLAGER ? find_mob_target(e, 8, pred_zombie) : (p && v3_dist(p->pos, e->pos) < 6 && !g_player.sneaking ? p : NULL);
        if (threat) { flee_from(e, threat->pos, d->speed * 1.3f); return; }
    }
    if (e->mob == MOB_VILLAGER && !e->baby && e->love <= 0 && villager_schedule(e)) return;
    // love mode: walk to a partner of the same kind
    if (e->love > 0) {
        Entity *best = NULL; f32 bd = 8;
        ENT_FOREACH(o) if (o != e && o->kind == ENT_MOB && o->mob == e->mob && o->love > 0 && !o->baby && v3_dist(o->pos, e->pos) < bd) { best = o; bd = v3_dist(o->pos, e->pos); }
        if (best) {
            walk_to(e, best->pos, d->speed * 0.8f);
            if (bd < 1.6f && e->id < best->id) {
                int t = e->mob;
                if (t == MOB_HORSE && frand() < 0.5f) t = frand() < 0.5f ? MOB_HORSE : MOB_HORSE;
                if ((e->mob == MOB_HORSE && best->mob == MOB_DONKEY) || (e->mob == MOB_DONKEY && best->mob == MOB_HORSE)) t = MOB_MULE;
                if (t == MOB_FROG) { world_set_block((int)e->pos.x, (int)e->pos.y, (int)e->pos.z, B_FROGSPAWN); }
                else if (t == MOB_TURTLE) world_set_block((int)e->pos.x, (int)e->pos.y, (int)e->pos.z, B_TURTLE_EGG);
                else if (t == MOB_SNIFFER) item_drop(e->pos, stack_of(I_SNIFFER_EGG, 1), v3(0, 2, 0), 10);
                else { Entity *b = spawn_baby(t, e->pos); if (b && t == MOB_SHEEP) b->color = frand() < 0.5f ? e->color : best->color; }
                xp_drop(e->pos, irand(1, 7));
                e->love = best->love = 0;
                e->breed_cooldown = best->breed_cooldown = MC(6000);
            }
            return;
        }
    }
    if (tempted(e, &lure)) {
        f32 dd = v3_dist(lure->pos, e->pos);
        look_at(e, entity_eye(lure));
        if (dd > 2.5f) walk_to(e, lure->pos, d->speed * 0.8f); else stop(e);
        return;
    }
    if (e->baby) {   // babies follow an adult
        ENT_FOREACH(o) if (o != e && o->kind == ENT_MOB && o->mob == e->mob && !o->baby && v3_dist(o->pos, e->pos) < 10) {
            if (v3_dist(o->pos, e->pos) > 3) { walk_to(e, o->pos, d->speed); return; }
            break;
        }
    }
    // occasionally look at a nearby player
    Entity *p = player_ent();
    if (p && v3_dist(p->pos, e->pos) < 6 && frand() < 0.02f) look_at(e, entity_eye(p));
    wander(e, d->speed, 8);
}

// neutral: passive until provoked (or provoked by a rule); then chase and attack
static void hostile_melee(Entity *e, Entity *t, f32 speed) {
    look_at(e, entity_eye(t));
    f32 dd = hdist(e->pos, t->pos);
    if (dd > reach_to(e, t) * 0.7f) walk_to(e, t->pos, speed); else { stop(e); e->yaw = e->body_yaw = approach_angle(e->body_yaw, yaw_to(e->pos, t->pos), 0.3f); }
    melee(e, t, MOB_DEFS[e->mob].attack);
}
static void neutral_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    Entity *t = NULL;
    if (e->anger > 0) { e->anger--; t = entity_get(e->target); if (t && t->kind == ENT_PLAYER && !player_target()) t = NULL; }
    if (e->mob == MOB_PIGLIN && !t && !wears_gold()) t = find_target(e);
    if (e->mob == MOB_WOLF && !e->tamed && !t && frand() < 0.01f) { Entity *prey = find_mob_target(e, 10, pred_wolf_prey); if (prey) { e->target = prey->id; e->anger = MC(400); t = prey; } }
    if (e->mob == MOB_POLAR_BEAR && !t) { Entity *p = player_target(); if (p && v3_dist(p->pos, e->pos) < 6) ENT_FOREACH(o) if (o->kind == ENT_MOB && o->mob == MOB_POLAR_BEAR && o->baby && v3_dist(o->pos, e->pos) < 8) { t = p; e->target = p->id; e->anger = MC(200); break; } }
    if (e->mob == MOB_GOAT && !t && --e->special_timer <= 0) {   // rams something now and then
        e->special_timer = MC(irand(600, 6000));
        Entity *p = player_target();
        if (p && v3_dist(p->pos, e->pos) < 8) { e->target = p->id; e->anger = MC(60); }
    }
    if (e->tamed && d->ai == AIK_NEUTRAL) { tamed_behaviour(e, d->speed); return; }
    if (!t || t->health <= 0) { e->target = 0; e->anger = 0; passive_ai(e); return; }
    if (d->projectile != 255 && d->projectile == PROJ_LLAMA_SPIT) {
        f32 dd = v3_dist(e->pos, t->pos);
        look_at(e, entity_eye(t));
        if (dd > 8) walk_to(e, t->pos, d->speed); else stop(e);
        if (e->attack_cooldown <= 0 && entity_can_see(e, t)) { shoot(e, t, PROJ_LLAMA_SPIT); e->attack_cooldown = MC(d->attack_cd); }
        return;
    }
    if (mob_has(e->mob, MF_FLYING)) {
        fly_to(e, v3_add(t->pos, v3(0, t->height * 0.6f, 0)), d->speed, 0.1f);
        look_at(e, entity_eye(t));
        melee(e, t, d->attack);
        return;
    }
    hostile_melee(e, t, d->speed * 1.25f);
}

static void hostile_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    Entity *t = find_target(e);
    if (e->mob == MOB_ZOGLIN && !t) t = find_mob_target(e, 16, pred_any_mob);
    if (!t && (e->mob == MOB_ZOMBIE || e->mob == MOB_HUSK || e->mob == MOB_VINDICATOR || e->mob == MOB_ZOMBIE_VILLAGER)) t = find_mob_target(e, 16, pred_villager);
    if (!t) { e->ai = AI_IDLE; wander(e, d->speed, 8); return; }
    e->ai = AI_CHASE;
    if (e->mob == MOB_DROWNED && !stack_empty(e->item) && e->item.id == I_TRIDENT && v3_dist(e->pos, t->pos) > 4) {
        look_at(e, entity_eye(t)); stop(e);
        if (e->attack_cooldown <= 0) { shoot(e, t, PROJ_TRIDENT); e->attack_cooldown = MC(60); }
        return;
    }
    if (e->in_water && mob_has(e->mob, MF_SWIMS)) {
        fly_to(e, v3_add(t->pos, v3(0, 0.3f, 0)), d->speed, 0.08f);
        look_at(e, entity_eye(t)); melee(e, t, d->attack);
        return;
    }
    hostile_melee(e, t, d->speed * 1.15f);
}

static void ranged_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    Entity *t = find_target(e);
    if (!t) { e->swell = 0; wander(e, d->speed, 8); return; }
    f32 dd = v3_dist(e->pos, t->pos);
    bool see = entity_can_see(e, t);
    look_at(e, entity_eye(t));
    e->yaw = e->body_yaw = approach_angle(e->body_yaw, yaw_to(e->pos, t->pos), 0.3f);
    if (!see || dd > d->range) walk_to(e, t->pos, d->speed);
    else {   // kite: back off when close, strafe otherwise
        if (--e->special_timer <= 0) { e->strafe_dir = frand() < 0.5f ? -1 : 1; e->special_timer = MC(irand(20, 60)); }
        f32 side = e->yaw + PI_F / 2 * e->strafe_dir;
        f32 back = dd < d->range * 0.4f ? -1.0f : 0.0f;
        vec3 f = v3(sinf(e->yaw), 0, cosf(e->yaw)), s = v3(sinf(side), 0, cosf(side));
        vec3 mv = v3_add(v3_scale(f, back), v3_scale(s, 0.6f));
        e->vel.x = lerpf(e->vel.x, mv.x * d->speed * 0.6f, 0.2f); e->vel.z = lerpf(e->vel.z, mv.z * d->speed * 0.6f, 0.2f);
        if (e->h_collision && e->on_ground) e->vel.y = 8.4f;
    }
    if (see && dd < d->range) {
        e->swell = fminf(1, e->swell + 1.0f / MC(20));
        if (e->attack_cooldown <= 0) { shoot(e, t, d->projectile); e->attack_cooldown = MC(d->attack_cd); e->swell = 0; }
    } else e->swell = fmaxf(0, e->swell - 0.05f);
}

static void creeper_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    Entity *cat = find_mob_target(e, 6, pred_cat);
    if (cat) { flee_from(e, cat->pos, d->speed * 1.3f); e->fuse = 0; e->swell = fmaxf(0, e->swell - 0.05f); return; }
    Entity *t = find_target(e);
    if (!t) { e->fuse = 0; e->swell = fmaxf(0, e->swell - 0.05f); wander(e, d->speed, 8); return; }
    f32 dd = v3_dist(e->pos, t->pos);
    look_at(e, entity_eye(t));
    if (dd < 3.0f || (e->fuse > 0 && dd < 7)) {
        if (e->fuse == 0) sound_play_at(SND_FUSE, e->pos, 1, 0.9f);
        stop(e);
        e->fuse++;
        e->swell = (f32)e->fuse / MC(30);
        if (e->fuse >= MC(30)) {
            e->remove = true; e->health = 0;
            explosion(v3_add(e->pos, v3(0, 0.6f, 0)), e->phase == 1 ? 6.0f : 3.0f, e);   // charged creepers (struck by lightning) blast twice as hard
        }
    } else {
        e->fuse = MAXI(0, e->fuse - 1); e->swell = (f32)e->fuse / MC(30);
        walk_to(e, t->pos, d->speed * 1.1f);
    }
}

static void slime_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    Entity *t = e->mob == MOB_SULFUR_CUBE ? NULL : find_target(e);
    if (t) { e->yaw = e->body_yaw = approach_angle(e->body_yaw, yaw_to(e->pos, t->pos), 0.4f); look_at(e, entity_eye(t)); }
    else if (--e->ai_timer <= 0) { e->yaw = e->body_yaw = frand_range(-PI_F, PI_F); e->ai_timer = irand(MC(20), MC(80)); }
    if (e->on_ground) {
        e->vel.x *= 0.6f; e->vel.z *= 0.6f;
        if (--e->jump_cooldown <= 0) {
            f32 sp = (t ? 1.0f : 0.5f) * d->speed * (0.8f + e->size * 0.2f);
            e->vel = v3(sinf(e->yaw) * sp, e->mob == MOB_MAGMA_CUBE ? 9.5f + e->size * 0.4f : 7.5f, cosf(e->yaw) * sp);
            e->jump_cooldown = irand(MC(10), MC(30)) * (t ? 1 : 3);
            e->swell = 1;
        }
    }
    e->swell *= 0.85f;
    if (t && e->size > 1 && e->mob != MOB_SULFUR_CUBE) melee(e, t, (e->mob == MOB_MAGMA_CUBE ? 1.5f : 1.0f) * e->size);
}

static void flyer_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    Entity *p = player_ent();
    if (e->mob == MOB_VEX) {
        if (--e->special_timer <= 0 && e->age % MC(20) == 0) entity_damage(e, 1, DMG_GENERIC, NULL);
        Entity *t = find_target(e);
        if (t) { fly_to(e, v3_add(t->pos, v3(0, t->height * 0.5f, 0)), d->speed, 0.1f); look_at(e, entity_eye(t)); melee(e, t, d->attack); return; }
    }
    if ((e->mob == MOB_ALLAY || e->mob == MOB_HAPPY_GHAST || e->mob == MOB_PARROT) && p && (e->tamed || e->mob == MOB_ALLAY || tempted(e, &p))) {
        f32 dd = v3_dist(p->pos, e->pos);
        if (dd > 4) fly_to(e, v3_add(p->pos, v3(0, e->mob == MOB_HAPPY_GHAST ? 4 : 1.5f, 0)), d->speed, 0.05f);
        else e->vel = v3_scale(e->vel, 0.9f);
        look_at(e, entity_eye(p));
        return;
    }
    // erratic wander for bats, gentle for others
    if (--e->ai_timer <= 0 || v3_dist(e->pos, e->goal) < 1) {
        e->goal = random_goal(e, e->mob == MOB_BAT ? 4 : 10, e->mob == MOB_BAT ? 3 : 4);
        int top = world_top_y((int)e->goal.x, (int)e->goal.z);
        if (e->mob != MOB_BAT && top > 0 && e->goal.y < top + 2) e->goal.y = top + 2.5f;
        if (e->mob == MOB_HAPPY_GHAST && top > 0) e->goal.y = top + 6;
        e->ai_timer = irand(MC(20), MC(e->mob == MOB_BAT ? 40 : 120));
    }
    fly_to(e, e->goal, d->speed * (e->mob == MOB_BAT ? 1.0f : 0.6f), 0.05f);
}

// Ghost (Hell): drifts toward players, now and then phases through walls for a few seconds,
// drains health on touch (healing itself) and leaves victims Haunted.
int g_ghost_phases;   // how many times any ghost has started phasing (tests)
static bool ghost_inside_block(Entity *e) {
    for (int dy = 0; dy < 2; dy++) {
        u16 b = world_get_block((int)floorf(e->pos.x), (int)floorf(e->pos.y + dy), (int)floorf(e->pos.z));
        if (BLOCKS[b].solid) return true;
    }
    return false;
}
// Soul lanterns (held, or placed within 4 blocks) repel ghosts and sear them
int g_holy_splashes, g_ghost_repels, g_revenant_novas, g_pearl_phases, g_hell_ambience;
static bool ghost_repelled(Entity *e, vec3 *from) {
    Entity *p = player_ent();
    if (p && v3_dist(p->pos, e->pos) < 7 && (inventory_held(&g_player.inv)->id == I_SOUL_LANTERN || g_player.inv.offhand.id == I_SOUL_LANTERN)) { *from = p->pos; return true; }
    if (e->age % 30 == 0) {   // re-scan for placed lanterns; e->stuck_ticks caches the result, e->anchor the lantern
        e->stuck_ticks = 0;
        ivec3 c = v3_floor(e->pos);
        for (int dy = -4; dy <= 4 && !e->stuck_ticks; dy++) for (int dz = -4; dz <= 4 && !e->stuck_ticks; dz++) for (int dx = -4; dx <= 4; dx++)
            if (BLOCKS[world_get_block(c.x + dx, c.y + dy, c.z + dz)].base == B_SOUL_LANTERN) { e->stuck_ticks = 1; e->anchor = v3(c.x + dx + 0.5f, (f32)(c.y + dy), c.z + dz + 0.5f); break; }
    }
    if (e->stuck_ticks) { *from = e->anchor; return true; }
    return false;
}
static void ghost_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    e->no_gravity = true;
    if (e->special_timer > 0 && --e->special_timer == 0) {   // phase ends: stay ethereal until out of the wall
        if (ghost_inside_block(e)) e->special_timer = 10; else e->no_clip = false;
    }
    Entity *seer = player_ent();
    bool seen = seer && effect_has(seer, EFF_GHOST_SIGHT) && v3_dist(seer->pos, e->pos) < 24;
    if (seen) effect_add(e, EFF_GLOWING, 0, 40);   // Ghost Sight: they glow and cannot slip into walls close to you
    vec3 rf;
    if (ghost_repelled(e, &rf)) {
        vec3 away = v3_add(e->pos, v3_scale(v3_norm(v3_sub(e->pos, rf)), 6));
        fly_to(e, v3(away.x, rf.y + 1.5f, away.z), d->speed * 1.2f, 0.1f);
        if (e->age % 40 == 0) { entity_damage(e, 1, DMG_MAGIC, NULL); g_ghost_repels++; fx_smoke(v3_add(e->pos, v3(0, 1, 0)), 3, 0.1f, rgba_u32(120, 220, 255, 255)); }
        return;
    }
    if (!e->no_clip && e->special_timer <= 0 && e->jump_cooldown <= 0 && !(seen && v3_dist(seer->pos, e->pos) < 10) && (e->h_collision || frand() < 0.002f)) {
        e->no_clip = true; e->special_timer = MC(irand(40, 80)); e->jump_cooldown = MC(irand(120, 240)); g_ghost_phases++;
        sound_play_at(SND_FIRE, e->pos, 0.3f, 0.4f);
    }
    f32 bob = sinf(e->age * 0.05f) * 0.4f;
    Entity *t = find_target(e);
    if (t) {
        vec3 aim = v3_add(t->pos, v3(0, t->height * 0.55f + bob, 0));
        fly_to(e, aim, d->speed * (e->no_clip ? 0.7f : 1.0f), 0.08f);
        look_at(e, entity_eye(t));
        if (!e->no_clip && melee(e, t, d->attack)) {   // life drain (slower than a zombie: one touch every 2.5 s)
            if (e->mob == MOB_EMBER_WRAITH) t->fire = MAXI(t->fire, MC(80));   // ember wraiths burn
            if (e->mob == MOB_GHOST || e->mob == MOB_CINDER_SPECTER) effect_add(t, EFF_HAUNTED, 0, MC(160));
            e->attack_cooldown = MC(50);
            e->health = fminf(e->max_health, e->health + 2);
            fx_smoke(v3_add(e->pos, v3(0, e->height * 0.6f, 0)), 6, 0.15f, rgba_u32(170, 220, 235, 255));
        }
        return;
    }
    if (--e->ai_timer <= 0 || v3_dist(e->pos, e->goal) < 1) {
        e->goal = random_goal(e, 10, 4);
        e->ai_timer = irand(MC(40), MC(140));
    }
    fly_to(e, v3_add(e->goal, v3(0, bob, 0)), d->speed * 0.4f, 0.04f);
}

static void fish_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    if (!e->in_water) {   // flopping on land
        if (e->on_ground && frand() < 0.1f && mob_has(e->mob, MF_AQUATIC)) e->vel = v3(frand_range(-2, 2), 5, frand_range(-2, 2));
        if (e->mob == MOB_AXOLOTL || e->mob == MOB_TURTLE) { passive_ai(e); return; }
        return;
    }
    e->no_gravity = true;
    Entity *t = NULL;
    if (e->mob == MOB_AXOLOTL && frand() < 0.05f) t = find_mob_target(e, 8, pred_fish);
    else if (e->mob == MOB_ZOMBIE_NAUTILUS || (e->anger > 0)) t = e->anger > 0 ? entity_get(e->target) : find_target(e);
    if (e->anger > 0) e->anger--;
    if (e->mob == MOB_PUFFERFISH) {   // puff up near players, sting on touch
        Entity *p = player_target();
        e->swell = p && v3_dist(p->pos, e->pos) < 2.5f ? fminf(1, e->swell + 0.1f) : fmaxf(0, e->swell - 0.02f);
        if (p && e->swell > 0.5f && v3_dist(p->pos, e->pos) < 1.5f && e->attack_cooldown <= 0) { entity_damage(p, 2, DMG_MELEE, e); effect_add(p, EFF_POISON, 0, MC(120)); e->attack_cooldown = MC(20); }
    }
    if (e->mob == MOB_DOLPHIN) { Entity *p = player_ent(); if (p && p->in_water && v3_dist(p->pos, e->pos) < 10) effect_add(p, EFF_SPEED, 0, MC(100)); }
    if (t && t->health > 0) {
        fly_to(e, v3_add(t->pos, v3(0, t->height * 0.5f, 0)), d->speed * 1.3f, 0.1f);
        melee(e, t, d->attack > 0 ? d->attack : 2);
        return;
    }
    if (--e->ai_timer <= 0 || v3_dist(e->pos, e->goal) < 1 || !in_water_at(e->goal)) {
        for (int i = 0; i < 6; i++) {
            vec3 g = random_goal(e, 8, 3);
            if (in_water_at(g)) { e->goal = g; break; }
        }
        e->ai_timer = irand(MC(40), MC(120));
    }
    fly_to(e, e->goal, d->speed * 0.6f, 0.05f);
}

static void squid_ai(Entity *e) {
    if (!e->in_water) { e->no_gravity = false; return; }
    e->no_gravity = true;
    e->swell = 0.5f + 0.5f * sinf(e->age * 0.08f);
    if (--e->ai_timer <= 0) {
        e->vel = v3(frand_range(-2, 2), frand_range(-1, 1.2f), frand_range(-2, 2));
        e->ai_timer = irand(MC(40), MC(100));
        if (!in_water_at(v3_add(e->pos, v3(0, 1.5f, 0)))) e->vel.y = -1;
    }
    e->vel = v3_scale(e->vel, 0.985f);
}

static void guardian_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    if (e->in_water) e->no_gravity = true; else e->no_gravity = false;
    Entity *t = find_target(e);
    if (e->mob == MOB_ELDER_GUARDIAN && e->age % MC(1200) == 0) {
        Entity *p = player_target();
        if (p && v3_dist(p->pos, e->pos) < 50) { effect_add(p, EFF_MINING_FATIGUE, 2, MC(6000)); sound_play(SND_ZOMBIE_IDLE, 1, 0.5f); }
    }
    if (t && entity_can_see(e, t) && v3_dist(e->pos, t->pos) < 16) {
        e->vel = v3_scale(e->vel, 0.8f);
        look_at(e, entity_eye(t));
        e->yaw = e->body_yaw = yaw_to(e->pos, t->pos);
        e->swell += 1.0f / MC(80);   // laser charge drawn by the renderer
        e->goal = t->pos;
        if (e->swell >= 1) { entity_damage(t, d->attack, DMG_MAGIC, e); e->swell = 0; }
        return;
    }
    e->swell = 0;
    if (e->in_water) {
        if (--e->ai_timer <= 0 || !in_water_at(e->goal)) { e->goal = random_goal(e, 10, 3); e->ai_timer = irand(MC(40), MC(100)); }
        fly_to(e, e->goal, d->speed * 0.5f, 0.05f);
    }
}

static void shulker_ai(Entity *e) {
    e->vel = v3(0, 0, 0);
    Entity *t = find_target(e);
    e->swell = t ? fminf(1, e->swell + 0.05f) : fmaxf(0, e->swell - 0.02f);   // shell opening
    if (t && e->attack_cooldown <= 0 && entity_can_see(e, t)) { shoot(e, t, PROJ_SHULKER_BULLET); e->attack_cooldown = MC(irand(20, 110)); }
    if (t) look_at(e, entity_eye(t));
}

static bool player_looks_at(Entity *e, f32 cos_limit) {
    Entity *p = player_ent();
    if (!p) return false;
    vec3 eye = entity_eye(p);
    vec3 fwd = v3(sinf(p->yaw) * cosf(p->pitch), sinf(p->pitch), cosf(p->yaw) * cosf(p->pitch));
    vec3 to = v3_norm(v3_sub(v3_add(e->pos, v3(0, e->height * 0.85f, 0)), eye));
    return v3_dot(fwd, to) > cos_limit && entity_can_see(p, e);
}
static bool teleport_random(Entity *e, int range) {
    for (int i = 0; i < 16; i++) {
        int x = (int)e->pos.x + irand(-range, range), z = (int)e->pos.z + irand(-range, range);
        for (int y = (int)e->pos.y + 8; y > (int)e->pos.y - 8; y--) {
            Block b = world_get_block(x, y - 1, z);
            if (BLOCKS[b].solid && !BLOCKS[b].liquid && block_is_air(world_get_block(x, y, z)) && block_is_air(world_get_block(x, y + 1, z)) && block_is_air(world_get_block(x, y + 2, z))) {
                fx_poof(e->pos, e->width, e->height);
                e->pos = e->prev_pos = v3(x + 0.5f, (f32)y, z + 0.5f);
                sound_play_at(SND_POP, e->pos, 1, 0.5f);
                return true;
            }
        }
    }
    return false;
}

static void enderman_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    if (e->in_water || (e->age % 40 == 0 && L_SKY(world_get_light((int)e->pos.x, (int)e->pos.y + 2, (int)e->pos.z)) == 15 && world_daylight() > 0.8f && frand() < 0.1f)) {
        if (e->in_water) entity_damage(e, 1, DMG_DROWN, NULL);
        teleport_random(e, 24);
    }
    Entity *p = player_target();
    bool pumpkin = g_player.inv.armor[0].id == I_CARVED_PUMPKIN;
    if (p && e->anger <= 0 && !pumpkin && v3_dist(p->pos, e->pos) < 64 && player_looks_at(e, 0.995f)) {
        e->anger = MC(600); e->target = p->id; e->stuck_ticks = MC(5);
        sound_play_at(SND_ZOMBIE_IDLE, e->pos, 1, 0.4f);
    }
    if (e->anger > 0) {
        e->anger--;
        Entity *t = entity_get(e->target);
        if (!t || !player_target()) { e->anger = 0; return; }
        if (e->stuck_ticks > 0) { e->stuck_ticks--; stop(e); look_at(e, entity_eye(t)); return; }   // the stare
        if (v3_dist(e->pos, t->pos) > 12 && frand() < 0.02f) {   // teleport closer
            vec3 old = e->pos; e->pos = v3_add(t->pos, v3(frand_range(-4, 4), 0, frand_range(-4, 4)));
            if (!teleport_random(e, 3)) e->pos = old;
        }
        hostile_melee(e, t, d->speed * 1.4f);
        return;
    }
    wander(e, d->speed * 0.6f, 10);
}

static void spider_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    int light = world_light_level(v3_floor(v3_add(e->pos, v3(0, 0.5f, 0))));
    Entity *t = NULL;
    if (light < 12 || e->anger > 0) t = find_target(e);
    if (e->anger > 0) e->anger--;
    if (!t) { wander(e, d->speed, 8); return; }
    look_at(e, entity_eye(t));
    f32 dd = hdist(e->pos, t->pos);
    if (dd > 2 && dd < 4 && e->on_ground && frand() < 0.08f) {   // pounce
        vec3 dir = v3_norm(v3_sub(t->pos, e->pos));
        e->vel = v3(dir.x * 8, 6, dir.z * 8);
    }
    hostile_melee(e, t, d->speed * 1.1f);
}

static void phantom_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    Entity *t = find_target(e);
    Entity *cat = find_mob_target(e, 16, pred_cat);
    if (!t || cat) {
        f32 a = e->age * 0.01f;
        fly_to(e, v3_add(e->anchor, v3(cosf(a) * 16, 22, sinf(a) * 16)), d->speed * 0.6f, 0.04f);
        return;
    }
    if (e->phase == 0) {   // circle above the target
        f32 a = e->age * 0.025f;
        e->anchor = t->pos;
        fly_to(e, v3_add(t->pos, v3(cosf(a) * 12, 14, sinf(a) * 12)), d->speed, 0.06f);
        if (--e->special_timer <= 0) { e->phase = 1; e->special_timer = MC(irand(40, 100)); }
    } else {   // swoop
        fly_to(e, v3_add(t->pos, v3(0, 0.5f, 0)), d->speed * 1.6f, 0.12f);
        if (melee(e, t, d->attack) || e->h_collision || --e->special_timer <= 0) { e->phase = 0; e->special_timer = MC(irand(100, 200)); }
    }
}

static void golem_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    Entity *t = e->anger > 0 ? entity_get(e->target) : NULL;
    if (e->anger > 0) e->anger--;
    if (!t || t->health <= 0) { t = find_mob_target(e, 16, pred_hostile_not_creeper); if (t) e->target = t->id; }
    if (e->mob == MOB_SNOW_GOLEM) {
        int b = world_biome_at((int)e->pos.x, (int)e->pos.y, (int)e->pos.z);
        if (BIOMES[b].temperature > 1.0f && e->age % MC(20) == 0) entity_damage(e, 1, DMG_FIRE, NULL);
        ivec3 f = v3_floor(e->pos);
        if (BIOMES[b].temperature < 0.8f && block_is_air(world_get_block(f.x, f.y, f.z)) && BLOCKS[world_get_block(f.x, f.y - 1, f.z)].opaque) world_set_block(f.x, f.y, f.z, B_SNOW);
        if (t) {
            look_at(e, entity_eye(t)); stop(e);
            if (e->attack_cooldown <= 0 && entity_can_see(e, t)) { shoot(e, t, PROJ_SNOWBALL); e->attack_cooldown = MC(20); }
            return;
        }
        wander(e, d->speed, 8);
        return;
    }
    if (t) { hostile_melee(e, t, d->speed); return; }
    wander(e, d->speed * 0.7f, 10);
}

static void warden_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    Entity *p = player_target();
    // senses vibrations: moving, non-sneaking players within 16 blocks
    if (p && v3_dist(p->pos, e->pos) < 16 && !g_player.sneaking && (fabsf(p->vel.x) + fabsf(p->vel.z) > 0.5f || g_player.mining)) { e->target = p->id; e->anger = MC(400); }
    if (p && e->age % MC(120) == 0 && v3_dist(p->pos, e->pos) < 20) effect_add(p, EFF_DARKNESS, 0, MC(260));
    Entity *t = e->anger > 0 ? entity_get(e->target) : NULL;
    if (e->anger > 0) e->anger--;
    if (!t || t->health <= 0) {
        wander(e, d->speed * 0.4f, 6);
        if (++e->special_timer > MC(1200) && !e->persistent) { fx_poof(e->pos, e->width, e->height); e->remove = true; }   // digs back down
        return;
    }
    e->special_timer = 0;
    f32 dd = v3_dist(e->pos, t->pos);
    if (dd > 3 && dd < 15 && e->attack_cooldown <= 0) {   // sonic boom: ignores walls and armour
        look_at(e, entity_eye(t));
        e->attack_cooldown = MC(100);
        entity_damage(t, 10, DMG_SONIC, e);
        entity_knockback(t, e->pos, 1.5f);
        vec3 a = entity_eye(e), b = entity_eye(t);
        for (int i = 0; i < 12; i++) fx_smoke(v3_lerp(a, b, i / 12.0f), 1, 0.05f, rgba_u32(80, 220, 230, 255));
        return;
    }
    hostile_melee(e, t, d->speed * 1.3f);
}

static void creaking_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    if (!e->from_spawner && !world_is_night() && g_world.dim == DIM_OVERWORLD) { fx_poof(e->pos, e->width, e->height); e->remove = true; return; }
    if (player_looks_at(e, 0.5f)) { e->vel.x = e->vel.z = 0; e->limb_amount = 0; return; }   // frozen while watched
    Entity *t = find_target(e);
    if (!t) { wander(e, d->speed * 0.5f, 6); return; }
    hostile_melee(e, t, d->speed);
}

static void evoker_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    Entity *t = find_target(e);
    if (!t) { wander(e, d->speed, 8); return; }
    look_at(e, entity_eye(t));
    f32 dd = v3_dist(e->pos, t->pos);
    if (dd < 8) flee_from(e, t->pos, d->speed); else stop(e);
    if (e->attack_cooldown <= 0) {
        e->attack_anim = 1;
        int vexes = 0;
        ENT_FOREACH(o) if (o->kind == ENT_MOB && o->mob == MOB_VEX && o->owner_id == e->id) vexes++;
        if (vexes < 3 && frand() < 0.35f) {
            for (int i = 0; i < 3; i++) { Entity *v = mob_spawn(MOB_VEX, v3_add(e->pos, v3(frand_range(-1, 1), 1, frand_range(-1, 1)))); if (v) { v->owner_id = e->id; v->target = t->id; } }
            e->attack_cooldown = MC(340);
        } else {   // a line of fangs toward the target
            vec3 dir = v3_norm(v3_sub(t->pos, e->pos)); dir.y = 0; dir = v3_norm(dir);
            for (int i = 1; i <= 16; i++) {
                vec3 p = v3_add(e->pos, v3_scale(dir, i * 1.25f));
                int y = world_top_y((int)floorf(p.x), (int)floorf(p.z));
                if (y < 0 || fabsf(y + 1 - e->pos.y) > 4) y = (int)e->pos.y - 1;
                fangs_spawn(v3(p.x, y + 1.0f, p.z), i * 2, e->id);
            }
            e->attack_cooldown = MC(100);
        }
    }
}

static void witch_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    if (e->health < e->max_health * 0.5f && e->special_timer <= 0) { potion_apply(e, POT_HEALING, 1); e->special_timer = MC(200); sound_play_at(SND_EAT, e->pos, 1, 1.4f); }
    if (e->fire > 0 && !effect_has(e, EFF_FIRE_RESISTANCE)) effect_add(e, EFF_FIRE_RESISTANCE, 0, MC(3600));
    if (e->special_timer > 0) e->special_timer--;
    Entity *t = find_target(e);
    if (!t) { wander(e, d->speed, 8); return; }
    look_at(e, entity_eye(t));
    f32 dd = v3_dist(e->pos, t->pos);
    if (dd > d->range * 0.7f) walk_to(e, t->pos, d->speed); else stop(e);
    if (dd < d->range && e->attack_cooldown <= 0 && entity_can_see(e, t)) { shoot(e, t, PROJ_POTION); e->attack_cooldown = MC(d->attack_cd); }
}

static void breeze_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    Entity *t = find_target(e);
    if (!t) { wander(e, d->speed * 0.5f, 6); return; }
    look_at(e, entity_eye(t));
    if (e->on_ground && --e->jump_cooldown <= 0) {
        vec3 dir = v3_norm(v3_sub(t->pos, e->pos));
        f32 side = frand() < 0.5f ? -1 : 1;
        e->vel = v3(dir.z * side * 5 + dir.x * 2, 11, -dir.x * side * 5 + dir.z * 2);
        e->jump_cooldown = MC(irand(30, 60));
    }
    if (e->attack_cooldown <= 0 && entity_can_see(e, t)) { shoot(e, t, PROJ_WIND_CHARGE); e->attack_cooldown = MC(d->attack_cd); }
}

static void blaze_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    Entity *t = find_target(e);
    if (!t) {
        e->no_gravity = false;
        wander(e, d->speed * 0.5f, 6);
        if (!e->on_ground && e->vel.y < -1) e->vel.y = -1;
        return;
    }
    e->no_gravity = true;
    look_at(e, entity_eye(t));
    vec3 goal = v3_add(t->pos, v3(0, 2.5f, 0));
    if (v3_dist(e->pos, t->pos) < 6) goal = e->pos, goal.y = t->pos.y + 2.5f;
    fly_to(e, goal, d->speed * 0.6f, 0.05f);
    if (e->attack_cooldown <= 0 && entity_can_see(e, t)) {   // bursts of three fireballs
        shoot(e, t, PROJ_SMALL_FIREBALL);
        e->phase++;
        if (e->phase >= 3) { e->phase = 0; e->attack_cooldown = MC(d->attack_cd); }
        else e->attack_cooldown = MC(6);
    }
    melee(e, t, d->attack);
}

static void ghast_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    Entity *t = find_target(e);
    if (--e->ai_timer <= 0 || v3_dist(e->pos, e->goal) < 2) { e->goal = random_goal(e, 20, 8); e->goal.y = clampf(e->goal.y, 40, 110); e->ai_timer = irand(MC(40), MC(160)); }
    fly_to(e, e->goal, d->speed, 0.02f);
    if (t && entity_can_see(e, t)) {
        look_at(e, entity_eye(t));
        e->yaw = e->body_yaw = approach_angle(e->body_yaw, yaw_to(e->pos, t->pos), 0.2f);
        e->swell = fminf(1, e->swell + 1.0f / MC(20));
        if (e->attack_cooldown <= 0) { shoot(e, t, PROJ_FIREBALL); e->attack_cooldown = MC(d->attack_cd); e->swell = 0; }
    } else e->swell = fmaxf(0, e->swell - 0.05f);
}

static void silverfish_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    Entity *t = find_target(e);
    if (!t) {
        wander(e, d->speed, 6);
        // silverfish hide in stone
        if (e->mob == MOB_SILVERFISH && frand() < 0.002f) {
            ivec3 b = v3_floor(v3_add(e->pos, v3(0, -0.5f, 0)));
            if (world_get_block(b.x, b.y, b.z) == B_STONE) { world_set_block(b.x, b.y, b.z, B_INFESTED_STONE); e->remove = true; }
        }
        return;
    }
    hostile_melee(e, t, d->speed * 1.2f);
}

// ---------------------------------------------------------------- bosses
static void wither_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    g_ents.boss = e->id;
    if (e->phase == 0) {   // charging up after being built
        e->vel = v3(0, 0, 0);
        e->health = fminf(e->max_health, e->health + e->max_health / MC(220));
        e->invuln = 10;
        if (--e->special_timer <= 0) { e->phase = 1; explosion_ex(v3_add(e->pos, v3(0, 1.5f, 0)), 7.0f, e, false, true); }
        return;
    }
    if (e->age % MC(20) == 0) entity_heal(e, 1);
    Entity *t = find_target(e);
    if (!t) t = find_mob_target(e, 32, pred_any_mob);
    if (t) {
        look_at(e, entity_eye(t));
        f32 a = e->age * 0.015f;
        fly_to(e, v3_add(t->pos, v3(cosf(a) * 8, 5, sinf(a) * 8)), d->speed, 0.04f);
        if (e->attack_cooldown <= 0 && entity_can_see(e, t)) { shoot(e, t, PROJ_WITHER_SKULL); e->attack_cooldown = MC(e->health < e->max_health / 2 ? 25 : 40); }
    } else fly_to(e, v3_add(e->pos, v3(0, 0.2f, 0)), 1, 0.02f);
    // tears through blocks it bumps into
    if (e->h_collision && e->age % MC(10) == 0) {
        AABB a = entity_aabb(e);
        for (int y = (int)floorf(a.min.y); y <= (int)floorf(a.max.y); y++) for (int z = (int)floorf(a.min.z) - 1; z <= (int)floorf(a.max.z) + 1; z++) for (int x = (int)floorf(a.min.x) - 1; x <= (int)floorf(a.max.x) + 1; x++) {
            Block b = world_get_block(x, y, z);
            if (b && BLOCKS[b].hardness >= 0 && BLOCKS[b].blast_resistance < 600) world_set_block(x, y, z, B_AIR);
        }
    }
}

static void dragon_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    g_ents.boss = e->id;
    // nearest end crystal heals the dragon
    Entity *crystal = NULL; f32 cd = 40;
    ENT_FOREACH(o) if (o->kind == ENT_CRYSTAL && v3_dist(o->pos, e->pos) < cd) { cd = v3_dist(o->pos, e->pos); crystal = o; }
    if (crystal && e->age % MC(10) == 0) entity_heal(e, 1);
    e->target = crystal ? crystal->id : e->target;
    Entity *p = player_target();
    if (--e->special_timer <= 0) {
        int r = irand(0, 9);
        e->phase = (u8)(r < 5 ? 0 : r < 8 ? 1 : r < 9 ? 2 : 3);
        e->special_timer = MC(e->phase == 3 ? 200 : irand(160, 400));
    }
    if (!p && e->phase != 3) e->phase = 0;
    switch (e->phase) {
    case 0: {   // circle the island
        f32 a = e->age * 0.006f;
        fly_to(e, v3(cosf(a) * 60, 85 + sinf(a * 3) * 8, sinf(a) * 60), d->speed, 0.02f);
    } break;
    case 1:   // strafe: fly toward the player and spit fireballs
        fly_to(e, v3_add(p->pos, v3(0, 12, 0)), d->speed, 0.02f);
        if (e->attack_cooldown <= 0 && v3_dist(e->pos, p->pos) < 64) { shoot(e, p, PROJ_DRAGON_FIREBALL); e->attack_cooldown = MC(100); }
        break;
    case 2:   // charge
        fly_to(e, v3_add(p->pos, v3(0, 1, 0)), d->speed * 1.3f, 0.05f);
        break;
    default:  // perch on the exit portal
        fly_to(e, v3(0, 65, 0), d->speed * 0.6f, 0.04f);
        break;
    }
    // anything touching the body is knocked away and hurt
    ENT_FOREACH(o) {
        if (o == e || (o->kind != ENT_PLAYER && o->kind != ENT_MOB)) continue;
        if (aabb_overlap(entity_aabb(o), entity_aabb(e)) && e->attack_cooldown % MC(10) == 0) {
            entity_damage(o, d->attack, DMG_MELEE, e);
            entity_knockback(o, e->pos, 1.5f);
            o->vel.y += 6;
        }
    }
    if (fabsf(e->vel.x) + fabsf(e->vel.z) > 0.1f) e->yaw = e->body_yaw = atan2f(e->vel.x, e->vel.z);
}

// ---------------------------------------------------------------- Underground City: Morrowgate, Keeper of the Deep
// Sits on the throne of the keep until someone comes close.  A heavy melee brute with a ground slam (every ~8 s,
// hurts and launches everything within 6 blocks) and lantern fire at range.  Below half health: phase II,
// summons Hollow Sentinels, throws fire faster.  Drops the Void Sigil that opens the Space portal.
int g_keeper_slams;
static void keeper_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    f32 dm = boss_diff_mul();
    Entity *t = find_target(e);
    if (!t) { if (e->phase == 0 && e->health >= e->max_health) { stop(e); return; } wander(e, d->speed * 0.5f, 6); return; }
    g_ents.boss = e->id;
    if (e->phase == 0) { e->phase = 1; title_show("Morrowgate", "Keeper of the Deep rises from the throne"); e->special_timer = MC(120); }
    if (e->phase == 1 && e->health < e->max_health * 0.5f) {
        e->phase = 2; title_show("Morrowgate", "The lanterns burn brighter");
        for (int i = 0; i < 3; i++) { Entity *m = mob_spawn(MOB_HOLLOW_SENTINEL, v3_add(e->pos, v3(frand_range(-4, 4), 0.5f, frand_range(-4, 4)))); if (m) { m->target = t->id; fx_poof(m->pos, 1, 2); } }
    }
    look_at(e, entity_eye(t));
    f32 dist = v3_dist(e->pos, t->pos);
    if (--e->special_timer <= 0) {   // ground slam
        e->special_timer = MC(e->phase == 2 ? 110 : 160);
        if (dist < 7) {
            g_keeper_slams++;
            sound_play_at(SND_EXPLODE, e->pos, 0.8f, 0.6f);
            for (int k = 0; k < 24; k++) fx_smoke(v3_add(e->pos, v3(cosf(k * 0.26f) * 3, 0.2f, sinf(k * 0.26f) * 3)), 1, 0.2f, rgba_u32(120, 110, 100, 255));
            ENT_FOREACH(o) if ((o->kind == ENT_PLAYER || (o->kind == ENT_MOB && o->mob != MOB_HOLLOW_SENTINEL && o != e)) && v3_dist(o->pos, e->pos) < 6 && o->on_ground) {
                entity_damage(o, d->attack * 0.75f * dm, DMG_MELEE, e); entity_knockback(o, e->pos, 1.2f); o->vel.y += 9;
            }
        }
    }
    if (dist > 5 && e->attack_cooldown <= 0 && entity_can_see(e, t)) { shoot(e, t, PROJ_SMALL_FIREBALL); e->attack_cooldown = MC(e->phase == 2 ? 35 : 60); }
    hostile_melee(e, t, d->speed * (e->phase == 2 ? 1.25f : 1.0f));
}

// ---------------------------------------------------------------- the Sift: souls, the Luminaries, the Avatar of Thalor
static void soul_ai(Entity *e) {   // drifts peacefully; stops and turns to face anyone close by
    const MobDef *d = &MOB_DEFS[e->mob];
    e->no_gravity = true;
    f32 bob = sinf(e->age * 0.05f + (f32)(e->id & 15)) * 0.3f;
    Entity *p = player_ent();
    if (p && v3_dist(p->pos, e->pos) < 6) { look_at(e, entity_eye(p)); e->vel = v3_scale(e->vel, 0.85f); return; }
    if (--e->ai_timer <= 0 || v3_dist(e->pos, e->goal) < 1) { e->goal = random_goal(e, 10, 2); e->ai_timer = irand(MC(60), MC(200)); }
    fly_to(e, v3_add(e->goal, v3(0, bob, 0)), d->speed * 0.4f, 0.03f);
}
static void god_ai(Entity *e) {   // hovers in place, watches the player, sheds a little light
    e->no_gravity = true;
    e->vel = v3(0, sinf(e->age * 0.03f) * 0.15f, 0);
    e->health = e->max_health;
    Entity *p = player_ent();
    if (p && v3_dist(p->pos, e->pos) < 24) { look_at(e, entity_eye(p)); e->body_yaw = e->yaw = approach_angle(e->body_yaw, yaw_to(e->pos, p->pos), 0.05f); }
    if (e->age % 15 == 0) fx_smoke(v3_add(e->pos, v3(frand_range(-0.8f, 0.8f), e->height + 0.3f, frand_range(-0.8f, 0.8f))), 1, 0.05f, rgba_u32(255, 240, 180, 255));
}
int g_soul_talks, g_avatar_eruptions;
static void avatar_ai(Entity *e) {   // Thalor's trial: a crystal-studded stone giant
    const MobDef *d = &MOB_DEFS[e->mob];
    f32 dm = boss_diff_mul();
    Entity *t = find_target(e);
    if (!t) { wander(e, d->speed * 0.5f, 6); return; }
    g_ents.boss = e->id;
    if (e->phase == 0) { e->phase = 1; title_show("Trial of Stone", "Defeat the Avatar of Thalor"); e->special_timer = MC(100); }
    if (e->phase == 1 && e->health < e->max_health * 0.5f) { e->phase = 2; title_show("Avatar of Thalor", "The stone remembers its strength"); e->special_timer = MC(40); }
    look_at(e, entity_eye(t));
    if (e->special_timer > 0) e->special_timer--;
    if (e->special_timer == MC(30)) {   // crystal eruption: the ground under the target starts to glow...
        e->goal = t->pos;
        for (int k = 0; k < 16; k++) fx_smoke(v3_add(e->goal, v3(cosf(k * 0.4f) * 2.2f, 0.1f, sinf(k * 0.4f) * 2.2f)), 1, 0.05f, rgba_u32(150, 230, 255, 255));
    }
    if (e->special_timer == 0) {   // ...and bursts
        g_avatar_eruptions++;
        e->special_timer = MC(e->phase == 2 ? 90 : 140);
        sound_play_at(SND_EXPLODE, e->goal, 0.7f, 1.4f);
        for (int k = 0; k < 20; k++) fx_smoke(v3_add(e->goal, v3(frand_range(-2, 2), frand_range(0, 3), frand_range(-2, 2))), 1, 0.2f, rgba_u32(170, 240, 255, 255));
        ENT_FOREACH(o) if (o->kind == ENT_PLAYER && v3_dist(o->pos, e->goal) < 2.6f) { entity_damage(o, d->attack * 0.8f * dm, DMG_MAGIC, e); o->vel.y += 10; }
    }
    hostile_melee(e, t, d->speed * (e->phase == 2 ? 1.3f : 1.0f));
}

// ---------------------------------------------------------------- Hell: the Brimstone Revenant (fortress mini-boss)
static void revenant_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    f32 dm = boss_diff_mul();
    Entity *t = find_target(e);
    if (!t) { if (e->phase == 0) { stop(e); return; } wander(e, d->speed * 0.5f, 6); return; }
    g_ents.boss = e->id;
    if (e->phase == 0) { e->phase = 1; title_show("Brimstone Revenant", "The fortress's dead lord awakens"); e->special_timer = MC(120); }
    if (e->phase == 1 && e->health < e->max_health * 0.5f) {
        e->phase = 2; title_show("Brimstone Revenant", "Ash and bone, rise!");
        for (int i = 0; i < 3; i++) { Entity *m = mob_spawn(i < 2 ? MOB_WITHER_SKELETON : MOB_CINDER_SPECTER, v3_add(e->pos, v3(frand_range(-4, 4), 0.5f, frand_range(-4, 4)))); if (m) { m->target = t->id; fx_poof(m->pos, 1, 2); } }
    }
    look_at(e, entity_eye(t));
    if (e->special_timer > 0) e->special_timer--;
    if (e->special_timer == 0) {   // fire nova
        g_revenant_novas++;
        e->special_timer = MC(e->phase == 2 ? 120 : 180);
        sound_play_at(SND_FIRE, e->pos, 1, 0.5f);
        for (int k = 0; k < 32; k++) fx_flame(v3_add(e->pos, v3(cosf(k * 0.196f) * 5, 0.5f, sinf(k * 0.196f) * 5)));
        ENT_FOREACH(o) if (o->kind == ENT_PLAYER && v3_dist(o->pos, e->pos) < 7) { entity_damage(o, d->attack * 0.6f * dm, DMG_FIRE, e); o->fire = MAXI(o->fire, MC(100)); entity_knockback(o, e->pos, 1.0f); }
    }
    f32 dist = v3_dist(e->pos, t->pos);
    if (dist > 4 && e->attack_cooldown <= 0 && entity_can_see(e, t)) {
        for (int i = 0; i < (e->phase == 2 ? 3 : 1); i++) shoot(e, t, PROJ_SMALL_FIREBALL);
        e->attack_cooldown = MC(e->phase == 2 ? 40 : 60);
    }
    hostile_melee(e, t, d->speed * (e->phase == 2 ? 1.25f : 1.0f));
}

// ---------------------------------------------------------------- Space: Void Drifter
// A violet jelly that hovers above its prey, then dives to sting; the sting briefly lifts you off the ground.
static void drifter_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    e->no_gravity = true;
    f32 bob = sinf(e->age * 0.04f + (f32)(e->id & 63)) * 0.6f;
    Entity *t = find_target(e);
    if (t) {
        look_at(e, entity_eye(t));
        if (e->special_timer > 0) {   // diving
            e->special_timer--;
            fly_to(e, v3_add(t->pos, v3(0, t->height * 0.5f, 0)), d->speed * 1.8f, 0.12f);
            if (melee(e, t, d->attack)) e->special_timer = 0;
        } else {
            f32 a = e->age * 0.02f + (f32)(e->id & 31);
            fly_to(e, v3_add(t->pos, v3(sinf(a) * 3, 4.5f + bob, cosf(a) * 3)), d->speed, 0.05f);
            if (e->attack_cooldown <= 0 && v3_dist(e->pos, t->pos) < 10 && frand() < 0.02f) e->special_timer = MC(40);
        }
        return;
    }
    if (--e->ai_timer <= 0 || v3_dist(e->pos, e->goal) < 1) { e->goal = random_goal(e, 12, 4); e->ai_timer = irand(MC(60), MC(160)); }
    fly_to(e, v3_add(e->goal, v3(0, bob, 0)), d->speed * 0.35f, 0.03f);
}

// ---------------------------------------------------------------- Space: Zorvath, the Cosmic Tyrant
// The ultimate boss of Space.  Three phases (e->phase 1..3, 0 = not yet awakened), switched at 66% and 33% health:
//   I   Herald of Stars   – orbits the player, star-bolt volleys, healed by Star Crystals
//   II  The Gravity Well  – adds periodic gravity pulls toward itself and Void Drifter minions
//   III Collapse of Stars – faster, wider volleys, meteor rain around the player, crawler + drifter minions
// Phase and timers live in Entity.phase / special_timer, which entities.dat saves, so a fight survives save/load.
static const char *TYRANT_TITLE[4] = {"", "Zorvath", "Phase II", "Phase III"};
static const char *TYRANT_SUB[4] = {"", "the Cosmic Tyrant awakens", "The Gravity Well", "Collapse of the Stars"};
static void tyrant_enter_phase(Entity *e, int phase) {
    e->phase = (u8)phase;
    title_show(TYRANT_TITLE[phase], TYRANT_SUB[phase]);
    e->invuln = MAXI(e->invuln, MC(30));
    e->special_timer = MC(50);   // a breath before attacking again
    sound_play_at(SND_EXPLODE, e->pos, 1.0f, 0.5f);
    fx_explosion(v3_add(e->pos, v3(0, e->height * 0.5f, 0)), 2);
    ENT_FOREACH(o) if (o->kind == ENT_PLAYER && v3_dist(o->pos, e->pos) < 14) { entity_knockback(o, e->pos, 2.0f); o->vel.y += 5; }   // shockwave
    LOG_INFO("Zorvath enters phase %d", phase);
}
static int tyrant_minion_count(Entity *e) {
    int n = 0;
    ENT_FOREACH(o) if (o->kind == ENT_MOB && mob_is_space_minion(o->mob) && o->death_time == 0 && v3_dist(o->pos, e->pos) < 80) n++;
    return n;
}
static void tyrant_volley(Entity *e, Entity *p, int n, f32 dmg) {
    vec3 from = v3_add(e->pos, v3(0, e->height * 0.75f, 0));
    vec3 dir = v3_norm(v3_sub(entity_eye(p), from));
    for (int k = 0; k < n; k++) {
        f32 off = (k - (n - 1) * 0.5f) * 0.16f, c = cosf(off), s = sinf(off);
        vec3 d2 = v3(dir.x * c - dir.z * s, dir.y, dir.x * s + dir.z * c);
        Entity *b = proj_spawn(PROJ_STAR_BOLT, v3_add(from, v3_scale(d2, 2.0f)), v3_scale(d2, 20.0f), e->id);
        if (b) b->damage = dmg;
    }
    sound_play_at(SND_FIRE, e->pos, 1.0f, 0.6f);
}
static void tyrant_ai(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    g_ents.boss = e->id;
    e->no_gravity = true;
    f32 frac = e->health / fmaxf(1, e->max_health), dm = boss_diff_mul();
    Entity *p = player_target();
    if (e->phase == 0) {   // dormant until a player comes close
        fly_to(e, v3_add(e->anchor, v3(0, sinf(e->age * 0.02f) * 2, 0)), 2, 0.02f);
        Entity *pe = player_ent();   // any nearby player wakes it (even in creative), and so does being hurt
        if ((pe && v3_dist(pe->pos, e->pos) < 96) || e->health < e->max_health) tyrant_enter_phase(e, 1);
        return;
    }
    if (e->phase == 1 && frac <= 0.66f) tyrant_enter_phase(e, 2);
    if (e->phase == 2 && frac <= 0.33f) tyrant_enter_phase(e, 3);
    if (e->special_timer > 0) e->special_timer--;
    // Star Crystals still standing keep healing it during phase I
    if (e->phase == 1 && e->age % MC(20) == 0) ENT_FOREACH(o) if (o->kind == ENT_CRYSTAL && v3_dist(o->pos, e->pos) < 64) { entity_heal(e, 2); break; }
    if (!p || p->health <= 0) {   // nobody to fight: drift back over the central island
        fly_to(e, v3(cosf(e->age * 0.004f) * 20, 82, sinf(e->age * 0.004f) * 20), d->speed * 0.5f, 0.02f);
        return;
    }
    look_at(e, entity_eye(p));
    f32 a = e->age * (0.004f + 0.0025f * e->phase), rad = e->phase == 3 ? 10.0f : 15.0f;
    fly_to(e, v3_add(p->pos, v3(cosf(a) * rad, 8 + sinf(a * 3) * 2, sinf(a) * rad)), d->speed * (1 + 0.15f * e->phase), 0.03f);
    if (fabsf(e->vel.x) + fabsf(e->vel.z) > 0.1f) e->body_yaw = e->yaw = approach_angle(e->body_yaw, yaw_to(e->pos, p->pos), 0.1f);
    f32 dist = v3_dist(e->pos, p->pos);
    // star-bolt volleys
    if (e->attack_cooldown <= 0 && e->special_timer <= 0 && dist < 64) {
        tyrant_volley(e, p, e->phase == 1 ? 1 : e->phase == 2 ? 3 : 5, 5 * dm);
        e->attack_cooldown = MC(e->phase == 1 ? 50 : e->phase == 2 ? 40 : 30);
    }
    // gravity well: pulls every player toward the tyrant for 2.5 s
    if (e->phase >= 2) {
        int cyc = e->age % MC(e->phase == 3 ? 200 : 280);
        if (cyc == 0) { g_tyrant_pulls++; snprintf(G.actionbar, sizeof G.actionbar, "Zorvath's gravity drags you in!"); G.actionbar_t = 90; sound_play_at(SND_FIRE, e->pos, 1, 0.4f); }
        if (cyc < MC(50)) ENT_FOREACH(o) {
            if (o->kind != ENT_PLAYER || v3_dist(o->pos, e->pos) > 48 || v3_dist(o->pos, e->pos) < 3) continue;
            vec3 dir = v3_norm(v3_sub(e->pos, o->pos));
            o->vel = v3_add(o->vel, v3_scale(dir, 0.22f));
            if (v3_len(o->vel) > 14) o->vel = v3_scale(v3_norm(o->vel), 14);
            if (cyc % 4 == 0) fx_smoke(v3_add(o->pos, v3(frand_range(-1, 1), frand_range(0, 2), frand_range(-1, 1))), 1, 0.05f, rgba_u32(150, 110, 255, 255));
        }
    }
    // minions
    if (e->phase >= 2 && e->age % MC(e->phase == 3 ? 240 : 360) == MC(100)) {
        int have = tyrant_minion_count(e), want = e->phase == 3 ? 6 : 4;
        for (int i = have; i < MINI(have + 2, want); i++) {
            bool crawler = e->phase == 3 && (i & 1) && p->on_ground;
            vec3 at = crawler ? v3_add(p->pos, v3(frand_range(-6, 6), 1, frand_range(-6, 6))) : v3_add(e->pos, v3(frand_range(-4, 4), frand_range(-2, 2), frand_range(-4, 4)));
            Entity *m = mob_spawn(crawler ? MOB_ASTEROID_CRAWLER : MOB_VOID_DRIFTER, at);
            if (m) { m->target = p->id; fx_poof(at, 1, 1); g_tyrant_summons++; }
        }
    }
    // meteor rain around the player
    if (e->phase == 3 && e->age % MC(50) == 0) for (int i = 0; i < 3; i++) {
        vec3 at = v3_add(p->pos, v3(frand_range(-9, 9), 26, frand_range(-9, 9)));
        Entity *m = proj_spawn(PROJ_STAR_BOLT, at, v3(frand_range(-1, 1), -22, frand_range(-1, 1)), e->id);
        if (m) { m->color = 1; m->damage = 7 * dm; m->width = m->height = 1.0f; g_tyrant_meteors++; }
    }
    // touching the colossus hurts
    if (e->age % MC(10) == 0) ENT_FOREACH(o) {
        if (o == e || (o->kind != ENT_PLAYER && o->kind != ENT_MOB) || (o->kind == ENT_MOB && mob_is_space_minion(o->mob))) continue;
        if (aabb_overlap(entity_aabb(o), entity_aabb(e))) { entity_damage(o, d->attack * dm, DMG_MELEE, e); entity_knockback(o, e->pos, 1.5f); o->vel.y += 5; }
    }
}

// ======================================================================== main tick
static void sun_burn(Entity *e) {
    if (!mob_has(e->mob, MF_BURNS) || e->in_water || g_world.dim != DIM_OVERWORLD || world_daylight() < 0.8f || e->fire > 0) return;
    ivec3 p = v3_floor(v3_add(e->pos, v3(0, e->height, 0)));
    if (L_SKY(world_get_light(p.x, p.y, p.z)) == 15 && frand() < 0.03f) e->fire = MC(160);
}

void mob_tick(Entity *e) {
    const MobDef *d = &MOB_DEFS[e->mob];
    if (e->death_time > 0) {
        e->death_time++;
        e->vel.x *= 0.8f; e->vel.z *= 0.8f;
        if (!e->no_gravity) entity_physics(e);
        if (e->mob == MOB_ENDER_DRAGON || e->mob == MOB_COSMIC_TYRANT) { e->vel = v3(0, 0.3f, 0); e->pos.y += 0.02f; if (e->death_time % 5 == 0) fx_explosion(v3_add(e->pos, v3(frand_range(-4, 4), frand_range(0, 4), frand_range(-4, 4))), 1); }
        if (e->death_time > (e->mob == MOB_ENDER_DRAGON ? MC(200) : e->mob == MOB_COSMIC_TYRANT ? MC(120) : 20)) { fx_poof(e->pos, e->width, e->height); e->remove = true; }
        return;
    }
    effects_tick(e);
    if (e->remove || e->health <= 0) return;
    if (e->mob == MOB_VILLAGER || e->mob == MOB_WANDERING_TRADER || e->mob == MOB_DELVARI_MERCHANT) villager_tick(e);
    if (e->rider && ride_mob_tick(e)) return;
    if (e->attack_cooldown > 0) e->attack_cooldown--;
    if (e->jump_cooldown > 0 && d->ai != AIK_SLIME) e->jump_cooldown--;
    if (e->attack_anim > 0) e->attack_anim = fmaxf(0, e->attack_anim - 0.12f);
    if (e->love > 0) { e->love--; if (e->age % 20 == 0) fx_hearts(v3_add(e->pos, v3(0, e->height, 0))); }
    if (e->breed_cooldown > 0) e->breed_cooldown--;
    if (e->baby && e->grow_age < 0 && ++e->grow_age >= 0 && d->ai != AIK_HOSTILE) {   // grow up
        e->baby = false; mob_set_size(e, e->size ? e->size : 1);
        if (e->mob == MOB_TURTLE) item_drop(e->pos, stack_of(I_TURTLE_SCUTE, 1), v3(0, 3, 0), 10);
        if (e->mob == MOB_TADPOLE) { vec3 p = e->pos; e->remove = true; mob_spawn(MOB_FROG, p); return; }
    }
    if (e->mob == MOB_TADPOLE && !e->baby && e->age > MC(24000)) { vec3 p = e->pos; e->remove = true; mob_spawn(MOB_FROG, p); return; }
    sun_burn(e);
    // species chores
    if (--e->egg_timer <= 0) {
        e->egg_timer = MC(irand(6000, 12000));
        if (!e->baby) {
            if (e->mob == MOB_CHICKEN) { item_drop(e->pos, stack_of(I_EGG, 1), v3(0, 1, 0), 10); sound_play_at(SND_POP, e->pos, 0.5f, 1.2f); }
            if (e->mob == MOB_ARMADILLO) item_drop(e->pos, stack_of(I_ARMADILLO_SCUTE, 1), v3(0, 1, 0), 10);
            if (e->mob == MOB_SNIFFER) item_drop(e->pos, stack_of(frand() < 0.5f ? I_TORCHFLOWER_SEEDS : I_PITCHER_POD, 1), v3(0, 3, 0), 10);
        }
    }
    if (e->mob == MOB_SHEEP && e->sheared && frand() < 0.001f) {   // eat grass to regrow wool
        ivec3 b = v3_floor(v3_add(e->pos, v3(0, -0.5f, 0)));
        Block g = world_get_block(b.x, b.y, b.z), a = world_get_block(b.x, b.y + 1, b.z);
        if (a == B_SHORT_GRASS) { world_set_block(b.x, b.y + 1, b.z, B_AIR); e->sheared = false; }
        else if (BLOCKS[g].base == B_GRASS_BLOCK) { world_set_block(b.x, b.y, b.z, B_DIRT); e->sheared = false; }
    }
    if (e->mob == MOB_FROG && frand() < 0.05f) {   // frogs eat small slimes & magma cubes
        Entity *s = find_mob_target(e, 5, pred_small_slime);
        if (s) {
            int b = world_biome_at((int)e->pos.x, (int)e->pos.y, (int)e->pos.z);
            f32 t = BIOMES[b].temperature;
            item_drop(s->pos, stack_of(s->mob == MOB_MAGMA_CUBE ? (t < 0.5f ? I_VERDANT_FROGLIGHT : t > 1.0f ? I_PEARLESCENT_FROGLIGHT : I_OCHRE_FROGLIGHT) : I_SLIME_BALL, 1), v3(0, 2, 0), 10);
            s->remove = true; sound_play_at(SND_EAT, e->pos, 0.6f, 1.3f);
        }
    }
    if (e->mob == MOB_BEE && e->sheared && --e->special_timer <= 0) { entity_damage(e, 100, DMG_GENERIC, NULL); return; }
    if (e->mob == MOB_ZOMBIE_VILLAGER && e->phase == 9 && --e->special_timer <= 0) {   // cured
        vec3 p = e->pos; e->remove = true; mob_spawn(MOB_VILLAGER, p); fx_poof(p, 0.6f, 1.9f); sound_play_at(SND_LEVELUP, p, 1, 1.5f); return;
    }
    if (e->mob == MOB_WANDERING_TRADER && --e->special_timer <= 0) { fx_poof(e->pos, e->width, e->height); e->remove = true; return; }
    if (e->mob == MOB_STRIDER && e->in_lava) e->vel.y = fmaxf(e->vel.y, 2.0f);
    // breathing
    bool head_in_water = block_is_water(world_get_block((int)floorf(e->pos.x), (int)floorf(e->pos.y + e->height * 0.85f), (int)floorf(e->pos.z)));
    if (mob_has(e->mob, MF_AQUATIC)) {
        if (!e->in_water) { if (--e->air < -MC(20)) { e->air = 0; entity_damage(e, 2, DMG_DROWN, NULL); } }
        else e->air = MC(300);
    } else if (head_in_water && !mob_has(e->mob, MF_IMMUNE_DROWN) && !mob_has(e->mob, MF_UNDEAD) && !effect_has(e, EFF_WATER_BREATHING)) {
        if (--e->air < -MC(20)) { e->air = 0; entity_damage(e, 2, DMG_DROWN, NULL); }
    } else e->air = MC(300);
    // behaviour
    if (e->mob == MOB_IRON_GOLEM && !e->tamed && e->age % MC(20) == 5) {   // golems defend villagers who dislike you
        Entity *p = player_target();
        if (p) ENT_FOREACH(o) if (o->kind == ENT_MOB && o->mob == MOB_VILLAGER && o->rep <= -15 && v3_dist(o->pos, e->pos) < 24) { e->target = p->id; e->anger = MC(600); break; }
    }
    if (e->age % MC(10) == 0 && e->health > 0 && rule_i(GR_MAX_ENTITY_CRAMMING) > 0 && !mob_has(e->mob, MF_FLYING)) {   // gamerule maxEntityCramming
        int n = 0; AABB a = entity_aabb(e);
        ENT_FOREACH(o) if (o != e && (o->kind == ENT_MOB || o->kind == ENT_PLAYER) && o->health > 0 && aabb_overlap(a, entity_aabb(o))) n++;
        if (n >= rule_i(GR_MAX_ENTITY_CRAMMING)) { entity_damage(e, 6, DMG_CRAMMING, NULL); g_cramming_hits++; }
    }
    switch (d->ai) {
    case AIK_PASSIVE: case AIK_VILLAGER: passive_ai(e); break;
    case AIK_NEUTRAL: neutral_ai(e); break;
    case AIK_HOSTILE: hostile_ai(e); break;
    case AIK_RANGED: ranged_ai(e); break;
    case AIK_CREEPER: creeper_ai(e); break;
    case AIK_SLIME: slime_ai(e); break;
    case AIK_FLYER: flyer_ai(e); break;
    case AIK_FISH: fish_ai(e); break;
    case AIK_SQUID: squid_ai(e); break;
    case AIK_SHULKER: shulker_ai(e); break;
    case AIK_GUARDIAN: guardian_ai(e); break;
    case AIK_ENDERMAN: enderman_ai(e); break;
    case AIK_SPIDER: spider_ai(e); break;
    case AIK_PHANTOM: phantom_ai(e); break;
    case AIK_GOLEM: golem_ai(e); break;
    case AIK_WARDEN: warden_ai(e); break;
    case AIK_CREAKING: creaking_ai(e); break;
    case AIK_BOSS_WITHER: wither_ai(e); break;
    case AIK_BOSS_DRAGON: dragon_ai(e); break;
    case AIK_EVOKER: evoker_ai(e); break;
    case AIK_WITCH: witch_ai(e); break;
    case AIK_BREEZE: breeze_ai(e); break;
    case AIK_BLAZE: blaze_ai(e); break;
    case AIK_GHAST: ghast_ai(e); break;
    case AIK_SILVERFISH: silverfish_ai(e); break;
    case AIK_GHOST: ghost_ai(e); break;
    case AIK_DRIFTER: drifter_ai(e); break;
    case AIK_BOSS_TYRANT: tyrant_ai(e); break;
    case AIK_BOSS_KEEPER: keeper_ai(e); break;
    case AIK_SOUL: soul_ai(e); break;
    case AIK_GOD: god_ai(e); break;
    case AIK_BOSS_AVATAR: avatar_ai(e); break;
    case AIK_BOSS_REVENANT: revenant_ai(e); break;
    default: wander(e, d->speed, 8); break;
    }
    if (e->remove) return;
    // swimming: non-aquatic walkers paddle up, aquatic ones use their own steering
    bool aquatic_now = (d->ai == AIK_FISH || d->ai == AIK_SQUID || d->ai == AIK_GUARDIAN) && e->in_water;
    if (e->in_water && !aquatic_now && !mob_has(e->mob, MF_FLYING) && !(mob_has(e->mob, MF_SWIMS) && e->target)) {
        if (e->mob != MOB_DROWNED || !e->target) e->vel.y = fmaxf(e->vel.y, 1.6f);
    }
    if ((d->ai == AIK_FISH || d->ai == AIK_SQUID || d->ai == AIK_GUARDIAN) && !e->in_water) e->no_gravity = false;
    // movement integration
    if (e->no_gravity) {
        vec3 want = v3_scale(e->vel, DT);
        if (e->no_clip) e->pos = v3_add(e->pos, want);
        else {
            bool ng = e->no_gravity;
            entity_physics(e);   // gravity is skipped because no_gravity is set
            e->no_gravity = ng;
        }
        if (!aquatic_now && mob_has(e->mob, MF_FLYING)) e->vel = v3_scale(e->vel, 0.98f);
    } else {
        entity_physics(e);
        if (e->in_water && mob_has(e->mob, MF_SWIMS) && d->ai == AIK_HOSTILE) e->vel.y *= 0.9f;
    }
    if (d->ai == AIK_SLIME && e->on_ground && e->swell > 0.9f) {}
    // animation
    f32 sp = sqrtf(e->vel.x * e->vel.x + e->vel.z * e->vel.z);
    e->limb_amount = lerpf(e->limb_amount, fminf(1, sp / 4.0f), 0.3f);
    e->limb_swing += sp * DT * 2.6f;
    e->head_yaw = approach_angle(e->head_yaw, e->body_yaw, 0.05f);
    if (frand() < 0.002f) mob_sound(e, 0);
    // despawn
    if (!e->persistent && !e->tamed && mob_hostile(e->mob) && !mob_has(e->mob, MF_BOSS)) {
        Entity *p = player_ent();
        f32 dd = p ? v3_dist(p->pos, e->pos) : 1e9f;
        if (dd > 128 || (dd > 32 && irand(0, MC(800)) == 0)) e->remove = true;
    }
}

// ======================================================================== damage / death hooks
void mob_on_hurt(Entity *e, Entity *src) {
    const MobDef *d = &MOB_DEFS[e->mob];
    Entity *attacker = src;
    if (src && (src->kind == ENT_ARROW || src->kind == ENT_PROJ)) attacker = entity_get(src->owner);
    if (e->health > 0) mob_sound(e, 1);
    if (d->ai == AIK_PASSIVE || d->ai == AIK_VILLAGER || (d->ai == AIK_FISH && e->mob != MOB_DOLPHIN && e->mob != MOB_ZOMBIE_NAUTILUS && e->mob != MOB_NAUTILUS && e->mob != MOB_AXOLOTL)) {
        if (!e->tamed) { e->ai = AI_PANIC; e->ai_timer = MC(irand(60, 120)); e->yaw = e->body_yaw = attacker ? yaw_to(attacker->pos, e->pos) : e->yaw; }
    }
    if (d->ai == AIK_SQUID) fx_smoke(e->pos, 12, 0.6f, e->mob == MOB_GLOW_SQUID ? rgba_u32(80, 230, 200, 255) : rgba_u32(20, 20, 20, 255));
    if (!attacker || attacker == e) return;
    if (d->ai == AIK_ENDERMAN && src && src->kind != ENT_PLAYER && src->kind != ENT_MOB) teleport_random(e, 16);
    if (d->ai == AIK_SHULKER && e->health < e->max_health * 0.5f && frand() < 0.25f) teleport_random(e, 8);
    if (e->mob == MOB_EVOKER || e->mob == MOB_WITCH) e->target = attacker->id;
    bool angerable = d->ai == AIK_NEUTRAL || d->ai == AIK_ENDERMAN || d->ai == AIK_SPIDER || d->ai == AIK_GOLEM || d->ai == AIK_FISH || d->ai == AIK_WARDEN;
    if (e->tamed && attacker->kind == ENT_PLAYER) angerable = false;
    if (angerable) {
        e->target = attacker->id; e->anger = MC(irand(400, 780));
        if (mob_has(e->mob, MF_GROUP_ANGER)) ENT_FOREACH(o) if (o != e && o->kind == ENT_MOB && o->mob == e->mob && v3_dist(o->pos, e->pos) < 20 && !o->tamed) { o->target = attacker->id; o->anger = MC(irand(400, 780)); }
    }
    if (mob_hostile(e->mob)) e->target = attacker->id;
    // tamed wolves defend their owner
    if (e->kind == ENT_PLAYER || (attacker->kind == ENT_PLAYER && e->kind == ENT_MOB)) {
        Entity *victim = attacker->kind == ENT_PLAYER ? e : NULL;
        ENT_FOREACH(w) if (w->kind == ENT_MOB && w->mob == MOB_WOLF && w->tamed && !w->sitting && victim && victim != w) w->target = victim->id;
    }
    if (e->mob == MOB_SILVERFISH && attacker->kind == ENT_PLAYER) {   // wake nearby infested blocks
        ivec3 c = v3_floor(e->pos);
        for (int dy = -3; dy <= 3; dy++) for (int dz = -5; dz <= 5; dz++) for (int dx = -5; dx <= 5; dx++) {
            Block b = world_get_block(c.x + dx, c.y + dy, c.z + dz);
            if (BLOCKS[b].key && !strncmp(BLOCKS[b].key, "infested_", 9) && frand() < 0.3f) { world_set_block(c.x + dx, c.y + dy, c.z + dz, B_AIR); mob_spawn(MOB_SILVERFISH, v3(c.x + dx + 0.5f, (f32)c.y + dy, c.z + dz + 0.5f)); }
        }
    }
}

static void activate_exit_portal(void) {
    for (int dz = -2; dz <= 2; dz++) for (int dx = -2; dx <= 2; dx++) if (dx * dx + dz * dz <= 5 && (dx || dz)) world_set_block(dx, 61, dz, B_END_PORTAL);
    world_set_block(0, 65, 0, B_DRAGON_EGG);
}

// ---- talking to the Sift's inhabitants
static const char *SOUL_LORE[] = {
    "I kept a lighthouse once. Here, the light keeps me.",
    "The Luminaries do not ask to be worshipped. They ask you to be brave.",
    "Solenne keeps the dawn in the Golden Meadows. Bring her the light of four crystals.",
    "Zephyrine lives where the clouds touch the sky. Climb her spire and she will meet you.",
    "Thalor tests every heart against stone. His arena lies among the crystal trees.",
    "Far below, the Delvari kept their lamps burning for a thousand years. We remember them.",
    "Zorvath swallowed whole constellations, and you stood against it. Rest a moment.",
    "Nothing here is in a hurry. Neither should you be.",
    "I built a little house of oak and cobblestone. I miss the sound of rain on its roof.",
    "When all three Luminaries have blessed you, the Sift will show you its last secret.",
};
static void sift_say(Entity *m, const char *line) { game_message(rgba_u32(255, 236, 170, 255), "%s: %s", MOB_DEFS[m->mob].name, line); }
static void sift_trial_done(int bit) {
    if (g_world.sift_trials & bit) return;
    g_world.sift_trials |= (u8)bit;
    sound_play(SND_LEVELUP, 1, 1.2f);
    if (g_world.sift_trials == 7 && !g_world.ascended) {   // the ending
        g_world.ascended = true;
        title_show("Ascension", "All three Luminaries have blessed you");
        G.credits_t = 0;
        game_open_screen(SCR_CREDITS);
    }
}
bool sift_interact(Entity *m) {
    Entity *p = player_ent();
    if (!p || m->kind != ENT_MOB) return false;
    switch (m->mob) {
    case MOB_SIFT_SOUL:
        if (g_player.sneaking) { G.trade_with = m->id; G.trade_sel = -1; game_open_screen(SCR_TRADE); return true; }
        g_soul_talks++; progress_grant("soul");
        sift_say(m, SOUL_LORE[(m->id + (u32)g_soul_talks) % ARRAY_LEN(SOUL_LORE)]);
        if (!m->sheared) {   // each soul leaves a small gift once
            static const int B[3] = {EFF_BLESSING_OF_LIGHT, EFF_BLESSING_OF_THE_SKY, EFF_BLESSING_OF_VIGOR};
            m->sheared = true;
            effect_add(p, B[irand(0, 2)], 0, 90 * TICKRATE);
            player_give(stack_of(I_SOUL_ESSENCE, 1));
            game_message(rgba_u32(200, 230, 255, 255), "The soul leaves you a little of its light.");
        }
        return true;
    case MOB_GOD_SOLENNE:
        if (!(g_world.sift_trials & 1)) {
            if (inventory_count(&g_player.inv, I_RADIANT_CRYSTAL) >= 4) {
                inventory_remove(&g_player.inv, I_RADIANT_CRYSTAL, 4);
                sift_say(m, "You carried the light all this way. Walk in the dawn, always.");
                effect_add(p, EFF_BLESSING_OF_LIGHT, 1, 20 * 60 * TICKRATE);
                player_give(stack_of(I_SOUL_ESSENCE, 4));
                title_show("Trial of Dawn", "Solenne grants the Blessing of Light");
                sift_trial_done(1);
            } else sift_say(m, "Bring me four Radiant Crystals from the Crystal Groves, and I will light your way.");
        } else { sift_say(m, "The dawn remembers you."); effect_add(p, EFF_BLESSING_OF_LIGHT, 1, 10 * 60 * TICKRATE); }
        return true;
    case MOB_GOD_ZEPHYRINE:
        if (!(g_world.sift_trials & 2)) {
            sift_say(m, "You climbed the winds to find me. That was the whole trial, and you passed it.");
            effect_add(p, EFF_BLESSING_OF_THE_SKY, 0, 20 * 60 * TICKRATE);
            title_show("Trial of Winds", "Zephyrine grants the Blessing of the Sky");
            sift_trial_done(2);
        } else { G.trade_with = m->id; G.trade_sel = -1; game_open_screen(SCR_TRADE); }
        return true;
    case MOB_GOD_THALOR:
        if (!(g_world.sift_trials & 4)) {
            bool alive = false;
            ENT_FOREACH(o) if (o->kind == ENT_MOB && o->mob == MOB_THALOR_AVATAR && o->death_time == 0) alive = true;
            if (alive) sift_say(m, "My avatar waits for you. Strike true.");
            else {
                sift_say(m, "Every heart is tested against stone. Face my avatar.");
                Entity *a = mob_spawn(MOB_THALOR_AVATAR, v3_add(m->pos, v3(0, -2, 8)));
                if (a) { a->target = p->id; fx_poof(a->pos, 2, 3); }
            }
        } else { sift_say(m, "Stand tall, stoneheart."); effect_add(p, EFF_BLESSING_OF_VIGOR, 1, 10 * 60 * TICKRATE); }
        return true;
    default: return false;
    }
}

// The Cosmic Tyrant falls: Space's exit portal opens and a gold gateway to the Sift rises on the central island.
static void open_sift_gateway(void) {
    int x0 = 0, z0 = 12, y = -1;
    for (int yy = world_build_top() - 8; yy > 2 && y < 0; yy--) if (BLOCKS[world_get_block(x0, yy, z0)].solid) y = yy + 1;
    if (y < 0) { y = 66; for (int i = -2; i <= 3; i++) for (int dz = -2; dz <= 2; dz++) world_set_block(x0 + i, y - 1, z0 + dz, B_GOLD_BLOCK); }
    for (int i = -1; i <= 2; i++) for (int h = 0; h < 4; h++) world_set_block(x0 + i, y + h, z0, B_AIR);
    for (int i = -1; i <= 2; i++) { u16 f = (i == -1 || i == 2) ? B_GLOWSTONE : B_GOLD_BLOCK; world_set_block(x0 + i, y - 1, z0, f); world_set_block(x0 + i, y + 3, z0, f); }
    for (int h = 0; h < 3; h++) { world_set_block(x0 - 1, y + h, z0, B_GOLD_BLOCK); world_set_block(x0 + 2, y + h, z0, B_GOLD_BLOCK); }
    for (int i = 0; i <= 1; i++) for (int h = 0; h < 3; h++) world_set_block_silent(x0 + i, y + h, z0, B_SIFT_PORTAL);
    g_world.sift_gate = iv3(x0, y, z0);
}
static void tyrant_defeated(Entity *e) {
    if (g_world.dim == DIM_END) { activate_exit_portal(); open_sift_gateway(); }
    g_world.tyrant_killed = true;
    title_show("Zorvath has fallen", g_world.dim == DIM_END ? "A gateway to the Sift has opened" : "The stars are quiet again");
    for (int i = 0; i < 30; i++) fx_smoke(v3_add(e->pos, v3(frand_range(-3, 3), frand_range(0, 5), frand_range(-3, 3))), 2, 0.3f, rgba_u32(255, 215, 90, 255));
}
void mob_on_death(Entity *e, Entity *killer) {
    const MobDef *d = &MOB_DEFS[e->mob];
    mob_sound(e, 2);
    Entity *who = killer;
    if (who && (who->kind == ENT_ARROW || who->kind == ENT_PROJ)) who = entity_get(who->owner);
    bool by_player = who && who->kind == ENT_PLAYER;
    if (e->mob == MOB_WITHER || e->mob == MOB_ENDER_DRAGON || e->mob == MOB_COSMIC_TYRANT || e->mob == MOB_DEEP_KEEPER || e->mob == MOB_THALOR_AVATAR || e->mob == MOB_BRIMSTONE_REVENANT) g_ents.boss = 0;
    raid_on_mob_death(e, killer);
    sculk_on_mob_death(e);
    if (e->mob == MOB_VILLAGER && killer && killer->kind == ENT_PLAYER) villager_hurt_by_player(e, true);
    if (killer && (killer->kind == ENT_PLAYER || (entity_get(killer->owner) && entity_get(killer->owner)->kind == ENT_PLAYER))) { scoreboard_event("totalKillCount", "Player", 1); progress_mob_killed(e); }
    effects_on_death(e);
    if (d->ai == AIK_SLIME && e->size > 1) {   // split
        int n = irand(2, 4);
        for (int i = 0; i < n; i++) {
            Entity *s = mob_spawn(e->mob, v3_add(e->pos, v3(frand_range(-0.5f, 0.5f), 0.5f, frand_range(-0.5f, 0.5f))));
            if (s) mob_set_size(s, e->size / 2);
        }
    }
    if (e->baby && e->mob != MOB_ZOMBIE && e->mob != MOB_HUSK) return;   // babies drop nothing
    if (!rule(GR_DO_MOB_LOOT)) return;
    for (int i = 0; i < 6; i++) {
        if (!d->drops[i].item) continue;
        int looting = by_player ? ench_level(*inventory_held(&g_player.inv), ENCH_LOOTING) : 0;
        if (irand(0, 99) >= d->drops[i].chance + looting * (d->drops[i].chance < 100 ? 1 : 0)) continue;
        int n = irand(d->drops[i].min, d->drops[i].max + looting);
        if (d->ai == AIK_SLIME && e->size > 1) n = 0;
        int item = d->drops[i].item;
        if (e->mob == MOB_SHEEP && item == I_WHITE_WOOL) { if (e->sheared) continue; item = I_WHITE_WOOL + 0; item = item_by_key((const char *[]){"white_wool", "orange_wool", "magenta_wool", "light_blue_wool", "yellow_wool", "lime_wool", "pink_wool", "gray_wool", "light_gray_wool", "cyan_wool", "purple_wool", "blue_wool", "brown_wool", "green_wool", "red_wool", "black_wool"}[e->color & 15]); }
        if (e->fire > 0) {   // cooked drops
            int c = smelting_result(item, SMELT_SMOKER);
            if (c) item = c;
        }
        if (n > 0) item_drop_scatter(v3_add(e->pos, v3(0, 0.5f, 0)), stack_of(item, n));
    }
    if (e->mob == MOB_DROWNED && e->item.id == I_TRIDENT && frand() < 0.25f) item_drop_scatter(e->pos, stack_of(I_TRIDENT, 1));
    if ((e->mob == MOB_CREEPER) && who && who->kind == ENT_ARROW) {}
    if (e->mob == MOB_CREEPER && killer && killer->kind == ENT_ARROW) {
        Entity *sh = entity_get(killer->owner);
        if (sh && sh->kind == ENT_MOB && (sh->mob == MOB_SKELETON || sh->mob == MOB_STRAY || sh->mob == MOB_BOGGED)) item_drop_scatter(e->pos, stack_of(I_MUSIC_DISC_13 + irand(0, 1) * 0, 1));
    }
    if (by_player || d->ai == AIK_SLIME) {
        int xp = irand(d->xp_min, d->xp_max);
        if (e->mob == MOB_ENDER_DRAGON) xp = g_world.dragon_killed ? 500 : 12000;
        if (e->mob == MOB_COSMIC_TYRANT) xp = g_world.tyrant_killed ? 1000 : 15000;
        if (xp) xp_drop(e->pos, xp);
    }
    if (by_player) g_player.kills++;
    if (e->mob == MOB_ENDER_DRAGON) { activate_exit_portal(); g_world.dragon_killed = true; }
    if (e->mob == MOB_COSMIC_TYRANT) tyrant_defeated(e);
    if (e->mob == MOB_THALOR_AVATAR) {   // Thalor's trial passed
        Entity *p = player_ent();
        if (p) effect_add(p, EFF_BLESSING_OF_VIGOR, 1, 20 * 60 * TICKRATE);
        game_message(rgba_u32(255, 236, 170, 255), "Thalor the Stoneheart: You did not break. Neither will what you build.");
        title_show("Trial of Stone", "Thalor grants the Blessing of Vigor");
        sift_trial_done(4);
    }
}

// ======================================================================== interaction
bool mob_interact(Entity *m, ItemStack *held) {
    if (MOB_DEFS[m->mob].ai == AIK_SOUL || MOB_DEFS[m->mob].ai == AIK_GOD) return sift_interact(m);
    if ((m->mob == MOB_VILLAGER || m->mob == MOB_WANDERING_TRADER || m->mob == MOB_DELVARI_MERCHANT) && !m->baby && held->id != I_NAME_TAG && !(item_valid(held->id) && ITEMS[held->id].use == USE_SPAWN_EGG)) {
        int n; villager_offers(m, &n);
        if (m->mob == MOB_VILLAGER && m->rep <= -10) { game_message(COL_WHITE, "The villager refuses to trade with you."); n = 0; }
        if (n == 0) { sound_play_at(SND_CLICK, m->pos, 0.5f, 0.6f); m->head_yaw += 0.6f; return true; }   // shakes its head
        G.trade_with = m->id; G.trade_sel = -1; G.trade_scroll = 0;
        game_open_screen(SCR_TRADE);
        return true;
    }
    const MobDef *d = &MOB_DEFS[m->mob];
    int id = held ? held->id : 0;
    bool creative = g_player.gamemode == GM_CREATIVE;
#define CONSUME() do { if (!creative && held) { if (--held->count == 0) *held = (ItemStack){0}; } } while (0)
    if (id == I_NAME_TAG) { m->persistent = true; CONSUME(); return true; }
    if (id && ITEMS[id].use == USE_SPAWN_EGG && ITEMS[id].use_arg == m->mob) { spawn_baby(m->mob, m->pos); CONSUME(); return true; }
    if (m->mob == MOB_SHEEP && id == I_SHEARS && !m->sheared && !m->baby) {
        m->sheared = true;
        int wool = item_by_key((const char *[]){"white_wool", "orange_wool", "magenta_wool", "light_blue_wool", "yellow_wool", "lime_wool", "pink_wool", "gray_wool", "light_gray_wool", "cyan_wool", "purple_wool", "blue_wool", "brown_wool", "green_wool", "red_wool", "black_wool"}[m->color & 15]);
        item_drop_scatter(v3_add(m->pos, v3(0, 1, 0)), stack_of(wool, irand(1, 3)));
        held->damage++;
        sound_play_at(SND_DIG_WOOL, m->pos, 1, 1.2f);
        return true;
    }
    if (m->mob == MOB_SHEEP && id && ITEMS[id].key && strstr(ITEMS[id].key, "_dye")) {
        static const char *DY[16] = {"white_dye", "orange_dye", "magenta_dye", "light_blue_dye", "yellow_dye", "lime_dye", "pink_dye", "gray_dye", "light_gray_dye", "cyan_dye", "purple_dye", "blue_dye", "brown_dye", "green_dye", "red_dye", "black_dye"};
        for (int i = 0; i < 16; i++) if (!strcmp(ITEMS[id].key, DY[i])) { m->color = (u8)i; CONSUME(); return true; }
    }
    if (m->mob == MOB_MOOSHROOM && id == I_SHEARS && !m->baby) {
        vec3 p = m->pos; f32 y = m->yaw; m->remove = true;
        Entity *c = mob_spawn(MOB_COW, p); if (c) c->yaw = c->body_yaw = y;
        item_drop_scatter(v3_add(p, v3(0, 1, 0)), stack_of(I_RED_MUSHROOM, 5));
        return true;
    }
    if (m->mob == MOB_MOOSHROOM && id == I_BOWL && !m->baby) { CONSUME(); player_give(stack_of(I_MUSHROOM_STEW, 1)); return true; }
    if (mob_has(m->mob, MF_MILKABLE) && id == I_BUCKET && !m->baby) { CONSUME(); player_give(stack_of(I_MILK_BUCKET, 1)); sound_play(SND_SPLASH, 0.5f, 1.4f); return true; }
    if (m->mob == MOB_ARMADILLO && id == I_BRUSH) { item_drop_scatter(m->pos, stack_of(I_ARMADILLO_SCUTE, 1)); held->damage += 16; return true; }
    if (m->mob == MOB_PIGLIN && id == I_GOLD_INGOT && !m->anger) {   // bartering
        static const char *LOOT[] = {"ender_pearl", "string", "obsidian", "crying_obsidian", "fire_charge", "leather", "soul_sand", "nether_brick", "spectral_arrow", "gravel", "blackstone", "quartz", "glowstone_dust", "magma_cream", "iron_nugget"};
        static const u8 CNT[] = {4, 9, 1, 3, 1, 4, 8, 6, 12, 16, 16, 8, 8, 4, 18};
        int k = irand(0, (int)ARRAY_LEN(LOOT) - 1);
        CONSUME();
        item_drop(v3_add(m->pos, v3(0, 1, 0)), stack_of(item_by_key(LOOT[k]), irand(1, CNT[k])), v3(frand_range(-1, 1), 3, frand_range(-1, 1)), 20);
        return true;
    }
    if (m->mob == MOB_ZOMBIE_VILLAGER && id == I_GOLDEN_APPLE && effect_has(m, EFF_WEAKNESS)) {
        m->phase = 9; m->special_timer = MC(irand(3600, 6000)); m->persistent = true; CONSUME();
        effect_add(m, EFF_STRENGTH, 0, m->special_timer);
        return true;
    }
    if (m->tamed && !id && (m->mob == MOB_WOLF || m->mob == MOB_CAT || m->mob == MOB_PARROT)) { m->sitting = !m->sitting; return true; }
    // food: tame, breed, heal or speed up growth
    for (int i = 0; i < 7 && d->food[i]; i++) if (d->food[i] == id) {
        if (mob_has(m->mob, MF_TAMEABLE) && !m->tamed) {
            CONSUME();
            if (frand() < (m->mob == MOB_PARROT ? 0.1f : 0.33f)) { m->tamed = true; m->persistent = true; m->owner_id = g_player.eid; fx_hearts(v3_add(m->pos, v3(0, m->height, 0))); if (m->mob == MOB_WOLF) { m->max_health = 40; m->health = 40; } }
            else fx_smoke(v3_add(m->pos, v3(0, m->height, 0)), 6, 0.3f, rgba_u32(20, 20, 20, 255));
            return true;
        }
        if (m->health < m->max_health) { entity_heal(m, (f32)ITEMS[id].food + 1); CONSUME(); return true; }
        if (m->baby) { m->grow_age = (int)(m->grow_age * 0.9f); CONSUME(); return true; }
        if (mob_has(m->mob, MF_BREEDS) && m->love <= 0 && m->breed_cooldown <= 0) { m->love = MC(600); CONSUME(); return true; }
        return false;
    }
#undef CONSUME
    return false;
}

// ======================================================================== golems & the wither
void golem_check_build(ivec3 p) {
    Block top = world_get_block(p.x, p.y, p.z);
    Block k = BLOCKS[top].base;
    if (k == B_CARVED_PUMPKIN || k == B_JACK_O_LANTERN) {
        Block b1 = world_get_block(p.x, p.y - 1, p.z), b2 = world_get_block(p.x, p.y - 2, p.z);
        if (b1 == B_SNOW_BLOCK && b2 == B_SNOW_BLOCK) {
            for (int i = 0; i < 3; i++) world_set_block(p.x, p.y - i, p.z, B_AIR);
            Entity *g = mob_spawn(MOB_SNOW_GOLEM, v3(p.x + 0.5f, p.y - 2.0f, p.z + 0.5f)); if (g) g->persistent = true;
            fx_poof(v3(p.x + 0.5f, p.y - 2.0f, p.z + 0.5f), 1, 2); return;
        }
        if (b1 == B_IRON_BLOCK && b2 == B_IRON_BLOCK) {
            for (int ax = 0; ax < 2; ax++) {
                int dx = ax ? 1 : 0, dz = ax ? 0 : 1;
                if (world_get_block(p.x + dx, p.y - 1, p.z + dz) == B_IRON_BLOCK && world_get_block(p.x - dx, p.y - 1, p.z - dz) == B_IRON_BLOCK) {
                    world_set_block(p.x, p.y, p.z, B_AIR); world_set_block(p.x, p.y - 1, p.z, B_AIR); world_set_block(p.x, p.y - 2, p.z, B_AIR);
                    world_set_block(p.x + dx, p.y - 1, p.z + dz, B_AIR); world_set_block(p.x - dx, p.y - 1, p.z - dz, B_AIR);
                    Entity *g = mob_spawn(MOB_IRON_GOLEM, v3(p.x + 0.5f, p.y - 2.0f, p.z + 0.5f)); if (g) g->persistent = true;
                    fx_poof(v3(p.x + 0.5f, p.y - 2.0f, p.z + 0.5f), 1.4f, 2.7f); return;
                }
            }
        }
        const char *k1 = BLOCKS[b1].key;
        if (k1 && strstr(k1, "copper") && (BLOCKS[b1].mesh == MESH_CUBE) && !strstr(k1, "cut") && !strstr(k1, "chiseled") && !strstr(k1, "grate") && !strstr(k1, "bulb") && !strstr(k1, "ore")) {
            world_set_block(p.x, p.y, p.z, B_AIR); world_set_block(p.x, p.y - 1, p.z, B_AIR);
            Entity *g = mob_spawn(MOB_COPPER_GOLEM, v3(p.x + 0.5f, p.y - 1.0f, p.z + 0.5f)); if (g) g->persistent = true;
            return;
        }
    }
    if (k == B_WITHER_SKELETON_SKULL) {
        // the placed skull may be any of the three: try every alignment
        for (int ax = 0; ax < 2; ax++) for (int off = -2; off <= 0; off++) {
            int dx = ax ? 1 : 0, dz = ax ? 0 : 1;
            int cx = p.x + dx * (off + 1), cz = p.z + dz * (off + 1);   // centre skull
            bool ok = true;
            for (int i = -1; i <= 1 && ok; i++) ok = world_get_block(cx + dx * i, p.y, cz + dz * i) == B_WITHER_SKELETON_SKULL;
            for (int i = -1; i <= 1 && ok; i++) { Block s = world_get_block(cx + dx * i, p.y - 1, cz + dz * i); ok = s == B_SOUL_SAND || s == B_SOUL_SOIL; }
            Block st = world_get_block(cx, p.y - 2, cz);
            ok = ok && (st == B_SOUL_SAND || st == B_SOUL_SOIL);
            if (!ok) continue;
            for (int i = -1; i <= 1; i++) { world_set_block(cx + dx * i, p.y, cz + dz * i, B_AIR); world_set_block(cx + dx * i, p.y - 1, cz + dz * i, B_AIR); }
            world_set_block(cx, p.y - 2, cz, B_AIR);
            Entity *w = mob_spawn(MOB_WITHER, v3(cx + 0.5f, p.y - 2.0f, cz + 0.5f));
            if (w) { w->persistent = true; sound_play(SND_EXPLODE, 1, 0.5f); }
            return;
        }
    }
}

// ======================================================================== natural spawning
static int pick_spawn(int biome, int cat, const SpawnEntry **out) {
    int total = 0;
    for (int i = 0; i < SPAWN_COUNT; i++) if (SPAWNS[i].biome == biome && SPAWNS[i].cat == cat) total += SPAWNS[i].weight;
    if (!total) return 0;
    int r = irand(0, total - 1);
    for (int i = 0; i < SPAWN_COUNT; i++) if (SPAWNS[i].biome == biome && SPAWNS[i].cat == cat) {
        r -= SPAWNS[i].weight;
        if (r < 0) { *out = &SPAWNS[i]; return 1; }
    }
    return 0;
}
static bool space_for(int x, int y, int z, f32 h) {
    for (int i = 0; i < (int)ceilf(h); i++) { Block b = world_get_block(x, y + i, z); if (BLOCKS[b].solid || BLOCKS[b].liquid) return false; }
    return true;
}
static int spawn_group(const SpawnEntry *s, int x, int y, int z, bool water) {
    int n = irand(s->min, s->max), made = 0;
    for (int i = 0; i < n; i++) {
        int gx = x + irand(-3, 3), gz = z + irand(-3, 3), gy = y;
        if (water) { if (!block_is_water(world_get_block(gx, gy, gz))) continue; }
        else {
            bool found = false;
            for (int dy = 2; dy >= -2 && !found; dy--) {
                Block below = world_get_block(gx, gy + dy - 1, gz);
                if (BLOCKS[below].solid && !block_is_leaves(below) && space_for(gx, gy + dy, gz, MOB_DEFS[s->mob].height)) { gy += dy; found = true; }
            }
            if (!found) continue;
        }
        Entity *e = mob_spawn(s->mob, v3(gx + 0.5f, (f32)gy + (water ? 0.2f : 0), gz + 0.5f));
        if (e) made++;
    }
    return made;
}

void spawn_animals_for_column(int cx, int cz) {
    if (frand() > 0.12f) return;
    int x = cx * 16 + irand(2, 13), z = cz * 16 + irand(2, 13);
    int y = world_top_y(x, z);
    if (y < 0) return;
    int b = world_biome_at(x, y + 1, z);
    const SpawnEntry *s;
    Block ground = world_get_block(x, y, z);
    if (block_is_water(ground) || BLOCKS[ground].liquid) {
        if (pick_spawn(b, frand() < 0.5f ? SPAWN_WATER_AMBIENT : SPAWN_WATER, &s)) spawn_group(s, x, y, z, true);
        return;
    }
    if (g_world.dim == DIM_OVERWORLD && pick_spawn(b, SPAWN_CREATURE, &s)) {
        int made = spawn_group(s, x, y + 1, z, false);
        (void)made;
        ENT_FOREACH(e) if (e->kind == ENT_MOB && e->mob == s->mob && v3_dist(e->pos, v3(x, y, z)) < 8 && e->age == 0) e->persistent = true;
    }
}

void mobs_spawn_tick(void) {
    Entity *p = player_ent();
    if (!p || g_player.dead) return;
    static int t;
    if (++t % 20) return;
    if (g_level.difficulty == DIFF_PEACEFUL) ENT_FOREACH(m) if (m->kind == ENT_MOB && mob_hostile(m->mob) && m->mob != MOB_ENDER_DRAGON && m->mob != MOB_COSMIC_TYRANT && !m->tamed) m->remove = true;   // Minecraft removes every monster on Peaceful
    if (!rule(GR_DO_MOB_SPAWNING)) return;
    int hostile = 0, creature = 0, water = 0, ambient = 0;
    ENT_FOREACH(e) if (e->kind == ENT_MOB && e->death_time == 0) {
        int c = mob_hostile(e->mob) ? 0 : MOB_DEFS[e->mob].ai == AIK_FISH || MOB_DEFS[e->mob].ai == AIK_SQUID ? 2 : e->mob == MOB_BAT ? 3 : 1;
        if (c == 0) hostile++; else if (c == 1) creature++; else if (c == 2) water++; else ambient++;
    }
    g_ents.count_mobs_hostile = hostile; g_ents.count_mobs_passive = creature; g_ents.count_water = water; g_ents.count_ambient = ambient;
    for (int attempt = 0; attempt < 3; attempt++) {
        f32 ang = frand() * 2 * PI_F, dist = frand_range(24, 64);
        int x = (int)floorf(p->pos.x + cosf(ang) * dist), z = (int)floorf(p->pos.z + sinf(ang) * dist);
        if (!world_loaded(x, z)) continue;
        int top = world_top_y(x, z);
        if (top < 0) continue;
        int cat = irand(0, 9);
        const SpawnEntry *s;
        if (cat < 6 && hostile < 45) {   // monsters: dark places
            int y = g_world.dim == DIM_NETHER ? irand(NETHER_LAVA_LEVEL + 1, 115) : irand(5, top + 1);
            if (frand() < 0.4f) y = top + 1;
            int b = world_biome_at(x, y, z);
            static SpawnEntry st_entry;
            int st = structure_at(x, y, z), sm = st >= 0 ? structure_spawn_mob(st, false) : -1;
            if (sm >= 0) { st_entry.mob = (u8)sm; st_entry.weight = 1; st_entry.min = 1; st_entry.max = sm == MOB_BLAZE || sm == MOB_PILLAGER ? 2 : 1; s = &st_entry; }
            else if (!pick_spawn(b, SPAWN_MONSTER, &s)) continue;
            // find a floor near y
            int fy = -1;
            for (int dy = 0; dy < 16; dy++) { Block below = world_get_block(x, y - dy - 1, z); if (BLOCKS[below].solid && !block_is_leaves(below) && space_for(x, y - dy, z, MOB_DEFS[s->mob].height)) { fy = y - dy; break; } }
            if (fy < 0 && !mob_has(s->mob, MF_FLYING)) continue;
            if (fy < 0) fy = y;
            u16 l = world_get_light(x, fy, z);
            int blk = MAXI(L_R(l), MAXI(L_G(l), L_B(l)));
            int sky = g_world.dim == DIM_OVERWORLD ? (int)(L_SKY(l) * world_daylight()) : 0;
            int limit = g_world.dim == DIM_NETHER ? 11 : 0;
            if (s->mob == MOB_SLIME) {   // swamps at night or slime chunks deep down
                bool slime_chunk = (hash3i(g_world.seed, x >> 4, 987, z >> 4) % 10) == 0 && fy < 40;
                if (!slime_chunk && !(b == BIOME_SWAMP && world_is_night())) continue;
            } else if (blk > limit || (sky > 7 && s->mob != MOB_DROWNED && s->mob != MOB_GUARDIAN && s->mob != MOB_ELDER_GUARDIAN)) continue;
            if (s->mob == MOB_DROWNED || s->mob == MOB_GUARDIAN || s->mob == MOB_ELDER_GUARDIAN || s->mob == MOB_ZOMBIE_NAUTILUS) {
                if (!block_is_water(world_get_block(x, fy, z))) continue;
                if (s->mob == MOB_GUARDIAN || s->mob == MOB_ELDER_GUARDIAN) { if (frand() > 0.3f) continue; }
            }
            if ((s->mob == MOB_GHAST || s->mob == MOB_BLAZE) && g_world.dim != DIM_NETHER) continue;
            if (s->mob == MOB_WARDEN) { bool any = false; ENT_FOREACH(o) if (o->kind == ENT_MOB && o->mob == MOB_WARDEN) any = true; if (any) continue; }
            if (s->mob == MOB_CREAKING && !world_is_night()) continue;
            spawn_group(s, x, fy, z, block_is_water(world_get_block(x, fy, z)));
        } else if (cat < 7 && creature < 12 && g_world.dim == DIM_OVERWORLD) {
            int b = world_biome_at(x, top + 1, z);
            if (!pick_spawn(b, SPAWN_CREATURE, &s)) continue;
            Block g = world_get_block(x, top, z);
            if (BLOCKS[g].base != B_GRASS_BLOCK && s->mob != MOB_RABBIT && s->mob != MOB_CAMEL && s->mob != MOB_MOOSHROOM && s->mob != MOB_TURTLE && s->mob != MOB_GOAT && s->mob != MOB_POLAR_BEAR && s->mob != MOB_FROG && s->mob != MOB_ARMADILLO) continue;
            if (world_light_level(iv3(x, top + 1, z)) < 9) continue;
            spawn_group(s, x, top + 1, z, false);
        } else if (cat < 9 && water < 14) {
            int y = irand(SEA_LEVEL - 12, SEA_LEVEL - 1);
            if (!block_is_water(world_get_block(x, y, z))) continue;
            int b = world_biome_at(x, y, z);
            int c = y < top - 6 ? (BIOMES[world_biome_at(x, y, z)].category == 4 && world_biome_at(x, y, z) == BIOME_LUSH_CAVES ? SPAWN_AXOLOTL : SPAWN_UNDERGROUND_WATER) : (frand() < 0.6f ? SPAWN_WATER_AMBIENT : SPAWN_WATER);
            if (!pick_spawn(b, c, &s)) continue;
            spawn_group(s, x, y, z, true);
        } else if (ambient < 8 && g_world.dim == DIM_OVERWORLD) {
            int y = irand(5, SEA_LEVEL - 4);
            if (world_light_level(iv3(x, y, z)) > 3 || BLOCKS[world_get_block(x, y, z)].solid) continue;
            Entity *e = mob_spawn(MOB_BAT, v3(x + 0.5f, y + 0.5f, z + 0.5f)); (void)e;
        }
        // striders on lava, phantoms over sleepless players
        if (g_world.dim == DIM_NETHER && frand() < 0.2f) {
            int y = NETHER_LAVA_LEVEL + 1;
            if (block_is_lava(world_get_block(x, NETHER_LAVA_LEVEL, z)) && block_is_air(world_get_block(x, y, z)) && creature < 10) mob_spawn(MOB_STRIDER, v3(x + 0.5f, (f32)y, z + 0.5f));
        }
    }
    if (g_world.dim == DIM_OVERWORLD && world_is_night() && g_player.rest_ticks > DAY_TICKS * 3 && frand() < 0.01f && hostile < 45) {
        int top = world_top_y((int)p->pos.x, (int)p->pos.z);
        if (top >= 0 && p->pos.y >= top - 1) for (int i = 0; i < irand(1, 3); i++) {
            Entity *ph = mob_spawn(MOB_PHANTOM, v3_add(p->pos, v3(frand_range(-10, 10), 22, frand_range(-10, 10))));
            if (ph) ph->anchor = p->pos;
        }
    }
}

// Sculk: vibrations, sculk sensors (power by distance, wool dampening, calibrated frequency filter),
// shriekers (warning levels -> Darkness -> a warden emerges, gamerule doWardenSpawning) and catalysts that
// spread sculk when mobs die nearby.
//  * sculk_vibration(pos, freq, by_player): every listening sensor within 8 blocks with no wool on the straight
//    line hears it and turns on with power 15 - floor(distance * 15 / 8) (min 1) for 1.5 s. Calibrated sensors
//    only hear the frequency equal to the redstone power fed into them (any frequency when unpowered).
//    Frequencies follow Minecraft's spirit: 1 step, 2 break, 3 place, 15 explosion.
//  * player vibrations also wake shriekers within 8 blocks (10 s cooldown): Darkness for everyone near, and the
//    warning level climbs; at 4 a natural shrieker (variant 1) calls a warden up out of the ground.
//    The level drops by one every 10 minutes and is saved in level.txt.
//  * sculk_on_mob_death(): a catalyst within 8 blocks spreads sculk over the ground where the mob died
//    (more for mobs worth more XP), sometimes growing a sensor or a (non-summoning) shrieker.
#include "world.h"
#include "rules.h"
#include "../entity/entity.h"
#include "../game/game.h"
#include "../game/player.h"
#include "../game/fx.h"
#include "../audio/audio.h"
#define MC(t) ((t) * TICKRATE / 20)

typedef struct { ivec3 p; int t; } ActiveSensor;
static ActiveSensor act[128];
static int nact, shriek_cd, decay_t;
int g_vibrations, g_sensor_triggers, g_shrieks, g_wardens_summoned, g_sculk_spread;

static bool is_sensor(u16 b) { int k = BLOCKS[b].base; return k == B_SCULK_SENSOR || k == B_CALIBRATED_SCULK_SENSOR; }
static bool is_wool(u16 b) { const char *k = BLOCKS[b].key; size_t n = k ? strlen(k) : 0; return n > 5 && !strcmp(k + n - 5, "_wool"); }
static bool wool_between(vec3 a, vec3 b) {   // wool absorbs vibrations travelling through it
    f32 len = v3_dist(a, b);
    for (f32 t = 0.5f; t < len - 0.5f; t += 0.25f) {
        ivec3 q = v3_floor(v3_lerp(a, b, t / len));
        if (is_wool(world_get_block(q.x, q.y, q.z))) return true;
    }
    return false;
}
static void summon_warden(ivec3 s) {
    for (int tries = 0; tries < 40; tries++) {
        int x = s.x + irand(-5, 5), z = s.z + irand(-5, 5);
        for (int y = s.y + 4; y >= s.y - 4; y--) {
            if (!BLOCKS[world_get_block(x, y - 1, z)].solid) continue;
            bool room = true;
            for (int h = 0; h < 3 && room; h++) room = !BLOCKS[world_get_block(x, y + h, z)].solid;
            if (!room) continue;
            Entity *w = mob_spawn(MOB_WARDEN, v3(x + 0.5f, (f32)y, z + 0.5f));
            if (w) {
                Entity *p = player_ent();
                if (p) { w->target = p->id; w->anger = MC(400); }
                for (int i = 0; i < 20; i++) fx_smoke(v3(x + 0.5f + frand_range(-1, 1), (f32)y, z + 0.5f + frand_range(-1, 1)), 1, 0.2f, rgba_u32(20, 60, 70, 255));
                g_wardens_summoned++;
                game_message(rgba_u32(80, 220, 230, 255), "A warden emerges from the deep dark!");
            }
            return;
        }
    }
}
static void shriek(ivec3 s, bool can_summon) {
    g_shrieks++;
    shriek_cd = MC(200);
    sound_play_at(SND_HELL_WAIL, iv3_center(s), 1.0f, 0.7f);
    for (int i = 0; i < 12; i++) fx_smoke(v3_add(iv3_center(s), v3(0, 0.5f + i * 0.4f, 0)), 1, 0.05f, rgba_u32(60, 200, 210, 255));
    ENT_FOREACH(o) if (o->kind == ENT_PLAYER && v3_dist(o->pos, iv3_center(s)) < 40) effect_add(o, EFF_DARKNESS, 0, MC(240));
    if (!can_summon || g_level.difficulty == DIFF_PEACEFUL) return;
    g_world.warden_warning = MINI(4, g_world.warden_warning + 1);
    static const char *W[] = {"", "A sculk shrieker screams.", "The deep dark stirs...", "Something is coming closer.", ""};
    if (g_world.warden_warning < 4) { snprintf(G.actionbar, sizeof G.actionbar, "%s", W[g_world.warden_warning]); G.actionbar_t = 80; return; }
    if (!rule(GR_DO_WARDEN_SPAWNING)) return;
    bool have = false;
    ENT_FOREACH(o) if (o->kind == ENT_MOB && o->mob == MOB_WARDEN && v3_dist(o->pos, iv3_center(s)) < 48) have = true;
    if (!have) summon_warden(s);
    g_world.warden_warning = 0;
}
void sculk_vibration(vec3 pos, int freq, bool by_player) {
    g_vibrations++;
    ivec3 c = v3_floor(pos);
    for (int dy = -8; dy <= 8; dy++) for (int dz = -8; dz <= 8; dz++) for (int dx = -8; dx <= 8; dx++) {
        ivec3 q = iv3(c.x + dx, c.y + dy, c.z + dz);
        u16 b = world_get_block(q.x, q.y, q.z);
        if (!is_sensor(b) || BLOCKS[b].var != 0) continue;
        vec3 qc = iv3_center(q);
        f32 d = v3_dist(qc, pos);
        if (d > 8.5f) continue;
        if (BLOCKS[b].base == B_CALIBRATED_SCULK_SENSOR) { int want = rs_input_power(q); if (want > 0 && want != freq) continue; }
        if (wool_between(pos, qc)) continue;
        if (nact >= (int)ARRAY_LEN(act)) continue;
        int power = clampi(15 - (int)floorf(d * 15 / 8), 1, 15);
        world_set_block(q.x, q.y, q.z, (u16)(BLOCKS[b].base + power));
        act[nact++] = (ActiveSensor){q, MC(30)};
        g_sensor_triggers++;
        if (by_player) progress_grant("sculk");
        sound_play_at(SND_CLICK, qc, 0.6f, 1.8f);
        fx_smoke(v3_add(qc, v3(0, 0.6f, 0)), 2, 0.05f, rgba_u32(60, 200, 210, 255));
    }
    if (!by_player || shriek_cd > 0) return;
    for (int dy = -8; dy <= 8; dy++) for (int dz = -8; dz <= 8; dz++) for (int dx = -8; dx <= 8; dx++) {
        u16 b = world_get_block(c.x + dx, c.y + dy, c.z + dz);
        if (BLOCKS[b].base != B_SCULK_SHRIEKER) continue;
        shriek(iv3(c.x + dx, c.y + dy, c.z + dz), BLOCKS[b].var == 1);
        return;
    }
}
void sculk_tick(void) {
    if (shriek_cd > 0) shriek_cd--;
    if (++decay_t >= MC(12000)) { decay_t = 0; if (g_world.warden_warning > 0) g_world.warden_warning--; }
    for (int i = 0; i < nact; i++) {
        if (--act[i].t > 0) continue;
        ivec3 q = act[i].p;
        u16 b = world_get_block(q.x, q.y, q.z);
        if (is_sensor(b) && BLOCKS[b].var) world_set_block(q.x, q.y, q.z, BLOCKS[b].base);
        act[i--] = act[--nact];
    }
}
void sculk_on_mob_death(Entity *e) {
    if (e->kind != ENT_MOB) return;
    ivec3 c = v3_floor(e->pos), cat = {0, -1000, 0};
    for (int dy = -8; dy <= 8 && cat.y == -1000; dy++) for (int dz = -8; dz <= 8 && cat.y == -1000; dz++) for (int dx = -8; dx <= 8; dx++)
        if (world_get_block(c.x + dx, c.y + dy, c.z + dz) == B_SCULK_CATALYST) { cat = iv3(c.x + dx, c.y + dy, c.z + dz); break; }
    if (cat.y == -1000) return;
    const MobDef *d = &MOB_DEFS[e->mob];
    int charge = clampi(d->xp_max * 2, 2, 40);
    for (int i = 0; i < charge * 3 && charge > 0; i++) {   // creep over the ground around the death
        int x = c.x + irand(-3, 3), z = c.z + irand(-3, 3);
        for (int y = c.y + 2; y >= c.y - 3; y--) {
            u16 b = world_get_block(x, y, z);
            if (!BLOCKS[b].solid || !BLOCKS[b].opaque || BLOCKS[b].hardness < 0 || b == B_SCULK || BLOCKS[b].base == B_SCULK_CATALYST) { if (b == B_SCULK) break; continue; }
            if (BLOCKS[world_get_block(x, y + 1, z)].solid) break;
            world_set_block(x, y, z, B_SCULK);
            g_sculk_spread++; charge--;
            if (charge > 6 && irand(0, 30) == 0) world_set_block(x, y + 1, z, irand(0, 3) ? B_SCULK_SENSOR : B_SCULK_SHRIEKER);
            break;
        }
    }
    for (int i = 0; i < 10; i++) fx_smoke(v3_add(iv3_center(cat), v3(frand_range(-0.5f, 0.5f), 0.6f, frand_range(-0.5f, 0.5f))), 1, 0.08f, rgba_u32(60, 200, 210, 255));
}

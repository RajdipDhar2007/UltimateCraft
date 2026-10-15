// Weather follows Minecraft's ServerLevel.advanceWeatherCycle: independent rain and thunder timers
// (clear 0.5-7.5 days, rain 0.5-1 day, thunder 3-13 min), /weather clear holds clear weather for a
// set time. Lightning strikes loaded chunks during thunderstorms, sets fires, charges creepers and
// converts pigs, villagers and mooshrooms. Rain puts out burning entities and fire; snow piles up
// and water freezes in cold biomes.
#include "weather.h"
int g_thunder_pending; f32 g_thunder_vol;
#include "game.h"
#include "player.h"
#include "fx.h"
#include "../world/rules.h"
#include "../entity/entity.h"
#include "../audio/audio.h"

static inline int mc_rand(int lo, int hi) { return irand(lo, hi); }

void weather_set(int kind, int dur) {
    if (kind == 0) {
        g_level.clear_time = dur > 0 ? dur : mc_rand(12000, 180000);
        g_level.rain_time = 0; g_level.thunder_time = 0;
        g_level.raining = false; g_level.thundering = false;
    } else {
        int d = dur > 0 ? dur : kind == 2 ? mc_rand(3600, 15600) : mc_rand(12000, 24000);
        g_level.clear_time = 0;
        g_level.rain_time = d; g_level.raining = true;
        g_level.thunder_time = d; g_level.thundering = kind == 2;
    }
}
void weather_sleep_reset(void) {
    if (!rule(GR_DO_WEATHER_CYCLE)) return;
    g_level.rain_time = 0; g_level.raining = false;
    g_level.thunder_time = 0; g_level.thundering = false;
}

int weather_precip_at(int x, int y, int z) {
    if (g_world.dim != DIM_OVERWORLD || g_level.rain_level < 0.2f) return 0;
    Column *c = world_column_at(x, z);
    if (!c || y < c->heightmap[z & 15][x & 15]) return 0;   // something above blocks the sky
    const BiomeDef *b = &BIOMES[c->biome[z & 15][x & 15]];
    if (!b->precip) return 0;
    // Minecraft: it snows where the biome temperature, lowered by 0.00125 per block above y 80, is below 0.15
    f32 t = b->temperature - fmaxf(0, (f32)(y - MCY(80))) * 0.00125f;
    return (b->precip == 2 || t < 0.15f) ? 2 : 1;
}
bool weather_raining_at(int x, int y, int z) { return weather_precip_at(x, y, z) == 1; }
f32 weather_sky_dim(void) { return g_world.dim == DIM_OVERWORLD ? g_level.rain_level * 0.3125f + g_level.thunder_level * 0.3125f * g_level.rain_level : 0; }

// ------------------------------------------------------------------ lightning bolts (visual)
typedef struct { vec3 pos; int life; u32 seed; } Bolt;
static Bolt bolts[8];

void lightning_strike(vec3 pos, bool visual_only) {
    for (int i = 0; i < 8; i++) if (bolts[i].life <= 0) { bolts[i] = (Bolt){pos, TICKRATE / 2, (u32)irand(1, 1 << 30)}; break; }
    g_level.lightning_flash = TICKRATE / 3;
    Entity *pl = player_ent();
    f32 d = pl ? v3_dist(pl->pos, pos) : 999;
    sound_play_at(SND_EXPLODE, pos, clampf(3.0f - d / 60.0f, 0.4f, 2.0f), 0.45f);
    g_thunder_pending = (int)(d / 340.0f * TICKRATE) + 1; g_thunder_vol = clampf(1.6f - d / 160.0f, 0.3f, 1.2f);   // sound travels ~340 m/s
    if (visual_only) return;
    ivec3 b = v3_floor(pos);
    // fire at the strike point (Minecraft: normal / hard difficulty, doFireTick)
    if (rule(GR_DO_FIRE_TICK) && g_level.difficulty >= DIFF_NORMAL && block_is_air(world_get_block(b.x, b.y, b.z)) && BLOCKS[world_get_block(b.x, b.y - 1, b.z)].solid)
        world_set_block(b.x, b.y, b.z, B_FIRE);
    ENT_FOREACH(e) {
        if (e->kind != ENT_MOB && e->kind != ENT_PLAYER && e->kind != ENT_ITEM) continue;
        if (v3_dist(e->pos, pos) > 3.0f) continue;
        if (e->kind == ENT_MOB) {
            if (e->mob == MOB_CREEPER) { e->phase = 1; e->persistent = true; continue; }
            int to = -1;
            if (e->mob == MOB_PIG) to = MOB_ZOMBIFIED_PIGLIN;
            else if (e->mob == MOB_VILLAGER && g_level.difficulty != DIFF_PEACEFUL) to = MOB_WITCH;
            if (to >= 0) {
                vec3 p = e->pos; f32 yaw = e->yaw;
                e->remove = true;
                Entity *n = mob_spawn(to, p);
                if (n) { n->yaw = n->body_yaw = yaw; n->persistent = true; }
                continue;
            }
            if (e->mob == MOB_MOOSHROOM) { e->color ^= 1; continue; }   // red <-> brown
        }
        if (e->kind == ENT_ITEM) { e->remove = true; continue; }
        e->invuln = 0;
        entity_damage(e, 5, DMG_FIRE, NULL);
        e->fire = MAXI(e->fire, 8 * TICKRATE);
    }
}

// ------------------------------------------------------------------ simulation
static void weather_cycle(void) {
    if (!rule(GR_DO_WEATHER_CYCLE)) return;
    if (g_level.clear_time > 0) {
        g_level.clear_time--;
        g_level.thunder_time = g_level.thundering ? 0 : 1;
        g_level.rain_time = g_level.raining ? 0 : 1;
        g_level.thundering = false; g_level.raining = false;
        return;
    }
    if (g_level.thunder_time > 0) { if (--g_level.thunder_time == 0) g_level.thundering = !g_level.thundering; }
    else g_level.thunder_time = g_level.thundering ? mc_rand(3600, 15600) : mc_rand(12000, 180000);
    if (g_level.rain_time > 0) { if (--g_level.rain_time == 0) g_level.raining = !g_level.raining; }
    else g_level.rain_time = g_level.raining ? mc_rand(12000, 24000) : mc_rand(12000, 180000);
}

static void precipitation_effects(void) {
    Entity *pl = player_ent();
    if (!pl || g_world.dim != DIM_OVERWORLD) return;
    // Minecraft ticks one random surface block per chunk every 16 game ticks for snow and ice
    int px = (int)floorf(pl->pos.x), pz = (int)floorf(pl->pos.z);
    for (int dcz = -6; dcz <= 6; dcz++) for (int dcx = -6; dcx <= 6; dcx++) {
        if (irand(0, 15)) continue;
        int x = ((px >> 4) + dcx) * 16 + irand(0, 15), z = ((pz >> 4) + dcz) * 16 + irand(0, 15);
        Column *c = world_column_at(x, z);
        if (!c) continue;
        int y = c->heightmap[z & 15][x & 15];   // first block open to the sky
        if (y <= 1 || y >= WORLD_H - 1) continue;
        u16 below = world_get_block(x, y - 1, z), here = world_get_block(x, y, z);
        const BiomeDef *bd = &BIOMES[c->biome[z & 15][x & 15]];
        f32 temp = bd->temperature - fmaxf(0, (f32)(y - MCY(80))) * 0.00125f;
        bool cold = bd->precip && temp < 0.15f;
        if (cold && below == B_WATER && world_light_level(iv3(x, y, z)) < 12) { world_set_block(x, y - 1, z, B_ICE); continue; }
        int pr = weather_precip_at(x, y, z);
        if (pr == 2 && rule(GR_SNOW_ACCUMULATION_HEIGHT) > 0) {
            if (block_is_air(here) && BLOCKS[below].solid && BLOCKS[below].opaque && below != B_ICE && below != B_PACKED_ICE) world_set_block(x, y, z, B_SNOW);
            else if (BLOCKS[here].base == B_SNOW && BLOCKS[here].var + 1 < rule_i(GR_SNOW_ACCUMULATION_HEIGHT) && BLOCKS[here].var + 1 < BLOCKS[here].nvar) world_set_block(x, y, z, here + 1);
        }
        if (pr == 1 && (here == B_FIRE)) world_set_block(x, y, z, B_AIR);   // rain puts out exposed fire
    }
    // entities standing in the rain stop burning
    ENT_FOREACH(e) {
        if (e->fire <= 0 || (e->kind != ENT_MOB && e->kind != ENT_PLAYER)) continue;
        if (weather_raining_at((int)floorf(e->pos.x), (int)floorf(e->pos.y + e->height), (int)floorf(e->pos.z))) e->fire = 0;
    }
}

static void lightning_tick(void) {
    if (g_world.dim != DIM_OVERWORLD || g_level.thunder_level < 0.9f || g_level.rain_level < 0.2f) return;
    // Minecraft: each loaded chunk has a 1 in 100000 chance per game tick during a thunderstorm
    int n = 0;
    Column *pick = NULL;
    for (int i = 0; i < COL_SLOTS * COL_SLOTS; i++) {
        Column *c = g_world.slots[i];
        if (!c || c->state != COL_READY) continue;
        if (irand(0, 99999) == 0 && irand(0, n) == 0) pick = c;
        n++;
    }
    if (!pick) return;
    int x = pick->cx * 16 + irand(0, 15), z = pick->cz * 16 + irand(0, 15);
    // strike a nearby entity that is out in the rain (lightning prefers living targets)
    ENT_FOREACH(e) if (e->kind == ENT_MOB && fabsf(e->pos.x - x) < 16 && fabsf(e->pos.z - z) < 16 && weather_precip_at((int)e->pos.x, (int)e->pos.y + 1, (int)e->pos.z)) { x = (int)floorf(e->pos.x); z = (int)floorf(e->pos.z); break; }
    int y = pick->heightmap[z & 15][x & 15];
    // lightning rods within 128 blocks attract the bolt
    if (weather_precip_at(x, y, z) != 1) return;   // only where it rains
    lightning_strike(v3(x + 0.5f, (f32)y, z + 0.5f), false);
}

void weather_tick(void) {
    if (g_level.lightning_flash > 0) g_level.lightning_flash--;
    for (int i = 0; i < 8; i++) if (bolts[i].life > 0) bolts[i].life--;
    if (G.ticks % MC_TICK_DIV) return;   // the rest runs at 20 Minecraft ticks per second
    weather_cycle();
    g_level.prev_rain_level = g_level.rain_level; g_level.prev_thunder_level = g_level.thunder_level;
    g_level.rain_level = clampf(g_level.rain_level + (g_level.raining ? 0.01f : -0.01f), 0, 1);
    g_level.thunder_level = clampf(g_level.thunder_level + (g_level.thundering && g_level.raining ? 0.01f : -0.01f), 0, 1);
    precipitation_effects();
    lightning_tick();
}

// ------------------------------------------------------------------ rendering
void weather_render(const mat4 *vp, vec3 cam, vec4 fog, f32 fog_near, f32 fog_far) {
    bool any_bolt = false;
    for (int i = 0; i < 8; i++) any_bolt |= bolts[i].life > 0;
    if (g_world.dim != DIM_OVERWORLD || (g_level.rain_level < 0.05f && !any_bolt)) return;
    b3d_begin(vp, fog, fog_near, fog_far);
    b3d_set_texture(gfx_white_texture());
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    f32 t = (f32)(g_world.time % 100000) / TICKRATE;
    int cx = (int)floorf(cam.x), cz = (int)floorf(cam.z), R = 12;
    f32 a = g_level.rain_level;
    for (int dz = -R; dz <= R; dz++) for (int dx = -R; dx <= R; dx++) {
        if (dx * dx + dz * dz > R * R) continue;
        int x = cx + dx, z = cz + dz;
        Column *c = world_column_at(x, z);
        if (!c) continue;
        const BiomeDef *bd = &BIOMES[c->biome[z & 15][x & 15]];
        if (!bd->precip) continue;
        int ground = c->heightmap[z & 15][x & 15];
        f32 y0 = fmaxf((f32)ground, cam.y - 10), y1 = cam.y + 12;
        if (y1 <= y0) continue;
        bool snow = weather_precip_at(x, ground, z) == 2;
        u32 h = (u32)hash3i(77, x, 0, z);
        f32 off = (h & 1023) / 1023.0f;
        // a vertical sheet across the cell, turned toward the camera
        vec3 to = v3_norm(v3(x + 0.5f - cam.x, 0, z + 0.5f - cam.z));
        vec3 side = v3(-to.z * 0.5f, 0, to.x * 0.5f);
        vec3 ctr = v3(x + 0.5f, 0, z + 0.5f);
        int drops = snow ? 3 : 4;
        for (int k = 0; k < drops; k++) {
            f32 ph = off + k * 0.37f;
            f32 speed = snow ? 1.5f : 14.0f, len = snow ? 0.08f : 0.9f;
            f32 span = y1 - y0;
            f32 yy = y1 - fmodf(t * speed + ph * span * 3.1f, span);
            f32 lx = (fmodf(ph * 7.3f, 1.0f) - 0.5f);
            vec3 p = v3_add(ctr, v3_scale(side, lx * 1.6f));
            if (snow) p = v3_add(p, v3(sinf(t + ph * 9) * 0.3f, 0, cosf(t * 0.8f + ph * 5) * 0.3f));
            vec3 w = v3_scale(side, snow ? 0.12f : 0.03f);
            u32 col = snow ? rgba_f(1, 1, 1, 0.85f * a) : rgba_f(0.55f, 0.65f, 0.9f, 0.45f * a);
            vec3 bot = v3(p.x, yy - len, p.z), top = v3(p.x, yy, p.z);
            if (snow) b3d_quad(v3_sub(bot, w), v3_add(bot, w), v3_add(v3(top.x, yy + 0.08f, top.z), w), v3_sub(v3(top.x, yy + 0.08f, top.z), w), 0, 0, 1, 1, col);
            else b3d_quad(v3_sub(bot, w), v3_add(bot, w), v3_add(top, w), v3_sub(top, w), 0, 0, 1, 1, col);
        }
    }
    b3d_flush();
    // lightning bolts: jagged bright segments from the clouds to the ground
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    for (int i = 0; i < 8; i++) {
        Bolt *b = &bolts[i];
        if (b->life <= 0) continue;
        Rng r = {b->seed + (u32)(b->life / 4)};
        vec3 p = v3_add(b->pos, v3(0, 120, 0));
        vec3 side = v3_norm(v3_cross(v3_sub(b->pos, cam), v3(0, 1, 0)));
        while (p.y > b->pos.y) {
            vec3 q = v3(p.x + rng_range(&r, -1.2f, 1.2f), fmaxf(b->pos.y, p.y - rng_range(&r, 3, 7)), p.z + rng_range(&r, -1.2f, 1.2f));
            if (q.y <= b->pos.y + 0.01f) q = b->pos;
            for (int w = 0; w < 2; w++) {
                vec3 s = v3_scale(side, w ? 0.35f : 0.12f);
                u32 col = w ? rgba_f(0.45f, 0.45f, 0.9f, 0.35f) : rgba_f(0.95f, 0.95f, 1, 0.95f);
                b3d_quad(v3_sub(p, s), v3_add(p, s), v3_add(q, s), v3_sub(q, s), 0, 0, 1, 1, col);
            }
            p = q;
        }
    }
    b3d_flush();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
}

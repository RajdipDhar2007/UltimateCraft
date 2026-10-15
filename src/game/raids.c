// Raids and pillager patrols, following Minecraft 1.21:
//  * Patrols: from day 5, every 10-11 minutes (12000 + rand(1200) ticks) a 20% roll during daylight
//    spawns 1 + local-difficulty-based pillagers 24-48 blocks from the player, outside villages,
//    led by a captain carrying the ominous banner. They walk a route and attack on sight.
//  * Killing a patrol captain (outside a raid) drops an Ominous Bottle. Drinking it gives Bad Omen I-V.
//  * A player with Bad Omen who enters a village gets Raid Omen; when it runs out (30 s) a raid
//    starts there: 3 / 5 / 7 waves on Easy / Normal / Hard (+1 bonus wave for Bad Omen II+),
//    using Minecraft's raider table (pillagers, vindicators, evokers, witches, ravagers).
//  * The bell-like horn sounds when a wave arrives; the boss bar shows the raid's remaining health.
//  * Victory gives Hero of the Village (level = omen level, 40 minutes): villagers discount trades.
//    The raid is lost when every villager in range dies, or it times out (40 minutes).
#include "raids.h"
#include "game.h"
#include "player.h"
#include "fx.h"
#include "../world/rules.h"
#include "../world/structures.h"
#include "../entity/entity.h"
#include "../audio/audio.h"
#include "../ui/ui.h"

#define MCT(t) ((t) * MC_TICK_DIV)   // Minecraft ticks -> engine ticks

typedef struct {
    bool active, won, lost;
    vec3 center;
    int omen;                 // Bad Omen level that started the raid (1..5)
    int waves, wave;          // total waves, waves spawned so far
    int cooldown;             // engine ticks until the next wave
    int age;                  // engine ticks since the raid began
    int post;                 // ticks of the victory / defeat celebration
    u32 id;
    f32 total_health;         // of the current wave (boss bar)
    bool had_villagers;
} Raid;
static Raid R;
static u32 next_id = 1;

void raids_reset(void) { memset(&R, 0, sizeof R); }
bool raid_active(void) { return R.active; }
int raid_wave(void) { return R.wave; }

// Minecraft's Raid.RaiderType counts, indexed by wave (1..7, index 0 unused)
static const struct { int mob; int n[8]; } RAIDERS[] = {
    {0, {0}}, // filled at runtime (MOB_* are not constant expressions in every build)
};
static int raider_count(int kind, int wave) {
    static const int VIND[8] = {0, 0, 2, 0, 1, 4, 2, 5};
    static const int EVOK[8] = {0, 0, 0, 0, 1, 1, 0, 2};
    static const int PILL[8] = {0, 4, 3, 3, 4, 4, 4, 2};
    static const int WITC[8] = {0, 0, 0, 0, 3, 0, 0, 1};
    static const int RAVA[8] = {0, 0, 0, 1, 0, 1, 0, 2};
    wave = clampi(wave, 1, 7);
    switch (kind) { case 0: return PILL[wave]; case 1: return VIND[wave]; case 2: return EVOK[wave]; case 3: return WITC[wave]; default: return RAVA[wave]; }
}
static int kind_mob(int k) { return k == 0 ? MOB_PILLAGER : k == 1 ? MOB_VINDICATOR : k == 2 ? MOB_EVOKER : k == 3 ? MOB_WITCH : MOB_RAVAGER; }

static int surface_y(int x, int z) {
    int y = world_top_y(x, z);
    return y < 0 ? -1 : y + 1;
}
static bool find_spawn(vec3 around, f32 rmin, f32 rmax, vec3 *out) {
    for (int tries = 0; tries < 24; tries++) {
        f32 a = frand() * 2 * PI_F, r = frand_range(rmin, rmax);
        int x = (int)floorf(around.x + cosf(a) * r), z = (int)floorf(around.z + sinf(a) * r);
        if (!world_loaded(x, z)) continue;
        int y = surface_y(x, z);
        if (y <= 0) continue;
        u16 below = world_get_block(x, y - 1, z);
        if (BLOCKS[below].liquid || !BLOCKS[below].solid) continue;
        *out = v3(x + 0.5f, (f32)y, z + 0.5f);
        return true;
    }
    return false;
}
static Entity *spawn_raider(int mob, vec3 p, vec3 goal, u32 rid, bool captain) {
    Entity *e = mob_spawn(mob, p);
    if (!e) return NULL;
    e->persistent = true;
    e->raid_id = rid;
    e->raid_role = captain;
    e->anchor = goal;
    return e;
}

// ------------------------------------------------------------------ patrols
static bool force_patrol;
static void try_patrol(void) {
    Entity *pl = player_ent();
    if (!pl || g_world.dim != DIM_OVERWORLD || !rule(GR_DO_PATROL_SPAWNING) || g_level.difficulty == DIFF_PEACEFUL) return;
    if (--g_level.patrol_timer > 0 && !force_patrol) return;
    g_level.patrol_timer = 12000 + irand(0, 1199);
    u64 days = world_daytime() / DAY_TICKS;
    if (!force_patrol && (days < 5 || world_is_night() || irand(0, 4) != 0)) return;
    if (!force_patrol && structure_at((int)pl->pos.x, (int)pl->pos.y, (int)pl->pos.z) == ST_VILLAGE) return;
    vec3 p;
    if (!find_spawn(pl->pos, 24, 48, &p)) return;
    int biome = world_biome_at((int)p.x, (int)p.y, (int)p.z);
    if (biome == BIOME_MUSHROOM_FIELDS) return;
    // group size grows with the regional difficulty (approximated by difficulty and days played)
    int extra = (int)ceilf(fminf(days / 10.0f, 3.0f) + g_level.difficulty * 0.5f);
    int n = 1 + irand(0, extra);
    f32 a = frand() * 2 * PI_F;
    vec3 goal = v3(p.x + cosf(a) * 64, p.y, p.z + sinf(a) * 64);   // a patrol walks a long route
    for (int i = 0; i < n; i++) {
        vec3 q = v3(p.x + frand_range(-3, 3), p.y, p.z + frand_range(-3, 3));
        int y = surface_y((int)floorf(q.x), (int)floorf(q.z)); if (y > 0) q.y = (f32)y;
        spawn_raider(MOB_PILLAGER, q, goal, 1u << 31, i == 0);
    }
    LOG_INFO("pillager patrol of %d spawned at %.0f %.0f", n, p.x, p.z);
}

// ------------------------------------------------------------------ raids
static int raid_alive(f32 *health) {
    int n = 0; f32 h = 0;
    ENT_FOREACH(e) if (e->kind == ENT_MOB && e->raid_id == R.id && e->death_time == 0) { n++; h += e->health; }
    if (health) *health = h;
    return n;
}
static int villagers_near(void) {
    int n = 0;
    ENT_FOREACH(e) if (e->kind == ENT_MOB && e->mob == MOB_VILLAGER && e->death_time == 0 && v3_dist(e->pos, R.center) < 96) n++;
    return n;
}
static void spawn_wave(void) {
    R.wave++;
    vec3 p;
    if (!find_spawn(R.center, 28, 40, &p)) p = v3_add(R.center, v3(32, 0, 0));
    bool bonus = R.wave > R.waves - (R.omen > 1 ? 1 : 0) && R.omen > 1;
    int wave = bonus ? R.waves - 1 : R.wave;
    bool captain_done = false;
    R.total_health = 0;
    for (int k = 0; k < 5; k++) {
        int n = raider_count(k, wave);
        // bonus raiders by difficulty, like Raid.getPotentialBonusSpawns
        if (k <= 1 && g_level.difficulty == DIFF_HARD) n += irand(0, 2);
        else if (k <= 1 && g_level.difficulty == DIFF_NORMAL) n += irand(0, 1);
        if (k == 3 && g_level.difficulty == DIFF_EASY) n = 0;
        if (k == 4 && g_level.difficulty == DIFF_EASY && wave < 4) n = 0;
        for (int i = 0; i < n; i++) {
            vec3 q = v3(p.x + frand_range(-4, 4), p.y, p.z + frand_range(-4, 4));
            int y = surface_y((int)floorf(q.x), (int)floorf(q.z)); if (y > 0) q.y = (f32)y;
            bool cap = !captain_done && (k == 0 || k == 1);
            Entity *e = spawn_raider(kind_mob(k), q, R.center, R.id, cap);
            if (cap && e) captain_done = true;
            if (e) R.total_health += e->max_health;
            if (e) e->target = 0;
        }
    }
    sound_play_at(SND_LEVELUP, R.center, 2.0f, 0.35f);   // the raid horn
    char t[64]; snprintf(t, sizeof t, "Raid - Wave %d", R.wave);
    G.actionbar_t = 0;
    title_show("", t);
    LOG_INFO("raid wave %d/%d spawned (%d raiders)", R.wave, R.waves, raid_alive(NULL));
}

static void raid_start(vec3 at, int omen) {
    if (rule(GR_DISABLE_RAIDS) || g_level.difficulty == DIFF_PEACEFUL) return;
    memset(&R, 0, sizeof R);
    R.active = true; R.center = at; R.omen = clampi(omen, 1, 5);
    R.waves = (g_level.difficulty == DIFF_EASY ? 3 : g_level.difficulty == DIFF_NORMAL ? 5 : 7) + (R.omen > 1 ? 1 : 0);
    R.id = next_id++;
    if (R.id >= (1u << 31) - 1) R.id = next_id = 1;
    R.cooldown = MCT(300);   // 15 seconds before the first wave
    game_message(rgba_u32(255, 85, 85, 255), "A raid has begun!");
    // villagers run home: they panic away from the centre of the raid
    ENT_FOREACH(e) if (e->kind == ENT_MOB && e->mob == MOB_VILLAGER && v3_dist(e->pos, at) < 96) { e->ai = AI_PANIC; e->ai_timer = MCT(200); }
}
static void raid_end(bool won) {
    Entity *pl = player_ent();
    R.won = won; R.lost = !won; R.post = MCT(200);
    if (won) {
        if (pl) effect_add(pl, EFF_HERO_OF_THE_VILLAGE, R.omen - 1, MCT(48000));
        title_show("Victory", "Raid - Victory");
        for (int i = 0; i < 6; i++) proj_spawn(PROJ_FIREWORK, v3_add(R.center, v3(frand_range(-8, 8), 1, frand_range(-8, 8))), v3(0, 10, 0), 0);
    } else {
        title_show("Defeat", "Raid - Defeat");
        // remaining raiders celebrate and stay
    }
    game_message(won ? rgba_u32(85, 255, 85, 255) : rgba_u32(255, 85, 85, 255), won ? "Raid - Victory" : "Raid - Defeat");
}

static void raid_tick(void) {
    if (!R.active) return;
    R.age++;
    if (R.post > 0) { if (--R.post == 0) { ENT_FOREACH(e) if (e->raid_id == R.id) e->raid_id = 0; R.active = false; } return; }
    Entity *pl = player_ent();
    if (R.age > MCT(48000) || (pl && v3_dist(pl->pos, R.center) > 160 && R.age > MCT(2400))) { raid_end(false); return; }
    f32 h;
    int alive = raid_alive(&h);
    if (R.wave > 0 && R.had_villagers && villagers_near() == 0) { raid_end(false); return; }   // every villager died
    if (villagers_near() > 0) R.had_villagers = true;
    if (alive == 0) {
        if (R.wave >= R.waves) { raid_end(true); return; }
        if (R.cooldown <= 0) R.cooldown = MCT(300);
        if (--R.cooldown == 0) spawn_wave();
    }
    // raiders that strayed far get pulled back toward the village
    if (R.age % MCT(100) == 0) ENT_FOREACH(e) if (e->kind == ENT_MOB && e->raid_id == R.id) e->anchor = R.center;
}

static void omen_tick(void) {
    Entity *pl = player_ent();
    if (!pl || g_world.dim != DIM_OVERWORLD) return;
    // Bad Omen in a village -> Raid Omen (30 s), then the raid starts where the player was
    if (effect_has(pl, EFF_BAD_OMEN) && !R.active && structure_at((int)pl->pos.x, (int)pl->pos.y, (int)pl->pos.z) == ST_VILLAGE && !rule(GR_DISABLE_RAIDS)) {
        int lvl = pl->eff_amp[EFF_BAD_OMEN];
        pl->eff_time[EFF_BAD_OMEN] = 0;
        effect_add(pl, EFF_RAID_OMEN, lvl, MCT(600));
        sound_play(SND_LEVELUP, 1, 0.5f);
    }
    if (effect_has(pl, EFF_RAID_OMEN) && pl->eff_time[EFF_RAID_OMEN] <= 1 && !R.active) {
        int lvl = pl->eff_amp[EFF_RAID_OMEN] + 1;
        pl->eff_time[EFF_RAID_OMEN] = 0;
        ivec3 at;
        vec3 c = pl->pos;
        if (structure_locate(ST_VILLAGE, (int)pl->pos.x, (int)pl->pos.z, &at) && v3_dist(v3((f32)at.x, pl->pos.y, (f32)at.z), pl->pos) < 80) c = v3(at.x + 0.5f, (f32)MAXI(at.y, surface_y(at.x, at.z)), at.z + 0.5f);
        raid_start(c, lvl);
    }
}

void raid_on_mob_death(Entity *e, Entity *killer) {
    if (!e || e->kind != ENT_MOB) return;
    // a patrol / outpost captain (not part of a raid) drops an Ominous Bottle (1.21)
    if (e->raid_role == 1 && (e->raid_id == 0 || (e->raid_id >> 31)) && rule(GR_DO_MOB_LOOT)) {
        ItemStack b = stack_of(I_OMINOUS_BOTTLE, 1);
        b.damage = (u16)irand(0, 4);   // amplifier of the Bad Omen it gives
        item_drop_scatter(v3_add(e->pos, v3(0, 0.5f, 0)), b);
    }
    (void)killer;
}

void raids_tick(void) {
    if (G.ticks % MC_TICK_DIV == 0) try_patrol();
    omen_tick();
    raid_tick();
}

void raids_render_hud(void) {
    if (!R.active) return;
    f32 h = 0; int alive = raid_alive(&h);
    f32 w = 182, x = ui_w / 2.0f - w / 2, y = 12;
    char t[64];
    if (R.won) snprintf(t, sizeof t, "Raid - Victory");
    else if (R.lost) snprintf(t, sizeof t, "Raid - Defeat");
    else if (alive == 0) snprintf(t, sizeof t, "Raid");
    else snprintf(t, sizeof t, "Raid - %d Raider%s Remaining", alive, alive == 1 ? "" : "s");
    ui_text_center(ui_w / 2.0f, 2, t, COL_WHITE, true);
    ui_rect(x, y, w, 5, rgba_u32(60, 10, 10, 220));
    f32 frac = alive == 0 ? (R.won ? 0 : 1.0f - (f32)R.cooldown / MCT(300)) : clampf(h / fmaxf(1, R.total_health), 0, 1);
    ui_rect(x, y, w * frac, 5, rgba_u32(220, 40, 40, 255));
}

void raid_command(const char *a) {
    Entity *pl = player_ent();
    if (!strcmp(a, "start") && pl) { raid_start(pl->pos, 1); return; }
    if (!strcmp(a, "stop")) { ENT_FOREACH(e) if (e->raid_id == R.id && R.active) e->remove = true; R.active = false; game_message(rgba_u32(170, 170, 170, 255), "Raid stopped"); return; }
    if (!strcmp(a, "patrol")) { force_patrol = true; try_patrol(); force_patrol = false; return; }
    if (!strcmp(a, "win")) { ENT_FOREACH(e) if (e->raid_id == R.id) e->remove = true; R.wave = R.waves; return; }
    if (R.active) game_message(COL_WHITE, "Raid: wave %d of %d, %d raiders alive, omen %d", R.wave, R.waves, raid_alive(NULL), R.omen);
    else game_message(COL_WHITE, "No raid is active");
    (void)RAIDERS;
}

// World persistence: level.txt (human readable), player.dat, entities.dat, containers.dat.
// Modified terrain is stored per column (save_column.c); untouched terrain regenerates from the seed.
#include "game.h"
#include "../entity/villager.h"
#include "player.h"
#include "../core/platform.h"
#include "../world/rules.h"

#define SAVE_VERSION 5   // v5: effect count stored in player.dat, Sift dimension (DIM2/), sift_return; v4: 384-block world (y -64..319), game rules, difficulty, weather; v2: items/mobs/blocks stored by Minecraft id, dimensions; v3: enchantments, names, item data

static void path_of(char *out, size_t n, const char *world, const char *file) { snprintf(out, n, "saves/%s/%s", world, file); }

bool save_level(void) {
    scoreboard_save(g_world.dir);
    char path[300], buf[8192];
    path_of(path, sizeof path, g_world.name, "level.txt");
    int n = snprintf(buf, sizeof buf,
        "# UltimateCraft world\nversion=%d\nname=%s\nseed=%llu\ntime=%llu\nspawn=%d %d %d\ngamemode=%d\ndim=%d\ndragon_killed=%d\ntyrant_killed=%d\nsift_gate=%d %d %d\nsift_trials=%d\nascended=%d\nwarden_warning=%d\nworldborder=%.3f %.3f %.3f %.3f %.3f\n",
        SAVE_VERSION, g_world.name, (unsigned long long)g_world.seed, (unsigned long long)g_world.time,
        g_world.spawn.x, g_world.spawn.y, g_world.spawn.z, g_player.gamemode, g_world.dim, (int)g_world.dragon_killed, (int)g_world.tyrant_killed, g_world.sift_gate.x, g_world.sift_gate.y, g_world.sift_gate.z, (int)g_world.sift_trials, (int)g_world.ascended, g_world.warden_warning, g_wb.cx, g_wb.cz, g_wb.target, (double)g_wb.damage, (double)g_wb.buffer);
    n += snprintf(buf + n, sizeof buf - n, "sift_return=%d %d %d\n", g_world.sift_return.x, g_world.sift_return.y, g_world.sift_return.z);
    n += snprintf(buf + n, sizeof buf - n, "day_shift=%lld\ndifficulty=%d\ndifficulty_locked=%d\nhardcore=%d\nallow_commands=%d\n"
                  "raining=%d\nrain_time=%d\nthundering=%d\nthunder_time=%d\nclear_weather_time=%d\npatrol_timer=%d\ntrader_timer=%d\ntrader_chance=%d\n",
                  (long long)g_world.day_shift, g_level.difficulty, g_level.difficulty_locked, g_level.hardcore, g_level.cheats,
                  g_level.raining, g_level.rain_time, g_level.thundering, g_level.thunder_time, g_level.clear_time,
                  g_level.patrol_timer, g_level.trader_timer, g_level.trader_chance);
    for (int i = 0; i < GR_COUNT && n < (int)sizeof buf - 80; i++) n += snprintf(buf + n, sizeof buf - n, "gamerule.%s=%d\n", GAMERULE_DEFS[i].key, g_rules[i]);
    char npath[300]; path_of(npath, sizeof npath, g_world.name, "names.txt");
    item_names_save(npath);
    return plat_write_file(path, buf, (size_t)n);
}

// Reads seed/gamemode (and time/spawn into g_world when the world is the active one being opened).
int save_level_dim(const char *name) {
    char path[300];
    path_of(path, sizeof path, name, "level.txt");
    char *txt = plat_read_file(path, NULL);
    if (!txt) return 0;
    int d = 0; char *p = strstr(txt, "dim=");
    if (p) d = atoi(p + 4);
    free(txt);
    return clampi(d, 0, DIM_COUNT - 1);
}
bool save_load_level(const char *name, u64 *seed, int *gamemode) {
    char path[300];
    path_of(path, sizeof path, name, "level.txt");
    char *txt = plat_read_file(path, NULL);
    if (!txt) return false;
    unsigned long long s = 0, t = 0;
    int gm = 0, sx = 0, sy = 0, sz = 0, dk = 0, tk = 0, ver = 1, iv, st = 0, asc = 0, ww = 0; double wb[5] = {0, 0, 59999968, 0.2, 5}; bool wbset = false; ivec3 gate = {0, 0, 0};
    long long ds = 0;
    bool have_spawn = false, active = g_world.active && !strcmp(g_world.name, name);
    if (active) level_state_reset();
    for (char *line = strtok(txt, "\r\n"); line; line = strtok(NULL, "\r\n")) {
        if (sscanf(line, "version=%d", &ver) == 1) continue;
        if (sscanf(line, "day_shift=%lld", &ds) == 1) continue;
        if (active) {
            char key[96]; 
            if (sscanf(line, "gamerule.%95[^=]=%d", key, &iv) == 2) { int r = rule_by_key(key); if (r >= 0) g_rules[r] = clampi(iv, GAMERULE_DEFS[r].lo, GAMERULE_DEFS[r].hi); continue; }
            if (sscanf(line, "difficulty=%d", &iv) == 1) { g_level.difficulty = (u8)clampi(iv, 0, 3); continue; }
            if (sscanf(line, "difficulty_locked=%d", &iv) == 1) { g_level.difficulty_locked = iv != 0; continue; }
            if (sscanf(line, "hardcore=%d", &iv) == 1) { g_level.hardcore = iv != 0; continue; }
            if (sscanf(line, "allow_commands=%d", &iv) == 1) { g_level.cheats = iv != 0; continue; }
            if (sscanf(line, "raining=%d", &iv) == 1) { g_level.raining = iv != 0; g_level.rain_level = (f32)g_level.raining; continue; }
            if (sscanf(line, "thundering=%d", &iv) == 1) { g_level.thundering = iv != 0; g_level.thunder_level = (f32)g_level.thundering; continue; }
            if (sscanf(line, "rain_time=%d", &iv) == 1) { g_level.rain_time = iv; continue; }
            if (sscanf(line, "thunder_time=%d", &iv) == 1) { g_level.thunder_time = iv; continue; }
            if (sscanf(line, "clear_weather_time=%d", &iv) == 1) { g_level.clear_time = iv; continue; }
            if (sscanf(line, "patrol_timer=%d", &iv) == 1) { g_level.patrol_timer = iv; continue; }
            if (sscanf(line, "trader_timer=%d", &iv) == 1) { g_level.trader_timer = iv; continue; }
            if (sscanf(line, "trader_chance=%d", &iv) == 1) { g_level.trader_chance = iv; continue; }
        }
        if (sscanf(line, "seed=%llu", &s) == 1) continue;
        if (sscanf(line, "time=%llu", &t) == 1) continue;
        if (sscanf(line, "gamemode=%d", &gm) == 1) continue;
        if (sscanf(line, "dragon_killed=%d", &dk) == 1) continue;
        if (sscanf(line, "tyrant_killed=%d", &tk) == 1) continue;
        if (sscanf(line, "sift_gate=%d %d %d", &gate.x, &gate.y, &gate.z) == 3) continue;
        if (sscanf(line, "sift_trials=%d", &st) == 1) continue;
        if (sscanf(line, "ascended=%d", &asc) == 1) continue;
        if (sscanf(line, "warden_warning=%d", &ww) == 1) continue;
        if (sscanf(line, "worldborder=%lf %lf %lf %lf %lf", &wb[0], &wb[1], &wb[2], &wb[3], &wb[4]) == 5) { wbset = true; continue; }
        if (sscanf(line, "sift_return=%d %d %d", &g_world.sift_return.x, &g_world.sift_return.y, &g_world.sift_return.z) == 3) continue;
        if (sscanf(line, "spawn=%d %d %d", &sx, &sy, &sz) == 3) { have_spawn = true; continue; }
    }
    free(txt);
    if (seed) *seed = s;
    if (gamemode) *gamemode = gm;
    if (g_world.active && !strcmp(g_world.name, name)) {
        g_world.time = t;
        g_world.day_shift = ds;
        if (ver < 4) { sy += OW_Y_OFFSET; g_level.cheats = true; g_world.old_height_save = true; }   // world from the 128-high era
        g_world.dragon_killed = dk != 0;
        g_world.tyrant_killed = tk != 0;
        g_world.sift_gate = gate; g_world.sift_trials = (u8)st; g_world.ascended = asc != 0; g_world.warden_warning = ww;
        g_wb.cx = wb[0]; g_wb.cz = wb[1]; g_wb.size = g_wb.target = wb[2]; g_wb.lerp = 0; g_wb.damage = (f32)wb[3]; g_wb.buffer = (f32)wb[4]; (void)wbset;

        if (have_spawn) g_world.spawn = iv3(sx, sy, sz);
    }
    return true;
}

// ------------------------------------------------------------------------ binary helpers
typedef struct { u8 *d; size_t n, cap; } WBuf;
static void w_put(WBuf *b, const void *p, size_t n) {
    if (b->n + n > b->cap) { b->cap = (b->cap + n) * 2; b->d = xrealloc(b->d, b->cap); }
    memcpy(b->d + b->n, p, n); b->n += n;
}
#define W(b, v) w_put(b, &(v), sizeof(v))
typedef struct { const u8 *d; size_t n, pos; bool ok; } RBuf;
static void r_get(RBuf *b, void *p, size_t n) {
    if (b->pos + n > b->n) { b->ok = false; memset(p, 0, n); return; }
    memcpy(p, b->d + b->pos, n); b->pos += n;
}
#define R(b, v) r_get(b, &(v), sizeof(v))

static void w_str(WBuf *b, const char *s) { u8 n = (u8)MINI(255, (int)strlen(s)); W(b, n); w_put(b, s, n); }
static void r_str(RBuf *b, char *out) { u8 n = 0; R(b, n); r_get(b, out, n); out[n] = 0; }
// stacks are stored by item id string so saves survive id renumbering
static int read_version = SAVE_VERSION;   // version of the file being read (stack layout differs)
static void w_stack(WBuf *b, ItemStack s) {
    w_str(b, stack_empty(s) ? "" : ITEMS[s.id].key);
    W(b, s.count); W(b, s.damage);
    if (stack_empty(s)) return;
    W(b, s.rcost); W(b, s.data); W(b, s.name);
    u8 n = (u8)ench_count(s); W(b, n);
    bool banner = strstr(ITEMS[s.id].key, "_banner") && !strstr(ITEMS[s.id].key, "pattern");
    for (int i = 0; i < MAX_ENCH; i++) if (s.ench[i][0]) {
        char num[8]; snprintf(num, sizeof num, "%d", s.ench[i][0]);
        w_str(b, banner || s.ench[i][0] >= ENCH_COUNT ? num : ENCHANTS[s.ench[i][0]].key); W(b, s.ench[i][1]);
    }
}
static ItemStack r_stack(RBuf *b) {
    char key[256]; r_str(b, key);
    ItemStack s = {0}; R(b, s.count); R(b, s.damage);
    if (read_version >= 3 && key[0]) {
        R(b, s.rcost); R(b, s.data); R(b, s.name);
        u8 n = 0; R(b, n);
        for (int i = 0; i < n && b->ok; i++) {
            char ek[64]; u8 lvl = 0; r_str(b, ek); R(b, lvl);
            int e = 0;
            for (int k = 1; k < ENCH_COUNT; k++) if (!strcmp(ENCHANTS[k].key, ek)) e = k;
            if (e && !(strstr(key, "_banner") && !strstr(key, "pattern"))) ench_add(&s, e, lvl);
            else if (n && i < MAX_ENCH) { s.ench[i][0] = (u8)atoi(ek); s.ench[i][1] = lvl; }   // banner layers are stored numerically
        }
    }
    s.id = (u16)item_by_key(key);
    if (!item_valid(s.id) || s.count == 0) s = (ItemStack){0};
    return s;
}

// ------------------------------------------------------------------------ player
void save_player(void) {
    Entity *e = player_ent();
    if (!e) return;
    WBuf b = {0};
    u32 magic = 0x32504355, ver = SAVE_VERSION; // "UCP2" + version
    W(&b, magic); W(&b, ver);
    W(&b, e->pos); W(&b, e->yaw); W(&b, e->pitch); W(&b, e->health);
    W(&b, g_player.food); W(&b, g_player.saturation); W(&b, g_player.exhaustion);
    W(&b, g_player.xp_level); W(&b, g_player.xp_progress); W(&b, g_player.xp_total);
    W(&b, e->air); W(&b, e->fire); W(&b, g_player.inv.selected);
    for (int i = 0; i < INV_MAIN; i++) w_stack(&b, g_player.inv.slots[i]);
    for (int i = 0; i < 4; i++) w_stack(&b, g_player.inv.armor[i]);
    w_stack(&b, g_player.inv.offhand);
    for (int i = 0; i < 27; i++) w_stack(&b, g_player.ender[i]);
    W(&b, g_player.rest_ticks);
    u32 neff = EFFECT_COUNT; W(&b, neff);   // v5+: the count is stored so new effects never shift the layout again
    for (int i = 0; i < EFFECT_COUNT; i++) { W(&b, e->eff_time[i]); W(&b, e->eff_amp[i]); }
    W(&b, g_player.score); W(&b, g_player.kills);
    u8 flying = g_player.flying, dead = g_player.dead;
    W(&b, flying); W(&b, dead);
    W(&b, g_player.ench_seed);
    u32 nrk = (u32)sizeof g_player.recipes_known; W(&b, nrk);
    for (u32 i = 0; i < nrk; i++) W(&b, g_player.recipes_known[i]);
    char path[300];
    path_of(path, sizeof path, g_world.name, "player.dat");
    plat_write_file(path, b.d, b.n);
    free(b.d);
}

bool save_load_player(void) {
    char path[300];
    path_of(path, sizeof path, g_world.name, "player.dat");
    size_t len;
    u8 *data = (u8 *)plat_read_file(path, &len);
    if (!data) return false;
    RBuf b = {data, len, 0, true};
    u32 magic, ver; R(&b, magic); R(&b, ver);
    read_version = (int)ver;
    if (magic != 0x32504355) { free(data); LOG_WARN("player.dat is from an older version; starting fresh"); return false; }
    vec3 pos; f32 yaw, pitch, hp;
    R(&b, pos); R(&b, yaw); R(&b, pitch); R(&b, hp);
    player_reset(pos, true);
    Entity *e = player_ent();
    e->yaw = yaw; e->pitch = pitch; e->health = hp;
    R(&b, g_player.food); R(&b, g_player.saturation); R(&b, g_player.exhaustion);
    R(&b, g_player.xp_level); R(&b, g_player.xp_progress); R(&b, g_player.xp_total);
    R(&b, e->air); R(&b, e->fire); R(&b, g_player.inv.selected);
    for (int i = 0; i < INV_MAIN; i++) g_player.inv.slots[i] = r_stack(&b);
    for (int i = 0; i < 4; i++) g_player.inv.armor[i] = r_stack(&b);
    g_player.inv.offhand = r_stack(&b);
    for (int i = 0; i < 27; i++) g_player.ender[i] = r_stack(&b);
    R(&b, g_player.rest_ticks);
    {   u32 neff = read_version >= 5 ? 0 : read_version >= 4 ? 40 : 27;   // v4 saves had 40 effects, v3 27
        if (read_version >= 5) { R(&b, neff); if (neff > 4096) { b.ok = false; neff = 0; } }
        for (u32 i = 0; i < neff && b.ok; i++) {
            __typeof__(e->eff_time[0]) t; __typeof__(e->eff_amp[0]) a; R(&b, t); R(&b, a);
            if (i < EFFECT_COUNT) { e->eff_time[i] = t; e->eff_amp[i] = a; }
        }
    }
    R(&b, g_player.score); R(&b, g_player.kills);
    u8 flying, dead; R(&b, flying); R(&b, dead);
    g_player.flying = flying && g_player.gamemode == GM_CREATIVE;
    memset(g_player.recipes_known, 0, sizeof g_player.recipes_known);
    if (read_version >= 3 && b.ok) {
        R(&b, g_player.ench_seed);
        u32 nrk = 0; R(&b, nrk);
        for (u32 i = 0; i < nrk && b.ok; i++) { u8 v; R(&b, v); if (i < sizeof g_player.recipes_known) g_player.recipes_known[i] = v; }
        b.ok = true;
    }
    free(data);
    g_player.inv.selected = clampi(g_player.inv.selected, 0, 8);
    if (!b.ok) LOG_WARN("player.dat truncated; some data reset");
    if (dead || e->health <= 0) player_respawn();   // died before quitting: come back alive at spawn
    return true;
}

// ------------------------------------------------------------------------ entities + containers
void save_entities(void) {
    WBuf b = {0};
    u32 magic = 0x35454355, n = 0; // "UCE5" (v3: villager / vehicle data; v4: decorations, armour-stand equipment; v5: reputation)
    W(&b, magic);
    size_t count_at = b.n; W(&b, n);
    ENT_FOREACH(e) {
        if (e->kind != ENT_MOB && e->kind != ENT_ITEM && e->kind != ENT_XP && e->kind != ENT_CRYSTAL && e->kind != ENT_VEHICLE && e->kind != ENT_DECOR && e->kind != ENT_FALLING) continue;
        if (e->kind == ENT_MOB && e->death_time > 0) continue;
        W(&b, e->kind); w_str(&b, e->kind == ENT_MOB ? MOB_DEFS[e->mob].key : ""); W(&b, e->pos); W(&b, e->yaw); W(&b, e->health);
        W(&b, e->fire); W(&b, e->age); W(&b, e->xp); W(&b, e->persistent);
        u8 flags = (u8)(e->baby | e->tamed << 1 | e->sheared << 2 | e->sitting << 3 | e->from_spawner << 4 | e->saddled << 5);
        W(&b, flags); W(&b, e->color); W(&b, e->size); W(&b, e->grow_age); W(&b, e->phase); W(&b, e->special_timer);
        w_stack(&b, e->item);
        // v3 extension
        W(&b, e->prof); W(&b, e->vlevel); W(&b, e->vxp); W(&b, e->jobsite); W(&b, e->vtype);
        int no = 0; TradeOffer *of = e->kind == ENT_MOB ? villager_offers(e, &no) : NULL;
        u8 nof = (u8)no; W(&b, nof);
        for (int k = 0; k < no; k++) { w_stack(&b, of[k].a); w_stack(&b, of[k].b); w_stack(&b, of[k].result); W(&b, of[k].uses); W(&b, of[k].max_uses); W(&b, of[k].xp); }
        // v4 extension
        W(&b, e->pose); for (int k = 0; k < 4; k++) w_stack(&b, e->equip[k]);
        W(&b, e->rep);
        n++;
    }
    memcpy(b.d + count_at, &n, sizeof n);
    u32 nb = (u32)g_world.nbents;
    W(&b, nb);
    for (int i = 0; i < g_world.nbents; i++) {
        BlockEntity *be = &g_world.bents[i];
        W(&b, be->pos); W(&b, be->type); W(&b, be->burn_time); W(&b, be->burn_max); W(&b, be->cook_time); W(&b, be->xp); W(&b, be->data);
        for (int k = 0; k < 27; k++) w_stack(&b, be->slots[k]);
    }
    char path[300];
    snprintf(path, sizeof path, "%.280s/entities.dat", g_world.dim_dir);
    plat_write_file(path, b.d, b.n);
    free(b.d);
}

void save_load_entities(void) {
    char path[300];
    snprintf(path, sizeof path, "%.280s/entities.dat", g_world.dim_dir);
    size_t len;
    u8 *data = (u8 *)plat_read_file(path, &len);
    if (!data) return;
    RBuf b = {data, len, 0, true};
    u32 magic, n; R(&b, magic); R(&b, n);
    if (magic != 0x32454355 && magic != 0x33454355 && magic != 0x34454355 && magic != 0x35454355) { free(data); return; }
    bool ent_v3 = magic >= 0x33454355, ent_v4 = magic >= 0x34454355, ent_v5 = magic >= 0x35454355;
    for (u32 i = 0; i < n && b.ok; i++) {
        u8 kind; char mkey[256]; vec3 pos; f32 yaw, hp; int fire, age, xp; bool pers;
        u8 flags, color, size, phase; int grow, special;
        R(&b, kind); r_str(&b, mkey); R(&b, pos); R(&b, yaw); R(&b, hp); R(&b, fire); R(&b, age); R(&b, xp); R(&b, pers);
        R(&b, flags); R(&b, color); R(&b, size); R(&b, grow); R(&b, phase); R(&b, special);
        ItemStack it = r_stack(&b);
        u8 prof = 0, vlevel = 0, vtype = 0, nof = 0; u16 vxp = 0; ivec3 jobsite = {0, -1000, 0};
        static TradeOffer offs[16];
        if (ent_v3) {
            R(&b, prof); R(&b, vlevel); R(&b, vxp); R(&b, jobsite); R(&b, vtype); R(&b, nof);
            for (int k = 0; k < nof && b.ok; k++) {
                TradeOffer t; memset(&t, 0, sizeof t);
                t.a = r_stack(&b); t.b = r_stack(&b); t.result = r_stack(&b); R(&b, t.uses); R(&b, t.max_uses); R(&b, t.xp);
                if (k < 16) offs[k] = t;
            }
        }
        u8 pose = 0; ItemStack equip[4]; memset(equip, 0, sizeof equip);
        s8 rep = 0;
        if (ent_v4) { R(&b, pose); for (int k = 0; k < 4; k++) equip[k] = r_stack(&b); }
        if (ent_v5) R(&b, rep);
        if (!b.ok) break;
        Entity *e = NULL;
        #define APPLY_REP(x) do { if (x) (x)->rep = rep; } while (0)
        int mob = mob_by_key(mkey);
        if (kind == ENT_MOB && mob >= 0) {
            e = mob_spawn(mob, pos);
            if (e) {
                e->yaw = e->body_yaw = yaw; e->persistent = pers;
                e->baby = flags & 1; e->tamed = (flags >> 1) & 1; e->sheared = (flags >> 2) & 1; e->sitting = (flags >> 3) & 1; e->from_spawner = (flags >> 4) & 1; e->saddled = (flags >> 5) & 1;
                e->color = color; e->grow_age = grow; e->phase = phase; e->special_timer = special; e->item = it;
                if (size) mob_set_size(e, size); else if (e->baby) mob_set_size(e, 1);
                if (e->tamed && mob == MOB_WOLF) e->max_health = 40;
                e->health = hp;
                e->prof = prof; e->vlevel = vlevel; e->vxp = vxp; e->jobsite = jobsite;
                APPLY_REP(e);
                for (int k = 0; k < MINI(nof, 12); k++) *villager_offer_slot(e, k) = offs[k];
                villager_set_offer_count(e, MINI(nof, 12));
            }
        }
        else if (kind == ENT_CRYSTAL) e = crystal_spawn(pos);
        else if (kind == ENT_FALLING) e = xp > 0 ? falling_spawn(pos, (u16)xp) : NULL;
        else if (kind == ENT_DECOR) { e = decor_spawn(vtype, pos, size, color); if (e) { e->item = it; e->special_timer = special; e->pose = pose; memcpy(e->equip, equip, sizeof equip); e->yaw = e->body_yaw = e->head_yaw = yaw; } }
        else if (kind == ENT_VEHICLE) { e = vehicle_spawn(vtype, pos, yaw); if (e) { e->item = it; e->health = hp; } }
        else if (kind == ENT_ITEM && !stack_empty(it)) e = item_drop(pos, it, v3(0, 0, 0), 0);
        else if (kind == ENT_XP) { e = entity_spawn(ENT_XP, pos); if (e) { e->xp = xp; e->width = e->height = 0.25f; } }
        if (e) { e->fire = fire; e->age = age; }
    }
    u32 nb = 0; R(&b, nb);
    for (u32 i = 0; i < nb && b.ok; i++) {
        BlockEntity tmp;
        memset(&tmp, 0, sizeof tmp);
        R(&b, tmp.pos); R(&b, tmp.type); R(&b, tmp.burn_time); R(&b, tmp.burn_max); R(&b, tmp.cook_time); R(&b, tmp.xp); R(&b, tmp.data);
        for (int k = 0; k < 27; k++) tmp.slots[k] = r_stack(&b);
        if (!b.ok) break;
        BlockEntity *be = world_block_entity(tmp.pos, true, tmp.type);
        *be = tmp;
    }
    free(data);
}

// ------------------------------------------------------------------------ shulker box contents
// A broken shulker box keeps its items: they are stored here under an id kept in the item's data
// field and restored when the box is placed again (saves/<world>/boxes.dat).
typedef struct { u16 id; ItemStack s[27]; } BoxStore;
static BoxStore *boxes; static int nboxes, cap_boxes; static u16 next_box = 1;
void boxes_reset(void) { free(boxes); boxes = NULL; nboxes = cap_boxes = 0; next_box = 1; }
u16 box_store(const ItemStack *slots) {
    bool any = false;
    for (int i = 0; i < 27; i++) any |= !stack_empty(slots[i]);
    if (!any) return 0;
    if (nboxes == cap_boxes) { cap_boxes = cap_boxes ? cap_boxes * 2 : 16; boxes = xrealloc(boxes, sizeof(BoxStore) * cap_boxes); }
    BoxStore *b = &boxes[nboxes++];
    if (next_box == 0) next_box = 1;
    b->id = next_box++;
    memcpy(b->s, slots, sizeof b->s);
    return b->id;
}
static BoxStore *box_find(u16 id) { for (int i = 0; i < nboxes; i++) if (boxes[i].id == id) return &boxes[i]; return NULL; }
bool box_take(u16 id, ItemStack *out) {
    BoxStore *b = box_find(id);
    if (!b) return false;
    memcpy(out, b->s, sizeof b->s);
    *b = boxes[--nboxes];
    return true;
}
int box_count(u16 id, ItemStack *first) {
    BoxStore *b = box_find(id);
    if (!b) return 0;
    int n = 0;
    for (int i = 0; i < 27; i++) if (!stack_empty(b->s[i])) { if (!n && first) *first = b->s[i]; n++; }
    return n;
}
void boxes_save(void) {
    WBuf b = {0};
    u32 magic = 0x31584f42, n = (u32)nboxes; W(&b, magic); W(&b, next_box); W(&b, n);
    for (int i = 0; i < nboxes; i++) { W(&b, boxes[i].id); for (int k = 0; k < 27; k++) w_stack(&b, boxes[i].s[k]); }
    char path[300]; snprintf(path, sizeof path, "saves/%s/boxes.dat", g_world.name);
    plat_write_file(path, b.d, b.n);
    free(b.d);
}
void boxes_load(void) {
    boxes_reset();
    char path[300]; snprintf(path, sizeof path, "saves/%s/boxes.dat", g_world.name);
    size_t len; u8 *data = (u8 *)plat_read_file(path, &len);
    if (!data) return;
    RBuf b = {data, len, 0, true};
    u32 magic = 0, n = 0; R(&b, magic); R(&b, next_box); R(&b, n);
    if (magic == 0x31584f42) for (u32 i = 0; i < n && b.ok; i++) {
        ItemStack s[27]; u16 id = 0; R(&b, id);
        for (int k = 0; k < 27; k++) s[k] = r_stack(&b);
        if (!b.ok) break;
        u16 nid = box_store(s);
        if (nid) box_find(nid)->id = id;
    }
    free(data);
}

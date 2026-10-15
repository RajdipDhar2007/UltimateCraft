#include "../ui/lang.h"
#include "../ui/ui.h"
#include "../audio/audio.h"
// Chat commands (T or / in game). The same commands drive the automated test scripts (--script).
#include "game.h"
#include "raids.h"
#include "../world/rules.h"
#include "../world/structures.h"
#include "maps.h"
#include "../entity/villager.h"
#include "../entity/vehicle.h"
#include "../ui/screens.h"
#include "player.h"
#include "fx.h"
#include "../core/platform.h"
#include <ctype.h>

static void lower_compact(const char *in, char *out, size_t n) {
    size_t k = 0;
    for (; *in && k + 1 < n; in++) if (*in != ' ' && *in != '_' && *in != '-') out[k++] = (char)tolower((unsigned char)*in);
    out[k] = 0;
}

static int find_item(const char *name) {
    if (isdigit((unsigned char)name[0])) { int id = atoi(name); return item_valid(id) ? id : 0; }
    int byk = item_by_key(name);
    if (byk) return byk;
    char want[64], have[64];
    lower_compact(name, want, sizeof want);
    for (int i = 1; i < ITEM_MAX; i++) {
        if (!item_valid(i)) continue;
        lower_compact(ITEMS[i].name, have, sizeof have);
        if (!strcmp(have, want)) return i;
    }
    for (int i = 1; i < ITEM_MAX; i++) {   // partial match fallback ("pickaxe" -> first pickaxe)
        if (!item_valid(i)) continue;
        lower_compact(ITEMS[i].name, have, sizeof have);
        if (strstr(have, want)) return i;
    }
    return 0;
}
static int find_block(const char *name) {
    if (isdigit((unsigned char)name[0])) return clampi(atoi(name), 0, BLOCK_STATES - 1);
    char buf[96]; snprintf(buf, sizeof buf, "%s", name);
    int var = 0;
    char *colon = strrchr(buf, ':');
    if (colon && isdigit((unsigned char)colon[1])) { var = atoi(colon + 1); *colon = 0; }
    if (!strcmp(buf, "air")) return 0;
    Block k = block_by_key(buf);
    if (k) return block_with_var(k, var);
    char want[64], have[64];
    lower_compact(buf, want, sizeof want);
    for (int i = 1; i < BLOCK_STATES; i++) { if (BLOCKS[i].base != i) continue; lower_compact(BLOCKS[i].name, have, sizeof have); if (!strcmp(have, want)) return block_with_var(i, var); }
    return -1;
}
static int find_mob(const char *name) {
    int k = mob_by_key(name);
    if (k >= 0) return k;
    for (int i = 0; i < MOB_COUNT; i++) {
        char have[32]; lower_compact(MOB_DEFS[i].name, have, sizeof have);
        char want[32]; lower_compact(name, want, sizeof want);
        if (!strcmp(have, want)) return i;
    }
    return -1;
}

// ------------------------------------------------------------------------ test checks
static int checks_run, checks_failed;
int commands_checks_failed(void) { return checks_failed; }
int commands_checks_run(void) { return checks_run; }

// "x,y,z" with optional ~ (relative to the player's feet)
static bool parse_pos(const char *s, ivec3 *out) {
    Entity *e = player_ent();
    ivec3 base = e ? v3_floor(e->pos) : iv3(0, 0, 0);
    char buf[96]; snprintf(buf, sizeof buf, "%s", s);
    char *parts[3]; int n = 0;
    for (char *t = strtok(buf, ","); t && n < 3; t = strtok(NULL, ",")) parts[n++] = t;
    if (n < 3) return false;
    int v[3], bv[3] = {base.x, base.y, base.z};
    for (int i = 0; i < 3; i++) {
        char *t = parts[i];
        if (*t == '~') v[i] = bv[i] + atoi(t + 1); else v[i] = atoi(t) + (i == 1 ? world_y_offset() : 0);
    }
    *out = iv3(v[0], v[1], v[2]);
    return true;
}
static vec3 mark_pos;
static char g_complete_buf[256]; static int g_complete_n; static int g_fuzz_runs;
static f32 query(const char *what, const char *arg, bool *ok) {
    Entity *e = player_ent();
    *ok = true;
    if (!strcmp(what, "moved")) return e ? v3_dist(v3(e->pos.x, 0, e->pos.z), v3(mark_pos.x, 0, mark_pos.z)) : 0;
    if (!strcmp(what, "vxp")) { int best = -1; f32 bd = 1e9f; ENT_FOREACH(o) if (o->kind == ENT_MOB && o->mob == MOB_VILLAGER && e && v3_dist(o->pos, e->pos) < bd) { bd = v3_dist(o->pos, e->pos); best = o->vxp; } return (f32)best; }
    if (!strcmp(what, "shulkerplaced")) {   // items inside the shulker box nearest to the player
        int best = -1; f32 bd = 1e9f;
        for (int i = 0; i < g_world.nbents; i++) { BlockEntity *be = &g_world.bents[i]; if (be->type != BE_SHULKER || !e) continue; f32 dd = v3_dist(iv3_center(be->pos), e->pos); if (dd < bd) { bd = dd; best = 0; for (int k = 0; k < 27; k++) best += be->slots[k].count; } }
        return (f32)best;
    }
    if (!strcmp(what, "beitems") && arg) { ivec3 p; if (!parse_pos(arg, &p)) return -1; BlockEntity *be = world_block_entity(p, false, 0); int k = 0; if (be) for (int i = 0; i < 27; i++) k += be->slots[i].count; return (f32)k; }
    if (!strcmp(what, "lookhit")) return g_player.look.hit ? (f32)g_player.look.block : -1;
    if (!strcmp(what, "lookdist")) return g_player.look.dist;
    if (!strcmp(what, "mineprog")) return g_player.mine_progress;
    if (!strcmp(what, "pitch")) return e ? e->pitch : 0;
    if (!strcmp(what, "nearmob")) { f32 bd = 999; ENT_FOREACH(o) if (o->kind != ENT_PLAYER && e && v3_dist(o->pos, e->pos) < bd) bd = v3_dist(o->pos, e->pos); return bd; }
    if (!strcmp(what, "lookentity")) return (f32)g_player.look_entity;
    if (!strcmp(what, "instructure")) return e ? (f32)structure_at((int)floorf(e->pos.x), (int)floorf(e->pos.y), (int)floorf(e->pos.z)) : -1;
    if (!strcmp(what, "spawners")) { int k = 0; for (int i = 0; i < g_world.nbents; i++) k += g_world.bents[i].type == BE_SPAWNER; return (f32)k; }
    if (!strcmp(what, "lootchests")) { int k = 0; for (int i = 0; i < g_world.nbents; i++) { BlockEntity *be = &g_world.bents[i]; if (be->type != BE_CHEST && be->type != BE_BARREL) continue; for (int j = 0; j < 27; j++) if (!stack_empty(be->slots[j])) { k++; break; } } return (f32)k; }
    if (!strcmp(what, "tagged") && arg) { extern int tag_count(const char *); return (f32)tag_count(arg); }
    if (!strcmp(what, "gamemode")) return (f32)g_player.gamemode;
    if (!strcmp(what, "rain")) return g_level.rain_level;
    if (!strcmp(what, "raidwave")) return (f32)raid_wave();
    if (!strcmp(what, "raiders")) { int n = 0; ENT_FOREACH(o) if (o->kind == ENT_MOB && o->raid_id && !(o->raid_id >> 31)) n++; return (f32)n; }
    if (!strcmp(what, "patrol")) { int n = 0; ENT_FOREACH(o) if (o->kind == ENT_MOB && (o->raid_id >> 31)) n++; return (f32)n; }
    if (!strcmp(what, "riding")) return e && e->mount ? 1.0f : 0.0f;
    if (!strcmp(what, "level")) return (f32)g_player.xp_level;
    if (!strcmp(what, "ench") && arg) {   // ench <item>:<enchantment> -> best level on that item in the inventory
        char buf[96]; snprintf(buf, sizeof buf, "%s", arg);
        char *colon = strchr(buf, ':'); if (!colon) return 0; *colon = 0;
        int it = find_item(buf), en = 0, best = 0;
        for (int k = 1; k < ENCH_COUNT; k++) if (!strcmp(ENCHANTS[k].key, colon + 1)) en = k;
        for (int i = 0; i < INV_MAIN; i++) if (g_player.inv.slots[i].id == it) best = MAXI(best, en ? ench_level(g_player.inv.slots[i], en) : ench_count(g_player.inv.slots[i]));
        if (g_player.cursor.id == it) best = MAXI(best, en ? ench_level(g_player.cursor, en) : ench_count(g_player.cursor));
        return (f32)best;
    }
    if (!strcmp(what, "named") && arg) { for (int i = 0; i < INV_MAIN; i++) { const char *nm = item_name_get(g_player.inv.slots[i].name); if (nm && !strcmp(nm, arg)) return 1; } return 0; }
    if (!strcmp(what, "layers") && arg) { int it = find_item(arg), best = 0; for (int i = 0; i < INV_MAIN; i++) if (g_player.inv.slots[i].id == it) { int k = 0; for (int j = 0; j < MAX_ENCH; j++) k += g_player.inv.slots[i].ench[j][0] != 0; best = MAXI(best, k); } return (f32)best; }
    if (!strcmp(what, "mapexplored")) { for (int i = 0; i < INV_MAIN; i++) if (g_player.inv.slots[i].id == I_FILLED_MAP) return map_explored(g_player.inv.slots[i].data) * 100; return 0; }
    if (!strcmp(what, "mapscale")) { int best = -1; for (int i = 0; i < INV_MAIN; i++) if (g_player.inv.slots[i].id == I_FILLED_MAP) best = MAXI(best, map_scale(g_player.inv.slots[i].data)); return (f32)best; }
    if (!strcmp(what, "prof")) { Entity *best = NULL; f32 bd = 1e9f; ENT_FOREACH(o) if (o->kind == ENT_MOB && o->mob == MOB_VILLAGER && e && v3_dist(o->pos, e->pos) < bd) { bd = v3_dist(o->pos, e->pos); best = o; } return best ? (f32)best->prof : -1; }
    if (!strcmp(what, "vehicles")) { int k = 0; ENT_FOREACH(o) k += o->kind == ENT_VEHICLE; return (f32)k; }
    if (!strcmp(what, "vehiclex")) { ENT_FOREACH(o) if (o->kind == ENT_VEHICLE) return o->pos.x; return 0; }
    if (!strcmp(what, "vehiclez")) { ENT_FOREACH(o) if (o->kind == ENT_VEHICLE) return o->pos.z; return 0; }
    if (!strcmp(what, "is") && arg) {   // is x,y,z:key[:var]  -> 1 / 0
        char buf[128]; snprintf(buf, sizeof buf, "%s", arg);
        char *colon = strchr(buf, ':');
        if (!colon) return 0;
        *colon = 0;
        ivec3 p; if (!parse_pos(buf, &p)) return 0;
        Block b = world_get_block(p.x, p.y, p.z);
        char *key = colon + 1, *vs = strchr(key, ':');
        if (vs) *vs++ = 0;
        if (!strcmp(key, "air")) return block_is_air(b) ? 1.0f : 0.0f;
        if (!BLOCKS[b].key || strcmp(BLOCKS[b].key, key)) return 0;
        return (!vs || BLOCKS[b].var == atoi(vs)) ? 1.0f : 0.0f;
    }
    if (!strcmp(what, "var") && arg) { ivec3 p; if (!parse_pos(arg, &p)) return -1; return (f32)BLOCKS[world_get_block(p.x, p.y, p.z)].var; }
    if (!strcmp(what, "power") && arg) { ivec3 p; if (!parse_pos(arg, &p)) return -1; Block b = world_get_block(p.x, p.y, p.z); return (BLOCKS[b].base == B_REDSTONE_WIRE || BLOCKS[b].base == B_SCULK_SENSOR || BLOCKS[b].base == B_CALIBRATED_SCULK_SENSOR) ? (f32)BLOCKS[b].var : (f32)rs_input_power(p); }
    if (!strcmp(what, "fluid") && arg) { ivec3 p; if (!parse_pos(arg, &p)) return -1; Block b = world_get_block(p.x, p.y, p.z); return block_is_water(b) ? 1.0f : block_is_lava(b) ? 2.0f : 0.0f; }
    if (!strcmp(what, "health")) return e ? e->health : 0;
    if (!strcmp(what, "food")) return (f32)g_player.food;
    if (!strcmp(what, "level")) return (f32)g_player.xp_level;
    if (!strcmp(what, "xp")) return (f32)g_player.xp_total;
    if (!strcmp(what, "kills")) return (f32)g_player.kills;
    if (!strcmp(what, "dead")) return g_player.dead ? 1.0f : 0.0f;
    if (!strcmp(what, "screen")) return (f32)G.screen;
    if (!strcmp(what, "y")) return e ? e->pos.y : 0;
    if (!strcmp(what, "air")) return e ? (f32)e->air : 0;
    if (!strcmp(what, "onground")) return e && e->on_ground ? 1.0f : 0.0f;
    if (!strcmp(what, "hostile")) return (f32)g_ents.count_mobs_hostile;
    if (!strcmp(what, "mobs")) { int n = 0; ENT_FOREACH(o) if (o->kind == ENT_MOB) n++; return (f32)n; }
    if (!strcmp(what, "items")) { int n = 0; ENT_FOREACH(o) if (o->kind == ENT_ITEM) n++; return (f32)n; }
    if (!strcmp(what, "itemx")) { ENT_FOREACH(o) if (o->kind == ENT_ITEM) return o->pos.x; return -99999.0f; }   // x of the first dropped item
    if (!strcmp(what, "orbs")) { int n = 0; ENT_FOREACH(o) if (o->kind == ENT_XP) n++; return (f32)n; }
    if (!strcmp(what, "inv") && arg) return (f32)inventory_count(&g_player.inv, find_item(arg));
    if (!strcmp(what, "loaded")) return (f32)g_world.loaded;
    if (!strcmp(what, "fps")) return G.fps;
    if (!strcmp(what, "x")) return e ? e->pos.x : 0;
    if (!strcmp(what, "z")) return e ? e->pos.z : 0;
    if (!strcmp(what, "time")) return (f32)(world_daytime() % DAY_TICKS);
    if (!strcmp(what, "look")) return g_player.look.hit ? (f32)g_player.look.block : -1.0f;
    if (!strcmp(what, "dim")) return (f32)g_world.dim;
    if (!strcmp(what, "near") && arg && e) {   // near <block> [radius]: blocks of that kind (any state) around the player
        char key[64]; int r = 8; if (sscanf(arg, "%63[^,],%d", key, &r) < 1) return -1;   // near <block>,<radius>
        Block k = block_by_key(key); if (!k) { *ok = false; return -1; }
        ivec3 c = v3_floor(e->pos); int n = 0;
        for (int y = -r; y <= r; y++) for (int z = -r; z <= r; z++) for (int x = -r; x <= r; x++) if (BLOCKS[world_get_block(c.x + x, c.y + y, c.z + z)].base == BLOCKS[k].base) n++;
        return (f32)n;
    }
    if (!strcmp(what, "inbiome") && arg && e) return world_biome_at((int)floorf(e->pos.x), (int)floorf(e->pos.y), (int)floorf(e->pos.z)) == biome_by_key(arg) ? 1.0f : 0.0f;
    if (!strcmp(what, "ghostphases")) { extern int g_ghost_phases; return (f32)g_ghost_phases; }
    if (!strcmp(what, "phasing")) { int n = 0; ENT_FOREACH(o) if (o->kind == ENT_MOB && o->mob == MOB_GHOST && o->no_clip) n++; return (f32)n; }
    if (!strcmp(what, "mobhp") && arg && e) { int m = find_mob(arg); f32 bd = 1e9f, hp = -1; ENT_FOREACH(o) if (o->kind == ENT_MOB && o->mob == m && o->death_time == 0 && v3_dist(o->pos, e->pos) < bd) { bd = v3_dist(o->pos, e->pos); hp = o->health; } return hp; }
    if (!strcmp(what, "vely")) return e ? e->vel.y : 0;
    if (!strcmp(what, "siftret")) return (f32)g_world.sift_return.y;
    if (!strcmp(what, "bossphase")) { Entity *b = entity_get(g_ents.boss); return b ? (f32)b->phase : -1; }
    if (!strcmp(what, "bossmax")) { Entity *b = entity_get(g_ents.boss); return b ? b->max_health : -1; }
    if (!strcmp(what, "tyrantkilled")) return (f32)g_world.tyrant_killed;
    if (!strcmp(what, "siftgate")) return (f32)g_world.sift_gate.y;
    if (!strcmp(what, "tyrantpulls")) return (f32)g_tyrant_pulls;
    if (!strcmp(what, "tyrantsummons")) return (f32)g_tyrant_summons;
    if (!strcmp(what, "meteors")) return (f32)g_tyrant_meteors;
    if (!strcmp(what, "title")) return G.title[0] ? 1.0f : 0.0f;
    if (!strcmp(what, "keeperslams")) return (f32)g_keeper_slams;
    if (!strcmp(what, "trials")) return (f32)g_world.sift_trials;
    if (!strcmp(what, "vibrations")) return (f32)g_vibrations;
    if (!strcmp(what, "hookon") && arg) { ivec3 p; if (!parse_pos(arg, &p)) return -1; Block b = world_get_block(p.x, p.y, p.z); return BLOCKS[b].base == B_TRIPWIRE_HOOK ? (f32)(BLOCKS[b].var >= 4) : -1; }
    if (!strcmp(what, "wirestate") && arg) { ivec3 p; if (!parse_pos(arg, &p)) return -1; Block b = world_get_block(p.x, p.y, p.z); return BLOCKS[b].base == B_TRIPWIRE ? (f32)BLOCKS[b].var : -1; }
    if (!strcmp(what, "trips")) return (f32)g_tripwire_trips;
    if (!strcmp(what, "fishcaught")) return (f32)g_fish_caught;
    if (!strcmp(what, "sbscore") && arg) { char h[64]; const char *cm = strchr(arg, ','); if (!cm) return -1; snprintf(h, sizeof h, "%.*s", (int)(cm - arg), arg); int v = -99999; if (!sb_get(h, sb_obj(cm + 1), &v)) return -99999; return (f32)v; }
    if (!strcmp(what, "sidebar")) return (f32)(g_sb_sidebar >= 0);
    if (!strcmp(what, "advdone") && arg) return (f32)adv_done(adv_by_id(arg));
    if (!strcmp(what, "advcount")) { int got = 0; for (int i = 0; i < adv_count(); i++) got += adv_done(i); return (f32)got; }
    if (!strcmp(what, "stat") && arg) { for (int i = 0; i < STAT_COUNT; i++) { char k[40]; size_t j = 0; for (const char *q = STAT_NAMES[i]; *q && *q != ' ' && j < 39; q++) k[j++] = (char)tolower((unsigned char)*q); k[j] = 0; if (!strcmp(k, arg)) return (f32)g_stats[i]; } return -1; }
    if (!strcmp(what, "fuzzruns")) return (f32)g_fuzz_runs;
    if (!strcmp(what, "mobdist") && arg && e) { int t = -1; for (int i = 0; i < MOB_COUNT; i++) if (!strcmp(MOB_DEFS[i].key, arg)) t = i; f32 bd = 1e9f; ENT_FOREACH(o) if (o->kind == ENT_MOB && o->mob == t && o->death_time == 0) bd = fminf(bd, v3_dist(o->pos, e->pos)); return bd; }
    if (!strcmp(what, "villagerrep")) { int n = 0, sum = 0; ENT_FOREACH(o) if (o->kind == ENT_MOB && o->mob == MOB_VILLAGER) { sum += o->rep; n++; } return n ? (f32)sum / n : 0; }
    if (!strcmp(what, "minrep")) { int m = 99; ENT_FOREACH(o) if (o->kind == ENT_MOB && o->mob == MOB_VILLAGER) m = MINI(m, o->rep); return (f32)m; }
    if (!strcmp(what, "vilact")) { ENT_FOREACH(o) if (o->kind == ENT_MOB && o->mob == MOB_VILLAGER) return (f32)o->phase; return -1; }
    if (!strcmp(what, "vilanchor")) { ENT_FOREACH(o) if (o->kind == ENT_MOB && o->mob == MOB_VILLAGER) return o->anchor.y; return -1; }
    if (!strcmp(what, "daytime")) return (f32)((world_daytime() % DAY_TICKS) / MC_TICK_DIV);
    if (!strcmp(what, "sleeping")) { int n = 0; ENT_FOREACH(o) if (o->kind == ENT_MOB && o->mob == MOB_VILLAGER && o->sleeping) n++; return (f32)n; }
    if (!strcmp(what, "golemangry")) { int n = 0; ENT_FOREACH(o) if (o->kind == ENT_MOB && o->mob == MOB_IRON_GOLEM && o->anger > 0) n++; return (f32)n; }
    if (!strcmp(what, "keybind") && arg) { for (int k = 0; k < KB_COUNT; k++) if (!strcmp(KB_KEYS[k], arg)) return (f32)G.set.keys[k]; return -1; }
    if (!strcmp(what, "sneaking")) return (f32)g_player.sneaking;
    if (!strcmp(what, "lmb")) return (f32)g_win.mouse_down[0];
    if (!strcmp(what, "tutorial")) return (f32)G.set.tutorial;
    if (!strcmp(what, "frozen")) { extern int g_frozen_ents; return (f32)g_frozen_ents; }
    if (!strcmp(what, "rmb")) return (f32)g_win.mouse_down[1];
    if (!strcmp(what, "lookkind")) { Entity *t = entity_get(g_player.look_entity); return t ? (f32)t->kind : -1; }
    if (!strcmp(what, "pitch")) return e ? e->pitch * 57.2958f : 0;
    if (!strcmp(what, "padframes")) { extern int g_pad_frames; return (f32)g_pad_frames; }
    if (!strcmp(what, "selected")) return (f32)g_player.inv.selected;
    if (!strcmp(what, "captions")) return (f32)g_captions_total;
    if (!strcmp(what, "langindex")) return (f32)lang_index();
    if (!strcmp(what, "translated")) return strcmp(TR("Save and Quit to Title"), "Save and Quit to Title") ? 1.0f : 0.0f;
    if (!strcmp(what, "cbcolor")) return (f32)((ui_cb_color(rgba_u32(255, 40, 40, 255)) >> 8) & 255);   // green channel of red after the colour-blind filter
    if (!strcmp(what, "pathsfound")) { extern int g_path_found; return (f32)g_path_found; }
    if (!strcmp(what, "doorsopened")) { extern int g_doors_opened; return (f32)g_doors_opened; }
    if (!strcmp(what, "ambient")) { extern int g_ambient_plays; return (f32)g_ambient_plays; }
    if (!strcmp(what, "cavesounds")) { extern int g_cave_sounds; return (f32)g_cave_sounds; }
    if (!strcmp(what, "cavemood")) { extern bool g_cave_ready; return (f32)g_cave_ready; }
    if (!strcmp(what, "recovered")) { extern int g_plat_recovered; return (f32)g_plat_recovered; }
    if (!strcmp(what, "fileexists") && arg) { FILE *f = fopen(arg, "rb"); if (f) fclose(f); return f ? 1.0f : 0.0f; }
    if (!strcmp(what, "cmdruns")) { extern int g_cmdblock_runs; return (f32)g_cmdblock_runs; }
    if (!strcmp(what, "completions")) return (f32)g_complete_n;
    if (!strcmp(what, "completedlen")) return (f32)strlen(g_complete_buf);
    if (!strcmp(what, "forceloaded")) { Entity *p = player_ent(); return p ? (f32)forceload_has((int)floorf(p->pos.x) >> 4, (int)floorf(p->pos.z) >> 4) : 0; }
    if (!strcmp(what, "wbsize")) return (f32)g_wb.size;
    if (!strcmp(what, "armorcount")) { int n = 0; for (int i = 0; i < 4; i++) n += !stack_empty(g_player.inv.armor[i]); return (f32)n; }
    if (!strcmp(what, "invcount")) { int n = 0; for (int i = 0; i < 36; i++) n += !stack_empty(g_player.inv.slots[i]); return (f32)n; }
    if (!strcmp(what, "blocksshot")) return (f32)g_blocks_shot;
    if (!strcmp(what, "crammed")) return (f32)g_cramming_hits;
    if (!strcmp(what, "effectdeaths")) return (f32)g_effect_deaths;
    if (!strcmp(what, "phantoms")) return (f32)g_phantoms_spawned;
    if (!strcmp(what, "traders")) return (f32)g_traders_spawned;
    if (!strcmp(what, "recipesunlocked")) { extern int g_recipes_unlocked; return (f32)g_recipes_unlocked; }
    if (!strcmp(what, "craftable")) { extern ItemStack screens_craft_preview(void); return (f32)screens_craft_preview().id; }
    if (!strcmp(what, "resttime")) return (f32)g_player.rest_ticks;
    if (!strcmp(what, "tradertimer")) return (f32)g_level.trader_timer;
    if (!strcmp(what, "phantomtimer")) { extern int g_phantom_timer; return (f32)g_phantom_timer; }
    if (!strcmp(what, "difficulty")) return (f32)g_level.difficulty;
    if (!strcmp(what, "decor") && arg) { int t = decor_type_by_key(arg), n = 0; ENT_FOREACH(o) if (o->kind == ENT_DECOR && !o->remove && (int)o->vtype == t) n++; return (f32)n; }
    if (!strcmp(what, "framesignal") && arg) { ivec3 p; if (!parse_pos(arg, &p)) return -1; return (f32)decor_comparator(p); }
    if (!strcmp(what, "comparatorin") && arg) { ivec3 p; if (!parse_pos(arg, &p)) return -1; return (f32)rs_container_signal(p); }
    if (!strcmp(what, "standequip")) { int n = 0; ENT_FOREACH(o) if (o->kind == ENT_DECOR && o->vtype == DECOR_STAND) for (int k = 0; k < 4; k++) n += !stack_empty(o->equip[k]); return (f32)n; }
    if (!strcmp(what, "standpose")) { ENT_FOREACH(o) if (o->kind == ENT_DECOR && o->vtype == DECOR_STAND) return (f32)o->pose; return -1; }
    if (!strcmp(what, "paintingarea")) { int best = 0; ENT_FOREACH(o) if (o->kind == ENT_DECOR && o->vtype == DECOR_PAINTING) best = MAXI(best, PAINTINGS[o->color % PAINTING_COUNT].w * PAINTINGS[o->color % PAINTING_COUNT].h); return (f32)best; }
    if (!strcmp(what, "fishhooked")) return (f32)g_fish_hooked;
    if (!strcmp(what, "bobber")) { Entity *b = entity_get(g_player.bobber); if (!b) return 0; return b->target ? 3 : b->stuck ? 4 : (block_is_water(world_get_block((int)floorf(b->pos.x), (int)floorf(b->pos.y - 0.15f), (int)floorf(b->pos.z))) ? 2 : 1); }
    if (!strcmp(what, "bitewait")) { Entity *b = entity_get(g_player.bobber); return b ? (f32)b->special_timer : -1; }
    if (!strcmp(what, "helddamage")) return (f32)inventory_held(&g_player.inv)->damage;
    if (!strcmp(what, "pistonmoves")) { extern int g_piston_moves; return (f32)g_piston_moves; }
    if (!strcmp(what, "pistonblocked")) { extern int g_piston_blocked; return (f32)g_piston_blocked; }
    if (!strcmp(what, "sensortriggers")) return (f32)g_sensor_triggers;
    if (!strcmp(what, "shrieks")) return (f32)g_shrieks;
    if (!strcmp(what, "wardenwarning")) return (f32)g_world.warden_warning;
    if (!strcmp(what, "wardenssummoned")) return (f32)g_wardens_summoned;
    if (!strcmp(what, "sculkspread")) return (f32)g_sculk_spread;
    if (!strcmp(what, "holysplashes")) return (f32)g_holy_splashes;
    if (!strcmp(what, "ghostrepels")) return (f32)g_ghost_repels;
    if (!strcmp(what, "revenantnovas")) return (f32)g_revenant_novas;
    if (!strcmp(what, "pearlphases")) return (f32)g_pearl_phases;
    if (!strcmp(what, "hellsounds")) return (f32)g_hell_ambience;
    if (!strcmp(what, "glowghosts")) { int n = 0; ENT_FOREACH(o) if (o->kind == ENT_MOB && MOB_DEFS[o->mob].ai == AIK_GHOST && effect_has(o, EFF_GLOWING)) n++; return (f32)n; }
    if (!strcmp(what, "blessed")) return e ? (f32)(effect_level(e, EFF_BLESSING_OF_LIGHT) + effect_level(e, EFF_BLESSING_OF_THE_SKY) + effect_level(e, EFF_BLESSING_OF_VIGOR)) : 0;
    if (!strcmp(what, "ascended")) return (f32)g_world.ascended;
    if (!strcmp(what, "soultalks")) return (f32)g_soul_talks;
    if (!strcmp(what, "eruptions")) return (f32)g_avatar_eruptions;
    if (!strcmp(what, "credits")) return G.screen == SCR_CREDITS ? 1.0f : 0.0f;
    if (!strcmp(what, "guide")) return G.screen == SCR_GUIDE ? 1.0f : 0.0f;
    if (!strcmp(what, "ucitems")) return (f32)UC_ITEM_COUNT;
    if (!strcmp(what, "screen")) return (f32)G.screen;
    if (!strcmp(what, "maxhp")) return e ? e->max_health : 0;
    if (!strcmp(what, "have") && arg) { int id = find_item(arg), n = inventory_count(&g_player.inv, id); ENT_FOREACH(o) if (o->kind == ENT_ITEM && o->item.id == id) n += o->item.count; return (f32)n; }
    if (!strcmp(what, "dropped") && arg) { int id = find_item(arg), n = 0; ENT_FOREACH(o) if (o->kind == ENT_ITEM && o->item.id == id) n += o->item.count; return (f32)n; }
    if (!strcmp(what, "offers")) {   // trade offers of the nearest trader-like mob
        Entity *best = NULL; f32 bd = 1e9f;
        ENT_FOREACH(o) if (o->kind == ENT_MOB && (o->mob == MOB_VILLAGER || o->mob == MOB_WANDERING_TRADER || o->mob == MOB_DELVARI_MERCHANT || o->mob == MOB_SIFT_SOUL || o->mob == MOB_GOD_ZEPHYRINE) && e && v3_dist(o->pos, e->pos) < bd) { bd = v3_dist(o->pos, e->pos); best = o; }
        int n = 0; if (best) villager_offers(best, &n); return (f32)n;
    }
    if (!strcmp(what, "biome")) return e ? (f32)world_biome_at((int)floorf(e->pos.x), (int)floorf(e->pos.y), (int)floorf(e->pos.z)) : -1;
    if (!strcmp(what, "block") && arg) { ivec3 p; if (parse_pos(arg, &p)) return (f32)world_get_block(p.x, p.y, p.z); }
    if (!strcmp(what, "effect") && arg && e) { for (int i = 1; i < EFFECT_COUNT; i++) { char k[48]; lower_compact(EFFECT_NAMES[i], k, sizeof k); char w2[48]; lower_compact(arg, w2, sizeof w2); if (!strcmp(k, w2)) return (f32)effect_level(e, i); } return 0; }
    if (!strcmp(what, "mob") && arg) { int m = find_mob(arg), n = 0; ENT_FOREACH(o) if (o->kind == ENT_MOB && o->mob == m && o->death_time == 0) n++; return (f32)n; }
    if (!strcmp(what, "recipes_ok")) {   // every recipe must produce its own result when laid out exactly
        int good = 0;
        for (int i = 0; i < RECIPE_COUNT; i++) {
            const RecipeDef *r = &RECIPES[i];
            ItemStack grid[9] = {0};
            for (int k = 0; k < (r->shapeless ? r->w : r->w * r->h); k++) {
                int ing = r->ing[k];
                if (!ing) continue;
                int x = r->shapeless ? k % 3 : k % r->w, y = r->shapeless ? k / 3 : k / r->w;
                grid[y * 3 + x] = stack_of(ING_ITEMS[INGREDIENTS[ing].off], 1);
            }
            ItemStack out = crafting_match(grid, 3, 3);
            if (out.id == r->result) good++;
            else LOG_WARN("recipe %d (%s) produced %s", i, ITEMS[r->result].key, out.id ? ITEMS[out.id].key : "nothing");
        }
        return (f32)good;
    }
    if (!strcmp(what, "recipes")) return (f32)RECIPE_COUNT;
    if (!strcmp(what, "itemtypes")) { int n = 0; for (int i = 1; i < ITEM_MAX; i++) n += item_valid(i); return (f32)n; }
    if (!strcmp(what, "biomes_found") && arg) {   // how many distinct overworld biomes appear in a radius (blocks) around the player
        int r = atoi(arg), step = 64; bool seen[BIOME_COUNT] = {0}; int n = 0;
        int cx = e ? (int)e->pos.x : 0, cz = e ? (int)e->pos.z : 0;
        for (int z = -r; z <= r; z += step) for (int x = -r; x <= r; x += step) {
            int b = worldgen_biome(g_world.seed, g_world.dim, cx + x, cz + z);
            if (!seen[b]) { seen[b] = true; n++; }
        }
        for (int b = 0; b < BIOME_COUNT; b++) if (!seen[b] && BIOMES[b].dim == g_world.dim && BIOMES[b].category != 4) LOG_INFO("biome not found within %d: %s", r, BIOMES[b].key);
        return (f32)n;
    }
    *ok = false;
    return 0;
}

bool command_execute(const char *line) {
    while (*line == '/' || *line == ' ') line++;
    {   bool handled = false;
        bool r = mc_command(line, &handled);   // Minecraft syntax first (selectors, ~ ^, /execute ...)
        if (handled) return r || true;
    }
    char cmd[32] = "", a[64] = "", b[64] = "", c[64] = "", d[64] = "";
    int n = sscanf(line, "%31s %63s %63s %63s %63s", cmd, a, b, c, d);
    if (n < 1) return false;
    Entity *e = player_ent();
    u32 ok_col = rgba_u32(170, 170, 170, 255);

    if (!strcmp(cmd, "help")) {
        game_message(rgba_u32(255, 255, 85, 255), "Commands: /give <item> [n], /summon <mob> [n], /time set day|night|<t>,");
        game_message(rgba_u32(255, 255, 85, 255), "/gamemode survival|creative, /tp x y z, /kill, /heal, /feed, /xp <n>,");
        game_message(rgba_u32(255, 255, 85, 255), "/seed, /spawnpoint, /setblock x y z <block>, /save, /killall, /fly, /biome,");
        game_message(rgba_u32(255, 255, 85, 255), "/locate biome <id> | stronghold, /dimension overworld|nether|end, /effect <id> [s] [lvl] | clear");
        return true;
    }
    if (!strcmp(cmd, "give") && n >= 2) {
        int id = find_item(a);
        if (!id) { game_message(rgba_u32(255, 85, 85, 255), "Unknown item '%s'", a); return true; }
        int count = n >= 3 ? clampi(atoi(b), 1, 64 * 36) : 1;
        int dmg = 0;
        if (n >= 4 && ITEMS[id].variants) { for (int i = 0; i < POTION_COUNT; i++) if (!strcmp(POTIONS[i].key, c)) dmg = i; }
        while (count > 0) { int k = MINI(count, ITEMS[id].max_stack); ItemStack s = stack_of(id, k); s.damage = (u16)dmg; player_give(s); count -= k; }
        game_message(ok_col, "Gave %s", ITEMS[id].name);
        return true;
    }
    if (!strcmp(cmd, "summon") && n >= 2 && e) {
        int m = find_mob(a);
        if (m < 0) { game_message(rgba_u32(255, 85, 85, 255), "Unknown mob '%s'", a); return true; }
        int count = n >= 3 ? clampi(atoi(b), 1, 50) : 1;
        vec3 fwd = v3(sinf(e->yaw), 0, cosf(e->yaw));
        f32 dist = n >= 4 ? (f32)atof(c) : 4.0f;
        for (int i = 0; i < count; i++) {
            f32 jit = count > 1 ? 1.0f : 0.0f;   // a single mob goes exactly where you look
            vec3 p = v3_add(e->pos, v3_add(v3_scale(fwd, dist), v3(frand_range(-jit, jit), 0.2f, frand_range(-jit, jit))));
            // first free spot at or above the player's level (not the top of the column: trees overhang)
            p.y = floorf(e->pos.y);
            for (int k = 0; k < 24; k++) {
                int bx = (int)floorf(p.x), by = (int)p.y, bz = (int)floorf(p.z);
                if (!BLOCKS[world_get_block(bx, by, bz)].solid && !BLOCKS[world_get_block(bx, by + 1, bz)].solid) break;
                p.y += 1;
            }
            Entity *m2 = mob_spawn(m, p);
            if (m2) { m2->persistent = true; m2->yaw = m2->body_yaw = e->yaw + PI_F; }
        }
        game_message(ok_col, "Summoned %d %s", count, MOB_DEFS[m].name);
        return true;
    }
    if (!strcmp(cmd, "time") && n >= 3) {
        u64 day = g_world.time - g_world.time % DAY_TICKS;
        u64 t;
        if (!strcmp(b, "day")) t = DAY_TICKS / 24;
        else if (!strcmp(b, "noon")) t = DAY_TICKS / 4;
        else if (!strcmp(b, "sunset")) t = DAY_TICKS / 2 - DAY_TICKS / 40;
        else if (!strcmp(b, "night")) t = DAY_TICKS * 5 / 8;
        else if (!strcmp(b, "midnight")) t = DAY_TICKS * 3 / 4;
        else t = (u64)strtoull(b, NULL, 10);
        g_world.time = !strcmp(a, "add") ? g_world.time + t : day + t;
        game_message(ok_col, "Time set");
        return true;
    }
    if (!strcmp(cmd, "gamemode") && n >= 2) {
        g_player.gamemode = (!strcmp(a, "creative") || !strcmp(a, "1") || !strcmp(a, "c")) ? GM_CREATIVE : GM_SURVIVAL;
        if (g_player.gamemode == GM_SURVIVAL) g_player.flying = false;
        game_message(ok_col, "Game mode set to %s", g_player.gamemode ? "Creative" : "Survival");
        return true;
    }
    if (!strcmp(cmd, "tp") && n >= 4 && e) {
        // supports Minecraft relative coordinates: /tp ~ ~20 ~
        f32 x = a[0] == '~' ? e->pos.x + (f32)atof(a + 1) : (f32)atof(a);
        f32 y = b[0] == '~' ? e->pos.y + (f32)atof(b + 1) : (f32)atof(b);
        f32 z = c[0] == '~' ? e->pos.z + (f32)atof(c + 1) : (f32)atof(c);
        e->pos = e->prev_pos = v3(x, y, z);
        e->vel = v3(0, 0, 0); e->fall_dist = 0;
        return true;
    }
    if (!strcmp(cmd, "kill") && e) { e->invuln = 0; int gm = g_player.gamemode; g_player.gamemode = GM_SURVIVAL; entity_damage(e, 1000, DMG_VOID, NULL); g_player.gamemode = gm; return true; }
    if (!strcmp(cmd, "killall")) { int k = 0; ENT_FOREACH(o) if (o->kind == ENT_MOB) { o->remove = true; k++; } game_message(ok_col, "Removed %d mobs", k); return true; }
    if (!strcmp(cmd, "heal") && e) { e->health = e->max_health; e->fire = 0; return true; }
    if (!strcmp(cmd, "feed")) { g_player.food = 20; g_player.saturation = 5; return true; }
    if (!strcmp(cmd, "hurt") && e && n >= 2) { e->invuln = 0; entity_damage(e, (f32)atof(a), DMG_GENERIC, NULL); return true; }
    if (!strcmp(cmd, "starve") && n >= 2) { g_player.food = clampi(atoi(a), 0, 20); g_player.saturation = 0; return true; }
    if (!strcmp(cmd, "xp") && n >= 2) { player_add_xp(atoi(a)); return true; }
    if (!strcmp(cmd, "seed")) { game_message(ok_col, "Seed: %llu", (unsigned long long)g_world.seed); return true; }
    if (!strcmp(cmd, "fly")) { if (g_player.gamemode == GM_CREATIVE) g_player.flying = !g_player.flying; return true; }
    if (!strcmp(cmd, "spawnpoint") && e) { g_world.spawn = v3_floor(e->pos); game_message(ok_col, "Spawn point set"); return true; }
    if (!strcmp(cmd, "setblock") && n >= 5) {
        int blk = find_block(d);
        if (blk < 0) { game_message(rgba_u32(255, 85, 85, 255), "Unknown block '%s'", d); return true; }
        ivec3 base = e ? v3_floor(e->pos) : iv3(0, 0, 0);
        int x = a[0] == '~' ? base.x + atoi(a + 1) : atoi(a);
        int y = b[0] == '~' ? base.y + atoi(b + 1) : atoi(b);
        int z = c[0] == '~' ? base.z + atoi(c + 1) : atoi(c);
        world_set_block(x, y, z, (u16)blk);
        return true;
    }
    if (!strcmp(cmd, "enchant") && n >= 2) {   // enchant <name> [level] : enchants the held item (any combination allowed, like /enchant with commands)
        ItemStack *h = inventory_held(&g_player.inv);
        int en = 0;
        for (int k = 1; k < ENCH_COUNT; k++) if (!strcmp(ENCHANTS[k].key, a) || !strcmp(ENCHANTS[k].key + 0, a)) en = k;
        if (!en) { game_message(rgba_u32(255, 85, 85, 255), "Unknown enchantment '%s'", a); return true; }
        if (stack_empty(*h)) { game_message(rgba_u32(255, 85, 85, 255), "Hold an item to enchant"); return true; }
        int lvl = n >= 3 ? clampi(atoi(b), 1, 10) : 1;
        if (h->id == I_BOOK) h->id = I_ENCHANTED_BOOK;
        if (!ench_add(h, en, lvl)) game_message(rgba_u32(255, 85, 85, 255), "No room for more enchantments");
        else game_message(ok_col, "Applied %s %s", ENCHANTS[en].name, ench_roman(lvl));
        return true;
    }
    if (!strcmp(cmd, "book") && n >= 2) {   // book <enchantment> [level] : gives an enchanted book
        int en = 0;
        for (int k = 1; k < ENCH_COUNT; k++) if (!strcmp(ENCHANTS[k].key, a)) en = k;
        if (!en) return true;
        ItemStack bk = stack_of(I_ENCHANTED_BOOK, 1);
        ench_add(&bk, en, n >= 3 ? clampi(atoi(b), 1, 10) : ENCHANTS[en].max_level);
        player_give(bk);
        return true;
    }
    // ---- test / power-user helpers for the workstation, trading and riding systems
    if (!strcmp(cmd, "open") && n >= 4) {   // open x y z : open the interface of the block there
        ivec3 base = e ? v3_floor(e->pos) : iv3(0, 0, 0);
        ivec3 p = iv3(a[0] == '~' ? base.x + atoi(a + 1) : atoi(a), b[0] == '~' ? base.y + atoi(b + 1) : atoi(b), c[0] == '~' ? base.z + atoi(c + 1) : atoi(c));
        const BlockDef *bd = &BLOCKS[world_get_block(p.x, p.y, p.z)];
        G.open_be = p; G.open_ender = false;
        switch (bd->ui) {
        case UI_ENCHANT: game_open_screen(SCR_ENCHANT); break;
        case UI_ANVIL: game_open_screen(SCR_ANVIL); break;
        case UI_GRINDSTONE: game_open_screen(SCR_GRINDSTONE); break;
        case UI_LOOM: game_open_screen(SCR_LOOM); break;
        case UI_CARTOGRAPHY: game_open_screen(SCR_CARTOGRAPHY); break;
        case UI_CRAFTING: game_open_screen(SCR_CRAFTING); break;
        case UI_CHEST: world_block_entity(p, true, bd->be ? bd->be : BE_CHEST); game_open_screen(SCR_CHEST); break;
        default: break;
        }
        return true;
    }
    if (!strcmp(cmd, "gridinv") && n >= 3) {   // gridinv <craft slot> <inventory slot> : move a whole stack into an input slot
        int cs = clampi(atoi(a), 0, 8), is = clampi(atoi(b), 0, INV_MAIN - 1);
        if (stack_empty(g_player.craft[cs])) { g_player.craft[cs] = g_player.inv.slots[is]; g_player.inv.slots[is] = (ItemStack){0}; }
        return true;
    }
    if (!strcmp(cmd, "mark") && e) { mark_pos = e->pos; return true; }
    if (!strcmp(cmd, "fillbe") && n >= 5) {   // fillbe x y z <item>[*n] : put items into a block entity's first free slot
        ivec3 base = e ? v3_floor(e->pos) : iv3(0, 0, 0);
        ivec3 p = iv3(a[0] == '~' ? base.x + atoi(a + 1) : atoi(a), b[0] == '~' ? base.y + atoi(b + 1) : atoi(b), c[0] == '~' ? base.z + atoi(c + 1) : atoi(c));
        Block bl = world_get_block(p.x, p.y, p.z);
        if (BLOCKS[bl].be == BE_NONE) return true;
        BlockEntity *be = world_block_entity(p, true, BLOCKS[bl].be);
        char key[64]; snprintf(key, sizeof key, "%s", d);
        char *star = strchr(key, '*'); int cnt = 1;
        if (star) { *star = 0; cnt = clampi(atoi(star + 1), 1, 64); }
        for (int i = 0; i < 27; i++) if (stack_empty(be->slots[i])) { be->slots[i] = stack_of(find_item(key), cnt); break; }
        return true;
    }
    if (!strcmp(cmd, "dig") && n >= 4) {   // dig x y z : break a block as the player would (drops included)
        ivec3 base = e ? v3_floor(e->pos) : iv3(0, 0, 0);
        ivec3 p = iv3(a[0] == '~' ? base.x + atoi(a + 1) : atoi(a), b[0] == '~' ? base.y + atoi(b + 1) : atoi(b), c[0] == '~' ? base.z + atoi(c + 1) : atoi(c));
        extern void player_break_block_for_tests(ivec3 p);
        player_break_block_for_tests(p);
        return true;
    }
    if (!strcmp(cmd, "enchantopt") && n >= 2) { screens_enchant_option(atoi(a)); return true; }
    if (!strcmp(cmd, "takeresult")) { screens_take_result(true); return true; }
    if (!strcmp(cmd, "anvilname")) { snprintf(G.anvil_name, sizeof G.anvil_name, "%s", n >= 2 ? a : ""); return true; }
    if (!strcmp(cmd, "loomsel") && n >= 2) { G.loom_sel = atoi(a); return true; }
    if (!strcmp(cmd, "recipebook")) { G.recipe_book = n >= 2 ? atoi(a) != 0 : !G.recipe_book; return true; }
    if (!strcmp(cmd, "recipefill") && n >= 2) {   // recipefill <result item> : fill the grid like clicking it in the recipe book
        int want = find_item(a);
        for (int r = 0; r < RECIPE_COUNT; r++) if (RECIPES[r].result == want && screens_recipe_fill(r, n >= 3)) break;
        return true;
    }
    if (!strcmp(cmd, "trade")) {   // trade : open trading with the nearest villager
        Entity *best = NULL; f32 bd = 1e9f;
        ENT_FOREACH(o) if (o->kind == ENT_MOB && (o->mob == MOB_VILLAGER || o->mob == MOB_WANDERING_TRADER || o->mob == MOB_DELVARI_MERCHANT || o->mob == MOB_SIFT_SOUL || o->mob == MOB_GOD_ZEPHYRINE) && e && v3_dist(o->pos, e->pos) < bd) { bd = v3_dist(o->pos, e->pos); best = o; }
        if (best) { G.trade_with = best->id; G.trade_sel = -1; game_open_screen(SCR_TRADE); }
        return true;
    }
    if (!strcmp(cmd, "tradesel") && n >= 2) { screens_trade_select(atoi(a)); return true; }
    if (!strcmp(cmd, "profession") && n >= 2) {   // profession <name> : sets the nearest villager's job
        Entity *best = NULL; f32 bd = 1e9f;
        ENT_FOREACH(o) if (o->kind == ENT_MOB && o->mob == MOB_VILLAGER && e && v3_dist(o->pos, e->pos) < bd) { bd = v3_dist(o->pos, e->pos); best = o; }
        if (best) for (int k = 0; k < PROF_COUNT; k++) { char nm[32]; lower_compact(PROF_NAMES[k], nm, sizeof nm); if (!strcmp(nm, a)) villager_set_profession(best, k); }
        return true;
    }
    if (!strcmp(cmd, "vehicle") && n >= 2 && e) {   // vehicle <item> [mount] : put a boat / minecart at the player
        int vt = vehicle_type_of_item(find_item(a));
        Entity *v = vt >= 0 ? vehicle_spawn(vt, e->pos, e->yaw) : NULL;
        if (v) { v->item = stack_of(find_item(a), 1); if (n >= 3) ride_mount(e, v); }
        return true;
    }
    if (!strcmp(cmd, "mount") && e) {   // mount : ride the nearest rideable mob or vehicle (taming & saddling it)
        Entity *best = NULL; f32 bd = 1e9f;
        ENT_FOREACH(o) if (((o->kind == ENT_MOB && mob_rideable(o->mob)) || o->kind == ENT_VEHICLE) && !o->rider && v3_dist(o->pos, e->pos) < bd) { bd = v3_dist(o->pos, e->pos); best = o; }
        if (best) { if (best->kind == ENT_MOB) { best->tamed = true; best->saddled = true; } ride_mount(e, best); }
        return true;
    }
    if (!strcmp(cmd, "dismount") && e) { ride_dismount(e); return true; }
    if (!strcmp(cmd, "useitem") && e) { extern void player_use_held_for_tests(void); player_use_held_for_tests(); return true; }
    if (!strcmp(cmd, "activate") && n >= 4) {   // activate x y z : flip a lever / press a button (like a right click)
        ivec3 base = e ? v3_floor(e->pos) : iv3(0, 0, 0);
        int x = a[0] == '~' ? base.x + atoi(a + 1) : atoi(a), y = b[0] == '~' ? base.y + atoi(b + 1) : atoi(b), z = c[0] == '~' ? base.z + atoi(c + 1) : atoi(c);
        Block bl = world_get_block(x, y, z);
        if (BLOCKS[bl].base == B_LEVER) rs_toggle_lever(iv3(x, y, z));
        else if (BLOCKS[bl].base == B_TRIPWIRE && inventory_held(&g_player.inv)->id == I_SHEARS) world_set_block(x, y, z, B_TRIPWIRE + 2);   // shears disarm string
        else if (BLOCKS[bl].key && strstr(BLOCKS[bl].key, "_button")) rs_press_button(iv3(x, y, z));
        return true;
    }
    if (!strcmp(cmd, "fill") && n >= 2 && e) {   // fill <block> : a 5x1x5 platform under the player (handy for tests)
        int blk = find_block(a);
        if (blk < 0) return true;
        ivec3 p = v3_floor(e->pos);
        for (int dz = -2; dz <= 2; dz++) for (int dx = -2; dx <= 2; dx++) world_set_block(p.x + dx, p.y - 1, p.z + dz, (u16)blk);
        return true;
    }
    if (!strcmp(cmd, "flatten") && e) {   // flatten <r>: open arena around the player (handy for tests)
        int r = n >= 2 ? clampi(atoi(a), 1, 24) : 8;
        ivec3 p = v3_floor(e->pos);
        for (int dz = -r; dz <= r; dz++) for (int dx = -r; dx <= r; dx++) {
            for (int y = p.y; y < MINI(WORLD_H, p.y + 12); y++) if (world_get_block(p.x + dx, y, p.z + dz)) world_set_block_silent(p.x + dx, y, p.z + dz, B_AIR);
            world_set_block_silent(p.x + dx, p.y - 1, p.z + dz, B_GRASS_BLOCK);
        }
        return true;
    }
    if (!strcmp(cmd, "furnace") && n >= 3 && e) {   // furnace <input> <fuel>: furnace 2 blocks ahead, loaded with 8 + 8
        ivec3 p = v3_floor(v3_add(e->pos, v3(sinf(e->yaw) * 2, 0, cosf(e->yaw) * 2)));
        world_set_block(p.x, p.y, p.z, B_FURNACE);
        BlockEntity *be = world_block_entity(p, true, BE_FURNACE);
        be->slots[0] = stack_of(find_item(a), 8); be->slots[1] = stack_of(find_item(b), 8);
        G.open_be = p;
        return true;
    }
    if (!strcmp(cmd, "takefurnace")) {
        BlockEntity *be = world_block_entity(G.open_be, false, 0);
        if (be && !stack_empty(be->slots[2])) { player_give(be->slots[2]); be->slots[2] = (ItemStack){0}; }
        return true;
    }
    if (!strcmp(cmd, "eatnow")) { g_player.eating_ticks = 95; return true; }
    if (!strcmp(cmd, "clear")) { memset(&g_player.inv, 0, sizeof g_player.inv); return true; }
    if (!strcmp(cmd, "grid") && n >= 2) {   // grid <slot 0-8> <item>: put one item into the crafting grid (tests)
        int s = clampi(atoi(a), 0, 8), id = n >= 3 ? find_item(b) : 0;
        g_player.craft[s] = id ? stack_of(id, n >= 4 ? atoi(c) : 1) : (ItemStack){0};
        return true;
    }
    if (!strcmp(cmd, "select") && n >= 2) { g_player.inv.selected = clampi(atoi(a), 0, 8); return true; }
    if (!strcmp(cmd, "save")) { game_save(); game_message(ok_col, "Saved the game"); return true; }
    if (!strcmp(cmd, "screenshot")) {
        char path[200];
        plat_mkdirs("screenshots");
        if (n >= 2) snprintf(path, sizeof path, "screenshots/%s.png", a);
        else snprintf(path, sizeof path, "screenshots/shot_%llu.png", (unsigned long long)G.frames);
        G.frames++;  // unique name
        extern char g_pending_screenshot[256];
        snprintf(g_pending_screenshot, 256, "%s", path);
        return true;
    }
    if ((!strcmp(cmd, "decoruse") || !strcmp(cmd, "decorhit")) && n >= 2 && e) {   // tests: right-click / hit the nearest decoration of a type
        int t = decor_type_by_key(a); Entity *best = NULL; f32 bd = 12;
        ENT_FOREACH(o) if (o->kind == ENT_DECOR && !o->remove && (int)o->vtype == t && v3_dist(o->pos, e->pos) < bd) { bd = v3_dist(o->pos, e->pos); best = o; }
        if (!best) return true;
        if (cmd[5] == 'h') decor_hit(best, e);
        else { bool sn = g_player.sneaking; g_player.sneaking = n >= 3 && !strcmp(b, "sneak"); decor_interact(best, inventory_held(&g_player.inv)); g_player.sneaking = sn; }
        return true;
    }
    if (!strcmp(cmd, "cmdblock") && n >= 5 && e) {   // cmdblock x y z <command...>: set a command block's command (tests)
        ivec3 base = v3_floor(e->pos);
        ivec3 p = iv3(a[0] == '~' ? base.x + atoi(a + 1) : atoi(a), b[0] == '~' ? base.y + atoi(b + 1) : atoi(b), c[0] == '~' ? base.z + atoi(c + 1) : atoi(c));
        const char *rest = line; for (int sp = 0; sp < 4 && rest; sp++) { rest = strchr(rest, ' '); if (rest) rest++; }
        if (rest) cmdblock_set(p, rest);
        return true;
    }
    if (!strcmp(cmd, "complete") && n >= 2) {   // complete <partial command>: Tab completion (tests)
        const char *rest = strchr(line, ' ');
        snprintf(g_complete_buf, sizeof g_complete_buf, "/%s", rest ? rest + 1 : "");
        g_complete_n = command_complete(g_complete_buf, sizeof g_complete_buf, G.chat_hint, sizeof G.chat_hint);
        game_message(COL_WHITE, "completed: '%s' (%d matches)", g_complete_buf, g_complete_n);
        return true;
    }
    if (!strcmp(cmd, "advancement") && n >= 2) {   // advancement grant|revoke <id>|everything, advancement list
        if (!strcmp(a, "list")) { int got = 0; for (int i = 0; i < adv_count(); i++) got += adv_done(i); game_message(COL_WHITE, "%d of %d advancements made", got, adv_count()); return true; }
        bool grant = !strcmp(a, "grant");
        const char *id = n >= 4 ? c : b;   // "advancement grant @s only <id>" or "advancement grant <id>"
        if (!strcmp(id, "everything") || (n >= 3 && !strcmp(b, "everything"))) { for (int i = 0; i < adv_count(); i++) { if (grant) adv_grant(i); else adv_revoke(i); } return true; }
        int i = adv_by_id(strncmp(id, "ultimatecraft:", 14) ? id : id + 14);
        if (i < 0) { game_message(rgba_u32(255, 85, 85, 255), "Unknown advancement '%s'", id); return true; }
        if (grant) adv_grant(i); else adv_revoke(i);
        return true;
    }
    if (!strcmp(cmd, "advancements")) { game_open_screen(SCR_ADVANCEMENTS); return true; }
    if (!strcmp(cmd, "controls")) { G.rebind = -1; G.controls_back = SCR_NONE; game_open_screen(SCR_CONTROLS); return true; }
    if (!strcmp(cmd, "statistics")) { game_open_screen(SCR_STATS); return true; }
    if (!strcmp(cmd, "tmpswap") && n >= 2) {   // tests: leave only "<file>.tmp" (as if the game crashed between write and rename)
        char a1[400], b1[420]; snprintf(a1, sizeof a1, "%.300s/%.80s", g_world.dir, a); snprintf(b1, sizeof b1, "%s.tmp", a1);
        remove(b1); rename(a1, b1); return true;
    }
    if (!strcmp(cmd, "fuzzcommands") && n >= 2) {   // tests: run N random command lines; the game must survive all of them
        static const char *W[] = {"give", "summon", "setblock", "fill", "tp", "kill", "effect", "execute", "scoreboard", "data", "item", "loot", "function", "worldborder",
            "spreadplayers", "fillbiome", "forceload", "attribute", "gamerule", "time", "weather", "clone", "damage", "enchant", "title", "tag", "ride", "particle", "locate",
            "@s", "@e[type=pig,limit=2]", "@e[distance=..5]", "@p", "@a[", "~", "~-3", "^1", "0", "-1", "99999999", "1e9", "-0.5", "nan", "diamond", "zombie", "stone",
            "run", "as", "at", "if", "unless", "store", "result", "score", "players", "objectives", "add", "set", "{Health:5f}", "[", "]", "{", "}", "\"", "minecraft:", "x",
            "matches", "1..", "..", "*=", "/=", "%=", "><", "replace", "with", "armor.head", "container.99", "#tag", "", "a b c", "ultimatecraft:hello", "sidebar", "center"};
        u64 seed = n >= 3 ? (u64)atoll(b) : 1; int count = atoi(a), nw = (int)(sizeof W / sizeof W[0]);
        Rng r = {seed * 2654435761u + 1};
        int saved_fb = g_rules[GR_SEND_COMMAND_FEEDBACK]; g_rules[GR_SEND_COMMAND_FEEDBACK] = 0;
        for (int i = 0; i < count; i++) {
            char l[256]; size_t len = 0; int words = rng_int(&r, 1, 9);
            for (int w = 0; w < words && len < sizeof l - 40; w++) {
                const char *t = W[rng_int(&r, 0, nw - 1)];
                if (w == 0) t = W[rng_int(&r, 0, 28)];
                len += (size_t)snprintf(l + len, sizeof l - len, "%s%s", w ? " " : "", t);
                if (rng_int(&r, 0, 9) == 0 && len < sizeof l - 4) { l[len++] = (char)rng_int(&r, 33, 126); l[len] = 0; }
            }
            if (strstr(l, "fill") || strstr(l, "clone") || strstr(l, "fillbiome")) continue;   // keep the run fast: no huge volume edits
            command_execute(l);
            g_fuzz_runs++;
        }
        g_rules[GR_SEND_COMMAND_FEEDBACK] = saved_fb;
        game_message(COL_WHITE, "fuzz: ran %d random commands", g_fuzz_runs);
        return true;
    }
    if (!strcmp(cmd, "fuzzsave") && n >= 2) {   // tests: copy world <a> to <a>_fuzz with random byte flips in every file
        char src[300], dst[300]; snprintf(src, sizeof src, "saves/%.100s", a); snprintf(dst, sizeof dst, "saves/%.100s_fuzz", a);
        u64 seed = n >= 3 ? (u64)atoll(b) : 7;
        int files = plat_fuzz_copy(src, dst, seed);
        game_message(COL_WHITE, "fuzz: corrupted %d files into %s", files, dst);
        return true;
    }
    if (!strcmp(cmd, "setting") && n >= 3) {   // setting <name> <value>: change an option from scripts (tests)
        int v = atoi(b);
        if (!strcmp(a, "toggle_sneak")) G.set.toggle_sneak = v; else if (!strcmp(a, "toggle_sprint")) G.set.toggle_sprint = v;
        else if (!strcmp(a, "reduced_motion")) G.set.reduced_motion = v; else if (!strcmp(a, "max_fps")) G.set.max_fps = v;
        else if (!strcmp(a, "subtitles")) G.set.subtitles = v; else if (!strcmp(a, "colorblind")) G.set.colorblind = v;
        else if (!strcmp(a, "simulation_distance")) G.set.sim_dist = clampi(v, 2, 32);
        else if (!strcmp(a, "language")) snprintf(G.set.lang, sizeof G.set.lang, "%.7s", b);
        else if (!strcmp(a, "high_contrast")) G.set.high_contrast = v; else if (!strcmp(a, "narrator")) G.set.narrator = v;
        else if (!strncmp(a, "key_", 4)) { for (int k = 0; k < KB_COUNT; k++) if (!strcmp(a + 4, KB_KEYS[k])) G.set.keys[k] = v; }
        settings_apply(); return true;
    }
    if (!strcmp(cmd, "gamepad") && n >= 2) {   // tests: gamepad off | gamepad button <i> <0|1> | gamepad axis <i> <value>
        extern GLFWgamepadstate g_fake_pad; extern bool g_fake_pad_on;
        if (!strcmp(a, "off")) { g_fake_pad_on = false; memset(&g_fake_pad, 0, sizeof g_fake_pad); g_fake_pad.axes[4] = g_fake_pad.axes[5] = -1; return true; }
        if (!g_fake_pad_on) { memset(&g_fake_pad, 0, sizeof g_fake_pad); g_fake_pad.axes[4] = g_fake_pad.axes[5] = -1; g_fake_pad_on = true; }
        int idx = n >= 3 ? atoi(b) : 0; f32 v = n >= 4 ? (f32)atof(c) : 0;
        if (!strcmp(a, "button") && idx >= 0 && idx < 15) g_fake_pad.buttons[idx] = v != 0 ? GLFW_PRESS : GLFW_RELEASE;
        if (!strcmp(a, "axis") && idx >= 0 && idx < 6) g_fake_pad.axes[idx] = v;
        return true;
    }
    if (!strcmp(cmd, "dbgaim") && e) {   // debug: where the crosshair ray goes
        vec3 d = v3(sinf(e->yaw) * cosf(e->pitch), sinf(e->pitch), cosf(e->yaw) * cosf(e->pitch));
        { Block lb = world_get_block(g_player.look.pos.x, g_player.look.pos.y, g_player.look.pos.z); LOG_INFO("AIM block %s at %d %d %d solid %d", BLOCKS[lb].key ? BLOCKS[lb].key : "air", g_player.look.pos.x, g_player.look.pos.y, g_player.look.pos.z, BLOCKS[lb].solid); }
        LOG_INFO("AIM pos %.2f %.2f %.2f yaw %.3f pitch %.3f dir %.2f %.2f %.2f hit %d dist %.2f ent %u", e->pos.x, e->pos.y, e->pos.z, e->yaw, e->pitch, d.x, d.y, d.z, g_player.look.hit, g_player.look.dist, g_player.look_entity);
        ENT_FOREACH(o) if (o != e && v3_dist(o->pos, e->pos) < 6) { AABB bb = o->kind == ENT_DECOR ? decor_aabb(o) : entity_aabb(o); LOG_INFO("AIM ent kind %d pos %.2f %.2f %.2f box %.2f..%.2f %.2f..%.2f %.2f..%.2f", o->kind, o->pos.x, o->pos.y, o->pos.z, bb.min.x, bb.max.x, bb.min.y, bb.max.y, bb.min.z, bb.max.z); }
        return true;
    }
    if (!strcmp(cmd, "forcespawns")) { extern int g_phantom_timer; g_phantom_timer = 1; g_level.trader_timer = 1; g_level.trader_chance = 100; return true; }   // tests: run the phantom / trader checks now
    if (!strcmp(cmd, "setrest") && n >= 2) { g_player.rest_ticks = atoi(a); return true; }   // tests: ticks since the player last slept
    if (!strcmp(cmd, "fishbite")) { Entity *b = entity_get(g_player.bobber); if (b) { b->phase = 0; b->special_timer = 1; } return true; }   // make a fish bite now (tests)
    if (!strcmp(cmd, "vibrate") && e) { sculk_vibration(e->pos, n >= 2 ? atoi(a) : 1, true); return true; }   // vibrate [freq]: a player vibration where you stand
    if (!strcmp(cmd, "credits")) { G.credits_t = 0; game_open_screen(SCR_CREDITS); return true; }
    if (!strcmp(cmd, "guide")) { G.guide_page = n >= 2 ? atoi(a) - 1 : 0; game_open_screen(SCR_GUIDE); return true; }
    if (!strcmp(cmd, "talk") && e) {   // talk to the nearest soul or Luminary within 10 blocks
        Entity *best = NULL; f32 bd = 10;
        ENT_FOREACH(o) if (o->kind == ENT_MOB && (MOB_DEFS[o->mob].ai == AIK_SOUL || MOB_DEFS[o->mob].ai == AIK_GOD) && v3_dist(o->pos, e->pos) < bd) { bd = v3_dist(o->pos, e->pos); best = o; }
        if (best) sift_interact(best); else game_message(COL_WHITE, "Nobody to talk to here");
        return true;
    }
    if (!strcmp(cmd, "siftgate") && e) {   // teleport to the Sift gateway Zorvath leaves behind in Space
        if (g_world.dim != DIM_END || g_world.sift_gate.y <= 0) { game_message(COL_WHITE, "There is no Sift gateway here"); return true; }
        e->pos = e->prev_pos = v3(g_world.sift_gate.x + 0.5f, (f32)g_world.sift_gate.y, g_world.sift_gate.z + 0.5f); e->vel = v3(0, 0, 0);
        return true;
    }
    if (!strcmp(cmd, "look") && n >= 3 && e) { e->yaw = DEG2RAD((f32)atof(a)); e->pitch = DEG2RAD((f32)atof(b)); return true; }
    if (!strcmp(cmd, "screen") && n >= 2) {
        if (!strcmp(a, "inventory")) game_open_screen(SCR_INVENTORY);
        else if (!strcmp(a, "pause")) game_open_screen(SCR_PAUSE);
        else if (!strcmp(a, "crafting")) game_open_screen(SCR_CRAFTING);
        else if (!strcmp(a, "stonecutter")) game_open_screen(SCR_STONECUTTER);
        else if (!strcmp(a, "smithing")) game_open_screen(SCR_SMITHING);
        else if (!strcmp(a, "close")) game_close_screen();
        else if (!strcmp(a, "title")) game_quit_to_title();
        else if (!strcmp(a, "settings")) { G.return_screen = G.screen; game_open_screen(SCR_SETTINGS); }
        return true;
    }
    if (!strcmp(cmd, "check") && n >= 4) {
        // check <what> <op> <value>   or   check inv <item> <op> <value>
        bool okq;
        const char *op = b, *val = c, *arg = NULL;
        if (n >= 5) { arg = b; op = c; val = d; }
        f32 v = query(a, arg, &okq), want = (f32)atof(val);
        bool pass = okq && ((!strcmp(op, ">") && v > want) || (!strcmp(op, "<") && v < want) || (!strcmp(op, ">=") && v >= want) ||
                            (!strcmp(op, "<=") && v <= want) || (!strcmp(op, "==") && fabsf(v - want) < 1e-3f) || (!strcmp(op, "!=") && fabsf(v - want) >= 1e-3f));
        checks_run++;
        if (!pass) checks_failed++;
        LOG_INFO("CHECK %-5s %s %s%s%s %s %s  (actual %.2f)", pass ? "PASS" : "FAIL", a, arg ? arg : "", arg ? " " : "", op, val, "", v);
        return true;
    }
    if (!strcmp(cmd, "biome") && e) {
        int b = world_biome_at((int)floorf(e->pos.x), (int)floorf(e->pos.y), (int)floorf(e->pos.z));
        game_message(ok_col, "Biome: %s (ultimatecraft:%s)", BIOMES[b].name, BIOMES[b].key);
        return true;
    }
    if (!strcmp(cmd, "locate") && n >= 2 && e) {
        if ((!strcmp(a, "structure") && n >= 3) || (strcmp(a, "biome") && strcmp(a, "stronghold") && strcmp(a, "structure") && structure_by_key(a) >= 0)) {
            const char *nm = !strcmp(a, "structure") ? b : a;
            int t = structure_by_key(nm);
            ivec3 at;
            if (t < 0 || t >= ST_COUNT) { game_message(rgba_u32(255, 85, 85, 255), "Unknown structure '%s'", nm); return true; }
            if (!structure_locate(t, (int)e->pos.x, (int)e->pos.z, &at)) { game_message(rgba_u32(255, 85, 85, 255), "No %s nearby in this dimension", STRUCTS[t].name); return true; }
            game_message(ok_col, "The nearest %s is at %d ~ %d (%d blocks away)", STRUCTS[t].name, at.x, at.z, (int)v3_dist(v3((f32)at.x, 0, (f32)at.z), v3(e->pos.x, 0, e->pos.z)));
            if ((!strcmp(a, "structure") && n >= 4 && !strcmp(c, "tp")) || (strcmp(a, "structure") && n >= 3 && !strcmp(b, "tp"))) {
                int y = at.y + 2;
                if (g_world.dim == DIM_OVERWORLD && t != ST_MINESHAFT && t != ST_ANCIENT_CITY && t != ST_TRIAL_CHAMBERS && t != ST_MONUMENT && t != ST_UNDERGROUND_CITY) y = worldgen_surface(g_world.seed, at.x, at.z, NULL) + 2;
                if (t == ST_MONUMENT) y = SEA_LEVEL + 2;
                if (t == ST_FORTRESS) y = 66;
                if (t == ST_BASTION) y = 40;
                if (t == ST_STRONGHOLD) y = at.y + 1;
                e->pos = e->prev_pos = v3(at.x + 0.5f, (f32)y, at.z + 0.5f); e->vel = v3(0, 0, 0); e->fall_dist = 0;
            }
            return true;
        }
        if (!strcmp(a, "stronghold") || !strcmp(a, "structure")) {
            ivec3 s = worldgen_stronghold(g_world.seed);
            game_message(ok_col, "The nearest stronghold portal room is at %d %d %d", s.x, s.y - world_y_offset(), s.z);
            return true;
        }
        if (!strcmp(a, "biome") && n >= 3) {
            int want = biome_by_key(b);
            if (want < 0) { game_message(rgba_u32(255, 85, 85, 255), "Unknown biome '%s'", b); return true; }
            int cx = (int)e->pos.x, cz = (int)e->pos.z;
            for (int r = 0; r <= 12000; r += 32) {
                int steps = r == 0 ? 1 : (int)(r * 2 * PI_F / 32);
                for (int i = 0; i < steps; i++) {
                    f32 ang = i * 2 * PI_F / steps;
                    int x = cx + (int)(cosf(ang) * r), z = cz + (int)(sinf(ang) * r);
                    if (worldgen_biome(g_world.seed, g_world.dim, x, z) == want) {
                        game_message(ok_col, "The nearest %s is at %d ~ %d (%d blocks away)", BIOMES[want].name, x, z, r);
                        if (n >= 4 && !strcmp(c, "tp")) {
                            e->pos = e->prev_pos = v3(x + 0.5f, (f32)MCY(250), z + 0.5f); e->vel = v3(0, 0, 0); e->fall_dist = 0;
                            G.loading_ticks = 0; G.snap_ground = true; game_open_screen(SCR_LOADING);
                        }
                        return true;
                    }
                }
            }
            game_message(rgba_u32(255, 85, 85, 255), "Could not find %s within 12000 blocks", BIOMES[want].name);
            return true;
        }
    }
    if ((!strcmp(cmd, "dimension") || !strcmp(cmd, "dim")) && n >= 2) {
        int dm = dim_by_id(a);
        if (dm < 0) { game_message(rgba_u32(255, 85, 85, 255), "Unknown dimension '%s' (overworld, hell, space, sift)", a); return true; }
        game_change_dimension(dm, false);
        return true;
    }
    if (!strcmp(cmd, "effect") && n >= 2 && e) {
        if (!strcmp(a, "clear")) { effects_clear(e); return true; }
        const char *nm = !strcmp(a, "give") ? b : a;
        int secs = atoi(!strcmp(a, "give") ? c : b), amp = atoi(!strcmp(a, "give") ? d : c);
        if (secs <= 0) secs = 30;
        char want[48]; lower_compact(nm, want, sizeof want);
        for (int i = 1; i < EFFECT_COUNT; i++) { char k[48]; lower_compact(EFFECT_NAMES[i], k, sizeof k); if (!strcmp(k, want)) { effect_add(e, i, MAXI(0, amp - (amp > 0)), secs * TICKRATE); game_message(ok_col, "Applied %s", EFFECT_NAMES[i]); return true; } }
        game_message(rgba_u32(255, 85, 85, 255), "Unknown effect '%s'", nm);
        return true;
    }
    if (!strcmp(cmd, "gallery") && e) {   // every mob in a grid in front of the player (screenshots / tests)
        int cols = 10, first = n >= 2 ? atoi(a) : 0, count = n >= 3 ? atoi(b) : MOB_COUNT;
        f32 sp = n >= 4 ? (f32)atof(c) : 4.0f;
        ivec3 base = v3_floor(e->pos);
        for (int i = first; i < MINI(MOB_COUNT, first + count); i++) {
            int k = i - first;
            if (i == MOB_ENDER_DRAGON || i == MOB_WITHER || i == MOB_COSMIC_TYRANT) continue;
            f32 x = base.x + ((k % cols) - cols / 2) * sp, z = base.z + 6 + (k / cols) * sp;
            Entity *m = mob_spawn(i, v3(x + 0.5f, (f32)base.y + (mob_has(i, MF_FLYING) ? 1.0f : 0), z + 0.5f));
            if (m) { m->persistent = true; m->yaw = m->body_yaw = m->head_yaw = PI_F; m->no_gravity = true; m->ai_timer = 1000000; m->from_spawner = true; }
        }
        return true;
    }
    if (!strcmp(cmd, "freeze")) {   // freezes all mobs in place (for screenshots)
        ENT_FOREACH(o) if (o->kind == ENT_MOB) { o->vel = v3(0, 0, 0); o->special_timer = 1 << 30; }
        G.freeze_mobs = !G.freeze_mobs;
        return true;
    }
    if (!strcmp(cmd, "hud")) { G.hide_hud = !G.hide_hud; G.nmsgs = 0; return true; }
    if (!strcmp(cmd, "clearchat")) { G.nmsgs = 0; return true; }
    if (!strcmp(cmd, "creativetab") && n >= 2) { G.creative_tab = clampi(atoi(a), 0, TAB_COUNT); G.creative_scroll = 0; return true; }
    if (!strcmp(cmd, "search") && n >= 2) { G.creative_tab = TAB_COUNT; snprintf(G.creative_search, sizeof G.creative_search, "%s", a); return true; }
    if (!strcmp(cmd, "log")) { LOG_INFO("SCRIPT: %s", line + 4); return true; }
    return false;
}

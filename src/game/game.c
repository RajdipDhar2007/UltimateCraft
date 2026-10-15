#include "../ui/lang.h"
#include "game.h"
#include "raids.h"
#include "weather.h"
#include "../world/rules.h"
#include "../world/structures.h"
#include "maps.h"
#include "player.h"
#include "fx.h"
#include "../world/sky.h"
#include "../entity/model.h"
#include "../ui/ui.h"
#include "../ui/screens.h"
#include "../audio/audio.h"
#include "../core/platform.h"
#include <stdarg.h>
#include <ctype.h>

Game G;
char g_pending_screenshot[256];
int commands_checks_failed(void);
int commands_checks_run(void);

// ============================================================================ settings
static void settings_defaults(void) {
    G.set = (Settings){.sim_dist = 8, .controller = true, .backups = true, .clouds = true, .max_fps = 0, .render_dist = 8, .fov = 70, .sensitivity = 0.5f, .master = 0.8f, .sfx = 1.0f, .music = 0.5f,
                       .gui_scale = 0, .view_bob = true, .vsync = true, .show_fps = false, .invert_y = false};
    for (int k = 0; k < KB_COUNT; k++) G.set.keys[k] = KB_DEFAULT[k];
}
static void settings_load(void) {
    settings_defaults();
    char *txt = plat_read_file("settings.txt", NULL);
    if (!txt) return;
    for (char *l = strtok(txt, "\r\n"); l; l = strtok(NULL, "\r\n")) {
        int i; f32 f;
        if (sscanf(l, "render_distance=%d", &i) == 1) G.set.render_dist = clampi(i, 2, MAX_RENDER_DIST);
        else if (sscanf(l, "fov=%f", &f) == 1) G.set.fov = clampf(f, 50, 110);
        else if (sscanf(l, "sensitivity=%f", &f) == 1) G.set.sensitivity = clampf(f, 0.1f, 1.5f);
        else if (sscanf(l, "master_volume=%f", &f) == 1) G.set.master = clampf(f, 0, 1);
        else if (sscanf(l, "sound_volume=%f", &f) == 1) G.set.sfx = clampf(f, 0, 1);
        else if (sscanf(l, "music_volume=%f", &f) == 1) G.set.music = clampf(f, 0, 1);
        else if (sscanf(l, "gui_scale=%d", &i) == 1) G.set.gui_scale = clampi(i, 0, 4);
        else if (sscanf(l, "view_bobbing=%d", &i) == 1) G.set.view_bob = i != 0;
        else if (sscanf(l, "vsync=%d", &i) == 1) G.set.vsync = i != 0;
        else if (sscanf(l, "show_fps=%d", &i) == 1) G.set.show_fps = i != 0;
        else if (sscanf(l, "invert_mouse=%d", &i) == 1) G.set.invert_y = i != 0;
        else if (sscanf(l, "world_backups=%d", &i) == 1) G.set.backups = i != 0;
        else if (sscanf(l, "clouds=%d", &i) == 1) G.set.clouds = i != 0;
        else if (sscanf(l, "toggle_sneak=%d", &i) == 1) G.set.toggle_sneak = i != 0;
        else if (sscanf(l, "toggle_sprint=%d", &i) == 1) G.set.toggle_sprint = i != 0;
        else if (sscanf(l, "reduced_motion=%d", &i) == 1) G.set.reduced_motion = i != 0;
        else if (sscanf(l, "fullscreen=%d", &i) == 1) G.set.fullscreen = i != 0;
        else if (sscanf(l, "max_fps=%d", &i) == 1) G.set.max_fps = clampi(i, 0, 1000);
        else if (sscanf(l, "subtitles=%d", &i) == 1) G.set.subtitles = i != 0;
        else if (sscanf(l, "colorblind=%d", &i) == 1) G.set.colorblind = i != 0;
        else if (sscanf(l, "high_contrast=%d", &i) == 1) G.set.high_contrast = i != 0;
        else if (sscanf(l, "narrator=%d", &i) == 1) G.set.narrator = i != 0;
        else if (sscanf(l, "controller=%d", &i) == 1) G.set.controller = i != 0;
        else if (sscanf(l, "simulation_distance=%d", &i) == 1) G.set.sim_dist = clampi(i, 2, 32);
        else if (sscanf(l, "tutorial=%d", &i) == 1) G.set.tutorial = clampi(i, 0, 99);
        else if (!strncmp(l, "language=", 9)) { snprintf(G.set.lang, sizeof G.set.lang, "%.7s", l + 9); G.set.lang[strcspn(G.set.lang, "\r\n ")] = 0; }
        else if (!strncmp(l, "key_", 4)) { char nm[32]; if (sscanf(l, "key_%31[^=]=%d", nm, &i) == 2) for (int k = 0; k < KB_COUNT; k++) if (!strcmp(nm, KB_KEYS[k]) && i > 0 && i <= GLFW_KEY_LAST) G.set.keys[k] = i; }
    }
    free(txt);
}
const char *KB_NAMES[KB_COUNT] = {"Walk Forwards", "Walk Backwards", "Strafe Left", "Strafe Right", "Jump", "Sneak", "Sprint", "Inventory",
    "Drop Item", "Swap Offhand", "Open Chat", "Open Command", "Screenshot", "Hide HUD", "Debug Screen"};
const char *KB_KEYS[KB_COUNT] = {"forward", "back", "left", "right", "jump", "sneak", "sprint", "inventory", "drop", "swap", "chat", "command", "screenshot", "hide_hud", "debug"};
const int KB_DEFAULT[KB_COUNT] = {GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_A, GLFW_KEY_D, GLFW_KEY_SPACE, GLFW_KEY_LEFT_SHIFT, GLFW_KEY_LEFT_CONTROL, GLFW_KEY_E,
    GLFW_KEY_Q, GLFW_KEY_F, GLFW_KEY_T, GLFW_KEY_SLASH, GLFW_KEY_F2, GLFW_KEY_F1, GLFW_KEY_F3};
const char *key_name(int k) {
    static char b[16];
    switch (k) {
    case GLFW_KEY_SPACE: return "Space"; case GLFW_KEY_LEFT_SHIFT: return "Left Shift"; case GLFW_KEY_RIGHT_SHIFT: return "Right Shift";
    case GLFW_KEY_LEFT_CONTROL: return "Left Ctrl"; case GLFW_KEY_RIGHT_CONTROL: return "Right Ctrl"; case GLFW_KEY_LEFT_ALT: return "Left Alt";
    case GLFW_KEY_TAB: return "Tab"; case GLFW_KEY_ENTER: return "Enter"; case GLFW_KEY_CAPS_LOCK: return "Caps Lock";
    case GLFW_KEY_UP: return "Up"; case GLFW_KEY_DOWN: return "Down"; case GLFW_KEY_LEFT: return "Left"; case GLFW_KEY_RIGHT: return "Right";
    case GLFW_KEY_SLASH: return "/"; default: break;
    }
    if (k >= GLFW_KEY_F1 && k <= GLFW_KEY_F25) { snprintf(b, sizeof b, "F%d", k - GLFW_KEY_F1 + 1); return b; }
    if ((k >= GLFW_KEY_A && k <= GLFW_KEY_Z) || (k >= GLFW_KEY_0 && k <= GLFW_KEY_9)) { b[0] = (char)k; b[1] = 0; return b; }
    if (k >= GLFW_KEY_KP_0 && k <= GLFW_KEY_KP_9) { snprintf(b, sizeof b, "Num %d", k - GLFW_KEY_KP_0); return b; }
    snprintf(b, sizeof b, "Key %d", k); return b;
}
void settings_save(void) {
    char buf[2048];
    int n = snprintf(buf, sizeof buf, "render_distance=%d\nfov=%.0f\nsensitivity=%.2f\nmaster_volume=%.2f\nsound_volume=%.2f\nmusic_volume=%.2f\n"
                     "gui_scale=%d\nview_bobbing=%d\nvsync=%d\nshow_fps=%d\ninvert_mouse=%d\nworld_backups=%d\nclouds=%d\n",
                     G.set.render_dist, G.set.fov, G.set.sensitivity, G.set.master, G.set.sfx, G.set.music, G.set.gui_scale,
                     G.set.view_bob, G.set.vsync, G.set.show_fps, G.set.invert_y, (int)G.set.backups, (int)G.set.clouds);
    n += snprintf(buf + n, sizeof buf - (size_t)n, "toggle_sneak=%d\ntoggle_sprint=%d\nreduced_motion=%d\nfullscreen=%d\nmax_fps=%d\nsubtitles=%d\ncolorblind=%d\nhigh_contrast=%d\nnarrator=%d\n",
                  (int)G.set.toggle_sneak, (int)G.set.toggle_sprint, (int)G.set.reduced_motion, (int)G.set.fullscreen, G.set.max_fps,
                  (int)G.set.subtitles, (int)G.set.colorblind, (int)G.set.high_contrast, (int)G.set.narrator);
    n += snprintf(buf + n, sizeof buf - (size_t)n, "language=%s\ncontroller=%d\nsimulation_distance=%d\ntutorial=%d\n", G.set.lang[0] ? G.set.lang : "en_us", (int)G.set.controller, G.set.sim_dist, G.set.tutorial);
    for (int k = 0; k < KB_COUNT && n < (int)sizeof buf - 40; k++) n += snprintf(buf + n, sizeof buf - (size_t)n, "key_%s=%d\n", KB_KEYS[k], G.set.keys[k]);
    plat_write_file("settings.txt", buf, (size_t)n);
}
void settings_apply(void) {
    window_set_fullscreen(G.set.fullscreen);
    lang_load(G.set.lang[0] ? G.set.lang : "en_us");
    g_ui_colorblind = G.set.colorblind; g_ui_contrast = G.set.high_contrast; g_narrate = G.set.narrator;
    { extern bool g_sky_clouds; g_sky_clouds = G.set.clouds; }
    audio_set_volumes(G.set.master, G.set.sfx, G.set.music);
    window_set_vsync(G.set.vsync);
    g_world.render_dist = G.set.render_dist;
}

u64 seed_from_string(const char *s) {
    bool digits = *s != 0;
    for (const char *p = s; *p; p++) if (!isdigit((unsigned char)*p)) digits = false;
    if (digits) return strtoull(s, NULL, 10);
    u64 h = 1469598103934665603ULL;
    for (const char *p = s; *p; p++) { h ^= (u8)*p; h *= 1099511628211ULL; }
    return h & 0xFFFFFFFFFFFFull;
}

void game_message(u32 color, const char *fmt, ...) {
    if (G.nmsgs == (int)ARRAY_LEN(G.msgs)) { memmove(G.msgs, G.msgs + 1, sizeof(ChatLine) * (ARRAY_LEN(G.msgs) - 1)); G.nmsgs--; }
    ChatLine *l = &G.msgs[G.nmsgs++];
    va_list ap; va_start(ap, fmt);
    vsnprintf(l->text, sizeof l->text, fmt, ap);
    va_end(ap);
    l->time = plat_time();
    l->color = color;
    LOG_INFO("[chat] %s", l->text);
    if (g_narrate) { printf("[narrate] %s\n", l->text); fflush(stdout); }
}

// ============================================================================ hooks from the world layer
// Everything a block drops, following Minecraft's loot rules for crops, leaves, ores ...
void game_block_drops(ivec3 p, u16 b, ItemStack *tool) {
    if (!rule(GR_DO_TILE_DROPS)) return;
    const BlockDef *d = &BLOCKS[b];
    vec3 c = iv3_center(p);
    Block k = d->base;
    int age = d->var, maxage = d->nvar - 1;
    bool ripe = d->mesh == MESH_CROP && age == maxage;
#define DROP(id, n) do { int n_ = (n); if (n_ > 0) item_drop_scatter(c, stack_of((id), n_)); } while (0)
    if (k == B_WHEAT_CROP) { if (ripe) DROP(I_WHEAT, 1); DROP(I_WHEAT_SEEDS, ripe ? irand(1, 4) : 1); return; }
    if (k == B_CARROTS) { DROP(I_CARROT, ripe ? irand(2, 5) : 1); return; }
    if (k == B_POTATOES) { DROP(I_POTATO, ripe ? irand(2, 5) : 1); if (ripe && frand() < 0.02f) DROP(I_POISONOUS_POTATO, 1); return; }
    if (k == B_BEETROOTS) { if (ripe) DROP(I_BEETROOT, 1); DROP(I_BEETROOT_SEEDS, ripe ? irand(1, 4) : 1); return; }
    if (k == B_NETHER_WART_CROP) { DROP(I_NETHER_WART, ripe ? irand(2, 4) : 1); return; }
    if (k == B_TORCHFLOWER_CROP) { DROP(I_TORCHFLOWER_SEEDS, 1); return; }
    if (k == B_PITCHER_CROP) { DROP(I_PITCHER_POD, 1); return; }
    if (k == B_MELON_STEM) { DROP(I_MELON_SEEDS, irand(0, age / 3 + 1)); return; }
    if (k == B_PUMPKIN_STEM) { DROP(I_PUMPKIN_SEEDS, irand(0, age / 3 + 1)); return; }
    if (k == B_SWEET_BERRY_BUSH) { DROP(I_SWEET_BERRIES, age >= 2 ? irand(age - 1, age) : 0); return; }
    if (k == B_COCOA) { DROP(I_COCOA_BEANS, age == 2 ? irand(2, 3) : 1); return; }
    if (k == B_CAVE_VINES) { if (age) DROP(I_GLOW_BERRIES, 1); return; }
    if (block_is_leaves(b)) {
        Block sap = 0;
        const char *key = d->key;
        char buf[64];
        if (k == B_MANGROVE_LEAVES) sap = 0;
        else if (k == B_AZALEA_LEAVES) sap = B_AZALEA;
        else if (k == B_FLOWERING_AZALEA_LEAVES) sap = B_FLOWERING_AZALEA;
        else { snprintf(buf, sizeof buf, "%.*s_sapling", (int)(strlen(key) - 7), key); sap = block_by_key(buf); }
        if (sap && frand() < (k == B_JUNGLE_LEAVES ? 0.025f : 0.05f)) DROP(sap, 1);
        if ((k == B_OAK_LEAVES || k == B_DARK_OAK_LEAVES) && frand() < 0.005f) DROP(I_APPLE, 1);
        if (frand() < 0.02f) DROP(I_STICK, irand(1, 2));
        return;
    }
    if ((k == B_SHORT_GRASS || k == B_FERN || k == B_TALL_GRASS || k == B_LARGE_FERN) && frand() < 0.125f) { DROP(I_WHEAT_SEEDS, 1); return; }
    if (k == B_GRAVEL && frand() < 0.1f) { DROP(I_FLINT, 1); return; }
    if (k == B_SNOW && !(tool && ITEMS[tool->id].tool == TOOL_SHOVEL)) return;
    if (d->key && !strncmp(d->key, "infested_", 9)) { mob_spawn(MOB_SILVERFISH, v3(p.x + 0.5f, (f32)p.y, p.z + 0.5f)); return; }
    if (k == B_SPAWNER || k == B_BUDDING_AMETHYST) { if (d->xp_max) xp_drop(c, irand(d->xp_min, d->xp_max)); return; }
    if (d->drop != DROP_NOTHING && item_valid(d->drop) && d->drop_max > 0) {
        int n = irand(d->drop_min, d->drop_max);
        int fortune = tool ? ench_level(*tool, ENCH_FORTUNE) : 0;
        if (fortune && d->xp_max && d->drop != d->base) { int bonus = irand(0, fortune + 1) - 1; if (bonus > 0) n *= bonus + 1; }   // ores
        else if (fortune && (k == B_GLOWSTONE || k == B_MELON || k == B_SEA_LANTERN)) n = MINI(k == B_MELON ? 9 : 4, n + irand(0, fortune));
        if (n > 0) DROP(d->drop, n);
    }
    if (d->xp_max && tool) xp_drop(c, irand(d->xp_min, d->xp_max));
#undef DROP
}
void game_hook_tnt_ignite(ivec3 p) { tnt_spawn(v3(p.x + 0.5f, (f32)p.y, p.z + 0.5f), 80 * MC_TICK_DIV); }
void game_hook_block_broken_naturally(ivec3 p, u16 block) {
    fx_block_break(p, block);
    game_block_drops(p, block, NULL);
}

static void end_spike(int i, u64 seed, int *px, int *pz, int *top) {
    f32 a = 2 * PI_F * (-i) / 10.0f;
    *px = (int)floorf(42 * cosf(a)); *pz = (int)floorf(42 * sinf(a));
    *top = 76 + (int)(hash64(seed * 7 + i) % 30);
    if (*top > END_H - 6) *top = END_H - 6;
}
void game_hook_column_ready(Column *c) {
    structures_column_ready(c);
    if (c->spawned_animals) return;
    c->spawned_animals = true;
    if (g_world.dim == DIM_END && !g_world.tyrant_killed) {   // star crystals on top of the obsidian pillars
        for (int i = 0; i < 10; i++) {
            int px, pz, top; end_spike(i, g_world.seed, &px, &pz, &top);
            if ((px >> 4) == c->cx && (pz >> 4) == c->cz) crystal_spawn(v3(px + 0.5f, top + 2.0f, pz + 0.5f));
        }
    }
    if (g_world.dim == DIM_SIFT && hash3i(g_world.seed, c->cx, 4747, c->cz) % 4 == 0) {   // a wandering soul on some islands
        int x = c->cx * 16 + 8, z = c->cz * 16 + 8, top = worldgen_sift_surface(g_world.seed, x, z);
        if (top > 0) mob_spawn(MOB_SIFT_SOUL, v3(x + 0.5f, top + 2.0f, z + 0.5f));
    }
    spawn_animals_for_column(c->cx, c->cz);
}

// ============================================================================ screens
bool screen_grabs_mouse(Screen s) { return s == SCR_NONE; }
bool screen_is_container(Screen s) {
    switch (s) {
    case SCR_INVENTORY: case SCR_CRAFTING: case SCR_CHEST: case SCR_FURNACE: case SCR_STONECUTTER: case SCR_SMITHING: case SCR_BREWING:
    case SCR_ENCHANT: case SCR_ANVIL: case SCR_GRINDSTONE: case SCR_LOOM: case SCR_CARTOGRAPHY: case SCR_TRADE: case SCR_LECTERN: return true;
    default: return false;
    }
}
bool game_paused(void) {
    return !G.in_world || G.screen == SCR_PAUSE || G.screen == SCR_LOADING || (G.screen == SCR_SETTINGS && G.return_screen == SCR_PAUSE);
}
void game_open_screen(Screen s) {
    Screen old = G.screen;
    if (old == s) return;
    if (screen_is_container(old) && s != old) screens_on_close(old);
    G.text_focus = false;
    G.screen = s;
    g_win.nchars = 0;
}
void game_close_screen(void) { game_open_screen(SCR_NONE); }

static int find_safe_y(int x, int z, int start) {
    for (int y = MAXI(1, start); y < WORLD_H - 2; y++) {
        if (!BLOCKS[world_get_block(x, y, z)].solid && !BLOCKS[world_get_block(x, y + 1, z)].solid &&
            !BLOCKS[world_get_block(x, y, z)].liquid && BLOCKS[world_get_block(x, y - 1, z)].solid) return y;
    }
    return world_top_y(x, z) + 1;
}

static bool fresh_world;
bool game_start_world(const char *name, u64 seed, int gamemode, bool create) {
    if (G.in_world) game_quit_to_title();
    entities_clear();
    fx_clear();
    memset(&g_player, 0, sizeof g_player);
    world_open(name, seed, create, create ? DIM_OVERWORLD : save_level_dim(name));
    g_world.old_height_save = false;
    { char np[300]; snprintf(np, sizeof np, "saves/%s/names.txt", name); if (create) item_names_reset(); else item_names_load(np); }
    maps_reset();
    raids_reset();
    if (create) boxes_reset(); else boxes_load();
    g_world.render_dist = G.set.render_dist;
    g_player.gamemode = gamemode;
    fresh_world = create;
    if (create) {
        level_state_reset();
        g_level.difficulty = (u8)(G.autotest ? DIFF_NORMAL : G.new_difficulty);
        g_level.hardcore = !G.autotest && G.new_hardcore;
        if (g_level.hardcore) { g_level.difficulty = DIFF_HARD; g_level.difficulty_locked = true; }
        g_level.cheats = G.autotest || G.new_cheats || gamemode == GM_CREATIVE;
        g_world.time = DAY_TICKS / 24;   // early morning
        g_world.spawn = worldgen_find_spawn(seed);
        player_reset(v3(g_world.spawn.x + 0.5f, (f32)g_world.spawn.y, g_world.spawn.z + 0.5f), true);
        save_level();
    cmdblocks_save(g_world.dim_dir);
    progress_save(g_world.dir);
    } else {
        int gm = gamemode;
        save_load_level(name, &seed, &gm);
        g_player.gamemode = gm;
        if (!save_load_player()) player_reset(v3(g_world.spawn.x + 0.5f, (f32)g_world.spawn.y, g_world.spawn.z + 0.5f), true);
        save_load_entities();
        if (g_world.old_height_save) {   // saved before the world became 384 blocks tall
            Entity *pe = player_ent();
            if (pe && g_world.dim == DIM_OVERWORLD) { pe->pos.y += OW_Y_OFFSET; pe->prev_pos = pe->pos; G.snap_ground = true; }
            game_message(rgba_u32(255, 255, 85, 255), "This world was made before the 384-block update; its terrain has been regenerated.");
        }
    }
    if (!create && G.set.backups) {   // automatic backup: backups/<world>/1..3 (newest first), skipped for worlds > 256 MB
        char bk[300]; snprintf(bk, sizeof bk, "backups/%.200s", name);
        plat_mkdir("backups");
        f64 t0 = plat_time(); long long n = plat_backup_dir(g_world.dir, bk, 3, 256ll << 20);
        if (n >= 0) LOG_INFO("backed up '%s' (%lld KB) in %.0f ms", name, n >> 10, (plat_time() - t0) * 1000); else LOG_WARN("'%s' is too big for an automatic backup", name);
    }
    G.in_world = true;
    { void world_opened(void); world_opened(); }   // scoreboard, forceload, data packs (commands_ext.inc)
    G.loading_ticks = 0;
    G.autosave_timer = 0;
    G.nmsgs = 0;
    game_open_screen(SCR_LOADING);
    LOG_INFO("%s world '%s'", create ? "created" : "loaded", name);
    return true;
}

void game_save(void) {
    boxes_save();
    maps_save_all();
    if (!G.in_world) return;
    f64 t = plat_time();
    save_level();
    forceload_save(g_world.dim_dir);
    save_player();
    save_entities();
    world_save_all();
    LOG_INFO("world saved in %.0f ms", (plat_time() - t) * 1000);
}

void game_quit_to_title(void) {
    if (G.in_world) {
        game_open_screen(SCR_NONE);  // returns crafting grid / cursor items
        game_save();
        world_close();
        entities_clear();
        fx_clear();
    }
    G.in_world = false;
    G.screen = SCR_TITLE;
}

// ============================================================================ furnaces
static void furnaces_tick(void) {
    for (int i = 0; i < g_world.nbents; i++) {
        BlockEntity *be = &g_world.bents[i];
        if (be->type == BE_BREWING) { brewing_tick(be); continue; }
        if ((be->type != BE_FURNACE && be->type != BE_BLAST && be->type != BE_SMOKER) || !world_loaded(be->pos.x, be->pos.z)) continue;
        int mask = be->type == BE_BLAST ? SMELT_BLAST : be->type == BE_SMOKER ? SMELT_SMOKER : SMELT_FURNACE;
        int cook = be->type == BE_FURNACE ? 600 : 300;
        ItemStack *in = &be->slots[0], *fuel = &be->slots[1], *out = &be->slots[2];
        int res = stack_empty(*in) ? 0 : smelting_result(in->id, mask);
        bool can = res && (stack_empty(*out) || (out->id == res && out->count < stack_max(*out)));
        bool was_lit = be->burn_time > 0;
        if (be->burn_time > 0) be->burn_time--;
        if (be->burn_time == 0 && can && !stack_empty(*fuel) && fuel_value(fuel->id) > 0) {
            be->burn_time = be->burn_max = fuel_value(fuel->id) * 3;
            if (be->type != BE_FURNACE) be->burn_time = be->burn_max = be->burn_time / 2;
            if (--fuel->count == 0) *fuel = fuel->id == I_LAVA_BUCKET ? stack_of(I_BUCKET, 1) : (ItemStack){0};
        }
        if (be->burn_time > 0 && can) {
            if (++be->cook_time >= cook) {
                be->cook_time = 0;
                be->xp += smelting_xp(in->id);
                if (--in->count == 0) *in = (ItemStack){0};
                if (stack_empty(*out)) *out = stack_of(res, 1); else out->count++;
                if (in->id == I_WET_SPONGE || res == I_SPONGE) {}
            }
        } else be->cook_time = MAXI(0, be->cook_time - 2);
        bool lit = be->burn_time > 0;
        if (lit != was_lit) {
            ivec3 p = be->pos;
            u16 cur = world_get_block(p.x, p.y, p.z);
            if (block_lit(cur) != lit && BLOCKS[cur].be == be->type) world_set_block_silent(p.x, p.y, p.z, block_set_lit(cur, lit));
        }
    }
}

// ============================================================================ brewing
// slots 0-2 bottles, 3 ingredient, 4 blaze powder fuel; be->data = fuel charges, cook_time = progress
static int brew_result(int pot, int ing, int *item_out) {
    *item_out = 0;
    if (ing == I_GUNPOWDER) { *item_out = I_SPLASH_POTION; return pot; }
    if (ing == I_DRAGON_BREATH) { *item_out = I_LINGERING_POTION; return pot; }
    if (pot == POT_WATER) {
        if (ing == I_NETHER_WART) return POT_AWKWARD;
        if (ing == I_GLOWSTONE_DUST) return POT_THICK;
        if (ing == I_REDSTONE || ing == I_SUGAR || ing == I_SPIDER_EYE || ing == I_GHAST_TEAR || ing == I_BLAZE_POWDER || ing == I_MAGMA_CREAM || ing == I_GLISTERING_MELON_SLICE || ing == I_RABBIT_FOOT) return POT_MUNDANE;
        if (ing == I_FERMENTED_SPIDER_EYE) return POT_WEAKNESS;
        return -1;
    }
    if (pot == POT_AWKWARD) {
        static const struct { int ing, pot; } T[] = {{0, 0}};
        (void)T;
        if (ing == I_GOLDEN_CARROT) return POT_NIGHT_VISION;
        if (ing == I_RABBIT_FOOT) return POT_LEAPING;
        if (ing == I_MAGMA_CREAM) return POT_FIRE_RESISTANCE;
        if (ing == I_SUGAR) return POT_SWIFTNESS;
        if (ing == I_PUFFERFISH) return POT_WATER_BREATHING;
        if (ing == I_GLISTERING_MELON_SLICE) return POT_HEALING;
        if (ing == I_SPIDER_EYE) return POT_POISON;
        if (ing == I_GHAST_TEAR) return POT_REGENERATION;
        if (ing == I_BLAZE_POWDER) return POT_STRENGTH;
        if (ing == I_PHANTOM_MEMBRANE) return POT_SLOW_FALLING;
        if (ing == I_TURTLE_HELMET) return POT_SLOWNESS;
        return -1;
    }
    if (ing == I_FERMENTED_SPIDER_EYE) {
        if (pot == POT_NIGHT_VISION) return POT_INVISIBILITY;
        if (pot == POT_SWIFTNESS || pot == POT_LEAPING) return POT_SLOWNESS;
        if (pot == POT_HEALING || pot == POT_POISON) return POT_HARMING;
        return -1;
    }
    if (ing == I_GLOWSTONE_DUST) {
        if (pot == POT_HEALING) return POT_STRONG_HEALING;
        if (pot == POT_STRENGTH) return POT_STRONG_STRENGTH;
        if (pot == POT_SWIFTNESS) return POT_STRONG_SWIFTNESS;
        if (pot == POT_REGENERATION) return POT_STRONG_REGENERATION;
    }
    return -1;
}
void brewing_tick(BlockEntity *be) {
    ItemStack *ing = &be->slots[3], *fuel = &be->slots[4];
    if (be->data <= 0 && fuel->id == I_BLAZE_POWDER) { be->data = 20; if (--fuel->count == 0) *fuel = (ItemStack){0}; }
    bool any = false;
    for (int i = 0; i < 3; i++) {
        ItemStack s = be->slots[i];
        if (stack_empty(s) || stack_empty(*ing)) continue;
        int it; int r = brew_result(s.damage, ing->id, &it);
        if (r >= 0 && (s.id == I_POTION || s.id == I_SPLASH_POTION || s.id == I_LINGERING_POTION)) any = true;
    }
    if (!any || be->data <= 0) { be->cook_time = 0; return; }
    if (++be->cook_time >= 400 * 3) {
        be->cook_time = 0;
        for (int i = 0; i < 3; i++) {
            ItemStack *s = &be->slots[i];
            if (stack_empty(*s)) continue;
            int it; int r = brew_result(s->damage, ing->id, &it);
            if (r < 0) continue;
            s->damage = (u16)r;
            if (it) s->id = (u16)it;
        }
        if (--ing->count == 0) *ing = ing->id == I_DRAGON_BREATH ? stack_of(I_GLASS_BOTTLE, 1) : (ItemStack){0};
        be->data--;
    }
}

// ============================================================================ dimensions & portals
static int pending_portal;        // 0 none, 1 find/build a hell portal at the destination, 2 space platform, 3 Sift arrival island, 4 Sift portal in the Overworld
static vec3 pending_pos;
void game_change_dimension(int dim, bool via_portal) {
    if (!G.in_world || dim == g_world.dim) return;
    Entity *pe = player_ent();
    Entity saved = *pe;
    int from = g_world.dim;
    save_level(); save_player(); save_entities(); world_save_all();
    world_close();
    entities_clear();
    fx_clear();
    char wname[64]; snprintf(wname, sizeof wname, "%s", g_world.name);
    world_open(wname, g_world.seed, false, dim);
    g_world.render_dist = G.set.render_dist;
    save_load_level(g_world.name, NULL, NULL);
    // put the player entity back
    Entity *e = entity_spawn(ENT_PLAYER, saved.pos);
    u32 id = e->id;
    *e = saved; e->id = id; g_player.eid = id;
    vec3 p = saved.pos;
    pending_portal = 0;
    if (dim == DIM_END) { p = v3(100.5f, 49, 0.5f); pending_portal = 2; e->yaw = PI_F / 2; }
    else if (dim == DIM_SIFT) {   // always arrive on the origin island, next to a return portal
        if (from == DIM_OVERWORLD) g_world.sift_return = v3_floor(saved.pos);
        int top = worldgen_sift_surface(g_world.seed, 0, 0);
        p = v3(0.5f, (f32)(top > 0 ? top + 1 : 120), 0.5f); pending_portal = 3;
    } else if (from == DIM_SIFT && dim == DIM_OVERWORLD && g_world.sift_return.y > 0) {
        ivec3 s = g_world.sift_return; p = v3(s.x + 0.5f, (f32)s.y, s.z + 0.5f); pending_portal = via_portal ? 4 : 0;
    } else if (from == DIM_END || from == DIM_SIFT) {
        ivec3 s = g_world.spawn; p = v3(s.x + 0.5f, (f32)s.y, s.z + 0.5f);
    } else if (dim == DIM_NETHER) { p = v3(floorf(p.x / 8) + 0.5f, clampf(p.y - dim_info(from)->y_offset, 40, 100), floorf(p.z / 8) + 0.5f); pending_portal = via_portal ? 1 : 0; }
    else { p = v3(floorf(p.x * 8) + 0.5f, p.y + OW_Y_OFFSET, floorf(p.z * 8) + 0.5f); pending_portal = via_portal ? 1 : 0; }
    e->pos = e->prev_pos = p; e->vel = v3(0, 0, 0); e->fall_dist = 0;
    pending_pos = p;
    save_load_entities();
    g_player.dim_changed = true;
    G.loading_ticks = 0;
    game_open_screen(SCR_LOADING);
    LOG_INFO("entered %s", dim_info(dim)->the_name);
    if (via_portal || dim != DIM_OVERWORLD) game_message(rgba_u32(255, 255, 160, 255), "You entered %s", dim_info(dim)->the_name);
}
static bool find_nearby_portal(vec3 at, vec3 *out, u16 portal) {
    ivec3 c = v3_floor(at);
    f32 best = 1e9f;
    for (int dz = -16; dz <= 16; dz++) for (int dx = -16; dx <= 16; dx++) for (int y = 2; y < world_build_top(); y++) {
        if (BLOCKS[world_get_block(c.x + dx, y, c.z + dz)].base != portal) continue;
        if (BLOCKS[world_get_block(c.x + dx, y - 1, c.z + dz)].base == portal) continue;
        f32 d = (f32)(dx * dx + dz * dz) + fabsf(y - at.y);
        if (d < best) { best = d; *out = v3(c.x + dx + 0.5f, (f32)y, c.z + dz + 0.5f); }
    }
    return best < 1e9f;
}
static void build_portal_at(vec3 at, u16 frame, u16 corner, u16 portal) {
    ivec3 c = v3_floor(at);
    // find a floor with room for a 4x5 frame
    int y = -1;
    for (int dy = 0; dy < 60 && y < 0; dy++) for (int s = -1; s <= 1; s += 2) {
        int yy = c.y + dy * s;
        if (yy < 3 || yy > world_build_top() - 7) continue;
        bool ok = true;
        for (int i = -1; i <= 2 && ok; i++) for (int h = 0; h < 4 && ok; h++) { u16 b = world_get_block(c.x + i, yy + h, c.z); ok = block_is_air(b) || BLOCKS[b].replaceable; }
        if (ok && BLOCKS[world_get_block(c.x, yy - 1, c.z)].solid && !BLOCKS[world_get_block(c.x, yy - 1, c.z)].liquid) { y = yy; break; }
    }
    if (y < 0) {   // carve a pocket and add a platform
        y = clampi(c.y, g_world.dim == DIM_NETHER ? NETHER_LAVA_LEVEL + 3 : MCY(10), world_build_top() - 9);
        for (int i = -2; i <= 3; i++) for (int dz = -2; dz <= 2; dz++) { world_set_block(c.x + i, y - 1, c.z + dz, frame); for (int h = 0; h < 5; h++) world_set_block(c.x + i, y + h, c.z + dz, B_AIR); }
    }
    for (int i = -1; i <= 2; i++) { world_set_block(c.x + i, y - 1, c.z, (i == -1 || i == 2) ? corner : frame); world_set_block(c.x + i, y + 3, c.z, (i == -1 || i == 2) ? corner : frame); }
    for (int h = 0; h < 3; h++) { world_set_block(c.x - 1, y + h, c.z, frame); world_set_block(c.x + 2, y + h, c.z, frame); }
    for (int i = 0; i <= 1; i++) for (int h = 0; h < 3; h++) world_set_block_silent(c.x + i, y + h, c.z, portal);
    pending_pos = v3(c.x + 0.5f, (f32)y, c.z + 0.5f);
}
static void finish_dimension_arrival(Entity *e) {
    if (pending_portal == 1) {
        vec3 q;
        if (find_nearby_portal(e->pos, &q, B_NETHER_PORTAL)) pending_pos = q; else build_portal_at(e->pos, B_OBSIDIAN, B_OBSIDIAN, B_NETHER_PORTAL);
        e->pos = e->prev_pos = pending_pos;
    } else if (pending_portal == 3 || pending_portal == 4) {   // Sift portals: gold frame with glowstone corners
        vec3 q, at = pending_portal == 3 ? v3(e->pos.x + 3, e->pos.y, e->pos.z) : e->pos;
        if (find_nearby_portal(at, &q, B_SIFT_PORTAL)) { if (pending_portal == 4) pending_pos = q; }
        else build_portal_at(at, B_GOLD_BLOCK, B_GLOWSTONE, B_SIFT_PORTAL);
        if (pending_portal == 4) e->pos = e->prev_pos = v3(pending_pos.x + 0.5f, pending_pos.y, pending_pos.z + 1.5f);   // step out in front
    } else if (pending_portal == 2) {
        for (int dz = -2; dz <= 2; dz++) for (int dx = -2; dx <= 2; dx++) {
            world_set_block(100 + dx, 48, dz, B_OBSIDIAN);
            for (int h = 0; h < 3; h++) world_set_block(100 + dx, 49 + h, dz, B_AIR);
        }
        e->pos = e->prev_pos = v3(100.5f, 49, 0.5f);
        if (!g_world.tyrant_killed) {   // Zorvath waits over the central island (the old dragon is now an optional /summon)
            bool have = false;
            ENT_FOREACH(o) if (o->kind == ENT_MOB && o->mob == MOB_COSMIC_TYRANT) have = true;
            if (!have) mob_spawn(MOB_COSMIC_TYRANT, v3(0, 84, 0));
        }
    }
    pending_portal = 0;
}

// ============================================================================ simulation tick
// Minecraft-timed natural spawns that have their own game rules
int g_phantoms_spawned, g_traders_spawned, g_phantom_timer = 1200, g_ambient_plays, g_cave_sounds; bool g_cave_ready;
static void spawn_rules_tick(void) {
    Entity *p = player_ent();
    if (!p || g_player.dead || g_world.dim != DIM_OVERWORLD || g_level.difficulty == DIFF_PEACEFUL) return;
    if (--g_phantom_timer <= 0) {   // doInsomnia: phantoms hunt players who have not slept for 3+ days
        g_phantom_timer = irand(1200, 2400);
        bool sky = world_top_y((int)floorf(p->pos.x), (int)floorf(p->pos.z)) <= (int)p->pos.y;
        if (rule(GR_DO_INSOMNIA) && rule(GR_DO_MOB_SPAWNING) && (world_is_night() || g_level.thundering) && sky && (g_player.gamemode == GM_SURVIVAL || g_player.gamemode == GM_ADVENTURE) &&
            irand(0, MAXI(1, g_player.rest_ticks)) >= 3 * DAY_TICKS) {
            int n = irand(1, 1 + g_level.difficulty);
            for (int i = 0; i < n; i++) if (mob_spawn(MOB_PHANTOM, v3_add(p->pos, v3(frand_range(-10, 10), frand_range(20, 34), frand_range(-10, 10))))) g_phantoms_spawned++;
        }
    }
    if (--g_level.trader_timer <= 0) {   // doTraderSpawning: every 20 minutes, 25 / 50 / 75 % chance
        g_level.trader_timer = 24000;
        if (!rule(GR_DO_TRADER_SPAWNING) || !rule(GR_DO_MOB_SPAWNING)) return;
        bool have = false; ENT_FOREACH(o) if (o->kind == ENT_MOB && o->mob == MOB_WANDERING_TRADER) have = true;
        if (have) return;
        if (irand(0, 99) >= MAXI(25, g_level.trader_chance)) { g_level.trader_chance = MINI(75, MAXI(25, g_level.trader_chance) + 25); return; }
        g_level.trader_chance = 25;
        for (int t = 0; t < 10; t++) {
            int x = (int)p->pos.x + irand(-24, 24), z = (int)p->pos.z + irand(-24, 24), y = world_top_y(x, z);
            if (y < 0 || BLOCKS[world_get_block(x, y, z)].liquid) continue;
            Entity *tr = mob_spawn(MOB_WANDERING_TRADER, v3(x + 0.5f, y + 1.0f, z + 0.5f));
            if (tr) { g_traders_spawned++; for (int l = 0; l < 2; l++) mob_spawn(MOB_TRADER_LLAMA, v3(x + 1.5f + l, y + 1.0f, z + 0.5f)); }
            break;
        }
    }
}
// tutorial hints for new players: one at a time, each disappears when you have done it (Options-independent:
// "tutorial=99" in settings.txt turns them off). Uses the player's own key bindings in the text.
int g_tutorial_shown;
static void tutorial_tick(void) {
    Entity *p = player_ent();
    if (!p || G.set.tutorial >= 99 || G.autotest_quiet || g_player.gamemode != GM_SURVIVAL || G.screen != SCR_NONE) { G.hint[0] = 0; return; }
    static vec3 start; static int step_t;
    if (step_t == 0) start = p->pos;
    step_t++;
    bool done = false; char h[160] = "";
    switch (G.set.tutorial) {
    case 0: snprintf(h, sizeof h, "Move with %s %s %s %s, look around with the mouse", key_name(KEY(KB_FORWARD)), key_name(KEY(KB_LEFT)), key_name(KEY(KB_BACK)), key_name(KEY(KB_RIGHT)));
            done = v3_dist(p->pos, start) > 6; break;
    case 1: snprintf(h, sizeof h, "Hold the left mouse button on a tree to collect wood"); done = g_stats[STAT_MINED] > 0; break;
    case 2: snprintf(h, sizeof h, "Press %s to open your inventory and craft planks", key_name(KEY(KB_INVENTORY))); done = g_stats[STAT_CRAFTED] > 0; break;
    case 3: snprintf(h, sizeof h, "Craft a crafting table and place it with the right mouse button"); done = g_stats[STAT_PLACED] > 0; break;
    case 4: snprintf(h, sizeof h, "Nights are dangerous: build a shelter, or sleep in a bed"); done = step_t > 60 * TICKRATE; break;
    case 5: snprintf(h, sizeof h, "Open the Game Menu (Esc) and read the Guide for the road to the Sift"); done = step_t > 30 * TICKRATE || G.guide_page > 0; break;
    default: G.set.tutorial = 99; settings_save(); G.hint[0] = 0; return;
    }
    snprintf(G.hint, sizeof G.hint, "%s", TR(h));
    if (done) { G.set.tutorial++; step_t = 0; g_tutorial_shown++; settings_save(); }
}
static void game_tick(void) {
    G.ticks++;
    {   Entity *sp = player_ent(); g_world.sim_dist = G.set.sim_dist;
        if (sp) { g_world.sim_cx = (int)floorf(sp->pos.x) >> 4; g_world.sim_cz = (int)floorf(sp->pos.z) >> 4; } }
    sculk_tick();
    if (G.in_world && G.ticks % 10 == 0) tutorial_tick();
    {   // ambience: rain, thunder, caves
        extern int g_thunder_pending; extern f32 g_thunder_vol;
        Entity *pl = player_ent();
        if (g_thunder_pending > 0 && --g_thunder_pending == 0) { sound_play(SND_THUNDER, g_thunder_vol, frand_range(0.8f, 1.05f)); g_ambient_plays++; }
        if (pl && G.in_world && G.ticks % (TICKRATE * 19 / 10) == 0 && g_level.raining && g_world.dim == DIM_OVERWORLD) {
            int x = (int)floorf(pl->pos.x), z = (int)floorf(pl->pos.z);
            bool open = world_top_y(x, z) <= (int)pl->pos.y + 1;
            sound_play(SND_RAIN, open ? 0.45f : 0.12f, frand_range(0.95f, 1.05f)); g_ambient_plays++;
        }
        if (pl && G.in_world && G.ticks % TICKRATE == 0 && g_world.dim == DIM_OVERWORLD) {
            int x = (int)floorf(pl->pos.x), z = (int)floorf(pl->pos.z), y = (int)floorf(pl->pos.y + 1);
            bool deep = world_top_y(x, z) > y + 12;
            u16 lw = world_get_light(x, y, z);
            g_cave_ready = deep && L_CH(lw, 0) == 0;
            if (g_cave_ready && frand() < 0.02f) { sound_play(SND_CAVE, 0.35f, frand_range(0.8f, 1.1f)); g_ambient_plays++; g_cave_sounds++; }
        }
    }
    progress_tick();
    datapacks_tick();   // data-pack tick functions + world border
    {   static char fl_dim[300]; if (G.in_world && strcmp(fl_dim, g_world.dim_dir)) { snprintf(fl_dim, sizeof fl_dim, "%s", g_world.dim_dir); forceload_load(g_world.dim_dir); void cmdblocks_load(const char *); cmdblocks_load(g_world.dim_dir); } }
    if (G.in_world && G.ticks % MC_TICK_DIV == 0) spawn_rules_tick();
    tripwire_tick();
    if (G.in_world && g_world.dim == DIM_NETHER && G.ticks % 120 == 37 && frand() < 0.3f) {   // Hell ambience
        g_hell_ambience++;
        sound_play(frand() < 0.65f ? SND_HELL_RUMBLE : SND_HELL_WAIL, frand_range(0.25f, 0.45f), frand_range(0.85f, 1.1f));
    }
    world_tick();
    weather_tick();
    raids_tick();
    player_tick();
    if (G.freeze_mobs) { ENT_FOREACH(o) if (o->kind == ENT_MOB) { o->prev_pos = o->pos; o->vel = v3(0, 0, 0); o->age++; o->prev_body_yaw = o->body_yaw; o->prev_head_yaw = o->head_yaw; } }
    else entities_tick();
    if (!g_player.dead && !G.freeze_mobs) mobs_spawn_tick();
    fx_tick();
    furnaces_tick();
    spawners_tick();
    if (G.ticks % 2 == 0) maps_tick();
    if (G.ticks % 10 == 0) recipes_unlock_tick();
    Entity *e = player_ent();
    if (e && e->age % 47 == 0 && g_player.gamemode == GM_SURVIVAL) {   // torch flicker particles near the player
        for (int k = 0; k < 6; k++) {
            ivec3 p = v3_floor(v3_add(e->pos, v3(frand_range(-8, 8), frand_range(-4, 4), frand_range(-8, 8))));
            if (BLOCKS[world_get_block(p.x, p.y, p.z)].base == B_TORCH || BLOCKS[world_get_block(p.x, p.y, p.z)].base == B_WALL_TORCH) fx_smoke(v3(p.x + 0.5f, p.y + 0.7f, p.z + 0.5f), 1, 0.02f, rgba_u32(80, 80, 80, 255));
        }
    }
}

// ============================================================================ scripted input (automated tests)
static char **script_lines;
static int script_n, script_pos;
static u64 script_wait_until;
static bool script_wait_ready;
static int key_by_name(const char *k) {
    static const struct { const char *n; int k; } T[] = {{"W", GLFW_KEY_W}, {"A", GLFW_KEY_A}, {"S", GLFW_KEY_S}, {"D", GLFW_KEY_D},
        {"SPACE", GLFW_KEY_SPACE}, {"SHIFT", GLFW_KEY_LEFT_SHIFT}, {"CTRL", GLFW_KEY_LEFT_CONTROL}, {"E", GLFW_KEY_E}, {"Q", GLFW_KEY_Q},
        {"ESC", GLFW_KEY_ESCAPE}, {"T", GLFW_KEY_T}, {"F3", GLFW_KEY_F3}, {"ENTER", GLFW_KEY_ENTER}};
    for (size_t i = 0; i < ARRAY_LEN(T); i++) if (!strcmp(T[i].n, k)) return T[i].k;
    if (strlen(k) == 1 && k[0] >= '1' && k[0] <= '9') return GLFW_KEY_1 + (k[0] - '1');
    if (strlen(k) == 1 && k[0] >= 'A' && k[0] <= 'Z') return GLFW_KEY_A + (k[0] - 'A');
    return -1;
}
static void script_load(const char *path) {
    char *txt = plat_read_file(path, NULL);
    if (!txt) { LOG_ERR("cannot read script %s", path); G.quit = true; return; }
    for (char *l = strtok(txt, "\r\n"); l; l = strtok(NULL, "\r\n")) {
        while (*l == ' ') l++;
        if (!*l || *l == '#') continue;
        script_lines = xrealloc(script_lines, sizeof(char *) * (script_n + 1));
        size_t len = strlen(l);
        script_lines[script_n] = xmalloc(len + 1);
        memcpy(script_lines[script_n++], l, len + 1);
    }
    free(txt);
}
static void script_update(void) {
    if (!script_lines) return;
    if (script_wait_ready) {
        if (G.screen == SCR_LOADING || !G.in_world) return;
        script_wait_ready = false;
    }
    while (script_pos < script_n && G.ticks >= script_wait_until && !script_wait_ready) {
        const char *l = script_lines[script_pos++];
        char cmd[32] = "", a[64] = "", b[64] = "", c[64] = "";
        int n = sscanf(l, "%31s %63s %63s %63s", cmd, a, b, c);
        if (n < 1) continue;
        if (!strcmp(cmd, "wait")) { script_wait_until = G.ticks + (u64)atoi(a); break; }
        else if (!strcmp(cmd, "waitready")) { script_wait_ready = true; break; }
        else if (!strcmp(cmd, "newworld")) { char p[128]; snprintf(p, sizeof p, "saves/%s", a); plat_remove_tree(p); game_start_world(a, strtoull(b, NULL, 10), atoi(c), true); }
        else if (!strcmp(cmd, "loadworld")) { u64 s; int gm; if (save_load_level(a, &s, &gm)) game_start_world(a, s, gm, false); else LOG_ERR("no world %s", a); }
        else if (!strcmp(cmd, "quit")) {
            LOG_INFO("SCRIPT DONE: %d checks, %d failed", commands_checks_run(), commands_checks_failed());
            G.quit = true;
            break;
        }
        else if (!strcmp(cmd, "key")) { int k = key_by_name(a); if (k >= 0) { g_win.key_down[k] = atoi(b) != 0; if (atoi(b)) g_win.key_pressed[k] = true; } }
        else if (!strcmp(cmd, "press")) { int k = key_by_name(a); if (k >= 0) g_win.key_pressed[k] = true; }
        else if (!strcmp(cmd, "mouse")) { int bt = atoi(a); g_win.mouse_down[bt] = atoi(b) != 0; if (atoi(b)) g_win.mouse_pressed[bt] = true; else g_win.mouse_released[bt] = true; }
        else if (!strcmp(cmd, "cursor")) { g_win.mouse_x = atof(a) * ui_scale; g_win.mouse_y = atof(b) * ui_scale; }
        else if (!command_execute(l)) LOG_WARN("script: unknown command '%s'", l);
        if (!strcmp(cmd, "screenshot")) { script_wait_until = G.ticks + 1; break; }   // capture before running later lines
    }
}

// ============================================================================ rendering helpers
static void outline_box(AABB b, u32 col) {
    vec3 c[8];
    for (int i = 0; i < 8; i++) c[i] = v3(i & 1 ? b.max.x : b.min.x, i & 2 ? b.max.y : b.min.y, i & 4 ? b.max.z : b.min.z);
    static const int E[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    b3d_set_mode(GL_LINES);
    b3d_set_texture(gfx_white_texture());
    for (int i = 0; i < 12; i++) { b3d_vertex(c[E[i][0]], 0, 0, col); b3d_vertex(c[E[i][1]], 0, 0, col); }
    b3d_flush();
    b3d_set_mode(GL_TRIANGLES);
}

static void tile_cube(vec3 mn, vec3 mx, int tile, u32 col) {
    static const u8 FC[6][4][3] = {
        {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}}, {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}},
        {{1, 0, 1}, {0, 0, 1}, {0, 0, 0}, {1, 0, 0}}, {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}},
        {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}}, {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}}};
    f32 u0 = (tile % ATLAS_TILES) / (f32)ATLAS_TILES, v0 = (tile / ATLAS_TILES) / (f32)ATLAS_TILES, d = 1.0f / ATLAS_TILES;
    for (int f = 0; f < 6; f++) {
        vec3 p[4];
        for (int i = 0; i < 4; i++) p[i] = v3(FC[f][i][0] ? mx.x : mn.x, FC[f][i][1] ? mx.y : mn.y, FC[f][i][2] ? mx.z : mn.z);
        f32 uv[8] = {u0, v0 + d, u0 + d, v0 + d, u0 + d, v0, u0, v0};
        b3d_quad_uv(p, uv, col);
    }
}

static void render_hand(f32 alpha, f32 aspect, f32 light) {
    Entity *e = player_ent();
    if (!e || g_player.dead || G.hide_hud) return;
    glClear(GL_DEPTH_BUFFER_BIT);
    mat4 proj = m4_perspective(DEG2RAD(70), aspect, 0.01f, 10);
    b3d_begin(&proj, (vec4){0, 0, 0, 1}, 1e9f, 2e9f);
    glDisable(GL_CULL_FACE);
    f32 sw = lerpf(g_player.prev_swing, g_player.swing, alpha);
    if (g_player.swing < g_player.prev_swing) sw = g_player.swing;
    f32 a1 = sinf(sw * sw * PI_F), a2 = sinf(sqrtf(sw) * PI_F);
    f32 bob = lerpf(g_player.prev_bob, g_player.bob, alpha) * PI_F, ba = lerpf(g_player.prev_bob_amount, g_player.bob_amount, alpha);
    bool vb = G.set.view_bob && !G.set.reduced_motion;
    f32 bx = vb ? sinf(bob) * ba * 0.03f : 0, by = vb ? -fabsf(cosf(bob)) * ba * 0.04f : 0;
    ItemStack h = *inventory_held(&g_player.inv);
    u32 col = rgba_f(light, light, light, 1);
    mat4 m = m4_translate(v3(0.56f - a2 * 0.3f + bx, -0.52f + sinf(sqrtf(sw) * 2 * PI_F) * 0.15f + by, -0.72f - a1 * 0.15f));
    if (g_player.eating_ticks > 0) {   // bring food to the mouth and chew
        f32 t = fminf(1, g_player.eating_ticks / 12.0f);
        m = m4_translate(v3(0.56f - 0.5f * t, -0.52f + 0.25f * t + fabsf(sinf(g_player.eating_ticks * 0.6f)) * 0.06f * t, -0.72f + 0.1f * t));
    }
    if (g_player.bow_ticks > 0) {
        f32 t = fminf(1, g_player.bow_ticks / (f32)TICKRATE);
        m = m4_translate(v3(0.25f, -0.35f + sinf(G.ticks * 0.8f) * 0.003f * t, -0.6f + t * 0.1f));
    }
    m = m4_mul(m, m4_rot_y(-a1 * 0.35f));
    m = m4_mul(m, m4_rot_z(-a2 * 0.3f));
    m = m4_mul(m, m4_rot_x(-a2 * 1.0f));
    if (stack_empty(h)) {
        static Texture skin;
        if (!skin.id) skin = texture_load("res/textures/entity/player.png");
        b3d_set_texture(skin.id);
        // forearm reaching in from the lower right: build the basis from shoulder and hand positions
        vec3 S = v3(0.86f - a2 * 0.25f + bx, -0.78f + by + sinf(sqrtf(sw) * 2 * PI_F) * 0.12f, -0.42f - a1 * 0.25f);
        vec3 H = v3(0.36f - a2 * 0.2f + bx, -0.36f + by - a2 * 0.1f, -0.88f - a1 * 0.2f);
        vec3 Y = v3_norm(v3_sub(S, H));
        vec3 X = v3_norm(v3_cross(Y, v3(0, 0, 1)));
        vec3 Z = v3_cross(X, Y);
        mat4 am = m4_identity();
        am.m[0] = X.x; am.m[1] = X.y; am.m[2] = X.z;
        am.m[4] = Y.x; am.m[5] = Y.y; am.m[6] = Y.z;
        am.m[8] = Z.x; am.m[9] = Z.y; am.m[10] = Z.z;
        am.m[12] = S.x; am.m[13] = S.y; am.m[14] = S.z;
        am = m4_mul(am, m4_translate(v3(0.06f, 0, 0)));
        model_draw_part(MODEL_HUMANOID, 2, am, col, 64, 32);
    } else if (ITEMS[h.id].icon == -1) {
        m = m4_mul(m, m4_rot_y(DEG2RAD(45)));
        m = m4_mul(m, m4_scale(v3(0.4f, 0.4f, 0.4f)));
        render_block_cube(m, ITEMS[h.id].block, col);
    } else {
        m = m4_mul(m, m4_translate(v3(0.0f, 0.12f, 0)));
        m = m4_mul(m, m4_rot_y(DEG2RAD(-70)));
        m = m4_mul(m, m4_rot_z(DEG2RAD(25)));
        m = m4_mul(m, m4_scale(v3(0.75f, 0.75f, 0.75f)));
        if (ITEMS[h.id].icon < 0) render_block_cube(m, ITEMS[h.id].block, col);
        else render_item_sprite(m, h.id, col);
    }
    b3d_flush();
    glEnable(GL_CULL_FACE);
}

static void render_world(f32 alpha) {
    Entity *e = player_ent();
    if (!e) return;
    vec3 pos = v3_lerp(e->prev_pos, e->pos, alpha);
    f32 eye_h = PLAYER_EYE - g_player.eye_offset;
    if (g_player.dead) eye_h = 0.3f + (PLAYER_EYE - 0.3f) * fmaxf(0, 1 - g_player.death_ticks / 20.0f);
    vec3 eye = v3_add(pos, v3(0, eye_h, 0));
    f32 yaw = e->yaw, pitch = e->pitch;
    f32 roll = 0;
    if (G.set.view_bob && !G.set.reduced_motion && !g_player.flying) {
        f32 b = lerpf(g_player.prev_bob, g_player.bob, alpha) * PI_F, a = lerpf(g_player.prev_bob_amount, g_player.bob_amount, alpha);
        vec3 right = v3(-cosf(yaw), 0, sinf(yaw));
        eye = v3_add(eye, v3_scale(right, sinf(b) * a * 0.05f));
        eye.y += fabsf(cosf(b)) * a * 0.07f - a * 0.035f;
        roll += sinf(b) * a * 0.012f;
    }
    if (g_player.hurt_tilt > 0.01f && !G.set.reduced_motion) roll += sinf(g_player.hurt_tilt * g_player.hurt_tilt * PI_F) * 0.18f;
    if (g_player.dead) roll = fminf(1, g_player.death_ticks / 20.0f) * 0.6f;
    f32 sh = G.set.reduced_motion ? 0 : fx_shake_amount();
    if (sh > 0) { yaw += frand_range(-sh, sh) * 0.05f; pitch += frand_range(-sh, sh) * 0.05f; }
    vec3 fwd = v3(sinf(yaw) * cosf(pitch), sinf(pitch), cosf(yaw) * cosf(pitch));
    vec3 right = v3_norm(v3_cross(fwd, v3(0, 1, 0)));
    vec3 up = v3_cross(right, fwd);
    vec3 up_r = v3_add(v3_scale(up, cosf(roll)), v3_scale(right, sinf(roll)));
    vec3 right_r = v3_norm(v3_cross(fwd, up_r));
    g_cam_pos = eye; g_cam_fwd = fwd; g_cam_right = right_r; g_cam_up = up_r;
    audio_set_listener(eye, yaw);

    f32 aspect = (f32)g_win.width / (f32)MAXI(1, g_win.height);
    f32 far_dist = g_world.render_dist * 16.0f;
    f32 fov = G.set.fov * (1.0f + g_player.fov_kick);
    u16 head = world_get_block((int)floorf(eye.x), (int)floorf(eye.y), (int)floorf(eye.z));
    if (block_is_water(head)) { fov *= 0.92f; head = B_WATER; }
    mat4 view = m4_look_dir(eye, fwd, up_r);
    mat4 proj = m4_perspective(DEG2RAD(fov), aspect, 0.05f, far_dist + 80);
    mat4 vp = m4_mul(proj, view);
    Frustum fr = frustum_from(&vp);
    f32 daylight = world_daylight();
    vec4 fog = sky_fog_color();
    if (g_level.lightning_flash > 0 && g_world.dim == DIM_OVERWORLD) { f32 fl = g_level.lightning_flash / (f32)(TICKRATE / 3) * 0.5f; fog = (vec4){fog.x + fl, fog.y + fl, fog.z + fl * 1.1f, 1}; }
    if (g_world.dim != DIM_OVERWORLD) {   // Nether / End fog comes from the biome
        u32 fc = BIOMES[world_biome_at((int)floorf(eye.x), (int)floorf(eye.y), (int)floorf(eye.z))].fog;
        fog = (vec4){((fc >> 16) & 255) / 255.0f, ((fc >> 8) & 255) / 255.0f, (fc & 255) / 255.0f, 1};
    }
    f32 fn = far_dist * 0.55f, ff = far_dist * 0.95f;
    // being in deep caves darkens the fog (Minecraft's "void fog" feel)
    f32 cave = g_world.dim != DIM_OVERWORLD ? 0 : clampf((SEA_LEVEL - 10 - eye.y) / 30.0f, 0, 0.85f) * (L_SKY(world_get_light((int)floorf(eye.x), (int)floorf(eye.y), (int)floorf(eye.z))) < 8);
    fog = (vec4){fog.x * (1 - cave), fog.y * (1 - cave), fog.z * (1 - cave), 1};
    if (g_world.dim == DIM_NETHER) { fn = 6; ff = fminf(ff, 96); }
    if (g_world.dim == DIM_SIFT) { fn = far_dist * 0.35f; }   // soft, hazy distance
    if (effect_has(e, EFF_BLINDNESS) || effect_has(e, EFF_DARKNESS)) { fog = (vec4){0, 0, 0, 1}; fn = 0; ff = effect_has(e, EFF_BLINDNESS) ? 5 : 15; }
    if (head == B_WATER) {
        f32 l = fmaxf(0.15f, L_SKY(world_get_light((int)floorf(eye.x), (int)floorf(eye.y), (int)floorf(eye.z))) / 15.0f * daylight);
        fog = (vec4){0.05f * l, 0.18f * l, 0.45f * l, 1}; fn = 0; ff = 24;
    } else if (block_is_lava(head)) { head = B_LAVA; fog = (vec4){0.8f, 0.3f, 0.05f, 1}; fn = 0; ff = 2.5f; }

    glViewport(0, 0, g_win.width, g_win.height);
    glClearColor(fog.x, fog.y, fog.z, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    if (head != B_WATER && head != B_LAVA) {
        if (g_world.dim == DIM_OVERWORLD) sky_render(&view, &proj, eye, far_dist);
        else if (g_world.dim == DIM_END) sky_render_space(&view, &proj, eye);
        else if (g_world.dim == DIM_SIFT) sky_render_sift(&view, &proj, eye, far_dist);
    }
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glDisable(GL_BLEND);
    world_render(&vp, eye, &fr, daylight, fog, fn, ff, 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    entities_render(alpha, &vp, fog, fn, ff);
    // block outline + crack overlay
    if (g_player.look.hit && !g_player.look_entity && G.screen == SCR_NONE && !G.hide_hud) {
        b3d_begin(&vp, fog, 1e9f, 2e9f);
        AABB box;
        if (world_block_selection(g_player.look.pos, g_player.look.block, &box)) {
            box.min = v3_sub(box.min, v3(0.002f, 0.002f, 0.002f)); box.max = v3_add(box.max, v3(0.002f, 0.002f, 0.002f));
            outline_box(box, rgba_u32(0, 0, 0, 110));
        }
        if (g_player.mining && g_player.mine_progress > 0) {
            int stage = clampi((int)(g_player.mine_progress * 10), 0, 9);
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(-1, -1);
            glDepthMask(GL_FALSE);
            b3d_set_texture(g_world.atlas.id);
            ivec3 p = g_player.mine_pos;
            AABB cb = {v3((f32)p.x, (f32)p.y, (f32)p.z), v3(p.x + 1.0f, p.y + 1.0f, p.z + 1.0f)};
            world_block_selection(p, world_get_block(p.x, p.y, p.z), &cb);
            tile_cube(v3_sub(cb.min, v3(0.001f, 0.001f, 0.001f)), v3_add(cb.max, v3(0.001f, 0.001f, 0.001f)), TILE_CRACK + stage, rgba_f(1, 1, 1, 0.75f));
            b3d_flush();
            glDepthMask(GL_TRUE);
            glDisable(GL_POLYGON_OFFSET_FILL);
        }
    }
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    world_render(&vp, eye, &fr, daylight, fog, fn, ff, 1);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    weather_render(&vp, eye, fog, fn, ff);
    fx_render(&vp, right_r, up_r, alpha, fog, fn, ff);
    render_hand(alpha, aspect, entity_light(e));
    glDisable(GL_DEPTH_TEST);
}

// ============================================================================ frame
void game_init(void) {
    memset(&G, 0, sizeof G);
    settings_load();
    G.screen = SCR_TITLE;
    G.tick_rate = 20;
    G.new_difficulty = DIFF_NORMAL;
}
void game_set_script(const char *path) {
    G.autotest_quiet = true;   // scripted runs: no tutorial hints
    script_load(path); G.autotest = true;
    g_world_sync = true;
    g_rng.s = 0x5EED5C21ull;   // scripted runs are reproducible: mob AI and drops no longer depend on the wall clock
}

static void handle_global_keys(void) {
    if (key_pressed(GLFW_KEY_F11)) { G.set.fullscreen = !G.set.fullscreen; settings_apply(); settings_save(); }
    if (key_pressed(KEY(KB_DEBUG))) G.debug = !G.debug;
    if (key_pressed(KEY(KB_HIDE_HUD))) G.hide_hud = !G.hide_hud;
    if (key_pressed(KEY(KB_SCREENSHOT))) {
        plat_mkdirs("screenshots");
        snprintf(g_pending_screenshot, sizeof g_pending_screenshot, "screenshots/screenshot_%llu.png", (unsigned long long)(plat_time() * 1000));
    }
    if (key_pressed(GLFW_KEY_ESCAPE)) {
        switch (G.screen) {
        case SCR_NONE: game_open_screen(SCR_PAUSE); break;
        case SCR_PAUSE: case SCR_CHAT: game_close_screen(); break;
        case SCR_SETTINGS: settings_apply(); settings_save(); game_open_screen(G.return_screen); break;
        case SCR_WORLDS: game_open_screen(SCR_TITLE); break;
        case SCR_CREATE: case SCR_CONFIRM_DELETE: game_open_screen(SCR_WORLDS); break;
        default: if (screen_is_container(G.screen)) game_close_screen(); break;
        }
        key_consume(GLFW_KEY_ESCAPE);
    }
    if (G.in_world && G.screen == SCR_NONE) {
        if (key_pressed(KEY(KB_INVENTORY))) { game_open_screen(SCR_INVENTORY); key_consume(GLFW_KEY_E); }
        else if (key_pressed(KEY(KB_CHAT))) { game_open_screen(SCR_CHAT); G.chat[0] = 0; }
        else if (key_pressed(KEY(KB_COMMAND))) { game_open_screen(SCR_CHAT); snprintf(G.chat, sizeof G.chat, "/"); }
    } else if (key_pressed(KEY(KB_INVENTORY)) && !G.text_focus && !(G.screen == SCR_INVENTORY && g_player.gamemode == GM_CREATIVE && G.creative_tab == TAB_COUNT) && screen_is_container(G.screen)) {
        game_close_screen();
    }
}

// ---- controllers (GLFW standard gamepad mapping): turned into the same key / mouse input as keyboard and mouse,
// through the player's own key bindings, so rebinding, toggle sneak and every screen work with a pad too.
//   in game: left stick move, right stick look, A jump, B sneak, left-stick click sprint, Y inventory, X drop,
//            LB / RB hotbar, RT attack / break, LT use / place, Start pause
//   in menus: left stick moves the cursor, A clicks, B goes back
// Tests inject a virtual pad with the "gamepad" script command.
GLFWgamepadstate g_fake_pad; bool g_fake_pad_on;
int g_pad_frames;
static int forced[16], nforced_keys;   // keys the pad held down last frame
static void force_key(int k) { if (k > 0 && k <= GLFW_KEY_LAST) { g_win.key_down[k] = true; if (nforced_keys < 16) forced[nforced_keys++] = k; } }
static void gamepad_poll(f64 dt) {
    static GLFWgamepadstate prev;
    GLFWgamepadstate st;
    for (int i = 0; i < nforced_keys; i++) g_win.key_down[forced[i]] = g_win.handle && glfwGetKey(g_win.handle, forced[i]) == GLFW_PRESS;   // back to the real keyboard state
    nforced_keys = 0;
    if (g_fake_pad_on) st = g_fake_pad;
    else if (!G.set.controller || !glfwJoystickIsGamepad(GLFW_JOYSTICK_1) || !glfwGetGamepadState(GLFW_JOYSTICK_1, &st)) { memset(&prev, 0, sizeof prev); return; }
    g_pad_frames++;
    #define BTN(b) (st.buttons[b] == GLFW_PRESS)
    #define EDGE(b) (st.buttons[b] == GLFW_PRESS && prev.buttons[b] != GLFW_PRESS)
    #define DZ(v) (fabsf(v) < 0.18f ? 0.0f : (v))
    f32 lx = DZ(st.axes[GLFW_GAMEPAD_AXIS_LEFT_X]), ly = DZ(st.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
    f32 rx = DZ(st.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]), ry = DZ(st.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
    bool lt = st.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] > 0.3f, rt = st.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.3f;
    bool plt = prev.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] > 0.3f, prt = prev.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.3f;
    if (G.screen != SCR_NONE) {   // menus: a virtual cursor
        g_win.mouse_x = clampf((f32)(g_win.mouse_x + lx * 900 * dt), 0, (f32)g_win.width);
        g_win.mouse_y = clampf((f32)(g_win.mouse_y + ly * 900 * dt), 0, (f32)g_win.height);
        if (EDGE(GLFW_GAMEPAD_BUTTON_A)) { g_win.mouse_down[0] = true; g_win.mouse_pressed[0] = true; }
        else if (!BTN(GLFW_GAMEPAD_BUTTON_A) && prev.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS) { g_win.mouse_down[0] = false; g_win.mouse_released[0] = true; }
        if (EDGE(GLFW_GAMEPAD_BUTTON_B) || EDGE(GLFW_GAMEPAD_BUTTON_START)) g_win.key_pressed[GLFW_KEY_ESCAPE] = true;
        g_win.scroll += ry * -6 * dt;
    } else {
        int k;
        if (ly < -0.3f) force_key(KEY(KB_FORWARD));
        if (ly > 0.3f) force_key(KEY(KB_BACK));
        if (lx < -0.3f) force_key(KEY(KB_LEFT));
        if (lx > 0.3f) force_key(KEY(KB_RIGHT));
        k = KEY(KB_JUMP); if (BTN(GLFW_GAMEPAD_BUTTON_A)) force_key(k); if (EDGE(GLFW_GAMEPAD_BUTTON_A)) g_win.key_pressed[k] = true;
        k = KEY(KB_SNEAK); if (BTN(GLFW_GAMEPAD_BUTTON_B)) force_key(k); if (EDGE(GLFW_GAMEPAD_BUTTON_B)) g_win.key_pressed[k] = true;
        k = KEY(KB_SPRINT); if (BTN(GLFW_GAMEPAD_BUTTON_LEFT_THUMB)) force_key(k); if (EDGE(GLFW_GAMEPAD_BUTTON_LEFT_THUMB)) g_win.key_pressed[k] = true;
        if (EDGE(GLFW_GAMEPAD_BUTTON_Y)) g_win.key_pressed[KEY(KB_INVENTORY)] = true;
        if (EDGE(GLFW_GAMEPAD_BUTTON_X)) g_win.key_pressed[KEY(KB_DROP)] = true;
        if (EDGE(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER)) g_win.scroll += 1;
        if (EDGE(GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER)) g_win.scroll -= 1;
        if (EDGE(GLFW_GAMEPAD_BUTTON_START)) g_win.key_pressed[GLFW_KEY_ESCAPE] = true;
        g_win.mouse_dx += rx * fabsf(rx) * 1400 * dt; g_win.mouse_dy += ry * fabsf(ry) * 1000 * dt;
        if (rt) { g_win.mouse_down[0] = true; if (!prt) g_win.mouse_pressed[0] = true; } else if (prt) { g_win.mouse_down[0] = false; g_win.mouse_released[0] = true; }
        if (lt) { g_win.mouse_down[1] = true; if (!plt) g_win.mouse_pressed[1] = true; } else if (plt) { g_win.mouse_down[1] = false; g_win.mouse_released[1] = true; }
    }
    prev = st;
    #undef BTN
    #undef EDGE
    #undef DZ
}
void game_frame(f64 dt) {
    G.frames++;
    if (dt > 0) G.fps = G.fps * 0.95f + (f32)(1.0 / dt) * 0.05f;
    script_update();
    gamepad_poll(dt);
    handle_global_keys();
    if (G.screen == SCR_PAUSE && !g_win.focused && G.autotest) game_close_screen();
    // pause automatically when the window loses focus (not during automated tests)
    if (G.in_world && G.screen == SCR_NONE && !g_win.focused && !G.autotest) game_open_screen(SCR_PAUSE);
    window_set_grab(G.in_world && G.screen == SCR_NONE && g_win.focused && !G.autotest);

    if (G.in_world) {
        Entity *e = player_ent();
        if (G.screen == SCR_LOADING) {
            world_update(e->pos, 12);
            if (world_area_ready(e->pos, 2) || ++G.loading_ticks > 60 * 30) {
                int x = (int)floorf(e->pos.x), z = (int)floorf(e->pos.z);
                if (pending_portal) { finish_dimension_arrival(e); x = (int)floorf(e->pos.x); z = (int)floorf(e->pos.z); }
                if (G.snap_ground && g_world.dim == DIM_NETHER) {   // find an air pocket with a solid, non-lava floor
                    G.snap_ground = false;
                    for (int dz = 0; dz < 24; dz++) for (int dx = 0; dx < 24; dx++) for (int y = 110; y > NETHER_LAVA_LEVEL; y--) {
                        int xx = x + dx - 12, zz = z + dz - 12;
                        u16 fl = world_get_block(xx, y - 1, zz);
                        if (BLOCKS[fl].solid && !BLOCKS[fl].liquid && block_is_air(world_get_block(xx, y, zz)) && block_is_air(world_get_block(xx, y + 1, zz))) {
                            e->pos = e->prev_pos = v3(xx + 0.5f, (f32)y, zz + 0.5f); dz = dx = 99; break;
                        }
                    }
                }
                if (G.snap_ground) { G.snap_ground = false; int ty = world_top_y(x, z); if (ty > 0) { e->pos.y = e->prev_pos.y = ty + 1.0f; while (BLOCKS[world_get_block(x, (int)e->pos.y, z)].solid || BLOCKS[world_get_block(x, (int)e->pos.y, z)].liquid || block_is_leaves(world_get_block(x, (int)e->pos.y - 1, z))) e->pos.y = e->prev_pos.y = e->pos.y + 1; } }
                if (fresh_world || BLOCKS[world_get_block(x, (int)floorf(e->pos.y), z)].solid) {
                    int y = find_safe_y(x, z, fresh_world ? g_world.spawn.y : (int)e->pos.y);
                    e->pos.y = e->prev_pos.y = (f32)y;
                    if (fresh_world) g_world.spawn.y = y;
                }
                fresh_world = false;
                game_close_screen();
                if (!g_player.portal_cooldown) game_message(rgba_u32(255, 255, 85, 255), "Welcome to %s! Press E for inventory, T for chat, /help for commands.", g_world.name);
            }
        } else {
            if (game_paused()) G.ticks++;   // menus over a paused world still advance timers
            else {
                G.tick_acc += dt * (G.tick_rate > 0 ? G.tick_rate / 20.0 : 1.0);   // /tick rate
                int n = 0;
                while (G.tick_acc >= DT && n < 8) {
                    G.title_t++; if (G.actionbar_t > 0) G.actionbar_t--;
                    if (G.tick_frozen && G.tick_step <= 0) { player_tick(); fx_tick(); G.ticks++; }   // /tick freeze: only the player moves
                    else { if (G.tick_step > 0) G.tick_step--; game_tick(); }
                    G.tick_acc -= DT; n++;
                }
                if (n == 8) G.tick_acc = 0;   // can't keep up: drop time instead of spiralling
            }
            G.alpha = (f32)clampf((f32)(G.tick_acc / DT), 0, 1);
            player_frame((f32)dt);
            world_update(e->pos, 4.0);
            if (++G.autosave_timer > 60 * 60 * 5 && !G.autotest) { G.autosave_timer = 0; game_save(); game_message(rgba_u32(170, 170, 170, 255), "Autosaved"); }
        }
        if (G.screen != SCR_LOADING) render_world(G.alpha);
    } else {
        G.ticks++;   // keeps timers (and test-script waits) moving on menus
        glViewport(0, 0, g_win.width, g_win.height);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    // UI
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    ui_begin(G.set.gui_scale);
    if (G.in_world && G.screen != SCR_LOADING) hud_render();
    screens_render();
    ui_end();
    audio_set_sift(G.in_world && g_world.dim == DIM_SIFT);
    audio_update(G.in_world, G.in_world && world_is_night());
    if (g_pending_screenshot[0]) {
        if (gfx_screenshot(g_pending_screenshot)) game_message(rgba_u32(170, 170, 170, 255), "Saved screenshot %s", g_pending_screenshot);
        g_pending_screenshot[0] = 0;
    }
}

void game_shutdown(void) {
    if (G.in_world) game_quit_to_title();
    settings_save();
}

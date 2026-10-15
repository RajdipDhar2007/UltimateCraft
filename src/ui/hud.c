#include "../audio/audio.h"
#include "../world/rules.h"
#include "../game/raids.h"
#include "ui.h"
#include "../game/maps.h"
#include "screens.h"
#include "../game/game.h"
#include "../game/player.h"
#include "../world/world.h"

static int last_selected = -1;
static f64 select_time;

static void hotbar(void) {
    f32 x = ui_w / 2.0f - 91, y = ui_h - 22.0f;
    ui_rect(x, y, 182, 22, rgba_u32(0, 0, 0, 160));
    ui_rect(x + 1, y + 1, 180, 20, rgba_u32(120, 120, 120, 120));
    for (int i = 0; i < 9; i++) {
        ui_rect(x + 2 + i * 20, y + 2, 18, 18, rgba_u32(40, 40, 40, 170));
        ui_item(x + 3 + i * 20, y + 3, g_player.inv.slots[i], true);
    }
    int s = g_player.inv.selected;
    f32 sx = x - 1 + s * 20;
    ui_rect(sx, y - 1, 24, 2, COL_WHITE); ui_rect(sx, y + 21, 24, 2, COL_WHITE);
    ui_rect(sx, y - 1, 2, 24, COL_WHITE); ui_rect(sx + 22, y - 1, 2, 24, COL_WHITE);
    ui_rect(sx + 2, y + 1, 20, 1, rgba_u32(0, 0, 0, 160));
}

static void stats(void) {
    Entity *e = player_ent();
    if (!e || !gm_survival_like(g_player.gamemode)) return;
    f32 x0 = ui_w / 2.0f - 91, y = ui_h - 39.0f;
    // hearts
    int hp = (int)ceilf(e->health);
    bool flash = e->hurt_time > 0 && (e->hurt_time / 3) % 2;
    for (int i = 0; i < 10; i++) {
        f32 hx = x0 + i * 8, hy = y;
        if (hp <= 4) hy += (f32)((int)(plat_time_wrap() * 30 + i * 7) % 3) - 1;   // shake at low health
        u32 hc = effect_has(e, EFF_POISON) ? rgba_u32(150, 200, 60, 255) : effect_has(e, EFF_WITHER) ? rgba_u32(60, 60, 60, 255) : COL_WHITE;
        ui_icon(flash ? 3 : 0, hx, hy, COL_WHITE);
        if (hp >= i * 2 + 2) ui_icon(1, hx, hy, hc);
        else if (hp == i * 2 + 1) ui_icon(2, hx, hy, hc);
    }
    int ab = (int)ceilf(e->absorption);   // golden hearts
    for (int i = 0; i < (ab + 1) / 2 && i < 10; i++) {
        ui_icon(0, x0 + i * 8, y - 10, COL_WHITE);
        ui_icon(ab >= i * 2 + 2 ? 1 : 2, x0 + i * 8, y - 10, rgba_u32(255, 210, 40, 255));
    }
    // armor
    int ap = inventory_armor_points(&g_player.inv);
    if (ap > 0) for (int i = 0; i < 10; i++) {
        f32 ax = x0 + i * 8, ay = y - (ab > 0 ? 20 : 10);
        ui_icon(ap >= i * 2 + 2 ? 8 : ap == i * 2 + 1 ? 9 : 7, ax, ay, COL_WHITE);
    }
    // food (right to left)
    f32 fx = ui_w / 2.0f + 91 - 9;
    for (int i = 0; i < 10; i++) {
        f32 ix = fx - i * 8, iy = y;
        if (g_player.saturation <= 0 && g_player.food < 6) iy += (f32)((int)(plat_time_wrap() * 20 + i * 5) % 3) - 1;
        u32 fc = effect_has(e, EFF_HUNGER) ? rgba_u32(140, 190, 60, 255) : COL_WHITE;
        ui_icon(4, ix, iy, COL_WHITE);
        if (g_player.food >= i * 2 + 2) ui_icon(5, ix, iy, fc);
        else if (g_player.food == i * 2 + 1) ui_icon(6, ix, iy, fc);
    }
    // air bubbles
    if (e->air < 900) {
        int full = (int)ceilf(MAXI(0, e->air) / 90.0f);
        for (int i = 0; i < 10; i++) if (i < full) ui_icon(10, fx - i * 8, y - 10, COL_WHITE);
    }
}

static void xp_bar(void) {
    f32 x = ui_w / 2.0f - 91, y = ui_h - 29.0f;
    if (gm_survival_like(g_player.gamemode)) {
        ui_rect(x, y, 182, 5, rgba_u32(0, 0, 0, 255));
        ui_rect(x + 1, y + 1, 180, 3, rgba_u32(30, 50, 10, 255));
        ui_rect(x + 1, y + 1, 180 * g_player.xp_progress, 3, rgba_u32(128, 255, 32, 255));
        ui_rect(x + 1, y + 1, 180 * g_player.xp_progress, 1, rgba_u32(200, 255, 140, 255));
    }
    if (g_player.xp_level > 0) {
        char buf[16]; snprintf(buf, sizeof buf, "%d", g_player.xp_level);
        f32 tx = ui_w / 2.0f - ui_text_width(buf) / 2.0f, ty = y - 7;
        for (int dx = -1; dx <= 1; dx++) for (int dy = -1; dy <= 1; dy++) if (dx || dy) ui_text(tx + dx, ty + dy, buf, rgba_u32(0, 0, 0, 255), false);
        ui_text(tx, ty, buf, rgba_u32(128, 255, 32, 255), false);
    }
}

static void crosshair(void) {
    f32 cx = ui_w / 2.0f, cy = ui_h / 2.0f;
    b2d_set_invert(true);
    ui_rect(cx - 5, cy - 0.5f, 11, 1, COL_WHITE);
    ui_rect(cx - 0.5f, cy - 5, 1, 4.5f, COL_WHITE);
    ui_rect(cx - 0.5f, cy + 0.5f, 1, 4.5f, COL_WHITE);
    b2d_set_invert(false);
    f32 s = player_attack_strength();
    if (s < 1.0f && gm_survival_like(g_player.gamemode)) {
        ui_rect(cx - 8, cy + 9, 16, 4, rgba_u32(40, 40, 40, 200));
        ui_rect(cx - 7, cy + 10, 14 * s, 2, rgba_u32(230, 230, 230, 230));
    }
}

static void overlays(void) {
    Entity *e = player_ent();
    if (!e) return;
    u16 head = world_get_block((int)floorf(e->pos.x), (int)floorf(e->pos.y + PLAYER_EYE - g_player.eye_offset), (int)floorf(e->pos.z));
    if (BLOCKS[head].base == B_SIFT_PORTAL) ui_rect(0, 0, (f32)ui_w, (f32)ui_h, rgba_u32(255, 230, 140, (int)clampf(60 + g_player.portal_ticks * 1.2f, 0, 200)));
    else if (BLOCKS[head].base == B_NETHER_PORTAL || g_player.portal_ticks > 0) ui_rect(0, 0, (f32)ui_w, (f32)ui_h, rgba_u32(120, 40, 200, (int)clampf(60 + g_player.portal_ticks * 0.6f, 0, 220)));
    if (g_player.inv.armor[0].id == I_CARVED_PUMPKIN) { ui_rect(0, 0, ui_w * 0.25f, (f32)ui_h, rgba_u32(30, 20, 5, 220)); ui_rect(ui_w * 0.75f, 0, ui_w * 0.25f, (f32)ui_h, rgba_u32(30, 20, 5, 220)); }
    if (g_player.using_spyglass) { ui_rect(0, 0, ui_w * 0.2f, (f32)ui_h, COL_BLACK); ui_rect(ui_w * 0.8f, 0, ui_w * 0.2f, (f32)ui_h, COL_BLACK); }
    if (effect_has(e, EFF_NAUSEA)) ui_rect(0, 0, (f32)ui_w, (f32)ui_h, rgba_u32(80, 160, 60, (int)(40 + 30 * sinf((f32)plat_time_wrap() * 3))));
    if (g_player.sleeping) ui_rect(0, 0, (f32)ui_w, (f32)ui_h, rgba_u32(0, 0, 10, (int)clampf(g_player.use_ticks * 2.0f, 0, 230)));
    if (block_is_water(head)) ui_rect(0, 0, (f32)ui_w, (f32)ui_h, rgba_u32(20, 50, 160, 90));
    if (BLOCKS[head].base == B_LAVA) ui_rect(0, 0, (f32)ui_w, (f32)ui_h, rgba_u32(230, 90, 10, 200));
    if (e->fire > 0 && gm_survival_like(g_player.gamemode)) {
        f32 t = (f32)plat_time_wrap();
        for (int i = 0; i < 12; i++) {
            f32 h = 30 + 14 * sinf(t * 9 + i * 1.7f);
            ui_rect(i * ui_w / 12.0f, ui_h - h, ui_w / 12.0f + 1, h, rgba_u32(255, 120 + i * 8 % 60, 20, 120));
        }
    }
    if (e->hurt_time > 0) {   // red damage vignette
        f32 a = e->hurt_time / 18.0f * 0.45f;
        int b = 24;
        for (int i = 0; i < 6; i++) {
            u32 c = rgba_f(0.7f, 0, 0, a * (1.0f - i / 6.0f));
            ui_rect(0, i * b / 6.0f, (f32)ui_w, b / 6.0f, c);
            ui_rect(0, ui_h - (i + 1) * b / 6.0f, (f32)ui_w, b / 6.0f, c);
            ui_rect(i * b / 6.0f, 0, b / 6.0f, (f32)ui_h, c);
            ui_rect(ui_w - (i + 1) * b / 6.0f, 0, b / 6.0f, (f32)ui_h, c);
        }
    }
}

static void held_name(void) {
    if (g_player.inv.selected != last_selected) { last_selected = g_player.inv.selected; select_time = plat_time_wrap(); }
    ItemStack h = *inventory_held(&g_player.inv);
    f64 t = plat_time_wrap() - select_time;
    if (stack_empty(h) || t > 2.0) return;
    f32 a = t < 1.5 ? 1.0f : (f32)(1 - (t - 1.5) / 0.5);
    ui_text_center(ui_w / 2.0f, ui_h - 59.0f - (g_player.gamemode == GM_CREATIVE ? -16 : 0), item_name(h.id), rgba_f(1, 1, 1, a), true);
}

static void chat_lines(bool open) {
    f64 now = plat_time_wrap();
    f32 y = ui_h - 50.0f;
    for (int i = G.nmsgs - 1; i >= 0; i--) {
        ChatLine *l = &G.msgs[i];
        f64 age = now - l->time;
        if (!open && age > 10) continue;
        f32 a = open ? 1.0f : (f32)fmin(1.0, (10 - age) / 1.0);
        ui_rect(2, y - 1, ui_text_width(l->text) + 4.0f, 10, rgba_f(0, 0, 0, 0.4f * a));
        u32 c = (l->color & 0x00FFFFFFu) | ((u32)(a * 255) << 24);
        ui_text(4, y, l->text, c, true);
        y -= 10;
    }
}

static void debug_screen(void) {
    ui_text(2, ui_h - 10.0f, "UltimateCraft " UC_VERSION, rgba_u32(200, 200, 200, 255), true);
    Entity *e = player_ent();
    if (!e) return;
    char lines[14][128];
    int n = 0;
    ivec3 b = v3_floor(e->pos);
    Column *c = world_column_at(b.x, b.z);
    u16 l = world_get_light(b.x, b.y, b.z);
    const char *dirs[4] = {"south (+Z)", "west (-X)", "north (-Z)", "east (+X)"};
    int di = (int)floorf(fmodf(-e->yaw / (PI_F / 2) + 0.5f + 8.0f, 4.0f)) % 4;
    snprintf(lines[n++], 128, "UltimateCraft  %.0f fps", G.fps);
    snprintf(lines[n++], 128, "XYZ: %.2f / %.2f / %.2f", e->pos.x, e->pos.y - world_y_offset(), e->pos.z);
    snprintf(lines[n++], 128, "Block: %d %d %d  Chunk: %d %d", b.x, b.y - world_y_offset(), b.z, b.x >> 4, b.z >> 4);
    snprintf(lines[n++], 128, "Facing: %s", dirs[di]);
    (void)c;
    { int bi = world_biome_at(b.x, b.y, b.z); snprintf(lines[n++], 128, "Biome: %s   Dimension: %s", BIOMES[bi].key, dim_info(g_world.dim)->id); }
    snprintf(lines[n++], 128, "Light: sky %d  block %d/%d/%d  level %d", L_SKY(l), L_R(l), L_G(l), L_B(l), world_light_level(b));
    snprintf(lines[n++], 128, "Time: day %llu, %02d:%02d", (unsigned long long)(world_daytime() / DAY_TICKS + 1),
             (int)((world_daytime() % DAY_TICKS) * 24 / DAY_TICKS + 6) % 24, (int)((world_daytime() % DAY_TICKS) * 24 * 60 / DAY_TICKS) % 60);
    snprintf(lines[n++], 128, "Columns: %d loaded, %d sections drawn", g_world.loaded, g_world.sections_drawn);
    snprintf(lines[n++], 128, "Mobs: %d hostile, %d passive", g_ents.count_mobs_hostile, g_ents.count_mobs_passive);
    snprintf(lines[n++], 128, "Seed: %llu   Mode: %s   Difficulty: %s%s", (unsigned long long)g_world.seed, (const char *[]){"Survival", "Creative", "Adventure", "Spectator"}[g_player.gamemode & 3], DIFFICULTY_NAMES[g_level.difficulty], g_level.hardcore ? " (Hardcore)" : "");
    if (g_player.look.hit) snprintf(lines[n++], 128, "Looking at: ultimatecraft:%s[%d] (%d %d %d)", BLOCKS[g_player.look.block].key, BLOCKS[g_player.look.block].var, g_player.look.pos.x, g_player.look.pos.y - world_y_offset(), g_player.look.pos.z);
    { Entity *t = entity_get(g_player.look_entity); if (t && t->kind == ENT_MOB) snprintf(lines[n++], 128, "Targeted entity: ultimatecraft:%s  %.0f/%.0f HP", MOB_DEFS[t->mob].key, t->health, t->max_health); }
    snprintf(lines[n++], 128, "Food %d  Sat %.1f  Exh %.2f", g_player.food, g_player.saturation, g_player.exhaustion);
    for (int i = 0; i < n; i++) {
        int w = ui_text_width(lines[i]);
        ui_rect(1, 1 + i * 10.0f, w + 3.0f, 10, rgba_u32(0, 0, 0, 110));
        ui_text(2, 2 + i * 10.0f, lines[i], rgba_u32(224, 224, 224, 255), false);
    }
}

static void effects_list(void) {
    Entity *e = player_ent();
    if (!e) return;
    int row = 0;
    for (int i = 1; i < EFFECT_COUNT; i++) {
        if (!effect_has(e, i)) continue;
        char b[64];
        int s = e->eff_time[i] / TICKRATE;
        static const char *R[] = {"", " II", " III", " IV", " V"};
        snprintf(b, sizeof b, "%s%s %d:%02d", EFFECT_NAMES[i], R[MINI(4, e->eff_amp[i])], s / 60, s % 60);
        int wdt = ui_text_width(b);
        ui_rect(ui_w - wdt - 6.0f, 2 + row * 11.0f, wdt + 4.0f, 10, rgba_u32(0, 0, 0, 130));
        ui_text(ui_w - wdt - 4.0f, 3 + row * 11.0f, b, rgba_u32(230, 230, 255, 255), false);
        row++;
    }
}
static void boss_bar(void) {
    Entity *b = entity_get(g_ents.boss);
    if (!b || b->kind != ENT_MOB) return;
    f32 w = 182, x = ui_w / 2.0f - w / 2, y = 12;
    char nm[128];
    if (b->mob == MOB_COSMIC_TYRANT && b->phase > 0) snprintf(nm, sizeof nm, "%s - Phase %s", MOB_DEFS[b->mob].name, b->phase == 1 ? "I" : b->phase == 2 ? "II" : "III");
    else snprintf(nm, sizeof nm, "%s", MOB_DEFS[b->mob].name);
    ui_text_center(ui_w / 2.0f, 2, nm, COL_WHITE, true);
    u32 col = b->mob == MOB_ENDER_DRAGON ? rgba_u32(80, 200, 240, 255) : b->mob == MOB_COSMIC_TYRANT ? rgba_u32(250, 200, 70, 255) : rgba_u32(120, 80, 220, 255);
    ui_rect(x, y, w, 5, rgba_u32(40, 20, 40, 220));
    ui_rect(x, y, w * clampf(b->health / b->max_health, 0, 1), 5, col);
}
static void held_info(void) {   // compass / clock / map read-outs above the hotbar
    Entity *e = player_ent();
    ItemStack h = *inventory_held(&g_player.inv);
    if (!e || !h.id) return;
    char b[96] = "";
    if (h.id == I_COMPASS || h.id == I_RECOVERY_COMPASS) {
        ivec3 s = g_world.spawn;
        if (g_world.dim != DIM_OVERWORLD) snprintf(b, sizeof b, "The compass spins wildly");
        else snprintf(b, sizeof b, "Spawn: %d blocks away", (int)sqrtf((s.x - e->pos.x) * (s.x - e->pos.x) + (s.z - e->pos.z) * (s.z - e->pos.z)));
    } else if (h.id == I_CLOCK) {
        int t = (int)((world_daytime() % DAY_TICKS) * 24 * 60 / DAY_TICKS) + 360;
        snprintf(b, sizeof b, "%02d:%02d", (t / 60) % 24, t % 60);
    } else if (h.id == I_FILLED_MAP || h.id == I_MAP) snprintf(b, sizeof b, "X %d  Z %d", (int)e->pos.x, (int)e->pos.z);
    else if (g_player.crossbow_loaded && h.id == I_CROSSBOW) snprintf(b, sizeof b, "Loaded");
    if (b[0]) ui_text_center(ui_w / 2.0f, ui_h - 72.0f, b, rgba_u32(255, 255, 160, 255), true);
}

// /title: fade in, stay, fade out (Minecraft ticks), big title + subtitle, and the action bar
static void titles(void) {
    if (G.title[0] || G.subtitle[0]) {
        int t = G.title_t / MC_TICK_DIV, tin = MAXI(1, G.title_in), st = G.title_stay, tout = MAXI(1, G.title_out);
        if (t > tin + st + tout) { G.title[0] = G.subtitle[0] = 0; }
        else {
            f32 a = t < tin ? (f32)t / tin : t < tin + st ? 1.0f : 1.0f - (f32)(t - tin - st) / tout;
            u32 col = rgba_f(1, 1, 1, clampf(a, 0.05f, 1));
            if (G.title[0]) { f32 sc = 4; ui_text_scaled(ui_w / 2.0f - ui_text_width(G.title) * sc / 2, ui_h / 2.0f - 40, sc, G.title, col, true); }
            if (G.subtitle[0]) { f32 sc = 2; ui_text_scaled(ui_w / 2.0f - ui_text_width(G.subtitle) * sc / 2, ui_h / 2.0f + 2, sc, G.subtitle, col, true); }
        }
    }
    if (G.actionbar_t > 0 && G.actionbar[0]) ui_text_center(ui_w / 2.0f, ui_h - 72.0f, G.actionbar, rgba_f(1, 1, 1, fminf(1, G.actionbar_t / 20.0f)), true);
}
void hud_render(void) {
    overlays();
    if (G.hide_hud) return;
    if (G.screen == SCR_NONE) crosshair();
    {   // a filled map held in the main hand is shown like Minecraft's map-in-hand view
        ItemStack h = *inventory_held(&g_player.inv);
        if (h.id == I_FILLED_MAP && G.screen == SCR_NONE) { f32 sz = fminf(128.0f, ui_h * 0.45f); map_draw(h.data, ui_w / 2.0f - sz / 2, ui_h - sz - 48.0f, sz, true); }
    }
    hotbar();
    stats();
    xp_bar();
    held_name();
    held_info();
    effects_list();
    if (raid_active()) raids_render_hud(); else boss_bar();
    if (G.set.subtitles) {   // subtitles, bottom right; arrows show which side a sound came from
        Caption cs[6]; int n = audio_captions(cs, 6);
        for (int i = 0; i < n; i++) {
            const Caption *c = &cs[n - 1 - i];
            char line[64]; snprintf(line, sizeof line, "%s %s %s", c->positional && c->pan < -0.25f ? "<" : " ", c->text, c->positional && c->pan > 0.25f ? ">" : " ");
            f32 w = ui_text_width(line) + 6, x = ui_w - w - 2, y = ui_h - 60 - i * 11.0f;
            ui_rect(x, y, w, 10, rgba_u32(0, 0, 0, 170));
            ui_text(x + 3, y + 1, line, COL_WHITE, false);
        }
    }
    if (G.hint[0]) {   // tutorial hint, top centre
        f32 w = ui_text_width(G.hint) + 12;
        ui_rect(ui_w / 2.0f - w / 2, 24, w, 14, rgba_u32(0, 0, 0, 150));
        ui_text_center(ui_w / 2.0f, 27, G.hint, rgba_u32(255, 255, 170, 255), true);
    }
    if (G.toast_t > 0) {   // advancement toast
        G.toast_t--;
        f32 w = ui_text_width(G.toast) + 12, x = ui_w - w - 4, y = 4 - (G.toast_t < 20 ? (20 - G.toast_t) : 0);
        ui_rect(x, y, w, 16, rgba_u32(20, 20, 30, 220)); ui_rect(x, y + 15, w, 1, rgba_u32(255, 220, 120, 255));
        ui_text(x + 6, y + 4, G.toast, rgba_u32(255, 255, 160, 255), true);
    }
    if (g_sb_sidebar >= 0 && g_sb_sidebar < g_sb_nobj) {   // scoreboard sidebar (top 15, highest first)
        int idx[15], n = 0;
        for (int i = 0; i < g_sb_n; i++) {
            if (g_sb[i].obj != g_sb_sidebar || g_sb[i].holder[0] == '#') continue;
            int p = n < 15 ? n++ : 14;
            if (p == 14 && n == 15 && g_sb[idx[14]].value >= g_sb[i].value) continue;
            idx[p] = i;
            for (int j = p; j > 0 && g_sb[idx[j]].value > g_sb[idx[j - 1]].value; j--) { int t = idx[j]; idx[j] = idx[j - 1]; idx[j - 1] = t; }
        }
        f32 w = 110, x = ui_w - w - 2, y = ui_h / 2.0f - (n + 1) * 5.0f;
        ui_rect(x, y, w, 10, rgba_u32(0, 0, 0, 110));
        ui_text_center(x + w / 2, y + 1, g_sb_obj[g_sb_sidebar].disp, COL_WHITE, false);
        for (int i = 0; i < n; i++) {
            f32 yy = y + 10 + i * 10;
            ui_rect(x, yy, w, 10, rgba_u32(0, 0, 0, 80));
            const char *h = g_sb[idx[i]].holder;
            ui_text(x + 2, yy + 1, h[0] == 'e' && h[1] >= '0' && h[1] <= '9' ? "Entity" : h, rgba_u32(200, 200, 200, 255), false);
            char v[16]; snprintf(v, sizeof v, "%d", g_sb[idx[i]].value);
            ui_text(x + w - 2 - ui_text_width(v), yy + 1, v, rgba_u32(255, 85, 85, 255), false);
        }
    }
    if (!stack_empty(g_player.inv.offhand)) { f32 x = ui_w / 2.0f - 91 - 29; ui_rect(x, ui_h - 23.0f, 22, 22, rgba_u32(0, 0, 0, 160)); ui_item(x + 3, ui_h - 20.0f, g_player.inv.offhand, true); }
    titles();
    chat_lines(G.screen == SCR_CHAT);
    if (G.debug && !rule(GR_REDUCED_DEBUG_INFO)) debug_screen();
    else if (G.debug) { char b[64]; snprintf(b, sizeof b, "UltimateCraft  %.0f fps (reduced debug info)", G.fps); ui_text(2, 2, b, COL_WHITE, true); }
    else if (G.set.show_fps) { char b[32]; snprintf(b, sizeof b, "%.0f FPS", G.fps); ui_text(2, 2, b, COL_WHITE, true); }
}

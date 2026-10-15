// Workstation screens (included by screens.c, shares its slot machinery):
// enchanting table, anvil, grindstone, loom, cartography table, lectern, villager trading and the
// recipe book. Inputs live in g_player.craft[] so they go back to the player when the screen closes.
#include "../game/maps.h"
#include "../entity/villager.h"

static const char *DYE_KEYS[16] = {"white", "orange", "magenta", "light_blue", "yellow", "lime", "pink", "gray", "light_gray", "cyan", "purple", "blue", "brown", "green", "red", "black"};
static const u32 DYE_COL[16] = {0xF9FFFE, 0xF9801D, 0xC74EBD, 0x3AB3DA, 0xFED83D, 0x80C71F, 0xF38BAA, 0x474F52, 0x9D9D97, 0x169C9C, 0x8932B8, 0x3C44AA, 0x835432, 0x5E7C16, 0xB02E26, 0x1D1D21};
static int dye_index_of_item(int id) {
    if (!item_valid(id)) return -1;
    const char *k = ITEMS[id].key;
    for (int i = 0; i < 16; i++) { char b[40]; snprintf(b, sizeof b, "%s_dye", DYE_KEYS[i]); if (!strcmp(k, b)) return i; }
    return -1;
}
int banner_base_color(int id) {
    if (!item_valid(id)) return 0;
    const char *k = ITEMS[id].key;
    for (int i = 15; i >= 0; i--) { size_t l = strlen(DYE_KEYS[i]); if (!strncmp(k, DYE_KEYS[i], l) && !strcmp(k + l, "_banner")) return i; }
    return 0;
}
bool item_is_banner(int id) { return item_valid(id) && strstr(ITEMS[id].key, "_banner") && !strstr(ITEMS[id].key, "pattern"); }

// ------------------------------------------------------------------ banner patterns (20 x 40 masks)
#define NPAT 29
static bool banner_mask(int p, int x, int y) {
    f32 fx = x + 0.5f, fy = y + 0.5f, cx = fabsf(fx - 10), cy = fabsf(fy - 20);
    switch (p) {
    case 1: return true;
    case 2: return y >= 27;
    case 3: return y < 13;
    case 4: return x < 7;
    case 5: return x >= 13;
    case 6: return x >= 7 && x < 13;
    case 7: return y >= 17 && y < 23;
    case 8: return fabsf(fx - fy * 0.5f) < 3;
    case 9: return fabsf((20 - fx) - fy * 0.5f) < 3;
    case 10: return fabsf(fx - fy * 0.5f) < 3 || fabsf((20 - fx) - fy * 0.5f) < 3;
    case 11: return (x >= 8 && x < 12) || (y >= 18 && y < 22);
    case 12: return fy >= 30 + cx;
    case 13: return fy <= 10 - cx;
    case 14: return y < 20;
    case 15: return x < 10;
    case 16: return x < 2 || x > 17 || y < 2 || y > 37;
    case 17: return cx * cx + cy * cy < 30;
    case 18: return cx / 8.0f + cy / 14.0f < 0.8f;
    case 19: return ((x * 7 + y * 3) % 40) < 40 - y;
    case 20: return y % 5 == 0 || ((x + (y / 5 % 2) * 5) % 10) == 0;
    case 21: { // creeper face
        int gx = x / 2 - 2, gy = y / 2 - 6;
        static const char *F[8] = {"........", ".XX..XX.", ".XX..XX.", "...XX...", "..XXXX..", "..XXXX..", "..X..X..", "........"};
        return gx >= 0 && gx < 8 && gy >= 0 && gy < 8 && F[gy][gx] == 'X'; }
    case 22: { // skull and crossbones
        if (cx * cx + (fy - 16) * (fy - 16) < 20 && !((cx > 1.2f && cx < 3.5f) && fy > 14 && fy < 17)) return true;
        return fabsf(fx - (fy - 14)) < 1.2f && fy > 21 && fy < 32 ? true : fabsf((20 - fx) - (fy - 14)) < 1.2f && fy > 21 && fy < 32; }
    case 23: { f32 a = atan2f(fy - 20, fx - 10), r = sqrtf(cx * cx + cy * cy); return r < 2.2f || (r < 7.5f && cosf(a * 5) > 0.2f && r > 3.2f); }
    case 24: { f32 r = sqrtf(cx * cx + cy * cy); return r < 8 && (fabsf(fy - 20) < 1 || fabsf(fx - 10) < 1 || fabsf(r - 5) < 0.8f); }
    case 25: return (cx < 6 && cy < 4) && !(cx > 1.5f && cx < 3.5f && cy < 2);
    case 26: { f32 r = sqrtf(cx * cx + (fy - 18) * (fy - 18)); return (r < 7 && r > 4.5f) || (cx < 1.5f && fy > 22 && fy < 30); }
    case 27: { f32 a = atan2f(fy - 20, fx - 10), r = sqrtf(cx * cx + cy * cy); return r < 9 && fabsf(fmodf(r - a * 1.4f + 30, 4.5f) - 2) < 0.9f; }
    case 28: return (cx < 5 && fabsf(fy - 16) < 2.5f) || (fabsf(cx - 3) < 1 && fy > 18 && fy < 27) || (cx < 1 && fy > 10 && fy < 14);
    default: return false;
    }
}
static Texture pattern_tex;
static void ensure_pattern_tex(void) {
    if (pattern_tex.id) return;
    int W = 32 * 20, H = 40;
    u8 *px = xcalloc((size_t)W * H, 4);
    for (int p = 1; p < NPAT; p++) for (int y = 0; y < 40; y++) for (int x = 0; x < 20; x++) if (banner_mask(p, x, y)) {
        u8 *o = px + ((size_t)y * W + p * 20 + x) * 4; o[0] = o[1] = o[2] = 255; o[3] = 255;
    }
    pattern_tex = texture_from_pixels(px, W, H, false);
    free(px);
}
GLuint banner_pattern_texture(void) { ensure_pattern_tex(); return pattern_tex.id; }
static u32 col_rgba(u32 c, f32 shade) { return rgba_f(((c >> 16) & 255) / 255.0f * shade, ((c >> 8) & 255) / 255.0f * shade, (c & 255) / 255.0f * shade, 1); }
// banner cloth with its layers in a GUI rectangle
void banner_draw_gui(f32 x, f32 y, f32 w, f32 h, int base, const u8 layers[][2], int n) {
    ensure_pattern_tex();
    ui_rect(x, y, w, h, col_rgba(DYE_COL[base & 15], 1));
    b2d_set_texture(pattern_tex.id);
    for (int i = 0; i < n; i++) {
        int p = layers[i][0];
        if (p <= 0 || p >= NPAT) continue;
        f32 u0 = p * 20 / 640.0f, u1 = (p * 20 + 20) / 640.0f;
        b2d_quad(x, y, w, h, u0, 0, u1, 1, col_rgba(DYE_COL[layers[i][1] & 15], 1));
    }
}

static int banner_layers(ItemStack s) { int n = 0; for (int i = 0; i < MAX_ENCH; i++) if (s.ench[i][0]) n++; return n; }
static const struct { int pat; const char *item; } SPECIAL_PATTERNS[] = {
    {21, "creeper_banner_pattern"}, {22, "skull_banner_pattern"}, {23, "flower_banner_pattern"}, {24, "globe_banner_pattern"},
    {25, "piglin_banner_pattern"}, {26, "mojang_banner_pattern"}, {27, "flow_banner_pattern"}, {28, "guster_banner_pattern"},
    {20, "field_masoned_banner_pattern"}, {16, "bordure_indented_banner_pattern"}};
static int loom_patterns(int *out) {   // available patterns for the current pattern slot
    int n = 0;
    ItemStack pi = g_player.craft[2];
    if (!stack_empty(pi)) {
        for (size_t i = 0; i < ARRAY_LEN(SPECIAL_PATTERNS); i++) if (!strcmp(ITEMS[pi.id].key, SPECIAL_PATTERNS[i].item)) out[n++] = SPECIAL_PATTERNS[i].pat;
        return n;
    }
    for (int p = 2; p <= 19; p++) if (p != 16) out[n++] = p;
    return n;
}

// ------------------------------------------------------------------ enchanting table
static int count_bookshelves(ivec3 t) {
    int n = 0;
    for (int dy = 0; dy <= 1; dy++) for (int dz = -2; dz <= 2; dz++) for (int dx = -2; dx <= 2; dx++) {
        if (MAXI(abs(dx), abs(dz)) != 2) continue;
        if (BLOCKS[world_get_block(t.x + dx, t.y + dy, t.z + dz)].base != B_BOOKSHELF) continue;
        int ix = t.x + (dx > 0 ? dx - 1 : dx < 0 ? dx + 1 : 0), iz = t.z + (dz > 0 ? dz - 1 : dz < 0 ? dz + 1 : 0);
        if (abs(dx) == 2 && abs(dz) == 2) { ix = t.x + dx / 2; iz = t.z + dz / 2; }
        if (block_is_air(world_get_block(ix, t.y + dy, iz))) n++;
    }
    return MINI(15, n);
}
static void enchant_offers(int levels[3]) {
    ItemStack it = g_player.craft[0];
    levels[0] = levels[1] = levels[2] = 0;
    if (stack_empty(it) || !item_enchantability(it.id) || ench_count(it) || it.count != 1) return;
    int b = count_bookshelves(G.open_be);
    u32 s = g_player.ench_seed * 747796405u + 2891336453u;
    s = s * 1664525u + 1013904223u; int r1 = (int)((s >> 8) % 8u);
    s = s * 1664525u + 1013904223u; int r2 = (int)((s >> 8) % (u32)(b + 1));
    int base = r1 + 1 + b / 2 + r2;
    levels[0] = MAXI(base / 3, 1);
    levels[1] = base * 2 / 3 + 1;
    levels[2] = MAXI(base, b * 2);
}
bool screens_enchant_option(int i) {
    int lv[3]; enchant_offers(lv);
    if (i < 0 || i > 2 || !lv[i]) return false;
    ItemStack *it = &g_player.craft[0], *lap = &g_player.craft[1];
    bool creative = g_player.gamemode == GM_CREATIVE;
    if (!creative && (g_player.xp_level < lv[i] || lap->id != I_LAPIS_LAZULI || lap->count < i + 1)) return false;
    u8 out[MAX_ENCH][2]; memset(out, 0, sizeof out);
    int n = ench_select(it->id, lv[i], g_player.ench_seed + (u32)i * 7919u, out, MAX_ENCH);
    if (!n) return false;
    if (it->id == I_BOOK) it->id = I_ENCHANTED_BOOK;
    for (int k = 0; k < n; k++) ench_add(it, out[k][0], out[k][1]);
    if (!creative) { lap->count -= (u8)(i + 1); if (!lap->count) *lap = (ItemStack){0}; }
    player_spend_levels(i + 1);
    g_player.ench_seed = (u32)hash64(g_player.ench_seed ^ 0x5bd1e995ULL);
    sound_play(SND_LEVELUP, 0.7f, 1.4f);
    return true;
}
static void enchant_screen(f32 x0, f32 y0, u32 title) {
    ui_text(x0 + 8, y0 + 4, "Enchant", title, false);
    add_slot(x0 + 15, y0 + 47, &g_player.craft[0], SK_NORMAL, SEC_CONTAINER);
    add_slot(x0 + 35, y0 + 47, &g_player.craft[1], SK_NORMAL, SEC_CONTAINER);
    // a little open book
    ui_rect(x0 + 14, y0 + 16, 34, 22, rgba_u32(90, 40, 30, 255));
    ui_rect(x0 + 16, y0 + 18, 14, 18, rgba_u32(230, 220, 190, 255)); ui_rect(x0 + 32, y0 + 18, 14, 18, rgba_u32(230, 220, 190, 255));
    int lv[3]; enchant_offers(lv);
    for (int i = 0; i < 3; i++) {
        f32 bx = x0 + 60, by = y0 + 14 + i * 19, bw = 108, bh = 19;
        bool can = lv[i] && (g_player.gamemode == GM_CREATIVE || (g_player.xp_level >= lv[i] && g_player.craft[1].id == I_LAPIS_LAZULI && g_player.craft[1].count >= i + 1));
        bool hov = ui_hover(bx, by, bw, bh);
        ui_rect(bx, by, bw, bh, rgba_u32(40, 30, 50, 255));
        ui_rect(bx + 1, by + 1, bw - 2, bh - 2, !lv[i] ? rgba_u32(80, 70, 90, 255) : can ? (hov ? rgba_u32(180, 150, 210, 255) : rgba_u32(150, 120, 180, 255)) : rgba_u32(100, 85, 110, 255));
        if (!lv[i]) continue;
        // hint: the first enchantment of this offer
        u8 out[MAX_ENCH][2]; memset(out, 0, sizeof out);
        int n = ench_select(g_player.craft[0].id, lv[i], g_player.ench_seed + (u32)i * 7919u, out, MAX_ENCH);
        char hint[64] = "";
        if (n) snprintf(hint, sizeof hint, "%s %s%s", ENCHANTS[out[0][0]].name, ENCHANTS[out[0][0]].max_level > 1 ? ench_roman(out[0][1]) : "", n > 1 ? " ..." : "");
        ui_text_scaled(bx + 3, by + 3, 0.75f, hint, can ? rgba_u32(80, 255, 80, 255) : rgba_u32(60, 50, 70, 255), false);
        char c[24]; snprintf(c, sizeof c, "%d", lv[i]);
        ui_text(bx + bw - 3 - ui_text_width(c), by + 9, c, can ? rgba_u32(128, 255, 32, 255) : rgba_u32(64, 128, 16, 255), true);
        char lp[16]; snprintf(lp, sizeof lp, "%d lapis", i + 1);
        ui_text_scaled(bx + 3, by + 11, 0.7f, lp, rgba_u32(60, 60, 140, 255), false);
        if (hov) {
            char lines[2][64]; snprintf(lines[0], 64, "%s . . . ?", hint); snprintf(lines[1], 64, "%d Lapis Lazuli, %d Enchantment Level%s", i + 1, i + 1, i ? "s" : "");
            ui_tooltip_lines(can ? "Click to enchant" : g_player.xp_level < lv[i] ? "Level requirement not met" : "Needs Lapis Lazuli", COL_WHITE, (const char (*)[64])lines, 2, NULL);
            if (g_win.mouse_pressed[GLFW_MOUSE_BUTTON_LEFT] && can) { screens_enchant_option(i); g_win.mouse_pressed[GLFW_MOUSE_BUTTON_LEFT] = false; }
        }
    }
    char b[32]; snprintf(b, sizeof b, "Bookshelves: %d", count_bookshelves(G.open_be));
    ui_text_scaled(x0 + 60, y0 + 6, 0.75f, b, rgba_u32(90, 90, 90, 255), false);
}

// ------------------------------------------------------------------ anvil / grindstone / loom / cartography: results
static int anvil_cost, anvil_used, grind_xp;
static ItemStack ws_result(void) {
    ItemStack *c = g_player.craft;
    switch (G.screen) {
    case SCR_ANVIL: {
        ItemStack r = anvil_combine(c[0], c[1], G.anvil_name, &anvil_cost, &anvil_used);
        if (stack_empty(r)) return r;
        if (g_player.gamemode != GM_CREATIVE && (anvil_cost >= 40 || g_player.xp_level < anvil_cost)) return (ItemStack){0};
        return r;
    }
    case SCR_GRINDSTONE: return grindstone_result(c[0], c[1], &grind_xp);
    case SCR_LOOM: {
        if (!item_is_banner(c[0].id) || dye_index_of_item(c[1].id) < 0 || G.loom_sel <= 0) return (ItemStack){0};
        int pats[32], np = loom_patterns(pats); bool ok = false;
        for (int i = 0; i < np; i++) ok |= pats[i] == G.loom_sel;
        if (!ok || banner_layers(c[0]) >= MAX_ENCH) return (ItemStack){0};
        ItemStack r = c[0]; r.count = 1;
        for (int i = 0; i < MAX_ENCH; i++) if (!r.ench[i][0]) { r.ench[i][0] = (u8)G.loom_sel; r.ench[i][1] = (u8)dye_index_of_item(c[1].id); break; }
        return r;
    }
    case SCR_CARTOGRAPHY: {
        if (c[0].id != I_FILLED_MAP || stack_empty(c[1])) return (ItemStack){0};
        ItemStack r = c[0];
        if (c[1].id == I_PAPER) { if (map_scale(c[0].data) >= 4 || map_locked(c[0].data)) return (ItemStack){0}; r.count = 1; r.ench[0][0] = 0; return r; }   // id assigned when taken
        if (c[1].id == I_MAP) { r.count = 2; return r; }
        if (c[1].id == I_GLASS_PANE) { if (map_locked(c[0].data)) return (ItemStack){0}; r.count = 1; return r; }
        return (ItemStack){0};
    }
    case SCR_TRADE: {
        Entity *v = entity_get(G.trade_with);
        if (!v) return (ItemStack){0};
        int n; TradeOffer *o = villager_offers(v, &n);
        if (G.trade_sel < 0 || G.trade_sel >= n) return (ItemStack){0};
        TradeOffer *t = &o[G.trade_sel];
        if (t->uses >= t->max_uses) return (ItemStack){0};
        if (c[0].id != t->a.id || c[0].count < t->a.count) return (ItemStack){0};
        if (!stack_empty(t->b) && (c[1].id != t->b.id || c[1].count < t->b.count)) return (ItemStack){0};
        return t->result;
    }
    default: return (ItemStack){0};
    }
}
static void dec(ItemStack *s, int n) { if (stack_empty(*s)) return; if (s->count <= n) *s = (ItemStack){0}; else s->count -= (u8)n; }
static ItemStack ws_take(void) {   // finalize the result (may differ from the preview, e.g. new map ids)
    ItemStack r = ws_result();
    if (stack_empty(r)) return r;
    ItemStack *c = g_player.craft;
    switch (G.screen) {
    case SCR_ANVIL: {
        player_spend_levels(anvil_cost);
        dec(&c[0], 1); dec(&c[1], anvil_used);
        G.anvil_name[0] = 0;
        Block b = world_get_block(G.open_be.x, G.open_be.y, G.open_be.z);
        if (g_player.gamemode != GM_CREATIVE && frand() < 0.12f) {
            int k = BLOCKS[b].base, v = BLOCKS[b].var;
            if (k == B_ANVIL) world_set_block(G.open_be.x, G.open_be.y, G.open_be.z, (Block)(B_CHIPPED_ANVIL + v));
            else if (k == B_CHIPPED_ANVIL) world_set_block(G.open_be.x, G.open_be.y, G.open_be.z, (Block)(B_DAMAGED_ANVIL + v));
            else { world_set_block(G.open_be.x, G.open_be.y, G.open_be.z, B_AIR); sound_play(SND_BREAK_GLASS, 0.8f, 0.5f); game_close_screen(); }
        }
        sound_play(SND_HIT, 0.6f, 1.6f);
    } break;
    case SCR_GRINDSTONE: { Entity *e = player_ent(); if (e && grind_xp) xp_drop(e->pos, grind_xp); dec(&c[0], 1); dec(&c[1], 1); sound_play(SND_DIG_STONE, 0.6f, 1.2f); } break;
    case SCR_LOOM: dec(&c[0], 1); dec(&c[1], 1); sound_play(SND_DIG_WOOL, 0.6f, 1.0f); break;
    case SCR_CARTOGRAPHY: {
        int tool = c[1].id;
        if (tool == I_PAPER) { int nid = map_zoom_out(c[0].data); if (!nid) return (ItemStack){0}; r.data = (u16)nid; }
        else if (tool == I_GLASS_PANE) { int nid = map_copy(c[0].data, true); if (!nid) return (ItemStack){0}; r.data = (u16)nid; }
        dec(&c[0], 1); dec(&c[1], 1);
        sound_play(SND_CLICK, 0.6f, 0.9f);
    } break;
    case SCR_TRADE: {
        Entity *v = entity_get(G.trade_with);
        int n; TradeOffer *o = villager_offers(v, &n);
        TradeOffer *t = &o[G.trade_sel];
        dec(&c[0], t->a.count); if (!stack_empty(t->b)) dec(&c[1], t->b.count);
        villager_trade_done(v, G.trade_sel);
        sound_play_at(SND_XP, v->pos, 0.5f, 1.2f);
        if (r.id == I_MAP) { int mid = map_create_at(v3_add(v->pos, v3(frand_range(-600, 600), 0, frand_range(-600, 600)))); if (mid) { r.id = I_FILLED_MAP; r.data = (u16)mid; } }
    } break;
    default: break;
    }
    return r;
}
static bool ws_has_result(void) { return G.screen == SCR_ANVIL || G.screen == SCR_GRINDSTONE || G.screen == SCR_LOOM || G.screen == SCR_CARTOGRAPHY || G.screen == SCR_TRADE; }

static void anvil_screen(f32 x0, f32 y0, u32 title) {
    ui_text(x0 + 60, y0 + 6, "Repair & Name", title, false);
    add_slot(x0 + 27, y0 + 47, &g_player.craft[0], SK_NORMAL, SEC_CONTAINER);
    add_slot(x0 + 76, y0 + 47, &g_player.craft[1], SK_NORMAL, SEC_CONTAINER);
    ui_text(x0 + 60, y0 + 51, "+", title, false);
    if (!stack_empty(g_player.craft[0]) && !G.anvil_name[0] && !G.text_focus) {}
    bool clicked = ui_textfield(x0 + 59, y0 + 20, 103, G.anvil_name, sizeof G.anvil_name, G.text_focus);
    if (clicked) G.text_focus = true;
    else if (g_win.mouse_pressed[GLFW_MOUSE_BUTTON_LEFT] && !ui_hover(x0 + 59, y0 + 20, 103, 20)) G.text_focus = false;
    if (G.text_focus) g_win.nchars = 0;
    ItemStack r = ws_result();
    if (!stack_empty(g_player.craft[0])) {
        int cost = 0, used = 0; ItemStack raw = anvil_combine(g_player.craft[0], g_player.craft[1], G.anvil_name, &cost, &used);
        if (!stack_empty(raw)) {
            char b[48];
            bool too = cost >= 40 && g_player.gamemode != GM_CREATIVE;
            if (too) snprintf(b, sizeof b, "Too Expensive!"); else snprintf(b, sizeof b, "Enchantment Cost: %d", cost);
            u32 col = (too || (stack_empty(r) && g_player.gamemode != GM_CREATIVE)) ? rgba_u32(255, 96, 96, 255) : rgba_u32(128, 255, 32, 255);
            ui_rect(x0 + 60, y0 + 68, ui_text_width(b) + 4, 10, rgba_u32(0, 0, 0, 120));
            ui_text(x0 + 62, y0 + 69, b, col, true);
        }
    }
}
static void grindstone_screen(f32 x0, f32 y0, u32 title) {
    ui_text(x0 + 8, y0 + 6, "Repair & Disenchant", title, false);
    add_slot(x0 + 49, y0 + 19, &g_player.craft[0], SK_NORMAL, SEC_CONTAINER);
    add_slot(x0 + 49, y0 + 40, &g_player.craft[1], SK_NORMAL, SEC_CONTAINER);
    ui_rect(x0 + 72, y0 + 30, 12, 26, rgba_u32(120, 120, 120, 255));
}
static void loom_screen(f32 x0, f32 y0, u32 title) {
    ui_text(x0 + 8, y0 + 4, "Loom", title, false);
    add_slot(x0 + 13, y0 + 26, &g_player.craft[0], SK_NORMAL, SEC_CONTAINER);
    add_slot(x0 + 33, y0 + 26, &g_player.craft[1], SK_NORMAL, SEC_CONTAINER);
    add_slot(x0 + 23, y0 + 45, &g_player.craft[2], SK_NORMAL, SEC_CONTAINER);
    int pats[32], np = loom_patterns(pats);
    bool ready = item_is_banner(g_player.craft[0].id) && dye_index_of_item(g_player.craft[1].id) >= 0;
    int dye = MAXI(0, dye_index_of_item(g_player.craft[1].id)), base = banner_base_color(g_player.craft[0].id);
    f32 gx = x0 + 60, gy = y0 + 13;
    ui_rect(gx - 1, gy - 1, 4 * 14 + 2, 4 * 14 + 2, rgba_u32(60, 60, 60, 255));
    int rows = (np + 3) / 4, maxs = MAXI(0, rows - 4);
    if (g_win.scroll != 0 && ui_hover(gx, gy, 56, 56)) G.loom_scroll = clampi(G.loom_scroll - (int)g_win.scroll, 0, maxs);
    G.loom_scroll = clampi(G.loom_scroll, 0, maxs);
    for (int i = 0; i < 16; i++) {
        int k = G.loom_scroll * 4 + i;
        if (k >= np) break;
        f32 bx = gx + (i % 4) * 14, by = gy + (i / 4) * 14;
        bool sel = G.loom_sel == pats[k], hov = ui_hover(bx, by, 14, 14);
        ui_rect(bx, by, 14, 14, sel ? rgba_u32(100, 160, 100, 255) : hov ? rgba_u32(170, 170, 200, 255) : rgba_u32(139, 139, 139, 255));
        if (ready) { u8 l[1][2] = {{(u8)pats[k], (u8)dye}}; banner_draw_gui(bx + 4, by + 1, 6, 12, base, l, 1); }
        else { u8 l[1][2] = {{(u8)pats[k], 15}}; banner_draw_gui(bx + 4, by + 1, 6, 12, 0, l, 1); }
        if (hov) { ui_tooltip(banner_pattern_name(pats[k]), NULL); if (g_win.mouse_pressed[GLFW_MOUSE_BUTTON_LEFT]) { G.loom_sel = pats[k]; sound_play(SND_CLICK, 0.4f, 1.2f); } }
    }
    // preview of the result
    ItemStack r = ws_result();
    if (!stack_empty(r)) banner_draw_gui(x0 + 141, y0 + 9, 16, 32, banner_base_color(r.id), (const u8 (*)[2])r.ench, MAX_ENCH);
}
static void cartography_screen(f32 x0, f32 y0, u32 title) {
    ui_text(x0 + 8, y0 + 4, "Cartography Table", title, false);
    add_slot(x0 + 15, y0 + 15, &g_player.craft[0], SK_NORMAL, SEC_CONTAINER);
    add_slot(x0 + 15, y0 + 52, &g_player.craft[1], SK_NORMAL, SEC_CONTAINER);
    if (g_player.craft[0].id == I_FILLED_MAP) {
        map_draw(g_player.craft[0].data, x0 + 72, y0 + 12, 54, false);
        const char *what = g_player.craft[1].id == I_PAPER ? "Zoom out" : g_player.craft[1].id == I_MAP ? "Copy" : g_player.craft[1].id == I_GLASS_PANE ? "Lock" : "Add paper, map or glass pane";
        ui_text_scaled(x0 + 44, y0 + 70, 0.75f, what, rgba_u32(80, 80, 80, 255), false);
    }
}
static void lectern_screen(f32 x0, f32 y0, u32 title) {
    BlockEntity *be = world_block_entity(G.open_be, true, BE_LECTERN);
    ui_text(x0 + 8, y0 + 6, "Lectern", title, false);
    add_slot(x0 + 80, y0 + 35, &be->slots[0], SK_NORMAL, SEC_CONTAINER);
    ui_text_scaled(x0 + 8, y0 + 60, 0.75f, stack_empty(be->slots[0]) ? "Place a book here" : "A comparator reads the book", rgba_u32(80, 80, 80, 255), false);
}

// ------------------------------------------------------------------ recipe book
static bool recipe_unlocked(int r) { return g_player.gamemode == GM_CREATIVE || (g_player.recipes_known[r >> 3] >> (r & 7)) & 1; }
void recipes_unlock_tick(void) {   // a recipe unlocks once the player has held one of its ingredients
    static int cursor;
    for (int k = 0; k < 40; k++) {
        int r = cursor++ % RECIPE_COUNT;
        if ((g_player.recipes_known[r >> 3] >> (r & 7)) & 1) continue;
        const RecipeDef *rd = &RECIPES[r];
        bool have = false;
        for (int c = 0; c < 9 && !have; c++) {
            int ing = rd->ing[c];
            if (!ing) continue;
            for (int i = 0; i < INV_MAIN && !have; i++) if (!stack_empty(g_player.inv.slots[i]) && ingredient_matches(ing, g_player.inv.slots[i].id)) have = true;
        }
        if (have) g_player.recipes_known[r >> 3] |= (u8)(1 << (r & 7));
    }
}
static bool recipe_fits(int r, int gw) { return RECIPES[r].shapeless ? RECIPES[r].w <= gw * gw : (RECIPES[r].w <= gw && RECIPES[r].h <= gw); }
// can it be made from the inventory (+ what is in the grid)? greedy ingredient assignment
static bool recipe_craftable(int r) {
    static int ids[INV_MAIN + 9], cnt[INV_MAIN + 9];
    int n = 0;
    for (int i = 0; i < INV_MAIN; i++) if (!stack_empty(g_player.inv.slots[i])) { ids[n] = g_player.inv.slots[i].id; cnt[n++] = g_player.inv.slots[i].count; }
    for (int i = 0; i < 9; i++) if (!stack_empty(g_player.craft[i])) { ids[n] = g_player.craft[i].id; cnt[n++] = g_player.craft[i].count; }
    const RecipeDef *rd = &RECIPES[r];
    int cells = rd->shapeless ? rd->w : rd->w * rd->h;
    for (int c = 0; c < cells; c++) {
        int ing = rd->ing[c];
        if (!ing) continue;
        bool ok = false;
        for (int i = 0; i < n && !ok; i++) if (cnt[i] > 0 && ingredient_matches(ing, ids[i])) { cnt[i]--; ok = true; }
        if (!ok) return false;
    }
    return true;
}
static bool take_matching(int ing, ItemStack *out) {
    for (int i = 0; i < INV_MAIN; i++) {
        ItemStack *s = &g_player.inv.slots[i];
        if (stack_empty(*s) || !ingredient_matches(ing, s->id)) continue;
        *out = *s; out->count = 1;
        if (--s->count == 0) *s = (ItemStack){0};
        return true;
    }
    return false;
}
bool screens_recipe_fill(int r, bool all) {
    int gw = craft_w();
    if (r < 0 || r >= RECIPE_COUNT || !recipe_fits(r, gw)) return false;
    for (int i = 0; i < 9; i++) if (!stack_empty(g_player.craft[i])) { player_give(g_player.craft[i]); g_player.craft[i] = (ItemStack){0}; }
    if (!recipe_craftable(r)) return false;
    const RecipeDef *rd = &RECIPES[r];
    int times = all ? 64 : 1;
    for (int t = 0; t < times; t++) {
        if (!recipe_craftable(r) && t > 0) break;
        bool ok = true;
        int cells = rd->shapeless ? rd->w : rd->w * rd->h;
        for (int c = 0; c < cells && ok; c++) {
            int ing = rd->ing[c];
            if (!ing) continue;
            int gx = rd->shapeless ? c % gw : c % rd->w, gy = rd->shapeless ? c / gw : c / rd->w;
            ItemStack *dst = &g_player.craft[gy * 3 + gx];
            if (!stack_empty(*dst) && dst->count >= stack_max(*dst)) { ok = false; break; }
            ItemStack one;
            if (!stack_empty(*dst)) {   // keep stacking the same item
                bool found = false;
                for (int i = 0; i < INV_MAIN && !found; i++) { ItemStack *s = &g_player.inv.slots[i]; if (stack_can_merge(*s, *dst)) { if (--s->count == 0) *s = (ItemStack){0}; dst->count++; found = true; } }
                if (!found) ok = false;
            } else if (take_matching(ing, &one)) *dst = one;
            else ok = false;
        }
        if (!ok) break;
    }
    sound_play(SND_CLICK, 0.4f, 1.3f);
    return true;
}
static void recipe_book_panel(f32 bx, f32 by) {
    f32 bw = 147, bh = 166;
    ui_panel(bx, by, bw, bh);
    u32 title = rgba_u32(64, 64, 64, 255);
    ui_text(bx + 8, by + 6, "Recipe Book", title, false);
    // search field
    bool clicked = ui_textfield(bx + 8, by + 16, 96, G.book_search, sizeof G.book_search, G.text_focus);
    if (clicked) G.text_focus = true; else if (g_win.mouse_pressed[GLFW_MOUSE_BUTTON_LEFT] && !ui_hover(bx + 8, by + 16, 96, 20)) G.text_focus = false;
    if (G.text_focus) g_win.nchars = 0;
    if (ui_button(bx + 107, by + 16, 34, 20, G.book_craftable ? "Can" : "All", true)) { G.book_craftable = !G.book_craftable; G.book_page = 0; }
    int gw = craft_w();
    static int list[RECIPE_COUNT];
    int n = 0;
    char want[40]; { size_t k = 0; for (const char *s = G.book_search; *s && k + 1 < sizeof want; s++) want[k++] = (char)tolower((unsigned char)*s); want[k] = 0; }
    for (int r = 0; r < RECIPE_COUNT; r++) {
        if (!recipe_unlocked(r) || !recipe_fits(r, gw)) continue;
        // hide duplicate results (variants of the same recipe) unless searching
        if (want[0]) {
            char nm[96]; size_t k = 0;
            for (const char *s = ITEMS[RECIPES[r].result].name; *s && k + 1 < sizeof nm; s++) nm[k++] = (char)tolower((unsigned char)*s);
            nm[k] = 0;
            if (!strstr(nm, want)) continue;
        }
        bool dup = false;
        for (int i = MAXI(0, n - 3); i < n; i++) if (RECIPES[list[i]].result == RECIPES[r].result) dup = true;
        if (dup) continue;
        if (G.book_craftable && !recipe_craftable(r)) continue;
        list[n++] = r;
    }
    int per = 20, pages = MAXI(1, (n + per - 1) / per);
    G.book_page = clampi(G.book_page, 0, pages - 1);
    for (int i = 0; i < per; i++) {
        int k = G.book_page * per + i;
        if (k >= n) break;
        int r = list[k];
        f32 x = bx + 11 + (i % 5) * 25, y = by + 42 + (i / 5) * 25;
        bool can = recipe_craftable(r), hov = ui_hover(x, y, 24, 24);
        ui_rect(x, y, 24, 24, can ? (hov ? rgba_u32(130, 200, 130, 255) : rgba_u32(100, 160, 100, 255)) : (hov ? rgba_u32(200, 130, 130, 255) : rgba_u32(150, 90, 90, 255)));
        ItemStack res = stack_of(RECIPES[r].result, RECIPES[r].count);
        ui_item(x + 4, y + 4, res, true);
        if (hov) {
            char lines[4][64]; int nl = 0;
            snprintf(lines[nl++], 64, "%s", can ? "Click: fill the grid (shift: max)" : "Missing ingredients");
            const RecipeDef *rd = &RECIPES[r];
            // list the distinct ingredients (first alternative of each)
            char ingtxt[64] = ""; int seen[9] = {0}, ns = 0;
            int cells = rd->shapeless ? rd->w : rd->w * rd->h;
            for (int c = 0; c < cells; c++) {
                int ing = rd->ing[c]; if (!ing) continue;
                bool dupi = false; for (int q = 0; q < ns; q++) dupi |= seen[q] == ing;
                if (dupi) continue;
                seen[ns++] = ing;
                int item = ING_ITEMS[INGREDIENTS[ing].off];
                size_t l = strlen(ingtxt);
                if (l < 50) snprintf(ingtxt + l, sizeof ingtxt - l, "%s%s", l ? ", " : "", ITEMS[item].name);
            }
            snprintf(lines[nl++], 64, "%s", ingtxt);
            ui_tooltip_lines(ITEMS[res.id].name, COL_WHITE, (const char (*)[64])lines, nl, NULL);
            if (g_win.mouse_pressed[GLFW_MOUSE_BUTTON_LEFT]) { screens_recipe_fill(r, key_down(GLFW_KEY_LEFT_SHIFT)); g_win.mouse_pressed[GLFW_MOUSE_BUTTON_LEFT] = false; }
        }
    }
    char pg[24]; snprintf(pg, sizeof pg, "%d/%d", G.book_page + 1, pages);
    ui_text_center(bx + bw / 2, by + 148, pg, title, false);
    if (ui_button(bx + 20, by + 143, 20, 16, "<", G.book_page > 0)) G.book_page--;
    if (ui_button(bx + bw - 40, by + 143, 20, 16, ">", G.book_page < pages - 1)) G.book_page++;
    if (n == 0) ui_text_center(bx + bw / 2, by + 80, G.book_craftable ? "Nothing craftable yet" : "No recipes unlocked", rgba_u32(90, 90, 90, 255), false);
}

// ------------------------------------------------------------------ villager trading
static void trade_fill(Entity *v, int idx) {
    int n; TradeOffer *o = villager_offers(v, &n);
    if (idx < 0 || idx >= n) return;
    for (int i = 0; i < 2; i++) if (!stack_empty(g_player.craft[i])) { player_give(g_player.craft[i]); g_player.craft[i] = (ItemStack){0}; }
    ItemStack want[2] = {o[idx].a, o[idx].b};
    for (int k = 0; k < 2; k++) {
        if (stack_empty(want[k])) continue;
        int need = want[k].count;
        for (int i = 0; i < INV_MAIN && need > 0; i++) {
            ItemStack *s = &g_player.inv.slots[i];
            if (s->id != want[k].id || ench_count(*s)) continue;
            int take = MINI(need, s->count);
            if (stack_empty(g_player.craft[k])) { g_player.craft[k] = *s; g_player.craft[k].count = 0; }
            g_player.craft[k].count += (u8)take; s->count -= (u8)take; need -= take;
            if (!s->count) *s = (ItemStack){0};
        }
    }
}
bool screens_trade_select(int idx) {
    Entity *v = entity_get(G.trade_with);
    if (!v) return false;
    G.trade_sel = idx;
    trade_fill(v, idx);
    return true;
}
static void trade_screen(f32 x0, f32 y0, u32 title) {
    Entity *v = entity_get(G.trade_with);
    if (!v || v->health <= 0) { game_close_screen(); return; }
    char t[64];
    if (v->prof == PROF_WANDERER) snprintf(t, sizeof t, "Wandering Trader");
    else snprintf(t, sizeof t, "%s - %s", PROF_NAMES[v->prof], villager_level_name(v->vlevel));
    ui_text_center(x0 + 88, y0 + 5, t, title, false);
    // level progress
    if (v->prof != PROF_WANDERER) {
        static const int LX[6] = {0, 0, 10, 70, 150, 250};
        f32 frac = v->vlevel >= 5 ? 1.0f : clampf((f32)(v->vxp - LX[v->vlevel]) / (LX[v->vlevel + 1] - LX[v->vlevel]), 0, 1);
        ui_rect(x0 + 48, y0 + 15, 80, 4, rgba_u32(40, 40, 40, 255));
        ui_rect(x0 + 48, y0 + 15, 80 * frac, 4, rgba_u32(120, 230, 80, 255));
    }
    add_slot(x0 + 36, y0 + 37, &g_player.craft[0], SK_NORMAL, SEC_CONTAINER);
    add_slot(x0 + 62, y0 + 37, &g_player.craft[1], SK_NORMAL, SEC_CONTAINER);
    ui_text(x0 + 92, y0 + 41, "=>", title, false);
    add_slot(x0 + 120, y0 + 37, &craft_result, SK_RESULT, SEC_RESULT);
    craft_result = ws_result();
    // offer list to the left
    int n; TradeOffer *o = villager_offers(v, &n);
    f32 lx = x0 - 112, ly = y0, lw = 108, lh = 166;
    ui_panel(lx, ly, lw, lh);
    ui_text(lx + 6, ly + 5, "Trades", title, false);
    int maxs = MAXI(0, n - 7);
    if (g_win.scroll != 0 && ui_hover(lx, ly, lw, lh)) G.trade_scroll = clampi(G.trade_scroll - (int)g_win.scroll, 0, maxs);
    G.trade_scroll = clampi(G.trade_scroll, 0, maxs);
    for (int i = 0; i < 7; i++) {
        int k = G.trade_scroll + i;
        if (k >= n) break;
        f32 rx = lx + 4, ry = ly + 16 + i * 21;
        bool out = o[k].uses >= o[k].max_uses, sel = G.trade_sel == k, hov = ui_hover(rx, ry, lw - 8, 20);
        ui_rect(rx, ry, lw - 8, 20, sel ? rgba_u32(150, 150, 190, 255) : hov ? rgba_u32(170, 170, 170, 255) : rgba_u32(139, 139, 139, 255));
        ui_item(rx + 2, ry + 2, o[k].a, true);
        if (!stack_empty(o[k].b)) ui_item(rx + 22, ry + 2, o[k].b, true);
        ui_text(rx + 44, ry + 6, out ? "X" : ">", out ? rgba_u32(200, 40, 40, 255) : title, false);
        ui_item(rx + 80, ry + 2, o[k].result, true);
        if (hov) {
            if (ui_hover(rx + 78, ry, 20, 20)) {
                char lines[6][64]; int nl = stack_tooltip_lines(o[k].result, lines, 5);
                ui_tooltip_lines(stack_display_name(o[k].result), ench_count(o[k].result) ? rgba_u32(85, 255, 255, 255) : COL_WHITE, (const char (*)[64])lines, nl, out ? "Out of stock" : NULL);
            }
            if (g_win.mouse_pressed[GLFW_MOUSE_BUTTON_LEFT]) { screens_trade_select(k); g_win.mouse_pressed[GLFW_MOUSE_BUTTON_LEFT] = false; }
        }
    }
    if (!n) ui_text_center(lx + lw / 2, ly + 70, "No trades", rgba_u32(90, 90, 90, 255), false);
}

#include "lang.h"
#include "ui.h"
#include "../world/rules.h"
#include <ctype.h>
#include "screens.h"
#include "../game/game.h"
#include "../game/player.h"
#include "../entity/model.h"
#include "../audio/audio.h"
#include "../core/platform.h"
#include <ctype.h>

// ============================================================================ container slots
typedef enum { SK_NORMAL, SK_RESULT, SK_ARMOR, SK_OUTPUT, SK_PALETTE } SlotKind;
enum { SEC_HOTBAR, SEC_MAIN, SEC_CONTAINER, SEC_RESULT, SEC_ARMOR };
typedef struct { f32 x, y; ItemStack *s; u8 kind, section; s8 armor; int palette_item; } USlot;
static USlot slots[160];
static int nslots;
static ItemStack craft_result, palette_tmp[160];
static ItemStack palette_list[ITEM_MAX + ENCH_COUNT + 64];

static void add_slot(f32 x, f32 y, ItemStack *s, int kind, int section) {
    if (nslots >= (int)ARRAY_LEN(slots)) return;
    slots[nslots++] = (USlot){x, y, s, (u8)kind, (u8)section, -1, 0};
}

static void add_player_inventory(f32 x0, f32 y0) {
    for (int r = 0; r < 3; r++) for (int c = 0; c < 9; c++) add_slot(x0 + 8 + c * 18, y0 + 84 + r * 18, &g_player.inv.slots[9 + r * 9 + c], SK_NORMAL, SEC_MAIN);
    for (int c = 0; c < 9; c++) add_slot(x0 + 8 + c * 18, y0 + 142, &g_player.inv.slots[c], SK_NORMAL, SEC_HOTBAR);
}

static int craft_w(void) { return G.screen == SCR_CRAFTING ? 3 : 2; }
static void trade_screen(f32 x0, f32 y0, u32 title);
#include "workstations.inc"

// the craft grid is stored 3 wide; the 2x2 inventory grid uses its top-left corner
static ItemStack smith_result, cutter_out;
static void consume_grid(void) {
    if (ws_has_result()) return;
    if (G.screen == SCR_SMITHING) {   // template, base and addition are used up
        for (int i = 0; i < 3; i++) { ItemStack *s = &g_player.craft[i]; if (!stack_empty(*s) && --s->count == 0) *s = (ItemStack){0}; }
        return;
    }
    if (G.screen == SCR_STONECUTTER) { ItemStack *s = &g_player.craft[0]; if (!stack_empty(*s) && --s->count == 0) *s = (ItemStack){0}; return; }
    int w = craft_w();
    for (int r = 0; r < w; r++) for (int c = 0; c < w; c++) {
        ItemStack *s = &g_player.craft[r * 3 + c];
        if (stack_empty(*s)) continue;
        int rem = crafting_remainder(s->id);
        if (--s->count == 0) *s = rem ? stack_of(rem, 1) : (ItemStack){0};
        else if (rem) player_give(stack_of(rem, 1));
    }
}
static ItemStack cutter_options[64];
static int ncutter;
static ItemStack current_result(void) {
    if (ws_has_result()) return ws_result();
    if (G.screen == SCR_SMITHING) return smithing_result(g_player.craft[0], g_player.craft[1], g_player.craft[2]);
    if (G.screen == SCR_STONECUTTER) {
        if (stack_empty(g_player.craft[0]) || G.cutter_sel < 0 || G.cutter_sel >= ncutter) return (ItemStack){0};
        return cutter_options[G.cutter_sel];
    }
    int w = craft_w();
    ItemStack grid[9];
    for (int i = 0; i < w * w; i++) grid[i] = g_player.craft[(i / w) * 3 + (i % w)];
    if (rule(GR_DO_LIMITED_CRAFTING) && g_player.gamemode != GM_CREATIVE) {   // only recipes in the recipe book
        int r = crafting_match_index(grid, w, w);
        if (r >= 0 && !(g_player.recipes_known[r / 8] & (1 << (r % 8)))) return (ItemStack){0};
    }
    return crafting_match(grid, w, w);
}

static bool merge_into(ItemStack *dst, ItemStack *src) {   // moves as much of src into dst as possible
    if (stack_empty(*src)) return false;
    if (stack_empty(*dst)) { *dst = *src; *src = (ItemStack){0}; return true; }
    if (!stack_can_merge(*dst, *src)) return false;
    int n = MINI(src->count, stack_max(*dst) - dst->count);
    if (n <= 0) return false;
    dst->count += n; src->count -= n;
    if (!src->count) *src = (ItemStack){0};
    return true;
}

// move a stack to the slots of a given section set (merge first, then empty)
static void quick_move(ItemStack *s, const int *sections, int nsec) {
    for (int pass = 0; pass < 2 && !stack_empty(*s); pass++)
        for (int i = 0; i < nslots && !stack_empty(*s); i++) {
            USlot *u = &slots[i];
            bool ok = false;
            for (int k = 0; k < nsec; k++) ok |= u->section == sections[k];
            if (!ok || u->kind != SK_NORMAL) continue;
            if (pass == 0 && !stack_empty(*u->s)) merge_into(u->s, s);
            if (pass == 1 && stack_empty(*u->s)) merge_into(u->s, s);
        }
}

static void take_result(bool shift) {
    { ItemStack r = current_result(); if (!stack_empty(r) && G.screen != SCR_TRADE) { g_stats[STAT_CRAFTED] += r.count; if (r.id && ITEMS[r.id].key && !strncmp(ITEMS[r.id].key, "divine_", 7)) progress_grant("divine"); } }
    if (ws_has_result()) {
        ItemStack r = ws_result();
        if (stack_empty(r)) return;
        ItemStack *c = &g_player.cursor;
        if (shift) {
            int free_slots = 0;
            for (int i = 0; i < INV_MAIN; i++) free_slots += stack_empty(g_player.inv.slots[i]);
            if (!free_slots) return;
            r = ws_take();
            if (!stack_empty(r)) player_give(r);
        } else {
            if (!stack_empty(*c) && (!stack_can_merge(*c, r) || c->count + r.count > stack_max(*c))) return;
            r = ws_take();
            if (stack_empty(r)) return;
            if (stack_empty(*c)) *c = r; else c->count += r.count;
        }
        return;
    }
    for (int guard = 0; guard < 64; guard++) {
        ItemStack r = current_result();
        if (stack_empty(r)) return;
        if (shift) {
            ItemStack tmp = r;
            int secs[2] = {SEC_HOTBAR, SEC_MAIN};
            Inventory backup = g_player.inv;
            quick_move(&tmp, secs, 2);
            if (!stack_empty(tmp)) { g_player.inv = backup; return; }
            consume_grid();
            sound_play(SND_CLICK, 0.3f, 1.4f);
        } else {
            ItemStack *c = &g_player.cursor;
            if (!stack_empty(*c) && (!stack_can_merge(*c, r) || c->count + r.count > stack_max(*c))) return;
            if (stack_empty(*c)) *c = r; else c->count += r.count;
            consume_grid();
            sound_play(SND_CLICK, 0.3f, 1.4f);
            return;
        }
    }
}

static void slot_click(USlot *u, int button, bool shift) {
    ItemStack *c = &g_player.cursor;
    switch (u->kind) {
    case SK_PALETTE:
        if (!stack_empty(*c)) { *c = (ItemStack){0}; return; }
        *c = *u->s; c->count = (u8)(button == 0 ? ITEMS[u->palette_item].max_stack : 1);
        if (shift) { ItemStack s = *c; *c = (ItemStack){0}; int secs[1] = {SEC_HOTBAR}; quick_move(&s, secs, 1); }
        return;
    case SK_RESULT: take_result(shift); return;
    case SK_OUTPUT:
        if (shift) { int secs[2] = {SEC_HOTBAR, SEC_MAIN}; quick_move(u->s, secs, 2); }
        else merge_into(c, u->s);
        return;
    case SK_ARMOR:
        if (!stack_empty(*c) && (!item_valid(c->id) || ITEMS[c->id].armor_slot != u->armor)) return;
        if (ench_level(*u->s, ENCH_BINDING_CURSE) && g_player.gamemode != GM_CREATIVE) return;   // curse of binding
        if (shift) { int secs[2] = {SEC_MAIN, SEC_HOTBAR}; quick_move(u->s, secs, 2); return; }
        { ItemStack t = *u->s; *u->s = *c; *c = t; }
        return;
    default: break;
    }
    if (shift) {
        if (stack_empty(*u->s)) return;
        // armor goes straight to its slot
        if (G.screen == SCR_INVENTORY && item_valid(u->s->id) && ITEMS[u->s->id].armor_slot >= 0 && stack_empty(g_player.inv.armor[ITEMS[u->s->id].armor_slot])) {
            g_player.inv.armor[ITEMS[u->s->id].armor_slot] = *u->s; *u->s = (ItemStack){0}; return;
        }
        bool has_container = screen_is_container(G.screen) && G.screen != SCR_INVENTORY && G.screen != SCR_CRAFTING;
        if (u->section == SEC_CONTAINER) { int s[2] = {SEC_HOTBAR, SEC_MAIN}; quick_move(u->s, s, 2); }
        else if (has_container) { int s[1] = {SEC_CONTAINER}; quick_move(u->s, s, 1); }
        else if (u->section == SEC_HOTBAR) { int s[1] = {SEC_MAIN}; quick_move(u->s, s, 1); }
        else { int s[1] = {SEC_HOTBAR}; quick_move(u->s, s, 1); }
        return;
    }
    ItemStack *s = u->s;
    if (button == 0) {
        if (stack_empty(*c)) { *c = *s; *s = (ItemStack){0}; }
        else if (stack_empty(*s) || !stack_can_merge(*s, *c)) {
            if (!stack_empty(*s) && stack_can_merge(*s, *c)) merge_into(s, c);
            else { ItemStack t = *s; *s = *c; *c = t; }
        } else merge_into(s, c);
    } else {
        if (stack_empty(*c)) {
            if (stack_empty(*s)) return;
            int half = (s->count + 1) / 2;
            *c = *s; c->count = (u8)half; s->count -= half;
            if (!s->count) *s = (ItemStack){0};
        } else if (stack_empty(*s) || (stack_can_merge(*s, *c) && s->count < stack_max(*s))) {
            if (stack_empty(*s)) { *s = *c; s->count = 0; }
            s->count++; c->count--;
            if (!c->count) *c = (ItemStack){0};
        }
    }
}

static void draw_and_handle_slots(f32 px, f32 py, f32 pw, f32 ph) {
    USlot *hover = NULL;
    for (int i = 0; i < nslots; i++) {
        USlot *u = &slots[i];
        ui_slot(u->x - 1, u->y - 1);
        if (u->kind == SK_ARMOR && stack_empty(*u->s)) ui_icon(7, u->x + 4, u->y + 4, rgba_u32(255, 255, 255, 90));
        ui_item(u->x, u->y, *u->s, true);
        if (ui_hover(u->x - 1, u->y - 1, 18, 18)) { hover = u; ui_rect(u->x, u->y, 16, 16, rgba_u32(255, 255, 255, 110)); }
    }
    bool shift = key_down(GLFW_KEY_LEFT_SHIFT) || key_down(GLFW_KEY_RIGHT_SHIFT);
    for (int b = 0; b < 2; b++) {
        if (!g_win.mouse_pressed[b == 0 ? GLFW_MOUSE_BUTTON_LEFT : GLFW_MOUSE_BUTTON_RIGHT]) continue;
        if (hover) slot_click(hover, b, shift);
        else if (!ui_hover(px, py, pw, ph) && !stack_empty(g_player.cursor)) {   // drop outside the window
            Entity *e = player_ent();
            ItemStack d = g_player.cursor;
            if (b == 1) d.count = 1;
            g_player.cursor.count -= d.count;
            if (!g_player.cursor.count) g_player.cursor = (ItemStack){0};
            if (e) item_drop(entity_eye(e), d, v3(sinf(e->yaw) * 5, 1.5f, cosf(e->yaw) * 5), 40);
        }
    }
    if (hover && hover->kind == SK_NORMAL)
        for (int k = 0; k < 9; k++) if (key_pressed(GLFW_KEY_1 + k)) { ItemStack t = *hover->s; *hover->s = g_player.inv.slots[k]; g_player.inv.slots[k] = t; }
    if (hover && key_pressed(GLFW_KEY_Q) && hover->kind == SK_NORMAL && !stack_empty(*hover->s)) {
        Entity *e = player_ent();
        ItemStack d = *hover->s; d.count = key_down(GLFW_KEY_LEFT_CONTROL) ? d.count : 1;
        hover->s->count -= d.count; if (!hover->s->count) *hover->s = (ItemStack){0};
        if (e) item_drop(entity_eye(e), d, v3(sinf(e->yaw) * 5, 1.5f, cosf(e->yaw) * 5), 40);
    }
    ui_item(ui_mx - 8, ui_my - 8, g_player.cursor, true);
    if (hover && stack_empty(g_player.cursor) && !stack_empty(*hover->s)) {
        const ItemDef *it = &ITEMS[hover->s->id];
        char sub[96] = "";
        if (it->variants) { const PotionDef *pd = &POTIONS[hover->s->damage % POTION_COUNT]; if (pd->effect) snprintf(sub, sizeof sub, "%s%s (%d:%02d)", EFFECT_NAMES[pd->effect], pd->amplifier ? " II" : "", pd->seconds / 60, pd->seconds % 60); else snprintf(sub, sizeof sub, "No effects"); }
        else if (it->durability) snprintf(sub, sizeof sub, "Durability: %d / %d", it->durability - hover->s->damage, it->durability);
        else if (it->food) snprintf(sub, sizeof sub, "Restores %d hunger", it->food);
        else if (it->armor_points) snprintf(sub, sizeof sub, "+%d armor", it->armor_points);
        else if (it->attack_damage > 1 && it->category == CAT_WEAPON) snprintf(sub, sizeof sub, "%.0f attack damage", it->attack_damage);
        if (!sub[0] && G.debug) snprintf(sub, sizeof sub, "ultimatecraft:%s", it->key);
        char lines[8][64];
        int nl = stack_tooltip_lines(*hover->s, lines, 7);
        bool ench = ench_count(*hover->s) && !strstr(it->key, "_banner");
        u32 tc = hover->s->name ? rgba_u32(255, 255, 170, 255) : ench ? rgba_u32(85, 255, 255, 255) : COL_WHITE;
        ui_tooltip_lines(stack_display_name(*hover->s), tc, (const char (*)[64])lines, nl, sub[0] ? sub : NULL);
    }
}

static void draw_player_preview(f32 x, f32 y, f32 w, f32 h) {
    ui_rect(x, y, w, h, rgba_u32(0, 0, 0, 255));
    ui_rect(x + 1, y + 1, w - 2, h - 2, rgba_u32(40, 40, 40, 255));
    b2d_flush();
    static Texture skin;
    if (!skin.id) skin = texture_load("res/textures/entity/player.png");
    mat4 proj = m4_ortho(0, (f32)ui_w, (f32)ui_h, 0, -200, 200);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    b3d_begin(&proj, (vec4){0, 0, 0, 1}, 1e9f, 2e9f);
    b3d_set_texture(skin.id);
    f32 cx = x + w / 2, by = y + h - 6;
    f32 look = clampf((ui_mx - cx) / 60.0f, -1, 1), pitch = clampf((ui_my - (y + 20)) / 60.0f, -1, 1);
    mat4 m = m4_mul(m4_translate(v3(cx, by, 50)), m4_scale(v3(28, -28, 28)));
    m = m4_mul(m, m4_rot_y(look * 0.5f));
    ModelPose ps = {0};
    ps.head_yaw = -look * 0.5f; ps.head_pitch = -pitch * 0.5f; ps.age = (f32)plat_time_wrap() * 60;
    model_draw(MODEL_HUMANOID, m, &ps, COL_WHITE, 64, 32);
    b3d_flush();
    glDisable(GL_DEPTH_TEST);
    b2d_begin(ui_w, ui_h);
}

static void container_screen(void) {
    nslots = 0;
    f32 pw = 176, ph = 166;
    if (G.screen == SCR_INVENTORY && g_player.gamemode == GM_CREATIVE) ph = 136;
    f32 x0 = floorf((ui_w - pw) / 2), y0 = floorf((ui_h - ph) / 2);
    bool book = G.recipe_book && (G.screen == SCR_CRAFTING || (G.screen == SCR_INVENTORY && g_player.gamemode != GM_CREATIVE));
    if (book) x0 += 77;
    ui_rect(0, 0, (f32)ui_w, (f32)ui_h, rgba_u32(0, 0, 0, 140));
    if (book) recipe_book_panel(x0 - 151, y0);
    ui_panel(x0, y0, pw, ph);
    if (G.screen == SCR_CRAFTING || (G.screen == SCR_INVENTORY && g_player.gamemode != GM_CREATIVE)) {   // recipe book toggle
        f32 bx = G.screen == SCR_CRAFTING ? x0 + 5 : x0 + 104, by = G.screen == SCR_CRAFTING ? y0 + 34 : y0 + 61;
        bool hov = ui_hover(bx, by, 20, 18);
        ui_rect(bx, by, 20, 18, hov ? rgba_u32(120, 200, 120, 255) : rgba_u32(80, 150, 80, 255));
        ui_rect(bx + 4, by + 3, 12, 12, rgba_u32(140, 70, 40, 255)); ui_rect(bx + 6, by + 5, 8, 8, rgba_u32(230, 220, 190, 255));
        if (hov) { ui_tooltip(G.recipe_book ? "Hide recipes" : "Show recipe book", NULL); if (g_win.mouse_pressed[GLFW_MOUSE_BUTTON_LEFT]) { G.recipe_book = !G.recipe_book; g_win.mouse_pressed[GLFW_MOUSE_BUTTON_LEFT] = false; } }
    }
    u32 title = rgba_u32(64, 64, 64, 255);
    BlockEntity *be = NULL;
    if ((G.screen == SCR_CHEST && !G.open_ender) || G.screen == SCR_FURNACE || G.screen == SCR_BREWING) {
        be = world_block_entity(G.open_be, false, 0);
        if (!be) { game_close_screen(); return; }
    }
    const char *block_title = G.in_world ? BLOCKS[world_get_block(G.open_be.x, G.open_be.y, G.open_be.z)].name : "";
    if (G.open_be.y == -500) block_title = be && be->type == BE_HOPPER ? "Minecart with Hopper" : "Chest";
    if (G.screen == SCR_INVENTORY && g_player.gamemode == GM_CREATIVE) {
        static const char *TAB_NAMES[TAB_COUNT + 2] = {"Building", "Colored", "Natural", "Functional", "Redstone", "Tools", "Combat", "Food", "Ingredients", "Spawn Eggs", "Operator", "Search", "UltimateCraft"};
        int ntabs = TAB_COUNT + 2;   // + Search + UltimateCraft (everything new in this game, in one place)
        static bool uc_built, is_uc[ITEM_MAX];
        if (!uc_built) { uc_built = true; for (int k = 0; k < UC_ITEM_COUNT; k++) if (UC_ITEMS[k] < ITEM_MAX) is_uc[UC_ITEMS[k]] = true; }
        bool uc_tab = G.creative_tab == TAB_COUNT + 1;
        f32 tw = (pw + 40) / ntabs;
        for (int t = 0; t < ntabs; t++) {
            f32 tx = x0 - 20 + t * tw, ty = y0 - 14;
            bool sel = G.creative_tab == t;
            ui_rect(tx, ty, tw - 1, 13, sel ? rgba_u32(198, 198, 198, 255) : rgba_u32(120, 120, 120, 255));
            char ab[6]; snprintf(ab, sizeof ab, "%.4s", TAB_NAMES[t]);
            ui_text(tx + 2, ty + 3, ab, sel ? title : rgba_u32(230, 230, 230, 255), false);
            if (ui_hover(tx, ty, tw - 1, 13)) { ui_tooltip(TAB_NAMES[t], NULL); if (g_win.mouse_pressed[GLFW_MOUSE_BUTTON_LEFT]) { G.creative_tab = t; G.creative_scroll = 0; } }
        }
        bool search = G.creative_tab == TAB_COUNT;
        if (search) {
            for (int k = 0; k < g_win.nchars; k++) {
                u32 ch = g_win.chars[k];
                size_t l = strlen(G.creative_search);
                if (ch >= 32 && ch < 127 && l + 1 < sizeof G.creative_search) { G.creative_search[l] = (char)ch; G.creative_search[l + 1] = 0; G.creative_scroll = 0; }
            }
            g_win.nchars = 0;
            if (key_pressed(GLFW_KEY_BACKSPACE) && G.creative_search[0]) G.creative_search[strlen(G.creative_search) - 1] = 0;
            ui_rect(x0 + 80, y0 + 4, 88, 11, rgba_u32(0, 0, 0, 255));
            char sb[64]; snprintf(sb, sizeof sb, "%s_", G.creative_search);
            ui_text(x0 + 82, y0 + 6, sb, COL_WHITE, false);
        }
        ui_text(x0 + 8, y0 + 6, TAB_NAMES[G.creative_tab], title, false);
        ItemStack *ids = palette_list;
        int n = 0;
        char want[64];
        { size_t k = 0; for (const char *s = G.creative_search; *s && k + 1 < sizeof want; s++) want[k++] = (char)tolower((unsigned char)*s); want[k] = 0; }
        for (int i = 1; i < ITEM_MAX; i++) {
            if (!item_valid(i)) continue;
            if (search) {
                if (want[0]) {
                    char nm[96]; size_t k = 0;
                    for (const char *s = ITEMS[i].name; *s && k + 1 < sizeof nm; s++) nm[k++] = (char)tolower((unsigned char)*s);
                    nm[k] = 0;
                    if (!strstr(nm, want) && !strstr(ITEMS[i].key, want)) continue;
                }
            } else if (uc_tab) { if (!is_uc[i]) continue; }
            else if (ITEMS[i].tab != G.creative_tab) continue;
            if (ITEMS[i].variants) { for (int v = 0; v < ITEMS[i].variants; v++) { ItemStack st = stack_of(i, 1); st.damage = (u16)v; ids[n++] = st; } continue; }
            if (i == I_ENCHANTED_BOOK) {
                for (int en = 1; en < ENCH_COUNT; en++) {
                    if (search && want[0]) { char nm[64]; size_t k = 0; for (const char *q = ENCHANTS[en].name; *q && k + 1 < sizeof nm; q++) nm[k++] = (char)tolower((unsigned char)*q); nm[k] = 0; if (!strstr(nm, want) && !strstr("enchanted book", want)) continue; }
                    ItemStack st = stack_of(i, 1); ench_add(&st, en, ENCHANTS[en].max_level); ids[n++] = st;
                }
                continue;
            }
            ids[n++] = stack_of(i, 1);
        }
        // potions: one entry per potion type
        int rows = (n + 8) / 9, maxs = MAXI(0, rows - 5);
        if (g_win.scroll != 0) G.creative_scroll = clampi(G.creative_scroll - (int)g_win.scroll, 0, maxs);
        G.creative_scroll = clampi(G.creative_scroll, 0, maxs);
        for (int r = 0; r < 5; r++) for (int c = 0; c < 9; c++) {
            int k = (r + G.creative_scroll) * 9 + c;
            if (k >= n) continue;
            palette_tmp[r * 9 + c] = ids[k];
            add_slot(x0 + 8 + c * 18, y0 + 18 + r * 18, &palette_tmp[r * 9 + c], SK_PALETTE, SEC_CONTAINER);
            slots[nslots - 1].palette_item = ids[k].id;
        }
        if (maxs > 0) {   // scroll bar
            ui_rect(x0 + pw - 6, y0 + 18, 3, 90, rgba_u32(80, 80, 80, 255));
            ui_rect(x0 + pw - 6, y0 + 18 + 80.0f * G.creative_scroll / maxs, 3, 10, COL_WHITE);
        }
        char cnt[32]; snprintf(cnt, sizeof cnt, "%d items", n);
        ui_text(x0 + pw - 8 - ui_text_width(cnt), y0 + 6 + (search ? 100 : 0), cnt, title, false);
        for (int c = 0; c < 9; c++) add_slot(x0 + 8 + c * 18, y0 + 112, &g_player.inv.slots[c], SK_NORMAL, SEC_HOTBAR);
        draw_and_handle_slots(x0, y0, pw, ph);
        return;
    }
    switch (G.screen) {
    case SCR_ENCHANT: enchant_screen(x0, y0, title); break;
    case SCR_ANVIL: anvil_screen(x0, y0, title); break;
    case SCR_GRINDSTONE: grindstone_screen(x0, y0, title); break;
    case SCR_LOOM: loom_screen(x0, y0, title); break;
    case SCR_CARTOGRAPHY: cartography_screen(x0, y0, title); break;
    case SCR_LECTERN: lectern_screen(x0, y0, title); break;
    case SCR_TRADE: trade_screen(x0, y0, title); break;
    case SCR_INVENTORY:
        for (int i = 0; i < 4; i++) { add_slot(x0 + 8, y0 + 8 + i * 18, &g_player.inv.armor[i], SK_ARMOR, SEC_ARMOR); slots[nslots - 1].armor = (s8)i; }
        draw_player_preview(x0 + 26, y0 + 8, 50, 70);
        add_slot(x0 + 77, y0 + 62, &g_player.inv.offhand, SK_NORMAL, SEC_ARMOR);
        ui_text(x0 + 87, y0 + 8, "Crafting", title, false);
        for (int r = 0; r < 2; r++) for (int c = 0; c < 2; c++) add_slot(x0 + 88 + c * 18, y0 + 26 + r * 18, &g_player.craft[r * 3 + c], SK_NORMAL, SEC_CONTAINER);
        break;
    case SCR_CRAFTING:
        ui_text(x0 + 28, y0 + 6, "Crafting", title, false);
        for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) add_slot(x0 + 30 + c * 18, y0 + 17 + r * 18, &g_player.craft[r * 3 + c], SK_NORMAL, SEC_CONTAINER);
        break;
    case SCR_CHEST:
        if (G.open_ender) {
            ui_text(x0 + 8, y0 + 6, "Ender Chest", title, false);
            for (int r = 0; r < 3; r++) for (int c = 0; c < 9; c++) add_slot(x0 + 8 + c * 18, y0 + 18 + r * 18, &g_player.ender[r * 9 + c], SK_NORMAL, SEC_CONTAINER);
        } else if (be->type == BE_HOPPER) {
            ui_text(x0 + 8, y0 + 6, block_title, title, false);
            for (int c = 0; c < 5; c++) add_slot(x0 + 44 + c * 18, y0 + 35, &be->slots[c], SK_NORMAL, SEC_CONTAINER);
        } else if (be->type == BE_DISPENSER) {
            ui_text(x0 + 8, y0 + 6, block_title, title, false);
            for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) add_slot(x0 + 62 + c * 18, y0 + 17 + r * 18, &be->slots[r * 3 + c], SK_NORMAL, SEC_CONTAINER);
        } else {
            ui_text(x0 + 8, y0 + 6, block_title, title, false);
            for (int r = 0; r < 3; r++) for (int c = 0; c < 9; c++) add_slot(x0 + 8 + c * 18, y0 + 18 + r * 18, &be->slots[r * 9 + c], SK_NORMAL, SEC_CONTAINER);
        }
        break;
    case SCR_STONECUTTER: {
        ui_text(x0 + 8, y0 + 6, "Stonecutter", title, false);
        add_slot(x0 + 20, y0 + 33, &g_player.craft[0], SK_NORMAL, SEC_CONTAINER);
        ncutter = stack_empty(g_player.craft[0]) ? 0 : stonecutter_options(g_player.craft[0].id, cutter_options, 64);
        if (G.cutter_sel >= ncutter) G.cutter_sel = -1;
        f32 gx = x0 + 52, gy = y0 + 15;
        ui_rect(gx - 1, gy - 1, 4 * 18 + 2, 3 * 18 + 2, rgba_u32(60, 60, 60, 255));
        for (int i = 0; i < ncutter && i < 12; i++) {
            f32 bx = gx + (i % 4) * 18, by = gy + (i / 4) * 18;
            bool hov = ui_hover(bx, by, 18, 18);
            ui_rect(bx, by, 18, 18, G.cutter_sel == i ? rgba_u32(100, 160, 100, 255) : hov ? rgba_u32(170, 170, 200, 255) : rgba_u32(139, 139, 139, 255));
            ui_item(bx + 1, by + 1, cutter_options[i], true);
            if (hov && g_win.mouse_pressed[GLFW_MOUSE_BUTTON_LEFT]) { G.cutter_sel = i; sound_play(SND_CLICK, 0.4f, 1.2f); }
            if (hov) ui_tooltip(item_name(cutter_options[i].id), NULL);
        }
        if (ncutter > 12) { char b[24]; snprintf(b, sizeof b, "+%d", ncutter - 12); ui_text(gx + 74, gy + 44, b, title, false); }
        cutter_out = current_result();
    } break;
    case SCR_SMITHING:
        ui_text(x0 + 44, y0 + 6, "Upgrade Gear", title, false);
        add_slot(x0 + 8, y0 + 48, &g_player.craft[0], SK_NORMAL, SEC_CONTAINER);
        add_slot(x0 + 26, y0 + 48, &g_player.craft[1], SK_NORMAL, SEC_CONTAINER);
        add_slot(x0 + 44, y0 + 48, &g_player.craft[2], SK_NORMAL, SEC_CONTAINER);
        ui_text(x0 + 8, y0 + 26, "Template Base Material", rgba_u32(90, 90, 90, 255), false);
        break;
    case SCR_BREWING: {
        ui_text(x0 + 60, y0 + 6, "Brewing Stand", title, false);
        add_slot(x0 + 56, y0 + 51, &be->slots[0], SK_NORMAL, SEC_CONTAINER);
        add_slot(x0 + 79, y0 + 58, &be->slots[1], SK_NORMAL, SEC_CONTAINER);
        add_slot(x0 + 102, y0 + 51, &be->slots[2], SK_NORMAL, SEC_CONTAINER);
        add_slot(x0 + 79, y0 + 17, &be->slots[3], SK_NORMAL, SEC_CONTAINER);
        add_slot(x0 + 17, y0 + 17, &be->slots[4], SK_NORMAL, SEC_CONTAINER);
        ui_rect(x0 + 18, y0 + 44, 18, 4, rgba_u32(60, 60, 60, 255));
        ui_rect(x0 + 18, y0 + 44, 18 * be->data / 20.0f, 4, rgba_u32(230, 160, 40, 255));
        ui_rect(x0 + 99, y0 + 17, 4, 28, rgba_u32(60, 60, 60, 255));
        ui_rect(x0 + 99, y0 + 17, 4, 28 * be->cook_time / 1200.0f, COL_WHITE);
    } break;
    case SCR_FURNACE: {
        ui_text(x0 + 88 - ui_text_width(block_title) / 2.0f, y0 + 6, block_title, title, false);
        add_slot(x0 + 56, y0 + 17, &be->slots[0], SK_NORMAL, SEC_CONTAINER);
        add_slot(x0 + 56, y0 + 53, &be->slots[1], SK_NORMAL, SEC_CONTAINER);
        add_slot(x0 + 116, y0 + 35, &be->slots[2], SK_OUTPUT, SEC_RESULT);
        // flame + progress arrow
        f32 burn = be->burn_max ? (f32)be->burn_time / be->burn_max : 0;
        ui_rect(x0 + 57, y0 + 37, 14, 14, rgba_u32(139, 139, 139, 255));
        ui_rect(x0 + 57, y0 + 37 + 14 * (1 - burn), 14, 14 * burn, rgba_u32(255, 140, 20, 255));
        f32 prog = be->cook_time / (be->type == BE_FURNACE ? 600.0f : 300.0f);
        ui_rect(x0 + 79, y0 + 39, 24, 8, rgba_u32(139, 139, 139, 255));
        ui_rect(x0 + 79, y0 + 39, 24 * prog, 8, COL_WHITE);
        ui_rect(x0 + 99, y0 + 35, 4, 16, rgba_u32(139, 139, 139, 255));
    } break;
    default: break;
    }
    if (ws_has_result() && G.screen != SCR_TRADE) {
        craft_result = ws_result();
        f32 rx = G.screen == SCR_ANVIL ? x0 + 134 : G.screen == SCR_GRINDSTONE ? x0 + 129 : G.screen == SCR_LOOM ? x0 + 143 : x0 + 145;
        f32 ry = G.screen == SCR_ANVIL ? y0 + 47 : G.screen == SCR_GRINDSTONE ? y0 + 34 : G.screen == SCR_LOOM ? y0 + 57 : y0 + 39;
        if (G.screen != SCR_CARTOGRAPHY && G.screen != SCR_LOOM) ui_text(rx - 18, ry + 4, "=>", title, false);
        add_slot(rx, ry, &craft_result, SK_RESULT, SEC_RESULT);
    }
    if (G.screen == SCR_INVENTORY || G.screen == SCR_CRAFTING || G.screen == SCR_STONECUTTER || G.screen == SCR_SMITHING) {
        craft_result = current_result();
        (void)smith_result; (void)cutter_out;
        f32 rx = G.screen == SCR_INVENTORY ? x0 + 144 : G.screen == SCR_STONECUTTER ? x0 + 143 : G.screen == SCR_SMITHING ? x0 + 98 : x0 + 124;
        f32 ry = G.screen == SCR_INVENTORY ? y0 + 36 : G.screen == SCR_SMITHING ? y0 + 48 : y0 + 35;
        ui_text(rx - 18, ry + 4, "=>", title, false);
        add_slot(rx, ry, &craft_result, SK_RESULT, SEC_RESULT);
    }
    if (G.screen != SCR_INVENTORY) ui_text(x0 + 8, y0 + 73, "Inventory", title, false);
    add_player_inventory(x0, y0);
    draw_and_handle_slots(x0, y0, pw, ph);
}


// ============================================================================ menus
static char world_names[64][64];
static int nworlds, selected_world = -1;
static bool worlds_scanned;
static f64 last_click_time;

static void scan_worlds(void) {
    plat_mkdirs("saves");
    nworlds = plat_list_dirs("saves", world_names, 64);
    worlds_scanned = true;
    if (selected_world >= nworlds) selected_world = -1;
}

static void title_screen(void) {
    ui_dirt_background(0.3f);
    f32 cx = ui_w / 2.0f;
    const char *logo = "ULTIMATECRAFT";
    f32 sc = ui_w > 420 ? 4.0f : 3.0f;
    f32 lw = ui_text_width(logo) * sc;
    ui_text_scaled(cx - lw / 2 + sc, 30 + sc, sc, logo, rgba_u32(40, 40, 40, 255), false);
    ui_text_scaled(cx - lw / 2, 30, sc, logo, rgba_u32(230, 230, 230, 255), false);
    f32 pulse = 1.0f + 0.06f * sinf((f32)plat_time_wrap() * 6);
    static const char *SPL[] = {"The Sift awaits!", "Beware the Cosmic Tyrant!", "Lamps burn below!", "Ghosts fear gold!", "Every pixel procedural!",
        "Now with fishing!", "Mind the sculk!", "Hello, Delvari!", "Ask a wandering soul!", "Bring oxygen!"};
    static int si = -1; if (si < 0) si = (int)((u64)(plat_time() * 1000) % ARRAY_LEN(SPL));
    const char *splash = SPL[si];
    ui_text_scaled(cx + lw / 2 - ui_text_width(splash) * pulse * 0.8f, 30 + 8 * sc + 2, pulse, splash, rgba_u32(255, 255, 0, 255), true);
    f32 y = ui_h / 4.0f + 48;
    if (ui_button(cx - 100, y, 200, 20, TR("Singleplayer"), true)) { worlds_scanned = false; game_open_screen(SCR_WORLDS); }
    if (ui_button(cx - 100, y + 24, 200, 20, TR("Settings"), true)) { G.return_screen = SCR_TITLE; game_open_screen(SCR_SETTINGS); }
    if (ui_button(cx - 100, y + 48, 200, 20, TR("Quit Game"), true)) G.quit = true;
    ui_text(2, ui_h - 10.0f, "UltimateCraft " UC_VERSION, COL_WHITE, true);
    const char *c = "Made by RAJDIP DHAR";
    ui_text(ui_w - ui_text_width(c) - 2.0f, ui_h - 10.0f, c, COL_WHITE, true);
}

static void worlds_screen(void) {
    if (!worlds_scanned) scan_worlds();
    ui_dirt_background(0.25f);
    f32 cx = ui_w / 2.0f;
    ui_text_center(cx, 12, TR("Select World"), COL_WHITE, true);
    f32 ly = 30, lh = ui_h - 90.0f;
    ui_rect(0, ly, (f32)ui_w, lh, rgba_u32(0, 0, 0, 140));
    for (int i = 0; i < nworlds; i++) {
        f32 y = ly + 4 + i * 30;
        if (y + 28 > ly + lh) break;
        bool hov = ui_hover(cx - 110, y, 220, 28);
        if (i == selected_world) { ui_rect(cx - 110, y, 220, 28, rgba_u32(128, 128, 128, 255)); ui_rect(cx - 109, y + 1, 218, 26, rgba_u32(0, 0, 0, 255)); }
        u64 seed = 0; int gm = 0;
        save_load_level(world_names[i], &seed, &gm);
        char info[96]; snprintf(info, sizeof info, "%s mode, seed %llu", gm ? "Creative" : "Survival", (unsigned long long)seed);
        ui_text(cx - 104, y + 4, world_names[i], COL_WHITE, true);
        ui_text(cx - 104, y + 15, info, rgba_u32(128, 128, 128, 255), false);
        if (hov && g_win.mouse_pressed[GLFW_MOUSE_BUTTON_LEFT]) {
            if (selected_world == i && plat_time_wrap() - last_click_time < 0.4) { game_start_world(world_names[i], seed, gm, false); return; }
            selected_world = i; last_click_time = plat_time_wrap();
            sound_play(SND_CLICK, 0.4f, 1.0f);
        }
    }
    if (nworlds == 0) ui_text_center(cx, ly + 20, TR("No worlds yet - create one!"), rgba_u32(170, 170, 170, 255), true);
    f32 by = ui_h - 52.0f;
    bool sel = selected_world >= 0 && selected_world < nworlds;
    if (ui_button(cx - 154, by, 100, 20, TR("Play Selected World"), sel)) {
        u64 seed; int gm; save_load_level(world_names[selected_world], &seed, &gm);
        game_start_world(world_names[selected_world], seed, gm, false);
        return;
    }
    if (ui_button(cx - 50, by, 100, 20, TR("Create New World"), true)) {
        snprintf(G.new_name, sizeof G.new_name, "New World");
        G.new_seed[0] = 0; G.new_mode = 0; G.focus_field = 0; G.new_difficulty = DIFF_NORMAL; G.new_cheats = false; G.new_hardcore = false;
        game_open_screen(SCR_CREATE);
    }
    if (ui_button(cx - 154, by + 24, 74, 20, TR("Delete"), sel)) { snprintf(G.selected_world, sizeof G.selected_world, "%s", world_names[selected_world]); game_open_screen(SCR_CONFIRM_DELETE); }
    if (ui_button(cx + 80, by + 24, 74, 20, TR("Cancel"), true)) game_open_screen(SCR_TITLE);
    if (ui_button(cx + 54, by, 100, 20, TR("Import Worlds"), true)) {   // every world folder in imports/ is copied into saves/
        char names[64][64]; plat_mkdir("imports"); int n = plat_list_dirs("imports", names, 64), got = 0;
        for (int i = 0; i < n; i++) {
            char src[200], chk[220], dst[200]; snprintf(src, sizeof src, "imports/%.60s", names[i]); snprintf(chk, sizeof chk, "%s/level.txt", src);
            FILE *f = fopen(chk, "rb"); if (!f) continue; fclose(f);
            snprintf(dst, sizeof dst, "saves/%.60s", names[i]);
            for (int k = 2; k < 100; k++) { char t[220]; snprintf(t, sizeof t, "%s/level.txt", dst); FILE *e2 = fopen(t, "rb"); if (!e2) break; fclose(e2); snprintf(dst, sizeof dst, "saves/%.50s_%d", names[i], k); }
            if (plat_copy_dir(src, dst) >= 0) got++;
        }
        snprintf(G.world_msg, sizeof G.world_msg, TR("Imported %d world(s) from the imports folder"), got);
        worlds_scanned = false;
    }
    if (ui_button(cx - 76, by + 24, 74, 20, TR("Backup"), sel)) {
        char src[200], bk[200]; snprintf(src, sizeof src, "saves/%s", world_names[selected_world]); snprintf(bk, sizeof bk, "backups/%s", world_names[selected_world]);
        plat_mkdir("backups"); long long nb = plat_backup_dir(src, bk, 3, 4096ll << 20);
        snprintf(G.world_msg, sizeof G.world_msg, nb >= 0 ? TR("Backed up to backups/%s/1") : TR("Backup failed for %s"), world_names[selected_world]);
    }
    if (ui_button(cx + 2, by + 24, 74, 20, TR("Export"), sel)) {
        char src[200], dst[200]; snprintf(src, sizeof src, "saves/%s", world_names[selected_world]); snprintf(dst, sizeof dst, "exports/%s", world_names[selected_world]);
        plat_mkdir("exports"); plat_copy_dir(src, dst);
        snprintf(G.world_msg, sizeof G.world_msg, TR("Exported to exports/%s (zip it to share)"), world_names[selected_world]);
    }
    if (G.world_msg[0]) ui_text_center(cx, by - 12, G.world_msg, rgba_u32(255, 255, 160, 255), true);
}

static void confirm_delete_screen(void) {
    ui_dirt_background(0.25f);
    f32 cx = ui_w / 2.0f;
    ui_text_center(cx, ui_h / 3.0f, TR("Are you sure you want to delete this world?"), COL_WHITE, true);
    char b[160]; snprintf(b, sizeof b, "'%s' will be lost forever! (A long time!)", G.selected_world);
    ui_text_center(cx, ui_h / 3.0f + 14, b, rgba_u32(170, 170, 170, 255), true);
    if (ui_button(cx - 154, ui_h / 3.0f + 40, 150, 20, TR("Delete"), true)) {
        char path[160]; snprintf(path, sizeof path, "saves/%s", G.selected_world);
        plat_remove_tree(path);
        worlds_scanned = false; selected_world = -1;
        game_open_screen(SCR_WORLDS);
    }
    if (ui_button(cx + 4, ui_h / 3.0f + 40, 150, 20, TR("Cancel"), true)) game_open_screen(SCR_WORLDS);
}

static void create_screen(void) {
    ui_dirt_background(0.25f);
    f32 cx = ui_w / 2.0f;
    ui_text_center(cx, 20, TR("Create New World"), COL_WHITE, true);
    ui_text(cx - 100, 47, "World Name", rgba_u32(160, 160, 160, 255), false);
    if (ui_textfield(cx - 100, 58, 200, G.new_name, sizeof G.new_name, G.focus_field == 0)) G.focus_field = 0;
    ui_text(cx - 100, 87, "Seed for the world generator (blank = random)", rgba_u32(160, 160, 160, 255), false);
    if (ui_textfield(cx - 100, 98, 200, G.new_seed, sizeof G.new_seed, G.focus_field == 1)) G.focus_field = 1;
    if (key_pressed(GLFW_KEY_TAB)) G.focus_field = !G.focus_field;
    // Minecraft's choices: Survival, Hardcore (survival, hard, one life), Creative
    int sel = G.new_hardcore ? 1 : G.new_mode == GM_CREATIVE ? 2 : 0;
    static const char *MODES[3] = {"Survival", "Hardcore", "Creative"};
    char mode[48]; snprintf(mode, sizeof mode, "Game Mode: %s", MODES[sel]);
    if (ui_button(cx - 100, 126, 200, 20, mode, true)) {
        sel = (sel + 1) % 3;
        G.new_hardcore = sel == 1; G.new_mode = sel == 2 ? GM_CREATIVE : GM_SURVIVAL;
        if (sel == 2) G.new_cheats = true;
    }
    ui_text_center(cx, 148, sel == 2 ? "Unlimited resources, free flying and destroy blocks instantly" : sel == 1 ? "Same as Survival, but locked at hardest difficulty, and one life only" : "Search for resources, craft, gain levels, health and hunger",
                   rgba_u32(160, 160, 160, 255), true);
    char diff[48]; snprintf(diff, sizeof diff, "Difficulty: %c%s", toupper((unsigned char)DIFFICULTY_NAMES[G.new_hardcore ? 3 : G.new_difficulty][0]), DIFFICULTY_NAMES[G.new_hardcore ? 3 : G.new_difficulty] + 1);
    if (ui_button(cx - 154, 162, 150, 20, diff, !G.new_hardcore)) G.new_difficulty = (G.new_difficulty + 1) % 4;
    char ch[48]; snprintf(ch, sizeof ch, "Allow Commands: %s", G.new_cheats && !G.new_hardcore ? TR("ON") : TR("OFF"));
    if (ui_button(cx + 4, 162, 150, 20, ch, !G.new_hardcore)) G.new_cheats = !G.new_cheats;
    ui_text_center(cx, 186, TR("Commands like /gamemode, /give and /gamerule"), rgba_u32(160, 160, 160, 255), true);
    bool create = ui_button(cx - 154, ui_h - 32.0f, 150, 20, TR("Create New World"), G.new_name[0] != 0) || (key_pressed(GLFW_KEY_ENTER) && G.new_name[0]);
    if (ui_button(cx + 4, ui_h - 32.0f, 150, 20, TR("Cancel"), true)) { game_open_screen(SCR_WORLDS); return; }
    if (create) {
        // sanitise folder name and avoid clobbering an existing world
        char name[64]; int n = 0;
        for (const char *p = G.new_name; *p && n < 40; p++) {
            char ch = *p;
            if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == ' ' || ch == '-' || ch == '_') name[n++] = ch;
        }
        name[n] = 0;
        if (!n) snprintf(name, sizeof name, "World");
        char final_name[64]; snprintf(final_name, sizeof final_name, "%s", name);
        for (int k = 2; k < 100; k++) {
            char path[160]; snprintf(path, sizeof path, "saves/%s", final_name);
            if (!plat_exists(path)) break;
            snprintf(final_name, sizeof final_name, "%s (%d)", name, k);
        }
        u64 seed = G.new_seed[0] ? seed_from_string(G.new_seed) : (hash64((u64)(plat_time_wrap() * 1e6) ^ (u64)rand()) & 0xFFFFFFFFFFFFull);
        game_start_world(final_name, seed, G.new_mode, true);
    }
}

static void settings_screen(void) {
    if (G.in_world) ui_rect(0, 0, (f32)ui_w, (f32)ui_h, rgba_u32(0, 0, 0, 150)); else ui_dirt_background(0.25f);
    f32 cx = ui_w / 2.0f, y = 30;
    ui_text_center(cx, 12, TR("Settings"), COL_WHITE, true);
    char b[64];
    f32 rd = (f32)G.set.render_dist;
    snprintf(b, sizeof b, TR("Render Distance: %d chunks"), G.set.render_dist);
    if (ui_slider(cx - 155, y, 150, b, &rd, 2, MAX_RENDER_DIST, 1)) G.set.render_dist = (int)rd;
    snprintf(b, sizeof b, "FOV: %.0f", G.set.fov);
    ui_slider(cx + 5, y, 150, b, &G.set.fov, 50, 110, 1);
    y += 24;
    snprintf(b, sizeof b, "Sensitivity: %.0f%%", G.set.sensitivity * 100);
    ui_slider(cx - 155, y, 150, b, &G.set.sensitivity, 0.1f, 1.5f, 0.01f);
    snprintf(b, sizeof b, "Master Volume: %.0f%%", G.set.master * 100);
    if (ui_slider(cx + 5, y, 150, b, &G.set.master, 0, 1, 0.01f)) settings_apply();
    y += 24;
    snprintf(b, sizeof b, "Music: %.0f%%", G.set.music * 100);
    if (ui_slider(cx - 155, y, 150, b, &G.set.music, 0, 1, 0.01f)) settings_apply();
    snprintf(b, sizeof b, "Sounds: %.0f%%", G.set.sfx * 100);
    if (ui_slider(cx + 5, y, 150, b, &G.set.sfx, 0, 1, 0.01f)) settings_apply();
    y += 24;
    if (G.set.gui_scale) snprintf(b, sizeof b, TR("GUI Scale: %d"), G.set.gui_scale); else snprintf(b, sizeof b, "%s", TR("GUI Scale: Auto"));
    if (ui_button(cx - 155, y, 150, 20, b, true)) G.set.gui_scale = (G.set.gui_scale + 1) % 5;
    snprintf(b, sizeof b, TR("View Bobbing: %s"), G.set.view_bob ? TR("ON") : TR("OFF"));
    if (ui_button(cx + 5, y, 150, 20, b, true)) G.set.view_bob = !G.set.view_bob;
    y += 24;
    snprintf(b, sizeof b, TR("VSync: %s"), G.set.vsync ? TR("ON") : TR("OFF"));
    if (ui_button(cx - 155, y, 150, 20, b, true)) { G.set.vsync = !G.set.vsync; settings_apply(); }
    snprintf(b, sizeof b, TR("Show FPS: %s"), G.set.show_fps ? TR("ON") : TR("OFF"));
    if (ui_button(cx + 5, y, 150, 20, b, true)) G.set.show_fps = !G.set.show_fps;
    y += 24;
    snprintf(b, sizeof b, TR("Invert Mouse: %s"), G.set.invert_y ? TR("ON") : TR("OFF"));
    if (ui_button(cx - 155, y, 150, 20, b, true)) G.set.invert_y = !G.set.invert_y;
    snprintf(b, sizeof b, TR("Clouds: %s"), G.set.clouds ? TR("ON") : TR("OFF"));
    if (ui_button(cx + 5, y, 150, 20, b, true)) { G.set.clouds = !G.set.clouds; settings_apply(); }
    y += 24;
    {   // graphics presets: Low / Medium / High set render & simulation distance, clouds and the frame cap
        static const char *PN[3] = {"Low", "Medium", "High"};
        static int preset = -1;
        char pb[64]; snprintf(pb, sizeof pb, TR("Graphics: %s"), preset < 0 ? TR("Custom") : TR(PN[preset]));
        if (ui_button(cx - 155, y, 310, 20, pb, true)) {
            preset = (preset + 1) % 3;
            static const int RD[3] = {4, 8, 12}, SDD[3] = {4, 8, 10};
            G.set.render_dist = RD[preset]; G.set.sim_dist = SDD[preset]; G.set.clouds = preset > 0; G.set.view_bob = true;
            settings_apply();
        }
        y += 24;
    }
    snprintf(b, sizeof b, TR("Language: %s"), LANG_NAMES[lang_index()]);
    if (ui_button(cx - 155, y, 310, 20, b, true)) { int li = (lang_index() + 1) % LANG_COUNT; snprintf(G.set.lang, sizeof G.set.lang, "%s", LANG_CODES[li]); settings_apply(); }
    y += 24;
    if (ui_button(cx - 155, y, 310, 20, TR("Controls & Accessibility..."), true)) { G.rebind = -1; G.controls_back = G.screen; game_open_screen(SCR_CONTROLS); }
    y += 26;
    if (ui_button(cx - 100, ui_h - 30.0f, 200, 20, TR("Done"), true)) {
        settings_apply(); settings_save();
        game_open_screen(G.return_screen);
    }
}

static void pause_screen(void) {
    ui_rect(0, 0, (f32)ui_w, (f32)ui_h, rgba_u32(0, 0, 0, 130));
    // six rows of 22 px, centred in the space above the info line, so every GUI scale fits
    f32 cx = ui_w / 2.0f, y = fmaxf(22.0f, fminf(ui_h / 4.0f, (ui_h - 24 - 6 * 22) / 2.0f + 10));
    ui_text_center(cx, y - 14, TR("Game Menu"), COL_WHITE, true);
    if (ui_button(cx - 100, y, 200, 20, TR("Back to Game"), true)) game_close_screen();
    if (ui_button(cx - 100, y + 22, 98, 20, TR("Settings"), true)) { G.return_screen = SCR_PAUSE; game_open_screen(SCR_SETTINGS); }
    if (ui_button(cx + 2, y + 22, 98, 20, TR("Save Game"), true)) { game_save(); game_message(COL_WHITE, "%s", TR("Game saved.")); }
    {   // Minecraft keeps difficulty in Options and "Allow Cheats" behind Open to LAN; both live here
        static const char *DN[4] = {"Peaceful", "Easy", "Normal", "Hard"};
        char b2[64];
        snprintf(b2, sizeof b2, TR("Difficulty: %s"), TR(DN[g_level.difficulty & 3]));
        if (g_level.difficulty_locked) strncat(b2, TR(" (locked)"), sizeof b2 - strlen(b2) - 1);
        if (ui_button(cx - 100, y + 44, 98, 20, b2, !g_level.difficulty_locked && !g_level.hardcore)) g_level.difficulty = (g_level.difficulty + 1) % 4;
        snprintf(b2, sizeof b2, TR("Cheats: %s"), g_level.cheats ? TR("ON") : TR("OFF"));
        if (ui_button(cx + 2, y + 44, 98, 20, b2, !g_level.hardcore)) g_level.cheats = !g_level.cheats;
    }
    if (ui_button(cx - 100, y + 66, 200, 20, TR("Guide: Dimensions & Bosses"), true)) { G.guide_page = 0; game_open_screen(SCR_GUIDE); }
    if (ui_button(cx - 100, y + 88, 98, 20, TR("Advancements"), true)) { G.adv_scroll = 0; game_open_screen(SCR_ADVANCEMENTS); }
    if (ui_button(cx + 2, y + 88, 98, 20, TR("Statistics"), true)) game_open_screen(SCR_STATS);
    if (ui_button(cx - 100, y + 110, 200, 20, TR("Save and Quit to Title"), true)) game_quit_to_title();
    char b[128];
    snprintf(b, sizeof b, TR("World: %s   Seed: %llu   Kills: %d   Score: %d"), g_world.name, (unsigned long long)g_world.seed, g_player.kills, g_player.score);
    ui_text_center(cx, ui_h - 14.0f, b, rgba_u32(200, 200, 200, 255), true);
}

static void death_screen(void) {
    f32 t = fminf(1.0f, g_player.death_ticks / 40.0f);
    ui_rect(0, 0, (f32)ui_w, (f32)ui_h, rgba_f(0.5f, 0, 0, 0.35f + 0.25f * t));
    f32 cx = ui_w / 2.0f, y = ui_h / 4.0f;
    const char *msg = "You Died!";
    ui_text_scaled(cx - ui_text_width(msg), y, 2, msg, COL_WHITE, true);
    char b[160];
    snprintf(b, sizeof b, TR("Player %s"), g_player.death_msg);
    ui_text_center(cx, y + 28, b, COL_WHITE, true);
    snprintf(b, sizeof b, TR("Score: %d"), g_player.score);
    ui_text_center(cx, y + 42, b, rgba_u32(255, 255, 85, 255), true);
    bool ready = g_player.death_ticks > TICKRATE;
    if (ui_button(cx - 100, y + 72, 200, 20, TR("Respawn"), ready)) player_respawn();
    if (ui_button(cx - 100, y + 96, 200, 20, TR("Title Screen"), ready)) game_quit_to_title();
}

// the in-game guide to UltimateCraft's dimensions and bosses (/guide, or the Game Menu)
static void guide_screen(void) {
    static const char *const P[][14] = {
        {"The Road to the Sift", "", "1. Overworld - gather, build, and find the Underground City", "   (/locate underground_city) deep below, around y -48.",
         "2. Defeat Morrowgate, Keeper of the Deep, for the Void Sigil.", "3. Find a stronghold with Star Eyes and open its Space portal", "   with the Void Sigil.",
         "4. In Space, defeat Zorvath, the Cosmic Tyrant: it drops the Sift Key", "   and opens a gateway to the Sift.", "5. In the Sift, pass the three Luminaries' trials.", "",
         "Optional: Hell, the Brimstone Revenant and its ghosts.", NULL},
        {"Dimensions", "", "Overworld - home. Villages, structures, the Underground City.", "Hell - fire and ghosts. Soul lanterns and holy water help.",
         "   Reach it with an obsidian portal.", "Space - asteroids under the stars, 0.4x gravity, NO AIR.", "   Wear a Space Helmet, drink Oxygen Canisters,",
         "   or stand near an Oxygen Bloom.", "The Sift - sky islands over a cloud sea. Peaceful souls,", "   the Luminaries, and eternal daylight.",
         "   Reach it with a gold/glowstone gate and the Sift Key.", NULL},
        {"Bosses", "", "Morrowgate, Keeper of the Deep (Underground City)", "   ground slams, lantern fire, summons sentinels. Drops the Void Sigil.",
         "Brimstone Revenant (Hell fortresses)", "   fire nova and fireballs. Drops the Brimstone Heart.",
         "Zorvath, the Cosmic Tyrant (Space)", "   3 phases: star bolts, gravity pulls, minions, meteors.", "   Drops the Sift Key and the Tyrant's Crown.",
         "Avatar of Thalor (the Sift)", "   crystal eruptions. Passing his trial completes the Ascension.", NULL},
        {"The Sift and Holy Tools", "", "Solenne (Temple of Dawn): bring 4 Radiant Crystals.", "Zephyrine (Spire of Winds): climb her spire. She trades.",
         "Thalor (Arena of Stone): defeat his Avatar.", "Blessings: Light (holy blows), Sky (no fall damage),", "   Vigor (+hearts, regeneration).", "",
         "Against ghosts: gold, Smite, divine and blessed weapons,", "   holy water, soul lanterns, the Tonic of Ghost Sight.",
         "Divine gear: 2 gold ingots + Radiant Crystal + Soul Essence.", NULL},
    };
    int np = (int)ARRAY_LEN(P);
    G.guide_page = clampi(G.guide_page, 0, np - 1);
    ui_rect(0, 0, (f32)ui_w, (f32)ui_h, rgba_u32(0, 0, 0, 170));
    f32 cx = ui_w / 2.0f, y = 8, bottom = ui_h - 26.0f;   // anchored to the screen edges so every GUI scale fits
    const char *const *pg = P[G.guide_page];
    ui_text_center(cx, y, TR(pg[0]), rgba_u32(255, 220, 120, 255), true);
    for (int i = 1; i < 14 && pg[i]; i++) ui_text(cx - 170, y + 10 + i * 12, pg[i], COL_WHITE, true);
    char b[32]; snprintf(b, sizeof b, TR("Page %d / %d"), G.guide_page + 1, np);
    ui_text_center(cx, bottom - 12, b, rgba_u32(170, 170, 170, 255), true);
    if (ui_button(cx - 154, bottom, 100, 20, TR("< Back"), G.guide_page > 0)) G.guide_page--;
    if (ui_button(cx - 50, bottom, 100, 20, TR("Done"), true)) game_close_screen();
    if (ui_button(cx + 54, bottom, 100, 20, TR("Next >"), G.guide_page < np - 1)) G.guide_page++;
}

// controls & accessibility: rebind keys (click, then press a key; Esc cancels), hold/toggle, reduced motion, display
static void controls_screen(void) {
    ui_rect(0, 0, (f32)ui_w, (f32)ui_h, rgba_u32(0, 0, 0, 180));
    f32 cx = ui_w / 2.0f, y = 6;
    char b[96];
    ui_text_center(cx, y, TR("Controls & Accessibility"), rgba_u32(255, 220, 120, 255), true);
    y += 14;
    if (G.rebind >= 0) {   // waiting for a key
        for (int k = 1; k <= GLFW_KEY_LAST; k++) if (g_win.key_pressed[k]) {
            if (k != GLFW_KEY_ESCAPE) G.set.keys[G.rebind] = k;
            G.rebind = -1; g_win.key_pressed[k] = false; settings_save(); break;
        }
    }
    for (int k = 0; k < KB_COUNT; k++) {   // three columns so everything fits small GUI scales
        f32 x = cx - 158 + (k % 3) * 106, yy = y + (k / 3) * 17;
        bool dup = false; for (int o = 0; o < KB_COUNT; o++) dup |= o != k && G.set.keys[o] == G.set.keys[k];
        if (G.rebind == k) snprintf(b, sizeof b, "%s: > ? <", TR(KB_NAMES[k])); else snprintf(b, sizeof b, "%s: %s%s", TR(KB_NAMES[k]), key_name(G.set.keys[k]), dup ? " (!)" : "");
        if (ui_button(x, yy, 104, 15, b, true)) G.rebind = k;
    }
    y += ((KB_COUNT + 2) / 3) * 17 + 4;
    static const int CAPS[] = {0, 30, 60, 120, 144, 240};
    for (int i = 0; i < 12; i++) {   // option grid, three per row (Done is the last cell)
        f32 x = cx - 158 + (i % 3) * 106, yy = y + (i / 3) * 17;
        switch (i) {
        case 0: snprintf(b, sizeof b, TR("Sneak: %s"), G.set.toggle_sneak ? TR("Toggle") : TR("Hold")); if (ui_button(x, yy, 104, 15, b, true)) G.set.toggle_sneak = !G.set.toggle_sneak; break;
        case 1: snprintf(b, sizeof b, TR("Sprint: %s"), G.set.toggle_sprint ? TR("Toggle") : TR("Hold")); if (ui_button(x, yy, 104, 15, b, true)) G.set.toggle_sprint = !G.set.toggle_sprint; break;
        case 2: snprintf(b, sizeof b, TR("Less Motion: %s"), G.set.reduced_motion ? TR("ON") : TR("OFF")); if (ui_button(x, yy, 104, 15, b, true)) G.set.reduced_motion = !G.set.reduced_motion; break;
        case 3: snprintf(b, sizeof b, TR("Subtitles: %s"), G.set.subtitles ? TR("ON") : TR("OFF")); if (ui_button(x, yy, 104, 15, b, true)) G.set.subtitles = !G.set.subtitles; break;
        case 4: snprintf(b, sizeof b, TR("Colour-blind: %s"), G.set.colorblind ? TR("ON") : TR("OFF")); if (ui_button(x, yy, 104, 15, b, true)) { G.set.colorblind = !G.set.colorblind; settings_apply(); } break;
        case 5: snprintf(b, sizeof b, TR("Contrast: %s"), G.set.high_contrast ? TR("High") : TR("Normal")); if (ui_button(x, yy, 104, 15, b, true)) { G.set.high_contrast = !G.set.high_contrast; settings_apply(); } break;
        case 6: snprintf(b, sizeof b, TR("Narrator: %s"), G.set.narrator ? TR("ON") : TR("OFF")); if (ui_button(x, yy, 104, 15, b, true)) { G.set.narrator = !G.set.narrator; settings_apply(); } break;
        case 7: snprintf(b, sizeof b, TR("Fullscreen: %s"), G.set.fullscreen ? TR("ON") : TR("OFF")); if (ui_button(x, yy, 104, 15, b, true)) { G.set.fullscreen = !G.set.fullscreen; settings_apply(); } break;
        case 8: if (G.set.max_fps) snprintf(b, sizeof b, TR("Max FPS: %d"), G.set.max_fps); else snprintf(b, sizeof b, "%s", TR("Max FPS: Unlimited"));
                if (ui_button(x, yy, 104, 15, b, true)) { int k = 0; while (k < 6 && CAPS[k] != G.set.max_fps) k++; G.set.max_fps = CAPS[(k + 1) % 6]; } break;
        case 9: if (ui_button(x, yy, 104, 15, TR("Reset Keys"), true)) for (int k = 0; k < KB_COUNT; k++) G.set.keys[k] = KB_DEFAULT[k]; break;
        case 10: { static const int SD[] = {4, 6, 8, 10, 12, 16, 24, 32}; snprintf(b, sizeof b, TR("Simulation: %d"), G.set.sim_dist);
                   if (ui_button(x, yy, 104, 15, b, true)) { int k = 0; while (k < 8 && SD[k] != G.set.sim_dist) k++; G.set.sim_dist = SD[(k + 1) % 8]; } } break;
        case 11: if (ui_button(x, yy, 104, 15, TR("Done"), G.rebind < 0)) { settings_save(); game_open_screen(G.controls_back ? G.controls_back : SCR_PAUSE); } break;
        default: break;
        }
    }
}

// advancements and statistics (Game Menu)
static void advancements_screen(void) {
    ui_rect(0, 0, (f32)ui_w, (f32)ui_h, rgba_u32(0, 0, 0, 180));
    f32 cx = ui_w / 2.0f;
    int n = adv_count(), got = 0; for (int i = 0; i < n; i++) got += adv_done(i);
    char t[64]; snprintf(t, sizeof t, "Advancements (%d / %d)", got, n);
    ui_text_center(cx, 6, t, rgba_u32(255, 220, 120, 255), true);
    int rows = MAXI(1, (int)((ui_h - 50) / 11));
    G.adv_scroll = clampi(G.adv_scroll - (int)g_win.scroll, 0, MAXI(0, n - rows));
    for (int k = 0; k < rows && G.adv_scroll + k < n; k++) {
        int i = G.adv_scroll + k; f32 y = 20 + k * 11;
        ui_text(cx - 170, y, adv_done(i) ? "[x]" : "[ ]", adv_done(i) ? rgba_u32(85, 255, 85, 255) : rgba_u32(120, 120, 120, 255), true);
        ui_text(cx - 150, y, adv_title(i), adv_done(i) ? COL_WHITE : rgba_u32(150, 150, 150, 255), true);
        ui_text(cx - 30, y, adv_desc(i), rgba_u32(170, 170, 170, 255), false);
    }
    if (ui_button(cx - 100, ui_h - 26.0f, 200, 20, TR("Done"), true)) game_close_screen();
}
static void stats_screen(void) {
    ui_rect(0, 0, (f32)ui_w, (f32)ui_h, rgba_u32(0, 0, 0, 180));
    f32 cx = ui_w / 2.0f;
    ui_text_center(cx, 6, TR("Statistics"), rgba_u32(255, 220, 120, 255), true);
    for (int i = 0; i < STAT_COUNT; i++) {
        f32 y = 22 + i * 12; char v[32]; snprintf(v, sizeof v, "%lld", g_stats[i]);
        ui_text(cx - 120, y, STAT_NAMES[i], COL_WHITE, true);
        ui_text(cx + 120 - ui_text_width(v), y, v, rgba_u32(255, 255, 140, 255), true);
    }
    if (ui_button(cx - 100, ui_h - 26.0f, 200, 20, TR("Done"), true)) game_close_screen();
}

// command block editor (creative): type a command, Enter or Done saves it
static void command_block_screen(void) {
    ui_rect(0, 0, (f32)ui_w, (f32)ui_h, rgba_u32(0, 0, 0, 160));
    f32 cx = ui_w / 2.0f, y = ui_h / 3.0f;
    ui_text_center(cx, y - 24, TR("Command Block"), COL_WHITE, true);
    int len = (int)strlen(G.cmdblock_text);
    for (int i = 0; i < g_win.nchars; i++) { u32 c = g_win.chars[i]; if (c >= 32 && c < 127 && len < (int)sizeof G.cmdblock_text - 1) { G.cmdblock_text[len++] = (char)c; G.cmdblock_text[len] = 0; }
        else if (c >= 0xA0 && c <= 0xFF && len < (int)sizeof G.cmdblock_text - 2) { G.cmdblock_text[len++] = (char)(0xC0 | (c >> 6)); G.cmdblock_text[len++] = (char)(0x80 | (c & 0x3F)); G.cmdblock_text[len] = 0; } }
    g_win.nchars = 0;
    if (key_pressed(GLFW_KEY_BACKSPACE) && len > 0) { while (len > 1 && ((u8)G.cmdblock_text[len - 1] & 0xC0) == 0x80) len--; G.cmdblock_text[--len] = 0; }
    if (key_pressed(GLFW_KEY_TAB)) command_complete(G.cmdblock_text, sizeof G.cmdblock_text, G.chat_hint, sizeof G.chat_hint);
    ui_rect(cx - 160, y, 320, 14, rgba_u32(0, 0, 0, 255));
    char shown[260]; snprintf(shown, sizeof shown, "%s%s", G.cmdblock_text, ((int)(plat_time_wrap() * 2) % 2) ? "_" : "");
    ui_text(cx - 157, y + 3, shown, COL_WHITE, false);
    if (G.chat_hint[0]) ui_text(cx - 157, y + 18, G.chat_hint, rgba_u32(170, 170, 170, 255), false);
    if (ui_button(cx - 100, y + 40, 200, 20, TR("Done"), true) || key_pressed(GLFW_KEY_ENTER) || key_pressed(GLFW_KEY_KP_ENTER)) { cmdblock_set(G.cmdblock_pos, G.cmdblock_text); G.chat_hint[0] = 0; game_close_screen(); }
}

// the ending: shown once all three Luminaries have blessed the player (and with /credits)
static void credits_screen(void) {
    static const char *L[] = {"U L T I M A T E C R A F T", "", "You have walked the whole road:", "the Overworld, the Underground City, Space and the Sift.", "",
        "Zorvath, the Cosmic Tyrant, is fallen.", "The lamps of the Delvari still burn below.", "Solenne, Zephyrine and Thalor have blessed you.", "",
        "The Sift keeps a place for everyone who builds with care.", "Go home, or stay a while. The world is yours.", "", "", "- Credits -", "",
        "Design and code: the UltimateCraft project", "Built on ideas from open-source voxel engines", "Libraries: GLFW, glad, stb_image, noise1234",
        "Every texture, model, sound and melody is generated by the game itself", "", "", "Thank you for playing."};
    G.credits_t++;
    ui_rect(0, 0, (f32)ui_w, (f32)ui_h, rgba_f(0.02f, 0.03f, 0.08f, 0.94f));
    f32 cx = ui_w / 2.0f, y0 = ui_h + 10 - G.credits_t * 0.4f;
    int n = (int)ARRAY_LEN(L);
    for (int i = 0; i < n; i++) {
        f32 y = y0 + i * 16;
        if (y < -12 || y > ui_h) continue;
        u32 col = i == 0 ? rgba_u32(255, 220, 120, 255) : (L[i][0] == '-' ? rgba_u32(170, 200, 255, 255) : COL_WHITE);
        if (i == 0) ui_text_scaled(cx - ui_text_width(L[i]), y, 2, L[i], col, true); else ui_text_center(cx, y, L[i], col, true);
    }
    if (y0 + n * 16 < -20) G.credits_t = 0;   // loop
    if (ui_button(ui_w - 110.0f, ui_h - 26.0f, 100, 20, TR("Continue"), true)) game_close_screen();
}

static void loading_screen(void) {
    ui_dirt_background(0.25f);
    f32 cx = ui_w / 2.0f;
    ui_text_center(cx, ui_h / 2.0f - 20, TR("Loading world"), COL_WHITE, true);
    ui_text_center(cx, ui_h / 2.0f - 6, TR("Building terrain..."), rgba_u32(170, 170, 170, 255), true);
    int need = (2 * 3 + 1) * (2 * 3 + 1);
    f32 p = clampf((f32)g_world.loaded / need, 0, 1);
    ui_rect(cx - 50, ui_h / 2.0f + 10, 100, 2, rgba_u32(128, 128, 128, 255));
    ui_rect(cx - 50, ui_h / 2.0f + 10, 100 * p, 2, rgba_u32(128, 255, 128, 255));
}

static void chat_screen(void) {
    ui_rect(2, ui_h - 14.0f, ui_w - 4.0f, 12, rgba_u32(0, 0, 0, 128));
    int len = (int)strlen(G.chat);
    for (int i = 0; i < g_win.nchars; i++) {
        u32 c = g_win.chars[i];
        if (c >= 32 && c < 127 && len < (int)sizeof G.chat - 1) { G.chat[len++] = (char)c; G.chat[len] = 0; }
        else if (c >= 0xA0 && c <= 0xFF && len < (int)sizeof G.chat - 2) { G.chat[len++] = (char)(0xC0 | (c >> 6)); G.chat[len++] = (char)(0x80 | (c & 0x3F)); G.chat[len] = 0; }   // accented letters as UTF-8
    }
    if (key_pressed(GLFW_KEY_BACKSPACE) && len > 0) { while (len > 1 && ((u8)G.chat[len - 1] & 0xC0) == 0x80) len--; G.chat[--len] = 0; G.chat_hint[0] = 0; }
    if (key_pressed(GLFW_KEY_TAB) && G.chat[0] == '/') command_complete(G.chat, sizeof G.chat, G.chat_hint, sizeof G.chat_hint);
    if (G.chat_hint[0]) ui_text(4, ui_h - 24.0f, G.chat_hint, rgba_u32(170, 170, 170, 255), true);
    if (key_pressed(GLFW_KEY_UP) && G.chat_hist_n > 0) { G.chat_hist_pos = MAXI(0, G.chat_hist_pos - 1); snprintf(G.chat, sizeof G.chat, "%s", G.chat_hist[G.chat_hist_pos % 16]); }
    if (key_pressed(GLFW_KEY_DOWN) && G.chat_hist_n > 0) { G.chat_hist_pos = MINI(G.chat_hist_n, G.chat_hist_pos + 1); if (G.chat_hist_pos >= G.chat_hist_n) G.chat[0] = 0; else snprintf(G.chat, sizeof G.chat, "%s", G.chat_hist[G.chat_hist_pos % 16]); }
    char shown[160]; snprintf(shown, sizeof shown, "%s%s", G.chat, ((int)(plat_time_wrap() * 2) % 2) ? "_" : "");
    ui_text(4, ui_h - 12.0f, shown, COL_WHITE, true);
    if (key_pressed(GLFW_KEY_ENTER) || key_pressed(GLFW_KEY_KP_ENTER)) {
        if (G.chat[0]) { snprintf(G.chat_hist[G.chat_hist_n % 16], 128, "%s", G.chat); G.chat_hist_n++; G.chat_hist_pos = G.chat_hist_n; }
        if (G.chat[0] == '/') { if (!command_execute_chat(G.chat + 1)) game_message(rgba_u32(255, 85, 85, 255), "Unknown command. Type /help"); }
        else if (G.chat[0]) game_message(COL_WHITE, "<Player> %s", G.chat);
        G.chat[0] = 0;
        game_close_screen();
    }
}

void screens_take_result(bool shift) { take_result(shift); }
ItemStack screens_craft_preview(void) { return current_result(); }
void screens_on_close(Screen s) {
    if (screen_is_container(s)) {
        G.anvil_name[0] = 0; G.trade_with = 0;
        G.cutter_sel = -1;
        for (int i = 0; i < 9; i++) if (!stack_empty(g_player.craft[i])) { player_give(g_player.craft[i]); g_player.craft[i] = (ItemStack){0}; }
        if (!stack_empty(g_player.cursor)) { player_give(g_player.cursor); g_player.cursor = (ItemStack){0}; }
    }
}

void screens_render(void) {
    switch (G.screen) {
    case SCR_TITLE: title_screen(); break;
    case SCR_WORLDS: worlds_screen(); break;
    case SCR_CREATE: create_screen(); break;
    case SCR_CONFIRM_DELETE: confirm_delete_screen(); break;
    case SCR_SETTINGS: settings_screen(); break;
    case SCR_PAUSE: pause_screen(); break;
    case SCR_DEATH: death_screen(); break;
    case SCR_LOADING: loading_screen(); break;
    case SCR_CREDITS: credits_screen(); break;
    case SCR_GUIDE: guide_screen(); break;
    case SCR_COMMAND_BLOCK: command_block_screen(); break;
    case SCR_ADVANCEMENTS: advancements_screen(); break;
    case SCR_STATS: stats_screen(); break;
    case SCR_CONTROLS: controls_screen(); break;
    case SCR_CHAT: chat_screen(); break;
    case SCR_INVENTORY: case SCR_CRAFTING: case SCR_CHEST: case SCR_FURNACE:
    case SCR_STONECUTTER: case SCR_SMITHING: case SCR_BREWING:
    case SCR_ENCHANT: case SCR_ANVIL: case SCR_GRINDSTONE: case SCR_LOOM: case SCR_CARTOGRAPHY: case SCR_TRADE: case SCR_LECTERN: container_screen(); break;
    default: break;
    }
}

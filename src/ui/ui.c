#include "ui.h"
#include "screens.h"
#include "../world/world.h"
#include "../entity/entity.h"
#include "../audio/audio.h"
#include "../core/platform.h"

int ui_scale = 2, ui_w = 640, ui_h = 360;
f32 ui_mx, ui_my;
static Texture tex_font, tex_gui;
static u8 glyph_w[256];

void ui_init(void) {
    int w, h;
    u8 *px = image_load_rgba("res/textures/font.png", &w, &h);
    if (px) {
        for (int c = 0; c < 256; c++) {
            int cx = (c % 16) * 8, cy = (c / 16) * 8, wmax = 0;
            for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) if (px[((cy + y) * w + cx + x) * 4 + 3] > 0 && x + 1 > wmax) wmax = x + 1;
            glyph_w[c] = (u8)wmax;
        }
        tex_font = texture_from_pixels(px, w, h, false);
        free(px);
    } else tex_font = texture_load("res/textures/font.png");
    glyph_w[' '] = 3;
    tex_gui = texture_load("res/textures/gui.png");
}

void ui_begin(int setting) {
    int s = setting > 0 ? setting : MAXI(1, MINI(g_win.width / 320, g_win.height / 240));
    if (setting > 0) s = MINI(s, MAXI(1, MINI(g_win.width / 320, g_win.height / 240)) + 1);
    ui_scale = MAXI(1, s);
    ui_w = g_win.width / ui_scale; ui_h = g_win.height / ui_scale;
    ui_mx = (f32)g_win.mouse_x / ui_scale; ui_my = (f32)g_win.mouse_y / ui_scale;
    glViewport(0, 0, g_win.width, g_win.height);
    b2d_begin(ui_w, ui_h);
}
void ui_end(void) { b2d_flush(); }

// UTF-8 -> font glyph (Latin-1). Two-byte sequences for U+00A0..U+00FF map to their glyphs, longer sequences show
// '?', and stray bytes are used as they are (so raw Latin-1 and the \xa7 colour code keep working).
static u8 next_glyph(const char **pp) {
    const u8 *p = (const u8 *)*pp;
    u8 c = p[0];
    if (c < 0x80) { *pp += 1; return c; }
    if ((c == 0xC2 || c == 0xC3) && (p[1] & 0xC0) == 0x80) { *pp += 2; return (u8)(c == 0xC2 ? p[1] : p[1] + 0x40); }
    int n = (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
    bool ok = n > 1; for (int i = 1; i < n && ok; i++) ok = (p[i] & 0xC0) == 0x80;
    if (ok) { *pp += n; return '?'; }
    *pp += 1; return c;
}
int ui_text_width(const char *s) {
    int w = 0;
    while (*s) {
        if (*s == '\xa7' && s[1]) { s += 2; continue; }   // colour codes
        w += glyph_w[next_glyph(&s)] + 1;
    }
    return w > 0 ? w - 1 : 0;
}

void ui_text_scaled(f32 x, f32 y, f32 sc, const char *s, u32 color, bool shadow) {
    color = ui_cb_color(color);
    if (g_ui_contrast && shadow && s && *s) b2d_rect(x - 1, y - 1, ui_text_width(s) * sc + 2, 8 * sc + 2, rgba_u32(0, 0, 0, 190));   // high contrast: solid backing
    b2d_set_texture(tex_font.id);
    for (int pass = shadow ? 0 : 1; pass < 2; pass++) {
        f32 cx = x;
        u32 c = color;
        if (pass == 0) c = rgba_u32((int)((color & 255) * 0.25f), (int)(((color >> 8) & 255) * 0.25f), (int)(((color >> 16) & 255) * 0.25f), (int)(color >> 24));
        f32 off = pass == 0 ? sc : 0;
        for (const char *p = s; *p;) {
            u8 ch = next_glyph(&p);
            if (ch < 32 && ch != 1) continue;
            f32 u0 = (ch % 16) * 8 / 128.0f, v0 = (ch / 16) * 8 / 128.0f;
            int gw = glyph_w[ch];
            if (ch != ' ')
                b2d_quad(cx + off, y + off, gw * sc, 8 * sc, u0, v0, u0 + gw / 128.0f, v0 + 8 / 128.0f, c);
            cx += (gw + 1) * sc;
        }
    }
}
bool g_ui_colorblind, g_ui_contrast;
// colour-blind friendly palette: strong reds become orange, strong greens become blue (safe for red-green deficiencies)
u32 ui_cb_color(u32 c) {
    if (!g_ui_colorblind) return c;
    int r = c & 255, g = (c >> 8) & 255, b = (c >> 16) & 255, a = (c >> 24) & 255;
    if (r > 150 && g < 130 && b < 130) return rgba_u32(255, 170, 0, a);
    if (g > 150 && r < 140 && b < 160) return rgba_u32(70, 160, 255, a);
    return c;
}
void ui_text(f32 x, f32 y, const char *s, u32 c, bool sh) { ui_text_scaled(x, y, 1, s, c, sh); }
void ui_text_center(f32 cx, f32 y, const char *s, u32 c, bool sh) { ui_text(cx - ui_text_width(s) / 2.0f, y, s, c, sh); }
void ui_rect(f32 x, f32 y, f32 w, f32 h, u32 c) { b2d_rect(x, y, w, h, ui_cb_color(c)); }

void ui_panel(f32 x, f32 y, f32 w, f32 h) {
    ui_rect(x, y, w, h, rgba_u32(0, 0, 0, 255));
    ui_rect(x + 1, y + 1, w - 2, h - 2, rgba_u32(255, 255, 255, 255));
    ui_rect(x + 3, y + 3, w - 4, h - 4, rgba_u32(85, 85, 85, 255));
    ui_rect(x + 3, y + 3, w - 6, h - 6, rgba_u32(198, 198, 198, 255));
}
void ui_slot(f32 x, f32 y) {
    ui_rect(x, y, 18, 18, rgba_u32(55, 55, 55, 255));
    ui_rect(x + 1, y + 1, 17, 17, rgba_u32(255, 255, 255, 255));
    ui_rect(x + 1, y + 1, 16, 16, rgba_u32(139, 139, 139, 255));
}
void ui_icon(int cell, f32 x, f32 y, u32 color) {
    b2d_set_texture(tex_gui.id);
    f32 u0 = (cell % 16) * 16 / 256.0f, v0 = (cell / 16) * 16 / 256.0f;
    b2d_quad(x, y, 9, 9, u0, v0, u0 + 9 / 256.0f, v0 + 9 / 256.0f, color);
}

u32 block_default_tint(const BlockDef *d, int face);
static u32 tinted(u32 tint, f32 shade) {
    return rgba_u32((int)(((tint >> 16) & 255) * shade), (int)(((tint >> 8) & 255) * shade), (int)((tint & 255) * shade), 255);
}
// isometric projection of a point given in 1/16 block units
static void iso(f32 x0, f32 y0, f32 px, f32 py, f32 pz, f32 *sx, f32 *sy) {
    px /= 16; py /= 16; pz /= 16;
    *sx = x0 + 8 + (px - pz) * 6.5f;
    *sy = y0 + 1.5f + (px + pz) * 3.5f + (1 - py) * 7.0f;
}
static void iso_box(f32 x, f32 y, const BlockDef *d, const f32 b[6]) {
    for (int f = 0; f < 3; f++) {
        int face = f == 0 ? FACE_U : f == 1 ? FACE_S : FACE_E;
        f32 P[4][3];
        if (face == FACE_U) { f32 q[4][3] = {{b[0], b[4], b[5]}, {b[3], b[4], b[5]}, {b[3], b[4], b[2]}, {b[0], b[4], b[2]}}; memcpy(P, q, sizeof q); }
        else if (face == FACE_S) { f32 q[4][3] = {{b[0], b[4], b[5]}, {b[0], b[1], b[5]}, {b[3], b[1], b[5]}, {b[3], b[4], b[5]}}; memcpy(P, q, sizeof q); }
        else { f32 q[4][3] = {{b[3], b[4], b[5]}, {b[3], b[1], b[5]}, {b[3], b[1], b[2]}, {b[3], b[4], b[2]}}; memcpy(P, q, sizeof q); }
        int t = d->tex[face];
        f32 u0 = (t % ATLAS_TILES) / (f32)ATLAS_TILES, v0 = (t / ATLAS_TILES) / (f32)ATLAS_TILES, du = 1.0f / ATLAS_TILES;
        f32 xy[8], uv[8];
        for (int i = 0; i < 4; i++) {
            iso(x, y, P[i][0], P[i][1], P[i][2], &xy[i * 2], &xy[i * 2 + 1]);
            f32 tu, tv;
            if (face == FACE_U) { tu = P[i][0]; tv = P[i][2]; }
            else if (face == FACE_S) { tu = P[i][0]; tv = 16 - P[i][1]; }
            else { tu = 16 - P[i][2]; tv = 16 - P[i][1]; }
            uv[i * 2] = u0 + clampf(tu / 16, 0.01f, 0.99f) * du; uv[i * 2 + 1] = v0 + clampf(tv / 16, 0.01f, 0.99f) * du;
        }
        b2d_quad4(xy, uv, tinted(block_default_tint(d, face), face == FACE_U ? 1.0f : face == FACE_S ? 0.8f : 0.6f));
    }
}
static int cmp_box(const void *a, const void *b) {
    const f32 *p = a, *q = b;
    f32 da = (p[0] + p[3]) + (p[2] + p[5]) + (p[1] + p[4]) * 0.01f, db = (q[0] + q[3]) + (q[2] + q[5]) + (q[1] + q[4]) * 0.01f;
    return da < db ? -1 : da > db;
}
static void block_icon(f32 x, f32 y, u16 b) {
    const BlockDef *d = &BLOCKS[b];
    b2d_set_texture(g_world.atlas.id);
    if (d->mesh == MESH_CROSS || d->mesh == MESH_TORCH || d->mesh == MESH_CROP || d->mesh == MESH_FLAT || d->mesh == MESH_WIRE || d->mesh == MESH_RAIL || d->mesh == MESH_LIQUID || d->mesh == MESH_NONE) {
        int t = d->tex[d->mesh == MESH_FLAT || d->mesh == MESH_RAIL ? FACE_U : FACE_S];
        f32 u0 = (t % ATLAS_TILES) / (f32)ATLAS_TILES, v0 = (t / ATLAS_TILES) / (f32)ATLAS_TILES, du = 1.0f / ATLAS_TILES;
        b2d_quad(x, y, 16, 16, u0, v0, u0 + du, v0 + du, tinted(block_default_tint(d, FACE_U), 1));
        return;
    }
    f32 boxes[12][6]; int n = 0;
    if (d->mesh == MESH_BOXES) {
        const BlockShape *s = &SHAPES[d->shape];
        for (int i = 0; i < s->n && n < 12; i++, n++) for (int k = 0; k < 6; k++) boxes[n][k] = s->b[i][k];
    } else if (d->mesh == MESH_CONNECT) {
        const BlockShape *s = &SHAPES[d->shape];
        for (int k = 0; k < 6; k++) boxes[n][k] = s->b[0][k];
        n++;
        for (int a = 1; a < s->n; a++) for (int side = 1; side <= 3; side += 2) {
            const u8 *q = s->b[a];
            f32 x0 = q[0], z0 = q[2], x1 = q[3], z1 = q[5];
            for (int r = 0; r < side; r++) { f32 nx0 = 16 - z1, nz0 = x0, nx1 = 16 - z0, nz1 = x1; x0 = nx0; z0 = nz0; x1 = nx1; z1 = nz1; }
            f32 bb[6] = {fminf(x0, x1), q[1], fminf(z0, z1), fmaxf(x0, x1), q[4], fmaxf(z0, z1)};
            if (n < 12) { memcpy(boxes[n], bb, sizeof bb); n++; }
        }
    } else if (d->mesh == MESH_CACTUS) { f32 bb[6] = {1, 0, 1, 15, 16, 15}; memcpy(boxes[0], bb, sizeof bb); n = 1; }
    else { f32 bb[6] = {0, 0, 0, 16, 16, 16}; memcpy(boxes[0], bb, sizeof bb); n = 1; }
    for (int i = 0; i < n; i++) { if (boxes[i][4] <= boxes[i][1]) boxes[i][4] = boxes[i][1] + 0.5f; if (boxes[i][3] <= boxes[i][0]) boxes[i][3] = boxes[i][0] + 0.5f; if (boxes[i][5] <= boxes[i][2]) boxes[i][5] = boxes[i][2] + 0.5f; }
    qsort(boxes, n, sizeof boxes[0], cmp_box);
    for (int i = 0; i < n; i++) iso_box(x, y, d, boxes[i]);
}

void ui_item(f32 x, f32 y, ItemStack s, bool show_count) {
    if (stack_empty(s) || !item_valid(s.id)) return;
    const ItemDef *it = &ITEMS[s.id];
    if (it->icon == -1) block_icon(x, y, it->block);
    else if (it->icon == -2) {
        const BlockDef *d = &BLOCKS[it->block];
        int t = d->tex[d->mesh == MESH_FLAT || d->mesh == MESH_RAIL ? FACE_U : FACE_S];
        f32 u0 = (t % ATLAS_TILES) / (f32)ATLAS_TILES, v0 = (t / ATLAS_TILES) / (f32)ATLAS_TILES, du = 1.0f / ATLAS_TILES;
        b2d_set_texture(g_world.atlas.id);
        b2d_quad(x, y, 16, 16, u0, v0, u0 + du, v0 + du, tinted(block_default_tint(d, FACE_U), 1));
    } else {
        int ic = it->icon + (it->variants ? s.damage % it->variants : 0);
        b2d_set_texture(entity_items_texture());
        f32 u0 = (ic % 64) / 64.0f, v0 = (ic / 64) / 64.0f;
        b2d_quad(x, y, 16, 16, u0, v0, u0 + 1 / 64.0f, v0 + 1 / 64.0f, COL_WHITE);
    }
    if (item_is_banner(s.id) && s.ench[0][0]) banner_draw_gui(x + 4, y + 1, 8, 14, banner_base_color(s.id), (const u8 (*)[2])s.ench, MAX_ENCH);
    if ((ench_count(s) && !strstr(it->key, "_banner")) || s.id == I_ENCHANTED_GOLDEN_APPLE || s.id == I_NETHER_STAR || s.id == I_EXPERIENCE_BOTTLE) {
        // enchantment glint: a moving purple sheen over the icon
        b2d_set_texture(gfx_white_texture());
        f32 t = (f32)fmod(plat_time_wrap() * 0.8, 2.0);
        for (int i = 0; i < 16; i++) {
            f32 ph = fabsf((i / 16.0f) - (t - 0.5f));
            f32 a = fmaxf(0, 0.45f - ph * 1.2f);
            if (a > 0.01f) b2d_quad(x + i, y, 1, 16, 0, 0, 1, 1, rgba_f(0.75f, 0.35f, 1.0f, a));
        }
        b2d_rect(x, y, 16, 16, rgba_f(0.55f, 0.25f, 0.9f, 0.12f));
    }
    if (it->durability && s.damage > 0 && !it->variants) {
        f32 f = 1.0f - (f32)s.damage / it->durability;
        ui_rect(x + 2, y + 13, 13, 2, rgba_u32(0, 0, 0, 255));
        ui_rect(x + 2, y + 13, 12 * f + 1, 1, rgba_f(1 - f, f, 0, 1));
    }
    if (show_count && s.count > 1) {
        char buf[8]; snprintf(buf, sizeof buf, "%d", s.count);
        ui_text(x + 17 - ui_text_width(buf), y + 9, buf, COL_WHITE, true);
    }
}

void ui_tooltip_lines(const char *title, u32 title_col, const char lines[][64], int n, const char *sub) {
    int w = ui_text_width(title);
    for (int i = 0; i < n; i++) w = MAXI(w, ui_text_width(lines[i]));
    if (sub) w = MAXI(w, ui_text_width(sub));
    f32 W = w + 8.0f, H = 12.0f + 10.0f * (n + (sub ? 1 : 0));
    f32 x = ui_mx + 10, y = ui_my - 12;
    if (x + W > ui_w) x = ui_w - W;
    if (y + H > ui_h) y = ui_h - H;
    if (y < 0) y = 0;
    ui_rect(x, y, W, H, rgba_u32(16, 0, 16, 240));
    ui_rect(x + 1, y + 1, W - 2, H - 2, rgba_u32(40, 0, 127, 255));
    ui_rect(x + 2, y + 2, W - 4, H - 4, rgba_u32(16, 0, 16, 245));
    ui_text(x + 4, y + 2, title, title_col, true);
    for (int i = 0; i < n; i++) {
        bool curse = strstr(lines[i], "Curse") != NULL;
        ui_text(x + 4, y + 12 + i * 10, lines[i], curse ? rgba_u32(255, 85, 85, 255) : rgba_u32(170, 170, 170, 255), true);
    }
    if (sub) ui_text(x + 4, y + 12 + n * 10, sub, rgba_u32(120, 120, 140, 255), true);
}
void ui_tooltip(const char *text, const char *sub) {
    f32 w = (f32)MAXI(ui_text_width(text), sub ? ui_text_width(sub) : 0) + 8, h = sub ? 22.0f : 12.0f;
    f32 x = ui_mx + 10, y = ui_my - 12;
    if (x + w > ui_w) x = ui_w - w;
    if (y < 0) y = 0;
    ui_rect(x, y, w, h, rgba_u32(16, 0, 16, 240));
    ui_rect(x + 1, y + 1, w - 2, h - 2, rgba_u32(40, 0, 127, 255));
    ui_rect(x + 2, y + 2, w - 4, h - 4, rgba_u32(16, 0, 16, 245));
    ui_text(x + 4, y + 2, text, COL_WHITE, true);
    if (sub) ui_text(x + 4, y + 12, sub, rgba_u32(170, 170, 170, 255), true);
}

void ui_dirt_background(f32 darken) {
    b2d_set_texture(g_world.atlas.id);
    int t = TILE(2, 0);
    f32 u0 = (t % 16) / 16.0f, v0 = (t / 16) / 16.0f;
    f32 off = (f32)fmod(plat_time_wrap(), 32.0);
    u8 c = (u8)(255 * darken);
    for (f32 y = -32 + off; y < ui_h; y += 32) for (f32 x = 0; x < ui_w; x += 32)
        b2d_quad(x, y, 32, 32, u0, v0, u0 + 1 / 16.0f, v0 + 1 / 16.0f, rgba_u32(c, c, c, 255));
}

bool ui_hover(f32 x, f32 y, f32 w, f32 h) { return ui_mx >= x && ui_mx < x + w && ui_my >= y && ui_my < y + h; }

static void button_frame(f32 x, f32 y, f32 w, f32 h, bool hover, bool enabled) {
    u32 face = !enabled ? rgba_u32(60, 60, 60, 255) : hover ? rgba_u32(126, 136, 191, 255) : rgba_u32(111, 111, 111, 255);
    ui_rect(x, y, w, h, hover && enabled ? rgba_u32(255, 255, 255, 255) : rgba_u32(0, 0, 0, 255));
    ui_rect(x + 1, y + 1, w - 2, h - 2, face);
    ui_rect(x + 1, y + 1, w - 2, 1, rgba_u32(255, 255, 255, enabled ? 90 : 30));
    ui_rect(x + 1, y + h - 3, w - 2, 2, rgba_u32(0, 0, 0, 70));
}

bool ui_button(f32 x, f32 y, f32 w, f32 h, const char *label, bool enabled) {
    bool hov = ui_hover(x, y, w, h);
    button_frame(x, y, w, h, hov, enabled);
    ui_text_center(x + w / 2, y + (h - 8) / 2, label, !enabled ? rgba_u32(160, 160, 160, 255) : hov ? rgba_u32(255, 255, 160, 255) : COL_WHITE, true);
    if (enabled && hov && g_win.mouse_pressed[GLFW_MOUSE_BUTTON_LEFT]) {
        g_win.mouse_pressed[GLFW_MOUSE_BUTTON_LEFT] = false;
        sound_play(SND_CLICK, 0.5f, 1.0f);
        return true;
    }
    return false;
}

bool ui_slider(f32 x, f32 y, f32 w, const char *label, f32 *v, f32 lo, f32 hi, f32 step) {
    f32 h = 20;
    bool hov = ui_hover(x, y, w, h);
    ui_rect(x, y, w, h, rgba_u32(0, 0, 0, 255));
    ui_rect(x + 1, y + 1, w - 2, h - 2, rgba_u32(50, 50, 50, 255));
    f32 t = (*v - lo) / (hi - lo);
    f32 kx = x + t * (w - 8);
    button_frame(kx, y, 8, h, hov, true);
    ui_text_center(x + w / 2, y + 6, label, COL_WHITE, true);
    static f32 *dragging;
    if (hov && g_win.mouse_pressed[GLFW_MOUSE_BUTTON_LEFT]) { dragging = v; g_win.mouse_pressed[GLFW_MOUSE_BUTTON_LEFT] = false; }
    if (!g_win.mouse_down[GLFW_MOUSE_BUTTON_LEFT]) dragging = NULL;
    if (dragging == v) {
        f32 nt = clampf((ui_mx - x - 4) / (w - 8), 0, 1);
        f32 nv = lo + nt * (hi - lo);
        if (step > 0) nv = roundf(nv / step) * step;
        nv = clampf(nv, lo, hi);
        if (nv != *v) { *v = nv; return true; }
    }
    return false;
}

bool ui_textfield(f32 x, f32 y, f32 w, char *buf, int cap, bool focused) {
    f32 h = 20;
    ui_rect(x, y, w, h, focused ? COL_WHITE : rgba_u32(160, 160, 160, 255));
    ui_rect(x + 1, y + 1, w - 2, h - 2, rgba_u32(0, 0, 0, 255));
    if (focused) {
        int len = (int)strlen(buf);
        for (int i = 0; i < g_win.nchars; i++) {
            u32 c = g_win.chars[i];
            if (c >= 32 && c < 127 && len < cap - 1) { buf[len++] = (char)c; buf[len] = 0; }
        }
        if (key_pressed(GLFW_KEY_BACKSPACE) && len > 0) buf[--len] = 0;
    }
    char shown[160];
    snprintf(shown, sizeof shown, "%s%s", buf, focused && ((int)(plat_time_wrap() * 2) % 2) ? "_" : "");
    ui_text(x + 4, y + 6, shown, rgba_u32(224, 224, 224, 255), true);
    bool clicked = ui_hover(x, y, w, h) && g_win.mouse_pressed[GLFW_MOUSE_BUTTON_LEFT];
    return clicked;
}

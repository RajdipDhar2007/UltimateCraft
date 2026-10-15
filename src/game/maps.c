// Maps: an empty map becomes a filled map (item data = map id) centred on a 128 * 2^scale grid
// cell. While held, the area around the player is drawn into the map from the terrain (block top
// colours sampled from the atlas, shaded by height like Minecraft). Maps are saved per world in
// saves/<world>/maps/map_<id>.dat and shown in the HUD when held in the main hand.
#include "game.h"
#include "player.h"
#include "maps.h"
#include "../core/platform.h"
#include "../ui/ui.h"

#define MAP_N 128
typedef struct {
    bool used, dirty, loaded_tex;
    int cx, cz, scale, dim;
    bool locked;
    u16 px[MAP_N * MAP_N];   // RGB565, 0 = unexplored
    GLuint tex;
} MapData;

#define MAX_MAPS 512
static MapData *maps[MAX_MAPS];
static int nmaps;              // next id (ids start at 1)
static u32 block_rgb[BLOCK_STATES];
static bool colors_ready;

static void compute_block_colors(void) {
    int w, h;
    u8 *px = image_load_rgba("res/textures/blocks.png", &w, &h);
    colors_ready = true;
    if (!px) return;
    int tsz = w / ATLAS_TILES;
    for (int b = 1; b < BLOCK_STATES; b++) {
        const BlockDef *d = &BLOCKS[b];
        if (!d->name) continue;
        int t = d->tex[FACE_U];
        int tx = (t % ATLAS_TILES) * tsz, ty = (t / ATLAS_TILES) * tsz;
        u32 r = 0, g = 0, bl = 0, n = 0;
        for (int y = 0; y < tsz; y++) for (int x = 0; x < tsz; x++) {
            const u8 *p = px + ((ty + y) * w + tx + x) * 4;
            if (p[3] < 100) continue;
            r += p[0]; g += p[1]; bl += p[2]; n++;
        }
        if (n) block_rgb[b] = ((r / n) << 16) | ((g / n) << 8) | (bl / n);
    }
    free(px);
}
static inline u16 to565(u32 c) { return (u16)((((c >> 19) & 31) << 11) | (((c >> 10) & 63) << 5) | ((c >> 3) & 31)); }
static u32 mul_rgb(u32 a, u32 t) {
    u32 r = ((a >> 16) & 255) * ((t >> 16) & 255) / 200, g = ((a >> 8) & 255) * ((t >> 8) & 255) / 200, b = (a & 255) * (t & 255) / 200;
    return (MINI(255, r) << 16) | (MINI(255, g) << 8) | MINI(255, b);
}
static u32 shade(u32 c, f32 f) {
    return ((u32)MINI(255, (int)(((c >> 16) & 255) * f)) << 16) | ((u32)MINI(255, (int)(((c >> 8) & 255) * f)) << 8) | (u32)MINI(255, (int)((c & 255) * f));
}

static MapData *map_get(int id) {
    if (id <= 0 || id >= MAX_MAPS) return NULL;
    if (maps[id]) return maps[id];
    // try loading from disk
    char path[320]; snprintf(path, sizeof path, "saves/%s/maps/map_%d.dat", g_world.name, id);
    size_t len; u8 *data = (u8 *)plat_read_file(path, &len);
    if (!data) return NULL;
    MapData *m = xcalloc(1, sizeof *m);
    if (len >= 24 + sizeof m->px) {
        s32 hdr[6]; memcpy(hdr, data, 24);
        m->cx = hdr[0]; m->cz = hdr[1]; m->scale = hdr[2]; m->dim = hdr[3]; m->locked = hdr[4] != 0;
        memcpy(m->px, data + 24, sizeof m->px);
    }
    free(data);
    m->used = true; m->dirty = true;
    maps[id] = m;
    if (id >= nmaps) nmaps = id + 1;
    return m;
}
static void map_save(int id) {
    MapData *m = maps[id];
    if (!m) return;
    char dir[300]; snprintf(dir, sizeof dir, "saves/%s/maps", g_world.name);
    plat_mkdirs(dir);
    char path[320]; snprintf(path, sizeof path, "%s/map_%d.dat", dir, id);
    u8 *buf = xmalloc(24 + sizeof m->px);
    s32 hdr[6] = {m->cx, m->cz, m->scale, m->dim, m->locked, 0};
    memcpy(buf, hdr, 24); memcpy(buf + 24, m->px, sizeof m->px);
    plat_write_file(path, buf, 24 + sizeof m->px);
    free(buf);
}
void maps_save_all(void) { for (int i = 1; i < MAX_MAPS; i++) if (maps[i]) map_save(i); }
void maps_reset(void) {
    for (int i = 0; i < MAX_MAPS; i++) if (maps[i]) { if (maps[i]->tex) glDeleteTextures(1, &maps[i]->tex); free(maps[i]); maps[i] = NULL; }
    nmaps = 1;
    // find the highest id already on disk
    char dir[300]; snprintf(dir, sizeof dir, "saves/%s/maps/map_1.dat", g_world.name);
    for (int i = 1; i < MAX_MAPS; i++) {
        char path[320]; snprintf(path, sizeof path, "saves/%s/maps/map_%d.dat", g_world.name, i);
        if (plat_exists(path)) nmaps = i + 1;
    }
    (void)dir;
}
static int map_new(int x, int z, int scale, int dim) {
    if (nmaps < 1) nmaps = 1;
    if (nmaps >= MAX_MAPS) return 0;
    int id = nmaps++;
    MapData *m = xcalloc(1, sizeof *m);
    int size = MAP_N << scale;
    m->cx = (int)floorf((x + 64.0f) / size) * size + size / 2 - 64;
    m->cz = (int)floorf((z + 64.0f) / size) * size + size / 2 - 64;
    m->scale = scale; m->dim = dim; m->used = true; m->dirty = true;
    maps[id] = m;
    return id;
}
int map_create_at(vec3 pos) { return map_new((int)floorf(pos.x), (int)floorf(pos.z), 0, g_world.dim); }
int map_copy(int id, bool locked) {
    MapData *src = map_get(id);
    if (!src || nmaps >= MAX_MAPS) return 0;
    int nid = nmaps++;
    MapData *m = xcalloc(1, sizeof *m);
    *m = *src; m->tex = 0; m->loaded_tex = false; m->dirty = true; m->locked = locked;
    maps[nid] = m;
    return nid;
}
int map_zoom_out(int id) {
    MapData *src = map_get(id);
    if (!src || src->scale >= 4) return 0;
    int nid = map_new(src->cx, src->cz, src->scale + 1, src->dim);
    MapData *m = maps[nid];
    if (!m) return 0;
    // keep what was explored, scaled down into the new map
    int size_old = MAP_N << src->scale, size_new = MAP_N << m->scale;
    for (int y = 0; y < MAP_N; y++) for (int x = 0; x < MAP_N; x++) {
        int wx = src->cx - size_old / 2 + (x << src->scale), wz = src->cz - size_old / 2 + (y << src->scale);
        int nx = (wx - (m->cx - size_new / 2)) >> m->scale, nz = (wz - (m->cz - size_new / 2)) >> m->scale;
        if (nx >= 0 && nx < MAP_N && nz >= 0 && nz < MAP_N && src->px[y * MAP_N + x]) m->px[nz * MAP_N + nx] = src->px[y * MAP_N + x];
    }
    return nid;
}
int map_scale(int id) { MapData *m = map_get(id); return m ? m->scale : 0; }
bool map_locked(int id) { MapData *m = map_get(id); return m && m->locked; }

static u32 column_color(int wx, int wz, int *height) {
    int top = world_top_y(wx, wz);
    *height = top;
    // include non-solid top things (water, plants on grass count as grass)
    int y = WORLD_H - 1;
    if (g_world.dim == DIM_NETHER) y = top + 1;
    Block b = B_AIR;
    for (; y >= 0; y--) {
        b = world_get_block(wx, y, wz);
        if (block_is_air(b)) continue;
        const BlockDef *d = &BLOCKS[b];
        if (d->mesh == MESH_CROSS || d->mesh == MESH_NONE || (d->mesh == MESH_BOXES && !d->solid && !d->liquid)) continue;
        break;
    }
    if (y < 0) return 0;
    *height = y;
    const BlockDef *d = &BLOCKS[b];
    u32 c = block_rgb[b];
    int biome = world_biome_at(wx, y, wz);
    if (d->tint == TINT_GRASS) c = mul_rgb(c, BIOMES[biome].grass);
    else if (d->tint == TINT_FOLIAGE) c = mul_rgb(c, BIOMES[biome].foliage);
    else if (d->tint == TINT_WATER || block_is_water(b)) {
        int depth = 0;
        while (depth < 10 && block_is_water(world_get_block(wx, y - depth - 1, wz))) depth++;
        c = shade(BIOMES[biome].water ? BIOMES[biome].water : 0x3F76E4, 1.05f - depth * 0.05f);
    }
    return c ? c : 0x404040;
}

void maps_tick(void) {
    Entity *e = player_ent();
    if (!e) return;
    if (!colors_ready) compute_block_colors();
    ItemStack h[2] = {*inventory_held(&g_player.inv), g_player.inv.offhand};
    for (int k = 0; k < 2; k++) {
        if (h[k].id != I_FILLED_MAP) continue;
        MapData *m = map_get(h[k].data);
        if (!m || m->locked || m->dim != g_world.dim) continue;
        int size = MAP_N << m->scale;
        int px = ((int)floorf(e->pos.x) - (m->cx - size / 2)) >> m->scale;
        int pz = ((int)floorf(e->pos.z) - (m->cz - size / 2)) >> m->scale;
        int r = 64 >> m->scale;
        // update one row band per tick around the player (spreads the cost)
        static int band;
        band = (band + 1) % 8;
        for (int y = pz - r + band; y <= pz + r; y += 8) {
            if (y < 0 || y >= MAP_N) continue;
            for (int x = px - r; x <= px + r; x++) {
                if (x < 0 || x >= MAP_N || (x - px) * (x - px) + (y - pz) * (y - pz) > r * r) continue;
                int wx = m->cx - size / 2 + (x << m->scale), wz = m->cz - size / 2 + (y << m->scale);
                if (!world_loaded(wx, wz) || !world_loaded(wx, wz - (1 << m->scale))) continue;
                int hgt, hn;
                u32 c = column_color(wx, wz, &hgt);
                column_color(wx, wz - (1 << m->scale), &hn);
                if (!c) continue;
                f32 f = hgt > hn ? 1.1f : hgt < hn ? 0.8f : 0.95f;
                if (block_is_water(world_get_block(wx, hgt, wz))) f = ((x + y) & 1) ? 0.95f : 1.0f;
                u16 v = to565(shade(c, f));
                if (!v) v = 1;
                if (m->px[y * MAP_N + x] != v) { m->px[y * MAP_N + x] = v; m->dirty = true; }
            }
        }
    }
}

// explored fraction (for tests)
f32 map_explored(int id) {
    MapData *m = map_get(id);
    if (!m) return 0;
    int n = 0;
    for (int i = 0; i < MAP_N * MAP_N; i++) n += m->px[i] != 0;
    return (f32)n / (MAP_N * MAP_N);
}

static void upload(MapData *m) {
    static u8 rgba[MAP_N * MAP_N * 4];
    for (int i = 0; i < MAP_N * MAP_N; i++) {
        u16 v = m->px[i];
        u8 *o = rgba + i * 4;
        if (!v) { o[0] = 214; o[1] = 190; o[2] = 150; o[3] = 255; continue; }   // parchment
        u8 r = (u8)(((v >> 11) & 31) * 255 / 31), g = (u8)(((v >> 5) & 63) * 255 / 63), b = (u8)((v & 31) * 255 / 31);
        // blend toward parchment for the map look
        o[0] = (u8)(r * 0.8f + 214 * 0.2f); o[1] = (u8)(g * 0.8f + 190 * 0.2f); o[2] = (u8)(b * 0.8f + 150 * 0.2f); o[3] = 255;
    }
    if (!m->tex) glGenTextures(1, &m->tex);
    glBindTexture(GL_TEXTURE_2D, m->tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, MAP_N, MAP_N, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    m->dirty = false;
}

// draw map `id` into a GUI rectangle; markers for the player when inside
void map_draw(int id, f32 x, f32 y, f32 size, bool marker) {
    MapData *m = map_get(id);
    ui_rect(x - 4, y - 4, size + 8, size + 8, rgba_u32(120, 90, 50, 255));
    ui_rect(x - 2, y - 2, size + 4, size + 4, rgba_u32(214, 190, 150, 255));
    if (!m) { ui_text_center(x + size / 2, y + size / 2 - 4, "?", COL_BLACK, false); return; }
    if (m->dirty || !m->tex) { b2d_flush(); upload(m); }
    b2d_set_texture(m->tex);
    b2d_quad(x, y, size, size, 0, 0, 1, 1, COL_WHITE);
    Entity *e = player_ent();
    if (marker && e && m->dim == g_world.dim) {
        int span = MAP_N << m->scale;
        f32 fx = (e->pos.x - (m->cx - span / 2)) / span, fz = (e->pos.z - (m->cz - span / 2)) / span;
        bool inside = fx >= 0 && fx <= 1 && fz >= 0 && fz <= 1;
        fx = clampf(fx, 0, 1); fz = clampf(fz, 0, 1);
        f32 cx = x + fx * size, cy = y + fz * size;
        // arrow pointing where the player looks (yaw 0 = +Z = down on the map)
        f32 dx = sinf(e->yaw), dz = cosf(e->yaw), s = inside ? 4 : 2.5f;
        f32 xy[8] = {cx + dx * s * 1.6f, cy + dz * s * 1.6f, cx - dz * s, cy + dx * s, cx - dx * s * 0.6f, cy - dz * s * 0.6f, cx + dz * s, cy - dx * s};
        f32 uv[8] = {0, 0, 1, 0, 1, 1, 0, 1};
        b2d_set_texture(gfx_white_texture());
        b2d_quad4(xy, uv, inside ? rgba_u32(255, 255, 255, 255) : rgba_u32(200, 200, 200, 255));
        b2d_rect(cx - 1, cy - 1, 2, 2, rgba_u32(40, 40, 40, 255));
    }
    if (m->locked) ui_text(x + 2, y + size - 9, "Locked", rgba_u32(80, 60, 30, 255), false);
}

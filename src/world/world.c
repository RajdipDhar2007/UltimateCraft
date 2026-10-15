#include "world.h"
bool forceload_has(int cx, int cz);
#include "rules.h"
#include "structures.h"
#include "../game/player.h"

World g_world;
int biome_by_key(const char *key) {
    if (!key) return -1;
    if (!strncmp(key, "minecraft:", 10)) key += 10;
    for (int i = 0; i < BIOME_COUNT; i++) if (!strcmp(BIOMES[i].key, key)) return i;
    return -1;
}
bool world_has_skylight(void) { return dim_info(g_world.dim)->skylight; }

// spiral of column offsets sorted by distance (for load order and mesh priority)
static struct { s8 dx, dz; } spiral[(2 * MAX_RENDER_DIST + 5) * (2 * MAX_RENDER_DIST + 5)];
static int spiral_n;

static int cmp_spiral(const void *a, const void *b) {
    const s8 *p = a, *q = b;
    return (p[0] * p[0] + p[1] * p[1]) - (q[0] * q[0] + q[1] * q[1]);
}

void world_gfx_init(void) {
    g_world.chunk_shader = shader_load("res/shaders/chunk.vs", "res/shaders/chunk.fs");
    int w, h;
    u8 *px = image_load_rgba("res/textures/blocks.png", &w, &h);
    if (px) { g_world.atlas = texture_from_pixels(px, w, h, true); free(px); }
    else g_world.atlas = texture_load("res/textures/blocks.png");
    int r = MAX_RENDER_DIST + 2;
    for (int dz = -r; dz <= r; dz++) for (int dx = -r; dx <= r; dx++) { spiral[spiral_n].dx = (s8)dx; spiral[spiral_n].dz = (s8)dz; spiral_n++; }
    qsort(spiral, spiral_n, sizeof spiral[0], cmp_spiral);
    mesher_init();
}

// ======================================================================== worker threads
static void *worker_main(void *arg) {
    (void)arg;
    while (!g_world.quit) {
        mutex_lock(&g_world.lock);
        Column *c = g_world.job_head;
        if (c) { g_world.job_head = c->next_job; if (!g_world.job_head) g_world.job_tail = NULL; }
        mutex_unlock(&g_world.lock);
        if (!c) { plat_sleep_ms(2); continue; }
        if (!c->cancelled) {
            if (!save_read_column(g_world.dim_dir, c)) worldgen_column(c, g_world.seed);
        }
        mutex_lock(&g_world.lock);
        c->next_job = g_world.done_head;
        g_world.done_head = c;
        mutex_unlock(&g_world.lock);
    }
    return NULL;
}

bool g_world_sync = false;

bool world_open(const char *name, u64 seed, bool create, int dim) {
    (void)create;
    World *w = &g_world;
    Shader sh = w->chunk_shader; Texture at = w->atlas;
    memset(w->slots, 0, sizeof w->slots);
    w->chunk_shader = sh; w->atlas = at;
    w->seed = seed;
    redstone_reset();
    structures_reset();
    snprintf(w->name, sizeof w->name, "%s", name);
    snprintf(w->dir, sizeof w->dir, "saves/%s", name);
    plat_mkdirs(w->dir);
    w->dim = clampi(dim, 0, DIM_COUNT - 1);
    worldgen_set_dim(w->dim);
    if (w->dim == DIM_OVERWORLD) snprintf(w->dim_dir, sizeof w->dim_dir, "%.250s", w->dir);
    else snprintf(w->dim_dir, sizeof w->dim_dir, "%.250s/%s", w->dir, dim_info(w->dim)->folder);
    char sub[400]; snprintf(sub, sizeof sub, "%.299s/columns", w->dim_dir); plat_mkdirs(sub);
    w->job_head = w->job_tail = w->done_head = NULL;
    w->jobs_pending = 0; w->loaded = 0; w->quit = false;
    w->nbents = 0; w->nupdates = 0;
    if (!w->render_dist) w->render_dist = 8;
    w->center_cx = w->center_cz = 0x7fffffff;
    mutex_init(&w->lock);
    w->nworkers = clampi(plat_cpu_count() - 1, 1, 4);
    for (int i = 0; i < w->nworkers; i++) pthread_create(&w->workers[i], NULL, worker_main, NULL);
    w->active = true;
    LOG_INFO("world '%s' opened (seed %llu, %d generator threads)", name, (unsigned long long)seed, w->nworkers);
    return true;
}

static void column_free(Column *c) {
    for (int s = 0; s < SECTIONS; s++) {
        if (c->sec[s].vbo) glDeleteBuffers(1, &c->sec[s].vbo);
        if (c->sec[s].vao) glDeleteVertexArrays(1, &c->sec[s].vao);
    }
    free(c);
}

void world_save_all(void) {
    for (int i = 0; i < COL_SLOTS * COL_SLOTS; i++) {
        Column *c = g_world.slots[i];
        if (c && c->state == COL_READY && c->modified) { save_write_column(g_world.dim_dir, c); c->modified = false; }
    }
}

void world_close(void) {
    World *w = &g_world;
    if (!w->active) return;
    world_save_all();
    w->quit = true;
    for (int i = 0; i < w->nworkers; i++) pthread_join(w->workers[i], NULL);
    // free queued + done + loaded columns (a column is in exactly one of: job list, done list, worker (none now), or slots)
    for (Column *c = w->job_head; c;) { Column *n = c->next_job; if (c->cancelled) column_free(c); c = n; }
    for (Column *c = w->done_head; c;) { Column *n = c->next_job; if (c->cancelled) column_free(c); c = n; }
    for (int i = 0; i < COL_SLOTS * COL_SLOTS; i++) if (w->slots[i]) { column_free(w->slots[i]); w->slots[i] = NULL; }
    w->job_head = w->job_tail = w->done_head = NULL;
    mutex_destroy(&w->lock);
    free(w->bents); w->bents = NULL; w->cap_bents = w->nbents = 0;
    free(w->updates); w->updates = NULL; w->cap_updates = w->nupdates = 0;
    w->active = false;
}

// ======================================================================== loading / unloading
static Column **slot_of(int cx, int cz) { return &g_world.slots[(cz & (COL_SLOTS - 1)) * COL_SLOTS + (cx & (COL_SLOTS - 1))]; }


static bool neighbours_ready(int cx, int cz) {
    for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) if (!world_column(cx + dx, cz + dz)) return false;
    return true;
}

static void mark_column_dirty(Column *c) { for (int s = 0; s < SECTIONS; s++) c->sec[s].dirty = true; }

bool world_area_ready(vec3 pos, int r) {
    int cx = (int)floorf(pos.x) >> 4, cz = (int)floorf(pos.z) >> 4;
    for (int dz = -r; dz <= r; dz++) for (int dx = -r; dx <= r; dx++) {
        Column *c = world_column(cx + dx, cz + dz);
        if (!c) return false;
        if (abs(dx) < r && abs(dz) < r) for (int s = 0; s < SECTIONS; s++) if (c->sec[s].dirty) return false;
    }
    return true;
}

void world_update(vec3 pp, f64 budget_ms) {
    World *w = &g_world;
    int pcx = (int)floorf(pp.x) >> 4, pcz = (int)floorf(pp.z) >> 4;
    int rd = clampi(w->render_dist, 2, MAX_RENDER_DIST);
    w->center_cx = pcx; w->center_cz = pcz;

    // 1) unload far columns
    for (int i = 0; i < COL_SLOTS * COL_SLOTS; i++) {
        Column *c = w->slots[i];
        if (!c) continue;
        bool forced = forceload_has(c->cx, c->cz) && abs(c->cx - pcx) < COL_SLOTS / 2 - 1 && abs(c->cz - pcz) < COL_SLOTS / 2 - 1;   // /forceload
        if ((abs(c->cx - pcx) > rd + 2 || abs(c->cz - pcz) > rd + 2) && !forced) {
            w->slots[i] = NULL;
            if (c->state != COL_READY) { c->cancelled = true; continue; } // worker or done list still owns it
            if (c->modified) save_write_column(w->dim_dir, c);
            column_free(c);
            w->loaded--;
        }
    }
    // 2) collect generated columns, light them
    if (g_world_sync) {   // scripted runs: wait for every queued column, so which frame a column appears in never depends on thread timing
        for (;;) {
            int n = 0;
            mutex_lock(&w->lock);
            for (Column *q = w->done_head; q; q = q->next_job) n++;
            mutex_unlock(&w->lock);
            if (n >= w->jobs_pending) break;
            plat_sleep_ms(1);
        }
    }
    mutex_lock(&w->lock);
    Column *done = w->done_head; w->done_head = NULL;
    mutex_unlock(&w->lock);
    if (g_world_sync) {   // ... and the finished columns are always processed in the same order (their hooks draw random numbers)
        Column *arr[512]; int n = 0;
        for (Column *q = done; q && n < 512; q = q->next_job) arr[n++] = q;
        for (int i = 1; i < n; i++) { Column *k = arr[i]; int j = i - 1; while (j >= 0 && (arr[j]->cz > k->cz || (arr[j]->cz == k->cz && arr[j]->cx > k->cx))) { arr[j + 1] = arr[j]; j--; } arr[j + 1] = k; }
        if (n < 512) { for (int i = 0; i < n; i++) arr[i]->next_job = i + 1 < n ? arr[i + 1] : NULL; done = n ? arr[0] : NULL; }
    }
    while (done) {
        Column *c = done; done = c->next_job;
        w->jobs_pending--;
        if (c->cancelled) { column_free(c); continue; }
        c->state = COL_READY;
        w->loaded++;
        light_init_column(c);
        mark_column_dirty(c);
        rs_scan_column(c);
        game_hook_column_ready(c);
        for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) {
            Column *n = world_column(c->cx + dx, c->cz + dz);
            if (n && n != c) mark_column_dirty(n);
        }
    }
    // 3) queue new columns nearest first (bounded queue keeps priorities fresh as the player moves)
    int max_pending = w->nworkers * 3 + 4;
    for (int i = 0; i < spiral_n && w->jobs_pending < max_pending; i++) {
        int dx = spiral[i].dx, dz = spiral[i].dz;
        if (abs(dx) > rd + 1 || abs(dz) > rd + 1) continue;
        int cx = pcx + dx, cz = pcz + dz;
        Column **slot = slot_of(cx, cz);
        if (*slot) continue;
        Column *c = xcalloc(1, sizeof(Column));
        c->cx = cx; c->cz = cz; c->state = COL_QUEUED;
        *slot = c;
        mutex_lock(&w->lock);
        if (w->job_tail) w->job_tail->next_job = c; else w->job_head = c;
        w->job_tail = c;
        mutex_unlock(&w->lock);
        w->jobs_pending++;
    }
    // 4) mesh dirty sections nearest first within the time budget
    f64 t0 = plat_time();
    int meshed = 0;
    for (int i = 0; i < spiral_n; i++) {
        int dx = spiral[i].dx, dz = spiral[i].dz;
        if (abs(dx) > rd || abs(dz) > rd) continue;
        Column *c = world_column(pcx + dx, pcz + dz);
        if (!c) continue;
        bool any = false;
        for (int s = 0; s < SECTIONS; s++) any |= c->sec[s].dirty;
        if (!any || !neighbours_ready(c->cx, c->cz)) continue;
        for (int s = 0; s < SECTIONS; s++) if (c->sec[s].dirty) { mesh_section(c, s); meshed++; }
        if (!g_world_sync && (plat_time() - t0) * 1000.0 > budget_ms && meshed > 0) break;   // (scripted runs mesh everything: no time budget)
    }
    w->sections_meshed_total += meshed;
}

// ======================================================================== block access
void world_mark_dirty(int x, int y, int z) {
    int lx = x & 15, lz = z & 15, s = y >> 4, ly = y & 15;
    for (int dx = -1; dx <= 1; dx++) for (int dz = -1; dz <= 1; dz++) {
        if ((dx == -1 && lx != 0) || (dx == 1 && lx != 15) || (dz == -1 && lz != 0) || (dz == 1 && lz != 15)) continue;
        Column *c = world_column_at(x + dx, z + dz);
        if (!c) continue;
        c->sec[s].dirty = true;
        if (ly == 0 && s > 0) c->sec[s - 1].dirty = true;
        if (ly == 15 && s < SECTIONS - 1) c->sec[s + 1].dirty = true;
    }
}

static void update_heightmap(Column *c, int lx, int lz) {
    int y = WORLD_H - 1;
    while (y >= 0 && BLOCKS[c->blocks[y][lz][lx]].opacity == 0) y--;
    c->heightmap[lz][lx] = (u16)(y + 1);
}

static bool has_block_entity(Block b) { return BLOCKS[b].be != BE_NONE; }

void world_set_block_silent(int x, int y, int z, u16 b) {
    if (y < 0 || y >= WORLD_H) return;
    Column *c = world_column_at(x, z);
    if (!c) return;
    u16 old = c->blocks[y][z & 15][x & 15];
    if (old == b) return;
    c->blocks[y][z & 15][x & 15] = b;
    c->modified = true;
    update_heightmap(c, x & 15, z & 15);
    light_block_changed(x, y, z);
    world_mark_dirty(x, y, z);
    if (has_block_entity(old) && BLOCKS[old].base != BLOCKS[b].base) world_remove_block_entity(iv3(x, y, z));
}

bool world_set_block(int x, int y, int z, u16 b) {
    if (y < 0 || y >= WORLD_H || !world_column_at(x, z)) return false;
    world_set_block_silent(x, y, z, b);
    rs_block_changed(iv3(x, y, z));
    world_schedule_update(iv3(x, y, z), 1);
    for (int f = 0; f < 6; f++) world_schedule_update(iv3(x + FACE_DIR[f].x, y + FACE_DIR[f].y, z + FACE_DIR[f].z), 1);
    return true;
}

int world_top_y(int x, int z) {
    Column *c = world_column_at(x, z);
    if (!c) return -1;
    int y0 = WORLD_H - 1;
    if (g_world.dim == DIM_NETHER) {   // skip the bedrock roof: find the first air pocket from the top
        y0 = 120;
        while (y0 > 0 && BLOCKS[c->blocks[y0][z & 15][x & 15]].solid) y0--;
    }
    for (int y = y0; y >= 0; y--) if (BLOCKS[c->blocks[y][z & 15][x & 15]].solid) return y;
    return -1;
}

int world_biome_at(int x, int y, int z) {
    Column *c = world_column_at(x, z);
    int b = c ? c->biome[z & 15][x & 15] : 0;
    if (g_world.dim == DIM_OVERWORLD && c && y < c->heightmap[z & 15][x & 15] - 8 && y < SEA_LEVEL - 4) {
        int cb = worldgen_cave_biome(g_world.seed, x, y, z);
        if (cb >= 0) return cb;
    }
    return b;
}

// ======================================================================== time & light
f32 world_daylight(void) {
    if (g_world.dim == DIM_NETHER) return 0.0f;
    if (g_world.dim == DIM_END) return 0.0f;
    if (g_world.dim == DIM_SIFT) return 1.0f;   // eternal soft daylight
    f32 t = (f32)(world_daytime() % DAY_TICKS) / DAY_TICKS;   // 0 = sunrise, .25 noon, .5 sunset, .75 midnight
    f32 s = sinf(t * 2 * PI_F);
    f32 d = 0.18f + 0.82f * smoothstepf(-0.12f, 0.25f, s);
    // rain and thunder darken the sky (Minecraft's rain/thunder sky darkening)
    d *= 1.0f - (g_level.rain_level * 0.3125f + g_level.thunder_level * g_level.rain_level * 0.3125f);
    if (g_level.lightning_flash > 0) d = fmaxf(d, 0.9f);
    return clampf(d, 0.18f, 1.0f);
}
bool world_is_night(void) { if (g_world.dim != DIM_OVERWORLD) return false; f32 t = (f32)(world_daytime() % DAY_TICKS) / DAY_TICKS; return t > 0.53f && t < 0.97f; }
int world_light_level(ivec3 p) {
    u16 l = world_get_light(p.x, p.y, p.z);
    int sky = g_world.dim == DIM_OVERWORLD ? (int)(L_SKY(l) * world_daylight() + 0.5f) - (world_daylight() < 0.3f ? 4 : 0) : g_world.dim == DIM_SIFT ? L_SKY(l) : 0;
    int blk = MAXI(L_R(l), MAXI(L_G(l), L_B(l)));
    return clampi(MAXI(sky, blk), 0, 15);
}

// ======================================================================== block entities
BlockEntity *world_block_entity(ivec3 p, bool create, int type) {
    for (int i = 0; i < g_world.nbents; i++) if (iv3_eq(g_world.bents[i].pos, p)) return &g_world.bents[i];
    if (!create) return NULL;
    if (g_world.nbents == g_world.cap_bents) {
        g_world.cap_bents = g_world.cap_bents ? g_world.cap_bents * 2 : 16;
        g_world.bents = xrealloc(g_world.bents, sizeof(BlockEntity) * g_world.cap_bents);
    }
    BlockEntity *be = &g_world.bents[g_world.nbents++];
    memset(be, 0, sizeof *be);
    be->pos = p; be->type = (u8)type;
    return be;
}
void world_remove_block_entity(ivec3 p) {
    for (int i = 0; i < g_world.nbents; i++) if (iv3_eq(g_world.bents[i].pos, p)) {
        g_world.bents[i] = g_world.bents[--g_world.nbents];
        return;
    }
}

// ======================================================================== scheduled & random updates
// Pending block updates are unique per position: scheduling a position that already has an update pending keeps
// the earlier of the two. (Before v5 every request was appended, so blocks that keep re-scheduling themselves,
// like eternal fire on hellrack, started a new self-sustaining chain on every random tick and the queue grew
// without bound until the game stalled.) A small open-addressing hash maps position -> index in g_world.updates.
typedef struct { u64 key; int idx; } USlot;   // key 0 = empty
static USlot *uset; static int ucap, ucount;
static u64 upkey(ivec3 p) { return (((u64)(u32)(p.x + 0x100000) & 0x1FFFFF) << 30 | ((u64)(u32)(p.z + 0x100000) & 0x1FFFFF) << 9 | ((u64)p.y & 0x1FF)) + 1; }
static int uslot(u64 k) { u64 h = k * 0x9E3779B97F4A7C15ull; return (int)(h >> 40) & (ucap - 1); }
static int uset_find(u64 k) { if (!ucap) return -1; for (int i = uslot(k);; i = (i + 1) & (ucap - 1)) { if (!uset[i].key) return -1; if (uset[i].key == k) return i; } }
static void uset_put(u64 k, int idx);
static void uset_grow(void) {
    USlot *old = uset; int oc = ucap;
    ucap = ucap ? ucap * 2 : 1024; uset = xcalloc((size_t)ucap, sizeof(USlot)); ucount = 0;
    for (int i = 0; i < oc; i++) if (old[i].key) uset_put(old[i].key, old[i].idx);
    free(old);
}
static void uset_put(u64 k, int idx) {
    if ((ucount + 1) * 2 > ucap) uset_grow();
    int i = uslot(k);
    while (uset[i].key && uset[i].key != k) i = (i + 1) & (ucap - 1);
    if (!uset[i].key) ucount++;
    uset[i] = (USlot){k, idx};
}
static void uset_del(u64 k) {   // linear-probing deletion with backward shift
    int i = uset_find(k);
    if (i < 0) return;
    uset[i].key = 0; ucount--;
    for (int j = (i + 1) & (ucap - 1); uset[j].key; j = (j + 1) & (ucap - 1)) {
        USlot s = uset[j]; uset[j].key = 0; ucount--;
        uset_put(s.key, s.idx);
    }
}
void world_schedule_update(ivec3 p, int delay) {
    if (p.y < 0 || p.y >= WORLD_H) return;
    if (g_world.nupdates == 0 && ucount) { memset(uset, 0, sizeof(USlot) * (size_t)ucap); ucount = 0; }   // fresh world / empty queue
    u64 k = upkey(p), due = g_world.time + (u64)delay;
    int s = uset_find(k);
    if (s >= 0) {
        int idx = uset[s].idx;
        if (idx < g_world.nupdates && iv3_eq(g_world.updates[idx].pos, p)) {
            if (due < g_world.updates[idx].due) g_world.updates[idx].due = due;
            return;
        }
    }
    if (g_world.nupdates == g_world.cap_updates) {
        g_world.cap_updates = g_world.cap_updates ? g_world.cap_updates * 2 : 256;
        g_world.updates = xrealloc(g_world.updates, sizeof(ScheduledUpdate) * g_world.cap_updates);
    }
    uset_put(k, g_world.nupdates);
    g_world.updates[g_world.nupdates++] = (ScheduledUpdate){p, due};
}
static void updates_remove(World *w, int i) {   // swap-remove keeping the position index in sync
    uset_del(upkey(w->updates[i].pos));
    w->updates[i] = w->updates[--w->nupdates];
    if (i < w->nupdates) uset_put(upkey(w->updates[i].pos), i);
}

static bool is_soil(Block b) {
    Block k = BLOCKS[b].base;
    return k == B_GRASS_BLOCK || k == B_DIRT || k == B_COARSE_DIRT || k == B_PODZOL || k == B_ROOTED_DIRT || k == B_MUD || k == B_MYCELIUM ||
           k == B_FARMLAND || k == B_MOSS_BLOCK || k == B_PALE_MOSS_BLOCK || k == B_MUDDY_MANGROVE_ROOTS || k == B_CLAY || k == B_GOLDEN_GRASS_BLOCK;
}
static bool is_sandy(Block b) {
    Block k = BLOCKS[b].base;
    return k == B_SAND || k == B_RED_SAND || k == B_TERRACOTTA || is_soil(b) || (BLOCKS[b].key && strstr(BLOCKS[b].key, "terracotta"));
}

// can block b stay at p?
bool world_block_supported(ivec3 p, Block b) {
    const BlockDef *d = &BLOCKS[b];
    Block below = world_get_block_or(p.x, p.y - 1, p.z, B_STONE), above = world_get_block_or(p.x, p.y + 1, p.z, B_STONE);
    // wall-mounted kinds: support lies in the direction encoded by the state
    if (d->place == PL_WALLONLY || (d->place == PL_SIGN && d->var >= 4)) {
        int f = H4_FACE[d->var % 4];   // support side
        ivec3 q = iv3(p.x + FACE_DIR[f].x, p.y, p.z + FACE_DIR[f].z);
        Block s = world_get_block_or(q.x, q.y, q.z, B_STONE);
        if (b >= B_VINE && b < B_VINE + 4) return BLOCKS[s].solid || block_is_leaves(s);
        return BLOCKS[s].solid;
    }
    if (d->place == PL_ATTACH) {
        int f = d->var % 6;
        ivec3 q = iv3(p.x + FACE_DIR[f].x, p.y + FACE_DIR[f].y, p.z + FACE_DIR[f].z);
        return BLOCKS[world_get_block_or(q.x, q.y, q.z, B_STONE)].solid;
    }
    if (d->place == PL_DOUBLE || d->place == PL_DOOR) {   // two-block things: halves need each other
        bool upper = block_upper_half(b);
        Block other = upper ? below : above;
        if (BLOCKS[other].base != d->base) return false;
        if (upper) return true;
    }
    if (d->place == PL_BED) {
        int fc = H4_FACE[block_facing4(b)];
        int dir = (d->var & 1) ? -1 : 1;   // head looks back toward the foot
        ivec3 q = iv3(p.x + FACE_DIR[fc].x * dir, p.y, p.z + FACE_DIR[fc].z * dir);
        return BLOCKS[world_get_block_or(q.x, q.y, q.z, B_STONE)].base == d->base;
    }
    switch (d->support) {
    case SUP_NONE: return true;
    case SUP_SOLID: return BLOCKS[below].solid && !block_is_leaves(below);
    case SUP_SOIL: return is_soil(below) || (d->base == B_DEAD_BUSH && is_sandy(below));
    case SUP_SAND:
        if (d->base == B_CACTUS) {
            for (int f = 0; f < 4; f++) { Block n = world_get_block(p.x + FACE_DIR[H4_FACE[f]].x, p.y, p.z + FACE_DIR[H4_FACE[f]].z); if (BLOCKS[n].solid) return false; }
            return BLOCKS[below].base == B_SAND || BLOCKS[below].base == B_RED_SAND || below == B_CACTUS;
        }
        return is_sandy(below);
    case SUP_FARMLAND: return BLOCKS[below].base == B_FARMLAND;
    case SUP_WATER: return block_is_water(below) || below == B_ICE;
    case SUP_SOUL: return below == B_SOUL_SAND;
    case SUP_NYLIUM: return below == B_CRIMSON_NYLIUM || below == B_WARPED_NYLIUM || below == B_SOUL_SOIL || is_soil(below);
    case SUP_CEILING: return BLOCKS[above].solid || BLOCKS[above].base == d->base || block_is_leaves(above);
    case SUP_STACK:
        if (BLOCKS[below].base == d->base) return true;
        if (d->base == B_KELP || d->base == B_KELP_PLANT) return BLOCKS[below].solid || below == B_KELP || below == B_KELP_PLANT;
        if (d->base == B_SUGAR_CANE) {
            if (!(is_sandy(below))) return false;
            for (int f = 0; f < 4; f++) if (block_is_water(world_get_block(p.x + FACE_DIR[H4_FACE[f]].x, p.y - 1, p.z + FACE_DIR[H4_FACE[f]].z))) return true;
            return false;
        }
        return BLOCKS[below].solid;
    case SUP_ANY: return below != B_AIR && !BLOCKS[below].liquid;
    default: return true;
    }
}

static void block_update(ivec3 p) {
    if (!world_loaded(p.x, p.z)) return;
    Block b = world_get_block(p.x, p.y, p.z);
    const BlockDef *d = &BLOCKS[b];
    if (b == B_AIR) return;
    if (d->support != SUP_NONE || d->place == PL_WALLONLY || d->place == PL_ATTACH || d->place == PL_DOUBLE || d->place == PL_DOOR || d->place == PL_BED || (d->place == PL_SIGN)) {
        if (!world_block_supported(p, b)) {
            world_set_block(p.x, p.y, p.z, B_AIR);
            if (!(d->place == PL_DOUBLE && block_upper_half(b)) && !(d->place == PL_DOOR && block_upper_half(b)) && !(d->place == PL_BED && (d->var & 1)))
                game_hook_block_broken_naturally(p, b);
            return;
        }
    }
    if (d->falls) {
        Block below = world_get_block_or(p.x, p.y - 1, p.z, B_STONE);
        if (p.y > 0 && (block_is_air(below) || BLOCKS[below].replaceable || BLOCKS[below].liquid)) {
            world_set_block(p.x, p.y, p.z, B_AIR);
            if (falling_try_spawn(v3(p.x + 0.5f, (f32)p.y, p.z + 0.5f), b)) return;   // it falls as an entity (gravity, drag, anvil damage)
            if (BLOCKS[below].liquid) { world_set_block(p.x, p.y - 1, p.z, b); return; }
            world_set_block(p.x, p.y - 1, p.z, b);
            if (BLOCKS[below].support != SUP_NONE && !BLOCKS[below].liquid) game_hook_block_broken_naturally(iv3(p.x, p.y - 1, p.z), below);
            return;
        }
    }
    // concrete powder hardens next to water
    if (d->key && strstr(d->key, "_concrete_powder")) {
        for (int f = 0; f < 6; f++) if (f != FACE_D && block_is_water(world_get_block(p.x + FACE_DIR[f].x, p.y + FACE_DIR[f].y, p.z + FACE_DIR[f].z))) {
            char k[64]; snprintf(k, sizeof k, "%.*s", (int)(strlen(d->key) - 7), d->key);   // strip "_powder"
            Block c = block_by_key(k);
            if (c) world_set_block(p.x, p.y, p.z, c);
            return;
        }
    }
    // frosted ice (frost walker) melts unless a player stands close
    if (d->base == B_FROSTED_ICE) {
        Entity *pl = player_ent();
        if (pl && fabsf(pl->pos.x - p.x - 0.5f) < 2.5f && fabsf(pl->pos.z - p.z - 0.5f) < 2.5f && fabsf(pl->pos.y - p.y - 1) < 1.5f) { world_schedule_update(p, 60); return; }
        if (d->var < 3) { world_set_block_silent(p.x, p.y, p.z, (Block)(b + 1)); world_schedule_update(p, irand(40, 80)); }
        else world_set_block(p.x, p.y, p.z, B_WATER);
        return;
    }
    // flowing water / lava
    if (d->base == B_WATER || d->base == B_LAVA) { fluid_update(p); return; }
    // redstone components and mechanisms
    redstone_update(p);
    // fire burns out unless it sits on netherrack / magma
    if (b == B_FIRE || b == B_SOUL_FIRE) {
        Block below = world_get_block(p.x, p.y - 1, p.z);
        bool eternal = below == B_NETHERRACK || below == B_MAGMA_BLOCK || (below == B_BEDROCK && g_world.dim == DIM_END) || b == B_SOUL_FIRE;
        bool fuel_near = false;
        for (int f = 0; f < 6; f++) fuel_near |= block_flammability(world_get_block(p.x + FACE_DIR[f].x, p.y + FACE_DIR[f].y, p.z + FACE_DIR[f].z)) > 0;
        if (!BLOCKS[below].solid && !fuel_near) { world_set_block(p.x, p.y, p.z, B_AIR); return; }
        if (!rule(GR_DO_FIRE_TICK)) return;
        // neighbour updates only check survival (above); burning and spreading happen on fire's own tick,
        // 30 + rand(10) game ticks apart. Running them on every neighbour change made big fire fields churn forever.
        if (!fluid_due_now(p, irand(30, 40) * MC_TICK_DIV)) return;
        fluid_arm(p, irand(30, 40) * MC_TICK_DIV);
        if (!eternal && irand(0, 3) == 0 && !fuel_near) { world_set_block(p.x, p.y, p.z, B_AIR); return; }   // burns out
        if (!eternal && irand(0, 15) == 0) { world_set_block(p.x, p.y, p.z, B_AIR); return; }
        // burn adjacent fuel: the block may be consumed and replaced by fire
        for (int f = 0; f < 6; f++) {
            ivec3 q = iv3(p.x + FACE_DIR[f].x, p.y + FACE_DIR[f].y, p.z + FACE_DIR[f].z);
            Block nb = world_get_block(q.x, q.y, q.z);
            int fl = block_flammability(nb);
            if (fl && irand(0, f == FACE_U || f == FACE_D ? 250 : 300) < fl) {
                if (nb == B_TNT) { world_set_block(q.x, q.y, q.z, B_AIR); game_hook_tnt_ignite(q); continue; }
                world_set_block(q.x, q.y, q.z, irand(0, 9) < 5 ? B_FIRE : B_AIR);
            }
        }
        // spread into air next to flammable blocks within 1 horizontal / 4 up
        for (int dy = -1; dy <= 4; dy++) for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) {
            if (!dx && !dy && !dz) continue;
            ivec3 q = iv3(p.x + dx, p.y + dy, p.z + dz);
            if (!block_is_air(world_get_block(q.x, q.y, q.z))) continue;
            int enc = 0;
            for (int f = 0; f < 6; f++) enc = MAXI(enc, block_encouragement(world_get_block(q.x + FACE_DIR[f].x, q.y + FACE_DIR[f].y, q.z + FACE_DIR[f].z)));
            if (!enc) continue;
            int odds = 100 + (dy > 1 ? (dy - 1) * 100 : 0);
            int chance = (enc + 40 + g_level.difficulty * 7) / 2;   // rough port of FireBlock.tick
            if (irand(0, odds - 1) < chance && irand(0, 2) == 0) { world_set_block(q.x, q.y, q.z, B_FIRE); fluid_arm(q, irand(30, 40) * MC_TICK_DIV); }
        }
    }
}

// growth helpers (also used by bone meal)
bool world_grow(ivec3 p, Rng *r, bool bonemeal) {
    Block b = world_get_block(p.x, p.y, p.z);
    const BlockDef *d = &BLOCKS[b];
    if (d->mesh == MESH_CROP && d->var < d->nvar - 1) {
        int step = bonemeal ? rng_int(r, 2, 5) : 1;
        if (d->base == B_NETHER_WART_CROP || d->base == B_SWEET_BERRY_BUSH || d->base == B_TORCHFLOWER_CROP) step = 1;
        world_set_block(p.x, p.y, p.z, block_with_var(b, d->var + step));
        if (d->base == B_TORCHFLOWER_CROP && d->var + step >= d->nvar - 1) world_set_block(p.x, p.y, p.z, B_TORCHFLOWER);
        if (d->base == B_PITCHER_CROP && d->var + step >= d->nvar - 1 && world_get_block(p.x, p.y + 1, p.z) == B_AIR) {
            world_set_block_silent(p.x, p.y, p.z, B_PITCHER_PLANT); world_set_block(p.x, p.y + 1, p.z, B_PITCHER_PLANT + 1);
        }
        return true;
    }
    if ((d->base == B_MELON_STEM || d->base == B_PUMPKIN_STEM) && d->var == d->nvar - 1) {   // fruit grows next to a ripe stem
        int f = H4_FACE[rng_int(r, 0, 3)];
        ivec3 q = iv3(p.x + FACE_DIR[f].x, p.y, p.z + FACE_DIR[f].z);
        if (block_is_air(world_get_block(q.x, q.y, q.z)) && is_soil(world_get_block(q.x, q.y - 1, q.z))) {
            world_set_block(q.x, q.y, q.z, d->base == B_MELON_STEM ? B_MELON : B_PUMPKIN);
            return true;
        }
        return false;
    }
    if (d->place == PL_SIMPLE && d->key && (strstr(d->key, "_sapling") || d->base == B_MANGROVE_PROPAGULE || d->base == B_AZALEA || d->base == B_FLOWERING_AZALEA)) {
        int kind = TREE_OAK;
        Block k = d->base;
        if (k == B_SPRUCE_SAPLING) kind = TREE_SPRUCE; else if (k == B_BIRCH_SAPLING) kind = TREE_BIRCH; else if (k == B_JUNGLE_SAPLING) kind = TREE_JUNGLE;
        else if (k == B_ACACIA_SAPLING) kind = TREE_ACACIA; else if (k == B_DARK_OAK_SAPLING) kind = TREE_DARK_OAK; else if (k == B_PALE_OAK_SAPLING) kind = TREE_PALE_OAK;
        else if (k == B_CHERRY_SAPLING) kind = TREE_CHERRY; else if (k == B_MANGROVE_PROPAGULE) kind = TREE_MANGROVE; else if (k == B_AZALEA || k == B_FLOWERING_AZALEA) kind = TREE_AZALEA;
        else if (k == B_OAK_SAPLING && rng_int(r, 0, 9) == 0) kind = TREE_FANCY_OAK;
        if (!bonemeal || rng_int(r, 0, 2) == 0) return worldgen_grow_tree(p.x, p.y, p.z, kind, r);
        return true;
    }
    if (d->base == B_RED_MUSHROOM || d->base == B_BROWN_MUSHROOM) {
        if (bonemeal && rng_int(r, 0, 2) == 0) return worldgen_grow_tree(p.x, p.y, p.z, d->base == B_RED_MUSHROOM ? TREE_HUGE_RED_MUSHROOM : TREE_HUGE_BROWN_MUSHROOM, r);
        return bonemeal;
    }
    if (d->base == B_CRIMSON_FUNGUS || d->base == B_WARPED_FUNGUS) {
        if (bonemeal && rng_int(r, 0, 2) == 0) return worldgen_grow_tree(p.x, p.y, p.z, d->base == B_CRIMSON_FUNGUS ? TREE_CRIMSON_FUNGUS : TREE_WARPED_FUNGUS, r);
        return bonemeal;
    }
    if (d->base == B_COCOA && d->var < 2) { world_set_block(p.x, p.y, p.z, b + 1); return true; }
    if (d->base == B_CAVE_VINES && d->var == 0 && bonemeal) { world_set_block(p.x, p.y, p.z, b + 1); return true; }
    // stacking plants grow upward
    if (d->base == B_SUGAR_CANE || d->base == B_CACTUS || d->base == B_BAMBOO || d->base == B_KELP || d->base == B_TWISTING_VINES) {
        int h = 1;
        while (h < 4 && world_get_block(p.x, p.y - h, p.z) == b) h++;
        Block up = world_get_block(p.x, p.y + 1, p.z);
        int max = d->base == B_BAMBOO ? 14 : d->base == B_KELP ? 20 : 3;
        bool room = d->base == B_KELP ? block_is_water(up) : block_is_air(up);
        if (h < max && room && p.y + 1 < WORLD_H) { world_set_block(p.x, p.y + 1, p.z, b); return true; }
        return false;
    }
    if (bonemeal && (d->base == B_GRASS_BLOCK || d->base == B_MOSS_BLOCK)) {   // scatter grass and flowers
        for (int i = 0; i < 24; i++) {
            int x = p.x + rng_int(r, -3, 3), z = p.z + rng_int(r, -3, 3);
            for (int y = p.y + 2; y >= p.y - 1; y--) {
                if (world_get_block(x, y - 1, z) == B_GRASS_BLOCK && block_is_air(world_get_block(x, y, z))) {
                    int k = rng_int(r, 0, 9);
                    world_set_block(x, y, z, k < 7 ? B_SHORT_GRASS : k == 7 ? B_DANDELION : k == 8 ? B_POPPY : B_TALL_GRASS);
                    if (k == 9 && block_is_air(world_get_block(x, y + 1, z))) world_set_block(x, y + 1, z, B_TALL_GRASS + 1); else if (k == 9) world_set_block(x, y, z, B_SHORT_GRASS);
                    break;
                }
            }
        }
        return true;
    }
    if (bonemeal && b == B_SHORT_GRASS && block_is_air(world_get_block(p.x, p.y + 1, p.z))) { world_set_block_silent(p.x, p.y, p.z, B_TALL_GRASS); world_set_block(p.x, p.y + 1, p.z, B_TALL_GRASS + 1); return true; }
    return false;
}

static bool is_tree_wood(Block b) {
    const char *k = BLOCKS[b].key;
    return BLOCKS[b].place == PL_AXIS && k && (strstr(k, "_log") || strstr(k, "_wood") || strstr(k, "_stem") || strstr(k, "hyphae") || b == B_MANGROVE_ROOTS);
}
// Minecraft's leaf rule: a leaf lives while a log is reachable through connected leaves, at most 6 blocks away
// (a leaf touching a log has distance 1, one more leaf in between 2, ...). Breadth-first search over the leaf network;
// unloaded neighbours count as wood so chunk borders never cause mass decay.
static bool leaves_attached(int x, int y, int z) {
    enum { R = 5, W = 2 * R + 1 };
    static const int D[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    u8 seen[W][W][W] = {{{0}}};
    s8 qx[W * W * W], qy[W * W * W], qz[W * W * W], qd[W * W * W];
    int head = 0, tail = 0;
    qx[tail] = qy[tail] = qz[tail] = 0; qd[tail++] = 1; seen[R][R][R] = 1;
    while (head < tail) {
        int cx = qx[head], cy = qy[head], cz = qz[head], dist = qd[head++];
        for (int f = 0; f < 6; f++) {
            int nx = cx + D[f][0], ny = cy + D[f][1], nz = cz + D[f][2];
            if (abs(nx) > R || abs(ny) > R || abs(nz) > R || seen[nx + R][ny + R][nz + R]) continue;
            Block b = world_get_block_or(x + nx, y + ny, z + nz, B_OAK_LOG);
            if (is_tree_wood(b)) return true;
            if (!block_is_leaves(b) || dist >= 6) continue;   // only leaves carry the distance on, and only so far
            seen[nx + R][ny + R][nz + R] = 1;
            qx[tail] = (s8)nx; qy[tail] = (s8)ny; qz[tail] = (s8)nz; qd[tail++] = (s8)(dist + 1);
        }
    }
    return false;
}

// Minecraft random ticks: randomTickSpeed random blocks per 16x16x16 section per game tick
static void random_tick(Column *c, Rng *r) {
    int per = rule_i(GR_RANDOM_TICK_SPEED);
    for (int i = 0; i < per * SECTIONS; i++) {
        int lx = rng_int(r, 0, 15), lz = rng_int(r, 0, 15), y = (i % SECTIONS) * 16 + rng_int(r, 0, 15);
        if (y < 1 || y > WORLD_H - 2) continue;
        Block b = c->blocks[y][lz][lx];
        if (!b) continue;
        int x = c->cx * 16 + lx, z = c->cz * 16 + lz;
        Block above = c->blocks[y + 1][lz][lx];
        const BlockDef *d = &BLOCKS[b];
        if (b >= B_VINE && b < B_VINE + 4 && rule(GR_DO_VINES_SPREAD) && rng_int(r, 0, 3) == 0) {   // vines creep downward
            if (block_is_air(c->blocks[y - 1][lz][lx])) world_set_block(x, y - 1, z, b);
        }
        else if (b == B_GRASS_BLOCK && BLOCKS[above].opaque) world_set_block(x, y, z, B_DIRT);
        else if (b == B_GRASS_BLOCK || b == B_MYCELIUM) {  // spread onto nearby lit dirt
            int nx = x + rng_int(r, -1, 1), ny = y + rng_int(r, -2, 1), nz = z + rng_int(r, -1, 1);
            if (world_get_block(nx, ny, nz) == B_DIRT && !BLOCKS[world_get_block(nx, ny + 1, nz)].opaque && L_SKY(world_get_light(nx, ny + 1, nz)) >= 9)
                world_set_block(nx, ny, nz, b);
        } else if (d->base == B_FARMLAND) {   // moist next to water, reverts to dirt when dry and empty
            bool wet = false;
            for (int dz = -4; dz <= 4 && !wet; dz++) for (int dx = -4; dx <= 4 && !wet; dx++) for (int dy = 0; dy <= 1; dy++) if (block_is_water(world_get_block(x + dx, y + dy, z + dz))) { wet = true; break; }
            if (wet && d->var == 0) world_set_block(x, y, z, B_FARMLAND + 1);
            else if (!wet && d->var == 1) world_set_block(x, y, z, B_FARMLAND);
            else if (!wet && BLOCKS[above].mesh != MESH_CROP && rng_int(r, 0, 3) == 0) world_set_block(x, y, z, B_DIRT);
        } else if (d->mesh == MESH_CROP || d->base == B_COCOA) {
            int light = MAXI(L_SKY(c->light[y + 1 < WORLD_H ? y : y][lz][lx]), L_R(c->light[y][lz][lx]));
            bool moist = BLOCKS[c->blocks[y - 1][lz][lx]].base == B_FARMLAND && BLOCKS[c->blocks[y - 1][lz][lx]].var == 1;
            if ((light >= 9 || d->base == B_NETHER_WART_CROP) && rng_int(r, 0, moist ? 2 : 5) == 0) world_grow(iv3(x, y, z), r, false);
        } else if (d->key && (strstr(d->key, "_sapling") || d->base == B_MANGROVE_PROPAGULE)) {
            if (rng_int(r, 0, 6) == 0 && L_SKY(c->light[y][lz][lx]) >= 9) world_grow(iv3(x, y, z), r, false);
        } else if (d->base == B_SUGAR_CANE || d->base == B_CACTUS || d->base == B_BAMBOO || d->base == B_KELP) {
            if (rng_int(r, 0, 3) == 0 && BLOCKS[world_get_block(x, y + 1, z)].base != d->base) world_grow(iv3(x, y, z), r, false);
        } else if (block_is_leaves(b) && d->var == 0 && rng_int(r, 0, 3) == 0) {   // natural leaves decay when no log is near
            if (!leaves_attached(x, y, z)) {
                world_set_block(x, y, z, B_AIR);
                game_hook_block_broken_naturally(iv3(x, y, z), b);
            }
        } else if (d->base == B_LAVA && rule(GR_DO_FIRE_TICK)) {   // lava ignites what burns around it
            int tries = rng_int(r, 0, 2);
            if (tries > 0) {
                ivec3 q = iv3(x, y, z);
                for (int j = 0; j < tries; j++) {
                    q.x += rng_int(r, -1, 1); q.y += rng_int(r, 0, 1); q.z += rng_int(r, -1, 1);
                    if (!world_loaded(q.x, q.z)) break;
                    Block nb = world_get_block(q.x, q.y, q.z);
                    if (block_is_air(nb)) {
                        bool fuel = false;
                        for (int f = 0; f < 6 && !fuel; f++) fuel = block_flammability(world_get_block(q.x + FACE_DIR[f].x, q.y + FACE_DIR[f].y, q.z + FACE_DIR[f].z)) > 0;
                        if (fuel) { world_set_block(q.x, q.y, q.z, B_FIRE); break; }
                    } else if (BLOCKS[nb].solid) break;
                }
            } else {
                for (int j = 0; j < 3; j++) {
                    int nx = x + rng_int(r, -1, 1), nz = z + rng_int(r, -1, 1);
                    if (!world_loaded(nx, nz)) break;
                    if (block_is_air(world_get_block(nx, y + 1, nz)) && block_flammability(world_get_block(nx, y, nz)) > 0) { world_set_block(nx, y + 1, nz, B_FIRE); break; }
                }
            }
        } else if (b == B_ICE && L_R(c->light[y][lz][lx]) > 11) {
            world_set_block(x, y, z, B_WATER);
        } else if (b == B_FIRE) {
            world_schedule_update(iv3(x, y, z), 1);
        }
    }
}

static u16 wget_any(int x, int y, int z) { return world_get_block(x, y, z); }
static int cmp_ivec3(const void *a, const void *b) {
    const ivec3 *p = a, *q = b;
    if (p->x != q->x) return p->x < q->x ? -1 : 1;
    if (p->z != q->z) return p->z < q->z ? -1 : 1;
    return p->y < q->y ? -1 : p->y > q->y;
}
void world_tick(void) {
    World *w = &g_world;
    w->time++;
    if (!rule(GR_DO_DAYLIGHT_CYCLE)) w->day_shift--;   // keep the time of day still
    // collect the due updates, drop duplicates (redstone notifies wide areas), then run them
    static ivec3 *due; static int cap_due;
    int nd = 0;
    for (int i = 0; i < w->nupdates;) {
        if (w->updates[i].due <= w->time) {
            if (nd == cap_due) { cap_due = cap_due ? cap_due * 2 : 256; due = xrealloc(due, sizeof(ivec3) * cap_due); }
            due[nd++] = w->updates[i].pos;
            updates_remove(w, i);
        } else i++;
    }
    if (nd > 1) qsort(due, nd, sizeof(ivec3), cmp_ivec3);
    if (nd > 2000 && getenv("UC_DEBUG_UPDATES")   /* diagnostic: log unusually busy ticks */) { LOG_INFO("DBG %d updates, %d pending", nd, w->nupdates); for (int k = 0; k < nd; k += nd / 6) { Block bb = world_get_block(due[k].x, due[k].y, due[k].z); LOG_INFO("DBG  %d %d %d %s:%d", due[k].x, due[k].y, due[k].z, BLOCKS[bb].key ? BLOCKS[bb].key : "air", BLOCKS[bb].var); } }
    for (int i = 0; i < nd; i++) {
        if (i > 0 && iv3_eq(due[i], due[i - 1])) continue;
        block_update(due[i]);
    }
    redstone_tick();
    static Rng r = {77};
    if (w->time % MC_TICK_DIV) return;   // random ticks run at Minecraft's 20 ticks per second
    int sd = w->sim_dist > 0 ? w->sim_dist : 8;
    for (int i = 0; i < COL_SLOTS * COL_SLOTS; i++) {
        Column *c = w->slots[i];
        if (c && c->state == COL_READY && abs(c->cx - w->sim_cx) <= sd && abs(c->cz - w->sim_cz) <= sd) random_tick(c, &r);   // only inside the simulation distance
    }
}

// ======================================================================== shapes, selection, collision
bool block_connects(Block self, Block o) {
    const BlockDef *s = &BLOCKS[self], *d = &BLOCKS[o];
    if (o == B_AIR || d->liquid) return false;
    if (d->opaque && d->mesh == MESH_CUBE) return true;
    if (d->mesh == MESH_CONNECT) {
        bool self_pane = SHAPES[s->shape].b[0][3] - SHAPES[s->shape].b[0][0] <= 2;
        bool o_pane = SHAPES[d->shape].b[0][3] - SHAPES[d->shape].b[0][0] <= 2;
        bool self_wall = s->tool == TOOL_PICKAXE && !self_pane && SHAPES[s->shape].b[0][3] - SHAPES[s->shape].b[0][0] >= 8;
        bool o_wall = d->tool == TOOL_PICKAXE && !o_pane && SHAPES[d->shape].b[0][3] - SHAPES[d->shape].b[0][0] >= 8;
        if (self_pane || o_pane) return self_pane == o_pane;
        if (self_wall || o_wall) return true;
        return (s->tool == TOOL_AXE) == (d->tool == TOOL_AXE);   // wood fences vs nether brick fence
    }
    if (d->place == PL_GATE) return true;
    return false;
}

static int add_box(AABB *out, int n, int max, int x, int y, int z, f32 x0, f32 y0, f32 z0, f32 x1, f32 y1, f32 z1) {
    if (n >= max) return n;
    out[n] = (AABB){v3(x + x0, y + y0, z + z0), v3(x + x1, y + y1, z + z1)};
    return n + 1;
}
// rotate a north-arm box (z from 0 toward center) to face index k (0 N, 1 E, 2 S, 3 W)
static void rot_arm(const u8 *b, int k, f32 out[6]) {
    f32 x0 = b[0], y0 = b[1], z0 = b[2], x1 = b[3], y1 = b[4], z1 = b[5];
    for (int i = 0; i < k; i++) { f32 nx0 = 16 - z1, nz0 = x0, nx1 = 16 - z0, nz1 = x1; x0 = nx0; z0 = nz0; x1 = nx1; z1 = nz1; }
    out[0] = fminf(x0, x1); out[1] = y0; out[2] = fminf(z0, z1); out[3] = fmaxf(x0, x1); out[4] = y1; out[5] = fmaxf(z0, z1);
}
int block_connect_mask(int x, int y, int z, Block b) {
    int m = 0;
    for (int k = 0; k < 4; k++) {
        int f = H4_FACE[k];
        if (block_connects(b, world_get_block(x + FACE_DIR[f].x, y, z + FACE_DIR[f].z))) m |= 1 << k;
    }
    return m;
}

int block_boxes(Block b, int x, int y, int z, AABB *out, int max, bool coll) {
    const BlockDef *d = &BLOCKS[b];
    if (coll && (!d->solid || (d->flags & BF_NOCOLLIDE))) return 0;
    switch (d->mesh) {
    case MESH_NONE: return 0;
    case MESH_LIQUID: return coll ? 0 : add_box(out, 0, max, x, y, z, 0, 0, 0, 1, 0.875f, 1);
    case MESH_CROSS: case MESH_CROP: return coll ? 0 : add_box(out, 0, max, x, y, z, 0.15f, 0, 0.15f, 0.85f, d->mesh == MESH_CROP ? 0.25f + 0.75f * (d->var + 1) / d->nvar : 0.8f, 0.85f);
    case MESH_TORCH: return coll ? 0 : add_box(out, 0, max, x, y, z, 0.4f, 0, 0.4f, 0.6f, 0.625f, 0.6f);
    case MESH_CACTUS: return add_box(out, 0, max, x, y, z, 0.0625f, 0, 0.0625f, 0.9375f, coll ? 0.9375f : 1, 0.9375f);
    case MESH_FLAT: case MESH_WIRE: return add_box(out, 0, max, x, y, z, 0, 0, 0, 1, coll ? 0.015f : 0.0625f, 1);
    case MESH_RAIL: { int sh = d->var % (d->base == B_RAIL ? 10 : 6); return coll ? 0 : add_box(out, 0, max, x, y, z, 0, 0, 0, 1, sh >= 2 && sh <= 5 ? 0.5f : 0.125f, 1); }
    case MESH_CONNECT: {
        const BlockShape *s = &SHAPES[d->shape];
        bool tall = coll && (d->flags & BF_FENCELIKE) && s->b[0][3] - s->b[0][0] >= 4;   // fences & walls are 1.5 high to jump over
        f32 top = tall ? 1.5f : 1.0f;
        int n = 0;
        n = add_box(out, n, max, x, y, z, s->b[0][0] / 16.0f, 0, s->b[0][2] / 16.0f, s->b[0][3] / 16.0f, tall ? top : s->b[0][4] / 16.0f, s->b[0][5] / 16.0f);
        int m = block_connect_mask(x, y, z, b);
        for (int k = 0; k < 4; k++) if (m & (1 << k)) {
            f32 a[6]; rot_arm(s->b[1], k, a);
            n = add_box(out, n, max, x, y, z, a[0] / 16, coll ? 0 : a[1] / 16, a[2] / 16, a[3] / 16, tall ? top : a[4] / 16, a[5] / 16);
        }
        return n;
    }
    case MESH_BOXES: {
        const BlockShape *s = &SHAPES[d->shape];
        BlockShape dyn;
        if (d->place == PL_STAIRS) { stair_shape(b, wget_any, x, y, z, &dyn); s = &dyn; }
        if (!coll && s->n > 1) {   // selection: one union box
            f32 mn[3] = {16, 16, 16}, mx[3] = {0, 0, 0};
            for (int i = 0; i < s->n; i++) for (int k = 0; k < 3; k++) { mn[k] = fminf(mn[k], s->b[i][k]); mx[k] = fmaxf(mx[k], s->b[i][k + 3]); }
            return add_box(out, 0, max, x, y, z, mn[0] / 16, mn[1] / 16, mn[2] / 16, mx[0] / 16, mx[1] / 16, mx[2] / 16);
        }
        int n = 0;
        for (int i = 0; i < s->n; i++) {
            const u8 *q = s->b[i];
            n = add_box(out, n, max, x, y, z, q[0] / 16.0f, q[1] / 16.0f, q[2] / 16.0f, q[3] / 16.0f, MAXI(q[4], q[1] + 1) / 16.0f, q[5] / 16.0f);
        }
        return n;
    }
    default: return add_box(out, 0, max, x, y, z, 0, 0, 0, 1, 1, 1);
    }
}

bool world_block_selection(ivec3 p, u16 b, AABB *out) {
    AABB boxes[5];
    int n = block_boxes(b, p.x, p.y, p.z, boxes, 5, false);
    if (!n) return false;
    *out = boxes[0];
    for (int i = 1; i < n; i++) { out->min = v3_min(out->min, boxes[i].min); out->max = v3_max(out->max, boxes[i].max); }
    return true;
}

RayHit world_raycast(vec3 o, vec3 d, f32 maxd, bool liquids) {
    RayHit h = {0};
    d = v3_norm(d);
    ivec3 p = v3_floor(o), prev = p;
    int step[3] = {d.x > 0 ? 1 : -1, d.y > 0 ? 1 : -1, d.z > 0 ? 1 : -1};
    f32 dv[3] = {d.x, d.y, d.z}, ov[3] = {o.x, o.y, o.z};
    int pv[3] = {p.x, p.y, p.z};
    f32 tmax[3], tdelta[3];
    for (int i = 0; i < 3; i++) {
        if (fabsf(dv[i]) < 1e-9f) { tmax[i] = 1e30f; tdelta[i] = 1e30f; continue; }
        f32 next = step[i] > 0 ? (f32)(pv[i] + 1) : (f32)pv[i];
        tmax[i] = (next - ov[i]) / dv[i];
        tdelta[i] = fabsf(1.0f / dv[i]);
    }
    f32 t = 0; int face = -1;
    for (int it = 0; it < 256 && t <= maxd; it++) {
        p = iv3(pv[0], pv[1], pv[2]);
        u16 b = world_get_block(p.x, p.y, p.z);
        if (b != B_AIR && (liquids || !BLOCKS[b].liquid)) {
            AABB boxes[8];
            int n = block_boxes(b, p.x, p.y, p.z, boxes, 8, false);
            f32 best = 1e30f;
            for (int i = 0; i < n; i++) { f32 tt = aabb_ray(boxes[i], o, d); if (tt >= 0 && tt < best) best = tt; }
            if (best <= maxd) {
                h.hit = true; h.pos = p; h.dist = best; h.block = b;
                h.point = v3_add(o, v3_scale(d, best));
                // which face of the hit box: compare the hit point against the box
                h.face = face >= 0 ? face : FACE_U;
                for (int i = 0; i < n; i++) {
                    AABB bx = boxes[i]; vec3 q = h.point; const f32 e = 1e-3f;
                    if (q.x < bx.min.x - e || q.x > bx.max.x + e || q.y < bx.min.y - e || q.y > bx.max.y + e || q.z < bx.min.z - e || q.z > bx.max.z + e) continue;
                    if (fabsf(q.y - bx.max.y) < e && d.y < 0) h.face = FACE_U; else if (fabsf(q.y - bx.min.y) < e && d.y > 0) h.face = FACE_D;
                    else if (fabsf(q.x - bx.min.x) < e && d.x > 0) h.face = FACE_W; else if (fabsf(q.x - bx.max.x) < e && d.x < 0) h.face = FACE_E;
                    else if (fabsf(q.z - bx.min.z) < e && d.z > 0) h.face = FACE_N; else if (fabsf(q.z - bx.max.z) < e && d.z < 0) h.face = FACE_S;
                    break;
                }
                h.prev = iv3(p.x + FACE_DIR[h.face].x, p.y + FACE_DIR[h.face].y, p.z + FACE_DIR[h.face].z);
                (void)prev;
                return h;
            }
        }
        prev = p;
        int ax = tmax[0] < tmax[1] ? (tmax[0] < tmax[2] ? 0 : 2) : (tmax[1] < tmax[2] ? 1 : 2);
        t = tmax[ax];
        tmax[ax] += tdelta[ax];
        pv[ax] += step[ax];
        face = ax == 0 ? (step[0] > 0 ? FACE_W : FACE_E) : ax == 1 ? (step[1] > 0 ? FACE_D : FACE_U) : (step[2] > 0 ? FACE_N : FACE_S);
    }
    return h;
}

int world_collect_boxes(AABB a, AABB *out, int max) {
    int n = 0;
    int x0 = (int)floorf(a.min.x), x1 = (int)floorf(a.max.x), y0 = (int)floorf(a.min.y) - 1, y1 = (int)floorf(a.max.y);
    int z0 = (int)floorf(a.min.z), z1 = (int)floorf(a.max.z);
    for (int y = y0; y <= y1; y++) for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++) {
        u16 b = world_get_block_or(x, y, z, B_BEDROCK);  // unloaded terrain is solid: never fall through the world
        if (!BLOCKS[b].solid || n >= max) continue;
        n += block_boxes(b, x, y, z, out + n, max - n, true);
    }
    return n;
}

// ======================================================================== rendering
typedef struct { Column *c; int s; f32 d; } DrawItem;
static int cmp_near(const void *a, const void *b) { f32 d = ((const DrawItem *)a)->d - ((const DrawItem *)b)->d; return d < 0 ? -1 : d > 0; }
static int cmp_far(const void *a, const void *b) { return -cmp_near(a, b); }

void world_render(const mat4 *vp, vec3 cam, const Frustum *fr, f32 daylight, vec4 fog, f32 fn, f32 ff, int pass) {
    static DrawItem items[COL_SLOTS * COL_SLOTS * SECTIONS / 4];
    int n = 0;
    int rd = g_world.render_dist;
    for (int i = 0; i < COL_SLOTS * COL_SLOTS; i++) {
        Column *c = g_world.slots[i];
        if (!c || c->state != COL_READY) continue;
        if (abs(c->cx - g_world.center_cx) > rd || abs(c->cz - g_world.center_cz) > rd) continue;
        for (int s = 0; s < SECTIONS; s++) {
            SectionMesh *m = &c->sec[s];
            if (!m->built || (pass == 0 ? m->n_opaque : m->n_trans) == 0) continue;
            vec3 mn = v3(c->cx * 16.0f, s * 16.0f, c->cz * 16.0f), mx = v3_add(mn, v3(16, 16, 16));
            if (!frustum_aabb(fr, mn, mx)) continue;
            vec3 ctr = v3_add(mn, v3(8, 8, 8));
            if ((int)ARRAY_LEN(items) <= n) break;
            items[n++] = (DrawItem){c, s, v3_dist(ctr, cam)};
        }
    }
    qsort(items, n, sizeof(DrawItem), pass == 0 ? cmp_near : cmp_far);
    Shader sh = g_world.chunk_shader;
    glUseProgram(sh.id);
    shader_mat4(sh, "vp", vp);
    shader_float(sh, "daylight", daylight);
    {   vec3 amb = g_world.dim == DIM_NETHER ? v3(0.46f, 0.36f, 0.30f) : g_world.dim == DIM_END ? v3(0.22f, 0.22f, 0.30f) : g_world.dim == DIM_SIFT ? v3(0.30f, 0.29f, 0.24f) : v3(0, 0, 0);
        glUniform3f(shader_loc(sh, "ambient"), amb.x, amb.y, amb.z); }
    shader_vec4(sh, "fog_color", fog);
    shader_float(sh, "fog_near", fn);
    shader_float(sh, "fog_far", ff);
    shader_vec3(sh, "cam", cam);
    shader_float(sh, "anim_frame", (f32)((g_world.time / (TICKRATE / 8)) % 16));
    shader_float(sh, "time", (f32)(g_world.time % 100000) / TICKRATE);
    shader_int(sh, "tex", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_world.atlas.id);
    GLint loc_origin = shader_loc(sh, "origin");
    for (int i = 0; i < n; i++) {
        SectionMesh *m = &items[i].c->sec[items[i].s];
        glUniform3f(loc_origin, items[i].c->cx * 16.0f, items[i].s * 16.0f, items[i].c->cz * 16.0f);
        glBindVertexArray(m->vao);
        if (pass == 0) glDrawElements(GL_TRIANGLES, m->n_opaque * 6, GL_UNSIGNED_INT, 0);
        else glDrawElementsBaseVertex(GL_TRIANGLES, m->n_trans * 6, GL_UNSIGNED_INT, 0, m->n_opaque * 4);
    }
    glBindVertexArray(0);
    if (pass == 0) g_world.sections_drawn = n;
}

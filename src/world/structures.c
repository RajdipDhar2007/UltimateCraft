// Generated structures: villages, pillager outposts, desert pyramids, jungle temples, witch huts,
// igloos, ocean monuments, shipwrecks, ocean ruins, buried treasure, ruined portals, mineshafts,
// dungeons, woodland mansions, ancient cities, trial chambers, the stronghold, nether fortresses,
// bastion remnants and end cities.
//
// Placement follows Minecraft's scheme: the world is cut into regions of `spacing` chunks; each
// region holds at most one start, at a hashed offset in [0, spacing - separation). A column
// being generated (on a worker thread) re-derives the starts of all nearby regions, checks the
// biome rule and runs the deterministic builder, which writes only the blocks inside that column.
// Chests, spawners and mobs are recorded as markers and applied on the main thread when the
// column becomes ready (the column is then marked modified so it is saved, not regenerated).
#include "rules.h"
#include "world.h"
#include "structures.h"
#include "../entity/entity.h"
#include "../entity/vehicle.h"
#include "../entity/villager.h"
#include "../game/player.h"
#include "../game/fx.h"

const StructDef STRUCTS[ST_COUNT] = {
    //                 name                 key                 dim            spacing sep salt      radius
    [ST_VILLAGE] = {"Village", "village", DIM_OVERWORLD, 34, 8, 10387312, 48},
    [ST_OUTPOST] = {"Pillager Outpost", "pillager_outpost", DIM_OVERWORLD, 32, 8, 165745296, 16},
    [ST_DESERT_PYRAMID] = {"Desert Pyramid", "desert_pyramid", DIM_OVERWORLD, 32, 8, 14357617, 14},
    [ST_JUNGLE_TEMPLE] = {"Jungle Temple", "jungle_pyramid", DIM_OVERWORLD, 32, 8, 14357619, 12},
    [ST_WITCH_HUT] = {"Swamp Hut", "swamp_hut", DIM_OVERWORLD, 32, 8, 14357620, 8},
    [ST_IGLOO] = {"Igloo", "igloo", DIM_OVERWORLD, 32, 8, 14357618, 8},
    [ST_MONUMENT] = {"Ocean Monument", "monument", DIM_OVERWORLD, 32, 5, 10387313, 32},
    [ST_SHIPWRECK] = {"Shipwreck", "shipwreck", DIM_OVERWORLD, 24, 4, 165745295, 16},
    [ST_OCEAN_RUIN] = {"Ocean Ruins", "ocean_ruin", DIM_OVERWORLD, 20, 8, 14357621, 8},
    [ST_BURIED_TREASURE] = {"Buried Treasure", "buried_treasure", DIM_OVERWORLD, 16, 6, 10387320, 2},
    [ST_RUINED_PORTAL] = {"Ruined Portal", "ruined_portal", DIM_OVERWORLD, 40, 15, 34222645, 8},
    [ST_MINESHAFT] = {"Mineshaft", "mineshaft", DIM_OVERWORLD, 14, 4, 47821351, 56},
    [ST_MANSION] = {"Woodland Mansion", "mansion", DIM_OVERWORLD, 80, 20, 10387319, 24},
    [ST_ANCIENT_CITY] = {"Ancient City", "ancient_city", DIM_OVERWORLD, 24, 8, 20083232, 28},
    [ST_TRIAL_CHAMBERS] = {"Trial Chambers", "trial_chambers", DIM_OVERWORLD, 34, 12, 94251327, 14},
    [ST_STRONGHOLD] = {"Stronghold", "stronghold", DIM_OVERWORLD, 0, 0, 0, 40},
    [ST_FORTRESS] = {"Hell Fortress", "fortress", DIM_NETHER, 27, 4, 30084232, 56},
    [ST_BASTION] = {"Bastion Remnant", "bastion_remnant", DIM_NETHER, 27, 4, 30084233, 20},
    [ST_RUINED_PORTAL_NETHER] = {"Ruined Portal", "ruined_portal_nether", DIM_NETHER, 25, 10, 34222646, 8},
    [ST_END_CITY] = {"Star Citadel", "end_city", DIM_END, 20, 11, 10387313, 24},
    [ST_UNDERGROUND_CITY] = {"Underground City", "underground_city", DIM_OVERWORLD, 40, 12, 73519841, 64},
    [ST_TEMPLE_DAWN] = {"Temple of Dawn", "temple_of_dawn", DIM_SIFT, 16, 5, 51738211, 10},
    [ST_SPIRE_WINDS] = {"Spire of Winds", "spire_of_winds", DIM_SIFT, 16, 5, 51738213, 8},
    [ST_ARENA_STONE] = {"Arena of Stone", "arena_of_stone", DIM_SIFT, 16, 5, 51738217, 12},
};
int structure_by_key(const char *k) {
    for (int i = 0; i < ST_COUNT; i++) if (!strcmp(STRUCTS[i].key, k) || !strcmp(STRUCTS[i].name, k)) return i;
    if (!strcmp(k, "temple") || !strcmp(k, "jungle_temple")) return ST_JUNGLE_TEMPLE;
    if (!strcmp(k, "witch_hut")) return ST_WITCH_HUT;
    if (!strcmp(k, "outpost")) return ST_OUTPOST;
    if (!strcmp(k, "bastion")) return ST_BASTION;
    if (!strcmp(k, "ocean_monument")) return ST_MONUMENT;
    if (!strcmp(k, "woodland_mansion")) return ST_MANSION;
    if (!strcmp(k, "nether_fortress")) return ST_FORTRESS;
    if (!strcmp(k, "delvari_city") || !strcmp(k, "city") || !strcmp(k, "underhold")) return ST_UNDERGROUND_CITY;
    if (!strcmp(k, "solenne") || !strcmp(k, "dawn_temple")) return ST_TEMPLE_DAWN;
    if (!strcmp(k, "zephyrine") || !strcmp(k, "wind_spire")) return ST_SPIRE_WINDS;
    if (!strcmp(k, "thalor") || !strcmp(k, "stone_arena")) return ST_ARENA_STONE;
    if (!strcmp(k, "dungeon")) return ST_COUNT;   // dungeons are per-column (not locatable)
    return -1;
}

// ------------------------------------------------------------------ biome rules
static bool in_list(int b, const int *l, int n) { for (int i = 0; i < n; i++) if (l[i] == b) return true; return false; }
#define BIOME_IN(b, ...) in_list(b, (const int[]){__VA_ARGS__}, (int)(sizeof((const int[]){__VA_ARGS__}) / sizeof(int)))
static bool is_ocean(int b) { return BIOME_IN(b, BIOME_OCEAN, BIOME_DEEP_OCEAN, BIOME_COLD_OCEAN, BIOME_DEEP_COLD_OCEAN, BIOME_LUKEWARM_OCEAN, BIOME_DEEP_LUKEWARM_OCEAN, BIOME_WARM_OCEAN, BIOME_FROZEN_OCEAN, BIOME_DEEP_FROZEN_OCEAN); }
static bool biome_ok(int t, int b, int h) {
    switch (t) {
    case ST_VILLAGE: return h > SEA_LEVEL && BIOME_IN(b, BIOME_PLAINS, BIOME_SUNFLOWER_PLAINS, BIOME_MEADOW, BIOME_DESERT, BIOME_SAVANNA, BIOME_TAIGA, BIOME_SNOWY_PLAINS);
    case ST_OUTPOST: return h > SEA_LEVEL && BIOME_IN(b, BIOME_PLAINS, BIOME_SUNFLOWER_PLAINS, BIOME_MEADOW, BIOME_DESERT, BIOME_SAVANNA, BIOME_TAIGA, BIOME_SNOWY_PLAINS, BIOME_GROVE, BIOME_CHERRY_GROVE, BIOME_SNOWY_SLOPES);
    case ST_DESERT_PYRAMID: return h > SEA_LEVEL && b == BIOME_DESERT;
    case ST_JUNGLE_TEMPLE: return h > SEA_LEVEL && BIOME_IN(b, BIOME_JUNGLE, BIOME_BAMBOO_JUNGLE);
    case ST_WITCH_HUT: return BIOME_IN(b, BIOME_SWAMP);
    case ST_IGLOO: return h > SEA_LEVEL && BIOME_IN(b, BIOME_SNOWY_PLAINS, BIOME_SNOWY_TAIGA, BIOME_SNOWY_SLOPES);
    case ST_MONUMENT: return BIOME_IN(b, BIOME_DEEP_OCEAN, BIOME_DEEP_COLD_OCEAN, BIOME_DEEP_LUKEWARM_OCEAN, BIOME_DEEP_FROZEN_OCEAN) && h < SEA_LEVEL - 14;
    case ST_SHIPWRECK: return (is_ocean(b) && h < SEA_LEVEL - 4) || b == BIOME_BEACH;
    case ST_OCEAN_RUIN: return is_ocean(b) && h < SEA_LEVEL - 5;
    case ST_BURIED_TREASURE: return BIOME_IN(b, BIOME_BEACH, BIOME_SNOWY_BEACH);
    case ST_RUINED_PORTAL: return true;
    case ST_MINESHAFT: return !is_ocean(b);
    case ST_MANSION: return h > SEA_LEVEL && BIOME_IN(b, BIOME_DARK_FOREST);
    case ST_ANCIENT_CITY: return !is_ocean(b) && h > SEA_LEVEL;
    case ST_TRIAL_CHAMBERS: return !is_ocean(b);
    case ST_UNDERGROUND_CITY: return !is_ocean(b);
    default: return true;
    }
}

// ------------------------------------------------------------------ starts
typedef struct { bool ok; int x, z, y, biome; u64 key; } Start;
static Start region_start(u64 seed, int t, int rx, int rz) {
    const StructDef *d = &STRUCTS[t];
    Start s = {0};
    u64 h = hash3i(seed ^ d->salt, rx, t * 131 + 7, rz);
    int span = MAXI(1, d->spacing - d->sep);
    int chx = rx * d->spacing + (int)(h % (u64)span), chz = rz * d->spacing + (int)((h >> 20) % (u64)span);
    s.x = chx * 16 + 8; s.z = chz * 16 + 8; s.key = h;
    // nether fortresses and bastions share a grid in Minecraft: a region holds one or the other
    if (t == ST_FORTRESS || t == ST_BASTION) {
        Start f = {0};
        u64 hf = hash3i(seed ^ STRUCTS[ST_FORTRESS].salt, rx, ST_FORTRESS * 131 + 7, rz);
        bool fort = (hf >> 40) % 5 < 2;
        if ((t == ST_FORTRESS) != fort) return f;
        if (t == ST_BASTION) { h = hf; s.x = (rx * d->spacing + (int)(hf % (u64)span)) * 16 + 8; s.z = (rz * d->spacing + (int)((hf >> 20) % (u64)span)) * 16 + 8; s.key = hf; }
        else { s.x = (rx * d->spacing + (int)(hf % (u64)span)) * 16 + 8; s.z = (rz * d->spacing + (int)((hf >> 20) % (u64)span)) * 16 + 8; s.key = hf; }
    }
    if (d->dim == DIM_OVERWORLD) {
        int b; int hgt = worldgen_surface(seed, s.x, s.z, &b);
        s.y = hgt; s.biome = b;
        s.ok = biome_ok(t, b, hgt);
        if (t == ST_MINESHAFT || t == ST_TRIAL_CHAMBERS || t == ST_ANCIENT_CITY) s.ok = s.ok && (h >> 33) % 3 != 0;
        if (t == ST_RUINED_PORTAL && is_ocean(b)) s.ok = false;
        if (t == ST_UNDERGROUND_CITY) s.y = MCY(-48);   // the city floor, deep in the deepslate
    } else if (d->dim == DIM_NETHER) {
        s.biome = worldgen_biome(seed, DIM_NETHER, s.x, s.z);
        s.ok = t != ST_BASTION || s.biome != BIOME_BASALT_DELTAS;
        s.y = 64;
    } else if (d->dim == DIM_SIFT) {   // the Luminaries' temples, each in its own domain
        s.biome = worldgen_biome(seed, DIM_SIFT, s.x, s.z);
        int top = worldgen_sift_surface(seed, s.x, s.z);
        if (t == ST_SPIRE_WINDS) { s.ok = s.biome == BIOME_CLOUD_SEA; s.y = 66; }
        else { s.ok = top > 0 && s.biome == (t == ST_TEMPLE_DAWN ? BIOME_GOLDEN_MEADOWS : BIOME_CRYSTAL_GROVE); s.y = top + 1; }
    } else {
        f32 dd = sqrtf((f32)s.x * s.x + (f32)s.z * s.z);
        int lo, hi;
        worldgen_end_column(seed, s.x, s.z, &lo, &hi);
        s.ok = dd > 1000 && hi > lo + 3;
        s.y = hi + 1;
    }
    return s;
}
static inline int fdiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

// ------------------------------------------------------------------ builder context
typedef struct {
    Column *c; int bx, bz;
    u64 seed; int type;
    Rng r;
    int ox, oy, oz;    // structure origin
} SB;
static inline bool inside(SB *b, int x, int z) { return x >= b->bx && x < b->bx + 16 && z >= b->bz && z < b->bz + 16; }
static inline void P(SB *b, int x, int y, int z, u16 blk) {
    if (y < 1 || y >= WORLD_H || !inside(b, x, z)) return;
    b->c->blocks[y][z - b->bz][x - b->bx] = blk;
}
static inline u16 GB(SB *b, int x, int y, int z) { if (y < 0 || y >= WORLD_H || !inside(b, x, z)) return B_AIR; return b->c->blocks[y][z - b->bz][x - b->bx]; }
static inline void PS(SB *b, int x, int y, int z, u16 blk) {   // only into air / plants / fluids
    u16 cur = GB(b, x, y, z);
    if (block_is_air(cur) || BLOCKS[cur].replaceable || BLOCKS[cur].mesh == MESH_CROSS || BLOCKS[cur].liquid) P(b, x, y, z, blk);
}
static void FILL(SB *b, int x0, int y0, int z0, int x1, int y1, int z1, u16 blk) {
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (z0 > z1) { int t = z0; z0 = z1; z1 = t; }
    int ax = MAXI(x0, b->bx), bx = MINI(x1, b->bx + 15), az = MAXI(z0, b->bz), bz = MINI(z1, b->bz + 15);
    for (int y = MAXI(1, y0); y <= MINI(WORLD_H - 1, y1); y++) for (int z = az; z <= bz; z++) for (int x = ax; x <= bx; x++) b->c->blocks[y][z - b->bz][x - b->bx] = blk;
}
static void SHELL(SB *b, int x0, int y0, int z0, int x1, int y1, int z1, u16 wall, u16 in) {
    FILL(b, x0, y0, z0, x1, y1, z1, wall);
    FILL(b, x0 + 1, y0 + 1, z0 + 1, x1 - 1, y1 - 1, z1 - 1, in);
}
static int GROUND(SB *b, int x, int z) { return worldgen_surface(b->seed, x, z, NULL); }
static void MARK(SB *b, int x, int y, int z, int kind, int arg) {
    if (!inside(b, x, z) || y < 0 || y >= WORLD_H) return;
    Column *c = b->c;
    if (c->nmarkers >= MAX_MARKERS) return;
    StructMarker *m = &c->markers[c->nmarkers++];
    m->lx = (u8)(x - b->bx); m->lz = (u8)(z - b->bz); m->y = (u16)y; m->kind = (u8)kind; m->arg = (u16)arg;
}
// container block + loot marker
static void CHEST(SB *b, int x, int y, int z, int loot) { P(b, x, y, z, B_CHEST); MARK(b, x, y, z, MK_LOOT, loot); }
static void BARREL(SB *b, int x, int y, int z, int loot) { P(b, x, y, z, B_BARREL + 3); MARK(b, x, y, z, MK_LOOT, loot); }
static void SPAWNER(SB *b, int x, int y, int z, int mob) { P(b, x, y, z, B_SPAWNER); MARK(b, x, y, z, MK_SPAWNER, mob); }
static void MOB(SB *b, int x, int y, int z, int mob) { MARK(b, x, y, z, MK_MOB, mob); }
static void foundation(SB *b, int x0, int z0, int x1, int z1, int top, u16 blk) {   // fill from the ground up to top-1
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (z0 > z1) { int t = z0; z0 = z1; z1 = t; }
    for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++) {
        if (!inside(b, x, z)) continue;
        int g = GROUND(b, x, z);
        for (int y = MINI(g, top - 1); y < top; y++) P(b, x, y, z, blk);
        for (int y = top - 1; y > 1 && y > top - 12; y--) { u16 cur = GB(b, x, y, z); if (BLOCKS[cur].solid && !block_is_leaves(cur)) break; P(b, x, y, z, blk); }
    }
}
// remove trees / plants above a footprint (Minecraft clears vegetation over structures)
static void clear_above(SB *b, int x0, int z0, int x1, int z1, int y) {
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (z0 > z1) { int t = z0; z0 = z1; z1 = t; }
    for (int z = z0 - 2; z <= z1 + 2; z++) for (int x = x0 - 2; x <= x1 + 2; x++) {
        if (!inside(b, x, z)) continue;
        for (int yy = MAXI(1, y); yy < WORLD_H; yy++) {
            u16 c = GB(b, x, yy, z);
            if (block_is_leaves(c) || block_is_log(c) || BLOCKS[c].mesh == MESH_CROSS || c == B_VINE || BLOCKS[c].base == B_VINE || BLOCKS[c].base == B_SNOW) P(b, x, yy, z, B_AIR);
        }
    }
}
static int rnd(SB *b, int lo, int hi) { return rng_int(&b->r, lo, hi); }

// orientation helper: local (lx, lz) with lz pointing inward from a door wall that faces `out` (h4)
typedef struct { int ox, oz, ax, az, zx, zz, out; } Frame;
static Frame frame(int doorx, int doorz, int out, int width) {
    Frame f;
    int ox = FACE_DIR[H4_FACE[out]].x, oz = FACE_DIR[H4_FACE[out]].z;
    f.zx = -ox; f.zz = -oz;                // inward
    f.ax = f.zz; f.az = -f.zx;            // across
    f.ox = doorx - (width / 2) * f.ax; f.oz = doorz - (width / 2) * f.az;
    f.out = out;
    return f;
}
static inline int FX(const Frame *f, int lx, int lz) { return f->ox + lx * f->ax + lz * f->zx; }
static inline int FZ(const Frame *f, int lx, int lz) { return f->oz + lx * f->az + lz * f->zz; }
static void LP(SB *b, const Frame *f, int lx, int y, int lz, u16 blk) { P(b, FX(f, lx, lz), y, FZ(f, lx, lz), blk); }
static void LFILL(SB *b, const Frame *f, int x0, int y0, int z0, int x1, int y1, int z1, u16 blk) {
    for (int lz = z0; lz <= z1; lz++) for (int lx = x0; lx <= x1; lx++) for (int y = y0; y <= y1; y++) LP(b, f, lx, y, lz, blk);
}
static u16 door_state(Block door, int out, bool upper) { return (u16)(door + ((out + 2) & 3) * 4 + (upper ? 2 : 0)); }

#include "structures_build.inc"

// ------------------------------------------------------------------ column generation (worker threads)
void structures_generate(Column *c, u64 seed, int dim) {
    c->nmarkers = 0;
    int bx = c->cx * 16, bz = c->cz * 16;
    for (int t = 0; t < ST_COUNT; t++) {
        const StructDef *d = &STRUCTS[t];
        if (d->dim != dim || d->spacing == 0) continue;
        int R = d->radius;
        int rx0 = fdiv(fdiv(bx - R, 16), d->spacing), rx1 = fdiv(fdiv(bx + 15 + R, 16), d->spacing);
        int rz0 = fdiv(fdiv(bz - R, 16), d->spacing), rz1 = fdiv(fdiv(bz + 15 + R, 16), d->spacing);
        for (int rz = rz0; rz <= rz1; rz++) for (int rx = rx0; rx <= rx1; rx++) {
            // cheap reject before the biome check
            u64 h = hash3i(seed ^ d->salt, rx, t * 131 + 7, rz);
            int span = MAXI(1, d->spacing - d->sep);
            int sx = (rx * d->spacing + (int)(h % (u64)span)) * 16 + 8, sz = (rz * d->spacing + (int)((h >> 20) % (u64)span)) * 16 + 8;
            if (t != ST_FORTRESS && t != ST_BASTION && (sx + R < bx || sx - R > bx + 15 || sz + R < bz || sz - R > bz + 15)) continue;
            Start s = region_start(seed, t, rx, rz);
            if (!s.ok || s.x + R < bx || s.x - R > bx + 15 || s.z + R < bz || s.z - R > bz + 15) continue;
            SB b = {c, bx, bz, seed, t, {s.key | 1}, s.x, s.y, s.z};
            build_structure(&b, &s);
        }
    }
    if (dim == DIM_OVERWORLD) {
        SB b = {c, bx, bz, seed, ST_STRONGHOLD, {hash3i(seed, c->cx, 777, c->cz) | 1}, 0, 0, 0};
        build_stronghold_extras(&b);
        b.type = ST_COUNT; b.r.s = hash3i(seed, c->cx, 9191, c->cz) | 1;
        build_dungeon(&b);
    }
}

// ------------------------------------------------------------------ queries (main thread, cached)
typedef struct { u64 key; Start s; bool used; } CacheSlot;
#define CACHE_N 4096
static CacheSlot cache[CACHE_N];
static Start cached_start(u64 seed, int t, int rx, int rz) {
    u64 key = hash3i(seed + (u64)t, rx, 0x5151, rz);
    CacheSlot *c = &cache[key & (CACHE_N - 1)];
    if (c->used && c->key == key) return c->s;
    c->used = true; c->key = key; c->s = region_start(seed, t, rx, rz);
    return c->s;
}
void structures_reset(void) { memset(cache, 0, sizeof cache); }

// structure whose bounding box contains the position (-1 none); y is checked against a generous band
int structure_at(int x, int y, int z) {
    u64 seed = g_world.seed;
    for (int t = 0; t < ST_COUNT; t++) {
        const StructDef *d = &STRUCTS[t];
        if (d->dim != g_world.dim || !d->spacing) continue;
        int rx = fdiv(fdiv(x, 16), d->spacing), rz = fdiv(fdiv(z, 16), d->spacing);
        for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) {
            Start s = cached_start(seed, t, rx + dx, rz + dz);
            if (!s.ok) continue;
            int R = d->radius;
            if (abs(x - s.x) > R || abs(z - s.z) > R) continue;
            int lo = s.y - 24, hi = s.y + 40;
            if (t == ST_MONUMENT) { lo = SEA_LEVEL - 40; hi = SEA_LEVEL + 2; }
            if (t == ST_FORTRESS) { lo = 40; hi = 90; }
            if (t == ST_UNDERGROUND_CITY) { lo = MCY(-62); hi = MCY(-12); }
            if (y < lo || y > hi) continue;
            return t;
        }
    }
    return -1;
}
bool structure_locate(int t, int fx, int fz, ivec3 *out) {
    if (t < 0 || t >= ST_COUNT) return false;
    const StructDef *d = &STRUCTS[t];
    if (t == ST_STRONGHOLD) { *out = worldgen_stronghold(g_world.seed); return g_world.dim == DIM_OVERWORLD; }
    if (d->dim != g_world.dim) return false;
    int rx0 = fdiv(fdiv(fx, 16), d->spacing), rz0 = fdiv(fdiv(fz, 16), d->spacing);
    f32 best = 1e30f; bool found = false;
    for (int r = 0; r <= 24 && !found; r++) {
        for (int dz = -r; dz <= r; dz++) for (int dx = -r; dx <= r; dx++) {
            if (abs(dx) != r && abs(dz) != r) continue;
            Start s = cached_start(g_world.seed, t, rx0 + dx, rz0 + dz);
            if (!s.ok) continue;
            f32 dd = (f32)(s.x - fx) * (s.x - fx) + (f32)(s.z - fz) * (s.z - fz);
            if (dd < best) { best = dd; *out = iv3(s.x, s.y, s.z); found = true; }
        }
    }
    return found;
}
// the mob that should spawn at a structure instead of the biome list (-1: use the biome)
int structure_spawn_mob(int st, bool water) {
    switch (st) {
    case ST_FORTRESS: { int r = irand(0, 30); return r < 10 ? MOB_BLAZE : r < 18 ? MOB_WITHER_SKELETON : r < 23 ? MOB_ZOMBIFIED_PIGLIN : r < 25 ? MOB_SKELETON : r < 28 ? MOB_GHOST : MOB_MAGMA_CUBE; }   // ghosts haunt Hell fortresses
    case ST_MONUMENT: return water ? MOB_GUARDIAN : -1;
    case ST_WITCH_HUT: return MOB_WITCH;
    case ST_OUTPOST: return MOB_PILLAGER;
    case ST_UNDERGROUND_CITY: return irand(0, 2) == 0 ? MOB_HOLLOW_SENTINEL : -1;
    default: return -1;
    }
}

// ------------------------------------------------------------------ loot
typedef struct { const char *item; u8 min, max, weight; } Loot;
typedef struct { const Loot *e; int n; u8 rolls_min, rolls_max; } LootTable;
#define LT(arr, a, b) {arr, (int)ARRAY_LEN(arr), a, b}
static const Loot L_VILLAGE[] = {{"bread", 1, 4, 10}, {"apple", 1, 5, 8}, {"emerald", 1, 4, 4}, {"wheat", 3, 8, 8}, {"potato", 2, 6, 6}, {"carrot", 2, 6, 6}, {"oak_sapling", 1, 3, 4}, {"iron_ingot", 1, 3, 3}, {"book", 1, 2, 3}, {"feather", 1, 3, 3}, {"paper", 1, 4, 3}};
static const Loot L_WEAPONSMITH[] = {{"iron_ingot", 1, 5, 10}, {"bread", 1, 3, 15}, {"apple", 1, 3, 15}, {"iron_pickaxe", 1, 1, 5}, {"iron_sword", 1, 1, 5}, {"iron_chestplate", 1, 1, 5}, {"iron_helmet", 1, 1, 5}, {"iron_leggings", 1, 1, 5}, {"iron_boots", 1, 1, 5}, {"obsidian", 3, 7, 5}, {"oak_sapling", 3, 7, 5}, {"saddle", 1, 1, 3}, {"iron_horse_armor", 1, 1, 1}, {"golden_horse_armor", 1, 1, 1}, {"diamond", 1, 3, 3}, {"diamond_horse_armor", 1, 1, 1}};
static const Loot L_DESERT[] = {{"bone", 4, 6, 25}, {"rotten_flesh", 3, 7, 25}, {"spider_eye", 1, 3, 25}, {"gunpowder", 1, 8, 10}, {"sand", 1, 8, 10}, {"string", 1, 8, 10}, {"gold_ingot", 2, 7, 15}, {"iron_ingot", 1, 5, 15}, {"emerald", 1, 3, 15}, {"diamond", 1, 3, 5}, {"saddle", 1, 1, 20}, {"iron_horse_armor", 1, 1, 15}, {"golden_horse_armor", 1, 1, 10}, {"golden_apple", 1, 1, 20}, {"enchanted_golden_apple", 1, 1, 2}, {"*enchanted_book", 1, 1, 20}};
static const Loot L_JUNGLE[] = {{"diamond", 1, 3, 3}, {"iron_ingot", 1, 5, 10}, {"gold_ingot", 2, 7, 15}, {"emerald", 1, 3, 2}, {"bone", 4, 6, 20}, {"rotten_flesh", 3, 7, 16}, {"saddle", 1, 1, 3}, {"iron_horse_armor", 1, 1, 1}, {"golden_horse_armor", 1, 1, 1}, {"diamond_horse_armor", 1, 1, 1}, {"*enchanted_book", 1, 1, 1}, {"bamboo", 1, 3, 15}};
static const Loot L_IGLOO[] = {{"apple", 1, 3, 15}, {"coal", 1, 4, 15}, {"gold_nugget", 1, 3, 10}, {"stone_axe", 1, 1, 2}, {"rotten_flesh", 1, 1, 10}, {"emerald", 1, 1, 1}, {"wheat", 2, 3, 10}, {"golden_apple", 1, 1, 30}};
static const Loot L_SHIP_SUPPLY[] = {{"paper", 1, 12, 8}, {"potato", 2, 6, 7}, {"poisonous_potato", 2, 6, 7}, {"carrot", 4, 8, 7}, {"wheat", 8, 21, 7}, {"coal", 2, 8, 6}, {"rotten_flesh", 5, 24, 5}, {"pumpkin", 1, 3, 2}, {"bamboo", 1, 3, 2}, {"gunpowder", 1, 5, 3}, {"tnt", 1, 2, 1}, {"*leather_helmet", 1, 1, 3}, {"*leather_chestplate", 1, 1, 3}};
static const Loot L_SHIP_TREASURE[] = {{"iron_ingot", 1, 5, 90}, {"gold_ingot", 1, 5, 10}, {"emerald", 1, 5, 40}, {"diamond", 1, 1, 5}, {"experience_bottle", 1, 1, 5}, {"iron_nugget", 1, 10, 50}, {"gold_nugget", 1, 10, 10}, {"lapis_lazuli", 1, 10, 20}};
static const Loot L_SHIP_MAP[] = {{"map", 1, 1, 10}, {"compass", 1, 1, 10}, {"paper", 1, 10, 20}, {"feather", 1, 5, 10}, {"book", 1, 5, 5}, {"clock", 1, 1, 1}};
static const Loot L_BURIED[] = {{"heart_of_the_sea", 1, 1, 100}, {"iron_ingot", 1, 4, 20}, {"gold_ingot", 1, 4, 10}, {"tnt", 1, 2, 5}, {"emerald", 4, 8, 5}, {"diamond", 1, 2, 5}, {"prismarine_crystals", 1, 5, 5}, {"cooked_cod", 2, 4, 10}, {"cooked_salmon", 2, 4, 10}, {"iron_sword", 1, 1, 5}};
static const Loot L_PORTAL[] = {{"obsidian", 1, 2, 40}, {"flint", 1, 4, 40}, {"iron_nugget", 9, 18, 40}, {"flint_and_steel", 1, 1, 40}, {"fire_charge", 1, 1, 40}, {"golden_apple", 1, 1, 15}, {"gold_nugget", 4, 24, 15}, {"golden_sword", 1, 1, 15}, {"golden_axe", 1, 1, 15}, {"golden_hoe", 1, 1, 15}, {"golden_pickaxe", 1, 1, 15}, {"golden_boots", 1, 1, 15}, {"golden_helmet", 1, 1, 15}, {"glistering_melon_slice", 4, 12, 5}, {"golden_carrot", 4, 12, 5}, {"clock", 1, 5, 5}, {"light_weighted_pressure_plate", 1, 5, 5}, {"golden_horse_armor", 1, 1, 5}, {"gold_block", 1, 2, 1}, {"bell", 1, 1, 1}, {"enchanted_golden_apple", 1, 1, 1}};
static const Loot L_MINESHAFT[] = {{"rail", 4, 8, 20}, {"torch", 1, 16, 15}, {"bread", 1, 3, 15}, {"coal", 3, 8, 10}, {"iron_ingot", 1, 5, 10}, {"gold_ingot", 1, 3, 5}, {"redstone", 4, 9, 5}, {"lapis_lazuli", 4, 9, 5}, {"diamond", 1, 2, 3}, {"melon_seeds", 2, 4, 10}, {"pumpkin_seeds", 2, 4, 10}, {"beetroot_seeds", 2, 4, 10}, {"name_tag", 1, 1, 30}, {"golden_apple", 1, 1, 20}, {"iron_pickaxe", 1, 1, 5}, {"powered_rail", 1, 4, 5}, {"activator_rail", 1, 4, 5}, {"detector_rail", 1, 4, 5}, {"glow_berries", 3, 6, 15}, {"*enchanted_book", 1, 1, 10}};
static const Loot L_DUNGEON[] = {{"saddle", 1, 1, 20}, {"golden_apple", 1, 1, 15}, {"enchanted_golden_apple", 1, 1, 2}, {"music_disc_13", 1, 1, 15}, {"music_disc_cat", 1, 1, 15}, {"name_tag", 1, 1, 20}, {"golden_horse_armor", 1, 1, 10}, {"iron_horse_armor", 1, 1, 15}, {"diamond_horse_armor", 1, 1, 5}, {"*enchanted_book", 1, 1, 10}, {"iron_ingot", 1, 4, 10}, {"gold_ingot", 1, 4, 5}, {"bread", 1, 1, 20}, {"wheat", 1, 4, 20}, {"bucket", 1, 1, 10}, {"redstone", 1, 4, 15}, {"coal", 1, 4, 15}, {"melon_seeds", 2, 4, 10}, {"bone", 1, 8, 10}, {"gunpowder", 1, 8, 10}, {"rotten_flesh", 1, 8, 10}, {"string", 1, 8, 10}};
static const Loot L_STRONGHOLD[] = {{"ender_pearl", 1, 1, 10}, {"diamond", 1, 3, 3}, {"iron_ingot", 1, 5, 10}, {"gold_ingot", 1, 3, 5}, {"redstone", 4, 9, 5}, {"bread", 1, 3, 15}, {"apple", 1, 3, 15}, {"iron_pickaxe", 1, 1, 5}, {"iron_sword", 1, 1, 5}, {"*enchanted_book", 1, 1, 6}, {"book", 1, 3, 20}, {"paper", 2, 7, 20}, {"compass", 1, 1, 1}, {"map", 1, 1, 1}};
static const Loot L_FORTRESS[] = {{"diamond", 1, 3, 5}, {"iron_ingot", 1, 5, 5}, {"gold_ingot", 1, 3, 15}, {"golden_sword", 1, 1, 5}, {"golden_chestplate", 1, 1, 5}, {"flint_and_steel", 1, 1, 5}, {"nether_wart", 3, 7, 5}, {"saddle", 1, 1, 10}, {"golden_horse_armor", 1, 1, 8}, {"iron_horse_armor", 1, 1, 5}, {"diamond_horse_armor", 1, 1, 3}, {"obsidian", 2, 4, 2}};
static const Loot L_BASTION[] = {{"gold_block", 2, 5, 10}, {"gold_ingot", 3, 9, 15}, {"gilded_blackstone", 5, 15, 10}, {"ancient_debris", 1, 2, 6}, {"netherite_scrap", 1, 1, 4}, {"netherite_upgrade_smithing_template", 1, 1, 8}, {"snout_banner_pattern", 1, 1, 4}, {"*diamond_sword", 1, 1, 6}, {"*diamond_chestplate", 1, 1, 6}, {"*golden_axe", 1, 1, 6}, {"crying_obsidian", 3, 8, 8}, {"magma_cream", 2, 6, 8}, {"string", 4, 6, 6}, {"cooked_porkchop", 6, 17, 6}, {"music_disc_pigstep", 1, 1, 2}, {"enchanted_golden_apple", 1, 1, 2}};
static const Loot L_END_CITY[] = {{"diamond", 2, 7, 5}, {"iron_ingot", 4, 8, 10}, {"gold_ingot", 2, 7, 15}, {"emerald", 2, 6, 2}, {"beetroot_seeds", 1, 10, 5}, {"saddle", 1, 1, 3}, {"iron_horse_armor", 1, 1, 1}, {"golden_horse_armor", 1, 1, 1}, {"diamond_horse_armor", 1, 1, 1}, {"*diamond_sword", 1, 1, 3}, {"*diamond_boots", 1, 1, 3}, {"*diamond_chestplate", 1, 1, 3}, {"*diamond_pickaxe", 1, 1, 3}, {"*iron_sword", 1, 1, 3}, {"spire_armor_trim_smithing_template", 1, 1, 2}};
static const Loot L_END_SHIP[] = {{"elytra", 1, 1, 100}};
static const Loot L_MANSION[] = {{"lead", 1, 1, 20}, {"golden_apple", 1, 1, 15}, {"enchanted_golden_apple", 1, 1, 2}, {"music_disc_13", 1, 1, 15}, {"name_tag", 1, 1, 20}, {"chainmail_chestplate", 1, 1, 10}, {"diamond_hoe", 1, 1, 15}, {"diamond_chestplate", 1, 1, 5}, {"*enchanted_book", 1, 1, 10}, {"iron_ingot", 1, 4, 10}, {"redstone", 1, 4, 15}, {"bone", 1, 8, 10}, {"totem_of_undying", 1, 1, 6}};
static const Loot L_DELVARI[] = {{"delvari_coin", 1, 6, 20}, {"umbral_lamp", 1, 3, 8}, {"glow_berries", 2, 8, 10}, {"iron_ingot", 1, 4, 10}, {"coal", 2, 8, 10}, {"bread", 1, 3, 8}, {"candle", 1, 3, 6}, {"echo_shard", 1, 1, 2}, {"amethyst_shard", 1, 5, 5}, {"*iron_sword", 1, 1, 3}};
static const Loot L_DELVARI_TEMPLE[] = {{"delvari_coin", 4, 12, 15}, {"*enchanted_book", 1, 1, 8}, {"golden_apple", 1, 2, 6}, {"ender_eye", 1, 3, 6}, {"experience_bottle", 2, 6, 8}, {"lapis_lazuli", 3, 10, 8}, {"diamond", 1, 2, 4}, {"runic_delvari_bricks", 2, 6, 6}};
static const Loot L_DELVARI_VAULT[] = {{"delvari_coin", 8, 24, 20}, {"diamond", 1, 4, 10}, {"ender_eye", 2, 4, 10}, {"*umbral_blade", 1, 1, 4}, {"*diamond_chestplate", 1, 1, 4}, {"enchanted_golden_apple", 1, 1, 2}, {"oxygen_canister", 1, 3, 6}, {"gold_block", 1, 2, 6}};
static const Loot L_SIFT[] = {{"soul_essence", 1, 4, 20}, {"golden_apple", 1, 2, 8}, {"radiant_crystal", 1, 4, 10}, {"gold_ingot", 2, 6, 10}, {"divine_ingot", 1, 1, 3}, {"*enchanted_book", 1, 1, 5}, {"experience_bottle", 2, 5, 8}, {"feather", 2, 6, 6}};
static const Loot L_ANCIENT[] = {{"enchanted_golden_apple", 1, 2, 1}, {"music_disc_otherside", 1, 1, 1}, {"compass", 1, 1, 2}, {"sculk_catalyst", 1, 2, 2}, {"name_tag", 1, 1, 2}, {"*diamond_hoe", 1, 1, 2}, {"lead", 1, 1, 2}, {"diamond_horse_armor", 1, 1, 2}, {"saddle", 1, 1, 2}, {"music_disc_13", 1, 1, 2}, {"*diamond_leggings", 1, 1, 2}, {"echo_shard", 1, 3, 4}, {"amethyst_shard", 1, 15, 3}, {"experience_bottle", 1, 3, 3}, {"glow_berries", 1, 15, 3}, {"*iron_leggings", 1, 1, 3}, {"sculk", 4, 10, 3}, {"sculk_sensor", 1, 3, 3}, {"candle", 1, 4, 3}, {"bone", 1, 15, 5}, {"soul_torch", 1, 15, 3}, {"swift_sneak_book", 1, 1, 4}, {"ward_armor_trim_smithing_template", 1, 1, 2}, {"disc_fragment_5", 1, 3, 2}};
static const Loot L_OUTPOST[] = {{"crossbow", 1, 1, 10}, {"wheat", 3, 5, 7}, {"potato", 2, 5, 5}, {"carrot", 3, 5, 5}, {"dark_oak_log", 2, 3, 10}, {"experience_bottle", 1, 1, 7}, {"string", 1, 6, 4}, {"arrow", 2, 7, 4}, {"tripwire_hook", 1, 3, 3}, {"iron_ingot", 1, 3, 3}, {"*enchanted_book", 1, 1, 1}, {"goat_horn", 1, 1, 1}};
static const Loot L_OCEAN_RUIN[] = {{"coal", 1, 4, 10}, {"stone_axe", 1, 1, 2}, {"rotten_flesh", 1, 1, 5}, {"emerald", 1, 1, 1}, {"wheat", 2, 3, 10}, {"golden_apple", 1, 1, 2}, {"*enchanted_book", 1, 1, 1}, {"map", 1, 1, 2}, {"gold_nugget", 1, 3, 5}, {"*fishing_rod", 1, 1, 1}};
static const Loot L_TRIAL[] = {{"emerald", 2, 4, 5}, {"arrow", 2, 8, 4}, {"tipped_arrow", 2, 8, 3}, {"iron_ingot", 1, 2, 4}, {"honey_bottle", 1, 1, 3}, {"ominous_bottle", 1, 1, 2}, {"*enchanted_book", 1, 1, 3}, {"diamond", 1, 2, 2}, {"golden_apple", 1, 1, 2}, {"*crossbow", 1, 1, 2}, {"trial_key", 1, 1, 3}, {"flow_banner_pattern", 1, 1, 1}, {"guster_banner_pattern", 1, 1, 1}, {"heavy_core", 1, 1, 1}};
static const Loot L_HUT[] = {{"glass_bottle", 1, 3, 10}, {"redstone", 1, 3, 10}, {"glowstone_dust", 1, 3, 10}, {"sugar", 1, 3, 10}, {"spider_eye", 1, 2, 10}, {"stick", 1, 4, 10}, {"gunpowder", 1, 2, 10}};
static const LootTable LOOTS[LOOT_COUNT] = {
    [LOOT_VILLAGE] = LT(L_VILLAGE, 3, 7), [LOOT_WEAPONSMITH] = LT(L_WEAPONSMITH, 3, 8), [LOOT_DESERT] = LT(L_DESERT, 2, 6), [LOOT_JUNGLE] = LT(L_JUNGLE, 2, 6),
    [LOOT_IGLOO] = LT(L_IGLOO, 2, 8), [LOOT_SHIP_SUPPLY] = LT(L_SHIP_SUPPLY, 3, 10), [LOOT_SHIP_TREASURE] = LT(L_SHIP_TREASURE, 3, 6), [LOOT_SHIP_MAP] = LT(L_SHIP_MAP, 3, 6),
    [LOOT_BURIED] = LT(L_BURIED, 1, 1), [LOOT_PORTAL] = LT(L_PORTAL, 4, 8), [LOOT_MINESHAFT] = LT(L_MINESHAFT, 3, 6), [LOOT_DUNGEON] = LT(L_DUNGEON, 4, 9),
    [LOOT_STRONGHOLD] = LT(L_STRONGHOLD, 2, 6), [LOOT_FORTRESS] = LT(L_FORTRESS, 2, 5), [LOOT_BASTION] = LT(L_BASTION, 3, 7), [LOOT_END_CITY] = LT(L_END_CITY, 2, 6),
    [LOOT_END_SHIP] = LT(L_END_SHIP, 1, 1), [LOOT_MANSION] = LT(L_MANSION, 2, 6), [LOOT_ANCIENT] = LT(L_ANCIENT, 5, 10), [LOOT_DELVARI] = LT(L_DELVARI, 2, 5), [LOOT_DELVARI_TEMPLE] = LT(L_DELVARI_TEMPLE, 3, 6), [LOOT_DELVARI_VAULT] = LT(L_DELVARI_VAULT, 5, 9), [LOOT_SIFT] = LT(L_SIFT, 3, 6), [LOOT_OUTPOST] = LT(L_OUTPOST, 2, 5),
    [LOOT_OCEAN_RUIN] = LT(L_OCEAN_RUIN, 2, 5), [LOOT_TRIAL] = LT(L_TRIAL, 2, 5), [LOOT_HUT] = LT(L_HUT, 2, 5),
};
static ItemStack loot_item(const Loot *l) {
    const char *key = l->item;
    bool ench = key[0] == '*';
    if (ench) key++;
    int id;
    ItemStack s = {0};
    if (!strcmp(key, "swift_sneak_book")) { s = stack_of(I_ENCHANTED_BOOK, 1); ench_add(&s, ENCH_SWIFT_SNEAK, irand(1, 3)); return s; }
    id = item_by_key(key);
    if (!item_valid(id)) return s;
    s = stack_of(id, irand(l->min, l->max));
    if (s.count > ITEMS[id].max_stack) s.count = (u8)ITEMS[id].max_stack;
    if (ench) {
        if (id == I_ENCHANTED_BOOK) {
            for (int t = 0; t < 40; t++) { int e = irand(1, ENCH_COUNT - 1); if (ENCHANTS[e].curse && irand(0, 3)) continue; ench_add(&s, e, irand(1, ENCHANTS[e].max_level)); break; }
        } else {
            u8 out[MAX_ENCH][2]; memset(out, 0, sizeof out);
            int n = ench_select(id, irand(5, 30), (u32)irand(1, 1 << 30), out, MAX_ENCH);
            for (int i = 0; i < n; i++) ench_add(&s, out[i][0], out[i][1]);
        }
    }
    return s;
}
void loot_fill(ItemStack *slots, int nslots, int table) {
    if (table < 0 || table >= LOOT_COUNT || !LOOTS[table].n) return;
    const LootTable *t = &LOOTS[table];
    int total = 0;
    for (int i = 0; i < t->n; i++) total += t->e[i].weight;
    int rolls = irand(t->rolls_min, t->rolls_max);
    if (table == LOOT_END_SHIP) { slots[13] = stack_of(I_ELYTRA, 1); return; }
    if (table == LOOT_BURIED) { slots[irand(0, nslots - 1)] = stack_of(I_HEART_OF_THE_SEA, 1); rolls = irand(3, 6); }
    for (int k = 0; k < rolls; k++) {
        int r = irand(0, total - 1);
        const Loot *pick = &t->e[0];
        for (int i = 0; i < t->n; i++) { r -= t->e[i].weight; if (r < 0) { pick = &t->e[i]; break; } }
        ItemStack s = loot_item(pick);
        if (stack_empty(s)) continue;
        for (int tries = 0; tries < 8; tries++) { int slot = irand(0, nslots - 1); if (stack_empty(slots[slot])) { slots[slot] = s; break; } }
    }
}

// ------------------------------------------------------------------ markers -> live objects (main thread)
void structures_column_ready(Column *c) {
    if (!c->nmarkers) return;
    int n = c->nmarkers;
    c->nmarkers = 0;
    for (int i = 0; i < n; i++) {
        StructMarker *m = &c->markers[i];
        ivec3 p = iv3(c->cx * 16 + m->lx, m->y, c->cz * 16 + m->lz);
        vec3 at = v3(p.x + 0.5f, (f32)p.y, p.z + 0.5f);
        switch (m->kind) {
        case MK_LOOT: {
            Block b = world_get_block(p.x, p.y, p.z);
            int bet = BLOCKS[b].be;
            if (bet == BE_NONE || world_block_entity(p, false, 0)) break;   // already looted / existing
            BlockEntity *be = world_block_entity(p, true, bet);
            loot_fill(be->slots, bet == BE_DISPENSER ? 9 : 27, m->arg);
        } break;
        case MK_ARROWS: {
            BlockEntity *be = world_block_entity(p, true, BE_DISPENSER);
            be->slots[4] = stack_of(I_ARROW, irand(2, 9));
        } break;
        case MK_SPAWNER: {
            if (world_block_entity(p, false, 0)) break;
            BlockEntity *be = world_block_entity(p, true, BE_SPAWNER);
            be->data = m->arg; be->burn_time = irand(20, 200) * 3;
        } break;
        case MK_MOB: {
            Entity *e = mob_spawn(m->arg, at);
            if (e) { e->persistent = true; if (m->arg == MOB_IRON_GOLEM || m->arg == MOB_CAT) e->anchor = at; }
        } break;
        case MK_VILLAGER: {
            Entity *e = mob_spawn(MOB_VILLAGER, at);
            if (!e) break;
            e->persistent = true;
            if (m->arg == PROF_NITWIT || m->arg == PROF_NONE) villager_set_profession(e, m->arg);
            else { villager_set_profession(e, m->arg); }
            if (irand(0, 9) == 0) { e->baby = true; e->grow_age = -24000 * 3; mob_set_size(e, 1); villager_set_profession(e, PROF_NONE); }
        } break;
        case MK_CART: {
            Entity *v = vehicle_spawn(VEH_CHEST_MINECART, v3(at.x, at.y + 0.0625f, at.z), irand(0, 1) ? 0 : PI_F / 2);
            if (!v) break;
            v->item = stack_of(I_CHEST_MINECART, 1);
            BlockEntity *be = world_block_entity(vehicle_storage_pos(v), true, BE_CHEST);
            loot_fill(be->slots, 27, LOOT_MINESHAFT);
        } break;
        }
    }
    c->modified = true;   // keep the result: saved columns are loaded, not regenerated (no duplicate mobs)
}

// ------------------------------------------------------------------ spawners
static int light_max(u16 l) { return MAXI(L_SKY(l) * (world_daytime() % DAY_TICKS < DAY_TICKS / 2 ? 1 : 0), MAXI(L_R(l), MAXI(L_G(l), L_B(l)))); }
static bool spawn_space(int x, int y, int z, f32 h) {
    for (int i = 0; i < (int)ceilf(h); i++) { Block b = world_get_block(x, y + i, z); if (BLOCKS[b].solid || BLOCKS[b].liquid) return false; }
    return BLOCKS[world_get_block(x, y - 1, z)].solid || h < 0.6f;
}
void spawners_tick(void) {
    Entity *pl = player_ent();
    if (!pl || g_player.dead || !rule(GR_SPAWNER_BLOCKS_ENABLED)) return;
    for (int i = 0; i < g_world.nbents; i++) {
        BlockEntity *be = &g_world.bents[i];
        if (be->type != BE_SPAWNER) continue;
        ivec3 p = be->pos;
        if (abs(p.x - (int)pl->pos.x) > 16 || abs(p.z - (int)pl->pos.z) > 16 || abs(p.y - (int)pl->pos.y) > 16) continue;
        if (v3_dist(iv3_center(p), pl->pos) > 16) continue;
        Block blk = world_get_block(p.x, p.y, p.z);
        if (BLOCKS[blk].be != BE_SPAWNER) continue;
        if (g_world.time % 4 == 0) fx_smoke(v3(p.x + frand(), p.y + frand(), p.z + frand()), 1, 0.02f, rgba_u32(255, 120, 40, 255));
        if (be->data <= 0 || be->data >= MOB_COUNT) continue;
        if (--be->burn_time > 0) continue;
        be->burn_time = irand(200, 800) * 3;
        bool trial = BLOCKS[blk].base == B_TRIAL_SPAWNER;
        int nearby = 0;
        ENT_FOREACH(o) if (o->kind == ENT_MOB && o->mob == be->data && v3_dist(o->pos, iv3_center(p)) < 9) nearby++;
        if (nearby >= (trial ? 3 : 6)) continue;
        int want = trial ? 2 : 4, made = 0;
        f32 h = MOB_DEFS[be->data].height;
        for (int k = 0; k < want * 3 && made < want; k++) {
            int x = p.x + irand(-4, 4), y = p.y + irand(-1, 1), z = p.z + irand(-4, 4);
            if (!spawn_space(x, y, z, h)) continue;
            if (mob_hostile(be->data) && light_max(world_get_light(x, y, z)) > 11 && be->data != MOB_BLAZE && be->data != MOB_MAGMA_CUBE) continue;
            Entity *e = mob_spawn(be->data, v3(x + 0.5f, (f32)y, z + 0.5f));
            if (!e) continue;
            e->from_spawner = true;
            fx_poof(e->pos, e->width, e->height);
            made++;
        }
        if (trial && made) be->cook_time++;   // trial spawners: after a few waves, eject loot
        if (trial && be->cook_time >= 3) {
            be->cook_time = 0;
            ItemStack loot[9] = {{0}};
            loot_fill(loot, 9, LOOT_TRIAL);
            for (int k = 0; k < 9; k++) if (!stack_empty(loot[k])) item_drop(v3(p.x + 0.5f, p.y + 1.2f, p.z + 0.5f), loot[k], v3(frand_range(-1, 1), 4, frand_range(-1, 1)), 10);
        }
    }
}

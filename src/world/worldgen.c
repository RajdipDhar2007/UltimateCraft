// Deterministic, thread-safe world generation: same seed => same world, independent of load order.
// Overworld biomes are chosen with a multi-noise climate model (temperature, humidity,
// continentalness, erosion, weirdness) modelled on Minecraft's OverworldBiomeBuilder; the Nether
// and the End have their own terrain generators. Decoration only ever writes inside the column
// being generated (features from neighbouring columns are re-derived from the same hashes), so
// column borders are seamless.
#include "world.h"
#include <noise1234.h>
#include "structures.h"

static volatile int g_dim;
void worldgen_set_dim(int dim) { g_dim = dim; }

typedef struct { f32 o[32][2]; } Seeds;   // layers 0..21 Overworld/Hell/Space, 22..27 the Sift
static void seeds_of(u64 seed, Seeds *s) {
    for (int i = 0; i < 32; i++) {
        u64 h = hash64(seed * 31 + (u64)i * 7919 + 17);
        s->o[i][0] = (f32)(h % 25600) / 100.0f;
        s->o[i][1] = (f32)((h >> 20) % 25600) / 100.0f;
    }
}
static f32 fbm(const Seeds *s, int layer, f32 x, f32 z, int oct) {
    f32 sum = 0, amp = 1, norm = 0, f = 1;
    for (int i = 0; i < oct; i++) {
        sum += noise2(x * f + s->o[layer][0] + i * 31.7f, z * f + s->o[layer][1] - i * 17.3f) * amp;
        norm += amp; amp *= 0.5f; f *= 2.0f;
    }
    return sum / norm * 1.6f;
}
static inline f32 rnd01(u64 seed, int x, int y, int z) { return hashf(seed, x, y, z); }

// ================================================================== overworld climate & biomes
typedef struct { int h; u8 biome; bool river; f32 C, E, W, T, H, pv; } ColParams;

enum { M_ = 255 };
static const u8 MIDDLE[5][5] = {
    {BIOME_SNOWY_PLAINS, BIOME_SNOWY_PLAINS, BIOME_SNOWY_PLAINS, BIOME_SNOWY_TAIGA, BIOME_TAIGA},
    {BIOME_PLAINS, BIOME_PLAINS, BIOME_FOREST, BIOME_TAIGA, BIOME_OLD_GROWTH_SPRUCE_TAIGA},
    {BIOME_FLOWER_FOREST, BIOME_PLAINS, BIOME_FOREST, BIOME_BIRCH_FOREST, BIOME_DARK_FOREST},
    {BIOME_SAVANNA, BIOME_SAVANNA, BIOME_FOREST, BIOME_JUNGLE, BIOME_JUNGLE},
    {BIOME_DESERT, BIOME_DESERT, BIOME_DESERT, BIOME_BADLANDS, BIOME_WOODED_BADLANDS}};
static const u8 MIDDLE_V[5][5] = {
    {BIOME_ICE_SPIKES, M_, BIOME_SNOWY_TAIGA, M_, M_},
    {M_, M_, M_, M_, BIOME_OLD_GROWTH_PINE_TAIGA},
    {BIOME_SUNFLOWER_PLAINS, M_, M_, BIOME_OLD_GROWTH_BIRCH_FOREST, BIOME_PALE_GARDEN},
    {M_, M_, BIOME_PLAINS, BIOME_SPARSE_JUNGLE, BIOME_BAMBOO_JUNGLE},
    {M_, M_, M_, BIOME_ERODED_BADLANDS, M_}};
static const u8 PLATEAU[5][5] = {
    {BIOME_SNOWY_PLAINS, BIOME_SNOWY_PLAINS, BIOME_SNOWY_PLAINS, BIOME_SNOWY_TAIGA, BIOME_SNOWY_TAIGA},
    {BIOME_MEADOW, BIOME_MEADOW, BIOME_FOREST, BIOME_TAIGA, BIOME_OLD_GROWTH_SPRUCE_TAIGA},
    {BIOME_MEADOW, BIOME_MEADOW, BIOME_MEADOW, BIOME_MEADOW, BIOME_DARK_FOREST},
    {BIOME_SAVANNA_PLATEAU, BIOME_SAVANNA_PLATEAU, BIOME_FOREST, BIOME_FOREST, BIOME_JUNGLE},
    {BIOME_BADLANDS, BIOME_BADLANDS, BIOME_BADLANDS, BIOME_WOODED_BADLANDS, BIOME_WOODED_BADLANDS}};
static const u8 PLATEAU_V[5][5] = {
    {BIOME_ICE_SPIKES, M_, M_, M_, M_},
    {BIOME_CHERRY_GROVE, M_, BIOME_MEADOW, BIOME_MEADOW, BIOME_OLD_GROWTH_PINE_TAIGA},
    {BIOME_CHERRY_GROVE, BIOME_CHERRY_GROVE, BIOME_FOREST, BIOME_BIRCH_FOREST, BIOME_PALE_GARDEN},
    {M_, M_, M_, M_, M_},
    {BIOME_ERODED_BADLANDS, BIOME_ERODED_BADLANDS, M_, M_, M_}};
static const u8 SHATTERED[3][5] = {
    {BIOME_WINDSWEPT_GRAVELLY_HILLS, BIOME_WINDSWEPT_GRAVELLY_HILLS, BIOME_WINDSWEPT_HILLS, BIOME_WINDSWEPT_FOREST, BIOME_WINDSWEPT_FOREST},
    {BIOME_WINDSWEPT_GRAVELLY_HILLS, BIOME_WINDSWEPT_GRAVELLY_HILLS, BIOME_WINDSWEPT_HILLS, BIOME_WINDSWEPT_FOREST, BIOME_WINDSWEPT_FOREST},
    {BIOME_WINDSWEPT_HILLS, BIOME_WINDSWEPT_HILLS, BIOME_WINDSWEPT_HILLS, BIOME_WINDSWEPT_FOREST, BIOME_WINDSWEPT_FOREST}};

static int idx5(f32 v, f32 a, f32 b, f32 c, f32 d) { return v < a ? 0 : v < b ? 1 : v < c ? 2 : v < d ? 3 : 4; }
static int pick(const u8 t[5][5], const u8 v[5][5], int ti, int hi, f32 W) {
    if (v && W > 0 && v[ti][hi] != M_) return v[ti][hi];
    return t[ti][hi];
}

// The climate splines below were tuned in a compact 128-block "shape space" (sea at 56). The final
// height is remapped onto Minecraft's range: ocean floors around y 30-45, plains at 64-80, hills to
// ~110 and mountain peaks around 180-230, with the sea at y 63.
#define OLD_SEA 56.0f
static f32 remap_height(f32 h) {
    if (h <= OLD_SEA) return SEA_LEVEL - (OLD_SEA - h) * 1.25f;
    if (h <= 80) return SEA_LEVEL + (h - OLD_SEA) * 1.6f;
    return SEA_LEVEL + 24 * 1.6f + (h - 80) * 2.6f;
}
static ColParams params(const Seeds *s, int x, int z) {
    ColParams p = {0};
    // domain warp keeps coastlines and biome borders organic
    f32 wx = x + fbm(s, 10, x / 180.0f, z / 180.0f, 2) * 40.0f, wz = z + fbm(s, 11, x / 180.0f, z / 180.0f, 2) * 40.0f;
    f32 C = fbm(s, 0, wx / 900.0f, wz / 900.0f, 4) + 0.12f;
    f32 E = fbm(s, 1, wx / 520.0f, wz / 520.0f, 3);
    f32 W = fbm(s, 2, wx / 380.0f, wz / 380.0f, 3);
    f32 T = fbm(s, 4, wx / 1150.0f, wz / 1150.0f, 2);
    f32 H = fbm(s, 5, wx / 950.0f, wz / 950.0f, 2);
    f32 pv = 1.0f - fabsf(3.0f * fabsf(W) - 2.0f);              // peaks & valleys (Minecraft's folded weirdness)
    f32 det = fbm(s, 3, x / 48.0f, z / 48.0f, 3);
    p.C = C; p.E = E; p.W = W; p.T = T; p.H = H; p.pv = pv;
    // continentalness spline -> base height
    f32 base;
    if (C < -0.75f) base = 22;
    else if (C < -0.45f) base = lerpf(22, 36, (C + 0.75f) / 0.3f);
    else if (C < -0.19f) base = lerpf(36, 50, (C + 0.45f) / 0.26f);
    else if (C < -0.11f) base = lerpf(50, 58, (C + 0.19f) / 0.08f);
    else if (C < 0.03f) base = lerpf(58, 63, (C + 0.11f) / 0.14f);
    else base = 63 + fminf(1.0f, (C - 0.03f) / 0.5f) * 8;
    f32 inland = smoothstepf(-0.15f, 0.25f, C);
    f32 rough = smoothstepf(0.45f, -0.6f, E);                      // low erosion = mountains
    f32 h = base + det * (2.0f + 3.0f * inland);
    h += inland * rough * (pv * 0.5f + 0.5f) * 58.0f;               // mountain ranges
    h += inland * smoothstepf(0.0f, 0.5f, pv) * (1.0f - rough) * 10.0f;  // hills
    f32 riv = fabsf(fbm(s, 6, wx / 420.0f, wz / 420.0f, 3));
    if (inland > 0.25f && riv < 0.05f && h < 96) {
        f32 t = smoothstepf(0.012f, 0.05f, riv);
        f32 nh = lerpf(OLD_SEA - 4.0f, h, t);
        if (nh <= OLD_SEA) p.river = true;
        h = nh;
    }
    // mushroom islands rise out of the deep ocean
    f32 mush = C < -0.45f ? smoothstepf(0.42f, 0.6f, fbm(s, 12, x / 140.0f, z / 140.0f, 2)) : 0;
    if (mush > 0) h = lerpf(h, OLD_SEA + 3.0f + det * 2, mush);
    h = clampf(h, 6, 124);
    int ho = (int)floorf(h);                       // shape-space height for the biome rules
    p.h = clampi((int)floorf(remap_height(h)), MCY(-40), MCY(300));
    if (mush > 0.5f) { p.biome = BIOME_MUSHROOM_FIELDS; return p; }
    int ti = idx5(T, -0.45f, -0.15f, 0.2f, 0.55f), hi = idx5(H, -0.35f, -0.1f, 0.1f, 0.3f);
    if (ho < OLD_SEA - 1 && !p.river) {

        bool deep = ho < 40;
        static const u8 O[5] = {BIOME_FROZEN_OCEAN, BIOME_COLD_OCEAN, BIOME_OCEAN, BIOME_LUKEWARM_OCEAN, BIOME_WARM_OCEAN};
        static const u8 D[5] = {BIOME_DEEP_FROZEN_OCEAN, BIOME_DEEP_COLD_OCEAN, BIOME_DEEP_OCEAN, BIOME_DEEP_LUKEWARM_OCEAN, BIOME_WARM_OCEAN};
        p.biome = deep ? D[ti] : O[ti];
        return p;
    }
    if (p.river) { p.biome = ti == 0 ? BIOME_FROZEN_RIVER : BIOME_RIVER; return p; }
    if (ho <= OLD_SEA + 2 && C < -0.05f) {
        if (E < -0.25f) p.biome = BIOME_STONY_SHORE;
        else p.biome = ti == 0 ? BIOME_SNOWY_BEACH : ti == 4 ? BIOME_DESERT : BIOME_BEACH;
        return p;
    }
    if (E > 0.5f && ho <= OLD_SEA + 5 && ti >= 1 && hi >= 2) { p.biome = ti >= 3 ? BIOME_MANGROVE_SWAMP : BIOME_SWAMP; return p; }
    if (ho >= 98 && rough > 0.5f) {
        if (ti <= 2) p.biome = W < 0 ? BIOME_FROZEN_PEAKS : BIOME_JAGGED_PEAKS;
        else if (ti == 3) p.biome = BIOME_STONY_PEAKS;
        else p.biome = BIOME_BADLANDS;
        return p;
    }
    if (ho >= 86 && rough > 0.35f) {
        if (ti <= 1) { p.biome = hi < 2 ? BIOME_SNOWY_SLOPES : BIOME_GROVE; return p; }
        p.biome = pick(PLATEAU, PLATEAU_V, ti, hi, W); return p;
    }
    if (E > 0.4f && pv > 0.2f && inland > 0.5f) {
        if (ti <= 2) { p.biome = SHATTERED[ti][hi]; return p; }
        if (W > 0) { p.biome = BIOME_WINDSWEPT_SAVANNA; return p; }
    }
    if (ho >= 76) { p.biome = pick(PLATEAU, PLATEAU_V, ti, hi, W - 0.2f); return p; }
    p.biome = pick(MIDDLE, MIDDLE_V, ti, hi, W - 0.25f);
    return p;
}

// ================================================================== nether / end shapes
static int nether_biome(const Seeds *s, int x, int z) {
    f32 t = fbm(s, 13, x / 160.0f, z / 160.0f, 2), h = fbm(s, 14, x / 160.0f, z / 160.0f, 2);
    if (t > 0.35f) return BIOME_BASALT_DELTAS;
    if (h > 0.3f) return BIOME_WARPED_FOREST;
    if (h < -0.3f) return BIOME_CRIMSON_FOREST;
    if (t < -0.3f) return BIOME_SOUL_SAND_VALLEY;
    return BIOME_NETHER_WASTES;
}
static bool nether_solid(const Seeds *s, int x, int y, int z) {
    if (y <= 0 || y >= NETHER_H - 1) return true;
    f32 n = noise3(x / 48.0f + s->o[15][0], y / 32.0f, z / 48.0f + s->o[15][1]) + noise3(x / 18.0f + s->o[16][0], y / 14.0f, z / 18.0f + s->o[16][1]) * 0.35f;
    f32 bias = 0;
    if (y < 24) bias += (24 - y) / 24.0f * 0.9f;
    if (y > 100) bias += (y - 100) / 27.0f * 1.2f;
    if (y > 40 && y < 90) bias -= 0.25f;
    return n + bias > 0.12f;
}
static int end_biome(int x, int z) {
    f32 d = sqrtf((f32)x * x + (f32)z * z);
    if (d < 800) return BIOME_THE_END;
    u64 h = hash3i(9, x >> 6, 0, z >> 6);
    int k = (int)(h % 4);
    return k == 0 ? BIOME_END_HIGHLANDS : k == 1 ? BIOME_END_MIDLANDS : k == 2 ? BIOME_SMALL_END_ISLANDS : BIOME_END_BARRENS;
}
// end island density: returns surface height range [lo, hi] or lo > hi when empty
static void end_column(const Seeds *s, int x, int z, int *lo, int *hi) {
    f32 d = sqrtf((f32)x * x + (f32)z * z);
    *lo = 1; *hi = 0;
    if (d < 80) {
        f32 r = 1.0f - d / 80.0f;
        f32 n = fbm(s, 17, x / 40.0f, z / 40.0f, 3) * 4;
        *hi = (int)(56 + r * 6 + n * r);
        *lo = (int)(56 - r * r * 40 - fabsf(n) * 2);
        return;
    }
    if (d < 900) return;   // the void between the main island and the outer islands
    f32 n = fbm(s, 18, x / 90.0f, z / 90.0f, 4);
    if (n < 0.15f) return;
    f32 t = (n - 0.15f) * 3.0f;
    *hi = (int)(58 + t * 10 + fbm(s, 19, x / 25.0f, z / 25.0f, 2) * 3);
    *lo = (int)(58 - t * 30);
}

// ================================================================== the Sift (heaven): sky islands over a cloud sea
#define SIFT_CLOUD_Y 60
static f32 sift_island(const Seeds *s, int x, int z) {
    f32 d = sqrtf((f32)x * x + (f32)z * z);
    f32 n = fbm(s, 22, x / 110.0f, z / 110.0f, 4);
    n += fmaxf(0, 1.0f - d / 70.0f) * 0.7f;    // a guaranteed arrival island at the origin
    return n;
}
static int sift_biome(const Seeds *s, int x, int z) {
    f32 n = sift_island(s, x, z);
    if (n < 0.12f) return BIOME_CLOUD_SEA;
    return fbm(s, 23, x / 200.0f, z / 200.0f, 2) > 0.15f ? BIOME_CRYSTAL_GROVE : BIOME_GOLDEN_MEADOWS;
}
// island surface range [lo, hi] (lo > hi: open sky)
static void sift_column_shape(const Seeds *s, int x, int z, int *lo, int *hi) {
    f32 n = sift_island(s, x, z);
    *lo = 1; *hi = 0;
    if (n < 0.12f) return;
    f32 t = n - 0.12f;
    *hi = (int)(104 + t * 34 + fbm(s, 24, x / 24.0f, z / 24.0f, 2) * 4);
    *lo = (int)(104 - t * 70 - fabsf(fbm(s, 25, x / 12.0f, z / 12.0f, 2)) * 8);
    if (*lo < SIFT_CLOUD_Y + 8) *lo = SIFT_CLOUD_Y + 8;
}
int worldgen_sift_surface(u64 seed, int x, int z) { Seeds s; seeds_of(seed, &s); int lo, hi; sift_column_shape(&s, x, z, &lo, &hi); return lo > hi ? -1 : hi; }

void worldgen_end_column(u64 seed, int x, int z, int *lo, int *hi) { Seeds s; seeds_of(seed, &s); end_column(&s, x, z, lo, hi); }
int worldgen_biome(u64 seed, int dim, int x, int z) {
    Seeds s; seeds_of(seed, &s);
    if (dim == DIM_NETHER) return nether_biome(&s, x, z);
    if (dim == DIM_END) return end_biome(x, z);
    if (dim == DIM_SIFT) return sift_biome(&s, x, z);
    return params(&s, x, z).biome;
}
int worldgen_surface(u64 seed, int x, int z, int *biome) {
    Seeds s; seeds_of(seed, &s);
    ColParams p = params(&s, x, z);
    if (biome) *biome = p.biome;
    return p.h;
}
int worldgen_cave_biome(u64 seed, int x, int y, int z) {
    Seeds s; seeds_of(seed, &s);
    f32 a = fbm(&s, 20, x / 120.0f, z / 120.0f, 2), b = fbm(&s, 21, x / 140.0f, z / 140.0f, 2);
    if (y < MCY(0) && a > 0.38f) return BIOME_DEEP_DARK;
    if (y < MCY(48) && b > 0.35f) return BIOME_LUSH_CAVES;
    if (y < MCY(48) && b < -0.38f) return BIOME_DRIPSTONE_CAVES;
    return -1;
}

ivec3 worldgen_find_spawn(u64 seed) {
    Seeds s; seeds_of(seed, &s);
    for (int r = 0; r < 6000; r += 8) {
        for (int a = 0; a < 16; a++) {
            int x = (int)(cosf(a * PI_F / 8) * r), z = (int)(sinf(a * PI_F / 8) * r);
            ColParams p = params(&s, x, z);
            int c = BIOMES[p.biome].category;
            if (p.h > SEA_LEVEL + 1 && p.h < SEA_LEVEL + 40 && c == 0 && p.biome != BIOME_PALE_GARDEN && p.biome != BIOME_DARK_FOREST)
                return iv3(x, p.h + 1, z);
            if (r == 0) break;
        }
    }
    return iv3(0, SEA_LEVEL + 20, 0);
}

ivec3 worldgen_stronghold(u64 seed) {
    f32 ang = (f32)(hash64(seed ^ 0x5157ULL) % 6283) / 1000.0f;
    f32 dist = 640.0f + (f32)(hash64(seed ^ 0x9192ULL) % 512);
    return iv3(((int)(cosf(ang) * dist)) & ~15, MCY(12), ((int)(sinf(ang) * dist)) & ~15);
}

// ================================================================== writers
// A Gen writes either into one column under construction (c != NULL; anything outside is
// discarded) or into the live world (c == NULL, used by saplings and bone meal).
typedef struct { Column *c; int bx, bz; } Gen;
static inline u16 gget(Gen *g, int wx, int y, int wz) {
    if (y < 0 || y >= WORLD_H) return B_AIR;
    if (!g->c) return world_get_block(wx, y, wz);
    int lx = wx - g->bx, lz = wz - g->bz;
    if (lx < 0 || lx > 15 || lz < 0 || lz > 15) return B_AIR;
    return g->c->blocks[y][lz][lx];
}
static inline bool soft(u16 b) { return b == B_AIR || b == B_CAVE_AIR || BLOCKS[b].replaceable || block_is_leaves(b) || BLOCKS[b].mesh == MESH_CROSS; }
static inline void gput(Gen *g, int wx, int y, int wz, u16 b, bool only_soft) {
    if (y < 1 || y >= WORLD_H) return;
    if (!g->c) {
        if (!world_loaded(wx, wz)) return;
        u16 cur = world_get_block(wx, y, wz);
        if (only_soft && !soft(cur)) return;
        world_set_block(wx, y, wz, b);
        return;
    }
    int lx = wx - g->bx, lz = wz - g->bz;
    if (lx < 0 || lx > 15 || lz < 0 || lz > 15) return;
    u16 *d = &g->c->blocks[y][lz][lx];
    if (only_soft && !soft(*d)) return;
    *d = b;
}

// ================================================================== trees
#define AX_Y 0
#define AX_X 1
#define AX_Z 2
static void leaf_blob(Gen *g, int cx, int cy, int cz, int rx, int ry, u16 leaf, Rng *r) {
    for (int dy = -ry; dy <= ry; dy++) for (int dz = -rx; dz <= rx; dz++) for (int dx = -rx; dx <= rx; dx++) {
        f32 d = (f32)(dx * dx + dz * dz) / (rx * rx + 0.5f) + (f32)(dy * dy) / (ry * ry + 0.5f);
        if (d > 1.0f || (d > 0.75f && rng_int(r, 0, 2) == 0)) continue;
        gput(g, cx + dx, cy + dy, cz + dz, leaf, true);
    }
}
static void classic_canopy(Gen *g, int x, int top, int z, u16 leaf, Rng *r) {
    for (int dy = -3; dy <= 0; dy++) {
        int rr = dy >= -1 ? 1 : 2, yy = top + dy;
        for (int dx = -rr; dx <= rr; dx++) for (int dz = -rr; dz <= rr; dz++) {
            bool corner = abs(dx) == rr && abs(dz) == rr;
            if (corner && (dy == 0 || rng_int(r, 0, 1))) continue;
            gput(g, x + dx, yy, z + dz, leaf, true);
        }
    }
}
static void trunk(Gen *g, int x, int y, int z, int h, u16 log) { for (int i = 0; i < h; i++) gput(g, x, y + i, z, log, false); }
static void vines_around(Gen *g, int x, int y, int z, int r, int h, Rng *rr) {
    for (int i = 0; i < r * 6; i++) {
        int k = rng_int(rr, 0, 3), f = H4_FACE[k];
        int vx = x + rng_int(rr, -r, r), vz = z + rng_int(rr, -r, r), vy = y + rng_int(rr, 0, h);
        if (!block_is_leaves(gget(g, vx, vy, vz))) continue;
        int ox = vx - FACE_DIR[f].x, oz = vz - FACE_DIR[f].z;   // vine hangs on the side facing away
        int len = rng_int(rr, 1, 4);
        for (int j = 0; j < len && gget(g, ox, vy - j, oz) == B_AIR; j++) gput(g, ox, vy - j, oz, B_VINE + k, true);
    }
}

static bool room_for(Gen *g, int x, int y, int z, int h, int w) {
    if (g->c) return true;   // worldgen trusts its own placement rules
    for (int i = 1; i <= h; i++) for (int dx = 0; dx < w; dx++) for (int dz = 0; dz < w; dz++) if (!soft(gget(g, x + dx, y + i, z + dz))) return false;
    return true;
}

static bool tree(Gen *g, int kind, int x, int y, int z, Rng *r) {
    u16 below = gget(g, x, y - 1, z);
    switch (kind) {
    case TREE_OAK: case TREE_BIRCH: case TREE_TALL_BIRCH: case TREE_SWAMP_OAK: {
        bool birch = kind != TREE_OAK && kind != TREE_SWAMP_OAK;
        int h = kind == TREE_TALL_BIRCH ? rng_int(r, 8, 12) : birch ? rng_int(r, 5, 7) : rng_int(r, 4, 6);
        if (!room_for(g, x, y, z, h + 1, 1)) return false;
        u16 log = birch ? B_BIRCH_LOG : B_OAK_LOG, leaf = birch ? B_BIRCH_LEAVES : B_OAK_LEAVES;
        classic_canopy(g, x, y + h, z, leaf, r);
        if (kind == TREE_SWAMP_OAK) { leaf_blob(g, x, y + h - 2, z, 3, 1, leaf, r); vines_around(g, x, y + h - 3, z, 3, 3, r); }
        trunk(g, x, y, z, h, log);
    } break;
    case TREE_FANCY_OAK: {
        int h = rng_int(r, 7, 11);
        if (!room_for(g, x, y, z, h + 2, 1)) return false;
        trunk(g, x, y, z, h, B_OAK_LOG);
        for (int b = 0; b < 4; b++) {
            int k = rng_int(r, 0, 3), len = rng_int(r, 2, 3), by = y + rng_int(r, h / 2, h - 1);
            int dx = FACE_DIR[H4_FACE[k]].x, dz = FACE_DIR[H4_FACE[k]].z;
            for (int i = 1; i <= len; i++) gput(g, x + dx * i, by + i / 2, z + dz * i, B_OAK_LOG + (dx ? AX_X : AX_Z), false);
            leaf_blob(g, x + dx * len, by + len / 2 + 1, z + dz * len, 2, 2, B_OAK_LEAVES, r);
        }
        leaf_blob(g, x, y + h, z, 3, 2, B_OAK_LEAVES, r);
    } break;
    case TREE_SPRUCE: case TREE_PINE: {
        int h = kind == TREE_PINE ? rng_int(r, 8, 12) : rng_int(r, 6, 9);
        if (!room_for(g, x, y, z, h + 1, 1)) return false;
        int start = kind == TREE_PINE ? h - 4 : 2, rad = 1;
        for (int dy = h + 1; dy >= start; dy--) {
            int rr = dy >= h ? (dy == h + 1 ? 0 : 1) : rad;
            for (int dx = -rr; dx <= rr; dx++) for (int dz = -rr; dz <= rr; dz++) {
                if (rr >= 2 && abs(dx) == rr && abs(dz) == rr) continue;
                gput(g, x + dx, y + dy, z + dz, B_SPRUCE_LEAVES, true);
            }
            rad = kind == TREE_PINE ? 1 : (rad == 1 ? 2 : (rad == 2 && dy < h / 2 ? 3 : 1));
        }
        trunk(g, x, y, z, h, B_SPRUCE_LOG);
    } break;
    case TREE_MEGA_SPRUCE: case TREE_MEGA_PINE: {
        int h = rng_int(r, 16, 26);
        if (y + h > WORLD_H - 4) h = WORLD_H - 4 - y;
        if (h < 10 || !room_for(g, x, y, z, h, 2)) return false;
        int start = kind == TREE_MEGA_PINE ? h - 5 : h / 3;
        for (int dy = h + 1; dy >= start; dy--) {
            int t = h + 1 - dy;
            int rr = kind == TREE_MEGA_PINE ? MINI(2, t / 2 + 1) : MINI(4, t / 3 + 1) - ((dy & 1) && t > 3 ? 1 : 0);
            for (int dx = -rr; dx <= rr + 1; dx++) for (int dz = -rr; dz <= rr + 1; dz++) {
                f32 cx = dx - 0.5f, cz = dz - 0.5f;
                if (cx * cx + cz * cz > (rr + 0.6f) * (rr + 0.6f)) continue;
                gput(g, x + dx, y + dy, z + dz, B_SPRUCE_LEAVES, true);
            }
        }
        for (int dx = 0; dx < 2; dx++) for (int dz = 0; dz < 2; dz++) trunk(g, x + dx, y, z + dz, h, B_SPRUCE_LOG);
        for (int i = 0; i < 20; i++) {   // podzol floor
            int px = x + rng_int(r, -4, 5), pz = z + rng_int(r, -4, 5);
            for (int py = y + 2; py >= y - 3; py--) if (gget(g, px, py, pz) == B_GRASS_BLOCK || gget(g, px, py, pz) == B_DIRT) { gput(g, px, py, pz, B_PODZOL, false); break; }
        }
    } break;
    case TREE_JUNGLE: {
        int h = rng_int(r, 6, 11);
        if (!room_for(g, x, y, z, h + 1, 1)) return false;
        classic_canopy(g, x, y + h, z, B_JUNGLE_LEAVES, r);
        trunk(g, x, y, z, h, B_JUNGLE_LOG);
        vines_around(g, x, y + h - 3, z, 2, 3, r);
        for (int i = 0; i < 2; i++) if (rng_int(r, 0, 3) == 0) {   // cocoa pods on the trunk
            int k = rng_int(r, 0, 3), f = H4_FACE[k];
            gput(g, x + FACE_DIR[f].x, y + rng_int(r, 2, 4), z + FACE_DIR[f].z, B_COCOA + rng_int(r, 0, 2), true);
        }
    } break;
    case TREE_MEGA_JUNGLE: {
        int h = rng_int(r, 18, 28);
        if (y + h > WORLD_H - 5) h = WORLD_H - 5 - y;
        if (h < 10 || !room_for(g, x, y, z, h, 2)) return false;
        for (int dx = 0; dx < 2; dx++) for (int dz = 0; dz < 2; dz++) trunk(g, x + dx, y, z + dz, h, B_JUNGLE_LOG);
        leaf_blob(g, x, y + h, z, 4, 2, B_JUNGLE_LEAVES, r);
        for (int b = 0; b < 3; b++) {
            int k = rng_int(r, 0, 3), by = y + h - rng_int(r, 4, 9), len = rng_int(r, 2, 4);
            int dx = FACE_DIR[H4_FACE[k]].x, dz = FACE_DIR[H4_FACE[k]].z;
            for (int i = 1; i <= len; i++) gput(g, x + dx * i, by + i / 2, z + dz * i, B_JUNGLE_LOG + (dx ? AX_X : AX_Z), false);
            leaf_blob(g, x + dx * len, by + len / 2 + 1, z + dz * len, 2, 1, B_JUNGLE_LEAVES, r);
        }
        vines_around(g, x, y + 3, z, 3, h - 2, r);
    } break;
    case TREE_JUNGLE_BUSH: case TREE_BUSH:
        gput(g, x, y, z, kind == TREE_BUSH ? B_OAK_LOG : B_JUNGLE_LOG, false);
        leaf_blob(g, x, y + 1, z, kind == TREE_BUSH ? 1 : 2, 1, kind == TREE_BUSH ? B_OAK_LEAVES : B_OAK_LEAVES, r);
        break;
    case TREE_ACACIA: {
        int h = rng_int(r, 5, 7);
        if (!room_for(g, x, y, z, h + 2, 1)) return false;
        int k = rng_int(r, 0, 3), dx = FACE_DIR[H4_FACE[k]].x, dz = FACE_DIR[H4_FACE[k]].z;
        int bend = rng_int(r, 2, 3), cx = x, cz = z;
        for (int i = 0; i < h; i++) {
            if (i >= h - bend) { cx += dx; cz += dz; }
            gput(g, cx, y + i, cz, B_ACACIA_LOG, false);
        }
        for (int dy = 0; dy <= 1; dy++) for (int ax = -3 + dy; ax <= 3 - dy; ax++) for (int az = -3 + dy; az <= 3 - dy; az++) {
            if (abs(ax) + abs(az) > 4 - dy) continue;
            gput(g, cx + ax, y + h + dy, cz + az, B_ACACIA_LEAVES, true);
        }
        if (rng_int(r, 0, 1)) {   // second, smaller canopy branch
            int bx = x - dx * 2, bz = z - dz * 2, by = y + h - 2;
            gput(g, x - dx, by - 1, z - dz, B_ACACIA_LOG, false); gput(g, bx, by, bz, B_ACACIA_LOG, false);
            for (int ax = -2; ax <= 2; ax++) for (int az = -2; az <= 2; az++) if (abs(ax) + abs(az) <= 3) gput(g, bx + ax, by + 1, bz + az, B_ACACIA_LEAVES, true);
        }
    } break;
    case TREE_DARK_OAK: case TREE_PALE_OAK: {
        int h = rng_int(r, 6, 9);
        if (!room_for(g, x, y, z, h + 1, 2)) return false;
        u16 log = kind == TREE_DARK_OAK ? B_DARK_OAK_LOG : B_PALE_OAK_LOG, leaf = kind == TREE_DARK_OAK ? B_DARK_OAK_LEAVES : B_PALE_OAK_LEAVES;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -3; dx <= 4; dx++) for (int dz = -3; dz <= 4; dz++) {
            f32 cx = dx - 0.5f, cz = dz - 0.5f;
            if (cx * cx + cz * cz > (dy == 1 ? 6.0f : 13.0f) || (cx * cx + cz * cz > 10 && rng_int(r, 0, 2) == 0)) continue;
            gput(g, x + dx, y + h + dy, z + dz, leaf, true);
        }
        for (int dx = 0; dx < 2; dx++) for (int dz = 0; dz < 2; dz++) trunk(g, x + dx, y, z + dz, h, log);
        if (kind == TREE_PALE_OAK) for (int i = 0; i < 6; i++) {
            int mx = x + rng_int(r, -3, 4), mz = z + rng_int(r, -3, 4);
            if (gget(g, mx, y + h - 2, mz) == B_AIR && block_is_leaves(gget(g, mx, y + h - 1, mz))) gput(g, mx, y + h - 2, mz, B_PALE_HANGING_MOSS, true);
        }
    } break;
    case TREE_MANGROVE: {
        int h = rng_int(r, 5, 8), base = y + 2;
        for (int k = 0; k < 4; k++) {   // arching roots
            int f = H4_FACE[k], rl = rng_int(r, 1, 2);
            for (int i = 0; i <= rl; i++) gput(g, x + FACE_DIR[f].x * (i + 1), base - i, z + FACE_DIR[f].z * (i + 1), B_MANGROVE_ROOTS, false);
            for (int dy = base - rl - 1; dy > y - 4 && soft(gget(g, x + FACE_DIR[f].x * (rl + 1), dy, z + FACE_DIR[f].z * (rl + 1))); dy--)
                gput(g, x + FACE_DIR[f].x * (rl + 1), dy, z + FACE_DIR[f].z * (rl + 1), B_MANGROVE_ROOTS, false);
        }
        for (int i = y; i < base; i++) gput(g, x, i, z, B_MANGROVE_ROOTS, false);
        trunk(g, x, base, z, h, B_MANGROVE_LOG);
        leaf_blob(g, x, base + h, z, 3, 2, B_MANGROVE_LEAVES, r);
        for (int i = 0; i < 4; i++) {
            int px = x + rng_int(r, -2, 2), pz = z + rng_int(r, -2, 2), py = base + h - 3;
            if (gget(g, px, py, pz) == B_AIR && block_is_leaves(gget(g, px, py + 1, pz))) gput(g, px, py, pz, B_MANGROVE_PROPAGULE, true);
        }
    } break;
    case TREE_CHERRY: {
        int h = rng_int(r, 4, 6);
        if (!room_for(g, x, y, z, h + 3, 1)) return false;
        trunk(g, x, y, z, h, B_CHERRY_LOG);
        for (int b = 0; b < 2; b++) {
            int k = (rng_int(r, 0, 3) + b * 2) % 4, dx = FACE_DIR[H4_FACE[k]].x, dz = FACE_DIR[H4_FACE[k]].z;
            int len = rng_int(r, 2, 3);
            for (int i = 1; i <= len; i++) gput(g, x + dx * i, y + h - 1 + i, z + dz * i, B_CHERRY_LOG + (dx ? AX_X : AX_Z), false);
            leaf_blob(g, x + dx * len, y + h + len, z + dz * len, 3, 2, B_CHERRY_LEAVES, r);
        }
    } break;
    case TREE_AZALEA: {
        int h = rng_int(r, 2, 4);
        trunk(g, x, y, z, h, B_OAK_LOG);
        for (int dy = 0; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) for (int dz = -2; dz <= 2; dz++) {
            if (abs(dx) + abs(dz) > 3 - (dy == 2)) continue;
            gput(g, x + dx, y + h + dy - 1, z + dz, rng_int(r, 0, 3) ? B_AZALEA_LEAVES : B_FLOWERING_AZALEA_LEAVES, true);
        }
        if (g->c && (below == B_DIRT || below == B_GRASS_BLOCK)) gput(g, x, y - 1, z, B_ROOTED_DIRT, false);
    } break;
    case TREE_HUGE_RED_MUSHROOM: case TREE_HUGE_BROWN_MUSHROOM: {
        int h = rng_int(r, 4, 7);
        if (!room_for(g, x, y, z, h + 1, 1)) return false;
        trunk(g, x, y, z, h, B_MUSHROOM_STEM);
        if (kind == TREE_HUGE_BROWN_MUSHROOM) {
            for (int dx = -3; dx <= 3; dx++) for (int dz = -3; dz <= 3; dz++) if (!(abs(dx) == 3 && abs(dz) == 3)) gput(g, x + dx, y + h, z + dz, B_BROWN_MUSHROOM_BLOCK, true);
        } else {
            for (int dy = h - 3; dy <= h; dy++) {
                int rr = dy == h ? 1 : 2;
                for (int dx = -rr; dx <= rr; dx++) for (int dz = -rr; dz <= rr; dz++) {
                    if (dy < h && abs(dx) < rr && abs(dz) < rr) continue;
                    if (abs(dx) == rr && abs(dz) == rr && dy < h) continue;
                    gput(g, x + dx, y + dy, z + dz, B_RED_MUSHROOM_BLOCK, true);
                }
            }
        }
    } break;
    case TREE_CRIMSON_FUNGUS: case TREE_WARPED_FUNGUS: {
        bool crim = kind == TREE_CRIMSON_FUNGUS;
        int h = rng_int(r, 5, 12);
        if (!room_for(g, x, y, z, h + 1, 1)) return false;
        u16 stem = crim ? B_CRIMSON_STEM : B_WARPED_STEM, wart = crim ? B_NETHER_WART_BLOCK : B_WARPED_WART_BLOCK;
        for (int dy = h - 4; dy <= h; dy++) {
            int rr = dy == h ? 1 : 2;
            for (int dx = -rr; dx <= rr; dx++) for (int dz = -rr; dz <= rr; dz++) {
                if (abs(dx) == rr && abs(dz) == rr && rng_int(r, 0, 1)) continue;
                if (dy < h && abs(dx) < rr && abs(dz) < rr) continue;
                gput(g, x + dx, y + dy, z + dz, rng_int(r, 0, 11) == 0 ? B_SHROOMLIGHT : wart, true);
                if (crim && dy == h - 4 && rng_int(r, 0, 4) == 0)
                    for (int v = 1; v <= rng_int(r, 1, 3); v++) gput(g, x + dx, y + dy - v, z + dz, B_WEEPING_VINES, true);
            }
        }
        trunk(g, x, y, z, h, stem);
    } break;
    case TREE_CRYSTAL: {   // Sift: a glassy trunk with a glowing, layered crown
        int h = rng_int(r, 4, 7);
        trunk(g, x, y, z, h, B_CRYSTAL_LOG);
        leaf_blob(g, x, y + h - 1, z, 2, 1, B_CRYSTAL_LEAVES, r);
        leaf_blob(g, x, y + h + 1, z, 1, 1, B_CRYSTAL_LEAVES, r);
        gput(g, x, y + h + 2, z, B_CRYSTAL_LEAVES, true);
    } break;
    case TREE_CHORUS: {
        int h = rng_int(r, 3, 6);
        for (int i = 0; i < h; i++) gput(g, x, y + i, z, B_CHORUS_PLANT, true);
        for (int b = 0; b < 2; b++) {
            int k = rng_int(r, 0, 3), f = H4_FACE[k], by = y + rng_int(r, 1, h - 1);
            gput(g, x + FACE_DIR[f].x, by, z + FACE_DIR[f].z, B_CHORUS_PLANT, true);
            gput(g, x + FACE_DIR[f].x, by + 1, z + FACE_DIR[f].z, B_CHORUS_FLOWER, true);
        }
        gput(g, x, y + h, z, B_CHORUS_FLOWER, true);
    } break;
    case TREE_FALLEN_OAK: case TREE_FALLEN_BIRCH: case TREE_FALLEN_SPRUCE: {
        u16 log = kind == TREE_FALLEN_OAK ? B_OAK_LOG : kind == TREE_FALLEN_BIRCH ? B_BIRCH_LOG : B_SPRUCE_LOG;
        int len = rng_int(r, 4, 6), ax = rng_int(r, 0, 1);
        gput(g, x, y, z, log, false);
        for (int i = 2; i < len + 2; i++) {
            int lx = x + (ax ? i : 0), lz = z + (ax ? 0 : i);
            if (!BLOCKS[gget(g, lx, y - 1, lz)].solid) break;
            gput(g, lx, y, lz, log + (ax ? AX_X : AX_Z), true);
        }
    } break;
    default: return false;
    }
    return true;
}

bool worldgen_grow_tree(int x, int y, int z, int kind, Rng *rng) {
    Gen g = {NULL, 0, 0};
    u16 old = world_get_block(x, y, z);
    // 2x2 saplings (dark oak / pale oak / spruce / jungle) need four saplings
    if (kind == TREE_DARK_OAK || kind == TREE_PALE_OAK) {
        if (world_get_block(x + 1, y, z) != old || world_get_block(x, y, z + 1) != old || world_get_block(x + 1, y, z + 1) != old) return false;
    }
    if ((kind == TREE_SPRUCE || kind == TREE_JUNGLE) && world_get_block(x + 1, y, z) == old && world_get_block(x, y, z + 1) == old && world_get_block(x + 1, y, z + 1) == old)
        kind = kind == TREE_SPRUCE ? TREE_MEGA_SPRUCE : TREE_MEGA_JUNGLE;
    if (!room_for(&g, x, y, z, 5, 1)) return false;
    world_set_block_silent(x, y, z, B_AIR);
    if (kind == TREE_DARK_OAK || kind == TREE_PALE_OAK || kind == TREE_MEGA_SPRUCE || kind == TREE_MEGA_JUNGLE)
        for (int dx = 0; dx < 2; dx++) for (int dz = 0; dz < 2; dz++) world_set_block_silent(x + dx, y, z + dz, B_AIR);
    if (!tree(&g, kind, x, y, z, rng)) { world_set_block(x, y, z, old); return false; }
    if (BLOCKS[world_get_block(x, y - 1, z)].base == B_GRASS_BLOCK) world_set_block(x, y - 1, z, B_DIRT);
    return true;
}

// ================================================================== column generation helpers
static void ore_veins(Gen *g, u64 seed, u16 ore, int count10, int size, int ymin, int ymax, u16 host, u16 deep_ore) {
    u64 base = hash3i(seed ^ (ore * 0x9E3779B97F4A7C15ULL), g->c->cx, ore, g->c->cz);
    Rng r = {base};
    int count = count10 / 10 + ((rng_int(&r, 0, 9) < count10 % 10) ? 1 : 0);
    for (int i = 0; i < count; i++) {
        int x = rng_int(&r, 0, 15), y = rng_int(&r, ymin, ymax), z = rng_int(&r, 0, 15);
        for (int k = 0; k < size; k++) {
            if (x >= 0 && x < 16 && z >= 0 && z < 16 && y > 0 && y < WORLD_H) {
                u16 *b = &g->c->blocks[y][z][x];
                if (*b == host) *b = ore;
                else if (deep_ore && *b == B_DEEPSLATE) *b = deep_ore;
                else if (host == B_STONE && (*b == B_GRANITE || *b == B_DIORITE || *b == B_ANDESITE || *b == B_TUFF) && ore != B_GRANITE && ore != B_DIORITE && ore != B_ANDESITE) *b = ore;
            }
            switch (rng_int(&r, 0, 5)) {
            case 0: x++; break; case 1: x--; break; case 2: y++; break; case 3: y--; break; case 4: z++; break; default: z--; break;
            }
        }
    }
}

static const u16 BADLANDS_BANDS[16] = {B_TERRACOTTA, B_ORANGE_TERRACOTTA, B_TERRACOTTA, B_YELLOW_TERRACOTTA, B_BROWN_TERRACOTTA, B_TERRACOTTA, B_WHITE_TERRACOTTA,
                                       B_LIGHT_GRAY_TERRACOTTA, B_TERRACOTTA, B_RED_TERRACOTTA, B_ORANGE_TERRACOTTA, B_TERRACOTTA, B_TERRACOTTA, B_YELLOW_TERRACOTTA, B_RED_TERRACOTTA, B_TERRACOTTA};

#define MARGIN 6
#define SPAN (16 + 2 * MARGIN)

static bool local_best(u64 seed, int wx, int wz, u32 salt, f32 chance, int spacing) {
    u64 hh = hash3i(seed, wx, (int)salt, wz);
    if ((hh & 0xFFFFFF) / 16777216.0f >= chance) return false;
    for (int dz = -spacing; dz <= spacing; dz++) for (int dx = -spacing; dx <= spacing; dx++)
        if ((dx || dz) && (hash3i(seed, wx + dx, (int)salt, wz + dz) & 0xFFFFFF) < (hh & 0xFFFFFF)) return false;
    return true;
}

static void overworld_column(Column *c, u64 seed, const Seeds *s) {
    Gen g = {c, c->cx * 16, c->cz * 16};
    static __thread ColParams P[SPAN][SPAN];
    for (int z = 0; z < SPAN; z++) for (int x = 0; x < SPAN; x++) P[z][x] = params(s, g.bx + x - MARGIN, g.bz + z - MARGIN);

    // ---- terrain & surface
    for (int z = 0; z < 16; z++) for (int x = 0; x < 16; x++) {
        ColParams p = P[z + MARGIN][x + MARGIN];
        int wx = g.bx + x, wz = g.bz + z, h = p.h;
        const BiomeDef *bd = &BIOMES[p.biome];
        c->biome[z][x] = p.biome;
        u16 top = bd->top, fill = bd->filler;
        int depth = 3 + (int)(rnd01(seed, wx, 5, wz) * 2);
        bool badl = biome_has(p.biome, FEAT_TERRACOTTA);
        if (biome_has(p.biome, FEAT_PEAKS_SNOW) && h < MCY(200) && p.biome != BIOME_SNOWY_SLOPES) top = h > MCY(170) ? B_SNOW_BLOCK : B_STONE, fill = B_STONE;
        if (p.biome == BIOME_STONY_PEAKS && rnd01(seed, wx >> 2, 6, wz >> 2) < 0.25f) top = fill = B_CALCITE;
        if (p.biome == BIOME_WINDSWEPT_GRAVELLY_HILLS && rnd01(seed, wx >> 1, 7, wz >> 1) < 0.35f) top = B_GRASS_BLOCK, fill = B_DIRT;
        if (p.biome == BIOME_WINDSWEPT_HILLS && h > MCY(160)) top = B_STONE, fill = B_STONE;
        if (biome_has(p.biome, FEAT_PODZOL) && rnd01(seed, wx, 8, wz) < 0.3f) top = B_PODZOL;
        if (biome_has(p.biome, FEAT_COARSE) && rnd01(seed, wx >> 1, 9, wz >> 1) < 0.2f) top = B_COARSE_DIRT;
        if (p.biome == BIOME_SAVANNA_PLATEAU && rnd01(seed, wx >> 1, 9, wz >> 1) < 0.15f) top = B_COARSE_DIRT;
        if (p.biome == BIOME_DARK_FOREST && rnd01(seed, wx, 10, wz) < 0.05f) top = B_PODZOL;
        if (p.biome == BIOME_PALE_GARDEN && rnd01(seed, wx >> 1, 11, wz >> 1) < 0.25f) top = B_PALE_MOSS_BLOCK;
        if (p.biome == BIOME_SWAMP && h <= SEA_LEVEL && rnd01(seed, wx >> 1, 12, wz >> 1) < 0.3f) top = B_CLAY;
        if (badl && h > SEA_LEVEL + 12) { top = BADLANDS_BANDS[(h + (int)(fbm(s, 22, wx / 60.0f, wz / 60.0f, 1) * 3)) & 15]; }
        bool under = h < SEA_LEVEL;
        if (under && BIOMES[p.biome].category == 0) top = fill = (rnd01(seed, wx >> 2, 13, wz >> 2) < 0.3f ? B_GRAVEL : (top == B_SAND || top == B_RED_SAND) ? top : B_DIRT);
        if (under && (BIOMES[p.biome].category == 1 || BIOMES[p.biome].category == 2)) {
            f32 rr = rnd01(seed, wx >> 2, 14, wz >> 2);
            top = fill = rr < 0.2f ? B_GRAVEL : rr < 0.28f ? B_CLAY : bd->under;
        }
        if (top == B_GRASS_BLOCK && under) top = B_DIRT;
        for (int y = 0; y < WORLD_H; y++) {
            u16 b = B_AIR;
            if (y == 0) b = B_BEDROCK;
            else if (y <= 4 && rnd01(seed, wx, y, wz) < 0.8f - y * 0.2f) b = B_BEDROCK;
            else if (y < h - depth) {
                if (badl && y > SEA_LEVEL - 6) b = BADLANDS_BANDS[(y + (int)(fbm(s, 22, wx / 60.0f, wz / 60.0f, 1) * 3)) & 15];
                else b = y < MCY(0) + (int)(rnd01(seed, wx, y, wz) * 8) ? B_DEEPSLATE : B_STONE;
            }
            else if (y < h) b = (fill == B_SAND && y < h - 2) ? B_SANDSTONE : (fill == B_RED_SAND && y < h - 2) ? B_RED_SANDSTONE : fill;
            else if (y == h) b = top;
            else if (y <= SEA_LEVEL) b = B_WATER;
            c->blocks[y][z][x] = b;
        }
        if (biome_has(p.biome, FEAT_FREEZE) && h < SEA_LEVEL && bd->precip == 2) c->blocks[SEA_LEVEL][z][x] = B_ICE;
    }

    // ---- caves (worms + caverns), lava below y=9, cave biome floors
    for (int z = 0; z < 16; z++) for (int x = 0; x < 16; x++) {
        int h = P[z + MARGIN][x + MARGIN].h;
        int wx = g.bx + x, wz = g.bz + z;
        bool wet = h <= SEA_LEVEL + 1;
        for (int y = 5; y <= h; y++) {
            if (wet && y > h - 6) break;
            u16 *b = &c->blocks[y][z][x];
            if (*b == B_BEDROCK || *b == B_WATER) continue;
            f32 n1 = noise3(wx / 42.0f + s->o[7][0], y / 26.0f, wz / 42.0f + s->o[7][1]);
            f32 n2 = noise3(wx / 42.0f + s->o[8][0], y / 26.0f + 31.0f, wz / 42.0f + s->o[8][1]);
            bool carve = n1 * n1 + n2 * n2 < 0.0045f;
            if (!carve && y < MCY(40)) {   // "cheese" caverns, larger the deeper you go
                f32 n3 = noise3(wx / 70.0f + s->o[9][0], y / 34.0f, wz / 70.0f + s->o[9][1]);
                carve = n3 > 0.56f - (MCY(40) - y) * 0.0018f;
            }
            if (carve) *b = y <= MCY(-55) ? B_LAVA : B_CAVE_AIR;
        }
    }

    // ---- ores and underground blobs (Minecraft 1.18+ distribution; y values are Minecraft y)
    ore_veins(&g, seed, B_DIRT, 70, 20, MCY(0), MCY(160), B_STONE, 0);
    ore_veins(&g, seed, B_GRAVEL, 70, 20, MCY(-64), MCY(160), B_STONE, 0);
    ore_veins(&g, seed, B_GRANITE, 40, 32, MCY(0), MCY(60), B_STONE, 0);
    ore_veins(&g, seed, B_DIORITE, 40, 32, MCY(0), MCY(60), B_STONE, 0);
    ore_veins(&g, seed, B_ANDESITE, 40, 32, MCY(0), MCY(60), B_STONE, 0);
    ore_veins(&g, seed, B_TUFF, 40, 32, MCY(-64), MCY(0), B_DEEPSLATE, 0);
    ore_veins(&g, seed, B_COAL_ORE, 300, 14, MCY(0), MCY(192), B_STONE, B_DEEPSLATE_COAL_ORE);
    ore_veins(&g, seed, B_COAL_ORE, 200, 12, MCY(136), MCY(319), B_STONE, 0);
    ore_veins(&g, seed, B_COPPER_ORE, 160, 10, MCY(-16), MCY(112), B_STONE, B_DEEPSLATE_COPPER_ORE);
    ore_veins(&g, seed, B_IRON_ORE, 100, 9, MCY(-24), MCY(56), B_STONE, B_DEEPSLATE_IRON_ORE);
    ore_veins(&g, seed, B_IRON_ORE, 100, 9, MCY(80), MCY(319), B_STONE, 0);
    ore_veins(&g, seed, B_IRON_ORE, 40, 4, MCY(-64), MCY(72), B_STONE, B_DEEPSLATE_IRON_ORE);
    ore_veins(&g, seed, B_GOLD_ORE, 40, 9, MCY(-64), MCY(32), B_STONE, B_DEEPSLATE_GOLD_ORE);
    ore_veins(&g, seed, B_GOLD_ORE, 5, 9, MCY(-64), MCY(-48), B_STONE, B_DEEPSLATE_GOLD_ORE);
    ore_veins(&g, seed, B_REDSTONE_ORE, 40, 8, MCY(-64), MCY(15), B_STONE, B_DEEPSLATE_REDSTONE_ORE);
    ore_veins(&g, seed, B_REDSTONE_ORE, 80, 8, MCY(-64), MCY(-32), B_STONE, B_DEEPSLATE_REDSTONE_ORE);
    ore_veins(&g, seed, B_LAPIS_ORE, 20, 7, MCY(-32), MCY(32), B_STONE, B_DEEPSLATE_LAPIS_ORE);
    ore_veins(&g, seed, B_LAPIS_ORE, 40, 7, MCY(-64), MCY(64), B_STONE, B_DEEPSLATE_LAPIS_ORE);
    ore_veins(&g, seed, B_DIAMOND_ORE, 70, 4, MCY(-64), MCY(16), B_STONE, B_DEEPSLATE_DIAMOND_ORE);
    ore_veins(&g, seed, B_DIAMOND_ORE, 2, 12, MCY(-64), MCY(-48), B_STONE, B_DEEPSLATE_DIAMOND_ORE);
    {   int b0 = P[8 + MARGIN][8 + MARGIN].biome;
        if (biome_has(b0, FEAT_EMERALD)) ore_veins(&g, seed, B_EMERALD_ORE, 100, 3, MCY(-16), MCY(319), B_STONE, B_DEEPSLATE_EMERALD_ORE);
        if (biome_has(b0, FEAT_TERRACOTTA)) ore_veins(&g, seed, B_GOLD_ORE, 50, 9, MCY(32), MCY(256), B_STONE, 0);
        if (biome_has(b0, FEAT_EMERALD) || b0 == BIOME_WINDSWEPT_HILLS || b0 == BIOME_STONY_PEAKS) ore_veins(&g, seed, B_INFESTED_STONE, 70, 9, MCY(-64), MCY(63), B_STONE, B_INFESTED_DEEPSLATE);
    }

    // ---- cave biome decoration
    {
        Rng cr = {hash3i(seed, c->cx, 777, c->cz)};
        for (int i = 0; i < 360; i++) {
            int x = rng_int(&cr, 0, 15), z = rng_int(&cr, 0, 15), y = rng_int(&cr, MCY(-58), MCY(60));
            u16 here = c->blocks[y][z][x], below = c->blocks[y - 1][z][x], above = c->blocks[y + 1][z][x];
            if (here != B_CAVE_AIR) continue;
            int wx = g.bx + x, wz = g.bz + z;
            int cb = worldgen_cave_biome(seed, wx, y, wz);
            bool floor = BLOCKS[below].opaque, ceil = BLOCKS[above].opaque;
            if (cb == BIOME_LUSH_CAVES) {
                if (floor) {
                    c->blocks[y - 1][z][x] = B_MOSS_BLOCK;
                    int k = rng_int(&cr, 0, 9);
                    c->blocks[y][z][x] = k < 4 ? B_MOSS_CARPET : k < 6 ? B_SHORT_GRASS : k == 6 ? B_AZALEA : k == 7 ? B_FLOWERING_AZALEA : k == 8 ? B_SMALL_DRIPLEAF : B_BIG_DRIPLEAF;
                } else if (ceil) {
                    int len = rng_int(&cr, 1, 5);
                    for (int j = 0; j < len && c->blocks[y - j][z][x] == B_CAVE_AIR; j++) c->blocks[y - j][z][x] = B_CAVE_VINES + (rng_int(&cr, 0, 2) == 0);
                    c->blocks[y + 1][z][x] = B_MOSS_BLOCK;
                }
            } else if (cb == BIOME_DRIPSTONE_CAVES) {
                if (floor) { c->blocks[y - 1][z][x] = B_DRIPSTONE_BLOCK; c->blocks[y][z][x] = B_POINTED_DRIPSTONE; }
                else if (ceil) { c->blocks[y + 1][z][x] = B_DRIPSTONE_BLOCK; c->blocks[y][z][x] = B_POINTED_DRIPSTONE + 1; }
            } else if (cb == BIOME_DEEP_DARK) {
                if (floor) {
                    for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) {
                        int xx = x + dx, zz = z + dz;
                        if (xx < 0 || xx > 15 || zz < 0 || zz > 15) continue;
                        if (BLOCKS[c->blocks[y - 1][zz][xx]].opaque) c->blocks[y - 1][zz][xx] = B_SCULK;
                    }
                    int k = rng_int(&cr, 0, 19);
                    c->blocks[y][z][x] = k < 2 ? B_SCULK_SENSOR : k == 2 ? B_SCULK_SHRIEKER + 1 : k == 3 ? B_SCULK_CATALYST : k < 8 ? B_SCULK_VEIN : B_CAVE_AIR;
                }
            } else if (ceil && rng_int(&cr, 0, 3) == 0) {
                c->blocks[y][z][x] = B_GLOW_LICHEN;
            } else if (ceil && rng_int(&cr, 0, 6) == 0 && BLOCKS[above].base == B_DIRT) {
                c->blocks[y][z][x] = B_HANGING_ROOTS;
            }
        }
    }

    // ---- stronghold portal room
    {
        ivec3 sh = worldgen_stronghold(seed);
        if (abs(sh.x - g.bx - 8) < 24 && abs(sh.z - g.bz - 8) < 24) {
            Rng sr = {hash64(seed ^ 0xABCDULL)};
            for (int dy = -1; dy <= 7; dy++) for (int dz = -7; dz <= 7; dz++) for (int dx = -5; dx <= 5; dx++) {
                bool shell = dy == -1 || dy == 7 || abs(dz) == 7 || abs(dx) == 5;
                u16 wall = rng_int(&sr, 0, 4) == 0 ? B_MOSSY_STONE_BRICKS : rng_int(&sr, 0, 6) == 0 ? B_CRACKED_STONE_BRICKS : B_STONE_BRICKS;
                gput(&g, sh.x + dx, sh.y + dy, sh.z + dz, shell ? wall : B_AIR, false);
            }
            for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) gput(&g, sh.x + dx, sh.y, sh.z + dz + 2, B_LAVA, false);
            // twelve portal frames facing inward around a 3x3 hole, each with a 10 % chance to already hold an eye
            for (int i = -1; i <= 1; i++) {
                int ex = rnd01(seed, i, 1, 1) < 0.1f;
                gput(&g, sh.x + i, sh.y + 1, sh.z + 2 - 2, B_END_PORTAL_FRAME + (rnd01(seed, i, 2, 0) < 0.1f), false);
                gput(&g, sh.x + i, sh.y + 1, sh.z + 2 + 2, B_END_PORTAL_FRAME + (rnd01(seed, i, 2, 1) < 0.1f), false);
                gput(&g, sh.x - 2, sh.y + 1, sh.z + 2 + i, B_END_PORTAL_FRAME + (rnd01(seed, i, 2, 2) < 0.1f), false);
                gput(&g, sh.x + 2, sh.y + 1, sh.z + 2 + i, B_END_PORTAL_FRAME + (rnd01(seed, i, 2, 3) < 0.1f), false);
                (void)ex;
            }
            for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) gput(&g, sh.x + dx, sh.y + 1, sh.z + 2 + dz, B_AIR, false);
            gput(&g, sh.x, sh.y, sh.z - 5, B_SPAWNER, false);
            for (int dz = -6; dz <= -4; dz++) gput(&g, sh.x, sh.y + 1, sh.z + dz, B_STONE_BRICK_STAIRS, false);
            gput(&g, sh.x - 4, sh.y + 3, sh.z, B_WALL_TORCH + 1, false);
            gput(&g, sh.x + 4, sh.y + 3, sh.z, B_WALL_TORCH + 3, false);
        }
    }

    // ---- trees (from this column and the margin, written only inside this column)
    for (int z = 0; z < SPAN; z++) for (int x = 0; x < SPAN; x++) {
        ColParams p = P[z][x];
        const BiomeDef *bd = &BIOMES[p.biome];
        int wx = g.bx + x - MARGIN, wz = g.bz + z - MARGIN;
        if (p.h < SEA_LEVEL - (p.biome == BIOME_MANGROVE_SWAMP ? 3 : 0) || p.h > WORLD_H - 40 || bd->tree_density <= 0) continue;
        if (!local_best(seed, wx, wz, 4242, bd->tree_density, bd->tree_density > 0.05f ? 1 : 2)) continue;
        int nk = 0; while (nk < 8 && bd->trees[nk] != TREE_NONE) nk++;
        if (!nk) continue;
        Rng tr = {hash3i(seed, wx, 99, wz)};
        int kind = bd->trees[rng_int(&tr, 0, nk - 1)];
        int y = p.h + 1;
        u16 ground = (x >= MARGIN && x < MARGIN + 16 && z >= MARGIN && z < MARGIN + 16) ? c->blocks[p.h][z - MARGIN][x - MARGIN] : bd->top;
        if (ground == B_WATER || (BLOCKS[ground].liquid)) continue;
        if (p.h < SEA_LEVEL && p.biome != BIOME_MANGROVE_SWAMP) continue;
        tree(&g, kind, wx, y, wz, &tr);
        if (kind != TREE_HUGE_RED_MUSHROOM && kind != TREE_HUGE_BROWN_MUSHROOM && kind < TREE_FALLEN_OAK && kind != TREE_MANGROVE) {
            gput(&g, wx, p.h, wz, B_DIRT, false);
            if (kind == TREE_DARK_OAK || kind == TREE_PALE_OAK || kind == TREE_MEGA_SPRUCE || kind == TREE_MEGA_PINE || kind == TREE_MEGA_JUNGLE)
                for (int dx = 0; dx < 2; dx++) for (int dz = 0; dz < 2; dz++) gput(&g, wx + dx, p.h, wz + dz, B_DIRT, false);
        }
    }
    // boulders and ice spikes / icebergs (also seeded from the margin)
    for (int z = 0; z < SPAN; z++) for (int x = 0; x < SPAN; x++) {
        ColParams p = P[z][x];
        int wx = g.bx + x - MARGIN, wz = g.bz + z - MARGIN;
        if (biome_has(p.biome, FEAT_BOULDERS) && p.h > SEA_LEVEL && local_best(seed, wx, wz, 5151, 0.004f, 3)) {
            Rng br = {hash3i(seed, wx, 5, wz)};
            leaf_blob(&g, wx, p.h + 1, wz, 2, 2, B_MOSSY_COBBLESTONE, &br);
        }
        if (biome_has(p.biome, FEAT_ICE_SPIKES) && local_best(seed, wx, wz, 6161, 0.008f, 3)) {
            Rng br = {hash3i(seed, wx, 6, wz)};
            int h = rng_int(&br, 6, 14) + (rng_int(&br, 0, 9) == 0 ? 18 : 0);
            for (int dy = 0; dy < h; dy++) {
                int rr = (int)((1.0f - (f32)dy / h) * 2.5f);
                for (int dx = -rr; dx <= rr; dx++) for (int dz = -rr; dz <= rr; dz++) if (dx * dx + dz * dz <= rr * rr + 1) gput(&g, wx + dx, p.h + 1 + dy, wz + dz, B_PACKED_ICE, false);
            }
        }
        if (biome_has(p.biome, FEAT_ICEBERG) && local_best(seed, wx, wz, 7171, 0.004f, 4)) {
            Rng br = {hash3i(seed, wx, 7, wz)};
            int h = rng_int(&br, 4, 10);
            for (int dy = -h; dy < h; dy++) {
                int rr = (int)((1.0f - fabsf((f32)dy) / h) * 5);
                for (int dx = -rr; dx <= rr; dx++) for (int dz = -rr; dz <= rr; dz++) if (dx * dx + dz * dz <= rr * rr) gput(&g, wx + dx, SEA_LEVEL + dy, wz + dz, dy > h / 2 ? B_SNOW_BLOCK : rng_int(&br, 0, 6) ? B_PACKED_ICE : B_BLUE_ICE, false);
            }
        }
    }

    // ---- ground cover (inside the column only)
    for (int z = 0; z < 16; z++) for (int x = 0; x < 16; x++) {
        ColParams p = P[z + MARGIN][x + MARGIN];
        const BiomeDef *bd = &BIOMES[p.biome];
        int wx = g.bx + x, wz = g.bz + z, y = p.h + 1;
        if (y >= WORLD_H - 2) continue;
        u16 ground = c->blocks[p.h][z][x];
        u16 *at = &c->blocks[y][z][x], *up = &c->blocks[y + 1][z][x];
        f32 r = rnd01(seed, wx, 99, wz), r2 = rnd01(seed, wx, 98, wz);
        bool water = *at == B_WATER;
        if (water) {   // underwater plants
            int depth = SEA_LEVEL - p.h;
            if (biome_has(p.biome, FEAT_CORAL) && depth >= 2 && r < 0.35f) {
                static const u16 CB[5] = {B_TUBE_CORAL_BLOCK, B_BRAIN_CORAL_BLOCK, B_BUBBLE_CORAL_BLOCK, B_FIRE_CORAL_BLOCK, B_HORN_CORAL_BLOCK};
                static const u16 CP[5] = {B_TUBE_CORAL, B_BRAIN_CORAL, B_BUBBLE_CORAL, B_FIRE_CORAL, B_HORN_CORAL};
                static const u16 CF[5] = {B_TUBE_CORAL_FAN, B_BRAIN_CORAL_FAN, B_BUBBLE_CORAL_FAN, B_FIRE_CORAL_FAN, B_HORN_CORAL_FAN};
                int k = (int)(rnd01(seed, wx >> 2, 97, wz >> 2) * 5) % 5;
                if (r < 0.12f) { *at = CB[k]; if (depth > 3 && r2 < 0.5f) *up = CP[k]; }
                else *at = r2 < 0.5f ? CP[k] : CF[k];
            } else if (biome_has(p.biome, FEAT_SEA_PICKLE) && r < 0.4f && r > 0.38f) *at = B_SEA_PICKLE;
            else if (biome_has(p.biome, FEAT_KELP) && depth >= 4 && r < 0.12f) {
                int len = 2 + (int)(r2 * (depth - 2));
                for (int i = 0; i < len && y + i < SEA_LEVEL; i++) c->blocks[y + i][z][x] = (i == len - 1 || y + i == SEA_LEVEL - 1) ? B_KELP : B_KELP_PLANT;
            } else if (biome_has(p.biome, FEAT_SEAGRASS) && r < 0.35f) {
                if (r < 0.06f && depth >= 3) { *at = B_TALL_SEAGRASS; *up = B_TALL_SEAGRASS + 1; } else *at = B_SEAGRASS;
            }
            if (biome_has(p.biome, FEAT_LILY) && depth <= 2 && r2 > 0.9f && c->blocks[SEA_LEVEL + 1][z][x] == B_AIR) c->blocks[SEA_LEVEL + 1][z][x] = B_LILY_PAD;
            continue;
        }
        if (*at != B_AIR) continue;
        bool grassy = ground == B_GRASS_BLOCK || ground == B_PODZOL || ground == B_COARSE_DIRT || ground == B_PALE_MOSS_BLOCK || ground == B_MYCELIUM;
        bool sandy = ground == B_SAND || ground == B_RED_SAND || (BLOCKS[ground].key && strstr(BLOCKS[ground].key, "terracotta"));
        bool near_water = false;
        for (int k = 0; k < 4 && !near_water; k++) {
            int nx = x + FACE_DIR[H4_FACE[k]].x, nz = z + FACE_DIR[H4_FACE[k]].z;
            if (nx >= 0 && nx < 16 && nz >= 0 && nz < 16) near_water = c->blocks[p.h][nz][nx] == B_WATER;
        }
        if (biome_has(p.biome, FEAT_SUGAR_CANE) && near_water && (grassy || sandy || ground == B_DIRT) && r < 0.12f) {
            int h = 1 + (int)(r2 * 3);
            for (int i = 0; i < h; i++) c->blocks[y + i][z][x] = B_SUGAR_CANE;
            continue;
        }
        if (sandy) {
            if (biome_has(p.biome, FEAT_CACTUS) && r < 0.006f && x > 0 && x < 15 && z > 0 && z < 15) {
                int h = 1 + (int)(r2 * 3);
                for (int i = 0; i < h; i++) c->blocks[y + i][z][x] = B_CACTUS;
                if (r2 > 0.8f) c->blocks[y + h][z][x] = B_CACTUS_FLOWER;
            } else if (biome_has(p.biome, FEAT_DEAD_BUSH) && r < 0.014f) *at = B_DEAD_BUSH;
            else if (biome_has(p.biome, FEAT_DRY_GRASS) && r < 0.03f) *at = r2 < 0.7f ? B_SHORT_DRY_GRASS : B_TALL_DRY_GRASS;
            continue;
        }
        if (ground == B_MUD && biome_has(p.biome, FEAT_FIREFLY) && r < 0.01f) { *at = B_FIREFLY_BUSH; continue; }
        if (!grassy) {
            if (ground == B_SNOW_BLOCK || ground == B_STONE || ground == B_GRAVEL) {
                if (bd->precip == 2 && ground != B_SNOW_BLOCK) *at = B_SNOW;
                if (biome_has(p.biome, FEAT_POWDER_SNOW) && ground == B_SNOW_BLOCK && r < 0.02f) c->blocks[p.h][z][x] = B_POWDER_SNOW;
            }
            continue;
        }
        // flowers & grass
        f32 fd = bd->flower_density, gd = bd->grass_density;
        if (bd->nflowers && r < fd) {
            u16 f = bd->flowers[(int)(rnd01(seed, wx >> 3, 96, wz >> 3) * bd->nflowers) % bd->nflowers];
            if (BLOCKS[f].place == PL_DOUBLE) { *at = f; *up = f + 1; } else *at = f;
        } else if (r < fd + gd) {
            bool fern = p.biome == BIOME_TAIGA || p.biome == BIOME_SNOWY_TAIGA || p.biome == BIOME_OLD_GROWTH_PINE_TAIGA || p.biome == BIOME_OLD_GROWTH_SPRUCE_TAIGA ||
                        p.biome == BIOME_JUNGLE || p.biome == BIOME_GROVE;
            bool tall = r2 < (p.biome == BIOME_SAVANNA || p.biome == BIOME_PLAINS ? 0.12f : 0.08f);
            u16 k = fern && r2 > 0.5f ? (tall ? B_LARGE_FERN : B_FERN) : (tall ? B_TALL_GRASS : B_SHORT_GRASS);
            if (BLOCKS[k].place == PL_DOUBLE) { *at = k; *up = k + 1; } else *at = k;
        } else {
            f32 q = r - fd - gd;
            if (biome_has(p.biome, FEAT_PUMPKIN) && q < 0.0006f) *at = B_PUMPKIN;
            else if (biome_has(p.biome, FEAT_MELON) && q < 0.004f) *at = B_MELON;
            else if (biome_has(p.biome, FEAT_BERRIES) && q < 0.004f) *at = B_SWEET_BERRY_BUSH + 3;
            else if (biome_has(p.biome, FEAT_BAMBOO) && q < (p.biome == BIOME_BAMBOO_JUNGLE ? 0.12f : 0.01f)) {
                int h = 4 + (int)(r2 * 10);
                for (int i = 0; i < h && y + i < WORLD_H - 1; i++) c->blocks[y + i][z][x] = B_BAMBOO;
                c->blocks[p.h][z][x] = B_PODZOL;
            } else if (biome_has(p.biome, FEAT_MUSHROOMS) && q < 0.004f) *at = r2 < 0.5f ? B_RED_MUSHROOM : B_BROWN_MUSHROOM;
            else if (biome_has(p.biome, FEAT_LEAF_LITTER) && q < 0.03f) *at = B_LEAF_LITTER;
            else if (biome_has(p.biome, FEAT_WILDFLOWERS) && q < 0.03f) *at = B_WILDFLOWERS;
            else if (biome_has(p.biome, FEAT_PETALS) && q < 0.12f) *at = B_PINK_PETALS;
            else if (biome_has(p.biome, FEAT_EYEBLOSSOM) && q < 0.02f) *at = r2 < 0.5f ? B_OPEN_EYEBLOSSOM : B_CLOSED_EYEBLOSSOM;
            else if (biome_has(p.biome, FEAT_PALE_MOSS) && q < 0.08f) *at = B_PALE_MOSS_CARPET;
            else if (biome_has(p.biome, FEAT_BUSHES) && q < 0.004f) *at = B_BUSH;
            else if (biome_has(p.biome, FEAT_FIREFLY) && q < 0.004f) *at = B_FIREFLY_BUSH;
        }
        if (bd->precip == 2 && *at == B_AIR) *at = B_SNOW;
        if (bd->precip == 2 && ground == B_GRASS_BLOCK) c->blocks[p.h][z][x] = B_GRASS_BLOCK + 1;   // snowy grass
    }
    // snow on leaves in snowy biomes
    for (int z = 0; z < 16; z++) for (int x = 0; x < 16; x++) {
        if (BIOMES[c->biome[z][x]].precip != 2) continue;
        for (int y = WORLD_H - 2; y > SEA_LEVEL; y--) {
            u16 b = c->blocks[y][z][x];
            if (b == B_AIR) continue;
            if (block_is_leaves(b) && c->blocks[y + 1][z][x] == B_AIR) c->blocks[y + 1][z][x] = B_SNOW;
            break;
        }
    }
}

static void nether_column(Column *c, u64 seed, const Seeds *s) {
    Gen g = {c, c->cx * 16, c->cz * 16};
    for (int z = 0; z < 16; z++) for (int x = 0; x < 16; x++) {
        int wx = g.bx + x, wz = g.bz + z;
        int bio = nether_biome(s, wx, wz);
        c->biome[z][x] = (u8)bio;
        const BiomeDef *bd = &BIOMES[bio];
        for (int y = 0; y < WORLD_H; y++) {
            u16 b;
            if (y >= NETHER_H) b = B_AIR;   // above the bedrock roof (Minecraft's nether is 128 high)
            else if (y == 0 || y == NETHER_H - 1) b = B_BEDROCK;
            else if (y <= 4 && rnd01(seed, wx, y, wz) < 0.6f - y * 0.12f) b = B_BEDROCK;
            else if (y >= NETHER_H - 5 && rnd01(seed, wx, y, wz) < 0.6f - (NETHER_H - 1 - y) * 0.12f) b = B_BEDROCK;
            else if (nether_solid(s, wx, y, wz)) b = bio == BIOME_BASALT_DELTAS && rnd01(seed, wx >> 1, y >> 1, wz >> 1) < 0.5f ? B_BASALT : bio == BIOME_SOUL_SAND_VALLEY && y < 40 ? B_SOUL_SOIL : B_NETHERRACK;
            else b = y <= NETHER_LAVA_LEVEL ? B_LAVA : B_AIR;
            c->blocks[y][z][x] = b;
        }
        // surface layer on every exposed floor
        for (int y = 1; y < NETHER_H - 2; y++) {
            u16 b = c->blocks[y][z][x], a = c->blocks[y + 1][z][x];
            if (b != B_NETHERRACK && b != B_SOUL_SOIL && b != B_BASALT) continue;
            if (a != B_AIR) continue;
            f32 r = rnd01(seed, wx, y, wz);
            if (bio == BIOME_CRIMSON_FOREST || bio == BIOME_WARPED_FOREST) c->blocks[y][z][x] = bd->top;
            else if (bio == BIOME_SOUL_SAND_VALLEY) c->blocks[y][z][x] = r < 0.6f ? B_SOUL_SAND : B_SOUL_SOIL;
            else if (bio == BIOME_BASALT_DELTAS) c->blocks[y][z][x] = r < 0.5f ? B_BASALT : B_BLACKSTONE;
            else if (y >= NETHER_LAVA_LEVEL - 1 && y <= NETHER_LAVA_LEVEL + 2 && r < 0.3f) c->blocks[y][z][x] = r < 0.15f ? B_SOUL_SAND : B_GRAVEL;
            if (y == NETHER_LAVA_LEVEL && r > 0.9f) c->blocks[y][z][x] = B_MAGMA_BLOCK;
        }
    }
    ore_veins(&g, seed, B_NETHER_QUARTZ_ORE, 160, 12, 10, 117, B_NETHERRACK, 0);
    ore_veins(&g, seed, B_NETHER_GOLD_ORE, 100, 9, 10, 117, B_NETHERRACK, 0);
    ore_veins(&g, seed, B_ANCIENT_DEBRIS, 3, 2, 8, 24, B_NETHERRACK, 0);
    ore_veins(&g, seed, B_MAGMA_BLOCK, 40, 12, 26, 36, B_NETHERRACK, 0);
    ore_veins(&g, seed, B_BLACKSTONE, 20, 16, 5, 30, B_NETHERRACK, 0);
    // decoration
    Rng r = {hash3i(seed, c->cx, 4040, c->cz)};
    for (int i = 0; i < 220; i++) {
        int x = rng_int(&r, 0, 15), z = rng_int(&r, 0, 15), y = rng_int(&r, NETHER_LAVA_LEVEL + 1, 118);
        int wx = g.bx + x, wz = g.bz + z;
        u16 b = c->blocks[y][z][x], below = c->blocks[y - 1][z][x], above = c->blocks[y + 1][z][x];
        if (b != B_AIR) continue;
        int bio = c->biome[z][x];
        if (BLOCKS[above].opaque && rng_int(&r, 0, 30) == 0) {   // glowstone clusters under the ceiling
            Rng gr = {hash3i(seed, wx, y, wz)};
            for (int k = 0; k < 18; k++) {
                int gx = x + rng_int(&gr, -2, 2), gy = y - rng_int(&gr, 0, 3), gz = z + rng_int(&gr, -2, 2);
                if (gx >= 0 && gx < 16 && gz >= 0 && gz < 16 && gy > 1 && c->blocks[gy][gz][gx] == B_AIR) c->blocks[gy][gz][gx] = B_GLOWSTONE;
            }
            continue;
        }
        if (!BLOCKS[below].opaque) continue;
        int k = rng_int(&r, 0, 99);
        if (bio == BIOME_CRIMSON_FOREST || bio == BIOME_WARPED_FOREST) {
            bool crim = bio == BIOME_CRIMSON_FOREST;
            if (k < 6 && x > 1 && x < 14 && z > 1 && z < 14) tree(&g, crim ? TREE_CRIMSON_FUNGUS : TREE_WARPED_FUNGUS, wx, y, wz, &r);
            else if (k < 50) c->blocks[y][z][x] = crim ? (k < 40 ? B_CRIMSON_ROOTS : B_CRIMSON_FUNGUS) : (k < 30 ? B_WARPED_ROOTS : k < 42 ? B_NETHER_SPROUTS : B_WARPED_FUNGUS);
            else if (!crim && k < 55) for (int v = 0; v < rng_int(&r, 1, 6) && c->blocks[y + v][z][x] == B_AIR; v++) c->blocks[y + v][z][x] = B_TWISTING_VINES;
        } else if (bio == BIOME_BASALT_DELTAS) {
            if (k < 8) for (int v = 0; v < rng_int(&r, 2, 8) && c->blocks[y + v][z][x] == B_AIR; v++) c->blocks[y + v][z][x] = B_BASALT;
        } else if (bio == BIOME_NETHER_WASTES) {
            if (k < 3) c->blocks[y][z][x] = k ? B_RED_MUSHROOM : B_BROWN_MUSHROOM;
            else if (k < 6 && below == B_NETHERRACK) c->blocks[y][z][x] = B_FIRE;
        } else if (bio == BIOME_SOUL_SAND_VALLEY) {
            if (k < 2 && below == B_SOUL_SOIL) c->blocks[y][z][x] = B_SOUL_FIRE;
            else if (k < 4) for (int v = 0; v < rng_int(&r, 3, 9) && c->blocks[y + v][z][x] == B_AIR; v++) c->blocks[y + v][z][x] = B_BONE_BLOCK;
        }
    }
}

static void end_column_gen(Column *c, u64 seed, const Seeds *s) {
    Gen g = {c, c->cx * 16, c->cz * 16};
    for (int z = 0; z < 16; z++) for (int x = 0; x < 16; x++) {
        int wx = g.bx + x, wz = g.bz + z;
        c->biome[z][x] = (u8)end_biome(wx, wz);
        int lo, hi; end_column(s, wx, wz, &lo, &hi);
        for (int y = 0; y < WORLD_H; y++) c->blocks[y][z][x] = (y >= lo && y <= hi) ? B_END_STONE : B_AIR;
    }
    // obsidian pillars around the main island (ten of them, radius 42)
    for (int i = 0; i < 10; i++) {
        f32 a = 2 * PI_F * (-i) / 10.0f;   // Minecraft's spike placement
        int px = (int)floorf(42 * cosf(a)), pz = (int)floorf(42 * sinf(a));
        int rad = 2 + (int)(hash64(seed + i) % 3), top = 76 + (int)(hash64(seed * 7 + i) % 30);
        if (top > END_H - 6) top = END_H - 6;
        for (int dz = -rad; dz <= rad; dz++) for (int dx = -rad; dx <= rad; dx++) {
            if (dx * dx + dz * dz > rad * rad + 1) continue;
            for (int y = 44; y <= top; y++) gput(&g, px + dx, y, pz + dz, B_OBSIDIAN, false);
        }
        gput(&g, px, top + 1, pz, B_BEDROCK, false);
        if (i % 4 == 1) for (int dx = -2; dx <= 2; dx++) for (int dz = -2; dz <= 2; dz++) for (int dy = 1; dy <= 4; dy++)   // caged crystals
            if (abs(dx) == 2 || abs(dz) == 2 || dy == 4) gput(&g, px + dx, top + dy, pz + dz, B_IRON_BARS, false);
    }
    // the (inactive) exit portal frame at the centre
    for (int dz = -3; dz <= 3; dz++) for (int dx = -3; dx <= 3; dx++) {
        int d2 = dx * dx + dz * dz;
        if (d2 > 12) continue;
        gput(&g, dx, 60, dz, B_BEDROCK, false);
        if (d2 >= 6) gput(&g, dx, 61, dz, B_BEDROCK, false);
    }
    for (int y = 61; y <= 64; y++) gput(&g, 0, y, 0, B_BEDROCK, false);
    gput(&g, 1, 63, 0, B_WALL_TORCH + 1, false); gput(&g, -1, 63, 0, B_WALL_TORCH + 3, false);
    gput(&g, 0, 63, 1, B_WALL_TORCH + 2, false); gput(&g, 0, 63, -1, B_WALL_TORCH + 0, false);
    // oxygen blooms on the outer asteroids
    {   Rng ro = {hash3i(seed, c->cx, 6060, c->cz)};
        for (int i = 0; i < 3; i++) {
            int x = rng_int(&ro, 1, 14), z = rng_int(&ro, 1, 14);
            if (rng_f(&ro) > 0.5f || (abs(g.bx + x) < 40 && abs(g.bz + z) < 40)) continue;
            for (int y = END_H - 2; y > 1; y--) if (c->blocks[y][z][x] == B_END_STONE) { if (c->blocks[y + 1][z][x] == B_AIR) c->blocks[y + 1][z][x] = B_OXYGEN_BLOOM; break; }
        }
    }
    // chorus on the outer islands
    Rng r = {hash3i(seed, c->cx, 5050, c->cz)};
    for (int i = 0; i < 6; i++) {
        int x = rng_int(&r, 2, 13), z = rng_int(&r, 2, 13);
        if (c->biome[z][x] != BIOME_END_HIGHLANDS && c->biome[z][x] != BIOME_END_MIDLANDS) continue;
        for (int y = WORLD_H - 2; y > 1; y--) if (c->blocks[y][z][x] == B_END_STONE) { if (rng_int(&r, 0, 2) == 0) tree(&g, TREE_CHORUS, g.bx + x, y + 1, g.bz + z, &r); break; }
    }
    // obsidian arrival platform (Minecraft puts it at 100, 49, 0)
    for (int dz = -2; dz <= 2; dz++) for (int dx = -2; dx <= 2; dx++) gput(&g, 100 + dx, 48, dz, B_OBSIDIAN, false);
}

static void sift_column_gen(Column *c, u64 seed, const Seeds *s) {
    Gen g = {c, c->cx * 16, c->cz * 16};
    for (int z = 0; z < 16; z++) for (int x = 0; x < 16; x++) {
        int wx = g.bx + x, wz = g.bz + z;
        int bi = sift_biome(s, wx, wz);
        c->biome[z][x] = (u8)bi;
        for (int y = 0; y < WORLD_H; y++) c->blocks[y][z][x] = B_AIR;
        // the cloud sea: thick puffy cloud banks with gaps that drop into the void
        f32 cn = fbm(s, 26, wx / 36.0f, wz / 36.0f, 3);
        if (cn > -0.25f) {
            int th = 1 + (int)((cn + 0.25f) * 4);
            for (int y = SIFT_CLOUD_Y - th / 2; y <= SIFT_CLOUD_Y + th; y++) c->blocks[y][z][x] = B_CLOUD_BLOCK;
        }
        int lo, hi; sift_column_shape(s, wx, wz, &lo, &hi);
        for (int y = lo; y <= hi; y++) {
            u16 b = y == hi ? B_GOLDEN_GRASS_BLOCK : B_HOLY_STONE;
            if (y < hi && y > lo + 2 && rnd01(seed ^ 0x51F7, wx, y, wz) < 0.004f) b = B_RADIANT_CRYSTAL;
            c->blocks[y][z][x] = b;
        }
        // small floating cloud puffs high in the sky
        f32 pn = fbm(s, 27, wx / 18.0f, wz / 18.0f, 2);
        if (pn > 0.62f) for (int y = 170; y < 170 + (int)((pn - 0.62f) * 12) + 1; y++) c->blocks[y][z][x] = B_CLOUD_BLOCK;
    }
    // decoration (kept inside the column so borders stay seamless)
    Rng r = {hash3i(seed, c->cx, 7171, c->cz)};
    for (int z = 0; z < 16; z++) for (int x = 0; x < 16; x++) {
        int bi = c->biome[z][x];
        if (bi == BIOME_CLOUD_SEA) continue;
        int y = WORLD_H - 2;
        while (y > 0 && c->blocks[y][z][x] != B_GOLDEN_GRASS_BLOCK) y--;
        if (y <= 0 || c->blocks[y + 1][z][x] != B_AIR) continue;
        f32 roll = rng_f(&r);
        const BiomeDef *B = &BIOMES[bi];
        if (x >= 2 && x <= 13 && z >= 2 && z <= 13 && roll < B->tree_density) tree(&g, TREE_CRYSTAL, g.bx + x, y + 1, g.bz + z, &r);
        else if (roll < B->tree_density + B->flower_density && B->nflowers) c->blocks[y + 1][z][x] = B->flowers[rng_int(&r, 0, B->nflowers - 1)];
        else if (roll < B->tree_density + B->flower_density + B->grass_density) c->blocks[y + 1][z][x] = B_SHORT_GRASS;
    }
}

void worldgen_column(Column *c, u64 seed) {
    Seeds s; seeds_of(seed, &s);
    switch (g_dim) {
    case DIM_NETHER: nether_column(c, seed, &s); break;
    case DIM_END: end_column_gen(c, seed, &s); break;
    case DIM_SIFT: sift_column_gen(c, seed, &s); break;
    default: overworld_column(c, seed, &s); break;
    }
    structures_generate(c, seed, g_dim);
    c->state = COL_GENERATED;
}

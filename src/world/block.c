#include "block.h"

// BLOCKS[] and SHAPES[] live in src/gen/content_blocks.c (generated).
#define BKEY_HASH 4096
static u16 bkey_table[BKEY_HASH];
static u32 bstr_hash(const char *s) { u32 h = 2166136261u; while (*s) h = (h ^ (u8)*s++) * 16777619u; return h; }

void blocks_init(void) {
    memset(bkey_table, 0xFF, sizeof bkey_table);
    for (int i = 0; i < BLOCK_STATES; i++) {
        if (BLOCKS[i].base != i) continue;
        u32 h = bstr_hash(BLOCKS[i].key) & (BKEY_HASH - 1);
        while (bkey_table[h] != 0xFFFF) h = (h + 1) & (BKEY_HASH - 1);
        bkey_table[h] = (u16)i;
    }
}
Block block_by_key(const char *key) {
    if (!key) return 0;
    if (!strncmp(key, "minecraft:", 10)) key += 10;
    u32 h = bstr_hash(key) & (BKEY_HASH - 1);
    while (bkey_table[h] != 0xFFFF) { if (!strcmp(BLOCKS[bkey_table[h]].key, key)) return bkey_table[h]; h = (h + 1) & (BKEY_HASH - 1); }
    return 0;
}
int block_age_max(Block b) { return BLOCKS[b].mesh == MESH_CROP ? BLOCKS[b].nvar - 1 : 0; }

// ------------------------------------------------------------------ stair corners
// The step of a straight stair covers half the footprint; corners cover one (outer) or three
// (inner) quarters, decided from the neighbouring stairs exactly like Minecraft.
static int step_box(const BlockShape *sh) {   // index of the half-footprint box, -1 if none
    for (int i = 0; i < sh->n; i++) { const u8 *q = sh->b[i]; if ((q[3] - q[0]) * (q[5] - q[2]) == 128) return i; }
    return -1;
}
static int step_side(Block b) {   // h4 side (N E S W) the step occupies
    const BlockShape *sh = &SHAPES[BLOCKS[b].shape];
    int i = step_box(sh);
    if (i < 0) return -1;
    const u8 *q = sh->b[i];
    if (q[3] - q[0] == 16) return (q[2] + q[5]) / 2 < 8 ? 0 : 2;
    return (q[0] + q[3]) / 2 < 8 ? 3 : 1;
}
static bool stair_bottom(Block b) { const BlockShape *sh = &SHAPES[BLOCKS[b].shape]; int i = step_box(sh); return i >= 0 && sh->b[i][1] >= 8; }
static int side_mask(int s) { return s == 0 ? 3 : s == 2 ? 12 : s == 3 ? 5 : 10; }   // quadrant bits: (z>=8)*2 + (x>=8)
static bool is_stair(Block b) { return BLOCKS[b].place == PL_STAIRS && step_side(b) >= 0; }
int stair_shape(Block b, u16 (*get)(int, int, int), int x, int y, int z, BlockShape *out) {
    const BlockShape *sh = &SHAPES[BLOCKS[b].shape];
    *out = *sh;
    int f = step_side(b), si = step_box(sh);
    if (f < 0) return out->n;
    bool bottom = stair_bottom(b);
    int mask = side_mask(f);
    int fx = FACE_DIR[H4_FACE[f]].x, fz = FACE_DIR[H4_FACE[f]].z;
    Block behind = get(x + fx, y, z + fz), front = get(x - fx, y, z - fz);
    #define SAME(o) (is_stair(o) && stair_bottom(o) == bottom)
    #define CAN(dx, dz) (!(SAME(get(x + (dx), y, z + (dz))) && step_side(get(x + (dx), y, z + (dz))) == f))
    if (SAME(behind) && (step_side(behind) & 1) != (f & 1)) {
        int g = step_side(behind);
        if (CAN(-FACE_DIR[H4_FACE[g]].x, -FACE_DIR[H4_FACE[g]].z)) mask = side_mask(f) & side_mask(g);
    } else if (SAME(front) && (step_side(front) & 1) != (f & 1)) {
        int g = step_side(front);
        if (CAN(FACE_DIR[H4_FACE[g]].x, FACE_DIR[H4_FACE[g]].z)) mask = side_mask(f) | side_mask(g);
    }
    #undef SAME
    #undef CAN
    if (mask == side_mask(f)) return out->n;
    // rebuild: keep every box except the step, then one box per quadrant
    u8 y0 = sh->b[si][1], y1 = sh->b[si][4];
    int n = 0;
    for (int i = 0; i < sh->n; i++) if (i != si) memcpy(out->b[n++], sh->b[i], 6);
    for (int qd = 0; qd < 4 && n < 8; qd++) if (mask & (1 << qd)) {
        u8 bx = (u8)((qd & 1) * 8), bz = (u8)((qd >> 1) * 8);
        u8 box[6] = {bx, y0, bz, (u8)(bx + 8), y1, (u8)(bz + 8)};
        memcpy(out->b[n++], box, 6);
    }
    out->n = (u8)n;
    return n;
}

// ------------------------------------------------------------------ fire odds (FireBlock.bootStrap)
static u8 fire_enc[BLOCK_STATES], fire_flam[BLOCK_STATES];
static bool fire_init;
static void fire_table(void) {
    static const struct { const char *pat; u8 enc, flam; } T[] = {
        {"_planks", 5, 20}, {"_slab", 5, 20}, {"_fence", 5, 20}, {"_stairs", 5, 20}, {"_log", 5, 5}, {"_wood", 5, 5},
        {"_leaves", 30, 60}, {"bookshelf", 30, 20}, {"tnt", 15, 100}, {"short_grass", 60, 100}, {"tall_grass", 60, 100},
        {"fern", 60, 100}, {"dead_bush", 60, 100}, {"_wool", 30, 60}, {"_carpet", 60, 20}, {"vine", 15, 100},
        {"coal_block", 5, 5}, {"hay_block", 60, 20}, {"target", 15, 20}, {"dried_kelp_block", 30, 60},
        {"scaffolding", 60, 60}, {"lectern", 30, 20}, {"composter", 5, 20}, {"beehive", 5, 20}, {"bee_nest", 30, 20},
        {"azalea", 30, 60}, {"moss", 5, 100}, {"bamboo", 60, 60}, {"sweet_berry_bush", 60, 100}, {"cave_vines", 15, 60},
        {"spore_blossom", 60, 100}, {"hanging_roots", 30, 60}, {"glow_lichen", 15, 100}, {"pink_petals", 60, 100},
        {"_door", 5, 20}, {"_trapdoor", 5, 20}, {"_sign", 5, 20}, {"crafting_table", 5, 20}, {"chest", 5, 20}, {"barrel", 5, 20},
    };
    for (int b = 1; b < BLOCK_STATES; b++) {
        const char *k = BLOCKS[b].key;
        if (!k) continue;
        if (strstr(k, "crimson") || strstr(k, "warped") || strstr(k, "stone") || strstr(k, "brick") || strstr(k, "copper") || strstr(k, "iron") || strstr(k, "nether")) continue;
        if ((strstr(k, "_flower") || BLOCKS[b].mesh == MESH_CROSS) && !BLOCKS[b].liquid) { fire_enc[b] = 60; fire_flam[b] = 100; }
        for (size_t i = 0; i < sizeof T / sizeof T[0]; i++) if (strstr(k, T[i].pat)) { fire_enc[b] = T[i].enc; fire_flam[b] = T[i].flam; break; }
    }
    fire_init = true;
}
int block_encouragement(Block b) { if (!fire_init) fire_table(); return fire_enc[b]; }
int block_flammability(Block b) { if (!fire_init) fire_table(); return fire_flam[b]; }

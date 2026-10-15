// Flowing water and lava, following Minecraft's rules:
//  * state 0 = source, 1..7 = flowing (distance from the source), 8+ = falling.
//  * fluid falls first; only when it cannot fall does it spread sideways, preferring the
//    directions with the shortest path (within 4 blocks) to a drop.
//  * water drops 1 level per block, lava 2 (1 in the Nether); water every 5 game ticks,
//    lava every 30 (10 in the Nether).
//  * two water sources next to each other over solid ground create a new source.
//  * lava touching water becomes obsidian (source) or cobblestone (flowing); lava falling
//    onto water turns it into stone; lava over soul soil next to blue ice makes basalt.
#include "rules.h"
#include "world.h"
#include "../audio/audio.h"
#include "../game/fx.h"

static inline Block B(int x, int y, int z) { return world_get_block(x, y, z); }
static inline bool same_fluid(Block a, int k) { return BLOCKS[a].base == k; }
static int flow_delay(int k) { return k == B_WATER ? 5 * 3 : (g_world.dim == DIM_NETHER ? 10 * 3 : 30 * 3); }
static int level_drop(int k) { return k == B_WATER ? 1 : (g_world.dim == DIM_NETHER ? 1 : 2); }

// can fluid of kind k flow into a cell holding b (breaking it if needed)?
static bool flowable(Block b, int k) {
    if (block_is_air(b)) return true;
    const BlockDef *d = &BLOCKS[b];
    if (d->liquid) return false;
    if (d->flags & BF_WATERLOGGED) return false;
    if (b == B_LADDER || d->place == PL_DOOR || d->place == PL_SIGN || d->be != BE_NONE) return false;
    if (d->base == B_NETHER_PORTAL || d->base == B_END_PORTAL || d->base == B_SIFT_PORTAL) return false;
    if (d->replaceable) return true;
    if (d->solid && d->mesh != MESH_RAIL) return false;
    return d->mesh == MESH_CROSS || d->mesh == MESH_CROP || d->mesh == MESH_FLAT || d->mesh == MESH_TORCH || d->mesh == MESH_WIRE || d->mesh == MESH_RAIL ||
           d->support != SUP_NONE || d->place == PL_ATTACH || d->place == PL_WALLONLY;
    (void)k;
}
// cell is free for fluid or holds a weaker flow of the same fluid
static bool can_take(Block b, int k, int new_var) {
    if (same_fluid(b, k)) {
        int v = BLOCKS[b].var;
        if (v == 0) return false;
        if (new_var >= 8) return v < 8;
        return v < 8 && v > new_var;
    }
    if (BLOCKS[b].liquid) return true;   // the other fluid: reaction handled on update
    return flowable(b, k);
}

static void put(int x, int y, int z, Block s) {
    Block old = B(x, y, z);
    if (old == s) return;
    int k = BLOCKS[s].base;
    if (!block_is_air(old) && !BLOCKS[old].liquid) {
        if (k == B_LAVA) { /* burns away */ }
        else game_hook_block_broken_naturally(iv3(x, y, z), old);
    }
    world_set_block_silent(x, y, z, s);
    if (BLOCKS[s].liquid) fluid_arm(iv3(x, y, z), flow_delay(k));
    for (int f = 0; f < 6; f++) world_schedule_update(iv3(x + FACE_DIR[f].x, y + FACE_DIR[f].y, z + FACE_DIR[f].z), 1);
    rs_block_changed(iv3(x, y, z));
}

// distance (<= limit) from (x,y,z) moving sideways to a cell where the fluid could fall
static int drop_distance(int x, int y, int z, int k, int from_k, int depth, int limit) {
    int best = 1000;
    for (int i = 0; i < 4; i++) {
        if (depth > 0 && i == (from_k ^ 2)) continue;   // don't walk back
        int f = H4_FACE[i];
        int nx = x + FACE_DIR[f].x, nz = z + FACE_DIR[f].z;
        Block n = B(nx, y, nz);
        if (!(flowable(n, k) || (same_fluid(n, k) && BLOCKS[n].var != 0))) continue;
        Block below = B(nx, y - 1, nz);
        if (flowable(below, k) || (same_fluid(below, k))) return depth;
        if (depth < limit) { int d = drop_distance(nx, y, nz, k, i, depth + 1, limit); if (d < best) best = d; }
    }
    return best;
}

static bool react_lava(int x, int y, int z, Block b) {
    for (int f = 0; f < 6; f++) {
        if (f == FACE_D) continue;
        if (block_is_water(B(x + FACE_DIR[f].x, y + FACE_DIR[f].y, z + FACE_DIR[f].z))) {
            put(x, y, z, BLOCKS[b].var == 0 ? B_OBSIDIAN : B_COBBLESTONE);
            sound_play_at(SND_FIRE, v3(x + 0.5f, y + 0.5f, z + 0.5f), 0.5f, 1.8f);
            fx_smoke(v3(x + 0.5f, y + 1.0f, z + 0.5f), 6, 0.2f, COL_WHITE);
            return true;
        }
    }
    if (B(x, y - 1, z) == B_SOUL_SOIL) {
        for (int i = 0; i < 4; i++) {
            int f = H4_FACE[i];
            if (B(x + FACE_DIR[f].x, y, z + FACE_DIR[f].z) == B_BLUE_ICE) { put(x, y, z, B_BASALT); return true; }
        }
    }
    return false;
}

void fluid_update(ivec3 p) {
    int x = p.x, y = p.y, z = p.z;
    Block b = B(x, y, z);
    int k = BLOCKS[b].base;
    if (k != B_WATER && k != B_LAVA) return;
    if (k == B_LAVA && react_lava(x, y, z, b)) return;
    if (!fluid_due_now(p, flow_delay(k))) return;
    int var = BLOCKS[b].var;
    int drop = level_drop(k);

    // ---- 1. a flowing cell re-derives its level from its neighbours (or dries up)
    if (var != 0) {
        int want = -1;
        if (same_fluid(B(x, y + 1, z), k)) want = 8;
        else {
            int best = 100, sources = 0;
            for (int i = 0; i < 4; i++) {
                int f = H4_FACE[i];
                Block n = B(x + FACE_DIR[f].x, y, z + FACE_DIR[f].z);
                if (!same_fluid(n, k)) continue;
                int nv = BLOCKS[n].var;
                if (nv == 0) sources++;
                int lvl = nv >= 8 ? 0 : nv;
                best = MINI(best, lvl + drop);
            }
            Block below = B(x, y - 1, z);
            if (((k == B_WATER && rule(GR_WATER_SOURCE_CONVERSION)) || (k == B_LAVA && rule(GR_LAVA_SOURCE_CONVERSION))) && sources >= 2 && (BLOCKS[below].solid || (same_fluid(below, k) && BLOCKS[below].var == 0))) want = 0;
            else if (best <= 7) want = best;
        }
        if (want < 0) { put(x, y, z, B_AIR); return; }
        if (want != var) { put(x, y, z, (Block)(k + want)); return; }
    }

    // ---- 2. fall
    if (y > 0) {
        Block below = B(x, y - 1, z);
        if (k == B_LAVA && block_is_water(below)) { put(x, y - 1, z, B_STONE); return; }
        if (k == B_WATER && block_is_lava(below)) { put(x, y - 1, z, BLOCKS[below].var == 0 ? B_OBSIDIAN : B_COBBLESTONE); return; }
        if (can_take(below, k, 8)) { put(x, y - 1, z, (Block)(k + 8)); return; }
        bool hole = same_fluid(below, k) || flowable(below, k);
        if (var != 0 && hole) return;
    }

    // ---- 3. spread sideways
    int lvl = var >= 8 ? 0 : var;
    int nl = lvl + drop;
    if (nl > 7) return;
    int limit = k == B_WATER ? 4 : (g_world.dim == DIM_NETHER ? 4 : 2);
    int dist[4], best = 1000;
    for (int i = 0; i < 4; i++) {
        int f = H4_FACE[i];
        int nx = x + FACE_DIR[f].x, nz = z + FACE_DIR[f].z;
        Block n = B(nx, y, nz);
        dist[i] = -1;
        if (!can_take(n, k, nl)) continue;
        Block nbelow = B(nx, y - 1, nz);
        if (flowable(nbelow, k) || same_fluid(nbelow, k)) dist[i] = 0;
        else dist[i] = 1 + MINI(drop_distance(nx, y, nz, k, i, 0, limit - 1), 500);
        best = MINI(best, dist[i]);
    }
    for (int i = 0; i < 4; i++) {
        if (dist[i] < 0 || dist[i] > best) continue;
        int f = H4_FACE[i];
        int nx = x + FACE_DIR[f].x, nz = z + FACE_DIR[f].z;
        Block n = B(nx, y, nz);
        if (k == B_WATER && block_is_lava(n)) { put(nx, y, nz, BLOCKS[n].var == 0 ? B_OBSIDIAN : B_COBBLESTONE); continue; }
        if (k == B_LAVA && block_is_water(n)) { put(nx, y, nz, B_STONE); continue; }
        put(nx, y, nz, (Block)(k + nl));
    }
}

f32 fluid_height(Block b) {
    int v = BLOCKS[b].var;
    if (v == 0 || v >= 8) return 8.0f / 9.0f;
    return (8 - v) / 9.0f;
}

vec3 fluid_flow(int x, int y, int z) {
    Block b = B(x, y, z);
    int k = BLOCKS[b].base;
    if (k != B_WATER && k != B_LAVA) return v3(0, 0, 0);
    f32 h = fluid_height(b);
    vec3 v = v3(0, 0, 0);
    for (int i = 0; i < 4; i++) {
        int f = H4_FACE[i];
        int nx = x + FACE_DIR[f].x, nz = z + FACE_DIR[f].z;
        Block n = B(nx, y, nz);
        f32 d = 0;
        if (same_fluid(n, k)) d = h - fluid_height(n);
        else if (!BLOCKS[n].solid) { Block nb = B(nx, y - 1, nz); if (same_fluid(nb, k)) d = h - (fluid_height(nb) - 1); }
        v.x += FACE_DIR[f].x * d; v.z += FACE_DIR[f].z * d;
    }
    if (BLOCKS[b].var >= 8) v.y -= 1;
    f32 l = v3_len(v);
    return l > 1e-4f ? v3_scale(v, 1.0f / l) : v;
}

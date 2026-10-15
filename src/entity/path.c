// A* pathfinding for walking mobs. Nodes are the cells a mob's feet can stand in: the cell and the one above are
// free (or a door the mob may open), and the block below is solid (or the cell is water). From a node a mob can step
// to the 4 neighbours on the same level, climb one block (if it has head room) or drop up to 3 blocks. Lava, fire,
// cactus, magma, sweet berries and powder snow are avoided. The search is capped (1,500 expansions, 32 blocks) and at
// most 4 searches run per tick; paths are cached per mob and recomputed when the goal moves or every ~2 s.
#include "entity.h"
#include "../world/world.h"
#include "../game/game.h"

#define PATH_MAX_PTS 48
#define PATH_SLOTS 128
#define MAX_EXPAND 1500
typedef struct { u32 id; ivec3 pts[PATH_MAX_PTS]; int n, idx; ivec3 goal; u64 made; } PathSlot;
static PathSlot slots[PATH_SLOTS];
static int searches_this_tick; static u64 search_tick;
int g_path_searches, g_path_found, g_doors_opened;

static bool hazard(Block b) {
    int k = BLOCKS[b].base;
    return k == B_LAVA || k == B_FIRE || k == B_SOUL_FIRE || k == B_CACTUS || k == B_MAGMA_BLOCK || k == B_SWEET_BERRY_BUSH || k == B_POWDER_SNOW || k == B_CAMPFIRE || k == B_WITHER_ROSE;
}
static bool is_door(Block b) { return BLOCKS[b].place == PL_DOOR; }
static bool opens_doors(const Entity *e) { return e->mob == MOB_VILLAGER || e->mob == MOB_WANDERING_TRADER || e->mob == MOB_DELVARI_MERCHANT || e->mob == MOB_PIGLIN || e->mob == MOB_VINDICATOR || e->mob == MOB_EVOKER || e->mob == MOB_PILLAGER; }
static bool free_cell(int x, int y, int z, bool doors) {
    Block b = world_get_block(x, y, z);
    if (hazard(b)) return false;
    if (is_door(b)) return doors || block_open(b);
    return !BLOCKS[b].solid;
}
static bool standable(int x, int y, int z, bool doors) {
    if (!free_cell(x, y, z, doors) || !free_cell(x, y + 1, z, doors)) return false;
    Block below = world_get_block(x, y - 1, z), here = world_get_block(x, y, z);
    if (hazard(below)) return false;
    return BLOCKS[below].solid || block_is_water(here);
}
// ---- open-addressing node table + binary heap
typedef struct { ivec3 p; int g, f, parent; bool closed; } Node;
static Node nodes[MAX_EXPAND * 6];
static int nnodes;
static int htab[8192];
static int heap[MAX_EXPAND * 6], nheap;
static u32 hkey(ivec3 p) { return ((u32)p.x * 73856093u) ^ ((u32)p.y * 19349663u) ^ ((u32)p.z * 83492791u); }
static int find_node(ivec3 p, bool create) {
    for (u32 h = hkey(p) & 8191;; h = (h + 1) & 8191) {
        if (htab[h] < 0) {
            if (!create || nnodes >= (int)ARRAY_LEN(nodes)) return -1;
            htab[h] = nnodes; nodes[nnodes] = (Node){p, 1 << 30, 1 << 30, -1, false};
            return nnodes++;
        }
        if (iv3_eq(nodes[htab[h]].p, p)) return htab[h];
    }
}
static void heap_push(int n) {
    int i = nheap++; heap[i] = n;
    while (i > 0) { int par = (i - 1) / 2; if (nodes[heap[par]].f <= nodes[heap[i]].f) break; int t = heap[par]; heap[par] = heap[i]; heap[i] = t; i = par; }
}
static int heap_pop(void) {
    int top = heap[0]; heap[0] = heap[--nheap];
    for (int i = 0;;) {
        int l = 2 * i + 1, r = l + 1, m = i;
        if (l < nheap && nodes[heap[l]].f < nodes[heap[m]].f) m = l;
        if (r < nheap && nodes[heap[r]].f < nodes[heap[m]].f) m = r;
        if (m == i) break;
        int t = heap[m]; heap[m] = heap[i]; heap[i] = t; i = m;
    }
    return top;
}
static int heur(ivec3 a, ivec3 b) { return 10 * (abs(a.x - b.x) + abs(a.z - b.z)) + 12 * abs(a.y - b.y); }
// returns number of waypoints written (start excluded, goal included), 0 = no path
static int astar(Entity *e, ivec3 s, ivec3 g, ivec3 *out, int max) {
    bool doors = opens_doors(e);
    memset(htab, 0xFF, sizeof htab); nnodes = 0; nheap = 0;
    int sn = find_node(s, true); nodes[sn].g = 0; nodes[sn].f = heur(s, g); heap_push(sn);
    int best = sn, bestd = heur(s, g), expanded = 0;
    while (nheap && expanded < MAX_EXPAND) {
        int cur = heap_pop();
        if (nodes[cur].closed) continue;
        nodes[cur].closed = true; expanded++;
        ivec3 c = nodes[cur].p;
        int d = heur(c, g); if (d < bestd) { bestd = d; best = cur; }
        if (iv3_eq(c, g) || (abs(c.x - g.x) + abs(c.z - g.z) <= 1 && abs(c.y - g.y) <= 1)) { best = cur; break; }
        static const int DX[4] = {1, -1, 0, 0}, DZ[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) {
            int nx = c.x + DX[k], nz = c.z + DZ[k];
            if (abs(nx - s.x) > 32 || abs(nz - s.z) > 32) continue;
            int ny = -1000, cost = 10;
            if (standable(nx, c.y, nz, doors)) ny = c.y;
            else if (standable(nx, c.y + 1, nz, doors) && free_cell(c.x, c.y + 2, c.z, doors)) { ny = c.y + 1; cost = 14; }
            else for (int dy = 1; dy <= 3; dy++) if (free_cell(nx, c.y - dy + 1, nz, doors) && standable(nx, c.y - dy, nz, doors)) { ny = c.y - dy; cost = 10 + 4 * dy; break; }
            if (ny == -1000) continue;
            if (is_door(world_get_block(nx, ny, nz))) cost += 6;
            ivec3 np = iv3(nx, ny, nz);
            int nn = find_node(np, true); if (nn < 0) continue;
            if (nodes[nn].closed || nodes[cur].g + cost >= nodes[nn].g) continue;
            nodes[nn].g = nodes[cur].g + cost; nodes[nn].f = nodes[nn].g + heur(np, g); nodes[nn].parent = cur;
            heap_push(nn);
        }
    }
    if (best == sn) return 0;
    int chain[512], nc = 0;
    for (int i = best; i >= 0 && i != sn && nc < 512; i = nodes[i].parent) chain[nc++] = i;
    int n = 0;
    for (int i = nc - 1; i >= 0 && n < max; i--) out[n++] = nodes[chain[i]].p;
    return n;
}
static PathSlot *slot_for(Entity *e) {
    PathSlot *free_s = NULL, *oldest = &slots[0];
    for (int i = 0; i < PATH_SLOTS; i++) {
        if (slots[i].id == e->id) return &slots[i];
        Entity *o = slots[i].id ? entity_get(slots[i].id) : NULL;
        if (!o && !free_s) free_s = &slots[i];
        if (slots[i].made < oldest->made) oldest = &slots[i];
    }
    PathSlot *s = free_s ? free_s : oldest;
    memset(s, 0, sizeof *s); s->id = e->id; s->goal = iv3(0, -1000, 0);
    return s;
}
// next point to walk towards (true), or false = no path: walk straight
bool path_next(Entity *e, vec3 goal, vec3 *wp) {
    if (search_tick != g_world.time) { search_tick = g_world.time; searches_this_tick = 0; }
    PathSlot *s = slot_for(e);
    ivec3 g = v3_floor(goal), me = v3_floor(v3_add(e->pos, v3(0, 0.1f, 0)));
    bool stale = abs(g.x - s->goal.x) + abs(g.z - s->goal.z) > 2 || abs(g.y - s->goal.y) > 2 || g_world.time - s->made > 120 || s->idx >= s->n;
    if (stale) {
        if (searches_this_tick >= 4) return s->idx < s->n ? (*wp = iv3_center(s->pts[s->idx]), wp->y = (f32)s->pts[s->idx].y, true) : false;
        searches_this_tick++; g_path_searches++;
        s->n = astar(e, me, g, s->pts, PATH_MAX_PTS); s->idx = 0; s->goal = g; s->made = g_world.time;
        if (s->n) g_path_found++;
    }
    while (s->idx < s->n) {
        ivec3 p = s->pts[s->idx];
        f32 dx = p.x + 0.5f - e->pos.x, dz = p.z + 0.5f - e->pos.z;
        if (dx * dx + dz * dz < 0.35f * 0.35f && fabsf(e->pos.y - p.y) < 1.2f) { s->idx++; continue; }
        break;
    }
    if (s->idx >= s->n) return false;
    ivec3 p = s->pts[s->idx];
    Block b = world_get_block(p.x, p.y, p.z);   // open a door that is in the way
    if (is_door(b) && !block_open(b) && opens_doors(e) && v3_dist(e->pos, iv3_center(p)) < 1.8f && BLOCKS[b].sound != SG_METAL) {
        world_set_block(p.x, p.y, p.z, block_set_open(b, true));
        int dy = block_upper_half(b) ? -1 : 1; Block o = world_get_block(p.x, p.y + dy, p.z);
        if (BLOCKS[o].base == BLOCKS[b].base) world_set_block_silent(p.x, p.y + dy, p.z, block_set_open(o, true));
        g_doors_opened++;
    }
    *wp = v3(p.x + 0.5f, (f32)p.y, p.z + 0.5f);
    return true;
}

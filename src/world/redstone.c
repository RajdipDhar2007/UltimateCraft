// Redstone: power sources, dust networks, torches, repeaters, comparators, observers, pistons,
// lamps, doors, TNT, note blocks, dispensers/droppers, hoppers, rails and daylight detectors.
//
// Model (close to Minecraft's):
//  * Components *emit* power toward a neighbour (emit_toward). Some emit *strong* power into the
//    solid block they face (torch -> block above, repeater -> block in front, lever -> its support).
//  * An opaque solid block is "strongly powered" by strong emitters and "weakly powered" by dust
//    pointing into it / lying on it. Mechanisms (lamps, pistons, doors ...) react to any adjacent
//    emitter or any adjacent powered block.
//  * Dust networks are recomputed as a whole: flood fill the connected dust, seed each wire with
//    the strongest non-dust input, then spread with -1 per wire.
//  * Delayed changes (torches, repeaters, comparators, buttons, observer pulses) are kept in a
//    pending table and applied by a scheduled block update when due.
#include "world.h"
#include "../entity/entity.h"
#include "../game/game.h"
#include "../audio/audio.h"
#include "../game/fx.h"

#define RT 6   // one redstone tick (2 game ticks) in engine ticks

// ------------------------------------------------------------------ small position hash tables
typedef struct { u64 key; u64 due; s32 val; u8 used; } HSlot;
typedef struct { HSlot *s; int cap, n, tombs; } HMap;
static inline u64 pkey(ivec3 p) { return ((u64)(u32)(p.x + 0x800000) << 40) | ((u64)((u32)(p.z + 0x800000) & 0xFFFFFF) << 16) | (u64)(u16)p.y; }
// open addressing with tombstones; rehashes when live + deleted slots pass half the table
// (previously deleted slots were never reclaimed, so a busy table could fill up and inserts failed)
static HSlot *hm_find(HMap *m, u64 k, bool create) {
    if (create && (m->n + m->tombs + 1) * 2 > m->cap) {
        int ncap = m->cap ? ((m->n + 1) * 4 > m->cap ? m->cap * 2 : m->cap) : 256;
        HMap nm = {xmalloc(sizeof(HSlot) * ncap), ncap, 0, 0};
        memset(nm.s, 0, sizeof(HSlot) * nm.cap);
        for (int i = 0; i < m->cap; i++) if (m->s[i].used == 1) { HSlot *d = hm_find(&nm, m->s[i].key, true); *d = m->s[i]; }
        free(m->s); *m = nm;
    }
    if (!m->cap) return NULL;
    u64 h = hash64(k);
    HSlot *tomb = NULL;
    for (int i = 0; i < m->cap; i++) {
        HSlot *s = &m->s[(h + i) & (m->cap - 1)];
        if (s->used == 0) {
            if (!create) return NULL;
            if (tomb) { s = tomb; m->tombs--; }
            s->used = 1; s->key = k; s->due = 0; s->val = 0; m->n++;
            return s;
        }
        if (s->used == 2) { if (!tomb) tomb = s; continue; }
        if (s->key == k) return s;
    }
    if (create && tomb) { tomb->used = 1; tomb->key = k; tomb->due = 0; tomb->val = 0; m->n++; m->tombs--; return tomb; }
    return NULL;
}
static void hm_del(HMap *m, u64 k) { HSlot *s = hm_find(m, k, false); if (s) { s->used = 2; m->n--; m->tombs++; } }
static void hm_clear(HMap *m) { free(m->s); memset(m, 0, sizeof *m); }

static HMap fluid_due; // fluid cells waiting for their flow tick
static HMap pending;   // delayed state changes: val = target state, due = tick
static HMap memory;    // remembered "was powered" for edge-triggered mechanisms
static HMap det_set;   // daylight detectors: dedupe set ...
typedef struct { ivec3 p; int level; } Detector;
static Detector *dets; static int ndets, cap_dets;   // ... and list (rescanned per loaded column)

void redstone_reset(void) { hm_clear(&fluid_due); hm_clear(&pending); hm_clear(&memory); hm_clear(&det_set); free(dets); dets = NULL; ndets = cap_dets = 0; }
static void add_detector(ivec3 p) {
    if (hm_find(&det_set, pkey(p), false)) return;
    hm_find(&det_set, pkey(p), true);
    if (ndets == cap_dets) { cap_dets = cap_dets ? cap_dets * 2 : 16; dets = xrealloc(dets, sizeof(Detector) * cap_dets); }
    dets[ndets++] = (Detector){p, -1};
}

// fluids act only on their own (delayed) ticks; any other update just arms the tick
bool fluid_due_now(ivec3 p, int delay) {
    HSlot *h = hm_find(&fluid_due, pkey(p), false);
    if (h && h->due <= g_world.time) { hm_del(&fluid_due, pkey(p)); return true; }
    if (!h) { h = hm_find(&fluid_due, pkey(p), true); h->due = g_world.time + (u64)delay; world_schedule_update(p, delay); }
    else world_schedule_update(p, (int)(h->due - g_world.time));   // pending updates are unique per position: re-arm the flow tick
    return false;
}
void fluid_arm(ivec3 p, int delay) {
    HSlot *h = hm_find(&fluid_due, pkey(p), true);
    if (!h->due || h->due > g_world.time + (u64)delay) h->due = g_world.time + (u64)delay;
    world_schedule_update(p, delay);
}

// ------------------------------------------------------------------ helpers
static inline Block B(ivec3 p) { return world_get_block(p.x, p.y, p.z); }
static inline ivec3 nb(ivec3 p, int f) { return iv3(p.x + FACE_DIR[f].x, p.y + FACE_DIR[f].y, p.z + FACE_DIR[f].z); }
static inline int kind(Block b) { return BLOCKS[b].base; }
static inline bool conductor(Block b) { return BLOCKS[b].opaque && BLOCKS[b].solid && kind(b) != B_REDSTONE_BLOCK && BLOCKS[b].place != PL_PISTON; }
static int face_of_h4(int k) { return H4_FACE[k & 3]; }
static int h4_of_face(int f) { for (int k = 0; k < 4; k++) if (H4_FACE[k] == f) return k; return -1; }

static bool is_torch(Block b) { return kind(b) == B_REDSTONE_TORCH || kind(b) == B_REDSTONE_WALL_TORCH; }
static bool torch_lit(Block b) { return kind(b) == B_REDSTONE_TORCH ? BLOCKS[b].var == 0 : BLOCKS[b].var < 4; }
static int torch_support(Block b) {   // face toward the block the torch hangs on
    if (kind(b) == B_REDSTONE_TORCH) return FACE_D;
    return H4_FACE[BLOCKS[b].var % 4];
}
static bool is_button(Block b) { const char *k = BLOCKS[b].key; return BLOCKS[b].place == PL_ATTACH && k && strstr(k, "_button"); }
static bool is_lever(Block b) { return kind(b) == B_LEVER; }
static bool is_plate(Block b) { const char *k = BLOCKS[b].key; return k && strstr(k, "pressure_plate"); }
static bool is_repeater(Block b) { return kind(b) == B_REPEATER; }
static bool is_comparator(Block b) { return kind(b) == B_COMPARATOR; }
static int diode_front(Block b) { return face_of_h4(BLOCKS[b].var / (BLOCKS[b].nvar / 4)); }   // output side
static bool diode_powered(Block b) { return BLOCKS[b].var & 1; }
static bool is_observer(Block b) { return kind(b) == B_OBSERVER; }
static bool is_piston(Block b) { return kind(b) == B_PISTON || kind(b) == B_STICKY_PISTON; }
static bool is_rail(Block b) { return BLOCKS[b].place == PL_RAIL; }

bool rs_is_component(Block b) {
    int k = kind(b);
    return k == B_REDSTONE_WIRE || is_torch(b) || is_lever(b) || is_button(b) || is_plate(b) || is_repeater(b) || is_comparator(b) || is_observer(b) ||
           k == B_REDSTONE_BLOCK || k == B_DAYLIGHT_DETECTOR || k == B_TARGET || k == B_TRAPPED_CHEST || k == B_LECTERN ||
           (k == B_DETECTOR_RAIL) || k == B_SCULK_SENSOR || k == B_CALIBRATED_SCULK_SENSOR || k == B_TRIPWIRE_HOOK;
}

// ------------------------------------------------------------------ dust connections
// mask bits 0..3: connects N/E/S/W (same level or one step), bits 4..7: climbs the side of the block in that direction
static int wire_mask_g(u16 (*get)(int, int, int), int x, int y, int z) {
    int m = 0;
    Block above = get(x, y + 1, z);
    bool up_open = !conductor(above);
    for (int k = 0; k < 4; k++) {
        int f = face_of_h4(k);
        int nx = x + FACE_DIR[f].x, nz = z + FACE_DIR[f].z;
        Block n = get(nx, y, nz);
        if (kind(n) == B_REDSTONE_WIRE) { m |= 1 << k; continue; }
        if (is_repeater(n) || is_comparator(n)) { int df = diode_front(n); if (df == f || df == FACE_OPP[f]) m |= 1 << k; continue; }
        if (is_observer(n)) { if (BLOCKS[n].var % 6 == f) m |= 1 << k; continue; }   // observer's back faces the wire
        if (rs_is_component(n) && kind(n) != B_REDSTONE_WIRE) { m |= 1 << k; continue; }
        if (up_open && conductor(n) && kind(get(nx, y + 1, nz)) == B_REDSTONE_WIRE) { m |= (1 << k) | (16 << k); continue; }
        if (!conductor(n) && kind(get(nx, y - 1, nz)) == B_REDSTONE_WIRE) m |= 1 << k;
    }
    return m;
}
static u16 wget(int x, int y, int z) { return world_get_block(x, y, z); }
int rs_wire_mask(int x, int y, int z) { return wire_mask_g(wget, x, y, z); }
// variant for the mesher, which reads a padded copy of the blocks
static u16 (*mesh_get)(int, int, int);
int rs_wire_mask_with(u16 (*get)(int, int, int), int x, int y, int z) { mesh_get = get; return wire_mask_g(get, x, y, z); }

// does the wire at w point into the neighbour at direction f (horizontal face) or down?
static bool wire_points(ivec3 w, int f) {
    if (f == FACE_D) return true;
    if (f == FACE_U) return false;
    int m = rs_wire_mask(w.x, w.y, w.z) & 15;
    int k = h4_of_face(f);
    if (m == 0) return true;                       // a lone dot powers all around
    if (m & (1 << k)) return true;
    int opp = h4_of_face(FACE_OPP[f]);
    return m == (1 << opp);                         // a single line extends both ways
}

// ------------------------------------------------------------------ power queries
static int detector_level(Block b) {
    f32 t = (f32)(world_daytime() % DAY_TICKS) / DAY_TICKS;
    f32 sun = sinf(t * 2 * PI_F);
    int lvl = g_world.dim == DIM_OVERWORLD ? clampi((int)roundf(sun * 15), 0, 15) : 0;
    return BLOCKS[b].var ? 15 - lvl : lvl;
}
static int plate_level(ivec3 p, Block b) {
    if (!(BLOCKS[b].var & 1)) return 0;
    int k = kind(b);
    if (k != B_LIGHT_WEIGHTED_PRESSURE_PLATE && k != B_HEAVY_WEIGHTED_PRESSURE_PLATE) return 15;
    int n = 0;
    AABB a = {v3((f32)p.x, (f32)p.y, (f32)p.z), v3(p.x + 1.0f, p.y + 0.3f, p.z + 1.0f)};
    ENT_FOREACH(e) if ((e->kind == ENT_MOB || e->kind == ENT_PLAYER || e->kind == ENT_ITEM) && aabb_overlap(entity_aabb(e), a)) n += e->kind == ENT_ITEM ? e->item.count : 1;
    return k == B_LIGHT_WEIGHTED_PRESSURE_PLATE ? MINI(15, n) : MINI(15, (n + 9) / 10);
}
static int comparator_out(ivec3 p) { BlockEntity *be = world_block_entity(p, false, 0); return be ? be->data : 0; }

// power component b at position at emits toward the neighbour in direction f; *strong set if it also strongly powers a block there
static int emit_toward(ivec3 at, Block b, int f, bool *strong) {
    int k = kind(b);
    *strong = false;
    if (k == B_REDSTONE_BLOCK) return 15;
    if (is_torch(b)) {
        if (!torch_lit(b) || f == torch_support(b)) return 0;
        if (f == FACE_U) *strong = true;
        return 15;
    }
    if (is_lever(b) || is_button(b)) {
        if (BLOCKS[b].var < 6) return 0;
        if (f == block_attach_face(b)) *strong = true;
        return 15;
    }
    if (is_plate(b)) { int l = plate_level(at, b); if (f == FACE_D) *strong = l > 0; return l; }
    if (is_repeater(b)) { if (!diode_powered(b) || f != diode_front(b)) return 0; *strong = true; return 15; }
    if (is_comparator(b)) { if (f != diode_front(b)) return 0; *strong = true; return comparator_out(at); }
    if (is_observer(b)) { if (BLOCKS[b].var < 6 || f != FACE_OPP[BLOCKS[b].var % 6]) return 0; *strong = true; return 15; }
    if (k == B_DAYLIGHT_DETECTOR) return detector_level(b);
    if (k == B_TARGET) return BLOCKS[b].var ? 15 : 0;
    if (k == B_TRIPWIRE_HOOK) { if (BLOCKS[b].var < 4) return 0; if (f == H4_FACE[BLOCKS[b].var % 4]) *strong = true; return 15; }
    if (k == B_SCULK_SENSOR || k == B_CALIBRATED_SCULK_SENSOR) { if (f == FACE_D) *strong = BLOCKS[b].var > 0; return BLOCKS[b].var; }
    if (k == B_DETECTOR_RAIL) { if (BLOCKS[b].var < 6) return 0; if (f == FACE_D) *strong = true; return 15; }
    if (k == B_TRAPPED_CHEST) { BlockEntity *be = world_block_entity(at, false, 0); return be && be->cook_time > 0 ? 15 : 0; }
    if (k == B_REDSTONE_WIRE) return 0;   // handled separately (weak)
    return 0;
}

// strong power of a conductor block q (from strong emitters around it)
static int strong_power(ivec3 q) {
    int best = 0;
    for (int f = 0; f < 6; f++) {
        ivec3 n = nb(q, f);
        Block b = B(n);
        if (!rs_is_component(b)) continue;
        bool st; int l = emit_toward(n, b, FACE_OPP[f], &st);
        if (st) best = MAXI(best, l);
    }
    return best;
}
// weak power: strong power plus dust pointing into / lying on the block
static int block_power(ivec3 q) {
    int best = strong_power(q);
    for (int f = 0; f < 6; f++) {
        if (f == FACE_D) continue;
        ivec3 n = nb(q, f);
        Block b = B(n);
        if (kind(b) == B_REDSTONE_WIRE && wire_points(n, FACE_OPP[f])) best = MAXI(best, BLOCKS[b].var);
    }
    return best;
}
// power arriving at a mechanism / input at p from direction f (the neighbour at p+f)
static int power_from(ivec3 p, int f) {
    ivec3 n = nb(p, f);
    Block b = B(n);
    if (kind(b) == B_REDSTONE_WIRE) return wire_points(n, FACE_OPP[f]) ? BLOCKS[b].var : 0;
    if (rs_is_component(b)) { bool st; return emit_toward(n, b, FACE_OPP[f], &st); }
    if (conductor(b)) return block_power(n);
    return 0;
}
int rs_input_power(ivec3 p) {
    int best = 0;
    for (int f = 0; f < 6; f++) best = MAXI(best, power_from(p, f));
    return best;
}
static int input_except(ivec3 p, int skip_face) {
    int best = 0;
    for (int f = 0; f < 6; f++) if (f != skip_face) best = MAXI(best, power_from(p, f));
    return best;
}
bool rs_powered(ivec3 p) { return rs_input_power(p) > 0; }

// ------------------------------------------------------------------ notifications
static void notify(ivec3 p) {   // neighbours and the neighbours of strongly powered blocks
    for (int f = 0; f < 6; f++) {
        ivec3 n = nb(p, f);
        world_schedule_update(n, 1);
        if (conductor(B(n))) for (int g = 0; g < 6; g++) world_schedule_update(nb(n, g), 1);
    }
}
static void set_state(ivec3 p, Block s) {
    if (B(p) == s) return;
    world_set_block_silent(p.x, p.y, p.z, s);
    notify(p);
}
static void schedule_change(ivec3 p, int target, int delay) {
    HSlot *h = hm_find(&pending, pkey(p), true);
    if (h->val == target && h->due > g_world.time) return;
    h->val = target; h->due = g_world.time + (u64)delay;
    world_schedule_update(p, delay);
}
static bool take_pending(ivec3 p, int *target) {   // true if a pending change is due now
    HSlot *h = hm_find(&pending, pkey(p), false);
    if (!h) return false;
    if (h->due > g_world.time) return false;
    *target = h->val;
    hm_del(&pending, pkey(p));
    return true;
}
static bool has_pending(ivec3 p) { HSlot *h = hm_find(&pending, pkey(p), false); return h && h->due > g_world.time; }
static bool edge(ivec3 p, bool powered) {   // returns true on a rising edge, remembers the level
    HSlot *h = hm_find(&memory, pkey(p), true);
    bool was = h->val != 0;
    h->val = powered;
    if (!powered) hm_del(&memory, pkey(p));
    return powered && !was;
}

// ------------------------------------------------------------------ dust networks
#define NET_MAX 4096
static ivec3 net[NET_MAX];
static s8 net_pow[NET_MAX];
static HMap net_idx;
static void recompute_dust(ivec3 start) {
    int n = 0;
    hm_clear(&net_idx);
    net[n++] = start; hm_find(&net_idx, pkey(start), true)->val = 0;
    for (int i = 0; i < n && n < NET_MAX - 8; i++) {
        ivec3 w = net[i];
        int m = rs_wire_mask(w.x, w.y, w.z);
        for (int k = 0; k < 4; k++) {
            if (!(m & (1 << k))) continue;
            int f = face_of_h4(k);
            ivec3 cands[3] = {nb(w, f), iv3(w.x + FACE_DIR[f].x, w.y + 1, w.z + FACE_DIR[f].z), iv3(w.x + FACE_DIR[f].x, w.y - 1, w.z + FACE_DIR[f].z)};
            for (int c = 0; c < 3; c++) {
                if (kind(B(cands[c])) != B_REDSTONE_WIRE) continue;
                if (hm_find(&net_idx, pkey(cands[c]), false)) continue;
                hm_find(&net_idx, pkey(cands[c]), true)->val = n;
                net[n++] = cands[c];
                break;
            }
        }
    }
    // seed with non-dust inputs
    for (int i = 0; i < n; i++) {
        ivec3 w = net[i];
        int best = 0;
        for (int f = 0; f < 6; f++) {
            ivec3 q = nb(w, f);
            Block b = B(q);
            if (kind(b) == B_REDSTONE_WIRE) continue;
            if (rs_is_component(b)) { bool st; best = MAXI(best, emit_toward(q, b, FACE_OPP[f], &st)); }
            else if (conductor(b)) best = MAXI(best, strong_power(q));
        }
        net_pow[i] = (s8)best;
    }
    // spread (Bellman-Ford style; networks are small)
    for (bool changed = true; changed;) {
        changed = false;
        for (int i = 0; i < n; i++) {
            ivec3 w = net[i];
            int m = rs_wire_mask(w.x, w.y, w.z);
            for (int k = 0; k < 4; k++) if (m & (1 << k)) {
                int f = face_of_h4(k);
                ivec3 cands[3] = {nb(w, f), iv3(w.x + FACE_DIR[f].x, w.y + 1, w.z + FACE_DIR[f].z), iv3(w.x + FACE_DIR[f].x, w.y - 1, w.z + FACE_DIR[f].z)};
                for (int c = 0; c < 3; c++) {
                    HSlot *h = hm_find(&net_idx, pkey(cands[c]), false);
                    if (!h) continue;
                    if (net_pow[h->val] - 1 > net_pow[i]) { net_pow[i] = (s8)(net_pow[h->val] - 1); changed = true; }
                    break;
                }
            }
        }
    }
    for (int i = 0; i < n; i++) {
        Block cur = B(net[i]);
        if (BLOCKS[cur].var != net_pow[i]) set_state(net[i], (Block)(B_REDSTONE_WIRE + net_pow[i]));
    }
}

// ------------------------------------------------------------------ pistons
static bool pushable(Block b, bool pulling) {
    const BlockDef *d = &BLOCKS[b];
    if (block_is_air(b)) return true;
    if (d->hardness < 0 || b == B_OBSIDIAN || b == B_CRYING_OBSIDIAN || b == B_RESPAWN_ANCHOR || d->be != BE_NONE) return false;
    if (kind(b) == B_PISTON_HEAD) return false;
    if (is_piston(b) && BLOCKS[b].var >= 6) return false;
    if (pulling && (d->replaceable || d->liquid || !d->solid)) return false;
    return true;
}
static bool breaks_when_pushed(Block b) {
    const BlockDef *d = &BLOCKS[b];
    return d->liquid || d->replaceable || (!d->solid && d->mesh != MESH_CUBE) || d->support != SUP_NONE || d->place == PL_ATTACH || d->place == PL_WALLONLY;
}
// Minecraft-style push structures: the blocks in front are collected breadth-first; slime and honey blocks drag
// every movable neighbour along (but slime and honey do not stick to each other), at most 12 blocks move,
// anything immovable in the way cancels the move, and fragile blocks in the path are destroyed.
#define PUSH_LIMIT 12
int g_piston_moves, g_piston_blocked;
static bool is_sticky_block(Block b) { return kind(b) == B_SLIME_BLOCK || kind(b) == B_HONEY_BLOCK; }
static bool sticks(Block a, Block b) {
    if (kind(a) == B_SLIME_BLOCK && kind(b) == B_HONEY_BLOCK) return false;
    if (kind(a) == B_HONEY_BLOCK && kind(b) == B_SLIME_BLOCK) return false;
    return true;
}
static int in_list(const ivec3 *l, int n, ivec3 q) { for (int i = 0; i < n; i++) if (iv3_eq(l[i], q)) return i; return -1; }
// collect the structure starting at `start` moving in direction `dir`; `piston` is never part of it
static int collect_structure(ivec3 start, int dir, ivec3 piston, ivec3 *list) {
    int n = 0;
    Block s0 = B(start);
    if (block_is_air(s0)) return 0;
    if (breaks_when_pushed(s0) && pushable(s0, false)) return 0;
    if (!pushable(s0, false)) return -1;
    list[n++] = start;
    for (int i = 0; i < n; i++) {
        ivec3 c = list[i];
        Block cb = B(c);
        // whatever is in front has to move too (or be fragile)
        ivec3 fr = nb(c, dir);
        if (in_list(list, n, fr) < 0 && !block_is_air(B(fr))) {
            Block fb = B(fr);
            if (iv3_eq(fr, piston)) return -1;
            if (!(breaks_when_pushed(fb) && pushable(fb, false))) {
                if (!pushable(fb, false) || fr.y <= 0 || fr.y >= WORLD_H - 1) return -1;
                if (n >= PUSH_LIMIT) return -1;
                list[n++] = fr;
            }
        }
        if (!is_sticky_block(cb)) continue;
        for (int g = 0; g < 6; g++) {   // slime / honey drag their neighbours
            if (g == dir) continue;
            ivec3 q = nb(c, g);
            if (iv3_eq(q, piston) || in_list(list, n, q) >= 0) continue;
            Block qb = B(q);
            if (block_is_air(qb) || breaks_when_pushed(qb) || !pushable(qb, true) || !sticks(cb, qb)) continue;
            if (n >= PUSH_LIMIT) return -1;
            list[n++] = q;
        }
    }
    return n;
}
static void move_structure(ivec3 *list, int n, int dir) {
    Block types[PUSH_LIMIT];
    for (int i = 0; i < n; i++) types[i] = B(list[i]);
    for (int i = 0; i < n; i++) {   // fragile blocks the structure moves into are destroyed
        ivec3 to = nb(list[i], dir);
        if (in_list(list, n, to) >= 0) continue;
        Block t = B(to);
        if (!block_is_air(t)) { world_set_block(to.x, to.y, to.z, B_AIR); if (!BLOCKS[t].liquid) game_hook_block_broken_naturally(to, t); }
    }
    for (int i = 0; i < n; i++) world_set_block_silent(list[i].x, list[i].y, list[i].z, B_AIR);
    for (int i = 0; i < n; i++) { ivec3 to = nb(list[i], dir); world_set_block(to.x, to.y, to.z, types[i]); }
    for (int i = 0; i < n; i++) notify(list[i]);
    ENT_FOREACH(e) {   // entities in (or riding on) the moved blocks go along
        if (e->kind != ENT_MOB && e->kind != ENT_PLAYER && e->kind != ENT_ITEM) continue;
        AABB a = entity_aabb(e);
        for (int i = 0; i < n; i++) {
            ivec3 to = nb(list[i], dir);
            AABB cell = {v3((f32)to.x, (f32)to.y, (f32)to.z), v3(to.x + 1.0f, to.y + 1.0f + (kind(types[i]) == B_SLIME_BLOCK ? 0.1f : 0), to.z + 1.0f)};
            if (!aabb_overlap(a, cell)) continue;
            e->pos = v3_add(e->pos, v3((f32)FACE_DIR[dir].x, (f32)FACE_DIR[dir].y * 1.01f, (f32)FACE_DIR[dir].z));
            if (kind(types[i]) == B_SLIME_BLOCK && dir == FACE_U) e->vel.y = fmaxf(e->vel.y, 12);   // slime launchers
            break;
        }
    }
    g_piston_moves++;
}
static bool piston_extend(ivec3 p, Block b) {
    int f = BLOCKS[b].var % 6;
    ivec3 list[PUSH_LIMIT + 1];
    ivec3 h = nb(p, f);
    int n = collect_structure(h, f, p, list);
    if (n < 0) { g_piston_blocked++; return false; }
    Block hb = B(h);
    if (n == 0 && !block_is_air(hb)) { world_set_block(h.x, h.y, h.z, B_AIR); if (!BLOCKS[hb].liquid) game_hook_block_broken_naturally(h, hb); }
    if (n > 0) move_structure(list, n, f);
    world_set_block_silent(p.x, p.y, p.z, (Block)(BLOCKS[b].base + 6 + f));
    world_set_block(h.x, h.y, h.z, (Block)(B_PISTON_HEAD + (kind(b) == B_STICKY_PISTON ? 6 : 0) + f));
    sound_play_at(SND_PISTON_OUT, iv3_center(p), 0.8f, 1.0f);
    return true;
}
static void piston_retract(ivec3 p, Block b) {
    int f = BLOCKS[b].var % 6;
    ivec3 h = nb(p, f);
    if (kind(B(h)) == B_PISTON_HEAD) world_set_block(h.x, h.y, h.z, B_AIR);
    world_set_block_silent(p.x, p.y, p.z, (Block)(BLOCKS[b].base + f));
    if (kind(b) == B_STICKY_PISTON) {   // pull the block in front (and everything stuck to it) back by one
        ivec3 q = nb(h, f);
        Block c = B(q);
        if (!block_is_air(c) && pushable(c, true) && !breaks_when_pushed(c)) {
            ivec3 list[PUSH_LIMIT + 1];
            int n = collect_structure(q, FACE_OPP[f], p, list);
            if (n > 0) move_structure(list, n, FACE_OPP[f]);
        }
    }
    notify(p);
    sound_play_at(SND_PISTON_IN, iv3_center(p), 0.7f, 1.0f);
}

// ------------------------------------------------------------------ containers (dispensers, droppers, hoppers, comparators)
static int be_slot_count(int type) {
    switch (type) {
    case BE_CHEST: case BE_BARREL: case BE_SHULKER: return 27;
    case BE_FURNACE: case BE_BLAST: case BE_SMOKER: return 3;
    case BE_HOPPER: return 5;
    case BE_DISPENSER: return 9;
    case BE_BREWING: return 5;
    default: return 0;
    }
}
int rs_container_signal(ivec3 p) {
    if (!world_block_entity(p, false, 0)) { int fs = decor_comparator(p); if (fs >= 0) return fs; }   // item frames
    Block b = B(p);
    if (kind(b) == B_CAKE) { BlockEntity *be = world_block_entity(p, false, 0); return (7 - (be ? be->data : 0)) * 2; }
    if (kind(b) == B_COMPOSTER) { BlockEntity *be = world_block_entity(p, false, 0); return be ? MINI(8, be->data) : 0; }
    if (BLOCKS[b].ui == UI_JUKEBOX) { BlockEntity *be = world_block_entity(p, false, 0); return be && be->data ? 15 : 0; }
    BlockEntity *be = world_block_entity(p, false, 0);
    if (!be) return -1;
    int n = be_slot_count(be->type);
    if (!n) return -1;
    f32 fill = 0; bool any = false;
    for (int i = 0; i < n; i++) if (!stack_empty(be->slots[i])) { fill += (f32)be->slots[i].count / stack_max(be->slots[i]); any = true; }
    return any ? (int)floorf(1 + fill / n * 14) : 0;
}
static bool insert_into(BlockEntity *be, ItemStack *s, int face_from) {
    int n = be_slot_count(be->type);
    int lo = 0, hi = n;
    if (be->type == BE_FURNACE || be->type == BE_BLAST || be->type == BE_SMOKER) {   // top -> input, sides -> fuel
        if (face_from == FACE_U) { lo = 0; hi = 1; } else { lo = 1; hi = 2; if (!fuel_value(s->id)) return false; }
    }
    if (be->type == BE_BREWING) { lo = 0; hi = 5; }
    for (int pass = 0; pass < 2; pass++) for (int i = lo; i < hi; i++) {
        ItemStack *d = &be->slots[i];
        if (pass == 0 && !stack_empty(*d) && stack_can_merge(*d, *s) && d->count < stack_max(*d)) { d->count++; if (--s->count == 0) *s = (ItemStack){0}; return true; }
        if (pass == 1 && stack_empty(*d)) { *d = *s; d->count = 1; if (--s->count == 0) *s = (ItemStack){0}; return true; }
    }
    return false;
}
static int take_slot(BlockEntity *be, int face_to) {   // slot index to pull from (-1 none)
    int n = be_slot_count(be->type);
    if (be->type == BE_FURNACE || be->type == BE_BLAST || be->type == BE_SMOKER) return stack_empty(be->slots[2]) ? -1 : 2;
    (void)face_to;
    for (int i = 0; i < n; i++) if (!stack_empty(be->slots[i])) return i;
    return -1;
}
static BlockEntity *container_at(ivec3 p) {
    Block b = B(p);
    if (BLOCKS[b].be == BE_NONE || BLOCKS[b].be == BE_ENDER || BLOCKS[b].be == BE_JUKEBOX || BLOCKS[b].be == BE_SPAWNER || BLOCKS[b].be == BE_COMPARATOR || BLOCKS[b].be == BE_BANNER || BLOCKS[b].be == BE_LECTERN) return NULL;
    return world_block_entity(p, true, BLOCKS[b].be);
}

static void dispense(ivec3 p, Block b) {
    BlockEntity *be = world_block_entity(p, true, BE_DISPENSER);
    int filled[9], nf = 0;
    for (int i = 0; i < 9; i++) if (!stack_empty(be->slots[i])) filled[nf++] = i;
    if (!nf) { sound_play_at(SND_CLICK, iv3_center(p), 0.5f, 1.4f); return; }
    int slot = filled[irand(0, nf - 1)];
    ItemStack *s = &be->slots[slot];
    int f = BLOCKS[b].var % 6;
    ivec3 q = nb(p, f);
    vec3 out = v3(p.x + 0.5f + FACE_DIR[f].x * 0.7f, p.y + 0.5f + FACE_DIR[f].y * 0.7f - 0.15f, p.z + 0.5f + FACE_DIR[f].z * 0.7f);
    vec3 dir = v3((f32)FACE_DIR[f].x, (f32)FACE_DIR[f].y + 0.1f, (f32)FACE_DIR[f].z);
    bool dropper = kind(b) == B_DROPPER;
    if (dropper) {
        BlockEntity *t = container_at(q);
        ItemStack one = *s; one.count = 1;
        if (t) { if (insert_into(t, &one, FACE_OPP[f])) { if (--s->count == 0) *s = (ItemStack){0}; } }
        else { item_drop(out, one, v3_scale(dir, 4), 10); if (--s->count == 0) *s = (ItemStack){0}; }
        sound_play_at(SND_CLICK, iv3_center(p), 0.5f, 1.0f);
        return;
    }
    const ItemDef *it = &ITEMS[s->id];
    bool used = true;
    if (s->id == I_ARROW || s->id == I_SPECTRAL_ARROW || s->id == I_TIPPED_ARROW) {
        Entity *a = arrow_spawn(out, v3_scale(dir, 30), 0, 2, false);
        if (a) { a->item = *s; a->item.count = 1; }
    } else if (it->use == USE_THROW) {
        Entity *pr = proj_spawn(it->use_arg, out, v3_scale(dir, 22), 0);
        if (pr && it->use_arg == PROJ_POTION) pr->item = *s;
    } else if (s->id == I_FIRE_CHARGE) proj_spawn(PROJ_SMALL_FIREBALL, out, v3_scale(dir, 20), 0);
    else if (item_is_block(s->id) && kind(ITEMS[s->id].block) == B_TNT) tnt_spawn(v3(q.x + 0.5f, (f32)q.y, q.z + 0.5f), 4 * TICKRATE);
    else if (it->use == USE_FLUID_BUCKET && block_is_air(B(q))) { world_set_block(q.x, q.y, q.z, it->block); *s = stack_of(I_BUCKET, 1); return; }
    else if (s->id == I_BUCKET && block_is_source(B(q))) { int full = block_is_water(B(q)) ? I_WATER_BUCKET : I_LAVA_BUCKET; world_set_block(q.x, q.y, q.z, B_AIR); *s = stack_of(full, 1); return; }
    else if (it->use == USE_FLINT_STEEL && block_is_air(B(q))) { world_set_block(q.x, q.y, q.z, B_FIRE); s->damage++; if (s->damage >= it->durability) *s = (ItemStack){0}; return; }
    else if (it->use == USE_BONE_MEAL) { static Rng r = {991}; used = world_grow(q, &r, true) || world_grow(nb(q, FACE_D), &r, true); }
    else if (it->use == USE_SPAWN_EGG) { Entity *m = mob_spawn(it->use_arg, v3(q.x + 0.5f, (f32)q.y, q.z + 0.5f)); if (m) m->persistent = true; }
    else { ItemStack one = *s; one.count = 1; item_drop(out, one, v3_scale(dir, 4), 10); }
    if (used) { if (--s->count == 0) *s = (ItemStack){0}; sound_play_at(SND_CLICK, iv3_center(p), 0.5f, 1.2f); fx_smoke(out, 3, 0.05f, COL_WHITE); }
}

static void hopper_tick(BlockEntity *be) {
    ivec3 p = be->pos;
    Block b = B(p);
    if (kind(b) != B_HOPPER) return;
    if (rs_powered(p)) return;   // locked
    if (be->burn_time > 0) { be->burn_time--; return; }
    bool moved = false;
    // push
    int dirf = BLOCKS[b].var == 0 ? FACE_D : face_of_h4(BLOCKS[b].var - 1);
    BlockEntity *t = container_at(nb(p, dirf));
    if (t) for (int i = 0; i < 5 && !moved; i++) if (!stack_empty(be->slots[i])) {
        ItemStack one = be->slots[i]; one.count = 1;
        if (insert_into(t, &one, FACE_OPP[dirf])) { if (--be->slots[i].count == 0) be->slots[i] = (ItemStack){0}; moved = true; }
    }
    // pull from above
    ivec3 up = nb(p, FACE_U);
    BlockEntity *src = container_at(up);
    if (src) {
        int si = take_slot(src, FACE_D);
        if (si >= 0) { ItemStack one = src->slots[si]; one.count = 1; if (insert_into(be, &one, FACE_U)) { if (--src->slots[si].count == 0) src->slots[si] = (ItemStack){0}; moved = true; } }
    } else if (!BLOCKS[B(up)].opaque) {
        AABB a = {v3((f32)p.x, p.y + 0.6f, (f32)p.z), v3(p.x + 1.0f, p.y + 1.6f, p.z + 1.0f)};
        ENT_FOREACH(e) if (e->kind == ENT_ITEM && e->pickup_delay <= 0 && aabb_overlap(entity_aabb(e), a)) {
            while (!stack_empty(e->item) && insert_into(be, &e->item, FACE_U)) moved = true;
            if (stack_empty(e->item)) e->remove = true;
            break;
        }
    }
    if (moved) be->burn_time = 8 * 3;
}

// ------------------------------------------------------------------ rails
static bool rail_at(ivec3 p) { return is_rail(B(p)); }
static int rail_nshapes(Block b) { return kind(b) == B_RAIL ? 10 : 6; }
int rs_rail_shape(Block b) { return BLOCKS[b].var % rail_nshapes(b); }
// connection directions (h4 indices) of a rail shape and whether each end climbs
void rs_rail_ends(int shape, int out[2], bool up[2]) {
    static const int E[10][2] = {{0, 2}, {1, 3}, {3, 1}, {1, 3}, {2, 0}, {0, 2}, {2, 1}, {2, 3}, {0, 3}, {0, 1}};
    out[0] = E[shape][0]; out[1] = E[shape][1];
    up[0] = false; up[1] = shape >= 2 && shape <= 5;   // the second end of an ascending rail is the high one
}
static int compute_rail_shape(ivec3 p, Block b, int fallback) {
    bool conn[4] = {0}, upc[4] = {0};
    for (int k = 0; k < 4; k++) {
        int f = face_of_h4(k);
        ivec3 n = nb(p, f);
        if (rail_at(n) || rail_at(iv3(n.x, n.y - 1, n.z))) conn[k] = true;
        else if (rail_at(iv3(n.x, n.y + 1, n.z))) { conn[k] = true; upc[k] = true; }
    }
    bool curves = kind(b) == B_RAIL;
    int nc = conn[0] + conn[1] + conn[2] + conn[3];
    if (curves && nc >= 2) {
        if (conn[2] && conn[1] && !(conn[0] && conn[2])) return 6;
        if (conn[2] && conn[3] && !(conn[0] && conn[2])) return 7;
        if (conn[0] && conn[3] && !(conn[0] && conn[2])) return 8;
        if (conn[0] && conn[1] && !(conn[0] && conn[2])) return 9;
    }
    if (conn[0] || conn[2]) { if (upc[0]) return 4; if (upc[2]) return 5; return 0; }
    if (conn[1] || conn[3]) { if (upc[1]) return 2; if (upc[3]) return 3; return 1; }
    return fallback;
}
void rs_rail_placed(ivec3 p, int look_h4) {
    Block b = B(p);
    if (!is_rail(b)) return;
    int pw = BLOCKS[b].var / rail_nshapes(b);
    int sh = compute_rail_shape(p, b, (look_h4 == 1 || look_h4 == 3) ? 1 : 0);
    world_set_block(p.x, p.y, p.z, (Block)(BLOCKS[b].base + pw * rail_nshapes(b) + sh));
    for (int k = 0; k < 4; k++) for (int dy = -1; dy <= 1; dy++) {
        ivec3 n = nb(p, face_of_h4(k)); n.y += dy;
        Block nbk = B(n);
        if (!is_rail(nbk)) continue;
        int npw = BLOCKS[nbk].var / rail_nshapes(nbk);
        int cur = BLOCKS[nbk].var % rail_nshapes(nbk);
        int ns = compute_rail_shape(n, nbk, cur);
        if (ns != cur) world_set_block(n.x, n.y, n.z, (Block)(BLOCKS[nbk].base + npw * rail_nshapes(nbk) + ns));
    }
}
static bool rail_chain_powered(ivec3 p, Block b, int depth, ivec3 from) {
    if (rs_powered(p)) return true;
    if (depth >= 8) return false;
    int ends[2]; bool up[2];
    rs_rail_ends(rs_rail_shape(b), ends, up);
    for (int i = 0; i < 2; i++) {
        ivec3 n = nb(p, face_of_h4(ends[i]));
        for (int dy = -1; dy <= 1; dy++) {
            ivec3 q = iv3(n.x, n.y + dy, n.z);
            if (iv3_eq(q, from)) continue;
            Block c = B(q);
            if (kind(c) == kind(b) && rail_chain_powered(q, c, depth + 1, p)) return true;
        }
    }
    return false;
}

// ------------------------------------------------------------------ the update entry point
static void note_play(ivec3 p) {
    BlockEntity *be = world_block_entity(p, true, BE_NONE);
    if (!block_is_air(B(nb(p, FACE_U)))) return;
    sound_play_at(SND_CLICK, iv3_center(p), 1, powf(2.0f, (be->data - 12) / 12.0f));
    fx_crit(v3(p.x + 0.5f, p.y + 1.2f, p.z + 0.5f), 2);
}
static void door_set(ivec3 p, Block b, bool open) {
    if (block_open(b) == open) return;
    world_set_block(p.x, p.y, p.z, block_set_open(b, open));
    if (BLOCKS[b].place == PL_DOOR) {
        int dy = block_upper_half(b) ? -1 : 1;
        Block o = world_get_block(p.x, p.y + dy, p.z);
        if (BLOCKS[o].base == BLOCKS[b].base) world_set_block_silent(p.x, p.y + dy, p.z, block_set_open(o, open));
    }
    sound_play_at(SND_DIG_WOOD, iv3_center(p), 0.6f, open ? 1.2f : 0.9f);
}

void redstone_update(ivec3 p) {
    Block b = B(p);
    const BlockDef *d = &BLOCKS[b];
    int k = d->base;
    int tgt;
    if (k == B_REDSTONE_WIRE) { recompute_dust(p); return; }
    if (is_torch(b)) {
        bool want_lit = power_from(p, torch_support(b)) == 0;
        if (take_pending(p, &tgt)) { want_lit = tgt; Block s = kind(b) == B_REDSTONE_TORCH ? (Block)(k + (want_lit ? 0 : 1)) : (Block)(k + (want_lit ? 0 : 4) + d->var % 4); set_state(p, s); return; }
        if (want_lit != torch_lit(b)) schedule_change(p, want_lit, RT);
        return;
    }
    if (is_repeater(b)) {
        int front = diode_front(b);
        bool in = power_from(p, FACE_OPP[front]) > 0;
        // locked by a powered diode pointing into a side
        for (int s = 0; s < 4; s++) {
            int f = face_of_h4(s);
            if (f == front || f == FACE_OPP[front]) continue;
            Block sb = B(nb(p, f));
            if ((is_repeater(sb) || is_comparator(sb)) && diode_front(sb) == FACE_OPP[f] && diode_powered(sb)) return;
        }
        if (take_pending(p, &tgt)) { set_state(p, (Block)(k + (d->var & ~1) + (tgt ? 1 : 0))); return; }
        int delay = ((d->var >> 1) & 3) + 1;
        if (in != diode_powered(b) && !has_pending(p)) schedule_change(p, in, delay * RT);
        return;
    }
    if (is_comparator(b)) {
        int front = diode_front(b);
        ivec3 back = nb(p, FACE_OPP[front]);
        int rear = rs_container_signal(back);
        if (rear < 0) rear = power_from(p, FACE_OPP[front]);
        int side = 0;
        for (int s = 0; s < 4; s++) {
            int f = face_of_h4(s);
            if (f == front || f == FACE_OPP[front]) continue;
            Block sb = B(nb(p, f));
            if (kind(sb) == B_REDSTONE_WIRE || is_repeater(sb) || is_comparator(sb) || kind(sb) == B_REDSTONE_BLOCK) side = MAXI(side, power_from(p, f));
        }
        bool subtract = (d->var >> 1) & 1;
        int out = subtract ? MAXI(rear - side, 0) : (rear >= side ? rear : 0);
        BlockEntity *be = world_block_entity(p, true, BE_COMPARATOR);
        if (take_pending(p, &tgt)) {
            be->data = tgt;
            Block s = (Block)(k + (d->var & ~1) + (tgt > 0 ? 1 : 0));
            if (s != b) world_set_block_silent(p.x, p.y, p.z, s);
            notify(p);
            return;
        }
        if (out != be->data) schedule_change(p, out, RT);
        return;
    }
    if (is_button(b) || k == B_TARGET || is_observer(b)) {
        if (take_pending(p, &tgt)) {
            Block s = is_observer(b) ? (Block)(k + (tgt ? 6 : 0) + d->var % 6) : is_button(b) ? (Block)(k + (tgt ? 6 : 0) + d->var % 6) : (Block)(k + (tgt ? 1 : 0));
            set_state(p, s);
            if (is_observer(b) && tgt) schedule_change(p, 0, RT);
        }
        return;
    }
    if (is_plate(b)) {
        if (take_pending(p, &tgt)) {   // periodic check while pressed
            bool any = false;
            AABB a = {v3((f32)p.x, (f32)p.y, (f32)p.z), v3(p.x + 1.0f, p.y + 0.3f, p.z + 1.0f)};
            bool stone = k == B_STONE_PRESSURE_PLATE || k == B_POLISHED_BLACKSTONE_PRESSURE_PLATE;
            ENT_FOREACH(e) if ((e->kind == ENT_MOB || e->kind == ENT_PLAYER || (!stone && (e->kind == ENT_ITEM || e->kind == ENT_ARROW))) && aabb_overlap(entity_aabb(e), a)) any = true;
            if (any) schedule_change(p, 1, 20 * 3);
            else set_state(p, (Block)k);
            notify(p);
        }
        return;
    }
    if (k == B_DETECTOR_RAIL) {
        if (take_pending(p, &tgt)) {
            bool any = false;
            AABB a = {v3((f32)p.x, (f32)p.y, (f32)p.z), v3(p.x + 1.0f, p.y + 0.8f, p.z + 1.0f)};
            ENT_FOREACH(e) if ((e->kind == ENT_VEHICLE) && aabb_overlap(entity_aabb(e), a)) any = true;
            if (any) schedule_change(p, 1, 20 * 3);
            else set_state(p, (Block)(k + d->var % 6));
        }
        return;
    }
    if (k == B_POWERED_RAIL || k == B_ACTIVATOR_RAIL) {
        bool on = rail_chain_powered(p, b, 0, p);
        Block s = (Block)(k + (on ? 6 : 0) + d->var % 6);
        if (s != b) set_state(p, s);
        return;
    }
    if (k == B_REDSTONE_LAMP) {
        bool on = rs_powered(p);
        if (take_pending(p, &tgt)) { if (!on) world_set_block_silent(p.x, p.y, p.z, B_REDSTONE_LAMP); return; }
        if (on && d->var == 0) world_set_block_silent(p.x, p.y, p.z, B_REDSTONE_LAMP + 1);
        else if (!on && d->var == 1) schedule_change(p, 0, 2 * RT);
        return;
    }
    if (k == B_COMMAND_BLOCK || k == B_REPEATING_COMMAND_BLOCK || k == B_CHAIN_COMMAND_BLOCK) { cmdblock_redstone(p, rs_input_power(p) > 0); return; }
    if (is_piston(b)) {
        int f = d->var % 6;
        // quasi-connectivity: power reaching the space above the piston also counts (Java-style BUD behaviour)
        bool on = input_except(p, f) > 0 || (f != FACE_U && input_except(nb(p, FACE_U), FACE_D) > 0);
        bool ext = d->var >= 6;
        if (on && !ext) piston_extend(p, b);
        else if (!on && ext) piston_retract(p, b);
        return;
    }
    if (k == B_TNT) { if (rs_powered(p)) { world_set_block(p.x, p.y, p.z, B_AIR); tnt_spawn(v3(p.x + 0.5f, (f32)p.y, p.z + 0.5f), 4 * TICKRATE); } return; }
    if (k == B_NOTE_BLOCK) {
        bool on = rs_powered(p);
        if (on && d->var == 0) { world_set_block_silent(p.x, p.y, p.z, B_NOTE_BLOCK + 1); note_play(p); }
        else if (!on && d->var == 1) world_set_block_silent(p.x, p.y, p.z, B_NOTE_BLOCK);
        return;
    }
    if (k == B_DISPENSER || k == B_DROPPER) { if (edge(p, rs_powered(p))) dispense(p, b); return; }
    if (d->place == PL_DOOR || d->place == PL_TRAPDOOR || d->place == PL_GATE) {
        ivec3 q = p;
        bool on = rs_powered(p);
        if (d->place == PL_DOOR) {   // either half powers the door
            ivec3 other = iv3(p.x, p.y + (block_upper_half(b) ? -1 : 1), p.z);
            on = on || rs_powered(other);
            if (block_upper_half(b)) q = other;
        }
        HSlot *m = hm_find(&memory, pkey(q), false);
        bool was = m && m->val;
        if (on && !was) { door_set(p, b, true); hm_find(&memory, pkey(q), true)->val = 1; }
        else if (!on && was) { door_set(p, b, false); hm_del(&memory, pkey(q)); }
        return;
    }
    if (k == B_DAYLIGHT_DETECTOR) { add_detector(p); notify(p); return; }
}

// ------------------------------------------------------------------ player / entity triggers
void rs_toggle_lever(ivec3 p) {
    Block b = B(p);
    bool on = BLOCKS[b].var < 6;
    set_state(p, (Block)(BLOCKS[b].base + (on ? 6 : 0) + BLOCKS[b].var % 6));
    sound_play_at(SND_LEVER, iv3_center(p), 0.7f, on ? 1.0f : 0.85f);
}
void rs_press_button(ivec3 p) {
    Block b = B(p);
    if (BLOCKS[b].var >= 6) return;
    set_state(p, (Block)(BLOCKS[b].base + 6 + BLOCKS[b].var % 6));
    bool wood = BLOCKS[b].sound == SG_WOOD;
    schedule_change(p, 0, (wood ? 30 : 20) * 3);
    sound_play_at(SND_CLICK, iv3_center(p), 0.6f, 0.6f);
}
void rs_hit_target(ivec3 p, bool arrow) {
    Block b = B(p);
    if (kind(b) != B_TARGET) return;
    set_state(p, (Block)(B_TARGET + 1));
    schedule_change(p, 0, (arrow ? 20 : 8) * 3);
}
// pressure plates and detector rails: called for entities resting on a block
void rs_entity_on(ivec3 p, Entity *e) {
    Block b = B(p);
    if (is_plate(b) && !(BLOCKS[b].var & 1)) {
        int k = kind(b);
        bool stone = k == B_STONE_PRESSURE_PLATE || k == B_POLISHED_BLACKSTONE_PRESSURE_PLATE;
        if (stone && e->kind != ENT_MOB && e->kind != ENT_PLAYER) return;
        set_state(p, (Block)(k + 1));
        schedule_change(p, 1, 20 * 3);
        sound_play_at(SND_PLATE, iv3_center(p), 0.5f, 1.0f);
    }
    if (kind(b) == B_DETECTOR_RAIL && BLOCKS[b].var < 6 && e->kind == ENT_VEHICLE) {
        set_state(p, (Block)(B_DETECTOR_RAIL + 6 + BLOCKS[b].var % 6));
        schedule_change(p, 1, 20 * 3);
    }
}
// observers: called by world_set_block for every changed position
void rs_block_changed(ivec3 p) {
    for (int f = 0; f < 6; f++) {
        ivec3 n = nb(p, f);
        Block o = B(n);
        if (!is_observer(o)) continue;
        int face = BLOCKS[o].var % 6;
        if (FACE_OPP[f] != face) continue;     // the observer must look at p
        if (BLOCKS[o].var < 6 && !has_pending(n)) schedule_change(n, 1, RT);
    }
}
// a column became ready: remember its daylight detectors
void rs_scan_column(Column *c) {
    for (int y = 0; y < WORLD_H; y++) for (int z = 0; z < 16; z++) for (int x = 0; x < 16; x++)
        if (BLOCKS[c->blocks[y][z][x]].base == B_DAYLIGHT_DETECTOR) add_detector(iv3(c->cx * 16 + x, y, c->cz * 16 + z));
}

// per game tick: hoppers, daylight detectors
void redstone_tick(void) {
    for (int i = 0; i < g_world.nbents; i++) if (g_world.bents[i].type == BE_HOPPER && world_loaded(g_world.bents[i].pos.x, g_world.bents[i].pos.z)) hopper_tick(&g_world.bents[i]);
    if (g_world.time % 60 == 0) for (int i = 0; i < ndets; i++) {
        Detector *dd = &dets[i];
        if (!world_loaded(dd->p.x, dd->p.z)) continue;
        Block b = B(dd->p);
        if (kind(b) != B_DAYLIGHT_DETECTOR) { hm_del(&det_set, pkey(dd->p)); dets[i--] = dets[--ndets]; continue; }
        int l = detector_level(b);
        if (l != dd->level) { dd->level = l; notify(dd->p); }
    }
}

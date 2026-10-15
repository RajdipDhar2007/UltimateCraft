// Tripwire: string between two tripwire hooks on the same line (up to 40 blocks of string).
// Any entity in a string block trips the line: both hooks output redstone power 15 for as long as something
// stands there, plus 10 game ticks. Breaking string trips it once, unless you use shears; right-clicking
// string with shears disarms it (variant 2, it never trips again). Hooks: variant 0..3 = wall side, +4 powered.
#include "world.h"
#include "../entity/entity.h"
#include "../game/game.h"
#include "../audio/audio.h"
#define MC(t) ((t) * TICKRATE / 20)
#define TRIP_MAX 40
typedef struct { ivec3 a, b; int t; } Trip;
static Trip trips[64];
static int ntrips;
int g_tripwire_trips;
static bool is_hook(u16 b) { return BLOCKS[b].base == B_TRIPWIRE_HOOK; }
static void hook_set(ivec3 p, bool on) {
    u16 b = world_get_block(p.x, p.y, p.z);
    if (!is_hook(b)) return;
    int v = BLOCKS[b].var % 4 + (on ? 4 : 0);
    if (BLOCKS[b].var != v) { world_set_block(p.x, p.y, p.z, (u16)(B_TRIPWIRE_HOOK + v)); sound_play_at(SND_CLICK, iv3_center(p), 0.5f, on ? 0.9f : 0.6f); }
}
// walk the string through p along one axis; true if hooks close both ends
static bool find_line(ivec3 p, int axis, ivec3 *ha, ivec3 *hb) {
    for (int s = -1; s <= 1; s += 2) {
        ivec3 q = p; bool found = false;
        for (int i = 1; i <= TRIP_MAX + 1; i++) {
            q = axis ? iv3(p.x + s * i, p.y, p.z) : iv3(p.x, p.y, p.z + s * i);
            u16 b = world_get_block(q.x, q.y, q.z);
            if (BLOCKS[b].base == B_TRIPWIRE) continue;
            if (is_hook(b) && (BLOCKS[b].var % 2) == (axis ? 1 : 0)) found = true;   // hook mounted across this axis
            break;
        }
        if (!found) return false;
        if (s < 0) *ha = q; else *hb = q;
    }
    return true;
}
bool tripwire_trip(ivec3 p) {
    u16 b = world_get_block(p.x, p.y, p.z);
    if (BLOCKS[b].base != B_TRIPWIRE || BLOCKS[b].var == 2) return false;
    for (int axis = 0; axis < 2; axis++) {
        ivec3 ha, hb;
        if (!find_line(p, axis, &ha, &hb)) continue;
        int i = 0;
        for (; i < ntrips; i++) if (iv3_eq(trips[i].a, ha) && iv3_eq(trips[i].b, hb)) break;
        if (i == ntrips) { if (ntrips >= (int)ARRAY_LEN(trips)) return false; trips[ntrips++] = (Trip){ha, hb, 0}; g_tripwire_trips++; }
        trips[i].t = MC(10);
        hook_set(ha, true); hook_set(hb, true);
        return true;
    }
    return false;
}
void tripwire_tick(void) {
    ENT_FOREACH(e) {
        if (e->kind != ENT_PLAYER && e->kind != ENT_MOB && e->kind != ENT_ITEM && e->kind != ENT_ARROW) continue;
        ivec3 f = v3_floor(e->pos);
        if (BLOCKS[world_get_block(f.x, f.y, f.z)].base == B_TRIPWIRE) tripwire_trip(f);
    }
    for (int i = 0; i < ntrips; i++) {
        if (--trips[i].t > 0) continue;
        hook_set(trips[i].a, false); hook_set(trips[i].b, false);
        trips[i--] = trips[--ntrips];
    }
}

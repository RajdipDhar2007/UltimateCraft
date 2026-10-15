// Flood-fill lighting: one sky channel + red/green/blue block light, 4 bits each.
// Queues live on the heap (the original engine kept up to 3 MB of queues on the stack, which overflows
// the 1 MB default stack on Windows).
#include "world.h"

typedef struct { int x, y, z; u8 v; } LNode;
typedef struct { LNode *a; size_t head, n, cap; } LQueue;

static LQueue q_inc, q_rem;

static inline void q_push(LQueue *q, int x, int y, int z, u8 v) {
    if (q->n == q->cap) {
        if (q->head > 0) { memmove(q->a, q->a + q->head, (q->n - q->head) * sizeof(LNode)); q->n -= q->head; q->head = 0; }
        if (q->n == q->cap) { q->cap = q->cap ? q->cap * 2 : 65536; q->a = xrealloc(q->a, q->cap * sizeof(LNode)); }
    }
    q->a[q->n++] = (LNode){x, y, z, v};
}
static inline bool q_pop(LQueue *q, LNode *out) {
    if (q->head >= q->n) { q->head = q->n = 0; return false; }
    *out = q->a[q->head++];
    return true;
}

static inline int get_ch(int x, int y, int z, int c, bool *ok) {
    if (y < 0 || y >= WORLD_H) { *ok = false; return 0; }
    Column *col = world_column_at(x, z);
    if (!col) { *ok = false; return 0; }
    *ok = true;
    return L_CH(col->light[y][z & 15][x & 15], c);
}
static inline void set_ch(int x, int y, int z, int c, int v) {
    Column *col = world_column_at(x, z);
    if (!col) return;
    u16 *l = &col->light[y][z & 15][x & 15];
    int sh = 12 - 4 * c;
    *l = (u16)((*l & ~(15 << sh)) | (v << sh));
    world_mark_dirty(x, y, z);
}

static void bfs_increase(int c) {
    LNode n;
    while (q_pop(&q_inc, &n)) {
        bool ok;
        int L = get_ch(n.x, n.y, n.z, c, &ok);
        if (!ok || L <= 1) continue;
        for (int f = 0; f < 6; f++) {
            int x = n.x + FACE_DIR[f].x, y = n.y + FACE_DIR[f].y, z = n.z + FACE_DIR[f].z;
            if (y < 0 || y >= WORLD_H) continue;
            Column *col = world_column_at(x, z);
            if (!col) continue;
            u16 b = col->blocks[y][z & 15][x & 15];
            int op = BLOCKS[b].opacity;
            if (op >= 15) continue;
            int nl = (c == 0 && f == FACE_D && L == 15 && op == 0) ? 15 : L - (op > 1 ? op : 1);
            if (nl <= 0) continue;
            int cur = L_CH(col->light[y][z & 15][x & 15], c);
            if (nl > cur) { set_ch(x, y, z, c, nl); q_push(&q_inc, x, y, z, 0); }
        }
    }
}

void light_init_column(Column *col) {
    int bx = col->cx * 16, bz = col->cz * 16;
    // 1) direct sky light straight down + emitters
    for (int z = 0; z < 16; z++) for (int x = 0; x < 16; x++) {
        int lvl = world_has_skylight() ? 15 : 0, h = 0;
        for (int y = WORLD_H - 1; y >= 0; y--) {
            const BlockDef *d = &BLOCKS[col->blocks[y][z][x]];
            if (d->opacity >= 15) lvl = 0;
            else if (d->opacity > 0) lvl = MAXI(0, lvl - d->opacity);
            if (d->opacity > 0 && !h) h = y + 1;
            col->light[y][z][x] = (u16)((lvl << 12) | (d->emit_r << 8) | (d->emit_g << 4) | d->emit_b);
        }
        col->heightmap[z][x] = (u16)h;
    }
    // 2) spread each channel: seeds are cells that can light a darker neighbour + lit border cells of neighbours
    for (int c = 0; c < 4; c++) {
        if (c == 0 && !world_has_skylight()) continue;
        for (int y = 0; y < WORLD_H; y++) for (int z = 0; z < 16; z++) for (int x = 0; x < 16; x++) {
            int L = L_CH(col->light[y][z][x], c);
            if (L <= 1) continue;
            if (c > 0) { q_push(&q_inc, bx + x, y, bz + z, 0); continue; }
            if (L == 15 && y >= col->heightmap[z][x]) {
                // open sky: only seed where a horizontal neighbour is shadowed (cave mouths, overhangs)
                bool need = false;
                for (int f = 0; f < 6 && !need; f++) {
                    if (f == FACE_U || f == FACE_D) continue;
                    int nx = x + FACE_DIR[f].x, nz = z + FACE_DIR[f].z;
                    if (nx >= 0 && nx < 16 && nz >= 0 && nz < 16) need = y < col->heightmap[nz][nx];
                    else need = true;  // column border: let neighbours pull/push
                }
                if (!need) continue;
            }
            q_push(&q_inc, bx + x, y, bz + z, 0);
        }
        // light flowing in from already-lit neighbour columns
        for (int f = 0; f < 6; f++) {
            if (f == FACE_U || f == FACE_D) continue;
            Column *n = world_column(col->cx + FACE_DIR[f].x, col->cz + FACE_DIR[f].z);
            if (!n) continue;
            for (int y = 0; y < WORLD_H; y++) for (int i = 0; i < 16; i++) {
                int lx, lz;
                if (f == FACE_W) { lx = 15; lz = i; } else if (f == FACE_E) { lx = 0; lz = i; }
                else if (f == FACE_N) { lx = i; lz = 15; } else { lx = i; lz = 0; }
                if (L_CH(n->light[y][lz][lx], c) > 1) q_push(&q_inc, n->cx * 16 + lx, y, n->cz * 16 + lz, 0);
            }
        }
        bfs_increase(c);
    }
}

void light_block_changed(int x, int y, int z) {
    Column *col = world_column_at(x, z);
    if (!col) return;
    const BlockDef *d = &BLOCKS[col->blocks[y][z & 15][x & 15]];
    int emit[4] = {0, d->emit_r, d->emit_g, d->emit_b};
    for (int c = 0; c < 4; c++) {
        if (c == 0 && !world_has_skylight()) continue;
        bool ok;
        int old = get_ch(x, y, z, c, &ok);
        // removal pass
        if (old > 0) {
            set_ch(x, y, z, c, 0);
            q_push(&q_rem, x, y, z, (u8)old);
            LNode n;
            while (q_pop(&q_rem, &n)) {
                for (int f = 0; f < 6; f++) {
                    int nx = n.x + FACE_DIR[f].x, ny = n.y + FACE_DIR[f].y, nz = n.z + FACE_DIR[f].z;
                    int nv = get_ch(nx, ny, nz, c, &ok);
                    if (!ok || nv == 0) continue;
                    if (nv < n.v || (c == 0 && f == FACE_D && n.v == 15 && nv == 15)) {
                        set_ch(nx, ny, nz, c, 0);
                        q_push(&q_rem, nx, ny, nz, (u8)nv);
                    } else {
                        q_push(&q_inc, nx, ny, nz, 0);   // a brighter source: relight from it
                    }
                }
            }
        }
        if (emit[c] > 0) { set_ch(x, y, z, c, emit[c]); q_push(&q_inc, x, y, z, 0); }
        for (int f = 0; f < 6; f++) q_push(&q_inc, x + FACE_DIR[f].x, y + FACE_DIR[f].y, z + FACE_DIR[f].z, 0);
        // sky directly above an opened column
        if (c == 0 && y + 1 >= WORLD_H) { set_ch(x, y, z, 0, BLOCKS[col->blocks[y][z & 15][x & 15]].opacity >= 15 ? 0 : 15); q_push(&q_inc, x, y, z, 0); }
        bfs_increase(c);
    }
}

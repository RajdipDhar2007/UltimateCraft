// Builds one 16x16x16 section mesh: hidden-face culling, smooth lighting and ambient occlusion.
#include "world.h"

typedef struct { u16 x, y, z; u16 u, v; u8 sky, r, g, b; u8 shade; u8 flags; u16 tint; } CVert;  // 18 bytes; tint = RGB565, 0xFFFF none
#define MAX_QUADS (16 * 16 * 16 * 12)
static CVert *vbuf[2];      // 0 = opaque+cutout, 1 = translucent
static int vcount[2];

#define FL_WATER 1
#define FL_LAVA 2
#define FL_WAVE_PLANT 4
#define FL_WAVE_LEAF 8

static u16 pb[18][18][18];  // padded blocks [y][z][x]
static u16 tints[3][18][18]; // grass, foliage, water colour per padded column (RGB565)
static u16 cur_tint = 0xFFFF;
static u16 pl[18][18][18];  // padded light

// corner offsets per face: BL, BR, TR, TL as seen from outside
static const u8 FACE_CORNERS[6][4][3] = {
    {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}},  // W
    {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}},  // E
    {{1, 0, 1}, {0, 0, 1}, {0, 0, 0}, {1, 0, 0}},  // D
    {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}},  // U
    {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}},  // N
    {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},  // S
};
static const f32 CORNER_UV[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};

void mesher_init(void) {
    for (int i = 0; i < 2; i++) vbuf[i] = xmalloc(sizeof(CVert) * 4 * MAX_QUADS);
}

static inline bool occ(int x, int y, int z) { return BLOCKS[pb[y + 1][z + 1][x + 1]].opaque; }
static inline u16 lit(int x, int y, int z) { return pl[y + 1][z + 1][x + 1]; }

static inline void emit(int layer, f32 px, f32 py, f32 pz, int tile, f32 tu, f32 tv, const u16 lsum[4], int ao, int face, int flags) {
    if (vcount[layer] >= 4 * MAX_QUADS) return;
    CVert *v = &vbuf[layer][vcount[layer]++];
    v->x = (u16)(px * 16 + 0.5f); v->y = (u16)(py * 16 + 0.5f); v->z = (u16)(pz * 16 + 0.5f);
    v->u = (u16)(((tile % ATLAS_TILES) * 16 + tu * 16) * 16 + 0.5f);
    v->v = (u16)(((tile / ATLAS_TILES) * 16 + tv * 16) * 16 + 0.5f);
    v->sky = (u8)lsum[0]; v->r = (u8)lsum[1]; v->g = (u8)lsum[2]; v->b = (u8)lsum[3];
    v->shade = (u8)((ao & 3) | (face << 2));
    v->flags = (u8)flags; v->tint = cur_tint;
}

// light of one cell expanded to 0..255 per channel
static inline void light_of(u16 l, u16 out[4]) { out[0] = L_SKY(l) * 17; out[1] = L_R(l) * 17; out[2] = L_G(l) * 17; out[3] = L_B(l) * 17; }

// full cube face (also used for liquids, cactus) with per-vertex AO + smooth light
static void cube_face(int layer, int x, int y, int z, int f, int tile, int flags, f32 top_h, f32 inset, bool use_ao) {
    ivec3 n = FACE_DIR[f];
    int ax1, ax2;   // tangent axes
    if (n.x) { ax1 = 1; ax2 = 2; } else if (n.y) { ax1 = 0; ax2 = 2; } else { ax1 = 0; ax2 = 1; }
    int ox = x + n.x, oy = y + n.y, oz = z + n.z;
    int ao[4]; u16 L[4][4];
    for (int i = 0; i < 4; i++) {
        const u8 *c = FACE_CORNERS[f][i];
        int s[3] = {c[0] ? 1 : -1, c[1] ? 1 : -1, c[2] ? 1 : -1};
        int d1[3] = {0, 0, 0}, d2[3] = {0, 0, 0};
        d1[ax1] = s[ax1]; d2[ax2] = s[ax2];
        bool o1 = occ(ox + d1[0], oy + d1[1], oz + d1[2]);
        bool o2 = occ(ox + d2[0], oy + d2[1], oz + d2[2]);
        bool oc = occ(ox + d1[0] + d2[0], oy + d1[1] + d2[1], oz + d1[2] + d2[2]);
        ao[i] = use_ao ? ((o1 && o2) ? 0 : 3 - (o1 + o2 + oc)) : 3;
        // smooth light: average of the non-opaque cells touching this corner on the face side
        u16 acc[4] = {0}, tmp[4]; int cnt = 0;
        light_of(lit(ox, oy, oz), tmp); for (int k = 0; k < 4; k++) acc[k] += tmp[k]; cnt++;
        if (!o1) { light_of(lit(ox + d1[0], oy + d1[1], oz + d1[2]), tmp); for (int k = 0; k < 4; k++) acc[k] += tmp[k]; cnt++; }
        if (!o2) { light_of(lit(ox + d2[0], oy + d2[1], oz + d2[2]), tmp); for (int k = 0; k < 4; k++) acc[k] += tmp[k]; cnt++; }
        if (!oc && !(o1 && o2)) { light_of(lit(ox + d1[0] + d2[0], oy + d1[1] + d2[1], oz + d1[2] + d2[2]), tmp); for (int k = 0; k < 4; k++) acc[k] += tmp[k]; cnt++; }
        for (int k = 0; k < 4; k++) L[i][k] = (u16)(acc[k] / cnt);
    }
    // flip the quad diagonal to avoid anisotropic AO artefacts
    int order[4] = {0, 1, 2, 3};
    if (ao[0] + ao[2] < ao[1] + ao[3]) { order[0] = 1; order[1] = 2; order[2] = 3; order[3] = 0; }
    for (int k = 0; k < 4; k++) {
        int i = order[k];
        const u8 *c = FACE_CORNERS[f][i];
        f32 px = c[0], py = c[1], pz = c[2];
        if (inset > 0 && f != FACE_U && f != FACE_D) {   // cactus: sides pulled in by 1/16
            if (n.x) px = n.x > 0 ? 1 - inset : inset;
            if (n.z) pz = n.z > 0 ? 1 - inset : inset;
        }
        f32 tu = CORNER_UV[i][0], tv = CORNER_UV[i][1];
        if (py > 0.5f && top_h < 1) { py = top_h; if (f != FACE_U && f != FACE_D) tv = 1 - top_h; }
        emit(layer, x + px, y + py, z + pz, tile, tu, tv, L[i], ao[i], f, flags);
    }
}

static void cross_quads(int layer, int x, int y, int z, int tile, u16 light) {
    u16 L[4]; light_of(light, L);
    const f32 i = 0.15f;
    const f32 P[2][2][2] = {{{i, i}, {1 - i, 1 - i}}, {{i, 1 - i}, {1 - i, i}}};
    for (int p = 0; p < 2; p++) for (int side = 0; side < 2; side++) {
        f32 ax = P[p][side][0], az = P[p][side][1], bx = P[p][!side][0], bz = P[p][!side][1];
        emit(layer, x + ax, (f32)y, z + az, tile, 0, 1, L, 3, FACE_U, 0);
        emit(layer, x + bx, (f32)y, z + bz, tile, 1, 1, L, 3, FACE_U, 0);
        emit(layer, x + bx, y + 1.0f, z + bz, tile, 1, 0, L, 3, FACE_U, FL_WAVE_PLANT);
        emit(layer, x + ax, y + 1.0f, z + az, tile, 0, 0, L, 3, FACE_U, FL_WAVE_PLANT);
    }
}

static void torch_quads(int x, int y, int z, const BlockDef *d, u16 light) {
    u16 L[4]; light_of(light, L);
    L[1] = L[2] = L[3] = 255;  // torches are self lit
    const f32 a = 7 / 16.0f, b = 9 / 16.0f, h = 10 / 16.0f;
    for (int f = 0; f < 6; f++) {
        if (f == FACE_D) continue;
        int tile = d->tex[f];
        for (int i = 0; i < 4; i++) {
            const u8 *c = FACE_CORNERS[f][i];
            f32 px = c[0] ? b : a, py = c[1] ? h : 0, pz = c[2] ? b : a;
            f32 tu, tv;
            if (f == FACE_U) { tu = (7 + CORNER_UV[i][0] * 2) / 16.0f; tv = (7 + CORNER_UV[i][1] * 2) / 16.0f; }
            else { tu = (7 + CORNER_UV[i][0] * 2) / 16.0f; tv = (6 + CORNER_UV[i][1] * 10) / 16.0f; }
            emit(0, x + px, y + py, z + pz, tile, tu, tv, L, 3, f, 0);
        }
    }
}

static inline u16 rgb565(u32 c) { return (u16)((((c >> 19) & 31) << 11) | (((c >> 10) & 63) << 5) | ((c >> 3) & 31)); }

// tint for one block face (or 0xFFFF)
static u16 tint_for(const BlockDef *d, int x, int z, int face) {
    if (d->tint == TINT_NONE || !(d->tint_faces & (1 << face))) return 0xFFFF;
    switch (d->tint) {
    case TINT_GRASS: return tints[0][z + 1][x + 1];
    case TINT_FOLIAGE: return tints[1][z + 1][x + 1];
    case TINT_WATER: return tints[2][z + 1][x + 1];
    case TINT_SPRUCE: return rgb565(0x619961);
    case TINT_BIRCH: return rgb565(0x80A755);
    case TINT_LILY: return rgb565(0x208030);
    default: return 0xFFFF;
    }
}

static void gather_tints(Column *col) {
    u32 acc[3][18][18][3];
    static u8 bio[20][20];
    int bx = col->cx * 16, bz = col->cz * 16;
    for (int z = -2; z < 18; z++) for (int x = -2; x < 18; x++) {
        Column *c = (x >= 0 && x < 16 && z >= 0 && z < 16) ? col : world_column_at(bx + x, bz + z);
        bio[z + 2][x + 2] = c ? c->biome[(bz + z) & 15][(bx + x) & 15] : col->biome[clampi(z, 0, 15)][clampi(x, 0, 15)];
    }
    for (int z = 0; z < 18; z++) for (int x = 0; x < 18; x++) {
        for (int k = 0; k < 3; k++) acc[k][z][x][0] = acc[k][z][x][1] = acc[k][z][x][2] = 0;
        for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) {
            const BiomeDef *b = &BIOMES[bio[z + 1 + dz][x + 1 + dx]];
            u32 cs[3] = {b->grass, b->foliage, b->water};
            for (int k = 0; k < 3; k++) { acc[k][z][x][0] += (cs[k] >> 16) & 255; acc[k][z][x][1] += (cs[k] >> 8) & 255; acc[k][z][x][2] += cs[k] & 255; }
        }
        for (int k = 0; k < 3; k++) tints[k][z][x] = rgb565(((acc[k][z][x][0] / 9) << 16) | ((acc[k][z][x][1] / 9) << 8) | (acc[k][z][x][2] / 9));
    }
}

static void crop_quads(int x, int y, int z, int tile, u16 light) {
    u16 L[4]; light_of(light, L);
    for (int axis = 0; axis < 2; axis++) for (int k = 0; k < 2; k++) {
        f32 o = k ? 12 / 16.0f : 4 / 16.0f;
        for (int side = 0; side < 2; side++) {
            f32 a0 = side ? 1 : 0, a1 = side ? 0 : 1;
            f32 p[4][3];
            if (axis == 0) { f32 q[4][3] = {{a0, 0, o}, {a1, 0, o}, {a1, 1, o}, {a0, 1, o}}; memcpy(p, q, sizeof q); }
            else { f32 q[4][3] = {{o, 0, a0}, {o, 0, a1}, {o, 1, a1}, {o, 1, a0}}; memcpy(p, q, sizeof q); }
            static const f32 UV[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
            for (int i = 0; i < 4; i++) emit(0, x + p[i][0], y + p[i][1] - 1 / 16.0f, z + p[i][2], tile, UV[i][0], UV[i][1], L, 3, FACE_U, i >= 2 ? FL_WAVE_PLANT : 0);
        }
    }
}

static void flat_quad(int x, int y, int z, const BlockDef *d, u16 light) {
    u16 L[4]; light_of(light, L);
    f32 h = 1 / 16.0f;
    static const f32 P[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
    bool r90 = (d->flags & BF_ROT90) != 0;
    for (int side = 0; side < 2; side++) for (int k = 0; k < 4; k++) {
        int i = side ? 3 - k : k;
        f32 tu = r90 ? P[i][1] : P[i][0], tv = r90 ? 1 - P[i][0] : P[i][1];
        emit(0, x + P[i][0], y + h, z + P[i][1], d->tex[FACE_U], tu, tv, L, 3, side ? FACE_D : FACE_U, 0);
    }
}

// arbitrary quad (corners in block units relative to the cell, UV 0..1 of a tile), both sides optional
static void quad4(int layer, int x, int y, int z, int tile, const f32 P[4][3], const f32 UV[4][2], u16 light, bool twosided, int flags) {
    u16 L[4]; light_of(light, L);
    for (int side = 0; side < (twosided ? 2 : 1); side++) for (int k = 0; k < 4; k++) {
        int i = side ? 3 - k : k;
        emit(layer, x + P[i][0], y + P[i][1], z + P[i][2], tile, UV[i][0], UV[i][1], L, 3, side ? FACE_D : FACE_U, flags);
    }
}
static int pad_get_base_x, pad_get_base_y, pad_get_base_z;
static u16 pad_get(int x, int y, int z) {   // world coords -> padded copy (only +-1 around the section is valid)
    int lx = x - pad_get_base_x, ly = y - pad_get_base_y, lz = z - pad_get_base_z;
    if (lx < -1 || lx > 16 || ly < -1 || ly > 16 || lz < -1 || lz > 16) return world_get_block_or(x, y, z, B_AIR);
    return pb[ly + 1][lz + 1][lx + 1];
}
static inline u16 rgb565c(u32 c) { return (u16)((((c >> 19) & 31) << 11) | (((c >> 10) & 63) << 5) | ((c >> 3) & 31)); }
static void wire_quads(int x, int y, int z, const BlockDef *d, u16 light, int wx, int wy, int wz) {
    int m = rs_wire_mask_with(pad_get, wx, wy, wz);
    int pw = d->var;
    f32 t = pw / 15.0f;
    u32 col = ((u32)(75 + 180 * t) << 16) | ((u32)(t > 0.6f ? (t - 0.6f) * 120 : 0) << 8);
    cur_tint = rgb565c(col);
    int tile = d->tex[FACE_U];
    const f32 h = 0.02f;
    int conn = m & 15;
    if (conn == 1 || conn == 4) conn = 5;   // a single connection draws a full line
    if (conn == 2 || conn == 8) conn = 10;
    // centre
    f32 a = 5 / 16.0f, b = 11 / 16.0f;
    if (conn == 0) { a = 3 / 16.0f; b = 13 / 16.0f; }
    if (conn != 5 && conn != 10) {
        f32 P[4][3] = {{a, h, b}, {b, h, b}, {b, h, a}, {a, h, a}};
        f32 UV[4][2] = {{a, b}, {b, b}, {b, a}, {a, a}};
        quad4(0, x, y, z, tile, P, UV, light, false, 0);
    }
    for (int k = 0; k < 4; k++) if (conn & (1 << k)) {
        // arm from the cell edge to the centre (N: z 0..5/16, E: x 11/16..1, S: z 11/16..1, W: x 0..5/16)
        f32 x0, x1, z0, z1;
        f32 c0 = 5 / 16.0f, c1 = 11 / 16.0f;
        bool line = (conn == 5 && (k == 0 || k == 2)) || (conn == 10 && (k == 1 || k == 3));
        if (k == 0) { x0 = c0; x1 = c1; z0 = 0; z1 = line ? 0.5f : c0; }
        else if (k == 2) { x0 = c0; x1 = c1; z0 = line ? 0.5f : c1; z1 = 1; }
        else if (k == 1) { z0 = c0; z1 = c1; x0 = line ? 0.5f : c1; x1 = 1; }
        else { z0 = c0; z1 = c1; x0 = 0; x1 = line ? 0.5f : c0; }
        f32 P[4][3] = {{x0, h, z1}, {x1, h, z1}, {x1, h, z0}, {x0, h, z0}};
        bool vert = k == 0 || k == 2;
        f32 UV[4][2];
        if (vert) { f32 u[4][2] = {{0.35f, z1}, {0.65f, z1}, {0.65f, z0}, {0.35f, z0}}; memcpy(UV, u, sizeof UV); }
        else { f32 u[4][2] = {{x0, 0.65f}, {x1, 0.65f}, {x1, 0.35f}, {x0, 0.35f}}; memcpy(UV, u, sizeof UV); }
        // the dust tile has its stripe along v; horizontal arms use a rotated mapping
        if (!vert) for (int i = 0; i < 4; i++) { f32 tu = UV[i][0]; UV[i][0] = UV[i][1]; UV[i][1] = tu; }
        quad4(0, x, y, z, tile, P, UV, light, false, 0);
        if (m & (16 << k)) {   // climb the side of the neighbouring block
            int f = H4_FACE[k];
            f32 off = 0.02f;
            f32 px = FACE_DIR[f].x > 0 ? 1 - off : FACE_DIR[f].x < 0 ? off : 0, pz = FACE_DIR[f].z > 0 ? 1 - off : FACE_DIR[f].z < 0 ? off : 0;
            f32 Q[4][3];
            if (FACE_DIR[f].x) { f32 q[4][3] = {{px, 0, c0}, {px, 0, c1}, {px, 1, c1}, {px, 1, c0}}; memcpy(Q, q, sizeof Q); }
            else { f32 q[4][3] = {{c0, 0, pz}, {c1, 0, pz}, {c1, 1, pz}, {c0, 1, pz}}; memcpy(Q, q, sizeof Q); }
            f32 V[4][2] = {{0.35f, 1}, {0.65f, 1}, {0.65f, 0}, {0.35f, 0}};
            quad4(0, x, y, z, tile, Q, V, light, true, 0);
        }
    }
    cur_tint = 0xFFFF;
}
static void rail_quads(int x, int y, int z, const BlockDef *d, u16 light) {
    int shapes = d->base == B_RAIL ? 10 : 6;
    int sh = d->var % shapes;
    f32 h = 1 / 16.0f;
    int tile = d->tex[FACE_U];
    f32 y00 = h, y10 = h, y11 = h, y01 = h;   // corner heights at (x0,z0) (x1,z0) (x1,z1) (x0,z1)
    if (sh == 2) { y10 = y11 = 1 + h; }        // rises to the east
    if (sh == 3) { y00 = y01 = 1 + h; }        // west
    if (sh == 4) { y00 = y10 = 1 + h; }        // north (z0)
    if (sh == 5) { y01 = y11 = 1 + h; }        // south
    f32 P[4][3] = {{0, y01, 1}, {1, y11, 1}, {1, y10, 0}, {0, y00, 0}};
    // texture rails run along v (north-south); rotate for east-west
    f32 UV[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}};
    bool ew = sh == 1 || sh == 2 || sh == 3;
    if (ew) { f32 r[4][2] = {{1, 1}, {1, 0}, {0, 0}, {0, 1}}; memcpy(UV, r, sizeof UV); }
    if (sh >= 6) {
        tile = d->tex[FACE_N];   // curve tile is drawn as the south-east curve
        static const f32 ROT[4][4][2] = {
            {{0, 1}, {1, 1}, {1, 0}, {0, 0}},     // SE
            {{1, 1}, {0, 1}, {0, 0}, {1, 0}},     // SW (mirror x)
            {{1, 0}, {0, 0}, {0, 1}, {1, 1}},     // NW (mirror both)
            {{0, 0}, {1, 0}, {1, 1}, {0, 1}}};    // NE (mirror z)
        memcpy(UV, ROT[sh - 6], sizeof UV);
    }
    quad4(0, x, y, z, tile, P, UV, light, true, 0);
}

// liquid surface: per-corner heights averaged from the neighbouring fluid cells
static f32 corner_h(int x, int y, int z, int cx, int cz, int kindb) {
    // cells sharing the corner (cx, cz in {0,1})
    f32 sum = 0; int w = 0;
    for (int dz = cz - 1; dz <= cz; dz++) for (int dx = cx - 1; dx <= cx; dx++) {
        u16 b = pb[y + 1][z + dz + 1][x + dx + 1];
        if (BLOCKS[b].base == kindb) {
            if (BLOCKS[pb[y + 2][z + dz + 1][x + dx + 1]].base == kindb) return 1.0f;
            f32 hh = fluid_height(b);
            if (BLOCKS[b].var == 0) { sum += hh * 10; w += 10; } else { sum += hh; w++; }
        } else if (!BLOCKS[b].solid) w++;
    }
    return w ? sum / w : 8.0f / 9.0f;
}
static void liquid_block(int layer, int x, int y, int z, const BlockDef *d, u16 b) {
    int k = d->base == B_BUBBLE_COLUMN ? B_WATER : d->base;
    bool water = k == B_WATER;
    u16 above = pb[y + 2][z + 1][x + 1];
    bool full = BLOCKS[above].base == k || (d->base == B_BUBBLE_COLUMN && block_is_water(above));
    f32 hc[4];   // (0,0) (1,0) (1,1) (0,1)
    if (full) hc[0] = hc[1] = hc[2] = hc[3] = 1.0f;
    else if (d->base == B_BUBBLE_COLUMN) hc[0] = hc[1] = hc[2] = hc[3] = 0.875f;
    else { hc[0] = corner_h(x, y, z, 0, 0, k); hc[1] = corner_h(x, y, z, 1, 0, k); hc[2] = corner_h(x, y, z, 1, 1, k); hc[3] = corner_h(x, y, z, 0, 1, k); }
    int flags = water ? FL_WATER : FL_LAVA;
    cur_tint = tint_for(d, x, z, FACE_U);
    int tile = d->tex[FACE_U];
    for (int f = 0; f < 6; f++) {
        u16 nb = pb[y + 1 + FACE_DIR[f].y][z + 1 + FACE_DIR[f].z][x + 1 + FACE_DIR[f].x];
        if (BLOCKS[nb].opaque) continue;
        bool same = (BLOCKS[nb].base == k) || (water && block_is_water(nb));
        if (f == FACE_U) { if (full) continue; }
        else if (same) continue;
        if (f == FACE_D && BLOCKS[nb].liquid) continue;
        u16 L[4]; light_of(f == FACE_U ? lit(x, y + 1, z) : lit(x + FACE_DIR[f].x, y + FACE_DIR[f].y, z + FACE_DIR[f].z), L);
        for (int i = 0; i < 4; i++) {
            const u8 *c = FACE_CORNERS[f][i];
            f32 px = c[0], py = c[1], pz = c[2];
            f32 tu = CORNER_UV[i][0], tv = CORNER_UV[i][1];
            if (py > 0.5f) {
                int ci = c[0] ? (c[2] ? 2 : 1) : (c[2] ? 3 : 0);
                py = hc[ci];
                if (f != FACE_U) tv = 1 - py;
            }
            emit(layer, x + px, y + py, z + pz, tile, tu, tv, L, 3, f, flags);
        }
    }
    cur_tint = 0xFFFF;
    (void)b;
}

// one axis-aligned box (coordinates in 1/16) with UVs taken from the box position, like Minecraft block models
static void box_quads(int layer, int x, int y, int z, const BlockDef *d, const f32 b[6], bool cull_edges) {
    for (int f = 0; f < 6; f++) {
        bool edge = (f == FACE_W && b[0] <= 0) || (f == FACE_E && b[3] >= 16) || (f == FACE_D && b[1] <= 0) || (f == FACE_U && b[4] >= 16) ||
                    (f == FACE_N && b[2] <= 0) || (f == FACE_S && b[5] >= 16);
        int nx = x + FACE_DIR[f].x, ny = y + FACE_DIR[f].y, nz = z + FACE_DIR[f].z;
        if (edge && cull_edges && occ(nx, ny, nz)) continue;
        u16 L[4]; light_of(edge ? lit(nx, ny, nz) : lit(x, y, z), L);
        if (!edge) { u16 L2[4]; light_of(lit(nx, ny, nz), L2); for (int k = 0; k < 4; k++) L[k] = MAXI(L[k], L2[k]); }
        if (d->emit_r | d->emit_g | d->emit_b) { L[1] = MAXI(L[1], d->emit_r * 17); L[2] = MAXI(L[2], d->emit_g * 17); L[3] = MAXI(L[3], d->emit_b * 17); }
        cur_tint = tint_for(d, x, z, f);
        for (int i = 0; i < 4; i++) {
            const u8 *c = FACE_CORNERS[f][i];
            f32 px = c[0] ? b[3] : b[0], py = c[1] ? b[4] : b[1], pz = c[2] ? b[5] : b[2];
            f32 tu, tv;
            switch (f) {
            case FACE_U: case FACE_D: tu = px; tv = pz; break;
            case FACE_N: tu = 16 - px; tv = 16 - py; break;
            case FACE_S: tu = px; tv = 16 - py; break;
            case FACE_W: tu = pz; tv = 16 - py; break;
            default: tu = 16 - pz; tv = 16 - py; break;
            }
            emit(layer, x + px / 16, y + py / 16, z + pz / 16, d->tex[f], tu / 16, tv / 16, L, 3, f, 0);
        }
    }
    cur_tint = 0xFFFF;
}

static void rot_arm16(const u8 *b, int k, f32 out[6]) {
    f32 x0 = b[0], y0 = b[1], z0 = b[2], x1 = b[3], y1 = b[4], z1 = b[5];
    for (int i = 0; i < k; i++) { f32 nx0 = 16 - z1, nz0 = x0, nx1 = 16 - z0, nz1 = x1; x0 = nx0; z0 = nz0; x1 = nx1; z1 = nz1; }
    out[0] = fminf(x0, x1); out[1] = y0; out[2] = fminf(z0, z1); out[3] = fmaxf(x0, x1); out[4] = y1; out[5] = fmaxf(z0, z1);
}

void mesh_section(Column *col, int s) {
    SectionMesh *m = &col->sec[s];
    m->dirty = false;
    int bx = col->cx * 16, by = s * 16, bz = col->cz * 16;
    bool empty = true;
    for (int y = -1; y <= 16; y++) for (int z = -1; z <= 16; z++) for (int x = -1; x <= 16; x++) {
        int wy = by + y;
        u16 b; u16 l;
        if (x >= 0 && x < 16 && z >= 0 && z < 16 && wy >= 0 && wy < WORLD_H) { b = col->blocks[wy][z][x]; l = col->light[wy][z][x]; }
        else { b = world_get_block_or(bx + x, wy, bz + z, B_AIR); l = world_get_light(bx + x, wy, bz + z); }
        pb[y + 1][z + 1][x + 1] = b; pl[y + 1][z + 1][x + 1] = l;
        if (b && x >= 0 && x < 16 && y >= 0 && y < 16 && z >= 0 && z < 16) empty = false;
    }
    vcount[0] = vcount[1] = 0;
    pad_get_base_x = bx; pad_get_base_y = by; pad_get_base_z = bz;
    if (!empty) {
        gather_tints(col);
        for (int y = 0; y < 16; y++) for (int z = 0; z < 16; z++) for (int x = 0; x < 16; x++) {
            u16 b = pb[y + 1][z + 1][x + 1];
            if (!b) continue;
            const BlockDef *d = &BLOCKS[b];
            int layer = d->layer == LAYER_TRANSLUCENT ? 1 : 0;
            switch (d->mesh) {
            case MESH_CUBE: case MESH_CACTUS: {
                int flags = block_is_leaves(b) ? FL_WAVE_LEAF : 0;
                for (int f = 0; f < 6; f++) {
                    u16 nb = pb[y + 1 + FACE_DIR[f].y][z + 1 + FACE_DIR[f].z][x + 1 + FACE_DIR[f].x];
                    const BlockDef *nd = &BLOCKS[nb];
                    if (d->mesh == MESH_CACTUS) { if ((f == FACE_U || f == FACE_D) && nd->opaque) continue; }
                    else if (nd->opaque || (nb == b && d->layer != LAYER_OPAQUE)) continue;
                    else if (d->layer == LAYER_TRANSLUCENT && nd->layer == LAYER_TRANSLUCENT && nd->mesh == MESH_CUBE && !nd->liquid) continue;
                    cur_tint = tint_for(d, x, z, f);
                    cube_face(layer, x, y, z, f, d->tex[f], flags, 1.0f, d->mesh == MESH_CACTUS ? 1 / 16.0f : 0, d->opaque);
                }
                cur_tint = 0xFFFF;
            } break;
            case MESH_LIQUID: liquid_block(layer, x, y, z, d, b); break;
            case MESH_WIRE: wire_quads(x, y, z, d, lit(x, y, z), bx + x, by + y, bz + z); break;
            case MESH_RAIL: rail_quads(x, y, z, d, lit(x, y, z)); break;
            case 255: {
                u16 above = pb[y + 2][z + 1][x + 1];
                bool water = block_is_water(b);
                f32 top = (BLOCKS[above].liquid && block_is_water(above) == water) ? 1.0f : 0.875f;
                int flags = water ? FL_WATER : FL_LAVA;
                cur_tint = tint_for(d, x, z, FACE_U);
                for (int f = 0; f < 6; f++) {
                    u16 nb = pb[y + 1 + FACE_DIR[f].y][z + 1 + FACE_DIR[f].z][x + 1 + FACE_DIR[f].x];
                    if (BLOCKS[nb].opaque) continue;
                    if (BLOCKS[nb].liquid && block_is_water(nb) == water) continue;
                    if (f != FACE_U && BLOCKS[nb].liquid) continue;
                    if (f == FACE_U && BLOCKS[nb].solid && water && BLOCKS[nb].mesh == MESH_CUBE) continue;
                    cube_face(layer, x, y, z, f, d->tex[f], flags, top, 0, false);
                }
                cur_tint = 0xFFFF;
            } break;
            case MESH_CROSS:
                cur_tint = tint_for(d, x, z, FACE_U);
                cross_quads(layer, x, y, z, d->tex[0], lit(x, y, z));
                cur_tint = 0xFFFF;
                break;
            case MESH_CROP: crop_quads(x, y, z, d->tex[0], lit(x, y, z)); break;
            case MESH_FLAT: cur_tint = tint_for(d, x, z, FACE_U); flat_quad(x, y, z, d, lit(x, y, z)); cur_tint = 0xFFFF; break;
            case MESH_TORCH: torch_quads(x, y, z, d, lit(x, y, z)); break;
            case MESH_BOXES: {
                const BlockShape *sh = &SHAPES[d->shape];
                BlockShape dyn;
                if (d->place == PL_STAIRS) { stair_shape(b, pad_get, bx + x, by + y, bz + z, &dyn); sh = &dyn; }
                for (int i = 0; i < sh->n; i++) {
                    f32 bb[6]; for (int k = 0; k < 6; k++) bb[k] = sh->b[i][k];
                    if (bb[4] <= bb[1]) bb[4] = bb[1] + 0.25f;   // zero-thickness boxes (wings of signs) get a sliver
                    if (bb[3] <= bb[0]) bb[3] = bb[0] + 0.25f;
                    if (bb[5] <= bb[2]) bb[5] = bb[2] + 0.25f;
                    box_quads(layer, x, y, z, d, bb, true);
                }
            } break;
            case MESH_CONNECT: {
                const BlockShape *sh = &SHAPES[d->shape];
                f32 post[6]; for (int k = 0; k < 6; k++) post[k] = sh->b[0][k];
                int mask = block_connect_mask(bx + x, by + y, bz + z, b);
                bool pane = sh->b[0][3] - sh->b[0][0] <= 2;
                bool wall = !pane && sh->b[0][3] - sh->b[0][0] >= 8;
                // straight walls without a post look like Minecraft's
                bool straight = (mask == 5 || mask == 10) && !BLOCKS[pb[y + 2][z + 1][x + 1]].solid;
                if (!(wall && straight)) box_quads(layer, x, y, z, d, post, true);
                for (int k = 0; k < 4; k++) if (mask & (1 << k)) {
                    for (int a = 1; a < sh->n; a++) {
                        f32 arm[6]; rot_arm16(sh->b[a], k, arm);
                        if (pane) { if (k == 0) arm[5] = 9; if (k == 2) arm[2] = 7; if (k == 1) arm[0] = 7; if (k == 3) arm[3] = 9; }
                        box_quads(layer, x, y, z, d, arm, true);
                    }
                }
            } break;
            default: break;
            }
        }
    }
    int nq = (vcount[0] + vcount[1]) / 4;
    m->n_opaque = vcount[0] / 4; m->n_trans = vcount[1] / 4;
    if (nq == 0) { m->built = true; return; }
    if (!m->vao) {
        glGenVertexArrays(1, &m->vao); glGenBuffers(1, &m->vbo);
        glBindVertexArray(m->vao);
        glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
        glVertexAttribPointer(0, 3, GL_UNSIGNED_SHORT, GL_FALSE, sizeof(CVert), (void *)0);
        glVertexAttribPointer(1, 2, GL_UNSIGNED_SHORT, GL_FALSE, sizeof(CVert), (void *)6);
        glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(CVert), (void *)10);
        glVertexAttribPointer(3, 2, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(CVert), (void *)14);
        glVertexAttribPointer(4, 1, GL_UNSIGNED_SHORT, GL_FALSE, sizeof(CVert), (void *)16);
        for (int i = 0; i < 5; i++) glEnableVertexAttribArray(i);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gfx_quad_ibo());
        glBindVertexArray(0);
    }
    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(CVert) * nq * 4, NULL, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(CVert) * vcount[0], vbuf[0]);
    glBufferSubData(GL_ARRAY_BUFFER, sizeof(CVert) * vcount[0], sizeof(CVert) * vcount[1], vbuf[1]);
    m->built = true;
}

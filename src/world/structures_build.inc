// Structure builders (included by structures.c). Every builder is deterministic: it consumes its
// random numbers in the same order whatever column is being generated, and P()/FILL() clip writes
// to that column.

static int h4_of(int dx, int dz) { return dz < 0 ? 0 : dx > 0 ? 1 : dz > 0 ? 2 : 3; }

// ============================================================ villages
typedef struct { u16 wall, log, floor, roof, found, door, fence, path; bool snow; } VMat;
static VMat village_mats(int biome) {
    VMat m = {B_OAK_PLANKS, B_OAK_LOG, B_OAK_PLANKS, B_SPRUCE_PLANKS, B_COBBLESTONE, B_OAK_DOOR, B_OAK_FENCE, B_DIRT_PATH, false};
    if (biome == BIOME_DESERT) { m.wall = B_SMOOTH_SANDSTONE; m.log = B_CUT_SANDSTONE; m.floor = B_SMOOTH_SANDSTONE; m.roof = B_CUT_SANDSTONE; m.found = B_SANDSTONE; m.door = B_ACACIA_DOOR; }
    else if (biome == BIOME_SAVANNA) { m.wall = B_ACACIA_PLANKS; m.log = B_ACACIA_LOG; m.floor = B_ACACIA_PLANKS; m.roof = B_ORANGE_TERRACOTTA; m.door = B_ACACIA_DOOR; m.fence = B_ACACIA_FENCE; }
    else if (biome == BIOME_TAIGA) { m.wall = B_SPRUCE_PLANKS; m.log = B_SPRUCE_LOG; m.floor = B_SPRUCE_PLANKS; m.roof = B_SPRUCE_PLANKS; m.found = B_MOSSY_COBBLESTONE; m.door = B_SPRUCE_DOOR; m.fence = B_SPRUCE_FENCE; }
    else if (biome == BIOME_SNOWY_PLAINS) { m.wall = B_SPRUCE_PLANKS; m.log = B_STRIPPED_SPRUCE_LOG; m.floor = B_SPRUCE_PLANKS; m.roof = B_SNOW_BLOCK; m.door = B_SPRUCE_DOOR; m.fence = B_SPRUCE_FENCE; m.snow = true; }
    return m;
}
static const struct { u16 block; u8 prof; } JOBS[] = {
    {B_BLAST_FURNACE, PROF_ARMORER}, {B_SMOKER, PROF_BUTCHER}, {B_CARTOGRAPHY_TABLE, PROF_CARTOGRAPHER}, {B_BREWING_STAND, PROF_CLERIC},
    {B_BARREL, PROF_FISHERMAN}, {B_FLETCHING_TABLE, PROF_FLETCHER}, {B_CAULDRON, PROF_LEATHERWORKER}, {B_STONECUTTER, PROF_MASON},
    {B_LOOM, PROF_SHEPHERD}, {B_SMITHING_TABLE, PROF_TOOLSMITH}, {B_GRINDSTONE, PROF_WEAPONSMITH}};

// house shell in a frame: W x D footprint, floor at y, H walls; returns nothing
static void house_shell(SB *b, const Frame *f, int W, int D, int y, int H, const VMat *m) {
    clear_above(b, FX(f, 0, 0), FZ(f, 0, 0), FX(f, W - 1, D - 1), FZ(f, W - 1, D - 1), y + 1);
    // foundation + clear
    for (int lz = 0; lz < D; lz++) for (int lx = 0; lx < W; lx++) {
        int x = FX(f, lx, lz), z = FZ(f, lx, lz);
        if (!inside(b, x, z)) continue;
        int g = GROUND(b, x, z);
        for (int yy = MINI(g, y - 1); yy < y; yy++) P(b, x, yy, z, m->found);
        P(b, x, y, z, m->floor);
        for (int yy = y + 1; yy <= y + H + 4; yy++) P(b, x, yy, z, B_AIR);
    }
    for (int lz = 0; lz < D; lz++) for (int lx = 0; lx < W; lx++) {
        bool edge = lx == 0 || lz == 0 || lx == W - 1 || lz == D - 1;
        if (!edge) continue;
        bool corner = (lx == 0 || lx == W - 1) && (lz == 0 || lz == D - 1);
        for (int yy = y + 1; yy <= y + H; yy++) LP(b, f, lx, yy, lz, corner ? m->log : m->wall);
    }
    // windows
    for (int lx = 2; lx < W - 2; lx += 2) LP(b, f, lx, y + 2, D - 1, B_GLASS_PANE);
    for (int lz = 2; lz < D - 1; lz += 2) { LP(b, f, 0, y + 2, lz, B_GLASS_PANE); LP(b, f, W - 1, y + 2, lz, B_GLASS_PANE); }
    // stepped roof
    for (int k = 0; k <= (MINI(W, D) - 1) / 2; k++)
        LFILL(b, f, k, y + H + 1 + k, k, W - 1 - k, y + H + 1 + k, D - 1 - k, m->roof);
    if (m->snow) for (int lz = 0; lz < D; lz++) for (int lx = 0; lx < W; lx++) {
        int k = MINI(MINI(lx, W - 1 - lx), MINI(lz, D - 1 - lz));
        if (k <= (MINI(W, D) - 1) / 2) LP(b, f, lx, y + H + 2 + k, lz, B_SNOW);
    }
    // door
    LP(b, f, W / 2, y + 1, 0, door_state(m->door, f->out, false));
    LP(b, f, W / 2, y + 2, 0, door_state(m->door, f->out, true));
    LP(b, f, W / 2, y + 3, 1, B_LANTERN + 1);   // hanging lantern
}
static void village_house(SB *b, int kind, int dx, int dz, int out, int biome, const VMat *m) {
    int W = kind == 2 ? 9 : kind == 3 ? 9 : kind >= 4 ? 7 : 5, D = kind == 0 ? 5 : 7;
    if (kind == 2) D = 9;
    Frame f = frame(dx, dz, out, W);
    int cx = FX(&f, W / 2, D / 2), cz = FZ(&f, W / 2, D / 2);
    int y = GROUND(b, cx, cz);
    if (y < SEA_LEVEL) y = SEA_LEVEL;
    int job = rnd(b, 0, (int)ARRAY_LEN(JOBS) - 1);
    bool chest = rnd(b, 0, 9) < 4;
    int villagers = rnd(b, 1, 2);
    if (kind == 2) {   // farm: fenced field with water channel, crops and a composter
        for (int lz = 0; lz < D; lz++) for (int lx = 0; lx < W; lx++) {
            int x = FX(&f, lx, lz), z = FZ(&f, lx, lz);
            if (!inside(b, x, z)) continue;
            int g = GROUND(b, x, z);
            for (int yy = MINI(g, y - 1); yy < y; yy++) P(b, x, yy, z, B_DIRT);
            for (int yy = y + 1; yy <= y + 4; yy++) P(b, x, yy, z, B_AIR);
            bool edge = lx == 0 || lz == 0 || lx == W - 1 || lz == D - 1;
            if (edge) { P(b, x, y, z, m->log); continue; }
            if (lx == W / 2) { P(b, x, y, z, B_WATER); continue; }
            P(b, x, y, z, B_FARMLAND + 1);
            int crop = (lz < D / 2) ? B_WHEAT_CROP : (lz % 2 ? B_CARROTS : B_POTATOES);
            int maxv = BLOCKS[crop].nvar - 1;
            P(b, x, y + 1, z, (u16)(crop + rng_int(&b->r, maxv / 2, maxv)));
        }
        LP(b, &f, 0, y + 1, 0, B_COMPOSTER);
        MARK(b, FX(&f, 1, 1), y + 1, FZ(&f, 1, 1), MK_VILLAGER, PROF_FARMER);
        return;
    }
    if (kind == 5) {   // lamp post
        for (int k = 1; k <= 3; k++) LP(b, &f, W / 2, GROUND(b, FX(&f, W / 2, 2), FZ(&f, W / 2, 2)) + k, 2, k < 3 ? m->fence : B_GLOWSTONE);
        return;
    }
    int H = kind == 0 ? 3 : 4;
    house_shell(b, &f, W, D, y, H, m);
    if (kind == 3) {   // library: bookshelves on the back wall, lectern
        for (int lx = 1; lx < W - 1; lx++) for (int yy = y + 1; yy <= y + 3; yy++) LP(b, &f, lx, yy, D - 2, B_BOOKSHELF);
        LP(b, &f, W / 2, y + 1, D - 3, B_LECTERN);
        LP(b, &f, 1, y + 1, 1, B_CRAFTING_TABLE);
        MARK(b, FX(&f, W / 2, 2), y + 1, FZ(&f, W / 2, 2), MK_VILLAGER, PROF_LIBRARIAN);
        if (chest) CHEST(b, FX(&f, W - 2, 1), y + 1, FZ(&f, W - 2, 1), LOOT_VILLAGE);
        return;
    }
    if (kind == 4) {   // smithy: stone, lava basin, smithing tools, the weaponsmith's chest
        for (int lz = 0; lz < D; lz++) for (int lx = 0; lx < W; lx++) if (lx == 0 || lz == 0 || lx == W - 1 || lz == D - 1)
            for (int yy = y + 1; yy <= y + 1; yy++) LP(b, &f, lx, yy, lz, B_COBBLESTONE);
        LP(b, &f, W / 2, y + 1, 0, door_state(m->door, f.out, false));
        LP(b, &f, 1, y, D - 2, B_LAVA); LP(b, &f, 2, y, D - 2, B_LAVA);
        LP(b, &f, 1, y + 1, D - 3, B_IRON_BARS); LP(b, &f, 2, y + 1, D - 3, B_IRON_BARS);
        int which = rnd(b, 0, 2);
        u16 jb = which == 0 ? B_GRINDSTONE : which == 1 ? B_SMITHING_TABLE : B_BLAST_FURNACE;
        u8 pr = which == 0 ? PROF_WEAPONSMITH : which == 1 ? PROF_TOOLSMITH : PROF_ARMORER;
        LP(b, &f, W - 2, y + 1, D - 2, jb);
        LP(b, &f, W - 2, y + 1, 2, B_ANVIL);
        CHEST(b, FX(&f, W - 2, 1), y + 1, FZ(&f, W - 2, 1), LOOT_WEAPONSMITH);
        MARK(b, FX(&f, W / 2, 3), y + 1, FZ(&f, W / 2, 3), MK_VILLAGER, pr);
        return;
    }
    // homes
    LP(b, &f, W / 2, y + 1, D - 2, JOBS[job].block);
    u16 bed = (u16)(B_RED_BED + ((f.out + 2) & 3) * 2);
    LP(b, &f, 1, y + 1, D - 2, (u16)(bed + 1)); LP(b, &f, 1, y + 1, D - 3, bed);
    if (W >= 7) { LP(b, &f, W - 2, y + 1, D - 2, (u16)(bed + 1)); LP(b, &f, W - 2, y + 1, D - 3, bed); LP(b, &f, W - 2, y + 1, 1, B_CRAFTING_TABLE); }
    if (chest && biome == BIOME_TAIGA) BARREL(b, FX(&f, 1, 1), y + 1, FZ(&f, 1, 1), LOOT_VILLAGE);
    else if (chest) CHEST(b, FX(&f, 1, 1), y + 1, FZ(&f, 1, 1), LOOT_VILLAGE);
    LP(b, &f, W / 2, y + 1, 2, B_WHITE_CARPET);
    MARK(b, FX(&f, W / 2, 2), y + 1, FZ(&f, W / 2, 2), MK_VILLAGER, JOBS[job].prof);
    if (villagers > 1 && W >= 7) MARK(b, FX(&f, W / 2 + 1, 3), y + 1, FZ(&f, W / 2 + 1, 3), MK_VILLAGER, rnd(b, 0, 9) == 0 ? PROF_NITWIT : PROF_NONE);
}
static void build_village(SB *b, const Start *s) {
    VMat m = village_mats(s->biome);
    int cx = s->x, cz = s->z;
    int g = MAXI(GROUND(b, cx, cz), SEA_LEVEL);
    int L = 30;
    // roads first (houses overwrite them where they overlap)
    for (int d = 0; d < 4; d++) {
        int Dx = FACE_DIR[H4_FACE[d]].x, Dz = FACE_DIR[H4_FACE[d]].z, Sx = -Dz, Sz = Dx;
        for (int t = 0; t <= L; t++) for (int w = -1; w <= 1; w++) {
            int x = cx + Dx * t + Sx * w, z = cz + Dz * t + Sz * w;
            if (!inside(b, x, z)) continue;
            int gg = GROUND(b, x, z);
            if (gg < SEA_LEVEL) { P(b, x, SEA_LEVEL, z, m.wall); gg = SEA_LEVEL; }
            else P(b, x, gg, z, m.path);
            for (int y = gg + 1; y <= gg + 4; y++) P(b, x, y, z, B_AIR);
        }
    }
    // plaza with a well and the bell
    for (int dz = -4; dz <= 4; dz++) for (int dx = -4; dx <= 4; dx++) {
        int x = cx + dx, z = cz + dz;
        if (!inside(b, x, z)) continue;
        int gg = GROUND(b, x, z);
        for (int y = MINI(gg, g - 1); y < g; y++) P(b, x, y, z, m.found);
        P(b, x, g, z, m.path);
        for (int y = g + 1; y <= g + 5; y++) P(b, x, y, z, B_AIR);
    }
    for (int dz = -1; dz <= 2; dz++) for (int dx = -1; dx <= 2; dx++) {
        bool rim = dx == -1 || dx == 2 || dz == -1 || dz == 2;
        P(b, cx + dx, g + 1, cz + dz, rim ? B_COBBLESTONE : B_AIR);
        if (!rim) for (int y = g - 3; y <= g; y++) P(b, cx + dx, y, cz + dz, B_WATER);
        if ((dx == -1 || dx == 2) && (dz == -1 || dz == 2)) { P(b, cx + dx, g + 2, cz + dz, m.fence); P(b, cx + dx, g + 3, cz + dz, m.fence); }
    }
    FILL(b, cx - 1, g + 4, cz - 1, cx + 2, g + 4, cz + 2, m.roof);
    P(b, cx + 4, g + 1, cz + 4, m.log); P(b, cx + 4, g + 2, cz + 4, B_BELL);
    MOB(b, cx + 3, g + 1, cz - 3, MOB_IRON_GOLEM);
    MOB(b, cx - 3, g + 1, cz + 3, MOB_CAT);
    // buildings along the roads
    for (int d = 0; d < 4; d++) {
        int Dx = FACE_DIR[H4_FACE[d]].x, Dz = FACE_DIR[H4_FACE[d]].z, Sx = -Dz, Sz = Dx;
        for (int t = 8; t <= 24; t += 8) for (int side = 1; side >= -1; side -= 2) {
            int roll = rnd(b, 0, 99);
            int kind = rnd(b, 0, 99);
            kind = kind < 30 ? 0 : kind < 50 ? 1 : kind < 64 ? 2 : kind < 76 ? 3 : kind < 88 ? 4 : 5;
            if (side == 1 && roll >= 85) continue;
            if (side == -1 && (t < 24 || roll >= 60)) continue;
            int doorx = cx + Dx * t + Sx * side * 2, doorz = cz + Dz * t + Sz * side * 2;
            village_house(b, kind, doorx, doorz, h4_of(-Sx * side, -Sz * side), s->biome, &m);
        }
    }
}

// ============================================================ pillager outpost
static void build_outpost(SB *b, const Start *s) {
    int cx = s->x, cz = s->z, g = GROUND(b, cx, cz);
    if (g < SEA_LEVEL) g = SEA_LEVEL;
    int H = 18;
    clear_above(b, cx - 6, cz - 6, cx + 12, cz + 6, g + 1);
    foundation(b, cx - 4, cz - 4, cx + 4, cz + 4, g + 1, B_COBBLESTONE);
    for (int y = g + 1; y <= g + H; y++) for (int dz = -4; dz <= 4; dz++) for (int dx = -4; dx <= 4; dx++) {
        bool edge = abs(dx) == 4 || abs(dz) == 4, corner = abs(dx) == 4 && abs(dz) == 4;
        u16 blk = B_AIR;
        if (corner) blk = B_DARK_OAK_LOG;
        else if (edge) blk = ((y - g) % 6 == 0) ? B_DARK_OAK_PLANKS : ((y - g) % 6 == 3 && (dx == 0 || dz == 0)) ? B_AIR : B_BIRCH_PLANKS;
        else if ((y - g) % 6 == 0) blk = B_DARK_OAK_PLANKS;
        P(b, cx + dx, y, cz + dz, blk);
    }
    for (int y = g + 1; y <= g + H; y++) { P(b, cx, y, cz + 3, B_LADDER + 2); if ((y - g) % 6 == 0) P(b, cx, y, cz + 3, B_LADDER + 2); }
    // top platform with railing
    FILL(b, cx - 5, g + H + 1, cz - 5, cx + 5, g + H + 1, cz + 5, B_DARK_OAK_PLANKS);
    for (int dz = -5; dz <= 5; dz++) for (int dx = -5; dx <= 5; dx++) if (abs(dx) == 5 || abs(dz) == 5) P(b, cx + dx, g + H + 2, cz + dz, B_DARK_OAK_FENCE);
    P(b, cx, g + H + 1, cz + 3, B_LADDER + 2);
    FILL(b, cx - 5, g + H + 6, cz - 5, cx + 5, g + H + 6, cz + 5, B_DARK_OAK_PLANKS);
    for (int dz = -5; dz <= 5; dz += 10) for (int dx = -5; dx <= 5; dx += 10) for (int y = g + H + 2; y <= g + H + 5; y++) P(b, cx + dx, y, cz + dz, B_DARK_OAK_LOG);
    P(b, cx, g + H + 2, cz, B_WHITE_BANNER);
    CHEST(b, cx + 2, g + H + 2, cz - 2, LOOT_OUTPOST);
    for (int i = 0; i < 4; i++) MOB(b, cx + (i % 2 ? 2 : -2), i < 2 ? g + 1 : g + H + 2, cz + (i / 2 ? 2 : -2), MOB_PILLAGER);
    MOB(b, cx, g + 7, cz, MOB_PILLAGER);
    // an iron golem in a cage nearby, and a pillager captain's tent
    int kx = cx + 10, kz = cz + 2, kg = GROUND(b, kx, kz);
    for (int dz = -1; dz <= 1; dz++) for (int dx = -1; dx <= 1; dx++) {
        P(b, kx + dx, kg, kz + dz, B_DARK_OAK_PLANKS);
        for (int y = kg + 1; y <= kg + 3; y++) P(b, kx + dx, y, kz + dz, (dx || dz) ? B_DARK_OAK_FENCE : B_AIR);
        P(b, kx + dx, kg + 4, kz + dz, B_DARK_OAK_SLAB);
    }
    P(b, kx, kg + 3, kz, B_AIR);
    MOB(b, kx, kg + 1, kz, MOB_IRON_GOLEM);
}

// ============================================================ desert pyramid
static void build_desert_pyramid(SB *b, const Start *s) {
    int cx = s->x, cz = s->z, g = GROUND(b, cx, cz);
    clear_above(b, cx - 12, cz - 12, cx + 12, cz + 10, g + 1);
    foundation(b, cx - 10, cz - 10, cx + 10, cz + 10, g, B_SANDSTONE);
    for (int k = 0; k <= 10; k++) {
        int r = 10 - k, y = g + k;
        for (int dz = -r; dz <= r; dz++) for (int dx = -r; dx <= r; dx++) {
            bool edge = abs(dx) == r || abs(dz) == r;
            u16 blk = edge ? ((k % 4 == 2) ? B_CUT_SANDSTONE : B_SANDSTONE) : B_AIR;
            if (k == 0) blk = B_SANDSTONE;
            P(b, cx + dx, y, cz + dz, blk);
        }
    }
    // entrance on the north face
    for (int y = g + 1; y <= g + 3; y++) for (int dx = -1; dx <= 1; dx++) for (int dz = -10; dz <= -7; dz++) P(b, cx + dx, y, cz + dz, B_AIR);
    // decorated floor and the shaft to the treasure room
    for (int dz = -2; dz <= 2; dz++) for (int dx = -2; dx <= 2; dx++) P(b, cx + dx, g, cz + dz, (abs(dx) == 2 || abs(dz) == 2) ? B_ORANGE_TERRACOTTA : B_SANDSTONE);
    P(b, cx, g, cz, B_BLUE_TERRACOTTA);
    for (int y = g - 11; y < g; y++) P(b, cx, y, cz, B_AIR);
    int ry = g - 13;
    SHELL(b, cx - 5, ry - 1, cz - 5, cx + 5, ry + 4, cz + 5, B_SANDSTONE, B_AIR);
    for (int y = ry + 4; y < g; y++) P(b, cx, y, cz, B_AIR);
    for (int dz = -4; dz <= 4; dz++) for (int dx = -4; dx <= 4; dx++) if ((dx + dz) % 2 == 0) P(b, cx + dx, ry - 1, cz + dz, B_ORANGE_TERRACOTTA);
    P(b, cx, ry, cz, B_STONE_PRESSURE_PLATE);
    FILL(b, cx - 1, ry - 3, cz - 1, cx + 1, ry - 2, cz + 1, B_TNT);
    CHEST(b, cx + 4, ry, cz, LOOT_DESERT); CHEST(b, cx - 4, ry, cz, LOOT_DESERT);
    CHEST(b, cx, ry, cz + 4, LOOT_DESERT); CHEST(b, cx, ry, cz - 4, LOOT_DESERT);
    // towers at the front corners
    for (int sx = -1; sx <= 1; sx += 2) for (int y = g + 1; y <= g + 12; y++) for (int dz = -12; dz <= -9; dz++) for (int dx = 0; dx <= 3; dx++) {
        int x = cx + sx * (9 + dx);
        bool edge = dx == 0 || dx == 3 || dz == -12 || dz == -9;
        P(b, x, y, cz + dz, edge ? (y % 3 == 0 ? B_CUT_SANDSTONE : B_SANDSTONE) : B_AIR);
    }
}

// ============================================================ jungle temple
static void build_jungle_temple(SB *b, const Start *s) {
    int cx = s->x, cz = s->z, g = GROUND(b, cx, cz);
    Rng r = b->r;
    #define MOSS() (rng_int(&r, 0, 2) == 0 ? B_MOSSY_COBBLESTONE : B_COBBLESTONE)
    clear_above(b, cx - 6, cz - 7, cx + 6, cz + 7, g);
    foundation(b, cx - 6, cz - 7, cx + 6, cz + 7, g, B_COBBLESTONE);
    for (int y = g; y <= g + 9; y++) for (int dz = -7; dz <= 7; dz++) for (int dx = -6; dx <= 6; dx++) {
        bool edge = abs(dx) == 6 || abs(dz) == 7, floor = y == g || y == g + 4 || y == g + 9;
        bool upper = y > g + 4 && (abs(dx) > 4 || abs(dz) > 5);
        u16 blk = (edge || floor) && !upper ? MOSS() : B_AIR;
        if (upper) blk = B_AIR;
        if (y > g + 4 && (abs(dx) == 4 || abs(dz) == 5) && y < g + 9) blk = MOSS();
        if (y == g + 9 && (abs(dx) > 4 || abs(dz) > 5)) blk = B_AIR;
        P(b, cx + dx, y, cz + dz, blk);
    }
    #undef MOSS
    for (int y = g + 1; y <= g + 3; y++) for (int dx = -1; dx <= 1; dx++) P(b, cx + dx, y, cz - 7, B_AIR);
    for (int y = g + 5; y <= g + 7; y++) P(b, cx, y, cz - 5, B_AIR);
    for (int k = 0; k < 4; k++) P(b, cx + 5, g + 1 + k, cz + 6 - k, B_COBBLESTONE_STAIRS);
    FILL(b, cx - 5, g - 4, cz - 6, cx + 5, g - 1, cz + 6, B_AIR);
    FILL(b, cx - 5, g - 5, cz - 6, cx + 5, g - 5, cz + 6, B_MOSSY_COBBLESTONE);
    for (int k = 0; k < 4; k++) P(b, cx - 5, g - k, cz + 2 + k, B_AIR);
    CHEST(b, cx + 4, g - 4, cz + 5, LOOT_JUNGLE);
    CHEST(b, cx - 4, g + 5, cz + 4, LOOT_JUNGLE);
    P(b, cx - 3, g - 3, cz - 5, B_DISPENSER + 5); MARK(b, cx - 3, g - 3, cz - 5, MK_ARROWS, 0);
    P(b, cx + 3, g - 3, cz + 1, B_DISPENSER + 0); MARK(b, cx + 3, g - 3, cz + 1, MK_ARROWS, 0);
    P(b, cx - 3, g - 4, cz - 2, B_TRIPWIRE_HOOK); P(b, cx - 1, g - 4, cz - 2, B_TRIPWIRE); P(b, cx + 1, g - 4, cz - 2, B_TRIPWIRE);
    P(b, cx + 2, g - 3, cz + 6, B_LEVER + 4);
}

// ============================================================ witch hut
static void build_witch_hut(SB *b, const Start *s) {
    int cx = s->x, cz = s->z;
    int y = MAXI(GROUND(b, cx, cz), SEA_LEVEL) + 3;
    clear_above(b, cx - 3, cz - 4, cx + 3, cz + 3, y - 2);
    for (int dz = -3; dz <= 3; dz += 6) for (int dx = -2; dx <= 2; dx += 4) {
        int gg = GROUND(b, cx + dx, cz + dz);
        for (int yy = gg; yy < y; yy++) P(b, cx + dx, yy, cz + dz, B_SPRUCE_LOG);
    }
    FILL(b, cx - 2, y, cz - 3, cx + 2, y, cz + 3, B_SPRUCE_PLANKS);
    for (int yy = y + 1; yy <= y + 3; yy++) for (int dz = -3; dz <= 2; dz++) for (int dx = -2; dx <= 2; dx++) {
        bool edge = abs(dx) == 2 || dz == -3 || dz == 2;
        P(b, cx + dx, yy, cz + dz, edge ? (yy == y + 2 && (dx == 0 || dz == 0) ? B_OAK_FENCE : B_SPRUCE_PLANKS) : B_AIR);
    }
    P(b, cx, y + 1, cz + 2, B_AIR); P(b, cx, y + 2, cz + 2, B_AIR);
    FILL(b, cx - 3, y + 4, cz - 4, cx + 3, y + 4, cz + 3, B_SPRUCE_SLAB);
    FILL(b, cx - 2, y + 5, cz - 3, cx + 2, y + 5, cz + 2, B_SPRUCE_SLAB);
    P(b, cx - 1, y + 1, cz - 2, B_CAULDRON); P(b, cx + 1, y + 1, cz - 2, B_CRAFTING_TABLE); P(b, cx + 1, y + 1, cz, B_FLOWER_POT);
    CHEST(b, cx - 1, y + 1, cz, LOOT_HUT);
    MOB(b, cx, y + 1, cz - 1, MOB_WITCH);
    MOB(b, cx, y + 1, cz, MOB_CAT);
}

// ============================================================ igloo
static void build_igloo(SB *b, const Start *s) {
    int cx = s->x, cz = s->z, g = GROUND(b, cx, cz);
    clear_above(b, cx - 5, cz - 6, cx + 5, cz + 5, g + 1);
    foundation(b, cx - 4, cz - 4, cx + 4, cz + 4, g + 1, B_SNOW_BLOCK);
    for (int dz = -4; dz <= 4; dz++) for (int dx = -4; dx <= 4; dx++) for (int dy = 0; dy <= 4; dy++) {
        f32 d = sqrtf((f32)(dx * dx + dz * dz) + dy * dy * 1.6f);
        if (d <= 4.4f) P(b, cx + dx, g + 1 + dy, cz + dz, d >= 3.4f ? B_SNOW_BLOCK : B_AIR);
    }
    FILL(b, cx - 3, g + 1, cz - 3, cx + 3, g + 1, cz + 3, B_SNOW_BLOCK);
    FILL(b, cx - 2, g + 2, cz - 2, cx + 2, g + 2, cz + 2, B_WHITE_CARPET);
    for (int y = g + 2; y <= g + 3; y++) for (int dz = -5; dz <= -3; dz++) P(b, cx, y, cz + dz, B_AIR);
    P(b, cx - 2, g + 2, cz, B_FURNACE); P(b, cx + 2, g + 2, cz, B_CRAFTING_TABLE);
    P(b, cx + 1, g + 2, cz + 2, (u16)(B_RED_BED + 1 * 2 + 1)); P(b, cx, g + 2, cz + 2, (u16)(B_RED_BED + 1 * 2));
    P(b, cx - 3, g + 3, cz - 2, B_PACKED_ICE); P(b, cx + 3, g + 3, cz - 2, B_PACKED_ICE);
    if (rnd(b, 0, 1)) {   // basement lab
        int by = g - 9;
        for (int y = by + 4; y <= g + 1; y++) P(b, cx - 1, y, cz - 1, (y == g + 1) ? B_OAK_TRAPDOOR : B_LADDER + 0);
        for (int y = by + 4; y <= g; y++) P(b, cx - 1, y, cz - 2, B_STONE_BRICKS);
        SHELL(b, cx - 4, by - 1, cz - 4, cx + 4, by + 4, cz + 4, B_STONE_BRICKS, B_AIR);
        P(b, cx - 1, by + 3, cz - 1, B_LADDER + 0); P(b, cx - 1, by + 4, cz - 1, B_LADDER + 0);
        P(b, cx + 2, by, cz + 2, B_BREWING_STAND); CHEST(b, cx - 3, by, cz + 3, LOOT_IGLOO);
        P(b, cx + 3, by, cz - 3, B_CAULDRON);
        for (int k = 0; k < 2; k++) {   // two cells with a villager and a zombie villager
            int px = cx - 2 + k * 4, pz = cz + 1;
            for (int y = by; y <= by + 1; y++) { P(b, px - 1, y, pz, B_IRON_BARS); P(b, px + 1, y, pz, B_IRON_BARS); P(b, px, y, pz - 1, B_IRON_BARS); }
            MARK(b, px, by, pz, k ? MK_MOB : MK_VILLAGER, k ? MOB_ZOMBIE_VILLAGER : PROF_CLERIC);
        }
        P(b, cx, by + 3, cz, B_LANTERN + 1);
    }
}

// ============================================================ ocean monument
static void build_monument(SB *b, const Start *s) {
    int cx = s->x, cz = s->z;
    int base = MAXI(GROUND(b, cx, cz) + 1, SEA_LEVEL - 26), top = SEA_LEVEL - 3;
    int R = 28;
    // pillars down to the sea floor, then the hollow body
    for (int dz = -R; dz <= R; dz++) for (int dx = -R; dx <= R; dx++) {
        int x = cx + dx, z = cz + dz;
        if (!inside(b, x, z)) continue;
        int ax = abs(dx), az = abs(dz);
        int height = top - (MAXI(ax, az) > 20 ? 10 : MAXI(ax, az) > 12 ? 5 : 0);
        if (ax == R || az == R || ((ax % 7 == 0) && (az % 7 == 0))) {
            int g = GROUND(b, x, z);
            for (int y = g; y < base; y++) P(b, x, y, z, B_PRISMARINE);
        }
        for (int y = base; y <= height; y++) {
            bool shell = ax == R || az == R || y == base || y == height || (MAXI(ax, az) == 20 && y <= top - 5) || (MAXI(ax, az) == 12);
            bool door = az == R && ax <= 2 && y <= base + 4 && y > base;
            u16 blk = shell && !door ? ((y + ax + az) % 9 == 0 ? B_SEA_LANTERN : (y % 4 == 0 ? B_DARK_PRISMARINE : B_PRISMARINE_BRICKS)) : B_WATER;
            if (MAXI(ax, az) == 12 && (ax % 4 == 2 || az % 4 == 2) && y > base + 1 && y < height - 1 && y % 3 == 0) blk = B_WATER;   // openings into the core
            P(b, x, y, z, blk);
        }
        for (int y = height + 1; y <= SEA_LEVEL; y++) if (block_is_air(GB(b, x, y, z)) || BLOCKS[GB(b, x, y, z)].solid) P(b, x, y, z, B_WATER);
    }
    // treasure: eight gold blocks wrapped in dark prismarine at the heart
    FILL(b, cx - 2, base + 1, cz - 2, cx + 2, base + 4, cz + 2, B_DARK_PRISMARINE);
    FILL(b, cx - 1, base + 2, cz - 1, cx, base + 3, cz, B_GOLD_BLOCK);
    for (int dz = -8; dz <= 8; dz += 8) for (int dx = -8; dx <= 8; dx += 8) if (dx || dz) P(b, cx + dx, base + 1, cz + dz, B_SEA_LANTERN);
    MOB(b, cx, base + 6, cz, MOB_ELDER_GUARDIAN);
    MOB(b, cx - 16, base + 3, cz, MOB_ELDER_GUARDIAN);
    MOB(b, cx + 16, base + 3, cz, MOB_ELDER_GUARDIAN);
    for (int i = 0; i < 8; i++) { int a = i * 45; MOB(b, cx + (int)(cosf(a * PI_F / 180) * 24), base + 2 + i % 3, cz + (int)(sinf(a * PI_F / 180) * 24), MOB_GUARDIAN); }
}

// ============================================================ shipwreck, ocean ruins, buried treasure
static void build_shipwreck(SB *b, const Start *s) {
    int out = rnd(b, 0, 3);
    int g = GROUND(b, s->x, s->z) + 1;
    u16 wood = rnd(b, 0, 1) ? B_OAK_PLANKS : B_SPRUCE_PLANKS, log = wood == B_OAK_PLANKS ? B_OAK_LOG : B_SPRUCE_LOG;
    Frame f = frame(s->x, s->z, out, 7);
    int broken = rnd(b, 0, 20);
    for (int lz = -11; lz <= 11; lz++) {
        int half = abs(lz) > 8 ? 1 : abs(lz) > 5 ? 2 : 3;
        for (int lx = 3 - half; lx <= 3 + half; lx++) for (int dy = 0; dy <= 4; dy++) {
            bool side = lx == 3 - half || lx == 3 + half, bottom = dy == 0;
            bool deck = dy == 3 && abs(lz) < 9;
            if (lz == broken - 10 && dy > 1) continue;   // broken section
            u16 blk = (side || bottom || deck) ? wood : (block_is_water(GB(b, FX(&f, lx, lz), g + dy, FZ(&f, lx, lz))) ? B_WATER : B_WATER);
            if (s->y >= SEA_LEVEL && !(side || bottom || deck)) blk = B_AIR;
            LP(b, &f, lx, g + dy, lz, blk);
        }
    }
    for (int dy = 4; dy <= 9; dy++) { LP(b, &f, 3, g + dy, -4, log); LP(b, &f, 3, g + dy, 3, log); }
    CHEST(b, FX(&f, 3, 9), g + 1, FZ(&f, 3, 9), LOOT_SHIP_SUPPLY);
    CHEST(b, FX(&f, 3, -9), g + 1, FZ(&f, 3, -9), LOOT_SHIP_TREASURE);
    CHEST(b, FX(&f, 2, 0), g + 1, FZ(&f, 2, 0), LOOT_SHIP_MAP);
}
static void build_ocean_ruin(SB *b, const Start *s) {
    int g = GROUND(b, s->x, s->z) + 1;
    bool warm = s->biome == BIOME_WARM_OCEAN || s->biome == BIOME_LUKEWARM_OCEAN || s->biome == BIOME_DEEP_LUKEWARM_OCEAN;
    u16 a = warm ? B_SANDSTONE : B_STONE_BRICKS, c = warm ? B_CUT_SANDSTONE : B_MOSSY_STONE_BRICKS, d = warm ? B_CHISELED_SANDSTONE : B_CRACKED_STONE_BRICKS;
    int R = rnd(b, 3, 5);
    for (int dz = -R; dz <= R; dz++) for (int dx = -R; dx <= R; dx++) {
        int x = s->x + dx, z = s->z + dz;
        bool edge = abs(dx) == R || abs(dz) == R;
        int hgt = edge ? rnd(b, 0, 4) : 0;
        P(b, x, g - 1, z, a);
        for (int y = g; y < g + hgt; y++) { int k = rnd(b, 0, 9); P(b, x, y, z, k < 5 ? a : k < 8 ? c : d); }
    }
    CHEST(b, s->x, g, s->z, LOOT_OCEAN_RUIN);
    MOB(b, s->x + 1, g, s->z + 1, MOB_DROWNED); MOB(b, s->x - 1, g, s->z - 1, MOB_DROWNED);
}
static void build_buried_treasure(SB *b, const Start *s) {
    int g = GROUND(b, s->x, s->z);
    CHEST(b, s->x, g - 3, s->z, LOOT_BURIED);
}

// ============================================================ ruined portals
static void build_ruined_portal(SB *b, const Start *s, bool nether) {
    int g = nether ? 40 : GROUND(b, s->x, s->z);
    if (nether) { for (int y = 32; y < 110; y++) if (!BLOCKS[GB(b, s->x, y, s->z)].solid && BLOCKS[GB(b, s->x, y - 1, s->z)].solid) { g = y - 1; break; } }
    int ax = rnd(b, 0, 1);
    for (int dz = -3; dz <= 3; dz++) for (int dx = -3; dx <= 3; dx++) {
        if (rnd(b, 0, 3) == 0) continue;
        P(b, s->x + dx, g, s->z + dz, rnd(b, 0, 4) == 0 ? B_MAGMA_BLOCK : B_NETHERRACK);
    }
    for (int i = -1; i <= 2; i++) for (int j = 0; j <= 4; j++) {
        bool frame_ = i == -1 || i == 2 || j == 0 || j == 4;
        if (!frame_) continue;
        int k = rnd(b, 0, 9);
        if (k < 2) continue;   // missing pieces
        u16 blk = k < 4 ? B_CRYING_OBSIDIAN : B_OBSIDIAN;
        int x = s->x + (ax ? i : 0), z = s->z + (ax ? 0 : i);
        P(b, x, g + 1 + j, z, blk);
    }
    if (!nether) P(b, s->x + 2, g + 1, s->z + 2, B_GOLD_BLOCK);
    CHEST(b, s->x - 2, g + 1, s->z + 2, LOOT_PORTAL);
}

// ============================================================ mineshaft
static void corridor(SB *b, int x0, int z0, int dx, int dz, int len, int y, int *spawner_left) {
    for (int t = 0; t <= len; t++) {
        int x = x0 + dx * t, z = z0 + dz * t;
        int sx = -dz, sz = dx;
        // decide once per step (consumes random numbers identically everywhere)
        int k = rnd(b, 0, 99), web = rnd(b, 0, 99), cart = rnd(b, 0, 999), torch = rnd(b, 0, 99);
        if (!inside(b, x - 2, z - 2) && !inside(b, x + 2, z + 2) && !inside(b, x - 2, z + 2) && !inside(b, x + 2, z - 2) && !inside(b, x, z)) continue;
        for (int w = -1; w <= 1; w++) for (int h = 0; h <= 2; h++) {
            int xx = x + sx * w, zz = z + sz * w;
            u16 cur = GB(b, xx, y + h, zz);
            if (BLOCKS[cur].liquid) continue;
            P(b, xx, y + h, zz, web < 6 && h == 2 ? B_COBWEB : B_AIR);
        }
        for (int w = -1; w <= 1; w++) { int xx = x + sx * w, zz = z + sz * w; if (!BLOCKS[GB(b, xx, y - 1, zz)].solid) P(b, xx, y - 1, zz, B_OAK_PLANKS); }
        if (k < 70 && t % 4 != 0) P(b, x, y, z, (u16)(B_RAIL + (dx ? 1 : 0)));
        if (t % 4 == 0) {   // support frame
            P(b, x + sx, y, z + sz, B_OAK_FENCE); P(b, x + sx, y + 1, z + sz, B_OAK_FENCE);
            P(b, x - sx, y, z - sz, B_OAK_FENCE); P(b, x - sx, y + 1, z - sz, B_OAK_FENCE);
            for (int w = -1; w <= 1; w++) P(b, x + sx * w, y + 2, z + sz * w, B_OAK_PLANKS);
            if (torch < 15) P(b, x, y + 1, z, B_AIR);
        }
        if (cart < 12 && t % 4 == 2) MARK(b, x, y, z, MK_CART, 0);
        if (*spawner_left > 0 && t == len / 2) {
            (*spawner_left)--;
            SPAWNER(b, x, y, z, MOB_CAVE_SPIDER);
            for (int w = -1; w <= 1; w++) for (int h = 0; h <= 2; h++) for (int q = -1; q <= 1; q++) if (w || h || q) P(b, x + sx * w + dx * q, y + h, z + sz * w + dz * q, B_COBWEB);
        }
    }
}
static void build_mineshaft(SB *b, const Start *s) {
    int y = MCY(-24) + (int)((s->key >> 40) % 56);
    int spawners = 2;
    // a central room, four main tunnels and side branches
    FILL(b, s->x - 4, y, s->z - 4, s->x + 4, y + 3, s->z + 4, B_CAVE_AIR);
    FILL(b, s->x - 4, y - 1, s->z - 4, s->x + 4, y - 1, s->z + 4, B_DIRT);
    for (int d = 0; d < 4; d++) {
        int dx = FACE_DIR[H4_FACE[d]].x, dz = FACE_DIR[H4_FACE[d]].z;
        int len = 30 + rnd(b, 0, 20);
        int yy = y + rnd(b, -2, 2);
        corridor(b, s->x + dx * 5, s->z + dz * 5, dx, dz, len, yy, &spawners);
        for (int br = 0; br < 2; br++) {
            int at = rnd(b, 8, len - 4), side = rnd(b, 0, 1) ? 1 : -1, blen = rnd(b, 10, 24);
            corridor(b, s->x + dx * (5 + at) + (-dz) * side * 2, s->z + dz * (5 + at) + dx * side * 2, -dz * side, dx * side, blen, yy, &spawners);
        }
    }
}

// ============================================================ dungeons (per column, underground)
static void build_dungeon(SB *b) {
    if (rnd(b, 0, 99) >= 12) return;
    int x0 = b->bx + rnd(b, 3, 7), z0 = b->bz + rnd(b, 3, 7);
    int y = rnd(b, MCY(-56), MCY(48));
    int sx = rnd(b, 3, 4), sz = rnd(b, 3, 4);
    int mob = rnd(b, 0, 3);
    mob = mob <= 1 ? MOB_ZOMBIE : mob == 2 ? MOB_SKELETON : MOB_SPIDER;
    // needs to be inside solid rock with some cave opening nearby
    int solid = 0, open = 0;
    for (int dz = -sz; dz <= sz; dz++) for (int dx = -sx; dx <= sx; dx++) {
        u16 fl = GB(b, x0 + dx, y - 1, z0 + dz), ce = GB(b, x0 + dx, y + 4, z0 + dz);
        solid += BLOCKS[fl].solid && BLOCKS[ce].solid;
        u16 mid = GB(b, x0 + dx, y + 1, z0 + dz);
        open += block_is_air(mid);
        if (BLOCKS[mid].liquid) return;
    }
    if (solid < (2 * sx + 1) * (2 * sz + 1) * 3 / 4 || open < 1 || open > 30) return;
    for (int dz = -sz - 1; dz <= sz + 1; dz++) for (int dx = -sx - 1; dx <= sx + 1; dx++) for (int dy = -1; dy <= 4; dy++) {
        bool shell = dy == -1 || dy == 4 || abs(dx) == sx + 1 || abs(dz) == sz + 1;
        u16 cur = GB(b, x0 + dx, y + dy, z0 + dz);
        if (shell) { if (BLOCKS[cur].solid) P(b, x0 + dx, y + dy, z0 + dz, (dy == -1 && rnd(b, 0, 3)) ? B_MOSSY_COBBLESTONE : B_COBBLESTONE); }
        else P(b, x0 + dx, y + dy, z0 + dz, B_CAVE_AIR);
    }
    SPAWNER(b, x0, y, z0, mob);
    CHEST(b, x0 - sx, y, z0, LOOT_DUNGEON);
    if (rnd(b, 0, 1)) CHEST(b, x0, y, z0 + sz, LOOT_DUNGEON);
}

// ============================================================ woodland mansion
static void build_mansion(SB *b, const Start *s) {
    int cx = s->x, cz = s->z, g = GROUND(b, cx, cz);
    int W = 21, D = 15, F = 6;
    clear_above(b, cx - W, cz - D, cx + W, cz + D, g + 1);
    foundation(b, cx - W, cz - D, cx + W, cz + D, g + 1, B_COBBLESTONE);
    for (int fl = 0; fl < 3; fl++) {
        int y = g + 1 + fl * F;
        int w = fl == 2 ? W - 6 : W, d = fl == 2 ? D - 4 : D;
        FILL(b, cx - w, y, cz - d, cx + w, y, cz + d, fl ? B_DARK_OAK_PLANKS : B_COBBLESTONE);
        for (int yy = y + 1; yy < y + F; yy++) for (int dz = -d; dz <= d; dz++) for (int dx = -w; dx <= w; dx++) {
            bool edge = abs(dx) == w || abs(dz) == d, pillar = edge && (dx % 5 == 0 || abs(dz) == d) && (abs(dx) == w || dx % 5 == 0) && (abs(dz) == d || dz % 5 == 0);
            bool inner = (dx % 8 == 0 || dz == 0) && !(abs(dz) % 6 == 3) && !(abs(dx) % 8 == 4);
            u16 blk = B_AIR;
            if (edge) blk = pillar ? B_DARK_OAK_LOG : ((yy - y) == 2 && (dx + dz) % 3 == 0 ? B_GLASS_PANE : B_DARK_OAK_PLANKS);
            else if (inner && (yy - y) < 4) blk = B_BIRCH_PLANKS;
            if (inner && (yy - y) < 3 && (abs(dx) % 8 == 2 || abs(dz) == 2)) blk = B_AIR;   // doorways
            P(b, cx + dx, yy, cz + dz, blk);
        }
    }
    // roof
    for (int k = 0; k < 6; k++) FILL(b, cx - (W - 6) + k, g + 1 + 3 * F + k, cz - (D - 4) + k, cx + (W - 6) - k, g + 1 + 3 * F + k, cz + (D - 4) - k, B_DARK_OAK_PLANKS);
    // entrance + stairs between floors
    for (int yy = g + 2; yy <= g + 4; yy++) for (int dx = -1; dx <= 1; dx++) P(b, cx + dx, yy, cz - D, B_AIR);
    for (int fl = 0; fl < 2; fl++) for (int k = 0; k < F; k++) { P(b, cx + 3 + k, g + 2 + fl * F + k, cz + 2, B_DARK_OAK_PLANKS); for (int h = 1; h <= 3; h++) P(b, cx + 3 + k, g + 2 + fl * F + k + h, cz + 2, B_AIR); }
    // rooms: loot, a few beds, candles
    CHEST(b, cx - W + 2, g + 2, cz - D + 2, LOOT_MANSION);
    CHEST(b, cx + W - 2, g + 2 + F, cz + D - 2, LOOT_MANSION);
    CHEST(b, cx, g + 2 + 2 * F, cz + 4, LOOT_MANSION);
    P(b, cx - 6, g + 2 + F, cz - 6, B_RED_CARPET); P(b, cx + 6, g + 2 + F, cz + 6, B_RED_CARPET);
    for (int i = 0; i < 5; i++) MOB(b, cx - 12 + i * 6, g + 2 + (i % 3) * F, cz + (i % 2 ? 5 : -5), MOB_VINDICATOR);
    MOB(b, cx - 5, g + 2 + F, cz + 5, MOB_EVOKER); MOB(b, cx + 5, g + 2 + 2 * F, cz - 4, MOB_EVOKER);
    MOB(b, cx, g + 2 + 2 * F, cz, MOB_ALLAY);
}

// ============================================================ ancient city (deep dark)
static void build_ancient_city(SB *b, const Start *s) {
    int cx = s->x, cz = s->z, y = MCY(-51);
    int R = 24;
    for (int dz = -R; dz <= R; dz++) for (int dx = -R; dx <= R; dx++) {
        int x = cx + dx, z = cz + dz;
        if (!inside(b, x, z)) continue;
        bool plaza = abs(dx) <= 14 && abs(dz) <= 14;
        int k = (int)(hash3i(b->seed, x, 77, z) % 100);
        P(b, x, y - 1, z, k < 15 ? B_SCULK : (k < 55 ? B_DEEPSLATE_BRICKS : B_DEEPSLATE_TILES));
        for (int h = 0; h < (plaza ? 14 : 8); h++) P(b, x, y + h, z, B_CAVE_AIR);
        if (!plaza && (abs(dx) % 6 == 0) && (abs(dz) % 6 == 0)) for (int h = 0; h < 5; h++) P(b, x, y + h, z, h == 4 ? B_SOUL_LANTERN : B_POLISHED_DEEPSLATE);
        if (k >= 96) P(b, x, y, z, k == 99 ? B_SCULK_SHRIEKER + 1 : B_SCULK_SENSOR);
    }
    // the great "portal" frame of reinforced deepslate
    for (int dx = -7; dx <= 7; dx++) for (int h = 0; h <= 10; h++) {
        bool frame_ = abs(dx) >= 6 || h >= 9 || (h == 0);
        if (frame_) P(b, cx + dx, y + h, cz, B_REINFORCED_DEEPSLATE);
    }
    for (int dx = -4; dx <= 4; dx++) P(b, cx + dx, y, cz + 2, B_DEEPSLATE_TILE_STAIRS);
    CHEST(b, cx - 10, y, cz - 10, LOOT_ANCIENT); CHEST(b, cx + 10, y, cz - 10, LOOT_ANCIENT);
    CHEST(b, cx - 10, y, cz + 10, LOOT_ANCIENT); CHEST(b, cx + 18, y, cz + 18, LOOT_ANCIENT);
    for (int i = 0; i < 6; i++) P(b, cx - 12 + i * 5, y, cz - 13, B_CANDLE);
}

// ============================================================ trial chambers
static void build_trial_chambers(SB *b, const Start *s) {
    int cx = s->x, cz = s->z, y = MCY(-40) + (int)((s->key >> 44) % 21);
    SHELL(b, cx - 10, y - 1, cz - 10, cx + 10, y + 8, cz + 10, B_TUFF_BRICKS, B_AIR);
    for (int dz = -9; dz <= 9; dz++) for (int dx = -9; dx <= 9; dx++) if ((abs(dx) + abs(dz)) % 4 == 0) P(b, cx + dx, y - 1, cz + dz, B_WAXED_COPPER_BLOCK);
    for (int dz = -8; dz <= 8; dz += 8) for (int dx = -8; dx <= 8; dx += 16) for (int h = 0; h < 8; h++) P(b, cx + dx, y + h, cz + dz, h == 7 ? B_COPPER_BULB : B_CHISELED_TUFF);
    static const int MOBS[4] = {0, 0, 0, 0};
    (void)MOBS;
    int kinds[4] = {MOB_BREEZE, MOB_ZOMBIE, MOB_SKELETON, MOB_SPIDER};
    for (int i = 0; i < 3; i++) { int k = rnd(b, 0, 3); P(b, cx - 5 + i * 5, y, cz, B_TRIAL_SPAWNER); MARK(b, cx - 5 + i * 5, y, cz, MK_SPAWNER, kinds[k]); }
    CHEST(b, cx, y, cz + 8, LOOT_TRIAL); CHEST(b, cx, y, cz - 8, LOOT_TRIAL);
    // a corridor up toward the caves
    for (int t = 0; t < 14; t++) for (int w = -1; w <= 1; w++) for (int h = 0; h < 3; h++) P(b, cx + 11 + t, y + h + t / 2, cz + w, B_AIR);
}

// ============================================================ stronghold: library, store rooms, the silverfish spawner
static void build_stronghold_extras(SB *b) {
    ivec3 sh = worldgen_stronghold(b->seed);
    if (abs(sh.x - b->bx - 8) > 48 || abs(sh.z - b->bz - 8) > 48) return;
    MARK(b, sh.x, sh.y, sh.z - 5, MK_SPAWNER, MOB_SILVERFISH);
    // corridors from the portal room
    for (int t = 6; t <= 30; t++) for (int w = -1; w <= 1; w++) for (int h = 0; h < 4; h++) {
        bool shell = abs(w) == 1 && h < 3 ? false : false;
        (void)shell;
        u16 wall = (t + h) % 5 == 0 ? B_MOSSY_STONE_BRICKS : B_STONE_BRICKS;
        P(b, sh.x + t, sh.y - 1, sh.z + w, wall);
        P(b, sh.x + t, sh.y + 3, sh.z + w, wall);
        P(b, sh.x + t, sh.y + h, sh.z + 2, wall); P(b, sh.x + t, sh.y + h, sh.z - 2, wall);
        if (h < 3) P(b, sh.x + t, sh.y + h, sh.z + w, B_CAVE_AIR);
    }
    for (int h = 0; h < 3; h++) for (int w = -1; w <= 1; w++) P(b, sh.x + 5, sh.y + h, sh.z + w, B_AIR);
    // library at the end of the corridor
    int lx = sh.x + 40, lz = sh.z;
    SHELL(b, lx - 9, sh.y - 1, lz - 7, lx + 9, sh.y + 7, lz + 7, B_STONE_BRICKS, B_AIR);
    for (int h = 0; h < 3; h++) for (int w = -1; w <= 1; w++) P(b, lx - 9, sh.y + h, lz + w, B_AIR);
    for (int dz = -6; dz <= 6; dz += 4) for (int dx = -7; dx <= 7; dx++) for (int h = 0; h < 4; h++) if (dz != -2 && dz != 2) P(b, lx + dx, sh.y + h, lz + dz, B_BOOKSHELF);
    for (int dx = -8; dx <= 8; dx += 4) for (int h = 0; h < 6; h++) P(b, lx + dx, sh.y + h, lz - 6, B_OAK_LOG);
    CHEST(b, lx + 7, sh.y, lz + 3, LOOT_STRONGHOLD); CHEST(b, lx - 6, sh.y, lz - 3, LOOT_STRONGHOLD);
    P(b, lx, sh.y + 6, lz, B_LANTERN + 1);
    // store room the other way
    int sx = sh.x - 16;
    SHELL(b, sx - 5, sh.y - 1, sh.z - 4, sx + 5, sh.y + 4, sh.z + 4, B_STONE_BRICKS, B_AIR);
    for (int t = sx + 5; t <= sh.x - 5; t++) for (int w = -1; w <= 1; w++) for (int h = 0; h < 3; h++) P(b, t, sh.y + h, sh.z + w, B_AIR);
    CHEST(b, sx, sh.y, sh.z + 3, LOOT_STRONGHOLD);
    P(b, sx - 3, sh.y, sh.z - 3, B_CRAFTING_TABLE); P(b, sx + 3, sh.y, sh.z - 3, B_FURNACE);
}

// ============================================================ nether fortress
static void build_fortress(SB *b, const Start *s) {
    int cx = s->x, cz = s->z, y = 64;
    int L = 52;
    // two crossing bridges
    for (int axis = 0; axis < 2; axis++) for (int t = -L; t <= L; t++) for (int w = -2; w <= 2; w++) {
        int x = axis ? cx + t : cx + w, z = axis ? cz + w : cz + t;
        if (!inside(b, x, z)) continue;
        P(b, x, y, z, B_NETHER_BRICKS);
        for (int h = 1; h <= 4; h++) P(b, x, y + h, z, (abs(w) == 2 && h == 1) ? B_NETHER_BRICK_FENCE : B_AIR);
        if (abs(t) % 9 == 0 && abs(w) <= 1) {   // pillars down to the ground / lava
            for (int yy = y - 1; yy > 4; yy--) { u16 c = GB(b, x, yy, z); if (BLOCKS[c].solid) break; P(b, x, yy, z, B_NETHER_BRICKS); }
        }
        if (abs(t) % 9 == 4 && abs(w) == 2) { P(b, x, y + 1, z, B_NETHER_BRICKS); P(b, x, y + 2, z, B_NETHER_BRICK_FENCE); }
    }
    // central hall with a nether wart garden and stairs
    SHELL(b, cx - 8, y, cz - 8, cx + 8, y + 9, cz + 8, B_NETHER_BRICKS, B_AIR);
    for (int d = 0; d < 4; d++) { int dx = FACE_DIR[H4_FACE[d]].x, dz = FACE_DIR[H4_FACE[d]].z; for (int w = -2; w <= 2; w++) for (int h = 1; h <= 4; h++) P(b, cx + dx * 8 + (dz ? w : 0), y + h, cz + dz * 8 + (dx ? w : 0), B_AIR); }
    for (int dz = -6; dz <= -3; dz++) for (int dx = -6; dx <= 6; dx++) { P(b, cx + dx, y, cz + dz, B_SOUL_SAND); P(b, cx + dx, y + 1, cz + dz, (u16)(B_NETHER_WART_CROP + rng_int(&b->r, 0, 3))); }
    for (int k = 0; k < 6; k++) P(b, cx + 6, y + 1 + k, cz + 2 + k, B_NETHER_BRICK_STAIRS);
    for (int dx = -7; dx <= 7; dx += 7) for (int dz = -7; dz <= 7; dz += 14) for (int h = 1; h < 9; h++) P(b, cx + dx, y + h, cz + dz, B_NETHER_BRICK_FENCE);
    CHEST(b, cx - 6, y + 1, cz + 6, LOOT_FORTRESS);
    CHEST(b, cx + L - 2, y + 1, cz + 1, LOOT_FORTRESS);
    // blaze spawner balconies at two bridge ends
    for (int k = 0; k < 2; k++) {
        int bx = k ? cx - L + 3 : cx, bz = k ? cz : cz + L - 3;
        FILL(b, bx - 3, y, bz - 3, bx + 3, y, bz + 3, B_NETHER_BRICKS);
        for (int dz = -3; dz <= 3; dz++) for (int dx = -3; dx <= 3; dx++) if (abs(dx) == 3 || abs(dz) == 3) P(b, bx + dx, y + 1, bz + dz, B_NETHER_BRICK_FENCE);
        SPAWNER(b, bx, y + 1, bz, MOB_BLAZE);
    }
    MOB(b, cx + 20, y + 1, cz, MOB_WITHER_SKELETON); MOB(b, cx, y + 1, cz - 20, MOB_WITHER_SKELETON); MOB(b, cx - 3, y + 1, cz + 3, MOB_WITHER_SKELETON);
    MOB(b, cx - 25, y + 1, cz, MOB_BLAZE);
}

// ============================================================ bastion remnant
static void build_bastion(SB *b, const Start *s) {
    int cx = s->x, cz = s->z, y = 38;
    int R = 14;
    for (int dz = -R; dz <= R; dz++) for (int dx = -R; dx <= R; dx++) {
        int x = cx + dx, z = cz + dz;
        if (!inside(b, x, z)) continue;
        for (int yy = 32; yy < y; yy++) P(b, x, yy, z, B_BLACKSTONE);
        bool wall = abs(dx) == R || abs(dz) == R;
        for (int h = 0; h <= 16; h++) {
            u16 blk = B_AIR;
            if (wall) blk = (h + dx + dz) % 7 == 0 ? B_GILDED_BLACKSTONE : (h % 5 == 0 ? B_POLISHED_BLACKSTONE_BRICKS : B_BLACKSTONE);
            if (wall && h > 12 && (dx + dz) % 3) blk = B_AIR;   // ruined top
            if (h == 0) blk = (abs(dx) + abs(dz)) % 6 == 0 ? B_GILDED_BLACKSTONE : B_POLISHED_BLACKSTONE_BRICKS;
            if (h == 6 && abs(dx) < R && abs(dz) < R && (abs(dx) > 6 || abs(dz) > 6)) blk = B_POLISHED_BLACKSTONE_BRICKS;   // gallery
            P(b, x, y + h, z, blk);
        }
    }
    for (int h = 1; h <= 4; h++) for (int w = -2; w <= 2; w++) P(b, cx + w, y + h, cz - R, B_AIR);
    // treasure heap
    for (int dz = -2; dz <= 2; dz++) for (int dx = -2; dx <= 2; dx++) if (abs(dx) + abs(dz) <= 3) P(b, cx + dx, y + 1, cz + dz, (abs(dx) + abs(dz)) <= 1 ? B_GOLD_BLOCK : B_GILDED_BLACKSTONE);
    CHEST(b, cx, y + 2, cz, LOOT_BASTION); CHEST(b, cx - 10, y + 7, cz + 10, LOOT_BASTION); CHEST(b, cx + 10, y + 1, cz - 10, LOOT_BASTION);
    SPAWNER(b, cx + 9, y + 1, cz + 9, MOB_MAGMA_CUBE);
    for (int i = 0; i < 3; i++) MOB(b, cx - 6 + i * 6, y + 1, cz - 6, MOB_PIGLIN_BRUTE);
    for (int i = 0; i < 4; i++) MOB(b, cx - 8 + i * 5, y + 7, cz + 9, MOB_PIGLIN);
    MOB(b, cx + 8, y + 1, cz + 4, MOB_HOGLIN); MOB(b, cx + 10, y + 1, cz + 4, MOB_HOGLIN);
}

// ============================================================ end city
static void build_end_city(SB *b, const Start *s) {
    int cx = s->x, cz = s->z, y = s->y;
    int floors = 3 + (int)((s->key >> 50) % 3);
    for (int f = 0; f < floors; f++) {
        int r = f == 0 ? 6 : 5 - (f % 2), yy = y + f * 6;
        for (int dz = -r; dz <= r; dz++) for (int dx = -r; dx <= r; dx++) for (int h = 0; h <= 5; h++) {
            bool edge = abs(dx) == r || abs(dz) == r, floor_ = h == 0 || h == 5;
            u16 blk = B_AIR;
            if (edge || floor_) blk = (abs(dx) == r && abs(dz) == r) ? B_PURPUR_PILLAR : B_PURPUR_BLOCK;
            if (edge && h == 2 && (dx == 0 || dz == 0)) blk = B_MAGENTA_STAINED_GLASS;
            if (floor_ && h == 5 && abs(dx) <= 1 && abs(dz) <= 1 && f < floors - 1) blk = B_AIR;   // hatch to the next floor
            P(b, cx + dx, yy + h, cz + dz, blk);
        }
        for (int h = 1; h <= 5; h++) P(b, cx, yy + h, cz + r - 1, B_LADDER + 0);
        P(b, cx - r + 1, yy + 3, cz - r + 1, B_END_ROD); P(b, cx + r - 1, yy + 3, cz + r - 1, B_END_ROD);
        MOB(b, cx + r - 2, yy + 1, cz - r + 2, MOB_SHULKER);
        if (f % 2 == 1) CHEST(b, cx - r + 1, yy + 1, cz, LOOT_END_CITY);
    }
    for (int h = 1; h <= 2; h++) for (int w = -1; w <= 1; w++) P(b, cx + w, y + h, cz - 6, B_AIR);
    FILL(b, cx - 7, y - 1, cz - 7, cx + 7, y - 1, cz + 7, B_END_STONE_BRICKS);
    // the end ship floating beside the city, with an elytra
    int sx = cx + 14, sy = y + floors * 6 - 2, sz = cz;
    for (int t = -8; t <= 8; t++) {
        int half = abs(t) > 6 ? 1 : 2;
        for (int w = -half; w <= half; w++) {
            P(b, sx + w, sy, sz + t, B_PURPUR_BLOCK);
            if (abs(w) == half) P(b, sx + w, sy + 1, sz + t, B_PURPUR_BLOCK);
        }
    }
    for (int h = 1; h <= 8; h++) P(b, sx, sy + h, sz, B_OBSIDIAN);
    CHEST(b, sx, sy + 1, sz + 5, LOOT_END_SHIP);
    CHEST(b, sx, sy + 1, sz - 5, LOOT_END_CITY);
    P(b, sx, sy + 1, sz + 7, B_DRAGON_HEAD);
    MOB(b, sx, sy + 1, sz - 2, MOB_SHULKER); MOB(b, sx + 1, sy + 1, sz + 2, MOB_SHULKER);
}

// ============================================================ the Underground City (the Delvari Underhold)
// A walled city in a vast cavern around y -48 (floor) .. -18 (roof), about 120 blocks across.  It is built from
// modular pieces on a 7x7 grid of 16-block cells: every cell is a 12x12 lot ringed by 4-wide streets.
//   * the 3x3 centre cells hold the Central Keep (walls, gates, courtyard, throne hall with Morrowgate, a vault)
//   * north of the keep: the Market (stalls + Delvari Merchants)      * south: the Temple and two shrines
//   * east: the Prison and guard barracks (Hollow Sentinels)            * west: the residential row
//   * the outer ring: houses, plazas with fountains, moss gardens and collapsed ruins (chosen per lot)
// Each piece only writes the parts that overlap the column being generated (P/FILL clip), and every random
// choice is a hash of the lot, so all columns agree.
typedef struct { int x0, z0, y; int door; } Lot;   // door: 0 north (-z), 1 east, 2 south, 3 west
static int lot_rnd(SB *b, const Start *s, int i, int j, int salt, int n) { return (int)(hash3i(s->key ^ (u64)salt, i, 9013, j) % (u64)n); }
static bool lot_hits(SB *b, int x0, int z0, int x1, int z1) { return !(x1 < b->bx || x0 > b->bx + 15 || z1 < b->bz || z0 > b->bz + 15); }
static void lot_door(SB *b, const Lot *l, int x0, int z0, int x1, int z1, int y, int h) {   // 2-wide opening on the door side
    int mx = (x0 + x1) / 2, mz = (z0 + z1) / 2;
    for (int k = 0; k < 2; k++) for (int dy = 0; dy < h; dy++) {
        if (l->door == 0) P(b, mx + k, y + dy, z0, B_CAVE_AIR);
        else if (l->door == 2) P(b, mx + k, y + dy, z1, B_CAVE_AIR);
        else if (l->door == 1) P(b, x1, y + dy, mz + k, B_CAVE_AIR);
        else P(b, x0, y + dy, mz + k, B_CAVE_AIR);
    }
}
static void delvari_box(SB *b, int x0, int z0, int x1, int z1, int y, int H, u16 wall) {   // walls, floor, flat roof, pillars
    FILL(b, x0, y - 1, z0, x1, y - 1, z1, B_DELVARI_TILES);
    SHELL(b, x0, y - 1, z0, x1, y + H, z1, wall, B_CAVE_AIR);
    FILL(b, x0, y - 1, z0, x1, y - 1, z1, B_DELVARI_TILES);
    for (int h = 0; h <= H; h++) { P(b, x0, y + h, z0, B_DELVARI_PILLAR); P(b, x1, y + h, z0, B_DELVARI_PILLAR); P(b, x0, y + h, z1, B_DELVARI_PILLAR); P(b, x1, y + h, z1, B_DELVARI_PILLAR); }
    for (int x = x0 + 2; x < x1 - 1; x += 3) { P(b, x, y + 2, z0, B_IRON_BARS); P(b, x, y + 2, z1, B_IRON_BARS); }   // windows
    for (int z = z0 + 2; z < z1 - 1; z += 3) { P(b, x0, y + 2, z, B_IRON_BARS); P(b, x1, y + 2, z, B_IRON_BARS); }
}
static void city_house(SB *b, const Start *s, const Lot *l, int i, int j, bool ruined) {
    int w = 8 + lot_rnd(b, s, i, j, 1, 3), d = 8 + lot_rnd(b, s, i, j, 2, 3), H = 4 + lot_rnd(b, s, i, j, 3, 2);
    int x0 = l->x0 + (12 - w) / 2, z0 = l->z0 + (12 - d) / 2, x1 = x0 + w - 1, z1 = z0 + d - 1, y = l->y;
    delvari_box(b, x0, z0, x1, z1, y, H, B_DELVARI_BRICKS);
    lot_door(b, l, x0, z0, x1, z1, y, 3);
    P(b, (x0 + x1) / 2, y + H - 1, (z0 + z1) / 2, B_UMBRAL_LAMP);
    {   // a lamp in the wall beside the door
        int mx = (x0 + x1) / 2, mz = (z0 + z1) / 2;
        if (l->door == 0) P(b, mx - 1, y + 2, z0, B_UMBRAL_LAMP); else if (l->door == 2) P(b, mx - 1, y + 2, z1, B_UMBRAL_LAMP);
        else if (l->door == 1) P(b, x1, y + 2, mz - 1, B_UMBRAL_LAMP); else P(b, x0, y + 2, mz - 1, B_UMBRAL_LAMP);
    }
    if (lot_rnd(b, s, i, j, 4, 2) == 0) CHEST(b, x0 + 1, y, z0 + 1, LOOT_DELVARI);
    BARREL(b, x1 - 1, y, z1 - 1, LOOT_DELVARI);
    if (ruined) {   // collapsed: holes in roof and walls, rubble and cobwebs
        for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++) {
            u64 h = hash3i(s->key, x, 31, z);
            if (h % 3 == 0) for (int dy = H; dy >= H - (int)(h % 4); dy--) P(b, x, y + dy, z, B_CAVE_AIR);
            if (h % 11 == 0 && x > x0 && x < x1 && z > z0 && z < z1) P(b, x, y, z, (h >> 8) % 2 ? B_COBWEB : B_COBBLED_DEEPSLATE);
        }
    }
}
static void city_plaza(SB *b, const Start *s, const Lot *l) {
    int cx = l->x0 + 5, cz = l->z0 + 5, y = l->y;
    for (int dz = -2; dz <= 3; dz++) for (int dx = -2; dx <= 3; dx++) {
        bool rim = dx == -2 || dx == 3 || dz == -2 || dz == 3;
        P(b, cx + dx, y - 1, cz + dz, rim ? B_DELVARI_TILES : B_DELVARI_BRICKS);
        if (rim) P(b, cx + dx, y, cz + dz, B_DELVARI_BRICKS); else P(b, cx + dx, y, cz + dz, B_WATER);
    }
    for (int h = 0; h < 4; h++) P(b, cx, y + h, cz, h == 3 ? B_UMBRAL_LAMP : B_DELVARI_PILLAR);
}
static void city_garden(SB *b, const Start *s, const Lot *l, int i, int j) {
    for (int z = l->z0; z < l->z0 + 12; z++) for (int x = l->x0; x < l->x0 + 12; x++) {
        bool rim = x == l->x0 || z == l->z0 || x == l->x0 + 11 || z == l->z0 + 11;
        P(b, x, l->y - 1, z, rim ? B_DELVARI_TILES : B_MOSS_BLOCK);
        if (rim) continue;
        u64 h = hash3i(s->key, x, 57, z);
        if (h % 5 == 0) P(b, x, l->y, z, (h >> 7) % 2 ? B_RED_MUSHROOM : B_BROWN_MUSHROOM);
    }
    for (int h = 0; h < 3; h++) P(b, l->x0 + 6, l->y + h, l->z0 + 6, h == 2 ? B_UMBRAL_LAMP : B_DELVARI_PILLAR);
}
static void city_market(SB *b, const Start *s, const Lot *l, int i, int j) {
    static const u16 CANOPY[4] = {B_RED_WOOL, B_BLUE_WOOL, B_YELLOW_WOOL, B_CYAN_WOOL};
    for (int z = l->z0; z < l->z0 + 12; z++) for (int x = l->x0; x < l->x0 + 12; x++) P(b, x, l->y - 1, z, B_DELVARI_TILES);
    for (int q = 0; q < 4; q++) {   // four stalls in the corners
        int sx = l->x0 + (q & 1 ? 8 : 1), sz = l->z0 + (q & 2 ? 8 : 1);
        for (int dz = 0; dz < 3; dz++) for (int dx = 0; dx < 3; dx++) {
            bool post = (dx == 0 || dx == 2) && (dz == 0 || dz == 2);
            if (post) for (int h = 0; h < 3; h++) P(b, sx + dx, l->y + h, sz + dz, B_DELVARI_PILLAR);
            P(b, sx + dx, l->y + 3, sz + dz, CANOPY[(q + lot_rnd(b, s, i, j, 5, 4)) & 3]);
        }
        BARREL(b, sx + 1, l->y, sz + 1, LOOT_DELVARI);
    }
    for (int h = 0; h < 4; h++) P(b, l->x0 + 6, l->y + h, l->z0 + 6, h == 3 ? B_UMBRAL_LAMP : B_DELVARI_PILLAR);
    MOB(b, l->x0 + 5, l->y, l->z0 + 4, MOB_DELVARI_MERCHANT);
    MOB(b, l->x0 + 7, l->y, l->z0 + 8, MOB_DELVARI_MERCHANT);
}
static void city_temple(SB *b, const Start *s, const Lot *l) {
    int x0 = l->x0, z0 = l->z0, x1 = x0 + 11, z1 = z0 + 11, y = l->y;
    delvari_box(b, x0, z0, x1, z1, y, 9, B_RUNIC_DELVARI_BRICKS);
    lot_door(b, l, x0, z0, x1, z1, y, 4);
    for (int k = 0; k < 4; k++) for (int h = 0; h < 9; h++) P(b, x0 + 3 + (k & 1) * 5, y + h, z0 + 3 + (k >> 1) * 5, B_DELVARI_PILLAR);
    FILL(b, x0 + 4, y, z1 - 2, x1 - 4, y, z1 - 2, B_RUNIC_DELVARI_BRICKS);   // altar
    CHEST(b, x0 + 5, y + 1, z1 - 2, LOOT_DELVARI_TEMPLE);
    P(b, x0 + 6, y + 1, z1 - 2, B_CANDLE);
    for (int k = 0; k < 4; k++) P(b, x0 + 3 + (k & 1) * 5, y + 8, z0 + 3 + (k >> 1) * 5, B_UMBRAL_LAMP);
}
static void city_prison(SB *b, const Start *s, const Lot *l, bool barracks) {
    int x0 = l->x0, z0 = l->z0, x1 = x0 + 11, z1 = z0 + 11, y = l->y;
    delvari_box(b, x0, z0, x1, z1, y, 5, B_DELVARI_BRICKS);
    lot_door(b, l, x0, z0, x1, z1, y, 3);
    if (!barracks) {   // three cells along the back wall behind iron bars, a corridor in front
        for (int c = 0; c < 3; c++) {
            int cx0 = x0 + 1 + c * 3;
            for (int h = 0; h < 4; h++) { P(b, cx0 + 3, y + h, z1 - 1, B_DELVARI_BRICKS); P(b, cx0 + 3, y + h, z1 - 2, B_DELVARI_BRICKS); P(b, cx0 + 3, y + h, z1 - 3, B_DELVARI_BRICKS); }
            for (int dx = 0; dx < 3; dx++) for (int h = 0; h < 4; h++) P(b, cx0 + dx, y + h, z1 - 4, B_IRON_BARS);
            P(b, cx0 + 1, y, z1 - 2, B_COBWEB);
        }
        CHEST(b, x0 + 1, y, z0 + 1, LOOT_DELVARI);
    }
    P(b, x0 + 6, y + 4, z0 + 4, B_UMBRAL_LAMP);
    MOB(b, x0 + 4, y, z0 + 4, MOB_HOLLOW_SENTINEL);
    MOB(b, x0 + 7, y, z0 + 5, MOB_HOLLOW_SENTINEL);
}
static void city_keep(SB *b, const Start *s, int y) {
    int cx = s->x, cz = s->z;
    // curtain wall with four gates and a runic band
    for (int dz = -22; dz <= 21; dz++) for (int dx = -22; dx <= 21; dx++) {
        int x = cx + dx, z = cz + dz;
        if (!inside(b, x, z)) continue;
        bool wall = dx <= -21 || dx >= 20 || dz <= -21 || dz >= 20;
        P(b, x, y - 1, z, B_DELVARI_TILES);
        for (int h = 0; h < 15; h++) P(b, x, y + h, z, wall ? (h == 10 ? B_RUNIC_DELVARI_BRICKS : B_DELVARI_BRICKS) : B_CAVE_AIR);
        if (wall && ((dx + dz) & 1)) P(b, x, y + 15, z, B_DELVARI_BRICKS);   // crenellations
        bool gate = (abs(dx) <= 2 && (dz <= -21 || dz >= 20)) || (abs(dz) <= 2 && (dx <= -21 || dx >= 20));
        if (gate) for (int h = 0; h < 6; h++) P(b, x, y + h, z, B_CAVE_AIR);
    }
    // the throne hall
    delvari_box(b, cx - 10, cz - 10, cx + 9, cz + 9, y, 11, B_DELVARI_BRICKS);
    for (int k = -1; k <= 0; k++) for (int h = 0; h < 5; h++) { P(b, cx + k, y + h, cz + 9, B_CAVE_AIR); P(b, cx + k + 1, y + h, cz + 9, B_CAVE_AIR); }
    for (int k = 0; k < 6; k++) for (int h = 0; h < 11; h++) { P(b, cx - 6, y + h, cz - 6 + k * 3, B_DELVARI_PILLAR); P(b, cx + 5, y + h, cz - 6 + k * 3, B_DELVARI_PILLAR); }
    for (int k = 0; k < 6; k++) for (int h = 4; h <= 10; h += 6) { P(b, cx - 6, y + h, cz - 6 + k * 3, B_UMBRAL_LAMP); P(b, cx + 5, y + h, cz - 6 + k * 3, B_UMBRAL_LAMP); }
    for (int k = -18; k <= 18; k += 6) { P(b, cx + k, y + 4, cz - 20, B_UMBRAL_LAMP); P(b, cx + k, y + 4, cz + 19, B_UMBRAL_LAMP); P(b, cx - 20, y + 4, cz + k, B_UMBRAL_LAMP); P(b, cx + 19, y + 4, cz + k, B_UMBRAL_LAMP); }
    FILL(b, cx - 2, y, cz - 8, cx + 1, y, cz - 6, B_RUNIC_DELVARI_BRICKS);   // throne dais
    FILL(b, cx - 1, y + 1, cz - 8, cx, y + 3, cz - 8, B_RUNIC_DELVARI_BRICKS);
    // the vault behind the throne (a door in the hall's back wall)
    SHELL(b, cx - 4, y - 1, cz - 16, cx + 3, y + 5, cz - 10, B_DELVARI_BRICKS, B_CAVE_AIR);
    for (int h = 0; h < 3; h++) { P(b, cx - 1, y + h, cz - 10, B_CAVE_AIR); P(b, cx, y + h, cz - 10, B_CAVE_AIR); }
    CHEST(b, cx - 3, y, cz - 15, LOOT_DELVARI_VAULT); CHEST(b, cx + 2, y, cz - 15, LOOT_DELVARI_VAULT);
    P(b, cx, y + 4, cz - 13, B_UMBRAL_LAMP);
    MOB(b, cx, y + 1, cz - 4, MOB_DEEP_KEEPER);
    MOB(b, cx - 14, y, cz - 14, MOB_HOLLOW_SENTINEL); MOB(b, cx + 13, y, cz - 14, MOB_HOLLOW_SENTINEL);
    MOB(b, cx - 14, y, cz + 13, MOB_HOLLOW_SENTINEL); MOB(b, cx + 13, y, cz + 13, MOB_HOLLOW_SENTINEL);
    for (int k = 0; k < 4; k++) for (int h = 0; h < 5; h++) P(b, cx + (k & 1 ? 15 : -16), y + h, cz + (k & 2 ? 15 : -16), h == 4 ? B_UMBRAL_LAMP : B_DELVARI_PILLAR);
}
static void build_underground_city(SB *b, const Start *s) {
    int cx = s->x, cz = s->z, y = s->y;
    // 1. the cavern: a flat floor and a domed roof, carved through the deepslate
    for (int z = b->bz; z < b->bz + 16; z++) for (int x = b->bx; x < b->bx + 16; x++) {
        int dx = x - cx, dz = z - cz;
        f32 r = sqrtf((f32)(dx * dx + dz * dz));
        if (r > 64) continue;
        int H = 30 - (int)(14.0f * (r / 64) * (r / 64));
        bool grid = abs(dx) <= 58 && abs(dz) <= 58;
        P(b, x, y - 2, z, B_DEEPSLATE_TILES);
        P(b, x, y - 1, z, grid ? B_DELVARI_TILES : B_POLISHED_DEEPSLATE);
        for (int h = 0; h < H; h++) P(b, x, y + h, z, B_CAVE_AIR);
        bool street_x = ((dx + 10) & 15) < 4, street_z = ((dz + 10) & 15) < 4;
        if (grid && ((street_x && ((dx + 10) & 15) == 1 && (dz & 3) == 0) || (street_z && ((dz + 10) & 15) == 1 && (dx & 3) == 0))) P(b, x, y - 1, z, B_UMBRAL_LAMP);   // amber floor lamps light every street
        if (r > 58) for (int h = 0; h < 4; h++) P(b, x, y + h, z, (hash3i(s->key, x, h, z) % 4) ? B_CAVE_AIR : B_COBBLED_DEEPSLATE);
    }
    // 2. district lots
    for (int j = -3; j <= 3; j++) for (int i = -3; i <= 3; i++) {
        if (abs(i) <= 1 && abs(j) <= 1) continue;   // the keep
        Lot l = {cx + i * 16 - 6, cz + j * 16 - 6, y, 0};
        if (!lot_hits(b, l.x0 - 1, l.z0 - 1, l.x0 + 12, l.z0 + 12)) continue;
        l.door = abs(i) >= abs(j) ? (i > 0 ? 3 : 1) : (j > 0 ? 0 : 2);   // face the keep
        if (j == -2 && abs(i) <= 1) city_market(b, s, &l, i, j);
        else if (j == 2 && i == 0) city_temple(b, s, &l);
        else if (j == 2 && abs(i) == 1) city_plaza(b, s, &l);
        else if (i == 2 && abs(j) <= 1) city_prison(b, s, &l, j != 0);
        else if (i == -2 && abs(j) <= 1) city_house(b, s, &l, i, j, false);
        else {
            int k = lot_rnd(b, s, i, j, 6, 20);
            if (k < 11) city_house(b, s, &l, i, j, false);
            else if (k < 14) city_plaza(b, s, &l);
            else if (k < 17) city_garden(b, s, &l, i, j);
            else city_house(b, s, &l, i, j, true);
        }
    }
    // 3. the keep
    if (lot_hits(b, cx - 23, cz - 23, cx + 22, cz + 22)) city_keep(b, s, y);
    // 4. lamp posts at the street crossings
    for (int j = -3; j <= 4; j++) for (int i = -3; i <= 4; i++) {
        int x = cx + i * 16 - 8, z = cz + j * 16 - 8;
        if (abs(x - cx) <= 23 && abs(z - cz) <= 23) continue;
        if (!inside(b, x, z)) continue;
        for (int h = 0; h < 4; h++) P(b, x, y + h, z, h == 3 ? B_UMBRAL_LAMP : B_DELVARI_PILLAR);
    }
}

// ============================================================ the Sift: temples of the Luminaries
static void sift_base(SB *b, int cx, int cz, int r, int y, u16 floor) {   // a floating platform with a tapering underside
    for (int dz = -r; dz <= r; dz++) for (int dx = -r; dx <= r; dx++) {
        int x = cx + dx, z = cz + dz;
        if (!inside(b, x, z)) continue;
        int k = MAXI(abs(dx), abs(dz));
        P(b, x, y - 1, z, k == r ? B_HOLY_STONE_BRICKS : floor);
        for (int h = 2; h <= 2 + (r - k); h++) P(b, x, y - h, z, B_HOLY_STONE);
        for (int h = 0; h < 12; h++) P(b, x, y + h, z, B_AIR);
    }
}
static void build_temple_dawn(SB *b, const Start *s) {   // Solenne: an open ring of gold-capped pillars in the Golden Meadows
    int cx = s->x, cz = s->z, y = s->y;
    sift_base(b, cx, cz, 7, y, B_HOLY_STONE_BRICKS);
    for (int k = 0; k < 8; k++) {
        int px = cx + (int)lroundf(6 * cosf(k * PI_F / 4)), pz = cz + (int)lroundf(6 * sinf(k * PI_F / 4));
        for (int h = 0; h < 6; h++) P(b, px, y + h, pz, h == 5 ? B_GOLD_BLOCK : B_HOLY_STONE_BRICKS);
        P(b, px, y + 6, pz, B_RADIANT_CRYSTAL);
    }
    FILL(b, cx - 1, y - 1, cz - 1, cx + 1, y - 1, cz + 1, B_GOLD_BLOCK);
    P(b, cx, y - 1, cz, B_RADIANT_CRYSTAL);
    CHEST(b, cx + 4, y, cz + 4, LOOT_SIFT);
    MOB(b, cx, y + 1, cz, MOB_GOD_SOLENNE);
}
static void build_spire_winds(SB *b, const Start *s) {   // Zephyrine: a tall spire on the Cloud Sea with a spiral stair
    int cx = s->x, cz = s->z, y0 = s->y, H = 34;
    for (int dz = -5; dz <= 5; dz++) for (int dx = -5; dx <= 5; dx++) for (int h = -8; h < 0; h++) if (dx * dx + dz * dz <= 26) P(b, cx + dx, y0 + h, cz + dz, B_CLOUD_BLOCK);
    for (int dz = -5; dz <= 5; dz++) for (int dx = -5; dx <= 5; dx++) for (int h = 0; h <= H + 6; h++) P(b, cx + dx, y0 + h, cz + dz, B_AIR);
    for (int dz = -2; dz <= 2; dz++) for (int dx = -2; dx <= 2; dx++) if (abs(dx) + abs(dz) < 4) for (int h = 0; h < H; h++) P(b, cx + dx, y0 + h, cz + dz, B_HOLY_STONE_BRICKS);
    for (int h = 0; h < H; h++) {   // one step per block of height, wrapping around the column
        f32 a = h * 0.26f;
        P(b, cx + (int)lroundf(4 * cosf(a)), y0 + h, cz + (int)lroundf(4 * sinf(a)), B_HOLY_STONE_BRICKS);
    }
    for (int dz = -5; dz <= 5; dz++) for (int dx = -5; dx <= 5; dx++) P(b, cx + dx, y0 + H, cz + dz, (abs(dx) == 5 || abs(dz) == 5) ? B_GOLD_BLOCK : B_HOLY_STONE_BRICKS);
    for (int k = 0; k < 4; k++) P(b, cx + (k & 1 ? 4 : -4), y0 + H + 1, cz + (k & 2 ? 4 : -4), B_RADIANT_CRYSTAL);
    CHEST(b, cx + 3, y0 + H + 1, cz - 3, LOOT_SIFT);
    MOB(b, cx, y0 + H + 1, cz, MOB_GOD_ZEPHYRINE);
}
static void build_arena_stone(SB *b, const Start *s) {   // Thalor: a walled arena of crystal and holy stone
    int cx = s->x, cz = s->z, y = s->y;
    sift_base(b, cx, cz, 10, y, B_CRYSTAL_PLANKS);
    for (int dz = -10; dz <= 10; dz++) for (int dx = -10; dx <= 10; dx++) if (abs(dx) == 10 || abs(dz) == 10) for (int h = 0; h < 2; h++) P(b, cx + dx, y + h, cz + dz, B_HOLY_STONE_BRICKS);
    for (int k = 0; k < 4; k++) for (int h = 0; h < 5; h++) P(b, cx + (k & 1 ? 8 : -8), y + h, cz + (k & 2 ? 8 : -8), h == 4 ? B_RADIANT_CRYSTAL : B_CRYSTAL_LOG);
    FILL(b, cx - 1, y, cz - 9, cx + 1, y + 1, cz - 7, B_HOLY_STONE_BRICKS);   // Thalor's plinth
    CHEST(b, cx + 7, y, cz - 8, LOOT_SIFT);
    MOB(b, cx, y + 3, cz - 8, MOB_GOD_THALOR);
}

// UltimateCraft: a walled arena where the fortress bridges cross, ruled by the Brimstone Revenant
static void fortress_arena(SB *b, const Start *s) {
    int cx = s->x, cz = s->z, y = 64;
    for (int dz = -7; dz <= 7; dz++) for (int dx = -7; dx <= 7; dx++) {
        int x = cx + dx, z = cz + dz;
        if (!inside(b, x, z)) continue;
        bool edge = abs(dx) == 7 || abs(dz) == 7, gate = abs(dx) <= 2 || abs(dz) <= 2;
        P(b, x, y, z, (abs(dx) + abs(dz)) % 5 == 0 ? B_RED_NETHER_BRICKS : B_NETHER_BRICKS);
        for (int h = 1; h <= 6; h++) P(b, x, y + h, z, B_AIR);
        if (edge && !gate) for (int h = 1; h <= 3; h++) P(b, x, y + h, z, h == 3 ? B_NETHER_BRICK_FENCE : B_NETHER_BRICKS);
    }
    for (int k = 0; k < 4; k++) { int x = cx + (k & 1 ? 6 : -6), z = cz + (k & 2 ? 6 : -6); P(b, x, y + 1, z, B_SOUL_SAND); P(b, x, y + 2, z, B_SOUL_FIRE); }
    CHEST(b, cx + 5, y + 1, cz - 5, LOOT_FORTRESS);
    MOB(b, cx, y + 1, cz, MOB_BRIMSTONE_REVENANT);
}

static void build_structure(SB *b, const Start *s) {
    switch (b->type) {
    case ST_VILLAGE: build_village(b, s); break;
    case ST_OUTPOST: build_outpost(b, s); break;
    case ST_DESERT_PYRAMID: build_desert_pyramid(b, s); break;
    case ST_JUNGLE_TEMPLE: build_jungle_temple(b, s); break;
    case ST_WITCH_HUT: build_witch_hut(b, s); break;
    case ST_IGLOO: build_igloo(b, s); break;
    case ST_MONUMENT: build_monument(b, s); break;
    case ST_SHIPWRECK: build_shipwreck(b, s); break;
    case ST_OCEAN_RUIN: build_ocean_ruin(b, s); break;
    case ST_BURIED_TREASURE: build_buried_treasure(b, s); break;
    case ST_RUINED_PORTAL: build_ruined_portal(b, s, false); break;
    case ST_RUINED_PORTAL_NETHER: build_ruined_portal(b, s, true); break;
    case ST_MINESHAFT: build_mineshaft(b, s); break;
    case ST_MANSION: build_mansion(b, s); break;
    case ST_ANCIENT_CITY: build_ancient_city(b, s); break;
    case ST_TRIAL_CHAMBERS: build_trial_chambers(b, s); break;
    case ST_FORTRESS: build_fortress(b, s); fortress_arena(b, s); break;
    case ST_BASTION: build_bastion(b, s); break;
    case ST_END_CITY: build_end_city(b, s); break;
    case ST_UNDERGROUND_CITY: build_underground_city(b, s); break;
    case ST_TEMPLE_DAWN: build_temple_dawn(b, s); break;
    case ST_SPIRE_WINDS: build_spire_winds(b, s); break;
    case ST_ARENA_STONE: build_arena_stone(b, s); break;
    default: break;
    }
}

// More Java-style commands (included by commands_mc.c, so it shares the tokenizer, selectors and coordinates):
// /scoreboard (+ sidebar), /execute store|if score, /data, /attribute, /worldborder, /spreadplayers, /fillbiome,
// /forceload, /item replace, /loot, /function with data packs (load/tick tags), /reload, /datapack list.

// forward declarations (command blocks are defined further down)
void cmdblocks_load(const char *dim_dir);
static void cmdblocks_tick(void);

// ------------------------------------------------------------------ scoreboard
SbObj g_sb_obj[SB_MAX_OBJ]; int g_sb_nobj, g_sb_sidebar = -1;
SbScore g_sb[SB_MAX_SCORES]; int g_sb_n;
int sb_obj(const char *name) { for (int i = 0; i < g_sb_nobj; i++) if (!strcmp(g_sb_obj[i].name, name)) return i; return -1; }
static const char *holder_of(Entity *e) {
    static char b[4][40]; static int k; k = (k + 1) & 3;
    if (e->kind == ENT_PLAYER) return "Player";
    snprintf(b[k], sizeof b[k], "e%u", e->id); return b[k];
}
static SbScore *sb_find(const char *h, int o) { for (int i = 0; i < g_sb_n; i++) if (g_sb[i].obj == o && !strcmp(g_sb[i].holder, h)) return &g_sb[i]; return NULL; }
bool sb_get(const char *h, int o, int *v) {
    if (o < 0) return false;
    if (!strcmp(g_sb_obj[o].crit, "health") && !strcmp(h, "Player")) { Entity *p = player_ent(); *v = p ? (int)ceilf(p->health) : 0; return true; }
    SbScore *s = sb_find(h, o); if (!s) return false; *v = s->value; return true;
}
void sb_set(const char *h, int o, int v) {
    if (o < 0) return;
    SbScore *s = sb_find(h, o);
    if (!s) { if (g_sb_n >= SB_MAX_SCORES) return; s = &g_sb[g_sb_n++]; snprintf(s->holder, sizeof s->holder, "%s", h); s->obj = (u8)o; }
    s->value = v;
}
static void sb_reset(const char *h, int o) { for (int i = 0; i < g_sb_n; i++) if (!strcmp(g_sb[i].holder, h) && (o < 0 || g_sb[i].obj == o)) g_sb[i--] = g_sb[--g_sb_n]; }
void scoreboard_event(const char *crit, const char *holder, int add) {   // deathCount / totalKillCount ...
    for (int o = 0; o < g_sb_nobj; o++) if (!strcmp(g_sb_obj[o].crit, crit)) { int v = 0; sb_get(holder, o, &v); sb_set(holder, o, v + add); }
}
void scoreboard_save(const char *dir) {
    char path[320]; snprintf(path, sizeof path, "%.300s/scoreboard.txt", dir);
    StrBuf f = {0};
    for (int o = 0; o < g_sb_nobj; o++) sbuf_printf(&f, "obj %s %s %s\n", g_sb_obj[o].name, g_sb_obj[o].crit, g_sb_obj[o].disp);
    for (int i = 0; i < g_sb_n; i++) sbuf_printf(&f, "score %s %s %d\n", g_sb_obj[g_sb[i].obj].name, g_sb[i].holder, g_sb[i].value);
    if (g_sb_sidebar >= 0) sbuf_printf(&f, "sidebar %s\n", g_sb_obj[g_sb_sidebar].name);
    sbuf_write(&f, path);
}
void scoreboard_load(const char *dir) {
    g_sb_nobj = g_sb_n = 0; g_sb_sidebar = -1;
    char path[320]; snprintf(path, sizeof path, "%.300s/scoreboard.txt", dir);
    FILE *f = fopen(path, "r"); if (!f) return;
    char line[256], a[64], b[64], c[64]; int v;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "obj %63s %63s %63[^\n]", a, b, c) >= 2 && g_sb_nobj < SB_MAX_OBJ) { SbObj *o = &g_sb_obj[g_sb_nobj++]; snprintf(o->name, 32, "%.31s", a); snprintf(o->crit, 32, "%.31s", b); snprintf(o->disp, 48, "%.47s", c); }
        else if (sscanf(line, "score %63s %63s %d", a, b, &v) == 3) sb_set(b, sb_obj(a), v);
        else if (sscanf(line, "sidebar %63s", a) == 1) g_sb_sidebar = sb_obj(a);
    }
    fclose(f);
}
// holder list for a score argument: a selector, "*" (everyone with a score), or a literal name
static int holders(const char *s, int obj, const char **out, int cap) {
    static char bufs[4][256][40]; static int bi; bi = (bi + 1) & 3; char (*names)[40] = bufs[bi]; int n = 0;   // rotating: two selectors can be live at once
    if (s[0] == '@') { Entity *t[256]; int k = targets(s, t, 256); for (int i = 0; i < k && n < cap; i++) { snprintf(names[n], 40, "%s", holder_of(t[i])); out[n] = names[n]; n++; } return n; }
    if (!strcmp(s, "*")) { for (int i = 0; i < g_sb_n && n < cap; i++) if (obj < 0 || g_sb[i].obj == obj) { bool dup = false; for (int j = 0; j < n; j++) dup |= !strcmp(out[j], g_sb[i].holder); if (!dup) out[n++] = g_sb[i].holder; } return n; }
    snprintf(names[0], 40, "%s", s); out[0] = names[0]; return 1;
}

// ------------------------------------------------------------------ world border
WorldBorder g_wb = {0, 0, 59999968, 59999968, 0, 0.2f, 5, 5};
static void wb_tick_player(void) {
    Entity *p = player_ent();
    if (g_wb.lerp > 0) { g_wb.size += (g_wb.target - g_wb.size) / g_wb.lerp; g_wb.lerp--; }
    if (!p || g_player.gamemode == GM_SPECTATOR || g_player.gamemode == GM_CREATIVE) return;
    f32 h = (f32)g_wb.size / 2, dx = p->pos.x - g_wb.cx, dz = p->pos.z - g_wb.cz;
    f32 out = fmaxf(fabsf(dx), fabsf(dz)) - h;
    if (out > 0) {   // pushed back inside, hurt beyond the buffer
        p->pos.x = g_wb.cx + clampf(dx, -h + 0.3f, h - 0.3f); p->pos.z = g_wb.cz + clampf(dz, -h + 0.3f, h - 0.3f);
        if (out > g_wb.buffer && G.ticks % TICKRATE == 0) entity_damage(p, fmaxf(1, g_wb.damage * (out - g_wb.buffer)), DMG_GENERIC, NULL);
    }
}

// ------------------------------------------------------------------ forceload (kept loaded while inside the loaded area)
static ivec3 forced[256]; static int nforced;
bool forceload_has(int cx, int cz) { for (int i = 0; i < nforced; i++) if (forced[i].x == cx && forced[i].z == cz) return true; return false; }
void forceload_save(const char *dim_dir) {
    char path[320]; snprintf(path, sizeof path, "%.300s/forceload.txt", dim_dir);
    if (!nforced) { remove(path); return; }
    StrBuf f = {0};
    for (int i = 0; i < nforced; i++) sbuf_printf(&f, "%d %d\n", forced[i].x, forced[i].z);
    sbuf_write(&f, path);
}
void forceload_load(const char *dim_dir) {
    nforced = 0;
    char path[320]; snprintf(path, sizeof path, "%.300s/forceload.txt", dim_dir);
    FILE *f = fopen(path, "r"); if (!f) return;
    int x, z; while (nforced < 256 && fscanf(f, "%d %d", &x, &z) == 2) forced[nforced++] = iv3(x, 0, z);
    fclose(f);
}

// ------------------------------------------------------------------ functions and data packs
// Data packs live in saves/<world>/datapacks/<pack>/data/<namespace>/function[s]/<path>.mcfunction (or the same
// layout in ./datapacks for every world). data/minecraft/tags/function[s]/load.json and tick.json list functions
// to run once when the world loads and every game tick.
static char packs[32][300]; static int npacks;
static char load_fns[32][96], tick_fns[32][96]; static int nload, ntick;
static int fn_budget;
static void scan_tag(const char *file, char out[][96], int *n) {
    size_t len; char *s = plat_read_file(file, &len); if (!s) return;
    const char *v = strstr(s, "\"values\"");
    for (const char *q = v ? strchr(v, '[') : NULL; q && *q && *q != ']'; q++) {
        if (*q != '"') continue;
        const char *e = strchr(q + 1, '"'); if (!e) break;
        if (*n < 32) { int l = (int)MINI(95, e - q - 1); memcpy(out[*n], q + 1, (size_t)l); out[*n][l] = 0; (*n)++; }
        q = e;
    }
    free(s);
}
void datapacks_scan(void) {
    npacks = nload = ntick = 0;
    const char *roots[2] = {g_world.dir, "."};
    for (int r = 0; r < 2; r++) {
        char base[300], names[32][64]; snprintf(base, sizeof base, "%.250s/datapacks", roots[r]);
        int n = plat_list_dirs(base, names, 32);
        for (int i = 0; i < n && npacks < 32; i++) {
            snprintf(packs[npacks], 300, "%.250s/%.40s", base, names[i]);
            for (int pl = 0; pl < 4; pl++) {   // data/{minecraft,ultimatecraft}/tags/function[s]/
                char tf[400]; const char *tns = pl < 2 ? "minecraft" : "ultimatecraft";
                snprintf(tf, sizeof tf, "%s/data/%s/tags/%s/load.json", packs[npacks], tns, pl & 1 ? "functions" : "function"); scan_tag(tf, load_fns, &nload);
                snprintf(tf, sizeof tf, "%s/data/%s/tags/%s/tick.json", packs[npacks], tns, pl & 1 ? "functions" : "function"); scan_tag(tf, tick_fns, &ntick);
            }
            npacks++;
        }
    }
}
static int function_run(const char *name, int depth) {
    if (depth > 8) return 0;
    const char *nm = name[0] == '#' ? name + 1 : name;
    char ns[64] = "ultimatecraft", path[160];
    const char *colon = strchr(nm, ':');
    if (colon) { snprintf(ns, sizeof ns, "%.*s", (int)MINI(63, colon - nm), nm); snprintf(path, sizeof path, "%s", colon + 1); }
    else snprintf(path, sizeof path, "%s", nm);
    char *src = NULL; size_t len;
    for (int p = 0; p < npacks && !src; p++) for (int pl = 0; pl < 2 && !src; pl++) {
        char f[600]; snprintf(f, sizeof f, "%.299s/data/%.60s/%s/%.150s.mcfunction", packs[p], ns, pl ? "functions" : "function", path);
        src = plat_read_file(f, &len);
    }
    if (!src && !colon) {   // no namespace given: also look in minecraft:
        for (int p = 0; p < npacks && !src; p++) for (int pl = 0; pl < 2 && !src; pl++) {
            char f[600]; snprintf(f, sizeof f, "%.299s/data/minecraft/%s/%.150s.mcfunction", packs[p], pl ? "functions" : "function", path);
            src = plat_read_file(f, &len);
        }
    }
    if (!src) return -1;
    int count = 0;
    for (char *line = src, *next; line && *line; line = next) {   // manual split: nested commands may use strtok
        next = line + strcspn(line, "\r\n");
        if (*next) { *next++ = 0; while (*next == '\r' || *next == '\n') next++; } else next = NULL;
        while (*line == ' ' || *line == '\t') line++;
        if (!*line || *line == '#') continue;
        if (fn_budget-- <= 0) { LOG_WARN("function %s: maxCommandChainLength reached", name); break; }
        char *save = NULL; (void)save;
        char copy[512]; snprintf(copy, sizeof copy, "%s", line);
        bool h; Toks *t2 = xmalloc(sizeof(Toks)); tokenize(copy, t2);
        if (t2->n && !strcmp(t2->t[0], "function") && t2->n >= 2) { char sub[128]; snprintf(sub, sizeof sub, "%.127s", t2->t[1]); free(t2); int c2 = function_run(sub, depth + 1); count += MAXI(0, c2); continue; }
        fb_suppress++;
        if (t2->n) { if (!run(t2, &h) && !h) command_execute(copy); }
        fb_suppress--;
        free(t2);
        count++;
    }
    free(src);
    return count;
}
static char dp_world[64];
static int g_fn_runs;
char g_sb_world[64];   // world whose scoreboard is loaded
// called once whenever a world is opened (new or loaded): scores, force-loaded chunks, data packs + load functions
void world_opened(void) {
    if (!strcmp(g_sb_world, g_world.name)) {   // same world, new dimension: only per-dimension data changes
        forceload_load(g_world.dim_dir); cmdblocks_load(g_world.dim_dir);
        snprintf(dp_world, sizeof dp_world, "%.63s", g_world.name);
        return;
    }
    snprintf(g_sb_world, sizeof g_sb_world, "%.63s", g_world.name);
    scoreboard_load(g_world.dir);
    forceload_load(g_world.dim_dir);
    cmdblocks_load(g_world.dim_dir);
    progress_load(g_world.dir);
    snprintf(dp_world, sizeof dp_world, "%.63s", g_world.name);
    datapacks_scan();
    Entity *pl = player_ent();
    for (int i = 0; i < nload; i++) { fn_budget = rule_i(GR_MAX_COMMAND_CHAIN_LENGTH); Ctx c = {pl, pl ? pl->pos : v3(0, 0, 0), 0, 0, 0, 1}; Ctx *sv = cur; cur = &c; function_run(load_fns[i], 0); cur = sv; g_fn_runs++; }
}
void datapacks_tick(void) {   // tick functions every game tick (+ the world border)
    if (!G.in_world) { dp_world[0] = 0; g_sb_world[0] = 0; return; }
    if (strcmp(dp_world, g_world.name)) world_opened();   // fallback: a world opened some other way
    for (int i = 0; i < ntick; i++) { fn_budget = rule_i(GR_MAX_COMMAND_CHAIN_LENGTH); Ctx c = {player_ent(), player_ent() ? player_ent()->pos : v3(0, 0, 0), 0, 0, 0, 1}; Ctx *s = cur; cur = &c; function_run(tick_fns[i], 0); cur = s; g_fn_runs++; }
    wb_tick_player();
    cmdblocks_tick();
}

// ------------------------------------------------------------------ loot table names
static const char *LOOT_NAMES[LOOT_COUNT] = {"village", "weaponsmith", "desert_pyramid", "jungle_temple", "igloo", "shipwreck_supply", "shipwreck_treasure",
    "shipwreck_map", "buried_treasure", "ruined_portal", "abandoned_mineshaft", "simple_dungeon", "stronghold", "hell_fortress", "bastion", "star_citadel",
    "star_ship", "woodland_mansion", "ancient_city", "pillager_outpost", "ocean_ruin", "trial_chambers", "witch_hut", "delvari_house", "delvari_temple",
    "delvari_vault", "sift_temple"};
static int loot_by_name(const char *s) {
    const char *k = strrchr(s, '/'); k = k ? k + 1 : strip_ns(s);
    for (int i = 0; i < LOOT_COUNT; i++) if (!strcmp(LOOT_NAMES[i], k)) return i;
    if (!strcmp(k, "nether_bridge")) return LOOT_FORTRESS;
    if (!strcmp(k, "end_city_treasure")) return LOOT_END_CITY;
    return -1;
}
static int slot_index(const char *s) {   // player slots: weapon.mainhand, hotbar.N, inventory.N, armor.*, weapon.offhand
    if (!strcmp(s, "weapon.mainhand") || !strcmp(s, "weapon")) return 100;
    if (!strcmp(s, "weapon.offhand")) return 101;
    if (!strcmp(s, "armor.head")) return 200;
    if (!strcmp(s, "armor.chest")) return 201;
    if (!strcmp(s, "armor.legs")) return 202;
    if (!strcmp(s, "armor.feet")) return 203;
    if (!strncmp(s, "hotbar.", 7)) return atoi(s + 7);
    if (!strncmp(s, "inventory.", 10)) return 9 + atoi(s + 10);
    if (!strncmp(s, "container.", 10)) return 300 + atoi(s + 10);
    return -1;
}


// ------------------------------------------------------------------ command blocks
// Impulse blocks run once when they become powered, repeating blocks every tick while powered, and chain blocks
// next to a block that just ran run right after it (up to maxCommandChainLength). Commands run at the block
// (selectors like @p measure from it) and only print when gamerule commandBlockOutput is on. Stored per dimension
// in commandblocks.txt. In creative, right-click a command block to edit it.
typedef struct { ivec3 p; char cmd[240]; bool powered; int last; } CmdBlock;
static CmdBlock cmdb[256]; static int ncmdb;
int g_cmdblock_runs;
static CmdBlock *cmdblock_at(ivec3 p, bool create) {
    for (int i = 0; i < ncmdb; i++) if (iv3_eq(cmdb[i].p, p)) return &cmdb[i];
    if (!create || ncmdb >= 256) return NULL;
    CmdBlock *c = &cmdb[ncmdb++]; memset(c, 0, sizeof *c); c->p = p; return c;
}
static bool is_cmd_block(u16 b) { int k = BLOCKS[b].base; return k == B_COMMAND_BLOCK || k == B_REPEATING_COMMAND_BLOCK || k == B_CHAIN_COMMAND_BLOCK; }
void cmdblocks_save(const char *dim_dir) {
    char path[320]; snprintf(path, sizeof path, "%.300s/commandblocks.txt", dim_dir);
    int n = 0; for (int i = 0; i < ncmdb; i++) n += cmdb[i].cmd[0] != 0;
    if (!n) { remove(path); return; }
    StrBuf f = {0};
    for (int i = 0; i < ncmdb; i++) if (cmdb[i].cmd[0]) sbuf_printf(&f, "%d %d %d %d %s\n", cmdb[i].p.x, cmdb[i].p.y, cmdb[i].p.z, (int)cmdb[i].powered, cmdb[i].cmd);
    sbuf_write(&f, path);
}
void cmdblocks_load(const char *dim_dir) {
    ncmdb = 0;
    char path[320]; snprintf(path, sizeof path, "%.300s/commandblocks.txt", dim_dir);
    FILE *f = fopen(path, "r"); if (!f) return;
    char line[300]; int x, y, z, pw, off;
    while (ncmdb < 256 && fgets(line, sizeof line, f)) {
        if (sscanf(line, "%d %d %d %d %n", &x, &y, &z, &pw, &off) < 4) continue;
        CmdBlock *c = cmdblock_at(iv3(x, y, z), true); c->powered = pw != 0;
        snprintf(c->cmd, sizeof c->cmd, "%.239s", line + off); c->cmd[strcspn(c->cmd, "\r\n")] = 0;
    }
    fclose(f);
}
const char *cmdblock_get(ivec3 p) { CmdBlock *c = cmdblock_at(p, false); return c ? c->cmd : ""; }
void cmdblock_set(ivec3 p, const char *cmd) { CmdBlock *c = cmdblock_at(p, true); if (c) { snprintf(c->cmd, sizeof c->cmd, "%.239s", cmd[0] == '/' ? cmd + 1 : cmd); cmdblocks_save(g_world.dim_dir); } }
static int cmdblock_exec(CmdBlock *c) {
    if (!c->cmd[0]) return 0;
    Ctx ctx = {NULL, iv3_center(c->p), 0, 0, 0, 1};
    Ctx *sv = cur; cur = &ctx;
    if (!rule(GR_COMMAND_BLOCK_OUTPUT)) fb_suppress++;
    Toks *t = xmalloc(sizeof(Toks)); tokenize(c->cmd, t);
    bool h = false;
    if (t->n) { if (!run(t, &h) && !h) { Entity *pl = player_ent(); vec3 keep = pl ? pl->pos : v3(0, 0, 0); command_execute(c->cmd); (void)keep; } }
    free(t);
    if (!rule(GR_COMMAND_BLOCK_OUTPUT)) fb_suppress--;
    cur = sv;
    g_cmdblock_runs++;
    c->last = ctx.result;
    return 1;
}
static void cmdblock_run_chain(CmdBlock *c) {   // run c, then chain blocks around it (breadth first)
    ivec3 q[64]; int nq = 0, head = 0, budget = MINI(64, rule_i(GR_MAX_COMMAND_CHAIN_LENGTH));
    cmdblock_exec(c); q[nq++] = c->p;
    while (head < nq && nq < budget) {
        ivec3 p = q[head++];
        for (int f = 0; f < 6 && nq < budget; f++) {
            ivec3 n = iv3(p.x + FACE_DIR[f].x, p.y + FACE_DIR[f].y, p.z + FACE_DIR[f].z);
            if (BLOCKS[world_get_block(n.x, n.y, n.z)].base != B_CHAIN_COMMAND_BLOCK) continue;
            bool seen = false; for (int i = 0; i < nq; i++) seen |= iv3_eq(q[i], n);
            if (seen) continue;
            CmdBlock *cc = cmdblock_at(n, false);
            if (cc) cmdblock_exec(cc);
            q[nq++] = n;
        }
    }
}
// redstone.c calls this whenever a command block's input changes
void cmdblock_redstone(ivec3 p, bool powered) {
    u16 b = world_get_block(p.x, p.y, p.z);
    if (!is_cmd_block(b)) return;
    CmdBlock *c = cmdblock_at(p, powered);
    if (!c) return;
    bool rising = powered && !c->powered;
    c->powered = powered;
    if (rising && BLOCKS[b].base == B_COMMAND_BLOCK) cmdblock_run_chain(c);
}
static void cmdblocks_tick(void) {   // repeating blocks run every game tick (20/s) while powered
    if (G.ticks % MC_TICK_DIV) return;
    for (int i = 0; i < ncmdb; i++) {
        CmdBlock *c = &cmdb[i];
        if (!c->powered || !world_loaded(c->p.x, c->p.z)) continue;
        if (BLOCKS[world_get_block(c->p.x, c->p.y, c->p.z)].base == B_REPEATING_COMMAND_BLOCK) cmdblock_run_chain(c);
    }
}

// ------------------------------------------------------------------ Tab completion (chat)
static const char *const CMD_NAMES[] = {"advancement", "attribute", "clear", "clone", "credits", "damage", "data", "datapack", "difficulty", "effect", "enchant",
    "execute", "experience", "fill", "fillbiome", "forceload", "function", "gamemode", "gamerule", "give", "guide", "help", "item", "kill", "list", "locate", "loot",
    "me", "msg", "particle", "playsound", "recipe", "reload", "ride", "say", "scoreboard", "seed", "setblock", "setworldspawn", "spawnpoint", "spreadplayers",
    "summon", "tag", "tellraw", "tick", "time", "title", "tp", "trigger", "weather", "worldborder", "xp", NULL};
static int common_prefix(const char *a, const char *b) { int n = 0; while (a[n] && a[n] == b[n]) n++; return n; }
// completes the last word of `line` in place; writes up to 8 suggestions to `hint`; returns the number of matches
int command_complete(char *line, size_t cap, char *hint, size_t hcap) {
    hint[0] = 0;
    char *body = line[0] == '/' ? line + 1 : line;
    char *last = strrchr(body, ' '); last = last ? last + 1 : body;
    bool first = last == body;
    char cmd[32] = ""; sscanf(body, "%31s", cmd);
    const char *prefix_ns = !strncmp(last, "minecraft:", 10) ? "minecraft:" : !strncmp(last, "ultimatecraft:", 14) ? "ultimatecraft:" : "";
    const char *want = last + strlen(prefix_ns);
    size_t wl = strlen(want);
    const char *best = NULL; int nm = 0, cp = 0; size_t hl = 0;
    #define CAND(s) do { const char *s_ = (s); if (s_ && !strncmp(s_, want, wl)) { if (!best) { best = s_; cp = (int)strlen(s_); } else cp = MINI(cp, common_prefix(best, s_)); \
        if (nm < 8 && hl + strlen(s_) + 2 < hcap) { hl += (size_t)snprintf(hint + hl, hcap - hl, "%s%s", nm ? "  " : "", s_); } \
        nm++; } } while (0)
    if (first) { for (int i = 0; CMD_NAMES[i]; i++) CAND(CMD_NAMES[i]); }
    else if (want[0] == '@') { static const char *S[] = {"@p", "@a", "@r", "@s", "@e", "@n"}; for (int i = 0; i < 6; i++) CAND(S[i]); }
    else if (!strcmp(cmd, "summon")) { for (int i = 0; i < MOB_COUNT; i++) CAND(MOB_DEFS[i].key); for (int i = 0; i < DECOR_COUNT; i++) { static const char *D[] = {"item_frame", "glow_item_frame", "painting", "armor_stand"}; CAND(D[i]); } }
    else if (!strcmp(cmd, "setblock") || !strcmp(cmd, "fill")) { for (int i = 1; i < BLOCK_STATES; i++) if (BLOCKS[i].var == 0 && BLOCKS[i].key) CAND(BLOCKS[i].key); }
    else if (!strcmp(cmd, "gamerule")) { for (int i = 0; i < GR_COUNT; i++) CAND(GAMERULE_DEFS[i].key); }
    else if (!strcmp(cmd, "locate")) { for (int i = 0; i < ST_COUNT; i++) CAND(STRUCTS[i].key); }
    else if (!strcmp(cmd, "difficulty")) { static const char *D[] = {"peaceful", "easy", "normal", "hard"}; for (int i = 0; i < 4; i++) CAND(D[i]); }
    else if (!strcmp(cmd, "gamemode")) { static const char *D[] = {"survival", "creative", "adventure", "spectator"}; for (int i = 0; i < 4; i++) CAND(D[i]); }
    else { for (int i = 1; i < ITEM_MAX; i++) if (item_valid(i)) CAND(ITEMS[i].key); }
    #undef CAND
    if (!nm) return 0;
    size_t pos = (size_t)(want - line);
    if (pos + (size_t)cp + 2 < cap) { memcpy(line + pos, best, (size_t)cp); line[pos + cp] = 0; if (nm == 1) strcat(line, " "); }
    return nm;
}

static bool run_ext(Toks *k, bool *handled) {
    #define A(i) (k->n > (i) ? k->t[i] : "")
    const char *c = k->t[0]; if (c[0] == '/') c++;
    *handled = true;
    // ---- /scoreboard
    if (!strcmp(c, "scoreboard") && k->n >= 3) {
        if (!strcmp(A(1), "objectives")) {
            if (!strcmp(A(2), "add") && k->n >= 5) {
                if (sb_obj(A(3)) >= 0) return fail("An objective already exists by that name");
                if (g_sb_nobj >= SB_MAX_OBJ) return fail("Too many objectives");
                SbObj *o = &g_sb_obj[g_sb_nobj++]; snprintf(o->name, 32, "%.31s", A(3)); snprintf(o->crit, 32, "%.31s", strip_ns(A(4)));
                char d[48]; snprintf(d, sizeof d, "%.47s", k->n >= 6 ? k->rest[5] : A(3)); unquote(d); snprintf(o->disp, 48, "%.47s", d);
                ok("Created new objective [%s]", o->disp); return true;
            }
            if (!strcmp(A(2), "remove") && k->n >= 4) {
                int o = sb_obj(A(3)); if (o < 0) return fail("Unknown scoreboard objective '%s'", A(3));
                for (int i = 0; i < g_sb_n; i++) if (g_sb[i].obj == o) g_sb[i--] = g_sb[--g_sb_n];
                for (int i = 0; i < g_sb_n; i++) if (g_sb[i].obj > o) g_sb[i].obj--;
                memmove(&g_sb_obj[o], &g_sb_obj[o + 1], sizeof(SbObj) * (size_t)(g_sb_nobj - o - 1)); g_sb_nobj--;
                if (g_sb_sidebar == o) g_sb_sidebar = -1; else if (g_sb_sidebar > o) g_sb_sidebar--;
                ok("Removed objective %s", A(3)); return true;
            }
            if (!strcmp(A(2), "list")) { char b[256] = ""; size_t bl = 0; for (int o = 0; o < g_sb_nobj && bl < sizeof b - 40; o++) bl += (size_t)snprintf(b + bl, sizeof b - bl, "%s%s", g_sb_obj[o].name, o + 1 < g_sb_nobj ? ", " : ""); game_message(C_INFO, "There are %d objective(s): %s", g_sb_nobj, b); cur->result = g_sb_nobj; return true; }
            if (!strcmp(A(2), "setdisplay") && k->n >= 4) {
                if (strncmp(A(3), "sidebar", 7)) return fail("Only the sidebar display slot is supported");
                g_sb_sidebar = k->n >= 5 ? sb_obj(A(4)) : -1;
                if (k->n >= 5 && g_sb_sidebar < 0) return fail("Unknown scoreboard objective '%s'", A(4));
                ok(g_sb_sidebar >= 0 ? "Set display slot sidebar to show objective %s" : "Cleared objective display slot sidebar", A(4)); return true;
            }
            return fail("Usage: /scoreboard objectives add|remove|list|setdisplay");
        }
        if (!strcmp(A(1), "players")) {
            const char *op = A(2);
            if (!strcmp(op, "list")) {
                if (k->n >= 4) { const char *h[1]; holders(A(3), -1, h, 1); int n = 0; for (int o = 0; o < g_sb_nobj; o++) { int v; if (sb_get(h[0], o, &v)) { game_message(C_INFO, "[%s]: %d", g_sb_obj[o].disp, v); n++; } } cur->result = n; return true; }
                const char *h[256]; int n = holders("*", -1, h, 256); game_message(C_INFO, "There are %d tracked entities", n); cur->result = n; return true;
            }
            if ((!strcmp(op, "set") || !strcmp(op, "add") || !strcmp(op, "remove")) && k->n >= 6) {
                int o = sb_obj(A(4)); if (o < 0) return fail("Unknown scoreboard objective '%s'", A(4));
                const char *h[256]; int n = holders(A(3), o, h, 256); int val = atoi(A(5)), last = 0;
                for (int i = 0; i < n; i++) { int v = 0; sb_get(h[i], o, &v); v = !strcmp(op, "set") ? val : !strcmp(op, "add") ? v + val : v - val; sb_set(h[i], o, v); last = v; }
                if (!n) return fail("No entity was found");
                ok("Set [%s] for %s to %d", g_sb_obj[o].disp, n == 1 ? h[0] : "entities", last); cur->result = n == 1 ? last : n; return true;
            }
            if (!strcmp(op, "get") && k->n >= 5) {
                int o = sb_obj(A(4)); if (o < 0) return fail("Unknown scoreboard objective '%s'", A(4));
                const char *h[1]; if (holders(A(3), o, h, 1) < 1) return fail("No entity was found");
                int v; if (!sb_get(h[0], o, &v)) return fail("Can't get value of %s for %s; none is set", A(4), h[0]);
                game_message(C_INFO, "%s has %d [%s]", h[0], v, g_sb_obj[o].disp); cur->result = v; return true;
            }
            if (!strcmp(op, "reset") && k->n >= 4) { const char *h[256]; int o = k->n >= 5 ? sb_obj(A(4)) : -1; int n = holders(A(3), o, h, 256); for (int i = 0; i < n; i++) sb_reset(h[i], o); ok("Reset scores of %d entities", n); return true; }
            if (!strcmp(op, "operation") && k->n >= 8) {
                int o1 = sb_obj(A(4)), o2 = sb_obj(A(7)); if (o1 < 0 || o2 < 0) return fail("Unknown scoreboard objective");
                const char *t[256], *s[256]; int nt = holders(A(3), o1, t, 256), ns = holders(A(6), o2, s, 256);
                const char *ope = A(5); int last = 0;
                for (int i = 0; i < nt; i++) for (int j = 0; j < ns; j++) {
                    int a = 0, b = 0; sb_get(t[i], o1, &a); if (!sb_get(s[j], o2, &b)) continue;
                    if (!strcmp(ope, "+=")) a += b; else if (!strcmp(ope, "-=")) a -= b; else if (!strcmp(ope, "*=")) a *= b;
                    else if (!strcmp(ope, "/=")) { if (b) a = (int)floor((double)a / b); } else if (!strcmp(ope, "%=")) { if (b) { a %= b; if (a < 0) a += abs(b); } }
                    else if (!strcmp(ope, "=")) a = b; else if (!strcmp(ope, "<")) a = MINI(a, b); else if (!strcmp(ope, ">")) a = MAXI(a, b);
                    else if (!strcmp(ope, "><")) { int tmp = a; a = b; sb_set(s[j], o2, tmp); }
                    else return fail("Invalid operation '%s'", ope);
                    sb_set(t[i], o1, a); last = a;
                }
                ok("Set [%s] for %s to %d", g_sb_obj[o1].disp, nt == 1 ? t[0] : "entities", last); cur->result = last; return true;
            }
        }
        return fail("Usage: /scoreboard objectives|players ...");
    }
    // ---- /data
    if (!strcmp(c, "data") && k->n >= 3) {
        if (!strcmp(A(1), "get") && !strcmp(A(2), "entity") && k->n >= 4) {
            Entity *t[1]; if (targets(A(3), t, 1) < 1) return fail("No entity was found");
            Entity *e = t[0]; const char *p = A(4); f32 scale = k->n >= 6 ? (f32)atof(A(5)) : 1; double v = 0; char txt[160] = "";
            if (!*p) { snprintf(txt, sizeof txt, "{id:\"%s\",Health:%.1ff,Pos:[%.2fd,%dd,%.2fd],Air:%d,Fire:%d}", mob_key_of(e), e->health, e->pos.x, disp_y(e->pos.y), e->pos.z, e->air, e->fire); v = 1; }
            else if (!strcmp(p, "Health")) v = e->health; else if (!strcmp(p, "Air")) v = e->air / 3; else if (!strcmp(p, "Fire")) v = e->fire / 3;
            else if (!strcmp(p, "Pos[0]")) v = e->pos.x; else if (!strcmp(p, "Pos[1]")) v = disp_y(e->pos.y); else if (!strcmp(p, "Pos[2]")) v = e->pos.z;
            else if (!strcmp(p, "Rotation[0]")) v = -e->yaw * 180 / PI_F; else if (!strcmp(p, "Rotation[1]")) v = -e->pitch * 180 / PI_F;
            else if (!strcmp(p, "OnGround")) v = e->on_ground; else if (!strcmp(p, "XpLevel") && e->kind == ENT_PLAYER) v = g_player.xp_level;
            else if (!strcmp(p, "foodLevel") && e->kind == ENT_PLAYER) v = g_player.food; else if (!strcmp(p, "Tags")) { v = 0; for (int b = 0; b < 32; b++) v += (e->tags >> b) & 1; }
            else if (!strcmp(p, "SelectedItem") && e->kind == ENT_PLAYER) { ItemStack *h = inventory_held(&g_player.inv); snprintf(txt, sizeof txt, "{id:\"ultimatecraft:%s\",count:%d}", h->id ? ITEMS[h->id].key : "air", h->count); v = h->count; }
            else return fail("Found no elements matching %s", p);
            if (!txt[0]) snprintf(txt, sizeof txt, "%g", v);
            game_message(C_INFO, "%s has the following entity data: %s", display_name(e), txt);
            cur->result = (int)floor(v * scale); return true;
        }
        if (!strcmp(A(1), "get") && !strcmp(A(2), "block") && k->n >= 6) {
            ivec3 p; if (!bcoord3(A(3), A(4), A(5), &p)) return fail("Invalid position");
            BlockEntity *be = world_block_entity(p, false, 0); if (!be) return fail("The target block is not a block entity");
            int items = 0; for (int i = 0; i < 27; i++) items += !stack_empty(be->slots[i]);
            game_message(C_INFO, "%d, %d, %d has the following block data: {Items:%d slots}", p.x, disp_y((f32)p.y), p.z, items);
            cur->result = items; return true;
        }
        if (!strcmp(A(1), "merge") && !strcmp(A(2), "entity") && k->n >= 5) {
            Entity *t[256]; int n = targets(A(3), t, 256); if (n < 1) return fail("No entity was found");
            const char *nbt = k->rest[4]; const char *q;
            for (int i = 0; i < n; i++) {
                if ((q = strstr(nbt, "Health:"))) t[i]->health = fminf(t[i]->max_health, (f32)atof(q + 7));
                if ((q = strstr(nbt, "Fire:"))) t[i]->fire = atoi(q + 5) * MC_TICK_DIV;
                if ((q = strstr(nbt, "Air:"))) t[i]->air = atoi(q + 4) * MC_TICK_DIV;
                if ((q = strstr(nbt, "PersistenceRequired:"))) t[i]->persistent = atoi(q + 20) != 0 || !strncmp(q + 20, "1b", 2) || !strncmp(q + 20, "true", 4);
                if ((q = strstr(nbt, "NoGravity:"))) t[i]->no_gravity = atoi(q + 10) != 0 || !strncmp(q + 10, "true", 4);
            }
            ok("Modified entity data of %d entities", n); cur->result = n; return true;
        }
        return fail("Supported: /data get entity|block, /data merge entity");
    }
    // ---- /attribute <target> <attribute> get [scale] | base get | base set <value>
    if (!strcmp(c, "attribute") && k->n >= 4) {
        Entity *t[1]; if (targets(A(1), t, 1) < 1) return fail("No entity was found");
        Entity *e = t[0]; const char *at = strip_ns(A(2)); if (!strncmp(at, "generic.", 8)) at += 8;
        const MobDef *d = e->kind == ENT_MOB ? &MOB_DEFS[e->mob] : NULL;
        double v;
        if (!strcmp(at, "max_health")) v = e->max_health;
        else if (!strcmp(at, "movement_speed")) v = d ? d->speed / 43.17 : 0.1;
        else if (!strcmp(at, "attack_damage")) v = d ? d->attack : 1;
        else if (!strcmp(at, "armor")) v = d ? d->armor : 0;
        else if (!strcmp(at, "knockback_resistance")) v = d ? d->kb_resist : 0;
        else if (!strcmp(at, "luck")) v = effect_level(e, EFF_LUCK);
        else return fail("Unsupported attribute '%s'", A(2));
        if (!strcmp(A(3), "base") && !strcmp(A(4), "set") && k->n >= 6) {
            if (strcmp(at, "max_health")) return fail("Only max_health can be changed in UltimateCraft");
            e->max_health = fmaxf(1, (f32)atof(A(5))); e->health = fminf(e->health, e->max_health);
            ok("Set base value for attribute %s for %s to %g", A(2), display_name(e), (double)e->max_health); cur->result = (int)e->max_health; return true;
        }
        f32 scale = !strcmp(A(3), "get") && k->n >= 5 ? (f32)atof(A(4)) : 1;
        game_message(C_INFO, "Value of attribute %s for %s is %g", A(2), display_name(e), v);
        cur->result = (int)floor(v * scale); return true;
    }
    // ---- /worldborder
    if (!strcmp(c, "worldborder") && k->n >= 2) {
        const char *s = A(1);
        if (!strcmp(s, "get")) { game_message(C_INFO, "The world border is currently %.0f block(s) wide", g_wb.size); cur->result = (int)g_wb.size; return true; }
        if ((!strcmp(s, "set") || !strcmp(s, "add")) && k->n >= 3) {
            f64 v = atof(A(2)); if (!strcmp(s, "add")) v += g_wb.target;
            if (v < 1 || v > 59999968) return fail("The world border cannot be smaller than 1 block wide");
            int secs = k->n >= 4 ? atoi(A(3)) : 0;
            g_wb.target = v; if (secs > 0) g_wb.lerp = secs * TICKRATE; else { g_wb.size = v; g_wb.lerp = 0; }
            ok("Set the world border to %.0f block(s) wide%s", v, secs > 0 ? " (over time)" : ""); return true;
        }
        if (!strcmp(s, "center") && k->n >= 4) { g_wb.cx = atof(A(2)); g_wb.cz = atof(A(3)); ok("Set the center of the world border to %.1f, %.1f", g_wb.cx, g_wb.cz); return true; }
        if (!strcmp(s, "damage") && k->n >= 4) { if (!strcmp(A(2), "amount")) g_wb.damage = (f32)atof(A(3)); else g_wb.buffer = (f32)atof(A(3)); ok("Set the world border damage %s to %s", A(2), A(3)); return true; }
        if (!strcmp(s, "warning") && k->n >= 4) { g_wb.warning = (f32)atof(A(3)); ok("Set the world border warning %s to %s", A(2), A(3)); return true; }
        return fail("Usage: /worldborder get|set|add|center|damage|warning");
    }
    // ---- /spreadplayers <x> <z> <spreadDistance> <maxRange> [under <maxHeight>] <respectTeams> <targets>
    if (!strcmp(c, "spreadplayers") && k->n >= 7) {
        f32 cx = (f32)atof(A(1)), cz = (f32)atof(A(2)), sd = (f32)atof(A(3)), mr = (f32)atof(A(4));
        int ti = !strcmp(A(5), "under") ? 8 : 6;
        Entity *t[256]; int n = targets(A(ti), t, 256); if (n < 1) return fail("No entity was found");
        vec3 placed[256]; int np = 0;
        for (int i = 0; i < n; i++) {
            for (int tries = 0; tries < 100; tries++) {
                f32 x = cx + frand_range(-mr, mr), z = cz + frand_range(-mr, mr);
                bool close = false; for (int j = 0; j < np; j++) close |= v3_dist(v3(x, 0, z), v3(placed[j].x, 0, placed[j].z)) < sd;
                if (close && tries < 99) continue;
                int top = world_top_y((int)floorf(x), (int)floorf(z)); if (top < 0) continue;
                Block g = world_get_block((int)floorf(x), top, (int)floorf(z)); if (BLOCKS[g].liquid && tries < 99) continue;
                t[i]->pos = t[i]->prev_pos = v3(floorf(x) + 0.5f, top + 1.0f, floorf(z) + 0.5f); t[i]->vel = v3(0, 0, 0);
                placed[np++] = t[i]->pos; break;
            }
        }
        ok("Spread %d entities around %.1f, %.1f", n, cx, cz); cur->result = n; return true;
    }
    // ---- /fillbiome <from> <to> <biome>
    if (!strcmp(c, "fillbiome") && k->n >= 8) {
        ivec3 a, b; if (!bcoord3(A(1), A(2), A(3), &a) || !bcoord3(A(4), A(5), A(6), &b)) return fail("Invalid position");
        int bio = biome_by_key(strip_ns(A(7))); if (bio < 0) return fail("Unknown biome '%s'", A(7));
        int x0 = MINI(a.x, b.x), x1 = MAXI(a.x, b.x), z0 = MINI(a.z, b.z), z1 = MAXI(a.z, b.z), n = 0;
        if ((long)(x1 - x0 + 1) * (z1 - z0 + 1) > 32768) return fail("Too many columns in the specified area (maximum 32768)");
        for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++) {
            Column *col = world_column_at(x, z); if (!col) continue;
            col->biome[z & 15][x & 15] = (u8)bio; col->modified = true; n++;
        }
        for (int z = z0 - 16; z <= z1 + 16; z += 16) for (int x = x0 - 16; x <= x1 + 16; x += 16) for (int y = 0; y < WORLD_H; y += 16) world_mark_dirty(x, y, z);
        ok("%d biome entries set", n); cur->result = n; return true;
    }
    // ---- /forceload add|remove|query
    if (!strcmp(c, "forceload") && k->n >= 2) {
        Entity *p = player_ent();
        if (!strcmp(A(1), "query")) {
            if (k->n >= 4) { int cx = (int)floorf(A(2)[0] == '~' ? (p ? p->pos.x : 0) + (f32)atof(A(2) + 1) : (f32)atof(A(2))) >> 4, cz = (int)floorf(A(3)[0] == '~' ? (p ? p->pos.z : 0) + (f32)atof(A(3) + 1) : (f32)atof(A(3))) >> 4; bool f = forceload_has(cx, cz); game_message(C_INFO, f ? "Chunk at [%d, %d] is marked for force loading" : "Chunk at [%d, %d] is not marked for force loading", cx, cz); cur->result = f; return true; }
            game_message(C_INFO, "%d force loaded chunk(s) in %s", nforced, dim_info(g_world.dim)->id); cur->result = nforced; return true;
        }
        bool add = !strcmp(A(1), "add");
        if (!add && strcmp(A(1), "remove")) return fail("Usage: /forceload add|remove|query");
        if (!add && !strcmp(A(2), "all")) { nforced = 0; forceload_save(g_world.dim_dir); ok("Unmarked all force loaded chunks"); return true; }
        f32 px = p ? p->pos.x : 0, pz = p ? p->pos.z : 0;
        #define RELC(s, base) ((s)[0] == '~' ? (base) + (f32)atof((s) + 1) : (f32)atof(s))
        f32 x0 = k->n >= 4 ? RELC(A(2), px) : px, z0 = k->n >= 4 ? RELC(A(3), pz) : pz;
        f32 x1 = k->n >= 6 ? RELC(A(4), px) : x0, z1 = k->n >= 6 ? RELC(A(5), pz) : z0;
        #undef RELC
        int n = 0;
        for (int cz = (int)floorf(fminf(z0, z1)) >> 4; cz <= (int)floorf(fmaxf(z0, z1)) >> 4; cz++) for (int cx = (int)floorf(fminf(x0, x1)) >> 4; cx <= (int)floorf(fmaxf(x0, x1)) >> 4; cx++) {
            bool has = forceload_has(cx, cz);
            if (add && !has && nforced < 256) { forced[nforced++] = iv3(cx, 0, cz); n++; }
            if (!add && has) { for (int i = 0; i < nforced; i++) if (forced[i].x == cx && forced[i].z == cz) forced[i--] = forced[--nforced]; n++; }
        }
        forceload_save(g_world.dim_dir);
        ok("%s %d chunk(s)", add ? "Marked for force loading:" : "Unmarked:", n); cur->result = n; return true;
    }
    // ---- /item replace entity <t> <slot> with <item> [count] | block <x y z> container.N with <item> [count]
    if (!strcmp(c, "item") && !strcmp(A(1), "replace") && k->n >= 7) {
        bool blk = !strcmp(A(2), "block");
        int si = blk ? 6 : 4, wi = si + 1;
        if (strcmp(A(wi), "with")) return fail("Usage: /item replace entity <targets> <slot> with <item> [count]");
        int id = item_named(A(wi + 1)); if (id <= 0 && strcmp(strip_ns(A(wi + 1)), "air")) return fail("Unknown item '%s'", A(wi + 1));
        int cnt = k->n > wi + 2 ? atoi(A(wi + 2)) : 1;
        ItemStack st = id > 0 ? stack_of(id, MINI(cnt, ITEMS[id].max_stack)) : (ItemStack){0};
        int slot = slot_index(A(si)); if (slot < 0) return fail("Unknown slot '%s'", A(si));
        if (blk) {
            ivec3 p; if (!bcoord3(A(3), A(4), A(5), &p)) return fail("Invalid position");
            BlockEntity *be = world_block_entity(p, false, 0); if (!be || slot < 300 || slot - 300 >= 27) return fail("Target block is not a container");
            be->slots[slot - 300] = st; ok("Replaced a slot at %d, %d, %d with %s", p.x, disp_y((f32)p.y), p.z, id ? ITEMS[id].name : "air"); return true;
        }
        Entity *t[256]; int n = targets(A(3), t, 256); if (n < 1) return fail("No entity was found");
        for (int i = 0; i < n; i++) {
            Entity *e = t[i];
            if (e->kind == ENT_PLAYER) {
                if (slot == 100) *inventory_held(&g_player.inv) = st; else if (slot == 101) g_player.inv.offhand = st;
                else if (slot >= 200 && slot < 204) g_player.inv.armor[slot - 200] = st; else if (slot < 36) g_player.inv.slots[slot] = st;
            } else if (e->kind == ENT_DECOR && slot >= 200 && slot < 204) e->equip[slot - 200] = st;
            else if (e->kind == ENT_DECOR || slot == 100) e->item = st;
        }
        ok("Replaced a slot on %d entities with %s", n, id ? ITEMS[id].name : "air"); cur->result = n; return true;
    }
    // ---- /loot give <targets> loot <table> | spawn <x y z> loot <table> | insert <x y z> loot <table>
    if (!strcmp(c, "loot") && k->n >= 5) {
        int li = !strcmp(A(1), "give") ? 3 : 5;
        if (strcmp(A(li), "loot")) return fail("Only the 'loot <table>' source is supported");
        int table = loot_by_name(A(li + 1)); if (table < 0) return fail("Unknown loot table '%s'", A(li + 1));
        ItemStack slots[27]; memset(slots, 0, sizeof slots); loot_fill(slots, 27, table);
        int n = 0;
        if (!strcmp(A(1), "give")) { for (int i = 0; i < 27; i++) if (!stack_empty(slots[i])) { player_give(slots[i]); n++; } }
        else {
            vec3 p; if (!coord3(A(2), A(3), A(4), &p, true)) return fail("Invalid position");
            if (!strcmp(A(1), "insert")) { BlockEntity *be = world_block_entity(v3_floor(p), false, 0); if (!be) return fail("Target is not a container"); for (int i = 0; i < 27; i++) if (!stack_empty(slots[i])) for (int j = 0; j < 27; j++) if (stack_empty(be->slots[j])) { be->slots[j] = slots[i]; n++; break; } }
            else for (int i = 0; i < 27; i++) if (!stack_empty(slots[i])) { item_drop(p, slots[i], v3(frand_range(-1, 1), 2, frand_range(-1, 1)), 10); n++; }
        }
        ok("Dropped %d stack(s) from %s", n, LOOT_NAMES[table]); cur->result = n; return true;
    }
    // ---- /function, /reload, /datapack list
    if (!strcmp(c, "function") && k->n >= 2) {
        if (!npacks) datapacks_scan();
        fn_budget = rule_i(GR_MAX_COMMAND_CHAIN_LENGTH);
        int r = function_run(A(1), 0); g_fn_runs++;
        if (r < 0) return fail("Unknown function '%s'", A(1));
        ok("Executed %d command(s) from function '%s'", r, A(1)); cur->result = r; return true;
    }
    if (!strcmp(c, "reload")) { dp_world[0] = 0; datapacks_tick(); ok("Reloading! %d data pack(s)", npacks); return true; }
    if (!strcmp(c, "datapack") && !strcmp(A(1), "list")) { if (!npacks) datapacks_scan(); game_message(C_INFO, "There are %d data pack(s) enabled", npacks); for (int i = 0; i < npacks; i++) game_message(C_INFO, " %s", packs[i]); cur->result = npacks; return true; }
    *handled = false;
    return false;
    #undef A
}

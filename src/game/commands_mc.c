// Minecraft (Java Edition 1.21) command syntax: target selectors (@p @a @r @s @e @n with arguments),
// relative (~) and local (^) coordinates, /execute, /gamerule, /difficulty, /weather, /time, /fill,
// /clone, /setblock, /give, /clear, /summon, /kill, /tp, /effect, /enchant, /experience, /title,
// /tag, /damage, /ride, /tick and more. Forms the original test scripts rely on (for example
// "give <item>", "summon <mob> <count> <distance>", "fill <block>") are passed back to commands.c.
// Coordinates typed by the player are Minecraft coordinates: y is converted with the dimension offset.
#include "game.h"
#include "player.h"
#include "fx.h"
#include "weather.h"
#include "raids.h"
#include "../world/rules.h"
#include "../world/structures.h"
#include "../entity/vehicle.h"
#include "../audio/audio.h"
#include <ctype.h>
#include <stdarg.h>

#define C_OK rgba_u32(170, 170, 170, 255)
#define C_ERR rgba_u32(255, 85, 85, 255)
#define C_INFO COL_WHITE

// ------------------------------------------------------------------ execution context
typedef struct {
    Entity *self;          // @s (NULL = the server)
    vec3 pos;              // execution position (internal coordinates)
    f32 yaw, pitch;        // radians, engine convention
    int depth;
    int result;            // success count of the last command
} Ctx;

static Ctx *cur;
static int fb_suppress;
static void ok(const char *fmt, ...) {
    if (!rule(GR_SEND_COMMAND_FEEDBACK) || fb_suppress) return;
    char b[256]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    game_message(C_OK, "%s", b);
}
static bool fail(const char *fmt, ...) {
    char b[256]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
    if (!fb_suppress) game_message(C_ERR, "%s", b);
    if (cur) cur->result = 0;
    return true;
}

// ------------------------------------------------------------------ tokenizer (keeps [..] {..} "..")
#define MAXTOK 48
typedef struct { char t[MAXTOK][192]; int n; const char *rest[MAXTOK]; } Toks;
static void tokenize(const char *s, Toks *k) {
    k->n = 0;
    while (*s && k->n < MAXTOK) {
        while (*s == ' ') s++;
        if (!*s) break;
        k->rest[k->n] = s;
        int depth = 0, len = 0; bool q = false;
        char *o = k->t[k->n];
        while (*s && (q || depth > 0 || *s != ' ')) {
            if (*s == '"' && (len == 0 || s[-1] != '\\')) q = !q;
            else if (!q && (*s == '[' || *s == '{')) depth++;
            else if (!q && (*s == ']' || *s == '}')) depth--;
            if (len < 191) o[len++] = *s;
            s++;
        }
        o[len] = 0;
        k->n++;
    }
}
static const char *strip_ns(const char *s) { return !strncmp(s, "minecraft:", 10) ? s + 10 : s; }
static bool is_num(const char *s) { char *e; if (!*s) return false; strtod(s, &e); return *e == 0; }
static void unquote(char *s) { size_t n = strlen(s); if (n >= 2 && s[0] == '"' && s[n - 1] == '"') { memmove(s, s + 1, n - 2); s[n - 2] = 0; } }

// ------------------------------------------------------------------ registries
static void compact(const char *in, char *out, size_t n) {
    size_t k = 0;
    for (; *in && k + 1 < n; in++) if (*in != ' ' && *in != '_' && *in != '-' && *in != '\'') out[k++] = (char)tolower((unsigned char)*in);
    out[k] = 0;
}
static int effect_by_name(const char *nm) {
    char w[64], h[64]; compact(strip_ns(nm), w, sizeof w);
    for (int i = 1; i < EFFECT_COUNT; i++) { compact(EFFECT_NAMES[i], h, sizeof h); if (!strcmp(h, w)) return i; }
    return 0;
}
static int ench_by_name(const char *nm) {
    nm = strip_ns(nm);
    for (int i = 1; i < ENCH_COUNT; i++) if (!strcmp(ENCHANTS[i].key, nm)) return i;
    char w[64], h[64]; compact(nm, w, sizeof w);
    for (int i = 1; i < ENCH_COUNT; i++) { compact(ENCHANTS[i].name, h, sizeof h); if (!strcmp(h, w)) return i; }
    return 0;
}
static int item_named(const char *nm) {
    char b[192]; snprintf(b, sizeof b, "%s", strip_ns(nm));
    char *br = strchr(b, '['); if (br) *br = 0;   // item components are accepted but ignored
    br = strchr(b, '{'); if (br) *br = 0;
    return item_by_key(b);
}
static int block_named(const char *nm) {
    char b[192]; snprintf(b, sizeof b, "%s", strip_ns(nm));
    int var = 0;
    char *br = strchr(b, '['); if (br) *br = 0;   // block states: only the numeric ":n" variant form is understood
    char *colon = strrchr(b, ':');
    if (colon && isdigit((unsigned char)colon[1])) { var = atoi(colon + 1); *colon = 0; }
    if (!strcmp(b, "air") || !strcmp(b, "cave_air") || !strcmp(b, "void_air")) return 0;
    Block k = block_by_key(b);
    return k ? block_with_var(k, var) : -1;
}
static const char *mob_key_of(const Entity *e) {
    switch (e->kind) {
    case ENT_PLAYER: return "player"; case ENT_ITEM: return "item"; case ENT_XP: return "experience_orb";
    case ENT_ARROW: return "arrow"; case ENT_TNT: return "tnt"; case ENT_CRYSTAL: return "end_crystal";
    case ENT_VEHICLE: return "boat"; case ENT_DECOR: return decor_key(e); case ENT_MOB: return MOB_DEFS[e->mob].key; default: return "unknown";
    }
}
static const char *display_name(const Entity *e) { return e->kind == ENT_PLAYER ? "Player" : e->kind == ENT_MOB ? MOB_DEFS[e->mob].name : mob_key_of(e); }

// entity tags (/tag): up to 32 distinct names
static char tag_names[32][32];
static int tag_bit(const char *n, bool create) {
    for (int i = 0; i < 32; i++) if (tag_names[i][0] && !strcmp(tag_names[i], n)) return i;
    if (!create) return -1;
    for (int i = 0; i < 32; i++) if (!tag_names[i][0]) { snprintf(tag_names[i], 32, "%s", n); return i; }
    return -1;
}

int tag_count(const char *n) { int b = tag_bit(n, false), c = 0; if (b < 0) return 0; ENT_FOREACH(e) if (e->tags >> b & 1 && !(e->kind == ENT_MOB && e->death_time)) c++; return c; }
// ------------------------------------------------------------------ coordinates
// One axis: "~1.5" relative, "^2" local, "5" absolute. axis 1 = y (Minecraft -> internal offset).
static bool coord3(const char *a, const char *b, const char *c, vec3 *out, bool block_center) {
    const char *t[3] = {a, b, c};
    if (!a || !b || !c) return false;
    bool local = t[0][0] == '^';
    for (int i = 0; i < 3; i++) if ((t[i][0] == '^') != local) return false;
    if (local) {   // ^left ^up ^forward relative to the executor's rotation
        f32 l = (f32)atof(a + 1), u = (f32)atof(b + 1), f = (f32)atof(c + 1);
        f32 yaw = cur->yaw, pitch = cur->pitch;
        vec3 fwd = v3(sinf(yaw) * cosf(pitch), sinf(pitch), cosf(yaw) * cosf(pitch));
        vec3 left = v3(cosf(yaw), 0, -sinf(yaw));
        vec3 up = v3_cross(fwd, left); up = v3_scale(up, -1);
        vec3 eye = cur->self ? entity_eye(cur->self) : cur->pos;
        (void)eye;
        *out = v3_add(cur->pos, v3_add(v3_scale(left, l), v3_add(v3_scale(up, u), v3_scale(fwd, f))));
        return true;
    }
    f32 base[3] = {cur->pos.x, cur->pos.y, cur->pos.z}, v[3];
    for (int i = 0; i < 3; i++) {
        if (t[i][0] == '~') v[i] = base[i] + (f32)atof(t[i] + 1);
        else if (is_num(t[i])) {
            v[i] = (f32)atof(t[i]);
            if (i == 1) v[i] += (f32)world_y_offset();
            if (block_center && i != 1 && !strchr(t[i], '.')) v[i] += 0.5f;
        } else return false;
    }
    *out = v3(v[0], v[1], v[2]);
    return true;
}
static bool bcoord3(const char *a, const char *b, const char *c, ivec3 *out) {
    vec3 v; if (!coord3(a, b, c, &v, false)) return false;
    *out = v3_floor(v); return true;
}
static int disp_y(f32 y) { return (int)floorf(y) - world_y_offset(); }

// ------------------------------------------------------------------ target selectors
#define MAXSEL MAX_ENTITIES
static Entity *sel[MAXSEL];
typedef struct { Entity **e; int n; bool single; } Sel;

static bool range_ok(const char *spec, f32 v) {   // "5", "..5", "2..", "2..5"
    const char *dots = strstr(spec, "..");
    if (!dots) return fabsf(v - (f32)atof(spec)) < 0.5f;
    f32 lo = dots == spec ? -1e30f : (f32)atof(spec), hi = dots[2] ? (f32)atof(dots + 2) : 1e30f;
    return v >= lo && v <= hi;
}
static f32 dist_from(vec3 a, Entity *e) { return v3_dist(a, e->pos); }
static int cmp_dist_base; static vec3 cmp_origin;
static int cmp_near(const void *a, const void *b) { f32 da = dist_from(cmp_origin, *(Entity **)a), db = dist_from(cmp_origin, *(Entity **)b); return (da > db) - (da < db); }

// Resolves a selector or a name. Returns false (with an error) when the syntax is not a target.
static bool resolve(const char *s, Sel *out) {
    out->e = sel; out->n = 0; out->single = false;
    Entity *pl = player_ent();
    if (!s || !*s) return false;
    if (s[0] != '@') {   // player name / UUID-ish: the only player is "Player" (any non-selector word naming the player)
        if (!strcasecmp(s, "Player") || !strcasecmp(s, "Dev") || !strcasecmp(s, "Steve") || !strcasecmp(s, "Alex")) { if (pl) sel[out->n++] = pl; out->single = true; return true; }
        return false;
    }
    char kind = s[1];
    if (!strchr("parsen", kind) || !kind) return false;
    // defaults
    bool players_only = kind == 'p' || kind == 'a' || kind == 'r';
    int limit = (kind == 'p' || kind == 'r' || kind == 'n' || kind == 's') ? 1 : 1 << 30;
    int sort = kind == 'p' || kind == 'n' ? 1 : kind == 'r' ? 3 : 0;   // 0 arbitrary 1 nearest 2 furthest 3 random
    vec3 origin = cur->pos;
    const char *dist = NULL, *lvl = NULL;
    char type[64] = ""; bool type_not = false, has_type = false;
    char name[64] = ""; bool name_not = false, has_name = false;
    char tags[8][32]; bool tag_not[8]; int ntags = 0;
    int gm = -1; bool gm_not = false;
    f32 vx = NAN, vy = NAN, vz = NAN;
    bool has_x = false, has_y = false, has_z = false;
    const char *br = strchr(s, '[');
    char args[512] = "";
    if (br) { snprintf(args, sizeof args, "%s", br + 1); char *e = strrchr(args, ']'); if (e) *e = 0; }
    for (char *a = strtok(args, ","); a; a = strtok(NULL, ",")) {
        while (*a == ' ') a++;
        char *eq = strchr(a, '='); if (!eq) continue;
        *eq = 0; char *v = eq + 1; while (*v == ' ') v++;
        bool neg = *v == '!'; if (neg) v++;
        if (!strcmp(a, "type")) { snprintf(type, sizeof type, "%s", strip_ns(v)); type_not = neg; has_type = true; }
        else if (!strcmp(a, "name")) { snprintf(name, sizeof name, "%s", v); unquote(name); name_not = neg; has_name = true; }
        else if (!strcmp(a, "distance")) { static char d[32]; snprintf(d, sizeof d, "%s", v); dist = d; }
        else if (!strcmp(a, "level")) { static char l[32]; snprintf(l, sizeof l, "%s", v); lvl = l; }
        else if (!strcmp(a, "limit")) limit = MAXI(1, atoi(v));
        else if (!strcmp(a, "sort")) sort = !strcmp(v, "nearest") ? 1 : !strcmp(v, "furthest") ? 2 : !strcmp(v, "random") ? 3 : 0;
        else if (!strcmp(a, "tag") && ntags < 8) { snprintf(tags[ntags], 32, "%s", v); tag_not[ntags++] = neg; }
        else if (!strcmp(a, "gamemode")) { gm = !strcmp(v, "creative") ? 1 : !strcmp(v, "adventure") ? 2 : !strcmp(v, "spectator") ? 3 : 0; gm_not = neg; }
        else if (!strcmp(a, "x")) { origin.x = (f32)atof(v); has_x = true; }
        else if (!strcmp(a, "y")) { origin.y = (f32)atof(v) + world_y_offset(); has_y = true; }
        else if (!strcmp(a, "z")) { origin.z = (f32)atof(v); has_z = true; }
        else if (!strcmp(a, "dx")) vx = (f32)atof(v);
        else if (!strcmp(a, "dy")) vy = (f32)atof(v);
        else if (!strcmp(a, "dz")) vz = (f32)atof(v);
    }
    (void)has_x; (void)has_y; (void)has_z;
    if (kind == 's') {
        if (cur->self && (!has_type || (!strcmp(mob_key_of(cur->self), type) != type_not))) sel[out->n++] = cur->self;
        out->single = true;
        return true;
    }
    ENT_FOREACH(e) {
        if (e->kind == ENT_PLAYER && g_player.dead) continue;
        if (e->kind == ENT_MOB && e->death_time > 0) continue;
        if (players_only && e->kind != ENT_PLAYER) continue;
        if (has_type && (strcmp(mob_key_of(e), type) == 0) == type_not) continue;
        if (has_name && (strcasecmp(display_name(e), name) == 0) == name_not) continue;
        if (dist && !range_ok(dist, dist_from(origin, e))) continue;
        if (lvl && (e->kind != ENT_PLAYER || !range_ok(lvl, (f32)g_player.xp_level))) continue;
        if (gm >= 0 && (e->kind != ENT_PLAYER || (g_player.gamemode == gm) == gm_not)) continue;
        if (!isnan(vx) || !isnan(vy) || !isnan(vz)) {
            AABB box = {v3(origin.x + fminf(0, isnan(vx) ? 0 : vx), origin.y + fminf(0, isnan(vy) ? 0 : vy), origin.z + fminf(0, isnan(vz) ? 0 : vz)),
                        v3(origin.x + fmaxf(0, isnan(vx) ? 0 : vx) + 1, origin.y + fmaxf(0, isnan(vy) ? 0 : vy) + 1, origin.z + fmaxf(0, isnan(vz) ? 0 : vz) + 1)};
            if (!aabb_overlap(box, entity_aabb(e))) continue;
        }
        bool tok = true;
        for (int i = 0; i < ntags && tok; i++) {
            if (!tags[i][0]) { tok = (e->tags != 0) == tag_not[i]; continue; }   // tag= (no tags) / tag=! (any tag)
            int bit = tag_bit(tags[i], false);
            bool has = bit >= 0 && (e->tags >> bit & 1);
            tok = has != tag_not[i];
        }
        if (!tok) continue;
        if (kind == 'p' && e->kind != ENT_PLAYER) continue;
        sel[out->n++] = e;
    }
    cmp_origin = origin; (void)cmp_dist_base;
    if (sort == 1 || sort == 2) qsort(sel, out->n, sizeof(Entity *), cmp_near);
    if (sort == 2) for (int i = 0; i < out->n / 2; i++) { Entity *t = sel[i]; sel[i] = sel[out->n - 1 - i]; sel[out->n - 1 - i] = t; }
    if (sort == 3) for (int i = out->n - 1; i > 0; i--) { int j = irand(0, i); Entity *t = sel[i]; sel[i] = sel[j]; sel[j] = t; }
    if (out->n > limit) out->n = limit;
    out->single = limit == 1;
    return true;
}
// convenience: resolve a selector that must match something; copies results (selectors nest in /execute)
static int targets(const char *s, Entity **dst, int cap) {
    Sel r;
    if (!resolve(s, &r)) return -1;
    int n = MINI(r.n, cap);
    memcpy(dst, r.e, sizeof(Entity *) * n);
    return n;
}
static bool is_target(const char *s) { return s && (s[0] == '@' || !strcasecmp(s, "Player")); }

// ------------------------------------------------------------------ time units
static int parse_time_mc(const char *s) {   // "100", "100t", "5s", "2d" -> Minecraft ticks
    f32 v = (f32)atof(s);
    size_t n = strlen(s);
    char u = n ? s[n - 1] : 't';
    if (u == 's') v *= 20; else if (u == 'd') v *= 24000;
    return (int)v;
}

// ------------------------------------------------------------------ individual commands
static bool cmd_execute_line(const char *line);

static void give_to(Entity *e, int id, int count, int potion) {
    (void)e;
    while (count > 0) { int k = MINI(count, ITEMS[id].max_stack); ItemStack s = stack_of(id, k); s.damage = (u16)potion; player_give(s); count -= k; }
}

static int clear_items(int id, int max) {   // removes up to max (-1 all) of item id (0 any)
    int removed = 0;
    ItemStack *all[INV_MAIN + 6]; int n = 0;
    for (int i = 0; i < INV_MAIN; i++) all[n++] = &g_player.inv.slots[i];
    for (int i = 0; i < 4; i++) all[n++] = &g_player.inv.armor[i];
    all[n++] = &g_player.inv.offhand; all[n++] = &g_player.cursor;
    for (int i = 0; i < n; i++) {
        ItemStack *s = all[i];
        if (stack_empty(*s) || (id && s->id != id)) continue;
        int take = max < 0 ? s->count : MINI(s->count, max - removed);
        if (max == 0) { removed += s->count; continue; }   // maxCount 0: just count
        s->count -= take; removed += take;
        if (s->count <= 0) *s = (ItemStack){0};
        if (max > 0 && removed >= max) break;
    }
    return removed;
}

static Entity *summon_at(const char *type, vec3 p, f32 yaw) {
    type = strip_ns(type);
    if (!strcmp(type, "lightning_bolt")) { lightning_strike(p, false); return NULL; }
    if (!strcmp(type, "tnt")) return tnt_spawn(p, 80 * MC_TICK_DIV);
    if (!strcmp(type, "end_crystal")) return crystal_spawn(p);
    if (!strcmp(type, "experience_orb")) { xp_drop(p, 1); return NULL; }
    if (!strcmp(type, "item")) return item_drop(p, stack_of(I_STONE, 1), v3(0, 0, 0), 10);
    int m = mob_by_key(type);
    if (m >= 0) { Entity *e = mob_spawn(m, p); if (e) { e->persistent = true; e->yaw = e->body_yaw = e->head_yaw = yaw; } return e; }
    int vit = item_by_key(type);
    int vt = vit ? vehicle_type_of_item(vit) : -1;
    if (vt >= 0) { Entity *v = vehicle_spawn(vt, p, yaw); if (v) v->item = stack_of(vit, 1); return v; }
    return NULL;
}

static void set_gamemode(int gm) {
    g_player.gamemode = gm;
    if (gm == GM_SURVIVAL || gm == GM_ADVENTURE) g_player.flying = false;
    if (gm == GM_SPECTATOR) g_player.flying = true;
    Entity *p = player_ent(); if (p) p->no_clip = gm == GM_SPECTATOR;
}
static const char *GM_NAMES[4] = {"Survival", "Creative", "Adventure", "Spectator"};
static int gm_parse(const char *s) {
    if (!strcmp(s, "survival") || !strcmp(s, "s") || !strcmp(s, "0")) return GM_SURVIVAL;
    if (!strcmp(s, "creative") || !strcmp(s, "c") || !strcmp(s, "1")) return GM_CREATIVE;
    if (!strcmp(s, "adventure") || !strcmp(s, "a") || !strcmp(s, "2")) return GM_ADVENTURE;
    if (!strcmp(s, "spectator") || !strcmp(s, "sp") || !strcmp(s, "3")) return GM_SPECTATOR;
    return -1;
}

static void place(int x, int y, int z, int blk, int mode_destroy) {
    if (y < 0 || y >= WORLD_H) return;
    if (mode_destroy) { u16 old = world_get_block(x, y, z); if (!block_is_air(old)) { fx_block_break(iv3(x, y, z), old); game_block_drops(iv3(x, y, z), old, NULL); } }
    if (BLOCKS[world_get_block(x, y, z)].be) world_remove_block_entity(iv3(x, y, z));
    world_set_block(x, y, z, (u16)blk);
}

// /fill and /clone volume, like Minecraft's commandModificationBlockLimit
static bool volume_ok(ivec3 a, ivec3 b, long *vol) {
    *vol = (long)(abs(a.x - b.x) + 1) * (abs(a.y - b.y) + 1) * (abs(a.z - b.z) + 1);
    if (*vol > rule_i(GR_COMMAND_MODIFICATION_LIMIT)) { fail("Too many blocks in the specified area (maximum %d, specified %ld)", rule_i(GR_COMMAND_MODIFICATION_LIMIT), *vol); return false; }
    return true;
}

static void help(void) {
    static const char *L[] = {
        "/advancement /attribute (n/a)  /clear [targets] [item] [max]  /clone <from> <to> <dest> [replace|masked] [force|move|normal]",
        "/damage <target> <amount> [type]  /defaultgamemode <mode>  /difficulty [level]  /effect give|clear <targets> ...",
        "/enchant <targets> <enchantment> [level]  /execute as|at|positioned|facing|rotated|if|unless|run ...",
        "/experience|xp add|set|query <targets> <n> [levels|points]  /fill <from> <to> <block> [destroy|hollow|keep|outline|replace]",
        "/gamemode <mode> [target]  /gamerule <rule> [value]  /give <targets> <item> [count]  /help  /kill [targets]  /list",
        "/locate structure|biome <id> [tp]  /me <action>  /msg|tell|w <target> <msg>  /particle <name> [pos]  /playsound",
        "/raid start|stop|status  /ride <target> mount|dismount  /say <msg>  /seed  /setblock <pos> <block> [destroy|keep|replace]",
        "/setworldspawn [pos]  /spawnpoint [targets] [pos]  /summon <entity> [pos]  /tag <targets> add|remove|list [name]",
        "/tellraw <targets> <json>  /tick query|rate|freeze|unfreeze|step  /time set|add|query  /title <targets> title|subtitle|actionbar|clear|reset",
        "/tp|teleport [targets] <pos|target> [yaw pitch]  /weather clear|rain|thunder [duration]",
    };
    for (size_t i = 0; i < ARRAY_LEN(L); i++) game_message(rgba_u32(255, 255, 85, 255), "%s", L[i]);
}

static const char *TEXT_JSON_SKIP = "{}[]\"";
static void json_text(const char *in, char *out, size_t n) {   // pulls "text":"..." values out of a JSON text component
    size_t k = 0;
    const char *p = in;
    bool any = false;
    while ((p = strstr(p, "\"text\"")) != NULL) {
        p = strchr(p + 6, '"'); if (!p) break; p++;
        while (*p && *p != '"' && k + 1 < n) out[k++] = *p++;
        any = true;
    }
    if (!any) { for (p = in; *p && k + 1 < n; p++) if (!strchr(TEXT_JSON_SKIP, *p)) out[k++] = *p; }
    out[k] = 0;
}

// ------------------------------------------------------------------ dispatcher
static bool run(Toks *k, bool *handled);
#include "commands_ext.inc"
static bool run(Toks *k, bool *handled) {
    *handled = true;
    { bool h2 = false; cur->result = 1; if (run_ext(k, &h2) && h2) return true; }
    const char *c = k->t[0];
    if (c[0] == '/') c++;
    #define A(i) (k->n > (i) ? k->t[i] : "")
    #define REST(i) (k->n > (i) ? k->rest[i] : "")
    cur->result = 1;

    if (!strcmp(c, "help") || !strcmp(c, "?")) { help(); return true; }
    if (!strcmp(c, "seed")) { game_message(C_INFO, "Seed: [%lld]", (long long)g_world.seed); return true; }
    if (!strcmp(c, "list")) { game_message(C_INFO, "There are 1 of a max of 1 players online: Player"); return true; }
    if (!strcmp(c, "say") && k->n >= 2) { game_message(C_INFO, "[%s] %s", cur->self ? display_name(cur->self) : "Server", REST(1)); return true; }
    if (!strcmp(c, "me") && k->n >= 2) { game_message(C_INFO, "* %s %s", cur->self ? display_name(cur->self) : "Server", REST(1)); return true; }
    if ((!strcmp(c, "msg") || !strcmp(c, "tell") || !strcmp(c, "w")) && k->n >= 3) { game_message(rgba_u32(170, 170, 170, 255), "You whisper to %s: %s", A(1), REST(2)); return true; }
    if (!strcmp(c, "tellraw") && k->n >= 3) { char t[256]; json_text(REST(2), t, sizeof t); game_message(C_INFO, "%s", t); return true; }
    if (!strcmp(c, "title") && k->n >= 3) {
        const char *what = A(2);
        char t[192] = ""; if (k->n >= 4) json_text(REST(3), t, sizeof t);
        if (!strcmp(what, "clear")) { G.title[0] = G.subtitle[0] = 0; G.title_t = 0; }
        else if (!strcmp(what, "reset")) { G.title_in = 10; G.title_stay = 70; G.title_out = 20; G.title[0] = G.subtitle[0] = 0; }
        else if (!strcmp(what, "title")) { snprintf(G.title, sizeof G.title, "%s", t); G.title_t = 0; if (!G.title_stay) { G.title_in = 10; G.title_stay = 70; G.title_out = 20; } }
        else if (!strcmp(what, "subtitle")) snprintf(G.subtitle, sizeof G.subtitle, "%s", t);
        else if (!strcmp(what, "actionbar")) { snprintf(G.actionbar, sizeof G.actionbar, "%s", t); G.actionbar_t = 60 * MC_TICK_DIV; }
        else if (!strcmp(what, "times") && k->n >= 6) { G.title_in = parse_time_mc(A(3)); G.title_stay = parse_time_mc(A(4)); G.title_out = parse_time_mc(A(5)); }
        else return fail("Unknown title action '%s'", what);
        return true;
    }

    // ---- world settings
    if (!strcmp(c, "gamerule")) {
        if (k->n < 2) { for (int i = 0; i < GR_COUNT; i++) game_message(C_OK, "%s = %d", GAMERULE_DEFS[i].key, g_rules[i]); return true; }
        int r = rule_by_key(A(1));
        if (r < 0) return fail("Unknown game rule: %s", A(1));
        const GameRuleDef *d = &GAMERULE_DEFS[r];
        if (k->n < 3) { if (d->is_bool) game_message(C_INFO, "Gamerule %s is currently set to: %s", d->key, g_rules[r] ? "true" : "false"); else game_message(C_INFO, "Gamerule %s is currently set to: %d", d->key, g_rules[r]); cur->result = g_rules[r]; return true; }
        int v;
        if (d->is_bool) { if (!strcmp(A(2), "true")) v = 1; else if (!strcmp(A(2), "false")) v = 0; else return fail("Invalid boolean, expected 'true' or 'false' but found '%s'", A(2)); }
        else { if (!is_num(A(2))) return fail("Invalid integer '%s'", A(2)); v = atoi(A(2)); if (v < d->lo || v > d->hi) return fail("Integer must be between %d and %d, found %d", d->lo, d->hi, v); }
        g_rules[r] = v;
        if (d->is_bool) ok("Gamerule %s is now set to: %s", d->key, v ? "true" : "false"); else ok("Gamerule %s is now set to: %d", d->key, v);
        return true;
    }
    if (!strcmp(c, "difficulty")) {
        if (k->n < 2) { game_message(C_INFO, "The difficulty is %s", DIFFICULTY_NAMES[g_level.difficulty]); cur->result = g_level.difficulty; return true; }
        int d = -1;
        for (int i = 0; i < 4; i++) if (!strcmp(A(1), DIFFICULTY_NAMES[i]) || (A(1)[0] == "pena"[i] && !A(1)[1]) || (atoi(A(1)) == i && isdigit((unsigned char)A(1)[0]))) d = i;
        if (d < 0) return fail("Invalid difficulty '%s'", A(1));
        if (g_level.hardcore || g_level.difficulty_locked) return fail("The difficulty is locked for this world");
        if (d == g_level.difficulty) return fail("The difficulty did not change; it is already set to %s", DIFFICULTY_NAMES[d]);
        g_level.difficulty = (u8)d;
        ok("The difficulty has been set to %s", DIFFICULTY_NAMES[d]);
        return true;
    }
    if (!strcmp(c, "weather") && k->n >= 2) {
        int dur = k->n >= 3 ? parse_time_mc(A(2)) : -1;
        int w = !strcmp(A(1), "clear") ? 0 : !strcmp(A(1), "rain") ? 1 : !strcmp(A(1), "thunder") ? 2 : -1;
        if (w < 0) return fail("Unknown weather '%s'", A(1));
        weather_set(w, dur);
        ok(w == 0 ? "Set the weather to clear" : w == 1 ? "Set the weather to rain" : "Set the weather to rain & thunder");
        return true;
    }
    if (!strcmp(c, "time") && k->n >= 2) {
        u64 dt = world_daytime();
        if (!strcmp(A(1), "query") && k->n >= 3) {
            long v = !strcmp(A(2), "daytime") ? (long)((dt / MC_TICK_DIV) % 24000) : !strcmp(A(2), "gametime") ? (long)(g_world.time / MC_TICK_DIV) : (long)(dt / DAY_TICKS);
            game_message(C_INFO, "The time is %ld", v); cur->result = (int)v; return true;
        }
        if (k->n < 3) return false;
        long v;
        const char *t = A(2);
        if (!strcmp(t, "day")) v = 1000; else if (!strcmp(t, "noon")) v = 6000; else if (!strcmp(t, "night")) v = 13000;
        else if (!strcmp(t, "midnight")) v = 18000; else if (!strcmp(t, "sunrise")) v = 23000; else if (!strcmp(t, "sunset")) v = 12000;
        else if (is_num(t) || isdigit((unsigned char)t[0])) v = parse_time_mc(t);
        else return fail("Invalid time '%s'", t);
        if (!strcmp(A(1), "set")) {
            // Minecraft's daytime 0 is 6:00 (sunrise) like the engine's; keep the day count
            u64 want = (dt / DAY_TICKS) * DAY_TICKS + (u64)(v % 24000) * MC_TICK_DIV;
            g_world.day_shift += (s64)want - (s64)dt;
            ok("Set the time to %ld", v);
        } else if (!strcmp(A(1), "add")) { g_world.day_shift += (s64)v * MC_TICK_DIV; ok("Set the time to %ld", (long)((world_daytime() / MC_TICK_DIV) % 24000)); }
        else return false;
        return true;
    }
    if (!strcmp(c, "tick") && k->n >= 2) {
        if (!strcmp(A(1), "query")) { game_message(C_INFO, "The game is %s, target tick rate: %.1f per second", G.tick_frozen ? "frozen" : "running normally", G.tick_rate); return true; }
        if (!strcmp(A(1), "rate") && k->n >= 3) { G.tick_rate = clampf((f32)atof(A(2)), 1, 10000); ok("Set the target tick rate to %.1f per second", G.tick_rate); return true; }
        if (!strcmp(A(1), "freeze")) { G.tick_frozen = true; ok("The game is frozen"); return true; }
        if (!strcmp(A(1), "unfreeze")) { G.tick_frozen = false; ok("The game is running normally"); return true; }
        if (!strcmp(A(1), "step")) { G.tick_step = (k->n >= 3 ? MAXI(1, parse_time_mc(A(2))) : 1) * MC_TICK_DIV; ok("Stepping %d tick(s)", G.tick_step / MC_TICK_DIV); return true; }
        return fail("Unknown /tick action");
    }
    if (!strcmp(c, "defaultgamemode") && k->n >= 2) { int gm = gm_parse(A(1)); if (gm < 0) return fail("Unknown game mode"); ok("The default game mode is now %s Mode", GM_NAMES[gm]); return true; }
    if (!strcmp(c, "gamemode") && k->n >= 2) {
        int gm = gm_parse(A(1));
        if (gm < 0) return fail("Unknown game mode '%s'", A(1));
        Entity *t[4]; int n = k->n >= 3 ? targets(A(2), t, 4) : (cur->self ? (t[0] = cur->self, 1) : 0);
        if (n < 0) return fail("Unknown target '%s'", A(2));
        if (n == 0 || t[0]->kind != ENT_PLAYER) return fail("No player was found");
        set_gamemode(gm);
        ok("Set own game mode to %s Mode", GM_NAMES[gm]);
        return true;
    }
    if (!strcmp(c, "setworldspawn")) {
        vec3 p = cur->pos;
        if (k->n >= 4 && !coord3(A(1), A(2), A(3), &p, false)) return fail("Invalid position");
        g_world.spawn = v3_floor(p);
        ok("Set the world spawn point to %d, %d, %d", g_world.spawn.x, disp_y((f32)g_world.spawn.y), g_world.spawn.z);
        return true;
    }
    if (!strcmp(c, "spawnpoint")) {
        vec3 p = cur->self ? cur->self->pos : cur->pos;
        if (k->n >= 5 && !coord3(A(2), A(3), A(4), &p, false)) return fail("Invalid position");
        g_world.spawn = v3_floor(p);
        ok("Set spawn point to %d, %d, %d for Player", g_world.spawn.x, disp_y(p.y), g_world.spawn.z);
        return true;
    }

    // ---- items & players
    if (!strcmp(c, "give") && k->n >= 2) {
        if (!is_target(A(1))) { *handled = false; return false; }   // legacy "give <item> [n]"
        if (k->n < 3) return fail("Incomplete command");
        Entity *t[4]; int n = targets(A(1), t, 4);
        if (n <= 0) return fail("No player was found");
        int id = item_named(A(2));
        if (!id) return fail("Unknown item '%s'", A(2));
        int count = k->n >= 4 ? atoi(A(3)) : 1;
        if (count < 1 || count > 6400) return fail("Can't give more than 6400 of %s", ITEMS[id].name);
        int pot = 0;
        if (strstr(A(2), "potion_contents")) { for (int i = 0; i < POTION_COUNT; i++) if (strstr(A(2), POTIONS[i].key)) pot = i; }
        if (k->n >= 5 && ITEMS[id].variants) { for (int i = 0; i < POTION_COUNT; i++) if (!strcmp(POTIONS[i].key, A(4))) pot = i; }
        give_to(t[0], id, count, pot);
        ok("Gave %d [%s] to Player", count, ITEMS[id].name);
        return true;
    }
    if (!strcmp(c, "clear")) {
        if (k->n >= 2 && !is_target(A(1))) return fail("Unknown target '%s'", A(1));
        int id = 0;
        if (k->n >= 3) { id = item_named(A(2)); if (!id) return fail("Unknown item '%s'", A(2)); }
        int max = k->n >= 4 ? atoi(A(3)) : -1;
        int r = clear_items(id, max);
        if (k->n == 1) { memset(&g_player.inv, 0, sizeof g_player.inv); }   // also resets the selected slot like the old helper
        cur->result = r;
        if (max == 0) ok("Found %d matching item(s) on Player", r);
        else if (k->n > 1 && r == 0) return fail("No items were found on player Player");
        else ok("Removed %d item(s) from player Player", r);
        return true;
    }
    if ((!strcmp(c, "experience") || !strcmp(c, "xp")) && k->n >= 2) {
        if (k->n == 2 && (is_num(A(1)) || (strlen(A(1)) > 1 && toupper((unsigned char)A(1)[strlen(A(1)) - 1]) == 'L'))) {   // pre-1.13 "xp 30" / "xp 5L"
            if (toupper((unsigned char)A(1)[strlen(A(1)) - 1]) == 'L') { g_player.xp_level += atoi(A(1)); ok("Gave %d experience levels to Player", atoi(A(1))); }
            else { *handled = false; return false; }   // legacy helper adds points
            return true;
        }
        const char *act = A(1);
        if (k->n < 4 && strcmp(act, "query")) return fail("Incomplete command");
        if (!strcmp(act, "query")) {
            bool lv = k->n >= 4 && !strcmp(A(3), "levels");
            int v = lv ? g_player.xp_level : (int)(g_player.xp_progress * xp_for_level(g_player.xp_level));
            game_message(C_INFO, "Player has %d experience %s", v, lv ? "levels" : "points"); cur->result = v; return true;
        }
        int amt = atoi(A(3));
        bool levels = k->n >= 5 && !strcmp(A(4), "levels");
        if (!strcmp(act, "add")) {
            if (levels) { g_player.xp_level = MAXI(0, g_player.xp_level + amt); }
            else if (amt >= 0) player_add_xp(amt);
            else { g_player.xp_total = MAXI(0, g_player.xp_total + amt); g_player.xp_progress = fmaxf(0, g_player.xp_progress + (f32)amt / MAXI(1, xp_for_level(g_player.xp_level))); }
            ok("Gave %d experience %s to Player", amt, levels ? "levels" : "points");
        } else if (!strcmp(act, "set")) {
            if (levels) g_player.xp_level = MAXI(0, amt);
            else g_player.xp_progress = clampf((f32)amt / MAXI(1, xp_for_level(g_player.xp_level)), 0, 0.999f);
            ok("Set %d experience %s on Player", amt, levels ? "levels" : "points");
        } else return fail("Unknown action '%s'", act);
        return true;
    }
    if (!strcmp(c, "enchant") && k->n >= 3 && is_target(A(1))) {
        int en = ench_by_name(A(2));
        if (!en) return fail("Unknown enchantment: %s", A(2));
        int lvl = k->n >= 4 ? atoi(A(3)) : 1;
        if (lvl > ENCHANTS[en].max_level) return fail("%d is higher than the maximum level of %d supported by that enchantment", lvl, ENCHANTS[en].max_level);
        ItemStack *h = inventory_held(&g_player.inv);
        if (stack_empty(*h)) return fail("Player is not holding any item");
        for (int i = 1; i < ENCH_COUNT; i++) if (ench_level(*h, i) && ENCHANTS[i].group && ENCHANTS[i].group == ENCHANTS[en].group && i != en) return fail("%s can't be combined with %s", ENCHANTS[en].name, ENCHANTS[i].name);
        if (!ench_add(h, en, lvl)) return fail("Player's item cannot accept that enchantment");
        ok("Applied enchantment %s %s to Player's item", ENCHANTS[en].name, ench_roman(lvl));
        return true;
    }
    if (!strcmp(c, "effect") && k->n >= 2) {
        if (!strcmp(A(1), "clear")) {
            Entity *t[MAX_ENTITIES]; int n = k->n >= 3 ? targets(A(2), t, MAX_ENTITIES) : (cur->self ? (t[0] = cur->self, 1) : 0);
            if (n < 0) { *handled = false; return false; }
            int eff = k->n >= 4 ? effect_by_name(A(3)) : 0;
            for (int i = 0; i < n; i++) { if (eff) t[i]->eff_time[eff] = 0; else effects_clear(t[i]); }
            ok("Removed %s from %d target(s)", eff ? EFFECT_NAMES[eff] : "every effect", n);
            return true;
        }
        if (!strcmp(A(1), "give") && k->n >= 4 && is_target(A(2))) {
            Entity *t[MAX_ENTITIES]; int n = targets(A(2), t, MAX_ENTITIES);
            if (n <= 0) return fail("No entity was found");
            int eff = effect_by_name(A(3));
            if (!eff) return fail("Unknown effect: %s", A(3));
            int secs = k->n >= 5 ? (!strcmp(A(4), "infinite") ? 1000000 : atoi(A(4))) : 30;
            int amp = k->n >= 6 ? clampi(atoi(A(5)), 0, 255) : 0;
            int ticks = secs * TICKRATE;
            if (eff == EFF_INSTANT_HEALTH || eff == EFF_INSTANT_DAMAGE) ticks = 1;
            for (int i = 0; i < n; i++) effect_add(t[i], eff, amp, ticks);
            ok("Applied effect %s to %d target(s)", EFFECT_NAMES[eff], n);
            return true;
        }
        *handled = false; return false;   // legacy "effect <name> [s] [lvl]"
    }
    if (!strcmp(c, "kill")) {
        if (k->n == 1) { *handled = false; return false; }
        Entity *t[MAX_ENTITIES]; int n = targets(A(1), t, MAX_ENTITIES);
        if (n < 0) return fail("Unknown target '%s'", A(1));
        if (n == 0) return fail("No entity was found");
        for (int i = 0; i < n; i++) {
            Entity *e = t[i];
            if (e->kind == ENT_PLAYER) { e->invuln = 0; int gm = g_player.gamemode; g_player.gamemode = GM_SURVIVAL; entity_damage(e, 1e30f, DMG_VOID, NULL); if (g_player.gamemode == GM_SURVIVAL) g_player.gamemode = gm; }
            else if (e->kind == ENT_MOB) { e->invuln = 0; e->absorption = 0; if (!entity_damage(e, 1e30f, DMG_VOID, NULL)) { e->health = 0; e->remove = true; } }
            else e->remove = true;
        }
        if (n == 1) ok("Killed %s", display_name(t[0])); else ok("Killed %d entities", n);
        cur->result = n;
        return true;
    }
    if ((!strcmp(c, "tp") || !strcmp(c, "teleport")) && k->n >= 2) {
        Entity *who[MAX_ENTITIES]; int nwho; int i = 1;
        vec3 dest; bool have = false; f32 yaw = NAN, pitch = NAN;
        if (k->n == 2) {   // tp <destination entity>
            Entity *d[1]; if (targets(A(1), d, 1) <= 0) return fail("No entity was found");
            if (!cur->self) return fail("A target is required");
            who[0] = cur->self; nwho = 1; dest = d[0]->pos; have = true;
        } else if (k->n == 4 || (k->n == 6 && !is_target(A(1)))) {   // tp <x y z> [yaw pitch]
            if (!cur->self) return fail("A target is required");
            who[0] = cur->self; nwho = 1;
            if (!coord3(A(1), A(2), A(3), &dest, true)) return fail("Invalid position");
            have = true; i = 4;
        } else {
            nwho = targets(A(1), who, MAX_ENTITIES);
            if (nwho < 0) { *handled = false; return false; }
            if (nwho == 0) return fail("No entity was found");
            if (k->n == 3) { Entity *d[1]; if (targets(A(2), d, 1) <= 0) return fail("No entity was found"); dest = d[0]->pos; have = true; }
            else if (k->n >= 5) { if (!coord3(A(2), A(3), A(4), &dest, true)) return fail("Invalid position"); have = true; i = 5; }
        }
        if (!have) return fail("Incomplete command");
        if (k->n > i + 1 && strcmp(A(i), "facing")) {
            const char *ys = A(i), *ps = A(i + 1);
            f32 by = cur->self ? cur->self->yaw : 0, bp = cur->self ? cur->self->pitch : 0;
            // Minecraft yaw: 0 = south (+z), 90 = west; pitch: positive looks down
            yaw = ys[0] == '~' ? by + DEG2RAD(-(f32)atof(ys + 1)) : DEG2RAD(-(f32)atof(ys));
            pitch = ps[0] == '~' ? bp - DEG2RAD((f32)atof(ps + 1)) : -DEG2RAD((f32)atof(ps));
        }
        for (int j = 0; j < nwho; j++) {
            Entity *e = who[j];
            if (e->mount) ride_dismount(e);
            e->pos = e->prev_pos = dest; e->vel = v3(0, 0, 0); e->fall_dist = 0;
            if (!isnan(yaw)) { e->yaw = e->body_yaw = e->head_yaw = yaw; e->pitch = clampf(pitch, -1.55f, 1.55f); }
        }
        if (nwho == 1) ok("Teleported %s to %.2f, %.2f, %.2f", display_name(who[0]), dest.x, dest.y - world_y_offset(), dest.z);
        else ok("Teleported %d entities", nwho);
        return true;
    }
    if (!strcmp(c, "summon") && k->n >= 2) {
        if (k->n == 3 || k->n == 4) { if (is_num(A(2))) { *handled = false; return false; } }   // legacy "summon <mob> <count> [dist]"
        vec3 p = cur->pos;
        if (k->n >= 5 && !coord3(A(2), A(3), A(4), &p, true)) return fail("Invalid position");
        const char *type = strip_ns(A(1));
        if (decor_type_by_key(type) >= 0) {   // frames and paintings attach to the first solid neighbour
            int dt = decor_type_by_key(type); ivec3 c = v3_floor(p); Entity *d = NULL;
            if (dt == DECOR_STAND) d = decor_place_at(dt, c, FACE_U, cur->self ? cur->self->yaw + PI_F : 0);
            else { static const int TRY[6] = {FACE_S, FACE_N, FACE_E, FACE_W, FACE_U, FACE_D}; for (int i = 0; i < 6 && !d; i++) d = decor_place_at(dt, c, TRY[i], 0); }
            if (!d) return fail("No room for ultimatecraft:%s there", type);
            ok("Summoned new %s", type); return true;
        }
        if (!strcmp(type, "fishing_bobber"))
            return fail("ultimatecraft:%s is not implemented yet", type);
        if (mob_by_key(type) < 0 && strcmp(type, "lightning_bolt") && strcmp(type, "tnt") && strcmp(type, "end_crystal") && strcmp(type, "item") && strcmp(type, "experience_orb") &&
            !(item_by_key(type) && vehicle_type_of_item(item_by_key(type)) >= 0)) return fail("Unknown entity: ultimatecraft:%s", type);
        Entity *e = summon_at(type, p, cur->self ? cur->self->yaw + PI_F : 0);
        if (e && k->n >= 6 && strstr(REST(5), "NoAI:1")) { e->ai_timer = 1 << 30; e->no_gravity = true; }
        if (e && k->n >= 6 && strstr(REST(5), "PersistenceRequired:1")) e->persistent = true;
        if (e && k->n >= 6 && strstr(REST(5), "IsBaby:1")) e->baby = true;
        if (e && k->n >= 6 && strstr(REST(5), "powered:1")) e->phase = 1;
        ok("Summoned new %s", e && e->kind == ENT_MOB ? MOB_DEFS[e->mob].name : type);
        return true;
    }
    if (!strcmp(c, "damage") && k->n >= 3) {
        Entity *t[1]; if (targets(A(1), t, 1) <= 0) return fail("No entity was found");
        int type = DMG_GENERIC;
        if (k->n >= 4) { const char *d = strip_ns(A(3)); type = !strcmp(d, "fall") ? DMG_FALL : !strcmp(d, "player_attack") || !strcmp(d, "mob_attack") ? DMG_MELEE : !strcmp(d, "arrow") ? DMG_ARROW : !strcmp(d, "lava") ? DMG_LAVA : !strcmp(d, "in_fire") || !strcmp(d, "on_fire") ? DMG_FIRE : !strcmp(d, "drown") ? DMG_DROWN : !strcmp(d, "magic") ? DMG_MAGIC : !strcmp(d, "out_of_world") ? DMG_VOID : !strcmp(d, "explosion") ? DMG_EXPLOSION : !strcmp(d, "freeze") ? DMG_FREEZE : !strcmp(d, "starve") ? DMG_STARVE : !strcmp(d, "wither") ? DMG_WITHER : DMG_GENERIC; }
        Entity *by = NULL;   // /damage <target> <amount> <type> by <entity>
        if (k->n >= 6 && !strcmp(A(4), "by")) { Entity *b[1]; if (targets(A(5), b, 1) > 0) by = b[0]; }
        t[0]->invuln = 0;
        entity_damage(t[0], (f32)atof(A(2)), type, by);
        ok("Applied %.1f damage to %s", atof(A(2)), display_name(t[0]));
        return true;
    }
    if (!strcmp(c, "ride") && k->n >= 3) {
        Entity *t[1]; if (targets(A(1), t, 1) <= 0) return fail("No entity was found");
        if (!strcmp(A(2), "dismount")) { if (!t[0]->mount) return fail("%s is not riding any vehicle", display_name(t[0])); ride_dismount(t[0]); ok("%s stopped riding", display_name(t[0])); return true; }
        if (!strcmp(A(2), "mount") && k->n >= 4) { Entity *v[1]; if (targets(A(3), v, 1) <= 0) return fail("No entity was found"); if (!ride_mount(t[0], v[0])) return fail("%s can't ride that", display_name(t[0])); ok("%s started riding %s", display_name(t[0]), display_name(v[0])); return true; }
        return fail("Incomplete command");
    }
    if (!strcmp(c, "tag") && k->n >= 3) {
        Entity *t[MAX_ENTITIES]; int n = targets(A(1), t, MAX_ENTITIES);
        if (n <= 0) return fail("No entity was found");
        if (!strcmp(A(2), "list")) {
            char b[256] = ""; size_t m = 0;
            for (int i = 0; i < 32; i++) if (t[0]->tags >> i & 1) m += snprintf(b + m, sizeof b - m, "%s%s", m ? ", " : "", tag_names[i]);
            game_message(C_INFO, "%s has %d tags: %s", display_name(t[0]), __builtin_popcount(t[0]->tags), b); return true;
        }
        if (k->n < 4) return fail("Incomplete command");
        int bit = tag_bit(A(3), !strcmp(A(2), "add"));
        if (bit < 0) return fail("Unknown tag");
        int changed = 0;
        for (int i = 0; i < n; i++) {
            u32 before = t[i]->tags;
            if (!strcmp(A(2), "add")) t[i]->tags |= 1u << bit; else t[i]->tags &= ~(1u << bit);
            changed += before != t[i]->tags;
        }
        if (!changed) return fail("Nothing changed");
        ok("%s tag '%s' %s %d entities", !strcmp(A(2), "add") ? "Added" : "Removed", A(3), !strcmp(A(2), "add") ? "to" : "from", changed);
        return true;
    }

    // ---- blocks
    if (!strcmp(c, "setblock") && k->n >= 5) {
        ivec3 p; if (!bcoord3(A(1), A(2), A(3), &p)) return fail("Invalid position");
        int blk = block_named(A(4));
        if (blk < 0) return fail("Unknown block type '%s'", A(4));
        if (p.y < 0 || p.y >= WORLD_H || !world_loaded(p.x, p.z)) return fail("That position is not loaded");
        const char *mode = k->n >= 6 ? A(5) : "replace";
        u16 old = world_get_block(p.x, p.y, p.z);
        if (!strcmp(mode, "keep") && !block_is_air(old)) return fail("Could not set the block");
        if (old == blk) return fail("Could not set the block");
        place(p.x, p.y, p.z, blk, !strcmp(mode, "destroy"));
        ok("Changed the block at %d, %d, %d", p.x, p.y - world_y_offset(), p.z);
        return true;
    }
    if (!strcmp(c, "fill") && k->n >= 8) {
        ivec3 a, b;
        if (!bcoord3(A(1), A(2), A(3), &a) || !bcoord3(A(4), A(5), A(6), &b)) return fail("Invalid position");
        int blk = block_named(A(7));
        if (blk < 0) return fail("Unknown block type '%s'", A(7));
        long vol; if (!volume_ok(a, b, &vol)) return true;
        const char *mode = k->n >= 9 ? A(8) : "replace";
        int filter = -2;
        if (!strcmp(mode, "replace") && k->n >= 10) { filter = block_named(A(9)); if (filter < 0) return fail("Unknown block type '%s'", A(9)); }
        int x0 = MINI(a.x, b.x), x1 = MAXI(a.x, b.x), y0 = MAXI(0, MINI(a.y, b.y)), y1 = MINI(WORLD_H - 1, MAXI(a.y, b.y)), z0 = MINI(a.z, b.z), z1 = MAXI(a.z, b.z);
        long changed = 0;
        for (int y = y0; y <= y1; y++) for (int z = z0; z <= z1; z++) for (int x = x0; x <= x1; x++) {
            if (!world_loaded(x, z)) continue;
            bool edge = x == x0 || x == x1 || y == y0 || y == y1 || z == z0 || z == z1;
            u16 old = world_get_block(x, y, z);
            int want = blk;
            if (!strcmp(mode, "hollow")) want = edge ? blk : 0;
            else if (!strcmp(mode, "outline")) { if (!edge) continue; }
            else if (!strcmp(mode, "keep")) { if (!block_is_air(old)) continue; }
            else if (filter >= -1) { if (BLOCKS[old].base != BLOCKS[filter].base && !(filter == 0 && block_is_air(old))) continue; }
            if (old == want) continue;
            place(x, y, z, want, !strcmp(mode, "destroy"));
            changed++;
        }
        if (!changed) return fail("No blocks were filled");
        cur->result = (int)changed;
        ok("Successfully filled %ld block(s)", changed);
        return true;
    }
    if (!strcmp(c, "clone") && k->n >= 10) {
        ivec3 a, b, d;
        if (!bcoord3(A(1), A(2), A(3), &a) || !bcoord3(A(4), A(5), A(6), &b) || !bcoord3(A(7), A(8), A(9), &d)) return fail("Invalid position");
        long vol; if (!volume_ok(a, b, &vol)) return true;
        const char *mask = k->n >= 11 ? A(10) : "replace", *mode = k->n >= 12 ? A(11) : "normal";
        int x0 = MINI(a.x, b.x), y0 = MINI(a.y, b.y), z0 = MINI(a.z, b.z);
        int sx = abs(a.x - b.x) + 1, sy = abs(a.y - b.y) + 1, sz = abs(a.z - b.z) + 1;
        bool overlap = !(d.x + sx <= x0 || d.x >= x0 + sx || d.y + sy <= y0 || d.y >= y0 + sy || d.z + sz <= z0 || d.z >= z0 + sz);
        if (overlap && strcmp(mode, "force") && strcmp(mode, "move")) return fail("The source and destination areas cannot overlap");
        u16 *buf = xmalloc(sizeof(u16) * (size_t)vol);
        BlockEntity *bes = xcalloc((size_t)vol, sizeof(BlockEntity));
        bool *hasbe = xcalloc((size_t)vol, 1);
        long i = 0;
        for (int y = 0; y < sy; y++) for (int z = 0; z < sz; z++) for (int x = 0; x < sx; x++, i++) {
            ivec3 p = iv3(x0 + x, y0 + y, z0 + z);
            buf[i] = world_get_block(p.x, p.y, p.z);
            BlockEntity *be = BLOCKS[buf[i]].be ? world_block_entity(p, false, 0) : NULL;
            if (be) { bes[i] = *be; hasbe[i] = true; }
        }
        if (!strcmp(mode, "move")) for (int y = 0; y < sy; y++) for (int z = 0; z < sz; z++) for (int x = 0; x < sx; x++) place(x0 + x, y0 + y, z0 + z, 0, 0);
        long changed = 0; i = 0;
        for (int y = 0; y < sy; y++) for (int z = 0; z < sz; z++) for (int x = 0; x < sx; x++, i++) {
            if (!strcmp(mask, "masked") && block_is_air(buf[i])) continue;
            ivec3 p = iv3(d.x + x, d.y + y, d.z + z);
            if (p.y < 0 || p.y >= WORLD_H || !world_loaded(p.x, p.z)) continue;
            place(p.x, p.y, p.z, buf[i], 0);
            if (hasbe[i]) { BlockEntity *be = world_block_entity(p, true, bes[i].type); if (be) { ivec3 keep = be->pos; *be = bes[i]; be->pos = keep; } }
            changed++;
        }
        free(buf); free(bes); free(hasbe);
        cur->result = (int)changed;
        ok("Successfully cloned %ld block(s)", changed);
        return true;
    }

    // ---- misc
    if (!strcmp(c, "particle") && k->n >= 2) {
        vec3 p = cur->pos; if (k->n >= 5) coord3(A(2), A(3), A(4), &p, false);
        const char *n = strip_ns(A(1));
        if (!strcmp(n, "explosion") || !strcmp(n, "explosion_emitter")) fx_explosion(p, 2);
        else if (!strcmp(n, "heart")) fx_hearts(p);
        else if (!strcmp(n, "flame") || !strcmp(n, "soul_fire_flame")) fx_flame(p);
        else if (!strcmp(n, "bubble")) fx_bubbles(p, 8);
        else if (!strcmp(n, "splash")) fx_splash(p, 10);
        else if (!strcmp(n, "crit") || !strcmp(n, "enchanted_hit")) fx_crit(p, 10);
        else if (!strcmp(n, "poof") || !strcmp(n, "cloud")) fx_poof(p, 1, 1);
        else if (!strcmp(n, "damage_indicator")) fx_damage(p, 4);
        else fx_smoke(p, 8, 0.2f, rgba_u32(200, 200, 200, 255));
        return true;
    }
    if (!strcmp(c, "playsound") && k->n >= 2) {
        const char *n = strip_ns(A(1));
        int s = strstr(n, "explode") ? SND_EXPLODE : strstr(n, "levelup") ? SND_LEVELUP : strstr(n, "orb") ? SND_XP : strstr(n, "pop") ? SND_POP : strstr(n, "bell") ? SND_LEVELUP : strstr(n, "click") ? SND_CLICK : SND_HIT;
        sound_play_at(s, cur->pos, 1, 1); return true;
    }
    if (!strcmp(c, "stopsound")) return true;
    if (!strcmp(c, "recipe") && k->n >= 4) {
        bool give = !strcmp(A(1), "give");
        int n = 0;
        if (!strcmp(A(3), "*")) { for (int r = 0; r < RECIPE_COUNT; r++) { if (give) g_player.recipes_known[r / 8] |= (u8)(1 << (r % 8)); else g_player.recipes_known[r / 8] &= (u8)~(1 << (r % 8)); n++; } }
        else { int it = item_named(A(3)); for (int r = 0; r < RECIPE_COUNT; r++) if (RECIPES[r].result == it) { if (give) g_player.recipes_known[r / 8] |= (u8)(1 << (r % 8)); else g_player.recipes_known[r / 8] &= (u8)~(1 << (r % 8)); n++; } }
        ok("%s %d recipe(s) %s Player", give ? "Unlocked" : "Took", n, give ? "for" : "from");
        return true;
    }
    if (!strcmp(c, "raid")) { raid_command(k->n >= 2 ? A(1) : "status"); return true; }
    if (!strcmp(c, "locate") && k->n >= 3) {   // accept namespaced ids, then let the original implementation answer
        static char buf[640];
        snprintf(buf, sizeof buf, "locate %s %s%s%s", A(1), strip_ns(A(2)), k->n >= 4 ? " " : "", k->n >= 4 ? A(3) : "");
        *handled = false;
        if (strncmp(A(2), "minecraft:", 10) == 0) { *handled = true; command_execute(buf); }
        return *handled;
    }

    // ---- /execute
    if (!strcmp(c, "execute") && k->n >= 2) {
        if (cur->depth > 16) return fail("Too much recursion in /execute");
        int i = 1;
        Ctx base = *cur;
        // fork state: a list of contexts
        static Ctx forks[16][256]; int nf = 1;
        int store_mode = 0; char store_h[64] = "", store_o[32] = "";   // execute store result|success score <holder> <objective>
        Ctx *list = forks[MINI(cur->depth, 15)];
        list[0] = base;
        while (i < k->n) {
            const char *sub = A(i);
            if (!strcmp(sub, "run")) {
                const char *line = REST(i + 1);
                int good = 0;
                for (int f = 0; f < nf; f++) {
                    Ctx sub_ctx = list[f]; sub_ctx.depth = cur->depth + 1;
                    Ctx *saved = cur; cur = &sub_ctx;
                    bool h; Toks t2; tokenize(line, &t2);
                    if (t2.n) { if (!run(&t2, &h) && !h) command_execute(line); }
                    good += sub_ctx.result;
                    if (store_mode) {
                        int o = sb_obj(store_o); const char *hs[64]; Ctx *sv2 = cur; cur = &list[f]; int nh = o >= 0 ? holders(store_h, o, hs, 64) : 0; cur = sv2;
                        for (int q = 0; q < nh; q++) sb_set(hs[q], o, store_mode == 1 ? sub_ctx.result : sub_ctx.result != 0);
                    }
                    cur = saved;
                }
                cur->result = good;
                return true;
            }
            if ((!strcmp(sub, "as") || !strcmp(sub, "at")) && i + 1 < k->n) {
                bool as = !strcmp(sub, "as");
                Ctx nl[256]; int nn = 0;
                for (int f = 0; f < nf; f++) {
                    Ctx *saved = cur; cur = &list[f];
                    Entity *t[256]; int n = targets(A(i + 1), t, 256);
                    cur = saved;
                    if (n < 0) return fail("Unknown target '%s'", A(i + 1));
                    for (int j = 0; j < n && nn < 256; j++) {
                        nl[nn] = list[f];
                        if (as) nl[nn].self = t[j];
                        else { nl[nn].pos = t[j]->pos; nl[nn].yaw = t[j]->yaw; nl[nn].pitch = t[j]->pitch; }
                        nn++;
                    }
                }
                memcpy(list, nl, sizeof(Ctx) * nn); nf = nn; i += 2; continue;
            }
            if (!strcmp(sub, "positioned") && i + 1 < k->n) {
                if (!strcmp(A(i + 1), "as") && i + 2 < k->n) {
                    for (int f = 0; f < nf; f++) { Ctx *saved = cur; cur = &list[f]; Entity *t[1]; int n = targets(A(i + 2), t, 1); cur = saved; if (n > 0) list[f].pos = t[0]->pos; }
                    i += 3; continue;
                }
                for (int f = 0; f < nf; f++) { Ctx *saved = cur; cur = &list[f]; vec3 p; if (!coord3(A(i + 1), A(i + 2), A(i + 3), &p, true)) { cur = saved; return fail("Invalid position"); } cur = saved; list[f].pos = p; }
                i += 4; continue;
            }
            if (!strcmp(sub, "rotated") && i + 2 < k->n) {
                if (!strcmp(A(i + 1), "as")) { for (int f = 0; f < nf; f++) { Ctx *saved = cur; cur = &list[f]; Entity *t[1]; int n = targets(A(i + 2), t, 1); cur = saved; if (n > 0) { list[f].yaw = t[0]->yaw; list[f].pitch = t[0]->pitch; } } i += 3; continue; }
                for (int f = 0; f < nf; f++) { list[f].yaw = DEG2RAD(-(f32)atof(A(i + 1))); list[f].pitch = -DEG2RAD((f32)atof(A(i + 2))); }
                i += 3; continue;
            }
            if (!strcmp(sub, "facing") && i + 1 < k->n) {
                for (int f = 0; f < nf; f++) {
                    vec3 tgt;
                    Ctx *saved = cur; cur = &list[f];
                    if (!strcmp(A(i + 1), "entity")) { Entity *t[1]; if (targets(A(i + 2), t, 1) <= 0) { cur = saved; nf = 0; break; } tgt = entity_eye(t[0]); }
                    else if (!coord3(A(i + 1), A(i + 2), A(i + 3), &tgt, true)) { cur = saved; return fail("Invalid position"); }
                    cur = saved;
                    vec3 d = v3_sub(tgt, list[f].pos);
                    list[f].yaw = atan2f(d.x, d.z); list[f].pitch = atan2f(d.y, sqrtf(d.x * d.x + d.z * d.z));
                }
                i += !strcmp(A(i + 1), "entity") ? 4 : 4; continue;
            }
            if ((!strcmp(sub, "align")) && i + 1 < k->n) {
                for (int f = 0; f < nf; f++) { if (strchr(A(i + 1), 'x')) list[f].pos.x = floorf(list[f].pos.x); if (strchr(A(i + 1), 'y')) list[f].pos.y = floorf(list[f].pos.y); if (strchr(A(i + 1), 'z')) list[f].pos.z = floorf(list[f].pos.z); }
                i += 2; continue;
            }
            if (!strcmp(sub, "in") && i + 1 < k->n) {   // switch the loaded dimension, then keep running in it
                int dm = dim_by_id(A(i + 1));
                if (dm < 0) return fail("Unknown dimension '%s'", A(i + 1));
                if (dm != g_world.dim) game_change_dimension(dm, false);
                i += 2; continue;
            }
            if (!strcmp(sub, "anchored")) { i += 2; continue; }
            if ((!strcmp(sub, "if") || !strcmp(sub, "unless")) && i + 1 < k->n) {
                bool want = !strcmp(sub, "if");
                const char *what = A(i + 1);
                int used = 0;
                Ctx nl[256]; int nn = 0;
                for (int f = 0; f < nf; f++) {
                    Ctx *saved = cur; cur = &list[f];
                    bool r = false;
                    if (!strcmp(what, "block") && i + 5 < k->n + 1) {
                        ivec3 p; int blk = block_named(A(i + 5));
                        r = bcoord3(A(i + 2), A(i + 3), A(i + 4), &p) && blk >= 0 && BLOCKS[world_get_block(p.x, p.y, p.z)].base == BLOCKS[blk].base;
                        used = 6;
                    } else if (!strcmp(what, "entity")) { Entity *t[1]; r = targets(A(i + 2), t, 1) > 0; used = 3; }
                    else if (!strcmp(what, "loaded")) { ivec3 p; r = bcoord3(A(i + 2), A(i + 3), A(i + 4), &p) && world_loaded(p.x, p.z); used = 5; }
                    else if (!strcmp(what, "dimension")) { r = dim_by_id(A(i + 2)) == g_world.dim; used = 3; }
                    else if (!strcmp(what, "score")) {   // if score <holder> <obj> matches <range> | <op> <holder2> <obj2>
                        const char *h1[1]; int o1 = sb_obj(A(i + 3)), v1 = 0;
                        bool have = o1 >= 0 && holders(A(i + 2), o1, h1, 1) > 0 && sb_get(h1[0], o1, &v1);
                        if (!strcmp(A(i + 4), "matches")) { r = have && range_ok(A(i + 5), (f32)v1); used = 6; }
                        else {
                            const char *h2[1]; int o2 = sb_obj(A(i + 6)), v2 = 0;
                            bool have2 = o2 >= 0 && holders(A(i + 5), o2, h2, 1) > 0 && sb_get(h2[0], o2, &v2);
                            const char *op = A(i + 4);
                            r = have && have2 && (!strcmp(op, "<") ? v1 < v2 : !strcmp(op, "<=") ? v1 <= v2 : !strcmp(op, "=") ? v1 == v2 : !strcmp(op, ">=") ? v1 >= v2 : !strcmp(op, ">") ? v1 > v2 : false);
                            used = 7;
                        }
                    }
                    else if (!strcmp(what, "biome")) { ivec3 p; r = bcoord3(A(i + 2), A(i + 3), A(i + 4), &p) && world_biome_at(p.x, p.y, p.z) == biome_by_key(strip_ns(A(i + 5))); used = 6; }
                    else { cur = saved; return fail("Unsupported /execute %s condition '%s'", sub, what); }
                    cur = saved;
                    if (r == want) nl[nn++] = list[f];
                }
                memcpy(list, nl, sizeof(Ctx) * nn); nf = nn; i += used;
                if (i >= k->n) { cur->result = nf; if (nf) ok("Test passed%s", nf > 1 ? "" : ""); else fail("Test failed"); return true; }
                continue;
            }
            if (!strcmp(sub, "store") && i + 4 < k->n) {
                if (strcmp(A(i + 2), "score")) return fail("Only /execute store result|success score is supported");
                store_mode = !strcmp(A(i + 1), "success") ? 2 : 1;
                snprintf(store_h, sizeof store_h, "%s", A(i + 3)); snprintf(store_o, sizeof store_o, "%s", A(i + 4));
                i += 5; continue;
            }
            return fail("Unknown /execute subcommand '%s'", sub);
        }
        return fail("Incomplete /execute command");
    }
    *handled = false;
    return false;
    #undef A
    #undef REST
}

static bool cmd_execute_line(const char *line) { (void)line; return false; }

bool mc_command(const char *line, bool *handled) {
    Toks *k = xmalloc(sizeof(Toks));
    tokenize(line, k);
    if (!k->n) { free(k); *handled = false; return false; }
    Entity *pl = player_ent();
    Ctx root = {pl, pl ? pl->pos : v3(0, 0, 0), pl ? pl->yaw : 0, pl ? pl->pitch : 0, 0, 1};
    Ctx *saved = cur;
    if (!cur) cur = &root;
    bool r = run(k, handled);
    cur = saved;
    free(k);
    (void)cmd_execute_line;
    return r;
}

// ------------------------------------------------------------------ chat entry point (cheats)
static const char *const NO_CHEAT_OK[] = {"help", "?", "me", "msg", "tell", "w", "list", "seed", "trigger", "teammsg", "tm", NULL};
bool command_execute_chat(const char *line) {
    char c[32] = ""; sscanf(line, "%31s", c);
    bool allowed = g_level.cheats;
    for (int i = 0; NO_CHEAT_OK[i]; i++) if (!strcmp(c, NO_CHEAT_OK[i])) allowed = true;
    if (!allowed) { game_message(C_ERR, "Cheats are not allowed in this world (turn on \"Allow Cheats\" in the pause menu)"); return true; }
    if (rule(GR_LOG_ADMIN_COMMANDS)) LOG_INFO("[Player: /%s]", line);
    return command_execute(line);
}

void title_show(const char *t, const char *sub) {
    if (g_narrate) { printf("[narrate] title: %s%s%s\n", t ? t : "", sub && *sub ? " - " : "", sub ? sub : ""); fflush(stdout); }
    snprintf(G.title, sizeof G.title, "%s", t ? t : "");
    snprintf(G.subtitle, sizeof G.subtitle, "%s", sub ? sub : "");
    G.title_t = 0;
    if (!G.title_stay) { G.title_in = 10; G.title_stay = 70; G.title_out = 20; }
}

// Enchantments: definitions, item compatibility, enchanting-table selection, tooltips and
// the custom item-name table used by the anvil.
#include "../core/platform.h"
#include "item.h"

#define D ET_DURABLE
const EnchantDef ENCHANTS[ENCH_COUNT] = {
    [ENCH_NONE] = {"none", "None", 0, 0, 0, 0, 0, 0, 0, false, false},
    [ENCH_PROTECTION] = {"protection", "Protection", 4, 10, ET_ARMOR, 1, 1, 11, 11, false, false},
    [ENCH_FIRE_PROTECTION] = {"fire_protection", "Fire Protection", 4, 5, ET_ARMOR, 1, 10, 8, 8, false, false},
    [ENCH_FEATHER_FALLING] = {"feather_falling", "Feather Falling", 4, 5, ET_FEET, 0, 5, 6, 6, false, false},
    [ENCH_BLAST_PROTECTION] = {"blast_protection", "Blast Protection", 4, 2, ET_ARMOR, 1, 5, 8, 8, false, false},
    [ENCH_PROJECTILE_PROTECTION] = {"projectile_protection", "Projectile Protection", 4, 5, ET_ARMOR, 1, 3, 6, 6, false, false},
    [ENCH_RESPIRATION] = {"respiration", "Respiration", 3, 2, ET_HEAD, 0, 10, 10, 30, false, false},
    [ENCH_AQUA_AFFINITY] = {"aqua_affinity", "Aqua Affinity", 1, 2, ET_HEAD, 0, 1, 0, 40, false, false},
    [ENCH_THORNS] = {"thorns", "Thorns", 3, 1, ET_ARMOR, 0, 10, 20, 50, false, false},
    [ENCH_DEPTH_STRIDER] = {"depth_strider", "Depth Strider", 3, 2, ET_FEET, 2, 10, 10, 15, false, false},
    [ENCH_FROST_WALKER] = {"frost_walker", "Frost Walker", 2, 2, ET_FEET, 2, 10, 10, 15, true, false},
    [ENCH_BINDING_CURSE] = {"binding_curse", "Curse of Binding", 1, 1, ET_ARMOR, 0, 25, 0, 25, true, true},
    [ENCH_SOUL_SPEED] = {"soul_speed", "Soul Speed", 3, 1, ET_FEET, 0, 10, 10, 15, true, false},
    [ENCH_SWIFT_SNEAK] = {"swift_sneak", "Swift Sneak", 3, 1, ET_LEGS, 0, 25, 25, 50, true, false},
    [ENCH_SHARPNESS] = {"sharpness", "Sharpness", 5, 10, ET_SWORD | ET_AXE, 3, 1, 11, 20, false, false},
    [ENCH_SMITE] = {"smite", "Smite", 5, 5, ET_SWORD | ET_AXE | ET_MACE, 3, 5, 8, 20, false, false},
    [ENCH_BANE_OF_ARTHROPODS] = {"bane_of_arthropods", "Bane of Arthropods", 5, 5, ET_SWORD | ET_AXE | ET_MACE, 3, 5, 8, 20, false, false},
    [ENCH_KNOCKBACK] = {"knockback", "Knockback", 2, 5, ET_SWORD, 0, 5, 20, 50, false, false},
    [ENCH_FIRE_ASPECT] = {"fire_aspect", "Fire Aspect", 2, 2, ET_SWORD | ET_MACE, 0, 10, 20, 50, false, false},
    [ENCH_LOOTING] = {"looting", "Looting", 3, 2, ET_SWORD, 0, 15, 9, 50, false, false},
    [ENCH_SWEEPING_EDGE] = {"sweeping_edge", "Sweeping Edge", 3, 2, ET_SWORD, 0, 5, 9, 15, false, false},
    [ENCH_EFFICIENCY] = {"efficiency", "Efficiency", 5, 10, ET_DIGGER | ET_AXE, 0, 1, 10, 50, false, false},
    [ENCH_SILK_TOUCH] = {"silk_touch", "Silk Touch", 1, 1, ET_DIGGER | ET_AXE, 4, 15, 0, 50, false, false},
    [ENCH_UNBREAKING] = {"unbreaking", "Unbreaking", 3, 5, D, 0, 5, 8, 50, false, false},
    [ENCH_FORTUNE] = {"fortune", "Fortune", 3, 2, ET_DIGGER | ET_AXE, 4, 15, 9, 50, false, false},
    [ENCH_POWER] = {"power", "Power", 5, 10, ET_BOW, 0, 1, 10, 15, false, false},
    [ENCH_PUNCH] = {"punch", "Punch", 2, 2, ET_BOW, 0, 12, 20, 25, false, false},
    [ENCH_FLAME] = {"flame", "Flame", 1, 2, ET_BOW, 0, 20, 0, 30, false, false},
    [ENCH_INFINITY] = {"infinity", "Infinity", 1, 1, ET_BOW, 5, 20, 0, 30, false, false},
    [ENCH_LUCK_OF_THE_SEA] = {"luck_of_the_sea", "Luck of the Sea", 3, 2, ET_ROD, 0, 15, 9, 50, false, false},
    [ENCH_LURE] = {"lure", "Lure", 3, 2, ET_ROD, 0, 15, 9, 50, false, false},
    [ENCH_LOYALTY] = {"loyalty", "Loyalty", 3, 5, ET_TRIDENT, 6, 12, 7, 38, false, false},
    [ENCH_IMPALING] = {"impaling", "Impaling", 5, 2, ET_TRIDENT, 0, 1, 8, 20, false, false},
    [ENCH_RIPTIDE] = {"riptide", "Riptide", 3, 2, ET_TRIDENT, 6, 17, 7, 38, false, false},
    [ENCH_CHANNELING] = {"channeling", "Channeling", 1, 1, ET_TRIDENT, 6, 25, 0, 25, false, false},
    [ENCH_MULTISHOT] = {"multishot", "Multishot", 1, 2, ET_CROSSBOW, 7, 20, 0, 30, false, false},
    [ENCH_QUICK_CHARGE] = {"quick_charge", "Quick Charge", 3, 5, ET_CROSSBOW, 0, 12, 20, 38, false, false},
    [ENCH_PIERCING] = {"piercing", "Piercing", 4, 10, ET_CROSSBOW, 7, 1, 10, 49, false, false},
    [ENCH_MENDING] = {"mending", "Mending", 1, 2, D, 5, 25, 0, 50, true, false},
    [ENCH_VANISHING_CURSE] = {"vanishing_curse", "Curse of Vanishing", 1, 1, D | ET_ANY, 0, 25, 0, 25, true, true},
    [ENCH_DENSITY] = {"density", "Density", 5, 5, ET_MACE, 8, 5, 8, 20, false, false},
    [ENCH_BREACH] = {"breach", "Breach", 4, 2, ET_MACE, 8, 15, 9, 50, false, false},
    [ENCH_WIND_BURST] = {"wind_burst", "Wind Burst", 3, 2, ET_MACE, 0, 15, 9, 50, true, false},
};
#undef D

int ench_level(ItemStack s, int e) {
    for (int i = 0; i < MAX_ENCH; i++) if (s.ench[i][0] == e) return s.ench[i][1];
    return 0;
}
int ench_count(ItemStack s) { int n = 0; for (int i = 0; i < MAX_ENCH; i++) n += s.ench[i][0] != 0; return n; }
bool ench_add(ItemStack *s, int e, int lvl) {
    if (e <= 0 || e >= ENCH_COUNT || lvl <= 0) return false;
    for (int i = 0; i < MAX_ENCH; i++) if (s->ench[i][0] == e) { s->ench[i][1] = (u8)lvl; return true; }
    for (int i = 0; i < MAX_ENCH; i++) if (!s->ench[i][0]) { s->ench[i][0] = (u8)e; s->ench[i][1] = (u8)lvl; return true; }
    return false;
}
void ench_remove(ItemStack *s, int e) {
    for (int i = 0; i < MAX_ENCH; i++) if (s->ench[i][0] == e) { s->ench[i][0] = s->ench[i][1] = 0; }
    // keep the list packed so stacks compare equal
    u8 tmp[MAX_ENCH][2] = {{0}}; int n = 0;
    for (int i = 0; i < MAX_ENCH; i++) if (s->ench[i][0]) { tmp[n][0] = s->ench[i][0]; tmp[n][1] = s->ench[i][1]; n++; }
    memcpy(s->ench, tmp, sizeof tmp);
}

u16 item_ench_targets(int id) {
    if (!item_valid(id)) return 0;
    const ItemDef *it = &ITEMS[id];
    const char *k = it->key;
    u16 t = 0;
    if (it->durability) t |= ET_DURABLE;
    if (it->armor_slot >= 0) t |= (u16)(ET_HEAD << it->armor_slot);
    switch (it->tool) {
    case TOOL_SWORD: t |= ET_SWORD; break;
    case TOOL_AXE: t |= ET_AXE; break;
    case TOOL_PICKAXE: case TOOL_SHOVEL: case TOOL_HOE: t |= ET_DIGGER; break;
    default: break;
    }
    if (!strcmp(k, "bow")) t |= ET_BOW;
    if (!strcmp(k, "crossbow")) t |= ET_CROSSBOW;
    if (!strcmp(k, "trident")) t |= ET_TRIDENT;
    if (!strcmp(k, "fishing_rod")) t |= ET_ROD;
    if (!strcmp(k, "mace")) t |= ET_MACE;
    if (it->armor_slot >= 0 || !strcmp(k, "carved_pumpkin") || strstr(k, "_head") || strstr(k, "_skull")) t |= ET_ANY;
    return t;
}
bool ench_applicable(int e, int item) {
    if (item == I_BOOK || item == I_ENCHANTED_BOOK) return true;
    u16 t = item_ench_targets(item);
    return (ENCHANTS[e].targets & t) != 0;
}
bool ench_compatible(ItemStack s, int e) {
    for (int i = 0; i < MAX_ENCH; i++) {
        int o = s.ench[i][0];
        if (!o || o == e) continue;
        if (ENCHANTS[e].group && ENCHANTS[o].group == ENCHANTS[e].group) return false;
    }
    return true;
}
int item_enchantability(int id) {
    if (id == I_BOOK) return 1;
    if (!item_valid(id)) return 0;
    const ItemDef *it = &ITEMS[id];
    const char *k = it->key;
    if (!it->durability) return 0;
    bool armor = it->armor_slot >= 0;
    if (!strncmp(k, "wooden_", 7)) return 15;
    if (!strncmp(k, "stone_", 6)) return 5;
    if (!strncmp(k, "leather_", 8)) return 15;
    if (!strncmp(k, "chainmail_", 10)) return 12;
    if (!strncmp(k, "iron_", 5)) return armor ? 9 : 14;
    if (!strncmp(k, "copper_", 7)) return armor ? 10 : 13;
    if (!strncmp(k, "golden_", 7)) return armor ? 25 : 22;
    if (!strncmp(k, "diamond_", 8)) return 10;
    if (!strncmp(k, "netherite_", 10)) return 15;
    if (!strcmp(k, "turtle_helmet")) return 9;
    if (item_ench_targets(id) & (ET_BOW | ET_CROSSBOW | ET_TRIDENT | ET_ROD)) return 1;
    if (!strcmp(k, "mace")) return 15;
    return 0;
}
const char *ench_roman(int l) {
    static const char *R[] = {"", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"};
    return l >= 0 && l <= 10 ? R[l] : "X+";
}

static u32 xr(u32 *s) { *s ^= *s << 13; *s ^= *s >> 17; *s ^= *s << 5; return *s; }
static int rint_(u32 *s, int n) { return n <= 0 ? 0 : (int)(xr(s) % (u32)n); }

int ench_select(int item, int level, u32 seed, u8 out[][2], int max) {
    int ench = item_enchantability(item);
    if (!ench || level <= 0) return 0;
    u32 s = seed * 2654435761u + (u32)level * 40503u + 1;
    if (!s) s = 1;
    int mod = level + 1 + rint_(&s, ench / 4 + 1) + rint_(&s, ench / 4 + 1);
    f32 bonus = 1.0f + ((xr(&s) % 1000) / 1000.0f + (xr(&s) % 1000) / 1000.0f - 1.0f) * 0.15f;
    mod = MAXI(1, (int)(mod * bonus + 0.5f));
    int n = 0;
    ItemStack tmp = stack_of(item, 1);
    for (int round = 0; round < max; round++) {
        // weighted list of enchantments whose power range covers mod
        int cand[ENCH_COUNT], lvl[ENCH_COUNT], total = 0, nc = 0;
        for (int e = 1; e < ENCH_COUNT; e++) {
            const EnchantDef *d = &ENCHANTS[e];
            if (d->treasure || !ench_applicable(e, item) || ench_level(tmp, e) || !ench_compatible(tmp, e)) continue;
            int best = 0;
            for (int l = d->max_level; l >= 1; l--) {
                int mn = d->min_base + (l - 1) * d->min_step, mx = mn + d->span;
                if (mod >= mn && mod <= mx) { best = l; break; }
            }
            if (!best) continue;
            cand[nc] = e; lvl[nc] = best; nc++; total += d->weight;
        }
        if (!nc) break;
        int r = rint_(&s, total), pick = 0;
        for (int i = 0; i < nc; i++) { r -= ENCHANTS[cand[i]].weight; if (r < 0) { pick = i; break; } }
        out[n][0] = (u8)cand[pick]; out[n][1] = (u8)lvl[pick]; n++;
        ench_add(&tmp, cand[pick], lvl[pick]);
        if (rint_(&s, 50) > mod) break;    // the chance of extra enchantments falls off
        mod /= 2;
        if (item == I_BOOK && n >= 1 && rint_(&s, 2)) break;
    }
    return n;
}

static const char *PATTERN_NAMES[] = {"", "Base", "Stripe Bottom", "Stripe Top", "Stripe Left", "Stripe Right", "Stripe Center", "Stripe Middle",
    "Stripe Downright", "Stripe Downleft", "Cross", "Straight Cross", "Chevron", "Inverted Chevron", "Half Horizontal", "Half Vertical",
    "Border", "Circle", "Rhombus", "Gradient", "Bricks", "Creeper Charge", "Skull Charge", "Flower Charge", "Globe", "Snout", "Thing", "Flow", "Guster"};
const char *banner_pattern_name(int p) { return p > 0 && p < (int)ARRAY_LEN(PATTERN_NAMES) ? PATTERN_NAMES[p] : "?"; }
static const char *COLOR_NAMES[16] = {"White", "Orange", "Magenta", "Light Blue", "Yellow", "Lime", "Pink", "Gray", "Light Gray", "Cyan", "Purple", "Blue", "Brown", "Green", "Red", "Black"};

int box_count(u16 id, ItemStack *first);   // game/save.c
int stack_tooltip_lines(ItemStack s, char lines[][64], int max) {
    int n = 0;
    if (stack_empty(s)) return 0;
    const char *k = ITEMS[s.id].key;
    if (strstr(k, "shulker_box") && s.data && max > 0) {
        ItemStack first = {0};
        int c = box_count(s.data, &first);
        if (c) { snprintf(lines[n++], 64, "%s x%d", ITEMS[first.id].name, first.count); if (c > 1 && n < max) snprintf(lines[n++], 64, "and %d more...", c - 1); }
        return n;
    }
    if (strstr(k, "_banner") && !strstr(k, "pattern")) {
        for (int i = 0; i < MAX_ENCH && n < max; i++) if (s.ench[i][0]) snprintf(lines[n++], 64, "%s %s", COLOR_NAMES[s.ench[i][1] & 15], banner_pattern_name(s.ench[i][0]));
        return n;
    }
    for (int i = 0; i < MAX_ENCH && n < max; i++) {
        int e = s.ench[i][0];
        if (!e || e >= ENCH_COUNT) continue;
        if (ENCHANTS[e].max_level == 1) snprintf(lines[n++], 64, "%s", ENCHANTS[e].name);
        else snprintf(lines[n++], 64, "%s %s", ENCHANTS[e].name, ench_roman(s.ench[i][1]));
    }
    if (s.id == I_FILLED_MAP && n < max) snprintf(lines[n++], 64, "Map #%d", s.data);
    if (s.rcost && n < max && ITEMS[s.id].durability) snprintf(lines[n++], 64, "Repaired %d time%s", s.rcost, s.rcost > 1 ? "s" : "");
    return n;
}

// ------------------------------------------------------------------------ custom names
static char **names;
static int nnames;
u16 item_name_intern(const char *name) {
    if (!name || !name[0]) return 0;
    for (int i = 0; i < nnames; i++) if (!strcmp(names[i], name)) return (u16)(i + 1);
    if (nnames >= 65000) return 0;
    names = xrealloc(names, sizeof(char *) * (nnames + 1));
    size_t l = strlen(name);
    names[nnames] = xmalloc(l + 1); memcpy(names[nnames], name, l + 1);
    return (u16)(++nnames);
}
const char *item_name_get(u16 idx) { return idx && idx <= nnames ? names[idx - 1] : NULL; }
void item_names_reset(void) { for (int i = 0; i < nnames; i++) free(names[i]); free(names); names = NULL; nnames = 0; }
void item_names_save(const char *path) {
    StrBuf b = {0};
    for (int i = 0; i < nnames; i++) sbuf_printf(&b, "%s\n", names[i]);
    sbuf_write(&b, path);
}
void item_names_load(const char *path) {
    item_names_reset();
    FILE *f = fopen(path, "rb");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        names = xrealloc(names, sizeof(char *) * (nnames + 1));
        size_t l = strlen(line);
        names[nnames] = xmalloc(l + 1); memcpy(names[nnames], line, l + 1);
        nnames++;
    }
    fclose(f);
}
const char *stack_display_name(ItemStack s) {
    const char *n = item_name_get(s.name);
    return n ? n : stack_name(s.id, s.damage);
}

// ------------------------------------------------------------------------ anvil & grindstone
static int repair_material(int id) {   // item that repairs this tool/armour in an anvil (0 none)
    if (!item_valid(id)) return 0;
    const char *k = ITEMS[id].key;
    if (!strncmp(k, "wooden_", 7) || !strcmp(k, "shield")) return I_OAK_PLANKS;
    if (!strncmp(k, "stone_", 6)) return I_COBBLESTONE;
    if (!strncmp(k, "iron_", 5) || !strncmp(k, "chainmail_", 10)) return I_IRON_INGOT;
    if (!strncmp(k, "golden_", 7)) return I_GOLD_INGOT;
    if (!strncmp(k, "diamond_", 8)) return I_DIAMOND;
    if (!strncmp(k, "netherite_", 10)) return I_NETHERITE_INGOT;
    if (!strncmp(k, "leather_", 8)) return I_LEATHER;
    if (!strncmp(k, "copper_", 7)) return I_COPPER_INGOT;
    if (!strcmp(k, "turtle_helmet")) return I_TURTLE_SCUTE;
    if (!strcmp(k, "elytra")) return I_PHANTOM_MEMBRANE;
    if (!strcmp(k, "mace")) return I_BREEZE_ROD;
    return 0;
}
static bool is_material_for(int mat, int item) {
    if (mat == I_OAK_PLANKS) return strstr(ITEMS[item].key, "_planks") != NULL;
    if (mat == I_COBBLESTONE) return item == I_COBBLESTONE || item == I_COBBLED_DEEPSLATE || item == I_BLACKSTONE;
    return mat == item;
}
static int ench_multiplier(int e, bool from_book) {
    int w = ENCHANTS[e].weight, m = w >= 10 ? 1 : w >= 5 ? 2 : w >= 2 ? 4 : 8;
    return from_book ? MAXI(1, m / 2) : m;
}
ItemStack anvil_combine(ItemStack L, ItemStack R, const char *rename, int *cost, int *right_used) {
    ItemStack out = {0};
    *cost = 0; *right_used = 0;
    if (stack_empty(L) || !item_valid(L.id)) return out;
    out = L; out.count = 1;
    int c = 0;
    bool renamed = false;
    if (!stack_empty(R)) {
        const ItemDef *li = &ITEMS[L.id];
        int mat = repair_material(L.id);
        bool book = R.id == I_ENCHANTED_BOOK;
        if (li->durability && mat && is_material_for(mat, R.id) && L.damage > 0) {   // repair with material: 25% each
            int n = 0;
            while (out.damage > 0 && n < R.count) { int fix = MINI(out.damage, li->durability / 4); out.damage -= (u16)fix; n++; c++; }
            *right_used = n;
        } else if (R.id == L.id || book) {
            if (!book && li->durability) {   // combine two damaged tools
                int left = li->durability - L.damage, right = li->durability - R.damage;
                int dur = MINI(li->durability, left + right + li->durability * 12 / 100);
                int nd = li->durability - dur;
                if (nd < out.damage) { out.damage = (u16)nd; c += 2; }
            }
            bool any = false;
            for (int i = 0; i < MAX_ENCH; i++) {
                int e = R.ench[i][0], lv = R.ench[i][1];
                if (!e || e >= ENCH_COUNT) continue;
                if (!ench_applicable(e, L.id) && L.id != I_ENCHANTED_BOOK) continue;
                if (!ench_compatible(out, e)) { c += 1; continue; }
                int cur = ench_level(out, e);
                int nl = cur == lv ? MINI(cur + 1, ENCHANTS[e].max_level) : MAXI(cur, lv);
                if (ench_add(&out, e, nl)) { any = true; c += nl * ench_multiplier(e, book); }
            }
            if (!any && !(li->durability && R.id == L.id)) return (ItemStack){0};
            *right_used = 1;
        } else return (ItemStack){0};
    }
    if (rename && rename[0]) {
        const char *cur = item_name_get(L.name);
        if (!cur || strcmp(cur, rename)) { out.name = item_name_intern(rename); c += 1; renamed = true; }
    }
    if (c == 0) return (ItemStack){0};
    c += (1 << MINI(L.rcost, 10)) - 1;
    if (!stack_empty(R)) c += (1 << MINI(R.rcost, 10)) - 1;
    if (renamed && stack_empty(R)) c = MAXI(1, c);
    if (!stack_empty(R)) out.rcost = (u8)MINI(31, MAXI(L.rcost, R.rcost) + 1);
    *cost = c;
    return out;
}
ItemStack grindstone_result(ItemStack a, ItemStack b, int *xp) {
    *xp = 0;
    ItemStack in = stack_empty(a) ? b : a;
    if (stack_empty(in)) return (ItemStack){0};
    if (!stack_empty(a) && !stack_empty(b)) {
        if (a.id != b.id || !ITEMS[a.id].durability) return (ItemStack){0};
        int d = ITEMS[a.id].durability;
        int dur = MINI(d, (d - a.damage) + (d - b.damage) + d * 5 / 100);
        in = a; in.damage = (u16)(d - dur);
        for (int i = 0; i < MAX_ENCH; i++) if (b.ench[i][0] && ENCHANTS[b.ench[i][0]].curse) ench_add(&in, b.ench[i][0], b.ench[i][1]);
        for (int i = 0; i < MAX_ENCH; i++) if (b.ench[i][0] && !ENCHANTS[b.ench[i][0]].curse) *xp += ENCHANTS[b.ench[i][0]].min_base + (b.ench[i][1] - 1) * ENCHANTS[b.ench[i][0]].min_step;
    } else if (!ench_count(in) || strstr(ITEMS[in.id].key, "_banner")) return (ItemStack){0};
    ItemStack out = in; out.count = 1; out.rcost = 0;
    for (int i = 0; i < MAX_ENCH; i++) {
        int e = in.ench[i][0];
        if (!e || ENCHANTS[e].curse) continue;
        *xp += ENCHANTS[e].min_base + (in.ench[i][1] - 1) * ENCHANTS[e].min_step;
        ench_remove(&out, e);
    }
    if (out.id == I_ENCHANTED_BOOK && !ench_count(out)) out.id = I_BOOK;
    *xp = (*xp + 1) / 2 + irand(0, (*xp + 1) / 2);
    return out;
}

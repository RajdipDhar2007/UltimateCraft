#include "item.h"
#include "../world/block.h"

// key -> id lookup (open addressing hash over item keys, built once)
#define KEY_HASH 8192
static u16 key_table[KEY_HASH];
static u32 str_hash(const char *s) { u32 h = 2166136261u; while (*s) h = (h ^ (u8)*s++) * 16777619u; return h; }

void items_init(void) {
    memset(key_table, 0, sizeof key_table);
    for (int i = 1; i < ITEM_MAX; i++) {
        if (!ITEMS[i].name) continue;
        u32 h = str_hash(ITEMS[i].key) & (KEY_HASH - 1);
        while (key_table[h]) h = (h + 1) & (KEY_HASH - 1);
        key_table[h] = (u16)i;
    }
}
int item_by_key(const char *key) {
    if (!key) return 0;
    if (!strncmp(key, "minecraft:", 10)) key += 10;
    u32 h = str_hash(key) & (KEY_HASH - 1);
    while (key_table[h]) { if (!strcmp(ITEMS[key_table[h]].key, key)) return key_table[h]; h = (h + 1) & (KEY_HASH - 1); }
    return 0;
}

const char *item_name(int id) { return item_valid(id) ? ITEMS[id].name : "?"; }
const char *stack_name(u16 id, u16 damage) {
    if (!item_valid(id)) return "?";
    const ItemDef *it = &ITEMS[id];
    if (!it->variants) return it->name;
    static char buf[96];
    const char *p = POTIONS[damage % POTION_COUNT].name;
    if (id == I_POTION) return p;
    if (id == I_TIPPED_ARROW) { snprintf(buf, sizeof buf, "Arrow (%s)", p + (strncmp(p, "Potion of ", 10) ? 0 : 10)); return buf; }
    snprintf(buf, sizeof buf, "%s %s", id == I_SPLASH_POTION ? "Splash" : "Lingering", p);
    return buf;
}

// ======================================================================== inventory
int inventory_add(Inventory *inv, ItemStack s) {
    if (stack_empty(s)) return 0;
    int left = s.count, maxs = stack_max(s);
    for (int i = 0; i < INV_MAIN && left > 0; i++) {     // merge into existing stacks (hotbar first)
        ItemStack *d = &inv->slots[i];
        if (!stack_empty(*d) && stack_can_merge(*d, s) && d->count < maxs) {
            int n = MINI(left, maxs - d->count); d->count += n; left -= n;
        }
    }
    for (int i = 0; i < INV_MAIN && left > 0; i++) {     // then empty slots
        ItemStack *d = &inv->slots[i];
        if (stack_empty(*d)) { int n = MINI(left, maxs); *d = s; d->count = (u8)n; left -= n; }
    }
    return left;
}
int inventory_count(const Inventory *inv, int id) {
    int n = 0;
    for (int i = 0; i < INV_MAIN; i++) if (inv->slots[i].id == id) n += inv->slots[i].count;
    if (inv->offhand.id == id) n += inv->offhand.count;
    return n;
}
bool inventory_remove(Inventory *inv, int id, int n) {
    if (inventory_count(inv, id) < n) return false;
    if (inv->offhand.id == id && n > 0) {
        int k = MINI(n, inv->offhand.count); inv->offhand.count -= k; n -= k;
        if (!inv->offhand.count) inv->offhand = (ItemStack){0};
    }
    for (int i = INV_MAIN - 1; i >= 0 && n > 0; i--) {
        ItemStack *s = &inv->slots[i];
        if (s->id != id) continue;
        int k = MINI(n, s->count); s->count -= k; n -= k;
        if (!s->count) *s = (ItemStack){0};
    }
    return true;
}
int inventory_armor_points(const Inventory *inv) {
    int p = 0;
    for (int i = 0; i < 4; i++) if (!stack_empty(inv->armor[i])) p += ITEMS[inv->armor[i].id].armor_points;
    return p;
}
f32 inventory_armor_toughness(const Inventory *inv) {
    f32 t = 0;
    for (int i = 0; i < 4; i++) if (!stack_empty(inv->armor[i])) t += ITEMS[inv->armor[i].id].toughness10 / 10.0f;
    return t;
}

// ======================================================================== crafting
bool ingredient_matches(int ing, int item) {
    if (ing <= 0) return item == 0;
    const IngredientDef *d = &INGREDIENTS[ing];
    for (int i = 0; i < d->n; i++) if (ING_ITEMS[d->off + i] == item) return true;
    return false;
}
int recipe_count(void) { return RECIPE_COUNT; }

static bool try_shaped(const RecipeDef *r, const u16 *g, int gw, int gh, bool mirror) {
    if (r->w != gw || r->h != gh) return false;
    for (int y = 0; y < gh; y++) for (int x = 0; x < gw; x++) {
        int rx = mirror ? gw - 1 - x : x;
        if (!ingredient_matches(r->ing[y * gw + rx], g[y * gw + x])) return false;
    }
    return true;
}
static bool try_shapeless(const RecipeDef *r, const u16 *items, int n) {
    if (r->w != n) return false;
    bool used[9] = {0};
    for (int i = 0; i < n; i++) {           // greedy matching; ingredient lists are small
        bool ok = false;
        for (int j = 0; j < n; j++) if (!used[j] && ingredient_matches(r->ing[i], items[j])) { used[j] = true; ok = true; break; }
        if (!ok) return false;
    }
    return true;
}

int crafting_match_index(const ItemStack *grid, int w, int h) {
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    u16 all[9]; int nall = 0;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (!stack_empty(grid[y * w + x])) {
        x0 = MINI(x0, x); y0 = MINI(y0, y); x1 = MAXI(x1, x); y1 = MAXI(y1, y);
        all[nall++] = grid[y * w + x].id;
    }
    if (x1 < 0) return -1;
    int gw = x1 - x0 + 1, gh = y1 - y0 + 1;
    u16 g[9];
    for (int y = 0; y < gh; y++) for (int x = 0; x < gw; x++) {
        ItemStack s = grid[(y + y0) * w + x + x0];
        g[y * gw + x] = stack_empty(s) ? 0 : s.id;
    }
    for (int i = 0; i < RECIPE_COUNT; i++) {
        const RecipeDef *r = &RECIPES[i];
        if (r->shapeless ? try_shapeless(r, all, nall) : (try_shaped(r, g, gw, gh, false) || try_shaped(r, g, gw, gh, true))) return i;
    }
    return -1;
}
ItemStack crafting_match(const ItemStack *grid, int w, int h) {
    int i = crafting_match_index(grid, w, h);
    return i < 0 ? (ItemStack){0} : stack_of(RECIPES[i].result, RECIPES[i].count);
}
int crafting_remainder(int item) {
    if (!item_valid(item)) return 0;
    if (item == I_WATER_BUCKET || item == I_LAVA_BUCKET || item == I_MILK_BUCKET || item == I_POWDER_SNOW_BUCKET) return I_BUCKET;
    if (item == I_HONEY_BOTTLE || item == I_DRAGON_BREATH) return I_GLASS_BOTTLE;
    return 0;
}

// ======================================================================== smelting & friends
int smelting_result(int id, int kind_mask) {
    for (int i = 0; i < SMELT_COUNT; i++)
        if ((SMELTS[i].kinds & kind_mask) && ingredient_matches(SMELTS[i].ing, id)) return SMELTS[i].result;
    return 0;
}
f32 smelting_xp(int id) {
    for (int i = 0; i < SMELT_COUNT; i++) if (ingredient_matches(SMELTS[i].ing, id)) return SMELTS[i].xp100 / 100.0f;
    return 0;
}
int fuel_value(int id) { return item_valid(id) ? ITEMS[id].fuel_ticks : 0; }

int stonecutter_options(int input, ItemStack *out, int max) {
    int n = 0;
    for (int i = 0; i < CUT_COUNT && n < max; i++) if (CUTS[i].input == input) out[n++] = stack_of(CUTS[i].result, CUTS[i].count);
    return n;
}
ItemStack smithing_result(ItemStack t, ItemStack b, ItemStack a) {
    if (stack_empty(t) || stack_empty(b) || stack_empty(a)) return (ItemStack){0};
    for (int i = 0; i < SMITH_COUNT; i++)
        if (SMITHS[i].tmpl == t.id && SMITHS[i].base == b.id && SMITHS[i].addition == a.id) {
            ItemStack r = stack_of(SMITHS[i].result, 1);
            // keep the relative wear of the upgraded item
            if (ITEMS[b.id].durability && ITEMS[r.id].durability) r.damage = (u16)((u32)b.damage * ITEMS[r.id].durability / ITEMS[b.id].durability);
            return r;
        }
    return (ItemStack){0};
}

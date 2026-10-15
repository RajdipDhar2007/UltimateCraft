// Statistics and advancements (UltimateCraft's own, original names): saved per world in stats.txt and
// advancements.txt. Advancements are checked once a second (cheap polls of the inventory, dimension, structure,
// trial flags and statistics) or granted directly by events; a toast appears and, with gamerule
// announceAdvancements, the chat announces it. /advancement grant|revoke <id>|everything, /advancement list.
#include "game.h"
#include "player.h"
#include "../world/rules.h"
#include "../world/structures.h"
#include "../audio/audio.h"

const char *STAT_NAMES[STAT_COUNT] = {"Blocks mined", "Blocks placed", "Mobs killed", "Deaths", "Distance walked (m)", "Jumps", "Damage dealt",
    "Damage taken", "Items crafted", "Fish caught", "Time played (min)", "Bosses defeated", "Advancements"};
long long g_stats[STAT_COUNT];
static f64 walk_acc;

typedef struct { const char *id, *title, *desc; } AdvDef;
static const AdvDef ADV[] = {
    {"first_log", "Taking Root", "Collect a log"},
    {"stone_age", "Hard as Stone", "Mine cobblestone"},
    {"stone_tools", "Sharper Edges", "Craft a stone pickaxe"},
    {"iron", "Forged in Fire", "Smelt an iron ingot"},
    {"diamonds", "Sparkle", "Find a diamond"},
    {"bed", "Sweet Dreams", "Sleep through a night"},
    {"monster", "First Blood", "Defeat a hostile creature"},
    {"fish", "Gone Fishing", "Catch something with a rod"},
    {"trade", "Fair Deal", "Trade with anyone"},
    {"enchant", "Glimmering", "Enchant an item"},
    {"hell", "Into the Fire", "Enter Hell"},
    {"ghost", "Unhaunted", "Defeat a ghost of any kind"},
    {"holy_water", "Sanctified", "Brew or find holy water"},
    {"revenant", "Ash and Bone", "Defeat the Brimstone Revenant"},
    {"city", "The Lamps Below", "Find the Underground City"},
    {"keeper", "Keeper No More", "Defeat Morrowgate, Keeper of the Deep"},
    {"void_sigil", "The Key to the Stars", "Hold a Void Sigil"},
    {"space", "Among the Stars", "Travel to Space"},
    {"oxygen", "Deep Breath", "Use an Oxygen Canister"},
    {"tyrant", "Starfall", "Defeat Zorvath, the Cosmic Tyrant"},
    {"crown", "Heavy Is the Head", "Wear the Tyrant's Crown"},
    {"sift", "Above the Clouds", "Enter the Sift"},
    {"soul", "Kindred Spirit", "Talk to a wandering soul"},
    {"dawn", "Walk in the Dawn", "Pass Solenne's trial"},
    {"winds", "On the High Winds", "Pass Zephyrine's trial"},
    {"stone", "Stoneheart", "Pass Thalor's trial"},
    {"ascension", "Ascension", "Receive all three blessings of the Luminaries"},
    {"divine", "Touched by Light", "Craft a piece of divine gear"},
    {"sculk", "Shh...", "Make a sculk sensor listen to you"},
    {"painter", "Gallery", "Hang a painting"},
};
#define NADV ((int)(sizeof ADV / sizeof ADV[0]))
static bool done[64];
int adv_count(void) { return NADV; }
bool adv_done(int i) { return i >= 0 && i < NADV && done[i]; }
const char *adv_title(int i) { return ADV[i].title; }
const char *adv_desc(int i) { return ADV[i].desc; }
int adv_by_id(const char *id) { for (int i = 0; i < NADV; i++) if (!strcmp(ADV[i].id, id)) return i; return -1; }
void adv_grant(int i) {
    if (i < 0 || i >= NADV || done[i]) return;
    done[i] = true; g_stats[STAT_ADVANCEMENTS]++;
    snprintf(G.toast, sizeof G.toast, "Advancement Made! %s", ADV[i].title); G.toast_t = 5 * TICKRATE;
    if (rule(GR_ANNOUNCE_ADVANCEMENTS)) game_message(rgba_u32(85, 255, 85, 255), "Player has made the advancement [%s]", ADV[i].title);
    sound_play(SND_LEVELUP, 0.6f, 1.4f);
}
void adv_revoke(int i) { if (i >= 0 && i < NADV && done[i]) { done[i] = false; g_stats[STAT_ADVANCEMENTS]--; } }
void progress_grant(const char *id) { adv_grant(adv_by_id(id)); }

static bool has(int id) { return id > 0 && inventory_count(&g_player.inv, id) > 0; }
static bool has_log(void) {
    for (int i = 0; i < 36; i++) { const ItemStack *s = &g_player.inv.slots[i]; if (s->id && ITEMS[s->id].key && (strstr(ITEMS[s->id].key, "_log") || strstr(ITEMS[s->id].key, "_stem"))) return true; }
    return false;
}
void progress_tick(void) {
    Entity *p = player_ent();
    if (!p || !G.in_world) return;
    if (G.ticks % (60 * TICKRATE) == 0) g_stats[STAT_TIME]++;
    if (G.ticks % TICKRATE) return;
    if (has_log()) progress_grant("first_log");
    if (has(I_COBBLESTONE)) progress_grant("stone_age");
    if (has(I_STONE_PICKAXE)) progress_grant("stone_tools");
    if (has(I_IRON_INGOT)) progress_grant("iron");
    if (has(I_DIAMOND)) progress_grant("diamonds");
    if (has(I_HOLY_WATER)) progress_grant("holy_water");
    if (has(I_VOID_SIGIL)) progress_grant("void_sigil");
    if (g_player.inv.armor[0].id == I_TYRANT_CROWN) progress_grant("crown");
    for (int i = 0; i < 4; i++) if (g_player.inv.armor[i].id && ITEMS[g_player.inv.armor[i].id].key && !strncmp(ITEMS[g_player.inv.armor[i].id].key, "divine_", 7)) progress_grant("divine");
    if (has(I_DIVINE_SWORD) || has(I_DIVINE_PICKAXE)) progress_grant("divine");
    if (g_world.dim == DIM_NETHER) progress_grant("hell");
    if (g_world.dim == DIM_END) progress_grant("space");
    if (g_world.dim == DIM_SIFT) progress_grant("sift");
    if (structure_at((int)floorf(p->pos.x), (int)floorf(p->pos.y), (int)floorf(p->pos.z)) == ST_UNDERGROUND_CITY) progress_grant("city");
    if (g_world.tyrant_killed) progress_grant("tyrant");
    if (g_world.sift_trials & 1) progress_grant("dawn");
    if (g_world.sift_trials & 2) progress_grant("winds");
    if (g_world.sift_trials & 4) progress_grant("stone");
    if (g_world.ascended) progress_grant("ascension");
    if (effect_has(p, EFF_OXYGEN)) progress_grant("oxygen");
    if (g_stats[STAT_FISH] > 0) progress_grant("fish");
    for (int i = 0; i < 36; i++) if (g_player.inv.slots[i].id && g_player.inv.slots[i].id != I_ENCHANTED_BOOK && g_player.inv.slots[i].ench[0][0] != 0) { progress_grant("enchant"); break; }
}
// ---- events
void progress_mob_killed(void *mob_entity) {
    Entity *m = (Entity *)mob_entity;
    g_stats[STAT_KILLS]++;
    if (mob_hostile(m->mob)) progress_grant("monster");
    if (MOB_DEFS[m->mob].ai == AIK_GHOST) progress_grant("ghost");
    if (mob_has(m->mob, MF_BOSS)) g_stats[STAT_BOSSES]++;
    if (m->mob == MOB_BRIMSTONE_REVENANT) progress_grant("revenant");
    if (m->mob == MOB_DEEP_KEEPER) progress_grant("keeper");
    if (m->mob == MOB_COSMIC_TYRANT) progress_grant("tyrant");
}
void progress_moved(f32 dist) { walk_acc += dist; if (walk_acc >= 1) { g_stats[STAT_WALK] += (long long)walk_acc; walk_acc -= floor(walk_acc); } }
// ---- persistence
void progress_save(const char *dir) {
    char path[320]; snprintf(path, sizeof path, "%.300s/stats.txt", dir);
    StrBuf b = {0};
    for (int i = 0; i < STAT_COUNT; i++) sbuf_printf(&b, "%d %lld\n", i, g_stats[i]);
    sbuf_write(&b, path);
    snprintf(path, sizeof path, "%.300s/advancements.txt", dir);
    for (int i = 0; i < NADV; i++) if (done[i]) sbuf_printf(&b, "%s\n", ADV[i].id);
    sbuf_write(&b, path);
}
void progress_load(const char *dir) {
    memset(g_stats, 0, sizeof g_stats); memset(done, 0, sizeof done); walk_acc = 0;
    char path[320], line[128]; snprintf(path, sizeof path, "%.300s/stats.txt", dir);
    FILE *f = fopen(path, "r");
    if (f) { int i; long long v; while (fgets(line, sizeof line, f)) if (sscanf(line, "%d %lld", &i, &v) == 2 && i >= 0 && i < STAT_COUNT) g_stats[i] = v; fclose(f); }
    snprintf(path, sizeof path, "%.300s/advancements.txt", dir);
    f = fopen(path, "r");
    if (f) { while (fgets(line, sizeof line, f)) { line[strcspn(line, "\r\n")] = 0; int a = adv_by_id(line); if (a >= 0) done[a] = true; } fclose(f); }
}

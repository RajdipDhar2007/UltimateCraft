#include "rules.h"
#include <ctype.h>

#define GR_DEF(id, key, isbool, def, lo, hi) [GR_##id] = {key, isbool, def, lo, hi},
const GameRuleDef GAMERULE_DEFS[GR_COUNT] = { GAMERULES(GR_DEF) };
#undef GR_DEF
int g_rules[GR_COUNT];
LevelState g_level;
const char *const DIFFICULTY_NAMES[4] = {"peaceful", "easy", "normal", "hard"};

void rules_reset(void) { for (int i = 0; i < GR_COUNT; i++) g_rules[i] = GAMERULE_DEFS[i].def; }
int rule_by_key(const char *key) {
    for (int i = 0; i < GR_COUNT; i++) {
        const char *a = GAMERULE_DEFS[i].key, *b = key;
        if (!strncmp(b, "minecraft:", 10)) b += 10;
        while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; }
        if (!*a && !*b) return i;
    }
    return -1;
}
void level_state_reset(void) {
    memset(&g_level, 0, sizeof g_level);
    g_level.difficulty = DIFF_NORMAL;
    g_level.cheats = false;
    g_level.clear_time = 0;
    g_level.rain_time = 12000 + (int)(rand() % 156000);    // first rain after 0.5 .. 7 in-game days
    g_level.thunder_time = 12000 + (int)(rand() % 156000);
    g_level.patrol_timer = 12000;
    g_level.trader_timer = 24000;
    g_level.trader_chance = 25;
    rules_reset();
}

// Minecraft game rules (/gamerule), difficulty and weather state shared by every module.
#ifndef RULES_H
#define RULES_H
#include "../core/common.h"

#define GAMERULES(X) \
    X(ANNOUNCE_ADVANCEMENTS,        "announceAdvancements",        1, 1, 0, 1) \
    X(BLOCK_EXPLOSION_DROP_DECAY,   "blockExplosionDropDecay",     1, 1, 0, 1) \
    X(COMMAND_BLOCK_OUTPUT,         "commandBlockOutput",          1, 1, 0, 1) \
    X(COMMAND_MODIFICATION_LIMIT,   "commandModificationBlockLimit", 0, 32768, 1, 1000000) \
    X(DISABLE_ELYTRA_CHECK,         "disableElytraMovementCheck",  1, 0, 0, 1) \
    X(DISABLE_PLAYER_MOVE_CHECK,    "disablePlayerMovementCheck",  1, 0, 0, 1) \
    X(DISABLE_RAIDS,                "disableRaids",                1, 0, 0, 1) \
    X(DO_DAYLIGHT_CYCLE,            "doDaylightCycle",             1, 1, 0, 1) \
    X(DO_ENTITY_DROPS,              "doEntityDrops",               1, 1, 0, 1) \
    X(DO_FIRE_TICK,                 "doFireTick",                  1, 1, 0, 1) \
    X(DO_IMMEDIATE_RESPAWN,         "doImmediateRespawn",          1, 0, 0, 1) \
    X(DO_INSOMNIA,                  "doInsomnia",                  1, 1, 0, 1) \
    X(DO_LIMITED_CRAFTING,          "doLimitedCrafting",           1, 0, 0, 1) \
    X(DO_MOB_LOOT,                  "doMobLoot",                   1, 1, 0, 1) \
    X(DO_MOB_SPAWNING,              "doMobSpawning",               1, 1, 0, 1) \
    X(DO_PATROL_SPAWNING,           "doPatrolSpawning",            1, 1, 0, 1) \
    X(DO_TILE_DROPS,                "doTileDrops",                 1, 1, 0, 1) \
    X(DO_TRADER_SPAWNING,           "doTraderSpawning",            1, 1, 0, 1) \
    X(DO_VINES_SPREAD,              "doVinesSpread",               1, 1, 0, 1) \
    X(DO_WARDEN_SPAWNING,           "doWardenSpawning",            1, 1, 0, 1) \
    X(DO_WEATHER_CYCLE,             "doWeatherCycle",              1, 1, 0, 1) \
    X(DROWNING_DAMAGE,              "drowningDamage",              1, 1, 0, 1) \
    X(ENDER_PEARLS_VANISH,          "enderPearlsVanishOnDeath",    1, 1, 0, 1) \
    X(FALL_DAMAGE,                  "fallDamage",                  1, 1, 0, 1) \
    X(FIRE_DAMAGE,                  "fireDamage",                  1, 1, 0, 1) \
    X(FORGIVE_DEAD_PLAYERS,         "forgiveDeadPlayers",          1, 1, 0, 1) \
    X(FREEZE_DAMAGE,                "freezeDamage",                1, 1, 0, 1) \
    X(GLOBAL_SOUND_EVENTS,          "globalSoundEvents",           1, 1, 0, 1) \
    X(KEEP_INVENTORY,               "keepInventory",               1, 0, 0, 1) \
    X(LAVA_SOURCE_CONVERSION,       "lavaSourceConversion",        1, 0, 0, 1) \
    X(LOG_ADMIN_COMMANDS,           "logAdminCommands",            1, 1, 0, 1) \
    X(MAX_COMMAND_CHAIN_LENGTH,     "maxCommandChainLength",       0, 65536, 0, 1000000) \
    X(MAX_COMMAND_FORK_COUNT,       "maxCommandForkCount",         0, 65536, 0, 1000000) \
    X(MAX_ENTITY_CRAMMING,          "maxEntityCramming",           0, 24, 0, 1000) \
    X(MINECART_MAX_SPEED,           "minecartMaxSpeed",            0, 8, 1, 1000) \
    X(MOB_EXPLOSION_DROP_DECAY,     "mobExplosionDropDecay",       1, 1, 0, 1) \
    X(MOB_GRIEFING,                 "mobGriefing",                 1, 1, 0, 1) \
    X(NATURAL_REGENERATION,         "naturalRegeneration",         1, 1, 0, 1) \
    X(PLAYERS_NETHER_PORTAL_CREATIVE_DELAY, "playersNetherPortalCreativeDelay", 0, 1, 0, 100000) \
    X(PLAYERS_NETHER_PORTAL_DEFAULT_DELAY,  "playersNetherPortalDefaultDelay",  0, 80, 0, 100000) \
    X(PLAYERS_SLEEPING_PERCENTAGE,  "playersSleepingPercentage",   0, 100, 0, 100000) \
    X(PROJECTILES_CAN_BREAK_BLOCKS, "projectilesCanBreakBlocks",   1, 1, 0, 1) \
    X(RANDOM_TICK_SPEED,            "randomTickSpeed",             0, 3, 0, 4096) \
    X(REDUCED_DEBUG_INFO,           "reducedDebugInfo",            1, 0, 0, 1) \
    X(SEND_COMMAND_FEEDBACK,        "sendCommandFeedback",         1, 1, 0, 1) \
    X(SHOW_DEATH_MESSAGES,          "showDeathMessages",           1, 1, 0, 1) \
    X(SNOW_ACCUMULATION_HEIGHT,     "snowAccumulationHeight",      0, 1, 0, 8) \
    X(SPAWN_CHUNK_RADIUS,           "spawnChunkRadius",            0, 2, 0, 32) \
    X(SPAWN_RADIUS,                 "spawnRadius",                 0, 10, 0, 1000) \
    X(SPAWNER_BLOCKS_ENABLED,       "spawnerBlocksEnabled",        1, 1, 0, 1) \
    X(SPECTATORS_GENERATE_CHUNKS,   "spectatorsGenerateChunks",    1, 1, 0, 1) \
    X(TNT_EXPLODES,                 "tntExplodes",                 1, 1, 0, 1) \
    X(TNT_EXPLOSION_DROP_DECAY,     "tntExplosionDropDecay",       1, 0, 0, 1) \
    X(UNIVERSAL_ANGER,              "universalAnger",              1, 0, 0, 1) \
    X(WATER_SOURCE_CONVERSION,      "waterSourceConversion",       1, 1, 0, 1)

#define GR_ENUM(id, key, isbool, def, lo, hi) GR_##id,
typedef enum { GAMERULES(GR_ENUM) GR_COUNT } GameRuleId;
#undef GR_ENUM
typedef struct { const char *key; bool is_bool; int def, lo, hi; } GameRuleDef;
extern const GameRuleDef GAMERULE_DEFS[GR_COUNT];
extern int g_rules[GR_COUNT];
static inline int rule_i(int id) { return g_rules[id]; }
static inline bool rule(int id) { return g_rules[id] != 0; }
void rules_reset(void);
int rule_by_key(const char *key);   // -1 when unknown (case-insensitive)

// ---------------------------------------------------------------- difficulty
typedef enum { DIFF_PEACEFUL, DIFF_EASY, DIFF_NORMAL, DIFF_HARD } Difficulty;
extern const char *const DIFFICULTY_NAMES[4];

// ---------------------------------------------------------------- level state (saved in level.txt)
typedef struct {
    u8 difficulty;
    bool difficulty_locked, hardcore, cheats;
    // weather, in Minecraft ticks (20 per second) like level.dat
    bool raining, thundering;
    int rain_time, thunder_time, clear_time;
    f32 rain_level, thunder_level;     // smoothed 0..1 (rendering, sky darkening)
    f32 prev_rain_level, prev_thunder_level;
    int lightning_flash;               // sky flash frames
    u64 day_count_raids;               // reserved
    int patrol_timer, trader_timer;    // ticks until the next patrol / wandering trader attempt
    int trader_chance;                 // 25 / 50 / 75 like Minecraft
} LevelState;
extern LevelState g_level;
void level_state_reset(void);
// Minecraft ticks happen 20 times a second; the engine runs at TICKRATE. True on the frames that
// complete a Minecraft tick (TICKRATE is a multiple of 20).
#define MC_TICK_DIV (TICKRATE / 20)
#endif

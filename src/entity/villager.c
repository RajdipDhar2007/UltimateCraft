// Villagers: professions (taken from the nearest job-site block), Minecraft trade tables per
// profession and level, villager experience / levelling, restocking and wandering traders.
#include "entity.h"
#include "villager.h"
#include "../world/world.h"
#include "../game/player.h"
#include "../game/fx.h"
#include "../audio/audio.h"

const char *PROF_NAMES[PROF_COUNT] = {"Unemployed", "Armorer", "Butcher", "Cartographer", "Cleric", "Farmer", "Fisherman", "Fletcher",
                                      "Leatherworker", "Librarian", "Mason", "Shepherd", "Toolsmith", "Weaponsmith", "Nitwit", "Wandering Trader", "Delvari Merchant", "Wandering Soul", "Zephyrine"};
static const char *LEVEL_NAMES[6] = {"", "Novice", "Apprentice", "Journeyman", "Expert", "Master"};
const char *villager_level_name(int l) { return LEVEL_NAMES[clampi(l, 1, 5)]; }

int profession_of_block(Block b) {
    switch (BLOCKS[b].base) {
    case B_BLAST_FURNACE: return PROF_ARMORER;
    case B_SMOKER: return PROF_BUTCHER;
    case B_CARTOGRAPHY_TABLE: return PROF_CARTOGRAPHER;
    case B_BREWING_STAND: return PROF_CLERIC;
    case B_COMPOSTER: return PROF_FARMER;
    case B_BARREL: return PROF_FISHERMAN;
    case B_FLETCHING_TABLE: return PROF_FLETCHER;
    case B_CAULDRON: case B_WATER_CAULDRON: return PROF_LEATHERWORKER;
    case B_LECTERN: return PROF_LIBRARIAN;
    case B_STONECUTTER: return PROF_MASON;
    case B_LOOM: return PROF_SHEPHERD;
    case B_SMITHING_TABLE: return PROF_TOOLSMITH;
    case B_GRINDSTONE: return PROF_WEAPONSMITH;
    default: return PROF_NONE;
    }
}

// trade table: "level costA nA costB nB result nR maxUses xp" ; "*" in an item name = enchanted (random)
typedef struct { u8 prof, level; const char *a; u8 na; const char *b; u8 nb; const char *r; u8 nr; u8 uses, xp; } TradeDef;
static const TradeDef TRADES[] = {
    // armorer
    {PROF_ARMORER, 1, "coal", 15, 0, 0, "emerald", 1, 16, 2}, {PROF_ARMORER, 1, "emerald", 5, 0, 0, "iron_helmet", 1, 12, 1}, {PROF_ARMORER, 1, "emerald", 9, 0, 0, "iron_chestplate", 1, 12, 1},
    {PROF_ARMORER, 1, "emerald", 7, 0, 0, "iron_leggings", 1, 12, 1}, {PROF_ARMORER, 1, "emerald", 4, 0, 0, "iron_boots", 1, 12, 1},
    {PROF_ARMORER, 2, "iron_ingot", 4, 0, 0, "emerald", 1, 12, 10}, {PROF_ARMORER, 2, "emerald", 36, 0, 0, "bell", 1, 12, 5}, {PROF_ARMORER, 2, "emerald", 1, 0, 0, "chainmail_boots", 1, 12, 5}, {PROF_ARMORER, 2, "emerald", 3, 0, 0, "chainmail_leggings", 1, 12, 5},
    {PROF_ARMORER, 3, "lava_bucket", 1, 0, 0, "emerald", 1, 12, 20}, {PROF_ARMORER, 3, "diamond", 1, 0, 0, "emerald", 1, 12, 20}, {PROF_ARMORER, 3, "emerald", 1, 0, 0, "chainmail_helmet", 1, 12, 10}, {PROF_ARMORER, 3, "emerald", 4, 0, 0, "chainmail_chestplate", 1, 12, 10}, {PROF_ARMORER, 3, "emerald", 5, 0, 0, "shield", 1, 12, 10},
    {PROF_ARMORER, 4, "emerald", 19, 0, 0, "*diamond_leggings", 1, 3, 15}, {PROF_ARMORER, 4, "emerald", 13, 0, 0, "*diamond_boots", 1, 3, 15},
    {PROF_ARMORER, 5, "emerald", 13, 0, 0, "*diamond_helmet", 1, 3, 30}, {PROF_ARMORER, 5, "emerald", 21, 0, 0, "*diamond_chestplate", 1, 3, 30},
    // butcher
    {PROF_BUTCHER, 1, "chicken", 14, 0, 0, "emerald", 1, 16, 2}, {PROF_BUTCHER, 1, "porkchop", 7, 0, 0, "emerald", 1, 16, 2}, {PROF_BUTCHER, 1, "rabbit", 4, 0, 0, "emerald", 1, 16, 2}, {PROF_BUTCHER, 1, "emerald", 1, 0, 0, "rabbit_stew", 1, 12, 1},
    {PROF_BUTCHER, 2, "coal", 15, 0, 0, "emerald", 1, 16, 2}, {PROF_BUTCHER, 2, "emerald", 1, 0, 0, "cooked_porkchop", 5, 16, 5}, {PROF_BUTCHER, 2, "emerald", 1, 0, 0, "cooked_chicken", 8, 16, 5},
    {PROF_BUTCHER, 3, "mutton", 7, 0, 0, "emerald", 1, 16, 20}, {PROF_BUTCHER, 3, "beef", 10, 0, 0, "emerald", 1, 16, 20},
    {PROF_BUTCHER, 4, "dried_kelp_block", 10, 0, 0, "emerald", 1, 12, 30}, {PROF_BUTCHER, 5, "sweet_berries", 10, 0, 0, "emerald", 1, 12, 30},
    // cartographer
    {PROF_CARTOGRAPHER, 1, "paper", 24, 0, 0, "emerald", 1, 16, 2}, {PROF_CARTOGRAPHER, 1, "emerald", 7, 0, 0, "map", 1, 12, 1},
    {PROF_CARTOGRAPHER, 2, "glass_pane", 11, 0, 0, "emerald", 1, 16, 10}, {PROF_CARTOGRAPHER, 2, "emerald", 13, "compass", 1, "filled_map", 1, 12, 5},
    {PROF_CARTOGRAPHER, 3, "compass", 1, 0, 0, "emerald", 1, 12, 20}, {PROF_CARTOGRAPHER, 3, "emerald", 14, "compass", 1, "filled_map", 1, 12, 10},
    {PROF_CARTOGRAPHER, 4, "emerald", 7, 0, 0, "item_frame", 1, 12, 15}, {PROF_CARTOGRAPHER, 4, "emerald", 3, 0, 0, "white_banner", 1, 12, 15},
    {PROF_CARTOGRAPHER, 5, "emerald", 8, 0, 0, "globe_banner_pattern", 1, 12, 30},
    // cleric
    {PROF_CLERIC, 1, "rotten_flesh", 32, 0, 0, "emerald", 1, 16, 2}, {PROF_CLERIC, 1, "emerald", 1, 0, 0, "redstone", 2, 12, 1},
    {PROF_CLERIC, 2, "gold_ingot", 3, 0, 0, "emerald", 1, 12, 10}, {PROF_CLERIC, 2, "emerald", 1, 0, 0, "lapis_lazuli", 1, 12, 5},
    {PROF_CLERIC, 3, "rabbit_foot", 2, 0, 0, "emerald", 1, 12, 20}, {PROF_CLERIC, 3, "emerald", 4, 0, 0, "glowstone", 1, 12, 10},
    {PROF_CLERIC, 4, "turtle_scute", 4, 0, 0, "emerald", 1, 12, 30}, {PROF_CLERIC, 4, "glass_bottle", 9, 0, 0, "emerald", 1, 12, 30}, {PROF_CLERIC, 4, "emerald", 5, 0, 0, "ender_pearl", 1, 12, 15},
    {PROF_CLERIC, 5, "nether_wart", 22, 0, 0, "emerald", 1, 12, 30}, {PROF_CLERIC, 5, "emerald", 3, 0, 0, "experience_bottle", 1, 12, 30},
    // farmer
    {PROF_FARMER, 1, "wheat", 20, 0, 0, "emerald", 1, 16, 2}, {PROF_FARMER, 1, "potato", 26, 0, 0, "emerald", 1, 16, 2}, {PROF_FARMER, 1, "carrot", 22, 0, 0, "emerald", 1, 16, 2}, {PROF_FARMER, 1, "beetroot", 15, 0, 0, "emerald", 1, 16, 2}, {PROF_FARMER, 1, "emerald", 1, 0, 0, "bread", 6, 16, 1},
    {PROF_FARMER, 2, "pumpkin", 6, 0, 0, "emerald", 1, 12, 10}, {PROF_FARMER, 2, "emerald", 1, 0, 0, "pumpkin_pie", 4, 12, 5}, {PROF_FARMER, 2, "emerald", 1, 0, 0, "apple", 4, 16, 5},
    {PROF_FARMER, 3, "melon", 4, 0, 0, "emerald", 1, 12, 20}, {PROF_FARMER, 3, "emerald", 3, 0, 0, "cookie", 18, 12, 10},
    {PROF_FARMER, 4, "emerald", 1, 0, 0, "cake", 1, 12, 15}, {PROF_FARMER, 4, "emerald", 1, 0, 0, "suspicious_stew", 1, 12, 15},
    {PROF_FARMER, 5, "emerald", 3, 0, 0, "golden_carrot", 3, 12, 30}, {PROF_FARMER, 5, "emerald", 4, 0, 0, "glistering_melon_slice", 3, 12, 30},
    // fisherman
    {PROF_FISHERMAN, 1, "string", 20, 0, 0, "emerald", 1, 16, 2}, {PROF_FISHERMAN, 1, "coal", 10, 0, 0, "emerald", 1, 16, 2}, {PROF_FISHERMAN, 1, "emerald", 1, "cod", 6, "cooked_cod", 6, 16, 1}, {PROF_FISHERMAN, 1, "emerald", 3, 0, 0, "cod_bucket", 1, 16, 1},
    {PROF_FISHERMAN, 2, "cod", 15, 0, 0, "emerald", 1, 16, 10}, {PROF_FISHERMAN, 2, "emerald", 1, "salmon", 6, "cooked_salmon", 6, 16, 5}, {PROF_FISHERMAN, 2, "emerald", 2, 0, 0, "campfire", 1, 12, 5},
    {PROF_FISHERMAN, 3, "salmon", 13, 0, 0, "emerald", 1, 16, 20}, {PROF_FISHERMAN, 3, "emerald", 8, 0, 0, "*fishing_rod", 1, 3, 10},
    {PROF_FISHERMAN, 4, "tropical_fish", 6, 0, 0, "emerald", 1, 12, 30}, {PROF_FISHERMAN, 5, "pufferfish", 4, 0, 0, "emerald", 1, 12, 30}, {PROF_FISHERMAN, 5, "oak_boat", 1, 0, 0, "emerald", 1, 12, 30},
    // fletcher
    {PROF_FLETCHER, 1, "stick", 32, 0, 0, "emerald", 1, 16, 2}, {PROF_FLETCHER, 1, "emerald", 1, 0, 0, "arrow", 16, 12, 1}, {PROF_FLETCHER, 1, "emerald", 1, "gravel", 10, "flint", 10, 12, 1},
    {PROF_FLETCHER, 2, "flint", 26, 0, 0, "emerald", 1, 12, 10}, {PROF_FLETCHER, 2, "emerald", 2, 0, 0, "bow", 1, 12, 5},
    {PROF_FLETCHER, 3, "string", 14, 0, 0, "emerald", 1, 16, 20}, {PROF_FLETCHER, 3, "emerald", 3, 0, 0, "crossbow", 1, 12, 10},
    {PROF_FLETCHER, 4, "feather", 24, 0, 0, "emerald", 1, 16, 30}, {PROF_FLETCHER, 4, "emerald", 7, 0, 0, "*bow", 1, 3, 15},
    {PROF_FLETCHER, 5, "tripwire_hook", 8, 0, 0, "emerald", 1, 12, 30}, {PROF_FLETCHER, 5, "emerald", 8, 0, 0, "*crossbow", 1, 3, 15}, {PROF_FLETCHER, 5, "emerald", 2, "arrow", 5, "tipped_arrow", 5, 12, 30},
    // leatherworker
    {PROF_LEATHERWORKER, 1, "leather", 6, 0, 0, "emerald", 1, 16, 2}, {PROF_LEATHERWORKER, 1, "emerald", 3, 0, 0, "leather_leggings", 1, 12, 1}, {PROF_LEATHERWORKER, 1, "emerald", 7, 0, 0, "leather_chestplate", 1, 12, 1},
    {PROF_LEATHERWORKER, 2, "flint", 26, 0, 0, "emerald", 1, 12, 10}, {PROF_LEATHERWORKER, 2, "emerald", 5, 0, 0, "leather_helmet", 1, 12, 5}, {PROF_LEATHERWORKER, 2, "emerald", 4, 0, 0, "leather_boots", 1, 12, 5},
    {PROF_LEATHERWORKER, 3, "rabbit_hide", 9, 0, 0, "emerald", 1, 12, 20}, {PROF_LEATHERWORKER, 3, "emerald", 7, 0, 0, "leather_chestplate", 1, 12, 10},
    {PROF_LEATHERWORKER, 4, "turtle_scute", 4, 0, 0, "emerald", 1, 12, 30}, {PROF_LEATHERWORKER, 4, "emerald", 6, 0, 0, "leather_horse_armor", 1, 12, 15},
    {PROF_LEATHERWORKER, 5, "emerald", 6, 0, 0, "saddle", 1, 12, 30}, {PROF_LEATHERWORKER, 5, "emerald", 5, 0, 0, "leather_helmet", 1, 12, 30},
    // librarian
    {PROF_LIBRARIAN, 1, "paper", 24, 0, 0, "emerald", 1, 16, 2}, {PROF_LIBRARIAN, 1, "emerald", 9, 0, 0, "bookshelf", 1, 12, 1}, {PROF_LIBRARIAN, 1, "emerald", 5, "book", 1, "*enchanted_book", 1, 12, 1},
    {PROF_LIBRARIAN, 2, "book", 4, 0, 0, "emerald", 1, 12, 10}, {PROF_LIBRARIAN, 2, "emerald", 1, 0, 0, "lantern", 1, 12, 5}, {PROF_LIBRARIAN, 2, "emerald", 12, "book", 1, "*enchanted_book", 1, 12, 5},
    {PROF_LIBRARIAN, 3, "ink_sac", 5, 0, 0, "emerald", 1, 12, 20}, {PROF_LIBRARIAN, 3, "emerald", 1, 0, 0, "glass", 4, 12, 10}, {PROF_LIBRARIAN, 3, "emerald", 18, "book", 1, "*enchanted_book", 1, 12, 10},
    {PROF_LIBRARIAN, 4, "writable_book", 2, 0, 0, "emerald", 1, 12, 30}, {PROF_LIBRARIAN, 4, "emerald", 5, 0, 0, "clock", 1, 12, 15}, {PROF_LIBRARIAN, 4, "emerald", 4, 0, 0, "compass", 1, 12, 15},
    {PROF_LIBRARIAN, 5, "emerald", 20, 0, 0, "name_tag", 1, 12, 30}, {PROF_LIBRARIAN, 5, "emerald", 25, "book", 1, "*enchanted_book", 1, 12, 30},
    // mason
    {PROF_MASON, 1, "clay_ball", 10, 0, 0, "emerald", 1, 16, 2}, {PROF_MASON, 1, "emerald", 1, 0, 0, "bricks", 10, 16, 1},
    {PROF_MASON, 2, "stone", 20, 0, 0, "emerald", 1, 16, 10}, {PROF_MASON, 2, "emerald", 1, 0, 0, "chiseled_stone_bricks", 4, 16, 5},
    {PROF_MASON, 3, "granite", 16, 0, 0, "emerald", 1, 16, 20}, {PROF_MASON, 3, "andesite", 16, 0, 0, "emerald", 1, 16, 20}, {PROF_MASON, 3, "emerald", 1, 0, 0, "polished_andesite", 4, 16, 10},
    {PROF_MASON, 4, "quartz", 12, 0, 0, "emerald", 1, 12, 30}, {PROF_MASON, 4, "emerald", 1, 0, 0, "orange_terracotta", 1, 12, 15}, {PROF_MASON, 4, "emerald", 1, 0, 0, "red_glazed_terracotta", 1, 12, 15},
    {PROF_MASON, 5, "emerald", 1, 0, 0, "quartz_pillar", 1, 12, 30}, {PROF_MASON, 5, "emerald", 1, 0, 0, "quartz_block", 1, 12, 30},
    // shepherd
    {PROF_SHEPHERD, 1, "white_wool", 18, 0, 0, "emerald", 1, 16, 2}, {PROF_SHEPHERD, 1, "brown_wool", 18, 0, 0, "emerald", 1, 16, 2}, {PROF_SHEPHERD, 1, "emerald", 2, 0, 0, "shears", 1, 12, 1},
    {PROF_SHEPHERD, 2, "white_dye", 12, 0, 0, "emerald", 1, 16, 10}, {PROF_SHEPHERD, 2, "emerald", 1, 0, 0, "red_wool", 1, 16, 5}, {PROF_SHEPHERD, 2, "emerald", 1, 0, 0, "blue_carpet", 4, 16, 5},
    {PROF_SHEPHERD, 3, "yellow_dye", 12, 0, 0, "emerald", 1, 16, 20}, {PROF_SHEPHERD, 3, "emerald", 3, 0, 0, "white_bed", 1, 12, 10},
    {PROF_SHEPHERD, 4, "light_blue_dye", 12, 0, 0, "emerald", 1, 16, 30}, {PROF_SHEPHERD, 4, "emerald", 3, 0, 0, "red_banner", 1, 12, 15},
    {PROF_SHEPHERD, 5, "emerald", 2, 0, 0, "painting", 3, 12, 30},
    // toolsmith
    {PROF_TOOLSMITH, 1, "coal", 15, 0, 0, "emerald", 1, 16, 2}, {PROF_TOOLSMITH, 1, "emerald", 1, 0, 0, "stone_axe", 1, 12, 1}, {PROF_TOOLSMITH, 1, "emerald", 1, 0, 0, "stone_pickaxe", 1, 12, 1}, {PROF_TOOLSMITH, 1, "emerald", 1, 0, 0, "stone_shovel", 1, 12, 1},
    {PROF_TOOLSMITH, 2, "iron_ingot", 4, 0, 0, "emerald", 1, 12, 10}, {PROF_TOOLSMITH, 2, "emerald", 36, 0, 0, "bell", 1, 12, 5},
    {PROF_TOOLSMITH, 3, "flint", 30, 0, 0, "emerald", 1, 12, 20}, {PROF_TOOLSMITH, 3, "emerald", 2, 0, 0, "*iron_axe", 1, 3, 10}, {PROF_TOOLSMITH, 3, "emerald", 3, 0, 0, "*iron_pickaxe", 1, 3, 10},
    {PROF_TOOLSMITH, 4, "diamond", 1, 0, 0, "emerald", 1, 12, 30}, {PROF_TOOLSMITH, 4, "emerald", 4, 0, 0, "diamond_hoe", 1, 3, 15},
    {PROF_TOOLSMITH, 5, "emerald", 13, 0, 0, "*diamond_pickaxe", 1, 3, 30}, {PROF_TOOLSMITH, 5, "emerald", 11, 0, 0, "*diamond_shovel", 1, 3, 30},
    // weaponsmith
    {PROF_WEAPONSMITH, 1, "coal", 15, 0, 0, "emerald", 1, 16, 2}, {PROF_WEAPONSMITH, 1, "emerald", 3, 0, 0, "iron_axe", 1, 12, 1}, {PROF_WEAPONSMITH, 1, "emerald", 2, 0, 0, "*iron_sword", 1, 12, 1},
    {PROF_WEAPONSMITH, 2, "iron_ingot", 4, 0, 0, "emerald", 1, 12, 10}, {PROF_WEAPONSMITH, 2, "emerald", 36, 0, 0, "bell", 1, 12, 5},
    {PROF_WEAPONSMITH, 3, "flint", 24, 0, 0, "emerald", 1, 12, 20},
    {PROF_WEAPONSMITH, 4, "diamond", 1, 0, 0, "emerald", 1, 12, 30}, {PROF_WEAPONSMITH, 4, "emerald", 17, 0, 0, "*diamond_axe", 1, 3, 15},
    {PROF_WEAPONSMITH, 5, "emerald", 13, 0, 0, "*diamond_sword", 1, 3, 30},
    // wandering trader (level 1 = common, 2 = rare)
    {PROF_WANDERER, 1, "emerald", 1, 0, 0, "fern", 1, 12, 1}, {PROF_WANDERER, 1, "emerald", 1, 0, 0, "sugar_cane", 1, 8, 1}, {PROF_WANDERER, 1, "emerald", 1, 0, 0, "pumpkin", 1, 4, 1},
    {PROF_WANDERER, 1, "emerald", 1, 0, 0, "cactus", 1, 8, 1}, {PROF_WANDERER, 1, "emerald", 1, 0, 0, "dandelion", 1, 12, 1}, {PROF_WANDERER, 1, "emerald", 1, 0, 0, "vine", 1, 4, 1},
    {PROF_WANDERER, 1, "emerald", 1, 0, 0, "lily_pad", 2, 5, 1}, {PROF_WANDERER, 1, "emerald", 1, 0, 0, "sand", 8, 8, 1}, {PROF_WANDERER, 1, "emerald", 3, 0, 0, "slime_ball", 1, 5, 1},
    {PROF_WANDERER, 1, "emerald", 1, 0, 0, "glowstone", 1, 5, 1}, {PROF_WANDERER, 1, "emerald", 1, 0, 0, "nautilus_shell", 1, 5, 1}, {PROF_WANDERER, 1, "emerald", 5, 0, 0, "oak_sapling", 1, 8, 1},
    {PROF_WANDERER, 2, "emerald", 3, 0, 0, "blue_ice", 1, 6, 1}, {PROF_WANDERER, 2, "emerald", 6, 0, 0, "packed_ice", 1, 6, 1}, {PROF_WANDERER, 2, "emerald", 5, 0, 0, "gunpowder", 1, 8, 1},
    // Delvari merchants of the Underground City trade in Delvari Coins
    {PROF_DELVARI, 1, "emerald", 1, 0, 0, "delvari_coin", 3, 16, 1}, {PROF_DELVARI, 1, "iron_ingot", 4, 0, 0, "delvari_coin", 1, 16, 1},
    {PROF_DELVARI, 1, "delvari_coin", 2, 0, 0, "umbral_lamp", 2, 12, 1}, {PROF_DELVARI, 1, "delvari_coin", 1, 0, 0, "glow_berries", 6, 12, 1},
    {PROF_DELVARI, 1, "delvari_coin", 3, 0, 0, "delvari_bricks", 16, 12, 1}, {PROF_DELVARI, 1, "delvari_coin", 6, 0, 0, "oxygen_canister", 1, 6, 1},
    {PROF_DELVARI, 2, "delvari_coin", 5, 0, 0, "ender_eye", 1, 8, 1}, {PROF_DELVARI, 2, "delvari_coin", 24, 0, 0, "*umbral_blade", 1, 2, 1},
    {PROF_DELVARI, 2, "delvari_coin", 10, 0, 0, "*enchanted_book", 1, 4, 1},
    // the Sift: wandering souls swap small kindnesses; Zephyrine trades in the Sift's materials
    {PROF_SOUL, 1, "radiant_crystal", 2, 0, 0, "soul_essence", 1, 12, 1}, {PROF_SOUL, 1, "gold_ingot", 3, 0, 0, "soul_essence", 1, 12, 1},
    {PROF_SOUL, 1, "soul_essence", 1, 0, 0, "golden_apple", 1, 4, 1}, {PROF_SOUL, 1, "soul_essence", 2, 0, 0, "experience_bottle", 4, 8, 1},
    {PROF_ZEPHYR, 1, "soul_essence", 6, 0, 0, "divine_ingot", 1, 8, 1}, {PROF_ZEPHYR, 1, "feather", 8, 0, 0, "soul_essence", 1, 12, 1},
    {PROF_ZEPHYR, 1, "soul_essence", 2, 0, 0, "cloud_block", 16, 12, 1}, {PROF_ZEPHYR, 1, "soul_essence", 12, 0, 0, "elytra", 1, 1, 1},
    {PROF_ZEPHYR, 1, "soul_essence", 4, 0, 0, "*divine_boots", 1, 2, 1},
    {PROF_WANDERER, 2, "emerald", 5, 0, 0, "tropical_fish_bucket", 1, 4, 1}, {PROF_WANDERER, 2, "emerald", 5, 0, 0, "pufferfish_bucket", 1, 4, 1}, {PROF_WANDERER, 2, "emerald", 2, 0, 0, "podzol", 3, 6, 1},
};

#define MAX_OFFERS 12
typedef struct { TradeOffer o[MAX_OFFERS]; int n; } OfferSet;
static OfferSet offers[MAX_ENTITIES];

static inline int slot_of(Entity *e) { return (int)(e - g_ents.e); }
// Hero of the Village lowers the first cost of every offer by (0.3 + 0.0625 * amplifier) of its price
// (at least one item), like Minecraft. Callers get a discounted copy; uses are tracked by index.
TradeOffer *villager_offers(Entity *e, int *n) {
    OfferSet *s = &offers[slot_of(e)];
    *n = s->n;
    Entity *pl = player_ent();
    if (!pl || !effect_has(pl, EFF_HERO_OF_THE_VILLAGE) || e->mob != MOB_VILLAGER) return s->o;
    static TradeOffer disc[MAX_OFFERS];
    memcpy(disc, s->o, sizeof(TradeOffer) * s->n);
    f32 f = 0.3f + 0.0625f * pl->eff_amp[EFF_HERO_OF_THE_VILLAGE];
    for (int i = 0; i < s->n; i++) { int c = disc[i].a.count; int cut = MAXI(1, (int)floorf(f * c)); disc[i].a.count = (u8)MAXI(1, c - cut); }
    return disc;
}

static ItemStack trade_item(const char *key, int n, int level) {
    if (!key) return (ItemStack){0};
    bool ench = key[0] == '*';
    if (ench) key++;
    ItemStack s = stack_of(item_by_key(key), n);
    if (!item_valid(s.id)) return (ItemStack){0};
    if (ench) {
        if (s.id == I_ENCHANTED_BOOK) {
            // random non-curse enchantment of a random level (treasure allowed, like librarians)
            for (int tries = 0; tries < 50; tries++) {
                int e = irand(1, ENCH_COUNT - 1);
                if (ENCHANTS[e].curse || e == ENCH_SOUL_SPEED || e == ENCH_SWIFT_SNEAK || e == ENCH_WIND_BURST) continue;
                ench_add(&s, e, irand(1, ENCHANTS[e].max_level));
                break;
            }
        } else {
            u8 out[MAX_ENCH][2]; memset(out, 0, sizeof out);
            int k = ench_select(s.id, 5 + level * 4 + irand(0, 10), (u32)irand(1, 1 << 30), out, MAX_ENCH);
            for (int i = 0; i < k; i++) ench_add(&s, out[i][0], out[i][1]);
        }
    }
    return s;
}
static void add_offers_for_level(Entity *e, int prof, int level, int count) {
    OfferSet *s = &offers[slot_of(e)];
    int cand[64], nc = 0;
    for (size_t i = 0; i < ARRAY_LEN(TRADES) && nc < 64; i++) if (TRADES[i].prof == prof && TRADES[i].level == level) cand[nc++] = (int)i;
    for (int k = 0; k < count && nc > 0 && s->n < MAX_OFFERS; k++) {
        int pick = irand(0, nc - 1);
        const TradeDef *t = &TRADES[cand[pick]];
        cand[pick] = cand[--nc];
        TradeOffer *o = &s->o[s->n++];
        memset(o, 0, sizeof *o);
        o->a = trade_item(t->a, t->na, level); o->b = trade_item(t->b, t->nb, level); o->result = trade_item(t->r, t->nr, level);
        if (t->r[0] == '*' && !strcmp(t->r + 1, "enchanted_book")) {   // books cost more for higher levels
            int lv = o->result.ench[0][1];
            o->a.count = (u8)MINI(64, 2 + lv * 3 + irand(0, 5 + lv * 10));
            if (ENCHANTS[o->result.ench[0][0]].treasure) o->a.count = (u8)MINI(64, o->a.count * 2);
        }
        if (o->result.id == I_FILLED_MAP) o->result.id = I_MAP;
        o->max_uses = t->uses; o->xp = t->xp;
    }
}

void villager_set_profession(Entity *e, int prof) {
    e->prof = (u8)prof;
    e->jobsite.y = -1000;
    e->vlevel = 1; e->vxp = 0;
    offers[slot_of(e)].n = 0;
    if (prof == PROF_NONE || prof == PROF_NITWIT) return;
    if (prof == PROF_WANDERER) { add_offers_for_level(e, prof, 1, 5); add_offers_for_level(e, prof, 2, 1); return; }
    if (prof == PROF_DELVARI) { add_offers_for_level(e, prof, 1, 4); add_offers_for_level(e, prof, 2, 2); return; }
    if (prof == PROF_SOUL) { add_offers_for_level(e, prof, 1, 3); return; }
    if (prof == PROF_ZEPHYR) { add_offers_for_level(e, prof, 1, 5); return; }
    add_offers_for_level(e, prof, 1, 2);
}
void villager_clear(Entity *e) { offers[slot_of(e)].n = 0; e->prof = PROF_NONE; e->jobsite.y = -1000; }

static const int LEVEL_XP[6] = {0, 0, 10, 70, 150, 250};
void progress_grant(const char *id);   // game/progress.c
void villager_trade_done(Entity *e, int idx) {
    progress_grant("trade");
    if (e->mob == MOB_VILLAGER) e->rep = (s8)MINI(30, e->rep + 1);
    OfferSet *s = &offers[slot_of(e)];
    if (idx < 0 || idx >= s->n) return;
    TradeOffer *o = &s->o[idx];
    o->uses++;
    e->vxp += o->xp;
    player_add_xp(3 + irand(0, 3));
    if (e->prof != PROF_WANDERER && e->prof != PROF_DELVARI && e->prof != PROF_SOUL && e->prof != PROF_ZEPHYR && e->vlevel < 5 && e->vxp >= LEVEL_XP[e->vlevel + 1]) {
        e->vlevel++;
        add_offers_for_level(e, e->prof, e->vlevel, 2);
        effect_add(e, EFF_REGENERATION, 0, 10 * TICKRATE);
        fx_hearts(v3_add(e->pos, v3(0, 2.2f, 0)));
        sound_play_at(SND_LEVELUP, e->pos, 0.6f, 1.6f);
    }
    e->persistent = true;
}

// job-site search and restocking (called from the mob AI a few times a second)
void villager_tick(Entity *e) {
    if (e->mob != MOB_VILLAGER || e->baby) return;
    if (e->age % 40 == 7 && e->prof == PROF_NONE) {
        ivec3 c = v3_floor(e->pos);
        for (int r = 1; r <= 12 && e->prof == PROF_NONE; r++)
            for (int dy = -4; dy <= 4; dy++) for (int dz = -r; dz <= r; dz++) for (int dx = -r; dx <= r; dx++) {
                if (abs(dx) != r && abs(dz) != r) continue;
                int p = profession_of_block(world_get_block(c.x + dx, c.y + dy, c.z + dz));
                if (p != PROF_NONE && e->prof == PROF_NONE) {
                    villager_set_profession(e, p);
                    e->jobsite = iv3(c.x + dx, c.y + dy, c.z + dz);
                    fx_crit(v3_add(e->pos, v3(0, 2.2f, 0)), 8);
                    goto done;
                }
            }
    done:;
    }
    // restock twice a day (morning and afternoon)
    u64 t = world_daytime() % DAY_TICKS;
    if (t == DAY_TICKS / 12 || t == DAY_TICKS / 3) { OfferSet *s = &offers[slot_of(e)]; for (int i = 0; i < s->n; i++) s->o[i].uses = 0; }
    // a villager whose job site was destroyed loses the job if it never traded
    if (e->prof != PROF_NONE && e->prof != PROF_NITWIT && e->prof != PROF_WANDERER && e->prof != PROF_DELVARI && e->prof != PROF_SOUL && e->prof != PROF_ZEPHYR && e->vxp == 0 && e->age % 200 == 11 && e->jobsite.y != -1000) {
        ivec3 a = e->jobsite;
        if (world_loaded(a.x, a.z) && profession_of_block(world_get_block(a.x, a.y, a.z)) != e->prof) villager_set_profession(e, PROF_NONE);
    }
}

// ------------------------------------------------------------------ persistence (save.c)
TradeOffer *villager_offer_slot(Entity *e, int idx) { return idx >= 0 && idx < MAX_OFFERS ? &offers[slot_of(e)].o[idx] : NULL; }
void villager_set_offer_count(Entity *e, int n) {
    OfferSet *s = &offers[slot_of(e)];
    s->n = 0;
    for (int i = 0; i < MINI(n, MAX_OFFERS); i++) if (item_valid(s->o[i].result.id)) s->o[s->n++] = s->o[i];
}

// Generated structures (structures.c)
#ifndef STRUCTURES_H
#define STRUCTURES_H
#include "world.h"
enum { ST_VILLAGE, ST_OUTPOST, ST_DESERT_PYRAMID, ST_JUNGLE_TEMPLE, ST_WITCH_HUT, ST_IGLOO, ST_MONUMENT, ST_SHIPWRECK, ST_OCEAN_RUIN,
       ST_BURIED_TREASURE, ST_RUINED_PORTAL, ST_MINESHAFT, ST_MANSION, ST_ANCIENT_CITY, ST_TRIAL_CHAMBERS, ST_STRONGHOLD,
       ST_FORTRESS, ST_BASTION, ST_RUINED_PORTAL_NETHER, ST_END_CITY, ST_UNDERGROUND_CITY,
       ST_TEMPLE_DAWN, ST_SPIRE_WINDS, ST_ARENA_STONE, ST_COUNT };
typedef struct { const char *name, *key; int dim, spacing, sep; u32 salt; int radius; } StructDef;
extern const StructDef STRUCTS[ST_COUNT];
enum { MK_LOOT, MK_SPAWNER, MK_MOB, MK_VILLAGER, MK_CART, MK_ARROWS };
enum { LOOT_VILLAGE, LOOT_WEAPONSMITH, LOOT_DESERT, LOOT_JUNGLE, LOOT_IGLOO, LOOT_SHIP_SUPPLY, LOOT_SHIP_TREASURE, LOOT_SHIP_MAP, LOOT_BURIED,
       LOOT_PORTAL, LOOT_MINESHAFT, LOOT_DUNGEON, LOOT_STRONGHOLD, LOOT_FORTRESS, LOOT_BASTION, LOOT_END_CITY, LOOT_END_SHIP, LOOT_MANSION,
       LOOT_ANCIENT, LOOT_OUTPOST, LOOT_OCEAN_RUIN, LOOT_TRIAL, LOOT_HUT, LOOT_DELVARI, LOOT_DELVARI_TEMPLE, LOOT_DELVARI_VAULT, LOOT_SIFT, LOOT_COUNT };
int structure_by_key(const char *key);
void structures_generate(Column *c, u64 seed, int dim);   // worker threads
void structures_column_ready(Column *c);                  // main thread: chests, spawners, mobs
void structures_reset(void);
int structure_at(int x, int y, int z);                    // ST_* containing the position, -1 none
bool structure_locate(int type, int x, int z, ivec3 *out);
int structure_spawn_mob(int st, bool water);
void loot_fill(ItemStack *slots, int nslots, int table);
void spawners_tick(void);
#endif

// Villager professions and trading (villager.c)
#ifndef VILLAGER_H
#define VILLAGER_H
#include "entity.h"
#include "../world/block.h"
enum { PROF_NONE, PROF_ARMORER, PROF_BUTCHER, PROF_CARTOGRAPHER, PROF_CLERIC, PROF_FARMER, PROF_FISHERMAN, PROF_FLETCHER, PROF_LEATHERWORKER,
       PROF_LIBRARIAN, PROF_MASON, PROF_SHEPHERD, PROF_TOOLSMITH, PROF_WEAPONSMITH, PROF_NITWIT, PROF_WANDERER, PROF_DELVARI, PROF_SOUL, PROF_ZEPHYR, PROF_COUNT };
extern const char *PROF_NAMES[PROF_COUNT];
typedef struct { ItemStack a, b, result; u8 uses, max_uses, xp; } TradeOffer;
const char *villager_level_name(int level);
int profession_of_block(Block b);
void villager_set_profession(Entity *e, int prof);   // also rolls the novice offers
void villager_clear(Entity *e);
TradeOffer *villager_offers(Entity *e, int *n);
void villager_trade_done(Entity *e, int idx);
void villager_tick(Entity *e);
TradeOffer *villager_offer_slot(Entity *e, int idx);  // for loading
void villager_set_offer_count(Entity *e, int n);
#endif

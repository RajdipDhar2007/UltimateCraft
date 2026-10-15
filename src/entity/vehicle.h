// Boats, minecarts and riding (vehicle.c)
#ifndef VEHICLE_H
#define VEHICLE_H
#include "entity.h"
enum { VEH_BOAT, VEH_CHEST_BOAT, VEH_MINECART, VEH_CHEST_MINECART, VEH_FURNACE_MINECART, VEH_TNT_MINECART, VEH_HOPPER_MINECART, VEH_COUNT };
int vehicle_type_of_item(int id);         // -1 if the item is not a vehicle
bool vehicle_is_cart(int vtype);
ivec3 vehicle_storage_pos(Entity *v);     // virtual block-entity position of a chest/hopper vehicle
void vehicle_tick(Entity *v);
void vehicle_break(Entity *v, bool drop);
void vehicle_hit(Entity *v, Entity *by);
bool vehicle_interact(Entity *v, ItemStack *held);
bool ride_mount(Entity *rider, Entity *mount);
void ride_dismount(Entity *rider);
vec3 ride_seat(Entity *mount);
void ride_update_rider(Entity *mount);
bool mob_rideable(int mob);
bool ride_mob_interact(Entity *m, ItemStack *held);
bool ride_mob_tick(Entity *m);            // true = player is steering (skip normal AI)
#endif

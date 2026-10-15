// Filled maps (maps.c)
#ifndef MAPS_H
#define MAPS_H
#include "../core/common.h"
void maps_reset(void);          // world opened
void maps_save_all(void);
void maps_tick(void);           // draws terrain into held maps
int map_create_at(vec3 pos);    // new map id centred on the grid cell containing pos (0 = failed)
int map_copy(int id, bool locked);
int map_zoom_out(int id);
int map_scale(int id);
bool map_locked(int id);
f32 map_explored(int id);
void map_draw(int id, f32 x, f32 y, f32 size, bool marker);   // GUI coordinates
#endif

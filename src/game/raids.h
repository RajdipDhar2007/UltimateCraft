// Raids (Bad Omen / Raid Omen -> waves of illagers at a village) and pillager patrols.
#ifndef RAIDS_H
#define RAIDS_H
#include "../core/common.h"
struct Entity;
void raids_tick(void);                 // every engine tick
void raid_command(const char *action); // /raid start|stop|status (debug helper, not a vanilla command)
void raid_on_mob_death(struct Entity *e, struct Entity *killer);
void raids_reset(void);
bool raid_active(void);
int raid_wave(void);
void raids_render_hud(void);           // boss bar
#endif

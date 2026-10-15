#ifndef SCREENS_H
#define SCREENS_H
#include "../game/game.h"
void hud_render(void);
void screens_render(void);
void screens_on_close(Screen s);
void recipes_unlock_tick(void);
bool screens_enchant_option(int i);
bool screens_recipe_fill(int recipe, bool all);
bool screens_trade_select(int idx);
void screens_take_result(bool shift);
void banner_draw_gui(f32 x, f32 y, f32 w, f32 h, int base, const u8 layers[][2], int n);
int banner_base_color(int item);
bool item_is_banner(int id);
GLuint banner_pattern_texture(void);
#endif

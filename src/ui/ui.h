// Immediate-mode, Minecraft-styled UI. All coordinates are "GUI pixels" (multiplied by the GUI scale).
#ifndef UI_H
#define UI_H
#include "../core/common.h"
#include "../gfx/gfx.h"
#include "../item/item.h"

extern int ui_scale, ui_w, ui_h;
extern f32 ui_mx, ui_my;

void ui_init(void);
void ui_begin(int gui_scale_setting);
void ui_end(void);

int ui_text_width(const char *s);
extern bool g_ui_colorblind, g_ui_contrast;
u32 ui_cb_color(u32 c);
void ui_text(f32 x, f32 y, const char *s, u32 color, bool shadow);
void ui_text_scaled(f32 x, f32 y, f32 scale, const char *s, u32 color, bool shadow);
void ui_text_center(f32 cx, f32 y, const char *s, u32 color, bool shadow);
void ui_rect(f32 x, f32 y, f32 w, f32 h, u32 color);
void ui_panel(f32 x, f32 y, f32 w, f32 h);          // raised grey inventory panel
void ui_slot(f32 x, f32 y);                          // 18x18 inset slot
void ui_icon(int cell, f32 x, f32 y, u32 color);     // 9x9 icon from gui.png
void ui_item(f32 x, f32 y, ItemStack s, bool show_count);
void ui_tooltip(const char *text, const char *sub);
void ui_tooltip_lines(const char *title, u32 title_col, const char lines[][64], int n, const char *sub);
void ui_dirt_background(f32 darken);

bool ui_hover(f32 x, f32 y, f32 w, f32 h);
bool ui_button(f32 x, f32 y, f32 w, f32 h, const char *label, bool enabled);
bool ui_slider(f32 x, f32 y, f32 w, const char *label, f32 *value, f32 lo, f32 hi, f32 step);
bool ui_textfield(f32 x, f32 y, f32 w, char *buf, int cap, bool focused);   // returns true when clicked
#endif

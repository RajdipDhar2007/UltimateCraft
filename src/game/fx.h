// Particles: block debris, smoke, explosions, crits, hearts, bubbles, splashes, flames.
#ifndef FX_H
#define FX_H
#include "../core/common.h"
#include "../gfx/gfx.h"
void fx_init(void);
void fx_clear(void);
void fx_tick(void);
void fx_render(const mat4 *vp, vec3 cam_right, vec3 cam_up, f32 alpha, vec4 fog, f32 fog_near, f32 fog_far);
void fx_block_break(ivec3 p, u16 block);
void fx_block_hit(ivec3 p, int face, u16 block);
void fx_smoke(vec3 p, int n, f32 spread, u32 color);
void fx_explosion(vec3 p, f32 radius);
void fx_crit(vec3 p, int n);
void fx_damage(vec3 p, int n);
void fx_hearts(vec3 p);
void fx_bubbles(vec3 p, int n);
void fx_splash(vec3 p, int n);
void fx_flame(vec3 p);
void fx_poof(vec3 p, f32 w, f32 h);
void fx_shake(f32 amount);
f32 fx_shake_amount(void);
#endif

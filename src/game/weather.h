// Weather: Minecraft's rain / thunder cycle, lightning, precipitation effects and rendering.
#ifndef WEATHER_H
#define WEATHER_H
#include "../core/common.h"
#include "../gfx/gfx.h"
void weather_tick(void);                       // every engine tick
void weather_set(int kind, int duration_mc);   // 0 clear, 1 rain, 2 thunder; duration in Minecraft ticks (<=0: random)
void weather_render(const mat4 *vp, vec3 cam, vec4 fog, f32 fog_near, f32 fog_far);
void lightning_strike(vec3 pos, bool visual_only);
bool weather_raining_at(int x, int y, int z);  // rain (not snow) falling on this block
int weather_precip_at(int x, int y, int z);    // 0 none, 1 rain, 2 snow at this exposed position
f32 weather_sky_dim(void);                     // 0..1 how much rain/thunder darkens the sky
void weather_sleep_reset(void);                // sleeping through the night clears the weather
#endif

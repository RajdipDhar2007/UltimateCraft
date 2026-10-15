// Sky dome, sun, moon, stars and clouds (colours driven by the world time).
#ifndef SKY_H
#define SKY_H
#include "../gfx/gfx.h"
void sky_init(void);
vec4 sky_fog_color(void);
void sky_render(const mat4 *view, const mat4 *proj, vec3 cam, f32 far_dist);
void sky_render_space(const mat4 *view, const mat4 *proj, vec3 cam);              // starfield in every direction, nebulae, a ringed planet
void sky_render_sift(const mat4 *view, const mat4 *proj, vec3 cam, f32 far_dist);  // bright heaven sky with a halo sun
#endif

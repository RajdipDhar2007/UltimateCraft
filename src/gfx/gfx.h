// Graphics layer: window + input, shaders, textures, batched 2D/3D drawing, frustum culling.
#ifndef GFX_H
#define GFX_H
#include "../core/common.h"
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// ------------------------------------------------------------------ window & input
typedef struct {
    GLFWwindow *handle;
    int width, height;          // framebuffer size
    bool key_down[GLFW_KEY_LAST + 1];
    bool key_pressed[GLFW_KEY_LAST + 1];   // went down this frame (includes key repeat for text editing keys)
    unsigned long frame;                   // frame counter (one-shot actions per frame)
    bool mouse_down[8], mouse_pressed[8], mouse_released[8];
    f64 mouse_x, mouse_y;       // window coords (pixels, top-left origin, framebuffer scaled)
    f64 mouse_dx, mouse_dy;     // accumulated motion since last frame
    f64 scroll;                 // accumulated wheel since last frame
    u32 chars[32]; int nchars;  // typed unicode codepoints this frame
    bool grabbed, resized, focused;
    bool vsync;
} Window;

extern Window g_win;
bool window_init(int w, int h, const char *title);
void window_shutdown(void);
void window_begin_frame(void);   // clears per-frame edges then polls events
bool window_should_close(void);
void window_request_close(void);
void window_swap(void);
void window_set_grab(bool grab);
void window_set_vsync(bool on);
static inline bool key_down(int k) { return g_win.key_down[k]; }
void window_set_fullscreen(bool on);
static inline bool key_pressed(int k) { return g_win.key_pressed[k]; }
static inline void key_consume(int k) { g_win.key_pressed[k] = false; }

// ------------------------------------------------------------------ textures
typedef struct { GLuint id; int w, h; } Texture;
Texture texture_load(const char *path);                               // nearest filtering, RGBA
Texture texture_from_pixels(const u8 *rgba, int w, int h, bool mipmaps);
u8 *image_load_rgba(const char *path, int *w, int *h);                // caller frees with free()
void texture_destroy(Texture *t);

// ------------------------------------------------------------------ shaders
typedef struct { GLuint id; } Shader;
Shader shader_load(const char *vs_path, const char *fs_path);
static inline GLint shader_loc(Shader s, const char *name) { return glGetUniformLocation(s.id, name); }
void shader_mat4(Shader s, const char *name, const mat4 *m);
void shader_vec4(Shader s, const char *name, vec4 v);
void shader_vec3(Shader s, const char *name, vec3 v);
void shader_float(Shader s, const char *name, f32 v);
void shader_int(Shader s, const char *name, int v);

// ------------------------------------------------------------------ shared quad index buffer
// One IBO with the 0,1,2,0,2,3 pattern for up to QUAD_IBO_MAX quads, reused by every quad mesh.
#define QUAD_IBO_MAX 65536
GLuint gfx_quad_ibo(void);

// ------------------------------------------------------------------ 2D batch (UI)
typedef struct { f32 x, y, u, v; u8 r, g, b, a; } Vert2D;
void b2d_init(void);
void b2d_begin(int screen_w, int screen_h);         // ortho, top-left origin
void b2d_set_texture(GLuint tex);
void b2d_quad(f32 x, f32 y, f32 w, f32 h, f32 u0, f32 v0, f32 u1, f32 v1, u32 rgba);
void b2d_quad4(const f32 xy[8], const f32 uv[8], u32 rgba); // arbitrary quad (for isometric item cubes)
void b2d_rect(f32 x, f32 y, f32 w, f32 h, u32 rgba);       // untextured
void b2d_flush(void);
void b2d_set_invert(bool on);    // inverted blending (crosshair)

// ------------------------------------------------------------------ 3D batch (entities, particles, items, overlays)
typedef struct { f32 x, y, z, u, v; u8 r, g, b, a; } Vert3D;
void b3d_init(void);
void b3d_begin(const mat4 *view_proj, vec4 fog_color, f32 fog_near, f32 fog_far);
void b3d_set_texture(GLuint tex);
void b3d_set_mode(GLenum mode);          // GL_TRIANGLES (quads) or GL_LINES
void b3d_vertex(vec3 p, f32 u, f32 v, u32 rgba);   // for GL_LINES
void b3d_quad(vec3 a, vec3 b, vec3 c, vec3 d, f32 u0, f32 v0, f32 u1, f32 v1, u32 rgba);
void b3d_quad_uv(const vec3 p[4], const f32 uv[8], u32 rgba);
void b3d_flush(void);
GLuint gfx_white_texture(void);

static inline u32 rgba_u32(int r, int g, int b, int a) { return (u32)(r & 255) | ((u32)(g & 255) << 8) | ((u32)(b & 255) << 16) | ((u32)(a & 255) << 24); }
static inline u32 rgba_f(f32 r, f32 g, f32 b, f32 a) { return rgba_u32((int)(clampf(r, 0, 1) * 255), (int)(clampf(g, 0, 1) * 255), (int)(clampf(b, 0, 1) * 255), (int)(clampf(a, 0, 1) * 255)); }
#define COL_WHITE 0xFFFFFFFFu
#define COL_BLACK 0xFF000000u

// ------------------------------------------------------------------ frustum
typedef struct { vec4 p[6]; } Frustum;
Frustum frustum_from(const mat4 *vp);
bool frustum_aabb(const Frustum *f, vec3 mn, vec3 mx);

// ------------------------------------------------------------------ screenshots
bool gfx_screenshot(const char *path);   // writes the current back buffer as PNG

#endif

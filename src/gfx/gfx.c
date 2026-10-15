#include "gfx.h"
#include "../core/platform.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb_image.h>

Window g_win;

// ======================================================================== window
static void cb_framebuffer(GLFWwindow *w, int width, int height) {
    (void)w; g_win.width = width; g_win.height = height; g_win.resized = true;
}
static void cb_key(GLFWwindow *w, int key, int sc, int action, int mods) {
    (void)w; (void)sc; (void)mods;
    if (key < 0 || key > GLFW_KEY_LAST) return;
    if (action == GLFW_PRESS) { g_win.key_down[key] = true; g_win.key_pressed[key] = true; }
    else if (action == GLFW_RELEASE) g_win.key_down[key] = false;
    else if (action == GLFW_REPEAT && (key == GLFW_KEY_BACKSPACE || key == GLFW_KEY_LEFT || key == GLFW_KEY_RIGHT || key == GLFW_KEY_DELETE))
        g_win.key_pressed[key] = true;
}
static void cb_char(GLFWwindow *w, unsigned int cp) {
    (void)w; if (g_win.nchars < (int)ARRAY_LEN(g_win.chars)) g_win.chars[g_win.nchars++] = cp;
}
static void cb_mouse(GLFWwindow *w, int b, int action, int mods) {
    (void)w; (void)mods;
    if (b < 0 || b >= 8) return;
    if (action == GLFW_PRESS) { g_win.mouse_down[b] = true; g_win.mouse_pressed[b] = true; }
    else if (action == GLFW_RELEASE) { g_win.mouse_down[b] = false; g_win.mouse_released[b] = true; }
}
static f64 last_cx, last_cy; static bool have_last;
static void cb_cursor(GLFWwindow *w, f64 x, f64 y) {
    int ww, wh; glfwGetWindowSize(w, &ww, &wh);
    f64 sx = ww > 0 ? (f64)g_win.width / ww : 1, sy = wh > 0 ? (f64)g_win.height / wh : 1;
    if (have_last) { g_win.mouse_dx += x - last_cx; g_win.mouse_dy += y - last_cy; } // accumulate, never overwrite
    last_cx = x; last_cy = y; have_last = true;
    g_win.mouse_x = x * sx; g_win.mouse_y = y * sy;
}
static void cb_scroll(GLFWwindow *w, f64 dx, f64 dy) { (void)w; (void)dx; g_win.scroll += dy; }
static void cb_focus(GLFWwindow *w, int f) { (void)w; g_win.focused = f != 0; }
static void cb_error(int code, const char *desc) { LOG_ERR("GLFW %d: %s", code, desc); }

bool window_init(int w, int h, const char *title) {
    glfwSetErrorCallback(cb_error);
    if (!glfwInit()) { LOG_ERR("glfwInit failed"); return false; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 0);
    g_win.handle = glfwCreateWindow(w, h, title, NULL, NULL);
    if (!g_win.handle) { LOG_ERR("could not create an OpenGL 3.3 window"); glfwTerminate(); return false; }
    glfwMakeContextCurrent(g_win.handle);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { LOG_ERR("gladLoadGL failed"); return false; }
    glfwGetFramebufferSize(g_win.handle, &g_win.width, &g_win.height);
    glfwSetFramebufferSizeCallback(g_win.handle, cb_framebuffer);
    glfwSetKeyCallback(g_win.handle, cb_key);
    glfwSetCharCallback(g_win.handle, cb_char);
    glfwSetMouseButtonCallback(g_win.handle, cb_mouse);
    glfwSetCursorPosCallback(g_win.handle, cb_cursor);
    glfwSetScrollCallback(g_win.handle, cb_scroll);
    glfwSetWindowFocusCallback(g_win.handle, cb_focus);
    g_win.focused = true;
    window_set_vsync(true);
    LOG_INFO("OpenGL %s | %s", (const char *)glGetString(GL_VERSION), (const char *)glGetString(GL_RENDERER));
    return true;
}
void window_shutdown(void) { glfwDestroyWindow(g_win.handle); glfwTerminate(); }
void window_begin_frame(void) {
    g_win.frame++;
    memset(g_win.key_pressed, 0, sizeof g_win.key_pressed);
    memset(g_win.mouse_pressed, 0, sizeof g_win.mouse_pressed);
    memset(g_win.mouse_released, 0, sizeof g_win.mouse_released);
    g_win.mouse_dx = g_win.mouse_dy = 0; g_win.scroll = 0; g_win.nchars = 0; g_win.resized = false;
    glfwPollEvents();
}
bool window_should_close(void) { return glfwWindowShouldClose(g_win.handle); }
void window_request_close(void) { glfwSetWindowShouldClose(g_win.handle, 1); }
void window_swap(void) { glfwSwapBuffers(g_win.handle); }
void window_set_fullscreen(bool on) {   // borderless-style fullscreen on the primary monitor, or back to a window
    static bool cur; static int wx = 100, wy = 100, ww = 1280, wh = 720;
    if (!g_win.handle || on == cur) return;
    GLFWmonitor *m = glfwGetPrimaryMonitor();
    if (on && m) {
        glfwGetWindowPos(g_win.handle, &wx, &wy); glfwGetWindowSize(g_win.handle, &ww, &wh);
        const GLFWvidmode *vm = glfwGetVideoMode(m);
        if (vm) glfwSetWindowMonitor(g_win.handle, m, 0, 0, vm->width, vm->height, vm->refreshRate);
    } else glfwSetWindowMonitor(g_win.handle, NULL, wx, wy, ww, wh, 0);
    cur = on;
}
void window_set_grab(bool grab) {
    if (g_win.grabbed == grab) return;
    g_win.grabbed = grab;
    glfwSetInputMode(g_win.handle, GLFW_CURSOR, grab ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (grab && glfwRawMouseMotionSupported()) glfwSetInputMode(g_win.handle, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    have_last = false;
    if (!grab) glfwSetCursorPos(g_win.handle, g_win.width / 2.0, g_win.height / 2.0);
}
void window_set_vsync(bool on) { g_win.vsync = on; glfwSwapInterval(on ? 1 : 0); }

// ======================================================================== textures
u8 *image_load_rgba(const char *path, int *w, int *h) {
    int n;
    stbi_set_flip_vertically_on_load(0);
    u8 *px = stbi_load(path, w, h, &n, 4);
    if (!px) LOG_ERR("cannot load image '%s' (run the game from the project folder)", path);
    return px;
}
Texture texture_from_pixels(const u8 *rgba, int w, int h, bool mipmaps) {
    Texture t = {0, w, h};
    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (mipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 4);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }
    return t;
}
Texture texture_load(const char *path) {
    int w, h;
    u8 *px = image_load_rgba(path, &w, &h);
    if (!px) {  // magenta/black checker fallback keeps the game running with a missing asset
        static u8 fb[16 * 16 * 4];
        for (int i = 0; i < 256; i++) { bool c = ((i % 16) / 4 + (i / 64)) & 1; fb[i*4] = c ? 255 : 0; fb[i*4+1] = 0; fb[i*4+2] = c ? 255 : 0; fb[i*4+3] = 255; }
        return texture_from_pixels(fb, 16, 16, false);
    }
    Texture t = texture_from_pixels(px, w, h, false);
    stbi_image_free(px);
    return t;
}
void texture_destroy(Texture *t) { if (t->id) glDeleteTextures(1, &t->id); t->id = 0; }

// ======================================================================== shaders
static GLuint compile(const char *path, GLenum type) {
    char *src = plat_read_file(path, NULL);
    if (!src) { LOG_ERR("missing shader '%s' (run the game from the project folder)", path); exit(1); }
    GLuint s = glCreateShader(type);
    const char *srcs[1] = {src};
    glShaderSource(s, 1, srcs, NULL);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[2048]; glGetShaderInfoLog(s, sizeof log, NULL, log); LOG_ERR("shader %s:\n%s", path, log); exit(1); }
    free(src);
    return s;
}
Shader shader_load(const char *vs, const char *fs) {
    Shader sh;
    GLuint v = compile(vs, GL_VERTEX_SHADER), f = compile(fs, GL_FRAGMENT_SHADER);
    sh.id = glCreateProgram();
    glAttachShader(sh.id, v); glAttachShader(sh.id, f);
    glLinkProgram(sh.id);
    GLint ok; glGetProgramiv(sh.id, GL_LINK_STATUS, &ok);
    if (!ok) { char log[2048]; glGetProgramInfoLog(sh.id, sizeof log, NULL, log); LOG_ERR("link %s:\n%s", vs, log); exit(1); }
    glDeleteShader(v); glDeleteShader(f);
    return sh;
}
void shader_mat4(Shader s, const char *n, const mat4 *m) { glUniformMatrix4fv(shader_loc(s, n), 1, GL_FALSE, m->m); }
void shader_vec4(Shader s, const char *n, vec4 v) { glUniform4f(shader_loc(s, n), v.x, v.y, v.z, v.w); }
void shader_vec3(Shader s, const char *n, vec3 v) { glUniform3f(shader_loc(s, n), v.x, v.y, v.z); }
void shader_float(Shader s, const char *n, f32 v) { glUniform1f(shader_loc(s, n), v); }
void shader_int(Shader s, const char *n, int v) { glUniform1i(shader_loc(s, n), v); }

// ======================================================================== shared quad IBO / white texture
static GLuint quad_ibo, white_tex;
GLuint gfx_quad_ibo(void) {
    if (!quad_ibo) {
        u32 *idx = xmalloc(sizeof(u32) * 6 * QUAD_IBO_MAX);
        for (u32 i = 0; i < QUAD_IBO_MAX; i++) {
            u32 b = i * 4;
            idx[i*6+0] = b; idx[i*6+1] = b + 1; idx[i*6+2] = b + 2;
            idx[i*6+3] = b; idx[i*6+4] = b + 2; idx[i*6+5] = b + 3;
        }
        glGenBuffers(1, &quad_ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad_ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * 6 * QUAD_IBO_MAX, idx, GL_STATIC_DRAW);
        free(idx);
    }
    return quad_ibo;
}
GLuint gfx_white_texture(void) {
    if (!white_tex) { u8 w[4] = {255, 255, 255, 255}; white_tex = texture_from_pixels(w, 1, 1, false).id; }
    return white_tex;
}

// ======================================================================== 2D batch
#define B2D_MAX_QUADS 8192
static struct { GLuint vao, vbo; Shader sh; Vert2D *v; int n; GLuint tex; mat4 proj; bool invert; } b2;
void b2d_init(void) {
    b2.sh = shader_load("res/shaders/ui.vs", "res/shaders/ui.fs");
    b2.v = xmalloc(sizeof(Vert2D) * 4 * B2D_MAX_QUADS);
    glGenVertexArrays(1, &b2.vao); glGenBuffers(1, &b2.vbo);
    glBindVertexArray(b2.vao);
    glBindBuffer(GL_ARRAY_BUFFER, b2.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vert2D) * 4 * B2D_MAX_QUADS, NULL, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vert2D), (void *)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vert2D), (void *)8);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vert2D), (void *)16);
    glEnableVertexAttribArray(0); glEnableVertexAttribArray(1); glEnableVertexAttribArray(2);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gfx_quad_ibo());
    glBindVertexArray(0);
}
void b2d_begin(int sw, int sh) {
    b2.proj = m4_ortho(0, (f32)sw, (f32)sh, 0, -1, 1);
    b2.n = 0; b2.tex = gfx_white_texture();
    glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
void b2d_flush(void) {
    if (!b2.n) return;
    glUseProgram(b2.sh.id);
    shader_mat4(b2.sh, "proj", &b2.proj);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, b2.tex);
    shader_int(b2.sh, "tex", 0);
    glBindVertexArray(b2.vao);
    glBindBuffer(GL_ARRAY_BUFFER, b2.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vert2D) * 4 * B2D_MAX_QUADS, NULL, GL_STREAM_DRAW); // orphan
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Vert2D) * 4 * b2.n, b2.v);
    if (b2.invert) glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
    glDrawElements(GL_TRIANGLES, b2.n * 6, GL_UNSIGNED_INT, 0);
    if (b2.invert) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(0);
    b2.n = 0;
}
void b2d_set_invert(bool on) { if (b2.invert != on) { b2d_flush(); b2.invert = on; } }
void b2d_set_texture(GLuint tex) { if (tex != b2.tex) { b2d_flush(); b2.tex = tex; } }
void b2d_quad4(const f32 xy[8], const f32 uv[8], u32 c) {
    if (b2.n >= B2D_MAX_QUADS) b2d_flush();
    Vert2D *v = &b2.v[b2.n * 4];
    for (int i = 0; i < 4; i++) {
        v[i].x = xy[i*2]; v[i].y = xy[i*2+1]; v[i].u = uv[i*2]; v[i].v = uv[i*2+1];
        v[i].r = c & 255; v[i].g = (c >> 8) & 255; v[i].b = (c >> 16) & 255; v[i].a = (c >> 24) & 255;
    }
    b2.n++;
}
void b2d_quad(f32 x, f32 y, f32 w, f32 h, f32 u0, f32 v0, f32 u1, f32 v1, u32 c) {
    f32 xy[8] = {x, y, x, y + h, x + w, y + h, x + w, y};
    f32 uv[8] = {u0, v0, u0, v1, u1, v1, u1, v0};
    b2d_quad4(xy, uv, c);
}
void b2d_rect(f32 x, f32 y, f32 w, f32 h, u32 c) { b2d_set_texture(gfx_white_texture()); b2d_quad(x, y, w, h, 0, 0, 1, 1, c); }

// ======================================================================== 3D batch
#define B3D_MAX_VERTS (4 * 16384)
static struct { GLuint vao, vbo; Shader sh; Vert3D *v; int n; GLuint tex; GLenum mode; } b3;
void b3d_init(void) {
    b3.sh = shader_load("res/shaders/entity.vs", "res/shaders/entity.fs");
    b3.v = xmalloc(sizeof(Vert3D) * B3D_MAX_VERTS);
    glGenVertexArrays(1, &b3.vao); glGenBuffers(1, &b3.vbo);
    glBindVertexArray(b3.vao);
    glBindBuffer(GL_ARRAY_BUFFER, b3.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vert3D) * B3D_MAX_VERTS, NULL, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vert3D), (void *)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vert3D), (void *)12);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vert3D), (void *)20);
    glEnableVertexAttribArray(0); glEnableVertexAttribArray(1); glEnableVertexAttribArray(2);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gfx_quad_ibo());
    glBindVertexArray(0);
    b3.mode = GL_TRIANGLES;
}
void b3d_begin(const mat4 *vp, vec4 fog, f32 fn, f32 ff) {
    glUseProgram(b3.sh.id);
    shader_mat4(b3.sh, "vp", vp);
    shader_vec4(b3.sh, "fog_color", fog);
    shader_float(b3.sh, "fog_near", fn);
    shader_float(b3.sh, "fog_far", ff);
    shader_int(b3.sh, "tex", 0);
    b3.n = 0; b3.tex = gfx_white_texture(); b3.mode = GL_TRIANGLES;
}
void b3d_flush(void) {
    if (!b3.n) return;
    glUseProgram(b3.sh.id);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, b3.tex);
    glBindVertexArray(b3.vao);
    glBindBuffer(GL_ARRAY_BUFFER, b3.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vert3D) * B3D_MAX_VERTS, NULL, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Vert3D) * b3.n, b3.v);
    if (b3.mode == GL_LINES) glDrawArrays(GL_LINES, 0, b3.n);
    else glDrawElements(GL_TRIANGLES, (b3.n / 4) * 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    b3.n = 0;
}
void b3d_set_texture(GLuint tex) { if (tex != b3.tex) { b3d_flush(); b3.tex = tex; } }
void b3d_set_mode(GLenum mode) { if (mode != b3.mode) { b3d_flush(); b3.mode = mode; } }
static inline void b3_put(vec3 p, f32 u, f32 v, u32 c) {
    Vert3D *o = &b3.v[b3.n++];
    o->x = p.x; o->y = p.y; o->z = p.z; o->u = u; o->v = v;
    o->r = c & 255; o->g = (c >> 8) & 255; o->b = (c >> 16) & 255; o->a = (c >> 24) & 255;
}
void b3d_vertex(vec3 p, f32 u, f32 v, u32 c) { if (b3.n + 2 > B3D_MAX_VERTS) b3d_flush(); b3_put(p, u, v, c); }
void b3d_quad_uv(const vec3 p[4], const f32 uv[8], u32 c) {
    if (b3.n + 4 > B3D_MAX_VERTS) b3d_flush();
    for (int i = 0; i < 4; i++) b3_put(p[i], uv[i*2], uv[i*2+1], c);
}
void b3d_quad(vec3 a, vec3 b, vec3 c, vec3 d, f32 u0, f32 v0, f32 u1, f32 v1, u32 col) {
    vec3 p[4] = {a, b, c, d};
    f32 uv[8] = {u0, v1, u1, v1, u1, v0, u0, v0}; // a=bottom-left, b=bottom-right, c=top-right, d=top-left
    b3d_quad_uv(p, uv, col);
}

// ======================================================================== frustum
Frustum frustum_from(const mat4 *vp) {
    const f32 *m = vp->m; Frustum f;
    f32 rows[4][4];
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) rows[r][c] = m[c * 4 + r];
    for (int i = 0; i < 3; i++) {
        for (int s = 0; s < 2; s++) {
            f32 sg = s ? -1.0f : 1.0f;
            vec4 p = {rows[3][0] + sg * rows[i][0], rows[3][1] + sg * rows[i][1], rows[3][2] + sg * rows[i][2], rows[3][3] + sg * rows[i][3]};
            f.p[i * 2 + s] = p;
        }
    }
    return f;
}
bool frustum_aabb(const Frustum *f, vec3 mn, vec3 mx) {
    for (int i = 0; i < 6; i++) {
        vec4 p = f->p[i];
        vec3 v = {p.x >= 0 ? mx.x : mn.x, p.y >= 0 ? mx.y : mn.y, p.z >= 0 ? mx.z : mn.z};
        if (p.x * v.x + p.y * v.y + p.z * v.z + p.w < 0) return false;
    }
    return true;
}

// ======================================================================== PNG screenshot (stored deflate, no zlib needed)
static u32 crc_table[256];
static u32 crc32_update(u32 c, const u8 *buf, size_t n) {
    if (!crc_table[1]) for (u32 i = 0; i < 256; i++) { u32 k = i; for (int j = 0; j < 8; j++) k = (k & 1) ? 0xEDB88320u ^ (k >> 1) : k >> 1; crc_table[i] = k; }
    for (size_t i = 0; i < n; i++) c = crc_table[(c ^ buf[i]) & 255] ^ (c >> 8);
    return c;
}
static void put32(u8 *p, u32 v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static void png_chunk(FILE *f, const char *type, const u8 *data, u32 len) {
    u8 b[8]; put32(b, len); memcpy(b + 4, type, 4); fwrite(b, 1, 8, f);
    if (len) fwrite(data, 1, len, f);
    u32 c = crc32_update(0xFFFFFFFFu, (const u8 *)type, 4); c = crc32_update(c, data, len) ^ 0xFFFFFFFFu;
    put32(b, c); fwrite(b, 1, 4, f);
}
bool gfx_screenshot(const char *path) {
    int w = g_win.width, h = g_win.height;
    u8 *px = xmalloc((size_t)w * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px);
    size_t raw_len = (size_t)(w * 3 + 1) * h;
    u8 *raw = xmalloc(raw_len);
    for (int y = 0; y < h; y++) { raw[y * (w * 3 + 1)] = 0; memcpy(raw + y * (w * 3 + 1) + 1, px + (size_t)(h - 1 - y) * w * 3, w * 3); }
    size_t nblocks = (raw_len + 65534) / 65535;
    size_t z_len = 2 + raw_len + nblocks * 5 + 4;
    u8 *z = xmalloc(z_len), *o = z;
    *o++ = 0x78; *o++ = 0x01;
    u32 a = 1, b = 0;
    for (size_t i = 0; i < raw_len; i++) { a = (a + raw[i]) % 65521; b = (b + a) % 65521; }
    for (size_t off = 0; off < raw_len; off += 65535) {
        u32 n = (u32)MINI(65535, raw_len - off);
        *o++ = (off + n >= raw_len) ? 1 : 0;
        *o++ = n & 255; *o++ = n >> 8; *o++ = ~n & 255; *o++ = (~n >> 8) & 255;
        memcpy(o, raw + off, n); o += n;
    }
    put32(o, (b << 16) | a); o += 4;
    FILE *f = fopen(path, "wb");
    bool ok = f != NULL;
    if (f) {
        static const u8 sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
        fwrite(sig, 1, 8, f);
        u8 ihdr[13]; put32(ihdr, w); put32(ihdr + 4, h); ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = ihdr[11] = ihdr[12] = 0;
        png_chunk(f, "IHDR", ihdr, 13);
        png_chunk(f, "IDAT", z, (u32)(o - z));
        png_chunk(f, "IEND", NULL, 0);
        fclose(f);
    }
    free(px); free(raw); free(z);
    return ok;
}

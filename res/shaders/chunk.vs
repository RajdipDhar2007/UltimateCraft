#version 330 core
layout(location = 0) in vec3 a_pos;     // section-local, 1/16 block units
layout(location = 1) in vec2 a_uv;      // atlas pixels * 16
layout(location = 2) in vec4 a_light;   // sky, r, g, b (0..1)
layout(location = 3) in vec2 a_shade;   // x: ao | face << 2, y: flags
layout(location = 4) in float a_tint;   // RGB565, 65535 = none
uniform mat4 vp;
uniform vec3 origin;
uniform vec3 cam;
uniform float daylight;
uniform vec3 ambient;     // dimension ambient light (Nether / End)
uniform float anim_frame;
uniform float time;
out vec2 v_uv;
out vec3 v_light;
out float v_dist;
out float v_alpha;
out vec3 v_tint;
flat out int v_tinted;

float curve(float l) { return l <= 0.001 ? 0.0 : pow(0.8, (1.0 - l) * 15.0); }

void main() {
    vec3 p = origin + a_pos / 16.0;
    int sh = int(a_shade.x + 0.5);
    int fl = int(a_shade.y + 0.5);
    int ao = sh & 3;
    int face = (sh >> 2) & 7;
    if ((fl & 4) != 0) { p.x += sin(time * 1.7 + p.x * 0.7 + p.z * 0.3) * 0.06; p.z += cos(time * 1.3 + p.z * 0.6 + p.x * 0.2) * 0.05; }
    if ((fl & 8) != 0) { p.x += sin(time * 1.1 + p.y * 0.9 + p.z * 0.5) * 0.02; p.z += cos(time * 0.9 + p.x * 0.6) * 0.02; }
    vec2 uv = a_uv / 16384.0;
    if ((fl & 3) != 0) uv.x += anim_frame / 64.0;
    int t = int(a_tint + 0.5);
    v_tinted = t == 65535 ? 0 : 1;
    v_tint = vec3(float((t >> 11) & 31) / 31.0, float((t >> 5) & 63) / 63.0, float(t & 31) / 31.0);
    v_uv = uv;
    float skyb = curve(a_light.x) * daylight;
    vec3 blk = vec3(curve(a_light.y), curve(a_light.z), curve(a_light.w));
    vec3 skyc = mix(vec3(0.75, 0.82, 1.35), vec3(1.0), clamp((daylight - 0.18) / 0.6, 0.0, 1.0));
    vec3 light = max(skyb * skyc, blk * vec3(1.05, 0.95, 0.85));
    light = max(light, vec3(0.03));
    light = max(light, ambient);
    float faceshade[6] = float[6](0.78, 0.78, 0.5, 1.0, 0.64, 0.64);
    float aof[4] = float[4](0.42, 0.62, 0.8, 1.0);
    light *= faceshade[face] * aof[ao];
    if ((fl & 2) != 0) light = vec3(1.0);
    v_light = light;
    v_alpha = ((fl & 1) != 0) ? 0.78 : 1.0;
    v_dist = length(p - cam);
    gl_Position = vp * vec4(p, 1.0);
}

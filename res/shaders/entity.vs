#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_col;
uniform mat4 vp;
out vec2 v_uv;
out vec4 v_col;
out float v_dist;
void main() {
    v_uv = a_uv;
    v_col = a_col;
    vec4 p = vp * vec4(a_pos, 1.0);
    v_dist = p.w;
    gl_Position = p;
}

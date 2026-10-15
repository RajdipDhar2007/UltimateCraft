#version 330 core
in vec2 v_uv;
in vec4 v_col;
uniform sampler2D tex;
out vec4 frag;
void main() { vec4 c = texture(tex, v_uv) * v_col; if (c.a < 0.01) discard; frag = c; }

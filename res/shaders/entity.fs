#version 330 core
in vec2 v_uv;
in vec4 v_col;
in float v_dist;
uniform sampler2D tex;
uniform vec4 fog_color;
uniform float fog_near;
uniform float fog_far;
out vec4 frag;
void main() {
    vec4 c = texture(tex, v_uv) * v_col;
    if (c.a < 0.08) discard;
    float f = smoothstep(fog_near, fog_far, v_dist);
    frag = vec4(mix(c.rgb, fog_color.rgb, f), c.a);
}

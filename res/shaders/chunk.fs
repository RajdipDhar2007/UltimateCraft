#version 330 core
in vec2 v_uv;
in vec3 v_light;
in float v_dist;
in float v_alpha;
in vec3 v_tint;
flat in int v_tinted;
uniform sampler2D tex;
uniform vec4 fog_color;
uniform float fog_near;
uniform float fog_far;
out vec4 frag;
void main() {
    vec4 c = texture(tex, v_uv);
    if (v_alpha > 0.99 && c.a < 0.5) discard;
    vec3 base = c.rgb;
    if (v_tinted == 1) {   // biome colour: tint only the grey parts of the texture (grass side keeps its dirt)
        float mx = max(base.r, max(base.g, base.b)), mn = min(base.r, min(base.g, base.b));
        float grey = 1.0 - clamp((mx - mn) * 6.0, 0.0, 1.0);
        base = mix(base, base * v_tint * 1.55, grey);
    }
    vec3 col = base * v_light;
    float a = v_alpha < 0.99 ? v_alpha : c.a;
    float f = smoothstep(fog_near, fog_far, v_dist);
    frag = vec4(mix(col, fog_color.rgb, f), a);
}

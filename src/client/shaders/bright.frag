#version 450
// [U-501] bloom, step 1: keep only what is bright (fire, lava, glowing skills), smoothly
layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_uv;
layout(location = 0) out vec4 o_color;
layout(set = 2, binding = 0) uniform sampler2D u_tex;
layout(set = 3, binding = 0, std140) uniform Params { float threshold, knee, pad0, pad1; } u;
void main() {
    vec3 c = texture(u_tex, v_uv).rgb;
    float l = max(c.r, max(c.g, c.b));
    o_color = vec4(c * smoothstep(u.threshold, u.threshold + u.knee, l), 1.0);
}

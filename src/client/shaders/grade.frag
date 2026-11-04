#version 450
// [U-501] the final look of the world: sharpen, contrast, saturation, day / night tint, vignette
layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_uv;
layout(location = 0) out vec4 o_color;
layout(set = 2, binding = 0) uniform sampler2D u_tex;
layout(set = 3, binding = 0, std140) uniform Params {
    vec4 tint;      // rgb multiplier (day warm, night cool)
    vec2 texel;     // 1 / size
    float sharpen, contrast;
    float saturation, vignette, pad0, pad1;
} u;
void main() {
    vec3 c = texture(u_tex, v_uv).rgb;
    if (u.sharpen > 0.0) {
        vec3 n = texture(u_tex, v_uv + vec2(u.texel.x, 0)).rgb + texture(u_tex, v_uv - vec2(u.texel.x, 0)).rgb +
                 texture(u_tex, v_uv + vec2(0, u.texel.y)).rgb + texture(u_tex, v_uv - vec2(0, u.texel.y)).rgb;
        c = clamp(c + (c - n * 0.25) * u.sharpen, 0.0, 1.0);
    }
    c *= u.tint.rgb;
    c = (c - 0.5) * u.contrast + 0.5;
    float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
    c = mix(vec3(l), c, u.saturation);
    vec2 d = v_uv - 0.5;
    c *= 1.0 - u.vignette * smoothstep(0.25, 0.75, dot(d, d) * 2.0);
    o_color = vec4(clamp(c, 0.0, 1.0), 1.0);
}

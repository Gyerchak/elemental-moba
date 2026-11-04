#version 450
// [U-501] bloom, step 2: a 9-tap gaussian along one axis (u.step = one texel × direction)
layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_uv;
layout(location = 0) out vec4 o_color;
layout(set = 2, binding = 0) uniform sampler2D u_tex;
layout(set = 3, binding = 0, std140) uniform Params { vec2 step_uv; float pad0, pad1; } u;
void main() {
    const float w[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec3 c = texture(u_tex, v_uv).rgb * w[0];
    for (int i = 1; i < 5; ++i)
        c += (texture(u_tex, v_uv + u.step_uv * float(i)).rgb + texture(u_tex, v_uv - u.step_uv * float(i)).rgb) * w[i];
    o_color = vec4(c, 1.0);
}

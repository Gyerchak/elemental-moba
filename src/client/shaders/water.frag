#version 450
// [U-502] animated water + lava over the ground. The texture is a mask made from the map cells: r = water, g = lava.
// v_uv = the map position; p = cells. Everything is procedural (moving noise), so it scales to any size.
layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_uv;
layout(location = 0) out vec4 o_color;
layout(set = 2, binding = 0) uniform sampler2D u_tex;
layout(set = 3, binding = 0, std140) uniform Params { float time, cells, pad0, pad1; } u;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) { return 0.55 * noise(p) + 0.3 * noise(p * 2.1 + 3.7) + 0.15 * noise(p * 4.3 + 9.1); }
void main() {
    vec2 m = texture(u_tex, v_uv).rg;
    if (m.r + m.g < 0.01) { o_color = vec4(0.0); return; }
    vec2 p = v_uv * u.cells;
    float t = u.time;
    // water: two noise fields moving against each other; where they meet, light lines (caustics)
    float c = 1.0 - abs(fbm(p * 0.45 + vec2(t * 0.16, t * 0.07)) - fbm(p * 0.6 - vec2(t * 0.12, -t * 0.09)));
    c = pow(c, 9.0);
    vec4 water = vec4(mix(vec3(0.20, 0.50, 0.85), vec3(0.72, 0.92, 1.0), c), m.r * (0.10 + 0.55 * c));
    // lava: slow glowing crust, hot cracks pulse
    float f = fbm(p * 0.3 + vec2(t * 0.05, -t * 0.03)) + 0.06 * sin(t * 1.6 + p.x * 0.4 + p.y * 0.3);
    float hot = smoothstep(0.42, 0.68, f);
    vec4 lava = vec4(mix(vec3(0.55, 0.07, 0.01), vec3(1.0, 0.82, 0.25), hot), m.g * (0.35 + 0.6 * hot));
    o_color = m.g > m.r ? lava : water;
}

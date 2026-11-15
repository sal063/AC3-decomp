#version 450

layout(set = 0, binding = 0) uniform usampler2D vram;

layout(push_constant) uniform Push {
    vec4 src;
    ivec4 origin;
} pc;

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_col;

vec3 pixel(ivec2 p) {
    p = clamp(p, ivec2(0), ivec2(pc.src.zw) - 1);
    int b0 = pc.origin.x * 2 + p.x * 3;
    int y = (pc.origin.y + p.y) & 511;
    vec3 c;
    for (int k = 0; k < 3; ++k) {
        int b = b0 + k;
        uint h = texelFetch(vram, ivec2((b >> 1) & 1023, y), 0).r;
        c[k] = float((b & 1) != 0 ? h >> 8 : h & 255u);
    }
    return c / 255.0;
}

void main() {
    vec2 t = v_uv - 0.5;
    ivec2 p = ivec2(floor(t));
    vec2 f = t - vec2(p);
    vec3 top = mix(pixel(p), pixel(p + ivec2(1, 0)), f.x);
    vec3 bottom = mix(pixel(p + ivec2(0, 1)), pixel(p + ivec2(1, 1)), f.x);
    o_col = vec4(mix(top, bottom, f.y), 1.0);
}

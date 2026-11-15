#version 450

layout(set = 0, binding = 0) uniform usampler2D vram;

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_col;
layout(location = 2) flat in uvec2 v_tex;

layout(location = 0) out vec4 o_col;

uint vram16(int x, int y) {
    return texelFetch(vram, ivec2(x & 1023, y & 511), 0).r;
}

void main() {
    uint tpage = v_tex.x, clut = v_tex.y;
    int page_x = int(tpage & 15u) * 64;
    int page_y = int((tpage >> 4) & 1u) * 256;
    uint depth = (tpage >> 7) & 3u;
    int clut_x = int(clut & 63u) * 16;
    int clut_y = int((clut >> 6) & 511u);
    uint u = uint(floor(v_uv.x)) & 255u;
    uint v = uint(floor(v_uv.y)) & 255u;

    uint t;
    if (depth == 0u) {
        uint w = vram16(page_x + int(u >> 2), page_y + int(v));
        t = vram16(clut_x + int((w >> ((u & 3u) * 4u)) & 15u), clut_y);
    } else if (depth == 1u) {
        uint w = vram16(page_x + int(u >> 1), page_y + int(v));
        t = vram16(clut_x + int((w >> ((u & 1u) * 8u)) & 255u), clut_y);
    } else {
        t = vram16(page_x + int(u), page_y + int(v));
    }
    if (t == 0u) discard;

    vec3 t5 = vec3(float(t & 31u), float((t >> 5) & 31u), float((t >> 10) & 31u));
    vec3 rgb = min(t5 * (v_col.rgb * 255.0) / (128.0 * 31.0), vec3(1.0));
    o_col = vec4(rgb, 1.0);
}

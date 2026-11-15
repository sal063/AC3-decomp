#version 450

layout(push_constant) uniform Push {
    vec4 rot0, rot1, rot2;
    vec4 cam;
    vec4 proj;
} pc;

layout(location = 0) in ivec4 a_pos;
layout(location = 1) in uvec2 a_uv;
layout(location = 2) in vec4 a_col;
layout(location = 3) in uvec2 a_tex;
layout(location = 4) in vec2 a_tile;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_col;
layout(location = 2) flat out uvec2 v_tex;

void main() {
    vec3 world = vec3(a_tile.x + float(a_pos.x), float(a_pos.y), a_tile.y + float(a_pos.z));
    vec3 d = world - pc.cam.xyz;
    vec3 v = vec3(dot(pc.rot0.xyz, d), dot(pc.rot1.xyz, d), dot(pc.rot2.xyz, d));
    float H = pc.cam.w;
    gl_Position = vec4(H * v.x / (120.0 * pc.proj.x), H * v.y / 120.0, pc.proj.y, v.z);
    v_uv = vec2(a_uv);
    v_col = a_col;
    v_tex = a_tex;
}

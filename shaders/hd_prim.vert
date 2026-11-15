#version 450

layout(push_constant) uniform Push {
    vec2 to_ndc;
    float inv_scale;
} pc;

layout(location = 0) in vec4 a_pos;
layout(location = 1) in vec2 a_uv;
layout(location = 2) in vec4 a_col;
layout(location = 3) in uvec4 a_info;
layout(location = 4) in vec3 a_view;
layout(location = 5) in vec3 a_normal;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_col;
layout(location = 2) flat out uvec4 v_info;
layout(location = 3) out vec3 v_view;
layout(location = 4) out vec3 v_normal;

void main() {
    vec2 ndc = a_pos.xy * pc.to_ndc - 1.0;
    float w = a_pos.z;
    gl_Position = vec4(ndc * w, a_pos.w * w, w);
    v_uv = a_uv;
    v_col = a_col;
    v_info = a_info;
    v_view = a_view;
    v_normal = a_normal;
}

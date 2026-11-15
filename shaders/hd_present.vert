#version 450

layout(push_constant) uniform Push {
    vec4 src;
} pc;

layout(location = 0) out vec2 v_uv;

void main() {
    vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    v_uv = pc.src.xy + p * pc.src.zw;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}

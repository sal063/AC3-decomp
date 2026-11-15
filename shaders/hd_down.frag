#version 450

layout(set = 0, binding = 0) uniform sampler2D target;

layout(push_constant) uniform Push {
    int scale;
} pc;

layout(location = 0) out uint o_pix;

void main() {
    vec4 c = texelFetch(target, ivec2(gl_FragCoord.xy) * pc.scale, 0);
    uvec3 q = uvec3(round(clamp(c.rgb, 0.0, 1.0) * 31.0));
    o_pix = q.r | (q.g << 5) | (q.b << 10) | (c.a >= 0.5 ? 0x8000u : 0u);
}

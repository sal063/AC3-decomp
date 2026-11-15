#version 450

layout(set = 0, binding = 0) uniform usampler2D vram;
layout(set = 1, binding = 0) uniform sampler2DArray hd;
const float kMaterial = 8.0;
layout(set = 2, binding = 0) uniform sampler2D strip;
layout(set = 3, binding = 0) uniform sampler2DArray oc_deriv;
layout(set = 3, binding = 1) uniform sampler2DArray oc_disp;
layout(set = 3, binding = 2, std140) uniform OceanParams {
    vec4 p[9];
} oc;
layout(set = 3, binding = 3) uniform sampler2D oc_shore;

layout(push_constant) uniform Push {
    vec2 to_ndc;
    float inv_scale;
    float spec;
    vec4 dir[3];
    vec4 colour[3];
    vec4 back;
} pc;

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_col;
layout(location = 2) flat in uvec4 v_info;
layout(location = 3) in vec3 v_view;
layout(location = 4) in vec3 v_normal;

layout(location = 0) out vec4 o_col;

const uint F_TEXTURED = 1u;
const uint F_RAW = 2u;
const uint F_SEMI = 4u;
const uint F_STP_ONLY = 8u;
const uint F_OPAQUE_ONLY = 16u;
const uint F_COPY = 32u;
const uint F_MASK_OR = 64u;
const uint F_CHECK = 128u;
const uint F_FLAT = 1024u;
const uint F_HD = 2048u;
const uint F_LIT = 4096u;
const uint F_SMOOTH = 8192u;
const uint F_STRIP = 16384u;
const uint F_WATER = 32768u;

const float PI = 3.14159265;

const float kSkyAmbient = 0.25;

vec3 lit(vec3 albedo, vec4 mat, vec2 gx, vec2 gy, vec3 px, vec3 py) {
    vec3 n0 = normalize(v_normal);
    vec3 n = n0;
    vec3 p2 = cross(py, n0), p1 = cross(n0, px);
    vec3 t = p2 * gx.x + p1 * gy.x;
    vec3 b = p2 * gx.y + p1 * gy.y;
    float m = max(dot(t, t), dot(b, b));
    if (m > 1e-30) {
        float s = (dot(n0, cross(px, py)) < 0.0 ? -1.0 : 1.0) * inversesqrt(m);
        vec3 tn = mat.rgb * 2.0 - 1.0;
        tn.y = -tn.y;
        vec3 bent = t * (s * tn.x) + b * (s * tn.y) + n0 * tn.z;
        if (dot(bent, bent) > 1e-12) n = normalize(bent);
    }
    vec3 up = vec3(pc.dir[0].w, pc.dir[1].w, pc.dir[2].w);
    vec3 horizon = vec3(pc.colour[0].w, pc.colour[1].w, pc.colour[2].w);
    vec3 ir = max(vec3(dot(pc.dir[0].xyz, n), dot(pc.dir[1].xyz, n), dot(pc.dir[2].xyz, n)), 0.0);
    vec3 ambient = pc.back.rgb * (1.0 + kSkyAmbient * dot(n, up));
    vec3 f = clamp(ambient + vec3(dot(pc.colour[0].xyz, ir), dot(pc.colour[1].xyz, ir), dot(pc.colour[2].xyz, ir)),
                   0.0, 255.0 / 128.0);
    vec3 c = albedo * f;
    float rough = clamp(mat.a, 0.05, 1.0);
    float a = rough * rough, a2 = a * a, k = 0.5 * a;
    vec3 v = normalize(-v_view);
    float nv = max(dot(n, v), 1e-3);
    vec3 s = vec3(0.0);
    for (int i = 0; i < 2; ++i) {
        vec3 l = pc.dir[i].xyz;
        float len = length(l);
        if (len < 1e-4) continue;
        l /= len;
        float nl = dot(n, l);
        if (nl <= 0.0) continue;
        vec3 h = normalize(l + v);
        float nh = max(dot(n, h), 0.0), vh = max(dot(v, h), 0.0);
        float q = nh * nh * (a2 - 1.0) + 1.0;
        float d = a2 / (PI * q * q);
        float vis = 0.25 / ((nl * (1.0 - k) + k) * (nv * (1.0 - k) + k));
        float fr = 0.04 + 0.96 * pow(1.0 - vh, 5.0);
        vec3 lc = vec3(pc.colour[0][i], pc.colour[1][i], pc.colour[2][i]) * len;
        s += (PI * d * vis * fr * nl) * lc;
    }
    vec3 r = reflect(-v, n);
    float w = 0.03 + 0.5 * rough;
    vec3 env = mix(horizon * 0.35, horizon, smoothstep(-w, w, dot(r, up)));
    float fe = (0.04 + (max(1.0 - rough, 0.04) - 0.04) * pow(1.0 - nv, 5.0)) * pc.back.w;
    c = c * (1.0 - 0.35 * fe) + env * fe;
    return min(c + s * pc.spec, vec3(1.0));
}

const vec2 OC_ROT[3] = vec2[3](vec2(1.0, 0.0), vec2(0.990268, 0.139173), vec2(0.974370, -0.224951));
vec2 oc_to(vec2 v, int c) { vec2 r = OC_ROT[c]; return vec2(r.x * v.x - r.y * v.y, r.y * v.x + r.x * v.y); }
vec2 oc_from(vec2 v, int c) { vec2 r = OC_ROT[c]; return vec2(r.x * v.x + r.y * v.y, -r.y * v.x + r.x * v.y); }

vec3 unpack_rgb(float f) {
    float b = floor(f / 65536.0);
    float g = floor((f - b * 65536.0) / 256.0);
    return vec3(f - b * 65536.0 - g * 256.0, g, b) / 255.0;
}

float oc_lost(int c, float lod) {
    lod = clamp(lod, 0.0, 8.0);
    int i0 = int(floor(lod));
    int i1 = min(i0 + 1, 8);
    int a = c * 9 + i0, b = c * 9 + i1;
    return mix(oc.p[2 + a / 4][a % 4], oc.p[2 + b / 4][b % 4], lod - float(i0));
}

float hash21(vec2 p) {
    vec3 q = fract(vec3(p.xyx) * 0.1031);
    q += dot(q, q.yzx + 33.33);
    return fract((q.x + q.y) * q.z);
}
float vnoise(vec2 p) {
    vec2 i = floor(p), f = p - i;
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21(i), hash21(i + vec2(1.0, 0.0)), f.x),
               mix(hash21(i + vec2(0.0, 1.0)), hash21(i + vec2(1.0, 1.0)), f.x), f.y);
}

float pnoise(vec2 p, float period) {
    vec2 i = floor(p), f = p - i;
    f = f * f * (3.0 - 2.0 * f);
    vec2 a = mod(i, period), b = mod(i + 1.0, period);
    return mix(mix(hash21(a), hash21(vec2(b.x, a.y)), f.x), mix(hash21(vec2(a.x, b.y)), hash21(b), f.x), f.y);
}

vec3 sky_at(float t, vec3 c[4], vec3 h) {
    vec3 s = mix(c[3], c[2], clamp((t - h.z * 0.267) / max(h.z * 0.733, 1e-4), 0.0, 1.0));
    s = mix(s, c[1], clamp((t - h.z) / max(h.y - h.z, 1e-4), 0.0, 1.0));
    s = mix(s, c[0], clamp((t - h.y) / max(h.x - h.y, 1e-4), 0.0, 1.0));
    return s;
}

vec3 water(vec3 base, vec3 px, vec3 py) {
    vec3 X = pc.dir[0].xyz, Y = pc.dir[1].xyz, Z = pc.dir[2].xyz;
    float dist = max(length(v_view), 1.0);
    vec3 v = -v_view / dist;
    vec2 rel = vec2(dot(v_view, X), dot(v_view, Z));
    vec2 rx = vec2(dot(px, X), dot(px, Z)), ry = vec2(dot(py, X), dot(py, Z));
    float major = sqrt(max(dot(rx, rx), dot(ry, ry)));
    float minor = abs(rx.x * ry.y - rx.y * ry.x) / max(major, 1e-6);
    float narrow = max(minor, major / 16.0);
    float upm = oc.p[1].y;
    vec2 eye_uv[3] = vec2[3](pc.colour[2].xy, pc.colour[2].zw, pc.back.xy);
    vec2 world = vec2(pc.dir[0].w, pc.dir[1].w) + rel;
    float shore = textureLod(oc_shore, (world / 2048.0 + 128.0) / 256.0, 0.0).r * 4080.0;
    float open = smoothstep(0.0, 1.0, shore / 3000.0);
    vec3 calm = vec3(open * open, mix(0.25, 1.0, open), mix(0.5, 1.0, open));

    float vy = max(dot(v, Y), 0.0);
    vec2 lean = vec2(dot(v, X), dot(v, Z)) / max(vy, 0.3);
    vec2 at = rel;
    float height = 0.0;
    for (int it = 0; it < 2; ++it) {
        vec3 d = vec3(0.0);
        for (int c = 0; c < 3; ++c) {
            float len = oc.p[0][c];
            vec3 t = textureGrad(oc_disp, vec3(eye_uv[c] + oc_to(at, c) / len, float(c)), oc_to(rx, c) / len,
                                 oc_to(ry, c) / len).xyz;
            d += vec3(oc_from(t.xz, c), t.y) * calm[c];
        }
        height = d.z;
        at = rel - d.xy * upm + lean * (d.z * upm);
    }

    vec2 slope = vec2(0.0);
    float foam = 0.0, lost = oc.p[1].x * mix(0.35, 1.0, open);
    for (int c = 0; c < 3; ++c) {
        float len = oc.p[0][c];
        vec4 d = textureGrad(oc_deriv, vec3(eye_uv[c] + oc_to(at, c) / len, float(c)), oc_to(rx, c) / len,
                             oc_to(ry, c) / len);
        slope += oc_from(d.xy, c) * calm[c];
        foam += d.w * (c == 0 ? 0.6 : c == 1 ? 1.0 : 0.7) * calm[c];
        lost += 0.5 * calm[c] * calm[c] *
                (oc_lost(c, log2(max(narrow * 256.0 / len, 1e-3))) + oc_lost(c, log2(max(major * 256.0 / len, 1e-3))));
    }
    float time = pc.dir[2].w;
    float gust = 0.78 + 0.44 * (0.65 * vnoise(world / 5200.0 + time * vec2(0.011, 0.007)) +
                              0.35 * vnoise(world / 1700.0 - time * vec2(0.017, 0.023)));
    slope *= gust;
    lost *= gust * gust;

    vec3 n = normalize(Y - slope.x * X - slope.y * Z);
    float nv = dot(n, v);
    if (nv < 0.03) {
        n = normalize(n + v * (0.03 - nv));
        nv = max(dot(n, v), 0.02);
    }
    float sigma = sqrt(lost);

    vec3 sky[4];
    for (int i = 0; i < 4; ++i) {
        sky[i] = unpack_rgb(pc.colour[1][i]);
        sky[i] *= sky[i];
    }
    vec3 heights = vec3(pc.back.z, pc.back.w, pc.spec);
    vec3 r = reflect(-v, n);
    float ry_ = dot(r, Y);
    float t = max(ry_, 0.0) / max(sqrt(max(1.0 - ry_ * ry_, 0.0)), 1e-3) + 0.9 * sigma;
    vec3 refl = 0.5 * (sky_at(t * 0.8, sky, heights) + sky_at(t * 1.35, sky, heights));
    float F = 0.02 + 0.98 * pow(1.0 - nv, 5.0 * exp(-2.69 * sigma)) / (1.0 + 22.7 * pow(sigma, 1.5));

    vec3 body = base * base;
    vec3 over = sky[0] + 0.5 * sky[1];
    body *= mix(vec3(1.0), over / max(dot(over, vec3(0.3, 0.5, 0.2)), 1e-3), 0.3);
    vec3 L = pc.colour[0].xyz;
    float sun_up = dot(L, Y);
    vec3 sun = unpack_rgb(pc.colour[0].w) * 2.0;
    sun *= sun;
    float lit = smoothstep(0.0, 0.1, sun_up);
    vec2 toward = normalize(vec2(dot(L, X), dot(L, Z)) + 1e-5), looking = -normalize(vec2(dot(v, X), dot(v, Z)) + 1e-5);
    float through = pow(clamp(dot(toward, looking), 0.0, 1.0), 3.0) * clamp(height / 1.2, 0.0, 1.0) * lit;
    body += body * vec3(0.5, 1.3, 1.0) * through * (1.0 - smoothstep(3000.0, 15000.0, dist));

    vec3 c = mix(body, refl, F);

    if (sun_up > 0.0) {
        float alpha2 = clamp(2.0 * lost, 0.0004, 0.5);
        vec3 h = normalize(L + v);
        float nl = max(dot(n, L), 0.0), nh = max(dot(n, h), 0.0);
        float q = nh * nh * (alpha2 - 1.0) + 1.0;
        float D = alpha2 / (PI * q * q);
        float k = 0.5 * sqrt(alpha2);
        float G = nl / (nl * (1.0 - k) + k) * nv / (nv * (1.0 - k) + k);
        float Fh = 0.02 + 0.98 * pow(1.0 - max(dot(h, v), 0.0), 5.0);
        float spec = D * G * Fh / (4.0 * nv) * lit;
        float gl = 3.0 * spec;
        c += sun * (gl / (1.0 + gl / 3.0));
    }

    vec2 gp = (eye_uv[2] + oc_to(at, 2) / oc.p[0][2]) * 40.0;
    float grain = 0.6 * pnoise(gp, 40.0) + 0.4 * pnoise(gp * 3.0, 120.0);
    grain = mix(grain, 0.5, smoothstep(0.4, 2.0, major * 40.0 / oc.p[0][2]));
    float white = 0.9 * open * smoothstep(0.38, 0.8, foam * gust * gust + 0.5 * (grain - 0.5));
    vec3 foam_colour = vec3(0.8) * (sun * (0.25 + 0.6 * max(sun_up, 0.0)) * lit + 0.6 * sky[0] + 0.2 * sky[2]);
    c = mix(c, min(foam_colour, vec3(1.0)), white);

    c = sqrt(clamp(c, 0.0, 1.0));
    c += (hash21(gl_FragCoord.xy) - 0.5) / 255.0;
    return clamp(c, 0.0, 1.0);
}

uint vram16(int x, int y) {
    return texelFetch(vram, ivec2(x & 1023, y & 511), 0).r;
}

vec3 rgb15(uint t) {
    return vec3(float(t & 31u), float((t >> 5) & 31u), float((t >> 10) & 31u)) / 31.0;
}

uint texel(ivec2 i, uint tpage, uint clut, uint tw, uint lim) {
    uint mu = tw & 0xFFu, mv = (tw >> 8) & 0xFFu, ou = (tw >> 16) & 0xFFu, ov = tw >> 24;
    uint u = uint(clamp(i.x, int(lim & 255u), int((lim >> 16) & 255u))) & 255u;
    uint v = uint(clamp(i.y, int((lim >> 8) & 255u), int(lim >> 24))) & 255u;
    u = (u & (~mu & 255u)) | (ou & mu);
    v = (v & (~mv & 255u)) | (ov & mv);
    int page_x = int(tpage & 15u) * 64;
    int page_y = int((tpage >> 4) & 1u) * 256;
    uint depth = (tpage >> 7) & 3u;
    int clut_x = int(clut & 63u) * 16;
    int clut_y = int((clut >> 6) & 511u);
    if (depth == 0u) {
        uint w = vram16(page_x + int(u >> 2), page_y + int(v));
        return vram16(clut_x + int((w >> ((u & 3u) * 4u)) & 15u), clut_y);
    }
    if (depth == 1u) {
        uint w = vram16(page_x + int(u >> 1), page_y + int(v));
        return vram16(clut_x + int((w >> ((u & 1u) * 8u)) & 255u), clut_y);
    }
    return vram16(page_x + int(u), page_y + int(v));
}

uint texel_index(ivec2 i, uint tpage, uint tw, uint lim) {
    uint mu = tw & 0xFFu, mv = (tw >> 8) & 0xFFu, ou = (tw >> 16) & 0xFFu, ov = tw >> 24;
    uint u = uint(clamp(i.x, int(lim & 255u), int((lim >> 16) & 255u))) & 255u;
    uint v = uint(clamp(i.y, int((lim >> 8) & 255u), int(lim >> 24))) & 255u;
    u = (u & (~mu & 255u)) | (ou & mu);
    v = (v & (~mv & 255u)) | (ov & mv);
    int page_x = int(tpage & 15u) * 64;
    int page_y = int((tpage >> 4) & 1u) * 256;
    if (((tpage >> 7) & 3u) == 0u) return (vram16(page_x + int(u >> 2), page_y + int(v)) >> ((u & 3u) * 4u)) & 15u;
    return (vram16(page_x + int(u >> 1), page_y + int(v)) >> ((u & 1u) * 8u)) & 255u;
}

void main() {
    uint flags = v_info.z;
    vec2 gx = dFdx(v_uv), gy = dFdy(v_uv);
    vec3 px = dFdx(v_view), py = dFdy(v_view);
    if ((flags & F_COPY) != 0u) {
        uint t = vram16(int(floor(v_uv.x)), int(floor(v_uv.y)));
        o_col = vec4(rgb15(t), float(t >> 15));
        return;
    }
    if ((flags & F_CHECK) != 0u) {
        ivec2 p = ivec2(gl_FragCoord.xy * pc.inv_scale);
        if ((vram16(p.x, p.y) & 0x8000u) != 0u) discard;
    }
    uint mode = (flags >> 8) & 3u;
    vec3 c;
    uint stp = 0u;
    if ((flags & F_TEXTURED) != 0u) {
        uint tpage = v_info.x & 0xFFFFu, clut = v_info.x >> 16;
        uint lim = v_info.w;
        uint t;
        vec3 tex;
        if ((flags & F_SMOOTH) != 0u) {
            vec2 p = v_uv - 0.5;
            vec2 b = floor(p), f = p - b;
            ivec2 i = ivec2(b);
            uint t00 = texel(i, tpage, clut, v_info.y, lim), t10 = texel(i + ivec2(1, 0), tpage, clut, v_info.y, lim);
            uint t01 = texel(i + ivec2(0, 1), tpage, clut, v_info.y, lim), t11 = texel(i + ivec2(1, 1), tpage, clut, v_info.y, lim);
            vec4 w = vec4((1.0 - f.x) * (1.0 - f.y), f.x * (1.0 - f.y), (1.0 - f.x) * f.y, f.x * f.y);
            vec4 o = vec4(t00 != 0u, t10 != 0u, t01 != 0u, t11 != 0u) * w;
            float sum = o.x + o.y + o.z + o.w;
            if (sum < 0.5) discard;
            tex = (rgb15(t00) * o.x + rgb15(t10) * o.y + rgb15(t01) * o.z + rgb15(t11) * o.w) / sum;
            float blended = dot(o, vec4(t00 >> 15, t10 >> 15, t01 >> 15, t11 >> 15)) / sum;
            ivec2 q = ivec2(gl_FragCoord.xy) & 3;
            const float kBayer[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0,
                                               3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);
            t = (blended > (kBayer[q.y * 4 + q.x] + 0.5) / 16.0 ? 0x8000u : 0u) | 1u;
        } else {
            t = texel(ivec2(floor(v_uv)), tpage, clut, v_info.y, lim);
            if (t == 0u) discard;
            tex = rgb15(t);
            if ((flags & F_STRIP) != 0u) {
                float s = floor(1.0 / pc.inv_scale + 0.5);
                float u0 = float(lim & 255u);
                vec2 p = v_uv - 0.5;
                vec2 b = floor(p), f = p - b;
                ivec2 i = ivec2(b);
                vec4 d = vec4(float(texel_index(i, tpage, v_info.y, lim)) - (b.x - u0),
                              float(texel_index(i + ivec2(1, 0), tpage, v_info.y, lim)) - (b.x + 1.0 - u0),
                              float(texel_index(i + ivec2(0, 1), tpage, v_info.y, lim)) - (b.x - u0),
                              float(texel_index(i + ivec2(1, 1), tpage, v_info.y, lim)) - (b.x + 1.0 - u0));
                float shift = mix(mix(d.x, d.y, f.x), mix(d.z, d.w, f.x), f.y);
                float v0 = float((lim >> 8) & 255u), v1 = float(lim >> 24) + 1.0;
                float u1 = float((lim >> 16) & 255u) + 1.0;
                float edge = min(min(v_uv.x - u0, u1 - v_uv.x), min(v_uv.y - v0, v1 - v_uv.y));
                shift *= smoothstep(0.0, 1.5, edge);
                ivec2 q = ivec2(gl_FragCoord.xy) & 3;
                const float kOrder[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0,
                                                   3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);
                if (smoothstep(0.0, 2.0, edge) <= (kOrder[q.y * 4 + q.x] + 0.5) / 16.0) discard;
                float x = clamp(v_uv.x - u0 + shift, 0.0, 15.999);
                float y = clamp((v_uv.y - v0) / max(v1 - v0, 1.0), 0.0, 0.999);
                tex = texelFetch(strip, ivec2(int(x * s), int(y * s)), 0).rgb;
            }
        }
        stp = t >> 15;
        if ((flags & F_STP_ONLY) != 0u && stp == 0u) discard;
        if ((flags & F_OPAQUE_ONLY) != 0u && stp != 0u) discard;
        bool done = false;
        if ((flags & F_HD) != 0u) {
            vec2 lo = vec2(float(lim & 255u), float((lim >> 8) & 255u));
            vec2 hi = vec2(float((lim >> 16) & 255u), float(lim >> 24)) + 1.0 - 1.0 / 64.0;
            vec2 uv = clamp(v_uv, lo, hi) / 256.0;
            float layer = float((flags >> 16) & 255u);
            tex = textureGrad(hd, vec3(uv, layer), gx / 256.0, gy / 256.0).rgb;
            if ((flags & F_LIT) != 0u) {
                vec4 mat = textureGrad(hd, vec3(uv, layer + kMaterial), gx / 256.0, gy / 256.0);
                c = lit(tex, mat, gx, gy, px, py);
                done = true;
            }
        }
        if (!done) c = (flags & F_RAW) != 0u ? tex : min(tex * v_col.rgb * (255.0 / 128.0), vec3(1.0));
    } else if ((flags & F_FLAT) != 0u) {
        c = floor(v_col.rgb * 255.0 / 8.0 + 0.001) / 31.0;
    } else {
        c = v_col.rgb;
    }
    if ((flags & F_WATER) != 0u) {
        if ((flags & F_TEXTURED) != 0u && v_normal.x >= 0.0) {
            vec3 whole = (flags & F_RAW) != 0u ? v_normal : min(v_normal * v_col.rgb * (255.0 / 128.0), vec3(1.0));
            c = mix(c, whole, 0.75);
        }
        c = water(c, px, py);
    }
    float a = float(stp | ((flags & F_MASK_OR) != 0u ? 1u : 0u));
    bool blend = (flags & F_SEMI) != 0u && ((flags & F_TEXTURED) == 0u || stp != 0u);
    if (blend && mode == 3u) c *= 0.25;
    o_col = vec4(c, a);
}

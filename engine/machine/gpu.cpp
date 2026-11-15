#include "machine/gpu.hpp"

#include "machine/state_reader.hpp"
#include "sim/memory.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace ac3::machine {
namespace {
s32 sx11(s32 v) { return static_cast<s32>(static_cast<u32>(v) << 21) >> 21; }

constexpr s8 kDither[4][4] = {
    {-4, 0, -3, 1},
    {2, -2, 3, -1},
    {-3, 1, -4, 0},
    {3, -1, 2, -2},
};

constexpr int kFbs = 12, kPad = 12;

s16 pos_x(u32 v) { return static_cast<s16>(v & 0x3FFu); }
s16 pos_y(u32 v) { return static_cast<s16>(v & 0x1FFu); }
s16 copy_w(u32 v) { return static_cast<s16>((((v & 0x3FFu) - 1u) & 0x3FFu) + 1u); }
s16 copy_h(u32 v) { return static_cast<s16>((((v & 0x1FFu) - 1u) & 0x1FFu) + 1u); }

bool polyline_end(u32 w) { return (w & 0xF000F000u) == 0x50005000u; }

void unpack_rgb(u32 w, u8& r, u8& g, u8& b) {
    r = static_cast<u8>(w);
    g = static_cast<u8>(w >> 8);
    b = static_cast<u8>(w >> 16);
}

u16 blend15(u16 b, u16 f, int mode) {
    const s32 br = b & 31, bg = (b >> 5) & 31, bb = (b >> 10) & 31;
    const s32 fr = f & 31, fg = (f >> 5) & 31, fb = (f >> 10) & 31;
    s32 r, g, bl;
    switch (mode) {
    case 0:
        r = (br + fr) >> 1;
        g = (bg + fg) >> 1;
        bl = (bb + fb) >> 1;
        break;
    case 1:
        r = br + fr;
        g = bg + fg;
        bl = bb + fb;
        break;
    case 2:
        r = br - fr;
        g = bg - fg;
        bl = bb - fb;
        break;
    default:
        r = br + (fr >> 2);
        g = bg + (fg >> 2);
        bl = bb + (fb >> 2);
        break;
    }
    r = r < 0 ? 0 : (r > 31 ? 31 : r);
    g = g < 0 ? 0 : (g > 31 ? 31 : g);
    bl = bl < 0 ? 0 : (bl > 31 ? 31 : bl);
    return static_cast<u16>(r | (g << 5) | (bl << 10));
}

u32 grad_term(s64 num, s32 denom) {
    return static_cast<u32>(static_cast<s32>(num * (1 << kFbs) / denom)) << kPad;
}

s64 xfp(s32 x) {
    return static_cast<s64>((static_cast<u64>(static_cast<u32>(x)) << 32) + ((1ull << 32) - (1u << 11)));
}

s64 xfp_step(s32 dx, s32 dy) {
    s64 d = static_cast<s64>(dx) * 4294967296ll;
    if (d < 0) d -= dy - 1;
    if (d > 0) d += dy - 1;
    return d / dy;
}

s32 xfp_int(u64 v) { return static_cast<s32>(static_cast<s64>(v) >> 32); }

s64 line_div(s32 delta, s32 k) {
    s64 d = static_cast<s64>(delta) * 4294967296ll;
    if (d < 0) d -= k - 1;
    if (d > 0) d += k - 1;
    return d / k;
}

s32 clamp_transfer(sim::Memory& m, u32 rect) {
    s32 w = m.r16s(rect + 4), h = m.r16s(rect + 6);
    if (w < 0) w = 0;
    else if (w > 1024) w = 1024;
    if (h < 0) h = 0;
    else if (h > 512) h = 512;
    m.w16(rect + 4, static_cast<u32>(w));
    m.w16(rect + 6, static_cast<u32>(h));
    return (w * h + 1) / 2;
}
}

Gpu::Gpu(sim::Memory& mem) : vram(static_cast<size_t>(kVramW) * kVramH, 0), m(mem) {
    build_luts();
}

void Gpu::build_luts() {
    for (int v = 0; v < 512; ++v) {
        const int p = v >> 3;
        lut_plain[v] = static_cast<u8>(p > 31 ? 31 : p);
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x) {
                const int d = (v + kDither[y][x]) >> 3;
                lut_dither[y][x][v] = static_cast<u8>(d < 0 ? 0 : (d > 31 ? 31 : d));
            }
    }
}

void Gpu::sw_reset() {
    regs = Regs{};
    load_cancel();
    display.enabled = 0;
    display.x = display.y = 0;
    display.w = 256;
    display.h = 240;
    display.rgb24 = 0;
}

void Gpu::gp0_state(u32 w) {
    switch (w >> 24) {
    case 0xE1: regs.texpage = static_cast<u16>(w & 0x3FFFu); break;
    case 0xE2: regs.texwin = w & 0xFFFFFu; break;
    case 0xE3:
        regs.area_x0 = static_cast<s32>(w & 0x3FFu);
        regs.area_y0 = static_cast<s32>((w >> 10) & 0x3FFu);
        break;
    case 0xE4:
        regs.area_x1 = static_cast<s32>(w & 0x3FFu);
        regs.area_y1 = static_cast<s32>((w >> 10) & 0x3FFu);
        break;
    case 0xE5:
        regs.ofs_x = sx11(static_cast<s32>(w & 0x7FFu));
        regs.ofs_y = sx11(static_cast<s32>((w >> 11) & 0x7FFu));
        break;
    case 0xE6:
        regs.mask_or = (w & 1u) ? 0x8000u : 0u;
        regs.mask_and = (w & 2u) ? 0x8000u : 0u;
        break;
    default: break;
    }
}

void Gpu::texpage_attr(u16 attr) {
    regs.texpage = static_cast<u16>((regs.texpage & 0x3600u) | (attr & 0x09FFu));
}

void Gpu::texwin(u16* mask, u16* off) const {
    const u32 w = regs.texwin;
    const u32 mx = (w & 0x1Fu) << 3, my = ((w >> 5) & 0x1Fu) << 3;
    const u32 ox = ((w >> 10) & 0x1Fu) << 3, oy = ((w >> 15) & 0x1Fu) << 3;
    if (mask) *mask = static_cast<u16>((my << 8) | mx);
    if (off) *off = static_cast<u16>((oy << 8) | ox);
}

void Gpu::shade_setup(Shade& s, int textured, int raw, int semitrans, u16 tpage, u16 clut, u16 tw_mask,
                      u16 tw_off) {
    s = Shade{};
    s.textured = textured;
    s.raw = raw;
    s.blend = semitrans ? static_cast<int>((tpage >> 5) & 3u) : -1;
    if (!textured) return;
    s.page_x = (tpage & 0xFu) * 64u;
    s.page_y = ((tpage >> 4) & 1u) * 256u;
    s.depth = (tpage >> 7) & 3u;
    s.clut_x = (clut & 0x3Fu) * 16u;
    s.clut_y = (clut >> 6) & 0x1FFu;
    const u32 mu = tw_mask & 0xFFu, mv = static_cast<u32>(tw_mask) >> 8;
    const u32 ou = tw_off & 0xFFu, ov = static_cast<u32>(tw_off) >> 8;
    s.and_u = ~mu & 0xFFu;
    s.or_u = ou & mu;
    s.and_v = ~mv & 0xFFu;
    s.or_v = ov & mv;
}

u16 Gpu::texel(const Shade& s, u32 u, u32 v) {
    u = ((u & 0xFFu) & s.and_u) | s.or_u;
    v = ((v & 0xFFu) & s.and_v) | s.or_v;
    const s32 y = static_cast<s32>(s.page_y + v);
    switch (s.depth) {
    case 0: {
        const u32 w = at(static_cast<s32>(s.page_x + (u >> 2)), y);
        const u32 i = (w >> ((u & 3u) * 4u)) & 0xFu;
        return at(static_cast<s32>(s.clut_x + i), static_cast<s32>(s.clut_y));
    }
    case 1: {
        const u32 w = at(static_cast<s32>(s.page_x + (u >> 1)), y);
        const u32 i = (w >> ((u & 1u) * 8u)) & 0xFFu;
        return at(static_cast<s32>(s.clut_x + i), static_cast<s32>(s.clut_y));
    }
    default: return at(static_cast<s32>(s.page_x + u), y);
    }
}

void Gpu::plot(s32 x, s32 y, u16 c, int blend) {
    u16& d = at(x, y);
    const u16 bg = d;
    if (bg & regs.mask_and) return;
    if (blend >= 0) c = static_cast<u16>((c & 0x8000u) | blend15(bg, c, blend));
    d = static_cast<u16>(c | regs.mask_or);
}

void Gpu::shade(const Shade& s, s32 x, s32 y, u32 r, u32 g, u32 b, u32 u, u32 v) {
    const u8* lut = s.dither ? lut_dither[y & 3][x & 3] : lut_plain;
    if (s.textured) {
        u16 t = texel(s, u, v);
        if (!t) return;
        if (!s.raw)
            t = static_cast<u16>((t & 0x8000u) | lut[((t & 0x1Fu) * r) >> 4] |
                                 (lut[(((t >> 5) & 0x1Fu) * g) >> 4] << 5) |
                                 (lut[(((t >> 10) & 0x1Fu) * b) >> 4] << 10));
        plot(x, y, t, (s.blend >= 0 && (t & 0x8000u)) ? s.blend : -1);
    } else {
        const u16 c = static_cast<u16>(lut[r] | (lut[g] << 5) | (lut[b] << 10));
        plot(x, y, c, s.blend);
    }
}

void Gpu::interp_add(Interp& i, const Interp& d, u32 n) {
    i.u += d.u * n;
    i.v += d.v * n;
    i.r += d.r * n;
    i.g += d.g * n;
    i.b += d.b * n;
}

void Gpu::draw_span(const Shade& s, s32 y, s32 x_start, s32 x_bound, Interp ig, const Grad& g) {
    s32 x_ig = x_start;
    s32 w = x_bound - x_start;
    s32 x = sx11(x_start);
    if (x < regs.area_x0) {
        const s32 d = regs.area_x0 - x;
        x_ig += d;
        x += d;
        w -= d;
    }
    if (x + w > regs.area_x1 + 1) w = regs.area_x1 + 1 - x;
    if (w <= 0) return;
    interp_add(ig, g.dx, static_cast<u32>(x_ig));
    interp_add(ig, g.dy, static_cast<u32>(y));
    do {
        shade(s, x, y, ig.r >> 24, ig.g >> 24, ig.b >> 24, ig.u >> 24, ig.v >> 24);
        ++x;
        interp_add(ig, g.dx, 1);
    } while (--w > 0);
}

void Gpu::draw_triangle(const Shade& s, int gouraud, Vert* v) {
    unsigned core;
    {
        unsigned cv;
        if (v[1].x <= v[0].x) cv = (v[2].x <= v[1].x) ? 4u : 2u;
        else if (v[2].x < v[0].x) cv = 4u;
        else cv = 1u;
        if (v[2].y < v[1].y) {
            std::swap(v[2], v[1]);
            cv = ((cv >> 1) & 2u) | ((cv << 1) & 4u) | (cv & 1u);
        }
        if (v[1].y < v[0].y) {
            std::swap(v[1], v[0]);
            cv = ((cv >> 1) & 1u) | ((cv << 1) & 2u) | (cv & 4u);
        }
        if (v[2].y < v[1].y) {
            std::swap(v[2], v[1]);
            cv = ((cv >> 1) & 2u) | ((cv << 1) & 4u) | (cv & 1u);
        }
        core = cv >> 1;
    }
    if (v[0].y == v[2].y) return;
    if (v[2].y - v[0].y >= 512) return;
    if (std::abs(v[2].x - v[0].x) >= 1024 || std::abs(v[2].x - v[1].x) >= 1024 || std::abs(v[1].x - v[0].x) >= 1024)
        return;

    Grad g{};
    {
        const Vert *A = &v[0], *B = &v[1], *C = &v[2];
        const s32 denom = (B->x - A->x) * (C->y - B->y) - (C->x - B->x) * (B->y - A->y);
        if (!denom) return;
        auto across = [&](s32 a, s32 b, s32 c) {
            return static_cast<s64>(b - a) * (C->y - B->y) - static_cast<s64>(c - b) * (B->y - A->y);
        };
        auto down = [&](s32 a, s32 b, s32 c) {
            return static_cast<s64>(B->x - A->x) * (c - b) - static_cast<s64>(C->x - B->x) * (b - a);
        };
        if (gouraud) {
            g.dx.r = grad_term(across(A->r, B->r, C->r), denom);
            g.dy.r = grad_term(down(A->r, B->r, C->r), denom);
            g.dx.g = grad_term(across(A->g, B->g, C->g), denom);
            g.dy.g = grad_term(down(A->g, B->g, C->g), denom);
            g.dx.b = grad_term(across(A->b, B->b, C->b), denom);
            g.dy.b = grad_term(down(A->b, B->b, C->b), denom);
        }
        if (s.textured) {
            g.dx.u = grad_term(across(A->u, B->u, C->u), denom);
            g.dy.u = grad_term(down(A->u, B->u, C->u), denom);
            g.dx.v = grad_term(across(A->v, B->v, C->v), denom);
            g.dy.v = grad_term(down(A->v, B->v, C->v), denom);
        }
    }

    Interp ig;
    ig.u = (static_cast<u32>(v[core].u << kFbs) + (1u << (kFbs - 1))) << kPad;
    ig.v = (static_cast<u32>(v[core].v << kFbs) + (1u << (kFbs - 1))) << kPad;
    ig.r = (static_cast<u32>(v[core].r << kFbs) + (1u << (kFbs - 1))) << kPad;
    ig.g = (static_cast<u32>(v[core].g << kFbs) + (1u << (kFbs - 1))) << kPad;
    ig.b = (static_cast<u32>(v[core].b << kFbs) + (1u << (kFbs - 1))) << kPad;
    interp_add(ig, g.dx, static_cast<u32>(-v[core].x));
    interp_add(ig, g.dy, static_cast<u32>(-v[core].y));

    const s64 base_coord = xfp(v[0].x);
    const s64 base_step = xfp_step(v[2].x - v[0].x, v[2].y - v[0].y);
    s64 upper_step, lower_step;
    int right_facing;
    if (v[1].y == v[0].y) {
        upper_step = 0;
        right_facing = v[1].x > v[0].x;
    } else {
        upper_step = xfp_step(v[1].x - v[0].x, v[1].y - v[0].y);
        right_facing = upper_step > base_step;
    }
    lower_step = (v[2].y == v[1].y) ? 0 : xfp_step(v[2].x - v[1].x, v[2].y - v[1].y);

    struct Half {
        u64 x[2], step[2];
        s32 y, y_bound;
        int upward;
    } part[2];
    const unsigned vo = core ? 1u : 0u;
    const unsigned vp = core == 2 ? 3u : 0u;
    const int rf = right_facing ? 1 : 0;

    part[vo].y = v[0 ^ vo].y;
    part[vo].y_bound = v[1 ^ vo].y;
    part[vo].x[rf] = static_cast<u64>(xfp(v[0 ^ vo].x));
    part[vo].step[rf] = static_cast<u64>(upper_step);
    part[vo].x[!rf] = static_cast<u64>(base_coord + static_cast<s64>(v[vo].y - v[0].y) * base_step);
    part[vo].step[!rf] = static_cast<u64>(base_step);
    part[vo].upward = vo != 0;

    part[vo ^ 1].y = v[1 ^ vp].y;
    part[vo ^ 1].y_bound = v[2 ^ vp].y;
    part[vo ^ 1].x[rf] = static_cast<u64>(xfp(v[1 ^ vp].x));
    part[vo ^ 1].step[rf] = static_cast<u64>(lower_step);
    part[vo ^ 1].x[!rf] = static_cast<u64>(base_coord + static_cast<s64>(v[1 ^ vp].y - v[0].y) * base_step);
    part[vo ^ 1].step[!rf] = static_cast<u64>(base_step);
    part[vo ^ 1].upward = vp != 0;

    for (int i = 0; i < 2; ++i) {
        s32 yi = part[i].y;
        const s32 yb = part[i].y_bound;
        u64 lc = part[i].x[0], ls = part[i].step[0];
        u64 rc = part[i].x[1], rs = part[i].step[1];
        if (part[i].upward) {
            while (yi > yb) {
                --yi;
                lc -= ls;
                rc -= rs;
                const s32 y = sx11(yi);
                if (y < regs.area_y0) break;
                if (y > regs.area_y1) continue;
                draw_span(s, yi, xfp_int(lc), xfp_int(rc), ig, g);
            }
        } else {
            while (yi < yb) {
                const s32 y = sx11(yi);
                if (y > regs.area_y1) break;
                if (y >= regs.area_y0) draw_span(s, yi, xfp_int(lc), xfp_int(rc), ig, g);
                ++yi;
                lc += ls;
                rc += rs;
            }
        }
    }
}

void Gpu::sw_poly(const Poly& p) {
    if (observer) observer->gpu_poly(*this, p);
    if (!rasterize) return;
    const int textured = p.textured != 0;
    const int raw = textured && p.raw;
    Shade s;
    shade_setup(s, textured, raw, p.semitrans, p.tpage, p.clut, p.twin_mask, p.twin_off);
    const int dtd = (regs.texpage >> 9) & 1;
    s.dither = dtd && (p.gouraud || (textured && !raw));
    Vert v[4];
    const int n = p.nverts == 4 ? 4 : 3;
    for (int i = 0; i < n; ++i) {
        v[i].x = sx11(p.x[i]) + regs.ofs_x;
        v[i].y = sx11(p.y[i]) + regs.ofs_y;
        v[i].u = p.u[i];
        v[i].v = p.v[i];
        v[i].r = p.r[i];
        v[i].g = p.g[i];
        v[i].b = p.b[i];
    }
    Vert t[3] = {v[0], v[1], v[2]};
    draw_triangle(s, p.gouraud, t);
    if (n == 4) {
        Vert q[3] = {v[1], v[2], v[3]};
        draw_triangle(s, p.gouraud, q);
    }
}

void Gpu::sw_rect(const Rect& r) {
    if (observer) observer->gpu_rect(*this, r);
    if (!rasterize) return;
    const int textured = r.textured != 0;
    Shade s;
    shade_setup(s, textured, textured && r.raw, r.semitrans, r.tpage, r.clut, r.twin_mask, r.twin_off);
    s.dither = 0;
    s32 x = sx11(sx11(r.x) + regs.ofs_x);
    s32 y = sx11(sx11(r.y) + regs.ofs_y);
    s32 x_bound = x + static_cast<s32>(r.w & 0x3FFu);
    s32 y_bound = y + static_cast<s32>(r.h & 0x1FFu);
    u8 u = r.u, v = r.v;
    int u_inc = 1, v_inc = 1;
    if (textured) {
        if (regs.texpage & 0x1000u) {
            u_inc = -1;
            u |= 1;
        }
        if (regs.texpage & 0x2000u) v_inc = -1;
    }
    if (x < regs.area_x0) {
        if (textured) u = static_cast<u8>(u + (regs.area_x0 - x) * u_inc);
        x = regs.area_x0;
    }
    if (y < regs.area_y0) {
        if (textured) v = static_cast<u8>(v + (regs.area_y0 - y) * v_inc);
        y = regs.area_y0;
    }
    if (x_bound > regs.area_x1 + 1) x_bound = regs.area_x1 + 1;
    if (y_bound > regs.area_y1 + 1) y_bound = regs.area_y1 + 1;
    if (x >= x_bound || y >= y_bound) return;
    for (; y < y_bound; ++y) {
        u8 ur = u;
        for (s32 px = x; px < x_bound; ++px) {
            shade(s, px, y, r.r, r.g, r.b, ur, v);
            ur = static_cast<u8>(ur + u_inc);
        }
        v = static_cast<u8>(v + v_inc);
    }
}

void Gpu::draw_segment(const Shade& s, int gouraud, int dither, s32 x0, s32 y0, u32 r0, u32 g0, u32 b0, s32 x1,
                       s32 y1, u32 r1, u32 g1, u32 b1) {
    const s32 dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
    if (dx >= 1024 || dy >= 512) return;
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
        std::swap(r0, r1);
        std::swap(g0, g1);
        std::swap(b0, b1);
    }
    const s32 k = dx > dy ? dx : dy;
    s64 stx = 0, sty = 0;
    s32 sr = 0, sg = 0, sb = 0;
    if (k) {
        stx = line_div(x1 - x0, k);
        sty = line_div(y1 - y0, k);
        if (gouraud) {
            sr = static_cast<s32>(static_cast<u32>(static_cast<s32>(r1 - r0)) << 12) / k;
            sg = static_cast<s32>(static_cast<u32>(static_cast<s32>(g1 - g0)) << 12) / k;
            sb = static_cast<s32>(static_cast<u32>(static_cast<s32>(b1 - b0)) << 12) / k;
        }
    }
    u64 cx = (static_cast<u64>(static_cast<s64>(x0)) << 32) | (1ull << 31);
    u64 cy = (static_cast<u64>(static_cast<s64>(y0)) << 32) | (1ull << 31);
    cx -= 1024;
    if (sty < 0) cy -= 1024;
    u32 cr = (r0 << 12) | (1u << 11);
    u32 cg = (g0 << 12) | (1u << 11);
    u32 cb = (b0 << 12) | (1u << 11);
    for (s32 i = 0; i <= k; ++i) {
        const s32 x = static_cast<s32>((cx >> 32) & 2047u);
        const s32 y = static_cast<s32>((cy >> 32) & 2047u);
        if (x >= regs.area_x0 && x <= regs.area_x1 && y >= regs.area_y0 && y <= regs.area_y1) {
            const u32 r = gouraud ? ((cr >> 12) & 0xFFu) : r0;
            const u32 g = gouraud ? ((cg >> 12) & 0xFFu) : g0;
            const u32 b = gouraud ? ((cb >> 12) & 0xFFu) : b0;
            const u8* lut = dither ? lut_dither[y & 3][x & 3] : lut_plain;
            plot(x, y, static_cast<u16>(lut[r] | (lut[g] << 5) | (lut[b] << 10)), s.blend);
        }
        cx += static_cast<u64>(stx);
        cy += static_cast<u64>(sty);
        cr += static_cast<u32>(sr);
        cg += static_cast<u32>(sg);
        cb += static_cast<u32>(sb);
    }
}

void Gpu::sw_line(const Line& l) {
    if (l.nverts < 2) return;
    if (observer) observer->gpu_line(*this, l);
    if (!rasterize) return;
    Shade s;
    shade_setup(s, 0, 0, l.semitrans, l.tpage, 0, 0, 0);
    const int dither = l.gouraud && ((regs.texpage >> 9) & 1);
    for (int i = 0; i + 1 < l.nverts; ++i) {
        const s32 x0 = sx11(l.x[i]) + regs.ofs_x, y0 = sx11(l.y[i]) + regs.ofs_y;
        const s32 x1 = sx11(l.x[i + 1]) + regs.ofs_x, y1 = sx11(l.y[i + 1]) + regs.ofs_y;
        draw_segment(s, l.gouraud, dither, x0, y0, l.r[i], l.g[i], l.b[i], x1, y1, l.r[i + 1], l.g[i + 1],
                     l.b[i + 1]);
    }
}

void Gpu::sw_fill(s32 x, s32 y, s32 w, s32 h, u16 pixel) {
    if (observer) observer->gpu_fill(*this, x, y, w, h, pixel);
    for (s32 j = 0; j < h; ++j)
        for (s32 i = 0; i < w; ++i) at(x + i, y + j) = pixel;
}

void Gpu::sw_copy(s32 sx, s32 sy, s32 w, s32 h, s32 dx, s32 dy) {
    if (observer) observer->gpu_copy(*this, sx, sy, w, h, dx, dy);
    for (s32 y = 0; y < h; ++y)
        for (s32 x = 0; x < w; x += 128) {
            const s32 n = w - x < 128 ? w - x : 128;
            u16 buf[128];
            for (s32 i = 0; i < n; ++i) buf[i] = at(sx + x + i, sy + y);
            for (s32 i = 0; i < n; ++i) {
                u16& d = at(dx + x + i, dy + y);
                if (!(d & regs.mask_and)) d = static_cast<u16>(buf[i] | regs.mask_or);
            }
        }
}

void Gpu::load_begin(s32 x, s32 y, s32 w, s32 h) {
    up.x = x;
    up.y = y;
    up.w = w;
    up.h = h;
    up.cx = up.cy = 0;
    up.left = (w > 0 && h > 0) ? static_cast<u32>(w) * static_cast<u32>(h) : 0u;
}

void Gpu::load_pixel(u16 p) {
    if (!up.left) return;
    u16& d = at(up.x + up.cx, up.y + up.cy);
    if (!(d & regs.mask_and)) d = static_cast<u16>(p | regs.mask_or);
    if (++up.cx == up.w) {
        up.cx = 0;
        ++up.cy;
    }
    if (!--up.left && observer) observer->gpu_upload(*this, up.x, up.y, up.w, up.h);
}

void Gpu::load_cancel() {
    if (observer && up.left && (up.cx || up.cy)) observer->gpu_upload(*this, up.x, up.y, up.w, up.cy + (up.cx ? 1 : 0));
    up = Upload{};
}

void Gpu::load_word(u32 w) {
    load_pixel(static_cast<u16>(w & 0xFFFFu));
    load_pixel(static_cast<u16>(w >> 16));
}

void Gpu::decode_poly(u32 addr, u8 code) {
    Poly p;
    p.nverts = (code & 0x08) ? 4 : 3;
    p.textured = (code & 0x04) != 0;
    p.gouraud = (code & 0x10) != 0;
    p.semitrans = (code & 0x02) != 0;
    p.raw = (code & 0x01) != 0;
    u32 a = addr;
    unpack_rgb(m.r32(a), p.r[0], p.g[0], p.b[0]);
    a += 4;
    for (int i = 0; i < p.nverts; ++i) {
        if (p.gouraud && i > 0) {
            unpack_rgb(m.r32(a), p.r[i], p.g[i], p.b[i]);
            a += 4;
        } else if (i > 0) {
            p.r[i] = p.r[0];
            p.g[i] = p.g[0];
            p.b[i] = p.b[0];
        }
        const u32 xy = m.r32(a);
        a += 4;
        p.x[i] = static_cast<s16>(xy);
        p.y[i] = static_cast<s16>(xy >> 16);
        if (p.textured) {
            const u32 uv = m.r32(a);
            a += 4;
            p.u[i] = static_cast<u8>(uv);
            p.v[i] = static_cast<u8>(uv >> 8);
            if (i == 0) p.clut = static_cast<u16>(uv >> 16);
            if (i == 1) p.tpage = static_cast<u16>(uv >> 16);
        }
    }
    if (p.textured) texpage_attr(p.tpage);
    else p.tpage = regs.texpage;
    texwin(&p.twin_mask, &p.twin_off);
    sw_poly(p);
}

void Gpu::draw_rect(Rect& r) {
    r.tpage = regs.texpage;
    texwin(&r.twin_mask, &r.twin_off);
    sw_rect(r);
}

void Gpu::decode_rect(u32 addr, u8 code) {
    Rect r;
    r.textured = (code & 0x04) != 0;
    r.semitrans = (code & 0x02) != 0;
    r.raw = (code & 0x01) != 0;
    u32 a = addr;
    unpack_rgb(m.r32(a), r.r, r.g, r.b);
    a += 4;
    const u32 xy = m.r32(a);
    a += 4;
    r.x = static_cast<s16>(xy);
    r.y = static_cast<s16>(xy >> 16);
    if (r.textured) {
        const u32 uv = m.r32(a);
        a += 4;
        r.u = static_cast<u8>(uv);
        r.v = static_cast<u8>(uv >> 8);
        r.clut = static_cast<u16>(uv >> 16);
    }
    switch ((code >> 3) & 3) {
    case 0: {
        const u32 wh = m.r32(a);
        r.w = static_cast<u16>(wh & 0x3FFu);
        r.h = static_cast<u16>((wh >> 16) & 0x1FFu);
        break;
    }
    case 1: r.w = r.h = 1; break;
    case 2: r.w = r.h = 8; break;
    default: r.w = r.h = 16; break;
    }
    draw_rect(r);
}

void Gpu::decode_line(u32 addr, u8 code, int len) {
    Line l;
    l.gouraud = (code & 0x10) != 0;
    l.semitrans = (code & 0x02) != 0;
    l.tpage = regs.texpage;
    const int polyline = (code & 0x08) != 0;
    unpack_rgb(m.r32(addr), l.r[0], l.g[0], l.b[0]);
    int n = 0, w = 1;
    while (n < 256) {
        if (n > 0 && l.gouraud) {
            if (w >= len) break;
            const u32 c = m.r32(addr + 4u * static_cast<u32>(w));
            if (polyline && n >= 2 && polyline_end(c)) break;
            unpack_rgb(c, l.r[n], l.g[n], l.b[n]);
            ++w;
        } else if (n > 0) {
            l.r[n] = l.r[0];
            l.g[n] = l.g[0];
            l.b[n] = l.b[0];
        }
        if (w >= len) break;
        const u32 xy = m.r32(addr + 4u * static_cast<u32>(w));
        if (polyline && !l.gouraud && n >= 2 && polyline_end(xy)) break;
        l.x[n] = static_cast<s16>(xy);
        l.y[n] = static_cast<s16>(xy >> 16);
        ++w;
        ++n;
        if (!polyline && n == 2) break;
    }
    l.nverts = n;
    if (n < 2) return;
    sw_line(l);
}

void Gpu::gp0_env(u32 w) { gp0_state(w); }

void Gpu::gp0_fill(u32 c, u32 xy, u32 wh) {
    const s32 x = static_cast<s16>(xy & 0x3F0u);
    const s32 y = pos_y(xy >> 16);
    const s32 w = static_cast<s16>(((wh & 0x3FFu) + 0x0Fu) & ~0x0Fu);
    const s32 h = static_cast<s16>((wh >> 16) & 0x1FFu);
    if (w <= 0 || h <= 0) return;
    u8 r, g, b;
    unpack_rgb(c, r, g, b);
    sw_fill(x, y, w, h, static_cast<u16>(((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3)));
}

void Gpu::gp0_copy(u32 s, u32 d, u32 z) {
    sw_copy(pos_x(s), pos_y(s >> 16), copy_w(z), copy_h(z >> 16), pos_x(d), pos_y(d >> 16));
}

void Gpu::gp0_load_begin(u32 xy, u32 wh) { load_begin(pos_x(xy), pos_y(xy >> 16), copy_w(wh), copy_h(wh >> 16)); }

void Gpu::decode_load(u32 addr, int len) {
    gp0_load_begin(m.r32(addr + 4), m.r32(addr + 8));
    for (int i = 3; i < len; ++i) load_word(m.r32(addr + 4u * static_cast<u32>(i)));
}

int Gpu::cmd_words(u32 addr, u8 code, int avail) {
    if (code >= 0x20 && code < 0x40) {
        const int nv = (code & 0x08) ? 4 : 3;
        const int per = (code & 0x04) ? 2 : 1;
        const int gour = (code & 0x10) ? nv - 1 : 0;
        return 1 + nv * per + gour;
    }
    if (code >= 0x40 && code < 0x60) {
        if (!(code & 0x08)) return (code & 0x10) ? 4 : 3;
        for (int w = 1; w < avail; ++w)
            if (m.r32(addr + 4u * static_cast<u32>(w)) == 0x55555555u) return w + 1;
        return avail;
    }
    if (code >= 0x60 && code < 0x80) {
        int n = 2 + ((code & 0x04) ? 1 : 0);
        if (((code >> 3) & 3) == 0) ++n;
        return n;
    }
    if (code == 0x02) return 3;
    if (code >= 0x80 && code < 0xA0) return 4;
    if (code >= 0xA0 && code < 0xC0) {
        const u32 size = m.r32(addr + 8);
        const u32 w = static_cast<u32>(copy_w(size)), h = static_cast<u32>(copy_h(size >> 16));
        const u32 n = 3u + (w * h + 1u) / 2u;
        return n > static_cast<u32>(avail) ? avail : static_cast<int>(n);
    }
    if (code >= 0xC0 && code < 0xE0) return 3;
    return 1;
}

void Gpu::decode_one(u32 addr, u8 code, int words) {
    if (code >= 0x20 && code < 0x40) decode_poly(addr, code);
    else if (code >= 0x40 && code < 0x60) decode_line(addr, code, words);
    else if (code >= 0x60 && code < 0x80) decode_rect(addr, code);
    else if (code >= 0xE0) gp0_env(m.r32(addr));
    else if (code >= 0xA0 && code < 0xC0) decode_load(addr, words);
    else if (code >= 0x80 && code < 0xA0) gp0_copy(m.r32(addr + 4), m.r32(addr + 8), m.r32(addr + 12));
    else if (code == 0x02) gp0_fill(m.r32(addr), m.r32(addr + 4), m.r32(addr + 8));
}

void Gpu::decode_packet(u32 addr, int len) {
    u32 a = addr;
    int left = len;
    while (left > 0) {
        const u32 pending = load_words_left();
        if (pending) {
            const u32 take = static_cast<u32>(left) < pending ? static_cast<u32>(left) : pending;
            for (u32 i = 0; i < take; ++i) load_word(m.r32(a + 4u * i));
            a += 4u * take;
            left -= static_cast<int>(take);
            continue;
        }
        if (in_polyline) {
            while (left > 0) {
                const u32 w = m.r32(a);
                a += 4u;
                --left;
                if (polyline_end(w)) {
                    in_polyline = 0;
                    break;
                }
            }
            continue;
        }
        const u8 code = static_cast<u8>(m.r32(a) >> 24);
        if (code >= 0x40 && code < 0x60 && (code & 0x08)) {
            int n = 1;
            while (n < left && !polyline_end(m.r32(a + 4u * static_cast<u32>(n)))) ++n;
            const bool found = n < left;
            const int take = found ? n + 1 : left;
            decode_one(a, code, take);
            if (!found) in_polyline = 1;
            a += 4u * static_cast<u32>(take);
            left -= take;
            continue;
        }
        int n = cmd_words(a, code, left);
        if (n < 1) n = 1;
        if (n > left) n = left;
        decode_one(a, code, n);
        a += 4u * static_cast<u32>(n);
        left -= n;
    }
}

void Gpu::walk_list(u32 head) {
    u32 cur = head & 0x7FFFFCu;
    load_cancel();
    in_polyline = 0;
    if (observer) observer->gpu_list(*this, true, head);
    for (unsigned guard = 0; guard < 0x40000u; ++guard) {
        const u32 tag = m.r32(0x80000000u | cur);
        u32 next = tag & 0x00FFFFFFu;
        const int len = static_cast<int>(tag >> 24);
        if (observer) observer->gpu_node(*this, 0x80000000u | cur);
        if (len > 0) decode_packet(0x80000000u | (cur + 4u), len);
        if (next & 0x800000u) break;
        next &= 0x7FFFFCu;
        if (next == cur) break;
        cur = next;
    }
    if (observer) observer->gpu_list(*this, false, head);
}

void Gpu::draw_otag(u32 ot) {
    walk_list(ot);
    ++draw_count;
}

void Gpu::draw_prim(u32 prim) {
    const u32 len = m.r32(prim) >> 24;
    if (len > 0) decode_packet(prim + 4, static_cast<int>(len));
}

void Gpu::put_draw_env(u32 env) {
    walk_list(env + 28);
    for (u32 i = 0; i < kDrawEnvBytes; ++i) cur_draw[i] = static_cast<u8>(m.r8(env + i));
}

void Gpu::put_disp_env(u32 env) {
    const s32 w = m.r16s(env + 4);
    s32 mw;
    if (w < 281) mw = 256;
    else if (w < 353) mw = 320;
    else if (w < 401) mw = 368;
    else if (w < 561) mw = 512;
    else mw = 640;
    display.x = m.r16s(env) & 0x3FF;
    display.y = m.r16s(env + 2) & 0x1FF;
    display.w = mw;
    display.h = m.r16s(env + 6) >= 257 ? 480 : 240;
    display.rgb24 = m.r8(env + 17) != 0;
    for (u32 i = 0; i < kDispEnvBytes; ++i) cur_disp[i] = static_cast<u8>(m.r8(env + i));
}

void Gpu::clear_image(u32 rect, u32 r, u32 g, u32 b) {
    s32 w = m.r16s(rect + 4), h = m.r16s(rect + 6);
    if (w < 0) w = 0;
    else if (w > kVramW - 1) w = kVramW - 1;
    if (h < 0) h = 0;
    else if (h > kVramH - 1) h = kVramH - 1;
    m.w16(rect + 4, static_cast<u32>(w));
    m.w16(rect + 6, static_cast<u32>(h));
    const u32 rgb = (r & 0xFFu) | ((g & 0xFFu) << 8) | ((b & 0xFFu) << 16);
    const u32 xy = m.r32(rect), wh = m.r32(rect + 4);
    const s32 x = m.r16s(rect);
    if (!(x & 0x3F) && !(w & 0x3F)) {
        gp0_env(0xE6000000u);
        gp0_env(0xE1000000u | (regs.texpage & 0x7FFu));
        gp0_fill(0x02000000u | rgb, xy, wh);
        return;
    }
    const u32 e3 = 0xE3000000u | (static_cast<u32>(regs.area_y0) << 10) | static_cast<u32>(regs.area_x0);
    const u32 e4 = 0xE4000000u | (static_cast<u32>(regs.area_y1) << 10) | static_cast<u32>(regs.area_x1);
    const u32 e5 =
        0xE5000000u | ((static_cast<u32>(regs.ofs_y) & 0x7FFu) << 11) | (static_cast<u32>(regs.ofs_x) & 0x7FFu);
    gp0_env(0xE3000000u);
    gp0_env(0xE4FFFFFFu);
    gp0_env(0xE5000000u);
    gp0_env(0xE6000000u);
    gp0_env(0xE1000000u | (regs.texpage & 0x7FFu));
    Rect t;
    t.x = static_cast<s16>(xy);
    t.y = static_cast<s16>(xy >> 16);
    t.w = static_cast<u16>(wh & 0x3FFu);
    t.h = static_cast<u16>((wh >> 16) & 0x1FFu);
    t.r = static_cast<u8>(r);
    t.g = static_cast<u8>(g);
    t.b = static_cast<u8>(b);
    draw_rect(t);
    gp0_env(e3);
    gp0_env(e4);
    gp0_env(e5);
}

void Gpu::load_image(u32 rect, u32 src) {
    const s32 words = clamp_transfer(m, rect);
    if (words <= 0) return;
    gp0_load_begin(m.r32(rect), m.r32(rect + 4));
    for (s32 i = 0; i < words; ++i) load_word(m.r32(src + 4u * static_cast<u32>(i)));
    load_cancel();
}

void Gpu::store_image(u32 rect, u32 dst) {
    const s32 words = clamp_transfer(m, rect);
    if (words <= 0) return;
    const u32 xy = m.r32(rect), wh = m.r32(rect + 4);
    const s32 x = pos_x(xy), y = pos_y(xy >> 16), w = copy_w(wh);
    const u32 n = static_cast<u32>(words) * 2u;
    if (!rasterize && observer) {
        const s32 rows = static_cast<s32>((n + static_cast<u32>(w) - 1u) / static_cast<u32>(w));
        for (s32 ry = y; ry < y + rows;) {
            const s32 h = std::min(y + rows - ry, kVramH - (ry & (kVramH - 1)));
            for (s32 rx = x; rx < x + w;) {
                const s32 cw = std::min(x + w - rx, kVramW - (rx & (kVramW - 1)));
                observer->gpu_read(*this, rx & (kVramW - 1), ry & (kVramH - 1), cw, h);
                rx += cw;
            }
            ry += h;
        }
    }
    for (u32 i = 0; i < n; ++i)
        m.w16(dst + 2 * i, at(x + static_cast<s32>(i % static_cast<u32>(w)), y + static_cast<s32>(i / static_cast<u32>(w))));
}

void Gpu::move_image(u32 rect, u32 x, u32 y) {
    if (!m.r16(rect + 4) || !m.r16(rect + 6)) return;
    gp0_copy(m.r32(rect), (y << 16) | (x & 0xFFFFu), m.r32(rect + 4));
}

void Gpu::reset_graph(u32 mode) {
    mode &= 7u;
    if (mode == 0 || mode == 5) {
        sw_reset();
        in_polyline = 0;
    }
    if (mode == 0 || mode == 3 || mode == 5) {
        cur_draw.fill(0xFF);
        cur_disp.fill(0xFF);
    }
}

void Gpu::set_disp_mask(u32 on) { display.enabled = on != 0; }

bool Gpu::load_state(StateReader& r) {
    if (!r.tag("GPUS")) return false;
    regs.texpage = static_cast<u16>(r.u());
    regs.texwin = r.u();
    regs.area_x0 = r.s();
    regs.area_y0 = r.s();
    regs.area_x1 = r.s();
    regs.area_y1 = r.s();
    regs.ofs_x = r.s();
    regs.ofs_y = r.s();
    regs.mask_or = static_cast<u16>(r.u());
    regs.mask_and = static_cast<u16>(r.u());
    display.enabled = r.s();
    display.x = r.s();
    display.y = r.s();
    display.w = r.s();
    display.h = r.s();
    display.rgb24 = r.s();
    if (r.u() != 0) return r.fail("a VRAM upload in progress"), false;
    if (!r.tag("VRAM")) return false;
    return r.block_into(vram.data(), vram.size() * sizeof(u16));
}

void Gpu::load_envs(const std::vector<u8>& draw, const std::vector<u8>& disp, u32 count) {
    if (draw.size() == kDrawEnvBytes) std::memcpy(cur_draw.data(), draw.data(), kDrawEnvBytes);
    if (disp.size() == kDispEnvBytes) std::memcpy(cur_disp.data(), disp.data(), kDispEnvBytes);
    draw_count = count;
}
}

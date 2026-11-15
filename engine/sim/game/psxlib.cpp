#include "core/log.hpp"
#include "core/wrap.hpp"
#include "sim/game/game.hpp"

#include <cstdio>
#include <cstring>

namespace ac3::sim {
namespace {
constexpr u32 kSinTable = 0x800B65C4u;
constexpr u32 kSqrtTable = 0x800B6DD4u;
constexpr u32 kRsqrtTable = 0x800B6F78u;
constexpr u32 kRandSeed = 0x800CBF58u;
}

s32 Game::sin_1(s32 a) {
    cover(0x800A6134u);
    auto t = [&](s32 i) { return static_cast<s32>(m.r16s(kSinTable + 2u * static_cast<u32>(i))); };
    if (a < 0x801) return a < 0x401 ? t(a) : t(0x800 - a);
    return a < 0xC01 ? -t(a - 0x800) : -t(0x1000 - a);
}

s32 Game::rsin(s32 a) {
    cover(0x800A60F8u);
    Frame f(*this, 0x18);
    if (a >= 0) return sin_1(a & 0xFFF);
    return wrap::neg(sin_1(wrap::neg(a) & 0xFFF));
}

s32 Game::rcos(s32 a) {
    cover(0x800A61C8u);
    auto t = [&](s32 i) { return static_cast<s32>(m.r16s(kSinTable + 2u * static_cast<u32>(i))); };
    if (a < 0) a = wrap::neg(a);
    a &= 0xFFF;
    if (a < 0x801) return a < 0x401 ? t(0x400 - a) : -t(a - 0x400);
    return a < 0xC01 ? -t(0xC00 - a) : t(a - 0xC00);
}

s32 Game::square_root0(s32 a) {
    cover(0x800A62F8u);
    gte.mtc2(30, static_cast<u32>(a));
    const s32 lz = static_cast<s32>(gte.mfc2(31));
    if (lz == 32) return 0;
    const s32 even = lz & ~1;
    const s32 up = (31 - even) >> 1;
    const s32 k = even - 24;
    const s32 top = k >= 0 ? static_cast<s32>(wrap::shl(static_cast<u32>(a), static_cast<u32>(k)))
                           : wrap::sar(a, static_cast<u32>(24 - even));
    const s32 i = wrap::shl(static_cast<u32>(top - 0x40), 1);
    const s32 t = m.r16s(kSqrtTable + static_cast<u32>(i));
    return static_cast<s32>(wrap::shl(static_cast<u32>(t), static_cast<u32>(up)) >> 12);
}

s32 Game::normalize_ss(s32& x, s32& y, s32& z) {
    cover(0x800A6488u);
    gte.mtc2(9, static_cast<u32>(x));
    gte.mtc2(10, static_cast<u32>(y));
    gte.mtc2(11, static_cast<u32>(z));
    gte.op(0xA00428);
    const s32 sum = wrap::add(wrap::add(static_cast<s32>(gte.mfc2(25)), static_cast<s32>(gte.mfc2(26))),
                              static_cast<s32>(gte.mfc2(27)));
    gte.mtc2(30, static_cast<u32>(sum));
    const s32 even = static_cast<s32>(gte.mfc2(31)) & ~1;
    const s32 down = (31 - even) >> 1;
    const s32 k = even - 24;
    const s32 top = k >= 0 ? static_cast<s32>(wrap::shl(static_cast<u32>(sum), static_cast<u32>(k)))
                           : wrap::sar(sum, static_cast<u32>(24 - even));
    const s32 i = wrap::shl(static_cast<u32>(top - 0x40), 1);
    gte.mtc2(8, static_cast<u32>(static_cast<s32>(m.r16s(kRsqrtTable + static_cast<u32>(i)))));
    gte.mtc2(9, static_cast<u32>(x));
    gte.mtc2(10, static_cast<u32>(y));
    gte.mtc2(11, static_cast<u32>(z));
    gte.op(0x190003D);
    x = wrap::sar(static_cast<s32>(gte.mfc2(25)), static_cast<u32>(down));
    y = wrap::sar(static_cast<s32>(gte.mfc2(26)), static_cast<u32>(down));
    z = wrap::sar(static_cast<s32>(gte.mfc2(27)), static_cast<u32>(down));
    return sum;
}

s32 Game::vector_normal_ss(u32 v, u32 out) {
    cover(0x800A6458u);
    return normal_to_short(m.r16s(v), m.r16s(v + 2), m.r16s(v + 4), out);
}

void Game::split_apply(s32 x, s32 y, s32 z, u32 out) {
    s32 hi[3], lo[3];
    const s32 in[3] = {x, y, z};
    for (int i = 0; i < 3; ++i) {
        if (in[i] >= 0) {
            hi[i] = wrap::sar(in[i], 15);
            lo[i] = in[i] & 0x7FFF;
        } else {
            const s32 n = wrap::neg(in[i]);
            hi[i] = wrap::neg(wrap::sar(n, 15));
            lo[i] = wrap::neg(n & 0x7FFF);
        }
    }
    for (int i = 0; i < 3; ++i) gte.mtc2(9 + static_cast<u32>(i), static_cast<u32>(hi[i]));
    gte.op(0x41E012);
    s32 h[3];
    for (int i = 0; i < 3; ++i) h[i] = static_cast<s32>(gte.mfc2(25 + static_cast<u32>(i)));
    for (int i = 0; i < 3; ++i) gte.mtc2(9 + static_cast<u32>(i), static_cast<u32>(lo[i]));
    gte.op(0x49E012);
    for (int i = 0; i < 3; ++i) {
        const s32 h8 = h[i] >= 0 ? static_cast<s32>(wrap::shl(static_cast<u32>(h[i]), 3))
                                 : wrap::neg(static_cast<s32>(wrap::shl(static_cast<u32>(wrap::neg(h[i])), 3)));
        m.w32(out + 4u * static_cast<u32>(i), static_cast<u32>(wrap::add(static_cast<s32>(gte.mfc2(25 + static_cast<u32>(i))), h8)));
    }
}

u32 Game::apply_matrix_lv(u32 mat, u32 v, u32 out) {
    cover(0x800A68A8u);
    for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(mat + 4 * r));
    split_apply(m.r32s(v), m.r32s(v + 4), m.r32s(v + 8), out);
    return out;
}

u32 Game::apply_rot_matrix_lv(u32 v, u32 out) {
    cover(0x800A6B18u);
    split_apply(m.r32s(v), m.r32s(v + 4), m.r32s(v + 8), out);
    return out;
}

u32 Game::mul_matrix(u32 m0, u32 m1) {
    cover(0x800A6C58u);
    return mul_matrix_into(m0, m1, m0);
}

void Game::set_mul_matrix(u32 m0, u32 m1) {
    cover(0x800A6A08u);
    for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(m0 + 4 * r));
    auto column = [&](u32 xy, u32 z) {
        gte.mtc2(0, xy);
        gte.mtc2(1, z);
        gte.op(0x486012);
    };
    column(m.r16(m1) | (m.r32(m1 + 4) & 0xFFFF0000u), m.r32(m1 + 12));
    const u32 c0x = gte.mfc2(9), c0y = gte.mfc2(10), c0z = gte.mfc2(11);
    column(m.r16(m1 + 2) | (m.r32(m1 + 8) << 16), static_cast<u32>(static_cast<s32>(m.r16s(m1 + 14))));
    const u32 c1x = gte.mfc2(9), c1y = gte.mfc2(10), c1z = gte.mfc2(11);
    column(m.r16(m1 + 4) | (m.r32(m1 + 8) & 0xFFFF0000u), m.r32(m1 + 16));
    const u32 c2x = gte.mfc2(9), c2y = gte.mfc2(10), c2z = gte.mfc2(11);
    gte.ctc2(0, (c1x << 16) | (c0x & 0xFFFFu));
    gte.ctc2(1, (c0y << 16) | (c2x & 0xFFFFu));
    gte.ctc2(2, (c2y << 16) | (c1y & 0xFFFFu));
    gte.ctc2(3, (c1z << 16) | (c0z & 0xFFFFu));
    gte.ctc2(4, c2z);
}

void Game::set_rot_matrix(u32 mat) {
    cover(0x800A7048u);
    for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(mat + 4 * r));
}

void Game::set_trans_matrix(u32 mat) {
    cover(0x800A70A8u);
    gte.ctc2(5, m.r32(mat + 0x14));
    gte.ctc2(6, m.r32(mat + 0x18));
    gte.ctc2(7, m.r32(mat + 0x1C));
}

void Game::set_geom_offset(s32 x, s32 y) {
    cover(0x800A7118u);
    gte.ctc2(24, static_cast<u32>(x) << 16);
    gte.ctc2(25, static_cast<u32>(y) << 16);
}

s32 Game::rot_trans_pers(u32 v, u32 sxy, u32 p, u32 flag) {
    cover(0x800A7358u);
    gte.mtc2(0, m.r32(v));
    gte.mtc2(1, m.r32(v + 4));
    gte.op(0x180001);
    m.w32(sxy, gte.mfc2(14));
    m.w32(p, gte.mfc2(8));
    m.w32(flag, gte.cfc2(31));
    return static_cast<s32>(gte.mfc2(19)) >> 2;
}

u32 Game::memset(u32 dst, s32 c, s32 n) {
    if (dst == 0 || n <= 0) return 0;
    for (u32 p = dst; n > 0; --n) m.w8(p++, static_cast<u32>(c));
    return dst;
}

u32 Game::libc_strlen(u32 s) {
    u32 n = 0;
    while (m.r8(s + n)) ++n;
    return n;
}

u32 Game::libc_strcpy(u32 d, u32 s) {
    u32 i = 0;
    for (;; ++i) {
        const u32 c = m.r8(s + i);
        m.w8(d + i, c);
        if (!c) break;
    }
    return d;
}

u32 Game::libc_sprintf(u32 buf, u32 fmt, std::initializer_list<u32> args) {
    return libc_sprintf(buf, fmt, args.begin(), args.size());
}

u32 Game::libc_sprintf(u32 buf, u32 fmt, const u32* av, size_t an) {
    size_t argn = 0;
    auto arg = [&]() { return argn < an ? av[argn++] : (++argn, 0u); };
    u32 n = 0;
    auto emit = [&](char c) { m.w8(buf + n++, static_cast<u8>(c)); };
    u32 p = fmt;
    for (;;) {
        const u32 c = m.r8(p++);
        if (!c) break;
        if (c != '%') {
            emit(static_cast<char>(c));
            continue;
        }
        char spec[32];
        int si = 0;
        spec[si++] = '%';
        char conv = 0;
        while (si < 31) {
            const u32 d = m.r8(p++);
            if (!d) break;
            spec[si++] = static_cast<char>(d);
            if (std::strchr("diouxXcspfgeEG%", static_cast<int>(d))) {
                conv = static_cast<char>(d);
                break;
            }
        }
        spec[si] = 0;
        if (!conv) break;
        char tmp[128];
        switch (conv) {
        case '%': emit('%'); break;
        case 'd': case 'i': case 'o': case 'u': case 'x': case 'X': {
            const u32 v = arg();
            char host[34];
            int k2 = 0;
            for (int k = 0; k < si; ++k) {
                if (spec[k] == 'l') continue;
                if (k == si - 1) host[k2++] = 'l';
                host[k2++] = spec[k];
            }
            host[k2] = 0;
            if (conv == 'd' || conv == 'i') std::snprintf(tmp, sizeof tmp, host, static_cast<long>(static_cast<s32>(v)));
            else std::snprintf(tmp, sizeof tmp, host, static_cast<unsigned long>(v));
            for (const char* q = tmp; *q; ++q) emit(*q);
            break;
        }
        case 'c': emit(static_cast<char>(arg())); break;
        case 's': {
            const u32 sa = arg();
            char sbuf[512];
            u32 i = 0;
            for (; i < sizeof sbuf - 1; ++i) {
                const u32 ch = m.r8(sa + i);
                if (!ch) break;
                sbuf[i] = static_cast<char>(ch);
            }
            sbuf[i] = 0;
            std::snprintf(tmp, sizeof tmp, spec, sbuf);
            for (const char* q = tmp; *q; ++q) emit(*q);
            break;
        }
        case 'p': {
            std::snprintf(tmp, sizeof tmp, "%08lx", static_cast<unsigned long>(arg()));
            for (const char* q = tmp; *q; ++q) emit(*q);
            break;
        }
        default: arg(); break;
        }
    }
    m.w8(buf + n, 0);
    return n;
}

u32 Game::rot_matrix(u32 ang, u32 out) {
    cover(0x800A7488u);
    constexpr u32 kTable = 0x800B7104u;
    auto sc = [&](s32 a, s32& s, s32& c) {
        const u32 e = m.r32(kTable + 4 * (static_cast<u32>(a < 0 ? -a : a) & 0xFFFu));
        const s32 v = static_cast<s16>(e);
        s = a < 0 ? -v : v;
        c = static_cast<s32>(e) >> 16;
    };
    auto mul = [](s32 a, s32 b) { return static_cast<s32>(static_cast<u32>(a) * static_cast<u32>(b)); };
    s32 sx, cx, sy, cy, sz, cz;
    sc(m.r16s(ang), sx, cx);
    sc(m.r16s(ang + 2), sy, cy);
    const s32 az = m.r16s(ang + 4);
    const s32 nsy = -sy;
    m.w16(out + 4, static_cast<u32>(sy));
    m.w16(out + 0xA, static_cast<u32>(-mul(cy, sx) >> 12));
    m.w16(out + 0x10, static_cast<u32>(mul(cy, cx) >> 12));
    sc(az, sz, cz);
    m.w16(out + 0, static_cast<u32>(mul(cz, cy) >> 12));
    m.w16(out + 2, static_cast<u32>(-mul(sz, cy) >> 12));
    s32 t8 = mul(cz, nsy) >> 12;
    m.w16(out + 6, static_cast<u32>((mul(sz, cx) >> 12) - (mul(t8, sx) >> 12)));
    m.w16(out + 0xC, static_cast<u32>((mul(sz, sx) >> 12) + (mul(t8, cx) >> 12)));
    t8 = mul(sz, nsy) >> 12;
    m.w16(out + 8, static_cast<u32>((mul(cz, cx) >> 12) + (mul(t8, sx) >> 12)));
    m.w16(out + 0xE, static_cast<u32>((mul(cz, sx) >> 12) - (mul(t8, cx) >> 12)));
    return out;
}

u32 Game::rot_matrix_gte(u32 ang, u32 out) {
    cover(0x800A79A8u);
    constexpr u32 kTable = 0x800B7104u;
    auto entry = [&](s32 a) {
        const s32 s = a >> 31;
        const u32 e = m.r32(kTable + (((static_cast<u32>((a + s) ^ s)) << 2) & 0x3FFCu));
        const u32 lo = static_cast<u32>((static_cast<s32>(e << 16) + s) ^ s) >> 16;
        return ((static_cast<u32>(static_cast<s32>(e) >> 16)) << 16) | lo;
    };
    const u32 w0 = m.r32(ang);
    const u32 z = entry(m.r16s(ang + 4));
    const u32 y = entry(static_cast<s32>(w0) >> 16);
    const u32 x = entry(static_cast<s16>(w0));
    const s32 cx = static_cast<s32>(x) >> 16, sx = static_cast<s16>(x);
    const s32 cy = static_cast<s32>(y) >> 16, sy = static_cast<s16>(y);
    const s32 cz = static_cast<s32>(z) >> 16, sz = static_cast<s16>(z);
    auto gpf = [&](s32 k, s32 a, s32 b, s32 c, s32& r1, s32& r2, s32& r3) {
        gte.mtc2(8, static_cast<u32>(k));
        gte.mtc2(9, static_cast<u32>(a));
        gte.mtc2(10, static_cast<u32>(b));
        gte.mtc2(11, static_cast<u32>(c));
        gte.op(0x198003D);
        r1 = static_cast<s32>(gte.mfc2(9));
        r2 = static_cast<s32>(gte.mfc2(10));
        r3 = static_cast<s32>(gte.mfc2(11));
    };
    s32 cxsy, cxsz, cxcz, sxsy, sxsz, sxcz, a0, a1, a2, szcy, szsxsy, szcxsy;
    gpf(cx, sy, sz, cz, cxsy, cxsz, cxcz);
    const s32 cycx = static_cast<s32>(static_cast<u32>(cy) * static_cast<u32>(cx));
    gpf(sx, sy, sz, cz, sxsy, sxsz, sxcz);
    m.w16(out + 16, static_cast<u32>(cycx >> 12));
    const s32 cysx = static_cast<s32>(static_cast<u32>(cy) * static_cast<u32>(sx));
    gpf(cz, cy, sxsy, cxsy, a0, a1, a2);
    gpf(sz, cy, sxsy, cxsy, szcy, szsxsy, szcxsy);
    m.w32(out + 4, (static_cast<u32>(a1 + cxsz) << 16) | (static_cast<u32>(sy) & 0xFFFFu));
    m.w32(out + 0, (static_cast<u32>(-szcy) << 16) | (static_cast<u32>(a0) & 0xFFFFu));
    m.w32(out + 8, (static_cast<u32>(-(cysx >> 12)) << 16) | (static_cast<u32>(cxcz - szsxsy) & 0xFFFFu));
    m.w32(out + 0xC, (static_cast<u32>(sxcz + szcxsy) << 16) | (static_cast<u32>(sxsz - a2) & 0xFFFFu));
    return out;
}

u32 Game::comp_matrix(u32 m0, u32 m1, u32 out) {
    cover(0x800A6748u);
    for (u32 k = 0; k < 5; ++k) gte.ctc2(k, m.r32(m0 + 4 * k));
    auto mv = [&](u32 xy, u32 z) {
        gte.mtc2(0, xy);
        gte.mtc2(1, z);
        gte.op(0x486012);
    };
    mv(m.r16(m1) | (m.r32(m1 + 4) & 0xFFFF0000u), m.r32(m1 + 12));
    const u32 t3 = gte.mfc2(9), t4 = gte.mfc2(10), t5 = gte.mfc2(11);
    mv(m.r16(m1 + 2) | (m.r32(m1 + 8) << 16), static_cast<u32>(m.r16s(m1 + 14)));
    const u32 t6 = gte.mfc2(9), t7 = gte.mfc2(10), t8 = gte.mfc2(11);
    mv(m.r16(m1 + 4) | (m.r32(m1 + 8) & 0xFFFF0000u), m.r32(m1 + 16));
    m.w32(out, (t6 << 16) | (t3 & 0xFFFFu));
    m.w32(out + 12, (t8 << 16) | (t5 & 0xFFFFu));
    const u32 c0 = gte.mfc2(9), c1 = gte.mfc2(10);
    m.w32(out + 16, gte.mfc2(11));
    mv(m.r16(m1 + 20) | (m.r32(m1 + 24) << 16), m.r32(m1 + 28));
    m.w32(out + 4, (t4 << 16) | (c0 & 0xFFFFu));
    m.w32(out + 8, (c1 << 16) | (t7 & 0xFFFFu));
    for (u32 k = 0; k < 3; ++k) m.w32(out + 20 + 4 * k, gte.mfc2(25 + k) + m.r32(m0 + 20 + 4 * k));
    return out;
}

u32 Game::trans_matrix(u32 mat, u32 v) {
    cover(0x800A6ED8u);
    const u32 x = m.r32(v), y = m.r32(v + 4), z = m.r32(v + 8);
    m.w32(mat + 20, x);
    m.w32(mat + 24, y);
    m.w32(mat + 28, z);
    return mat;
}

void Game::set_def_disp_env(u32 env, s32 x, s32 y, s32 w, s32 h) {
    for (u32 i = 0; i < 20; ++i) m.w8(env + i, 0);
    m.w16(env, static_cast<u32>(x));
    m.w16(env + 2, static_cast<u32>(y));
    m.w16(env + 4, static_cast<u32>(w));
    m.w16(env + 6, static_cast<u32>(h));
}

void Game::set_def_draw_env(u32 env, s32 x, s32 y, s32 w, s32 h) {
    for (u32 i = 0; i < 28; ++i) m.w8(env + i, 0);
    m.w16(env, static_cast<u32>(x));
    m.w16(env + 2, static_cast<u32>(y));
    m.w16(env + 4, static_cast<u32>(w));
    m.w16(env + 6, static_cast<u32>(h));
    m.w16(env + 8, static_cast<u32>(x));
    m.w16(env + 0xA, static_cast<u32>(y));
    m.w8(env + 0x16, 1);
    m.w8(env + 0x17, static_cast<s16>(h) < 257 ? 1u : 0u);
    m.w16(env + 0x14, 10);
}

s32 Game::rand() {
    const u32 s = m.r32(kRandSeed) * 0x41C64E6Du + 0x3039u;
    m.w32(kRandSeed, s);
    return static_cast<s32>((s >> 16) & 0x7FFFu);
}

void Game::spu_set_key(s32 on, u32 voices) {
    if (hw) hw->spu_set_key(on, voices);
    if (sound.set_key) sound.set_key(on, voices);
}

void Game::unported(u32 addr, const char* what) {
    if (unported_hits++ < 16) log::warn("sim: {:08X} ({}) is not reimplemented yet", addr, what);
}
}

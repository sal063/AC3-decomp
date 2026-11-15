#include "core/wrap.hpp"
#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kClzTab = 0x80024914u;
constexpr u32 kCtype = 0x800BCA55u;
constexpr u32 kUpper = 1u, kLower = 2u;

void udiv_qrnnd(u32& q, u32& r, u32 n1, u32 n0, u32 d) {
    const u32 dh = d >> 16, dl = d & 0xFFFFu;
    auto half = [&](u32 hi, u32 low16, u32& qh, u32& rh) {
        const wrap::HiLo t = wrap::divu(hi, dh);
        qh = t.lo;
        const u32 m = qh * dl;
        rh = (t.hi << 16) | low16;
        if (rh < m) {
            --qh;
            rh += d;
            if (rh >= d && rh < m) {
                --qh;
                rh += d;
            }
        }
        rh -= m;
    };
    u32 q1, r1, q0, r0;
    half(n1, n0 >> 16, q1, r1);
    half(r1, n0 & 0xFFFFu, q0, r0);
    q = (q1 << 16) | q0;
    r = r0;
}
}

u32 Game::libgcc_clz(u32 x) {
    const u32 a = x > 0xFFFFu ? (x > 0xFFFFFFu ? 24u : 16u) : (x >= 0x100u ? 8u : 0u);
    return 32u - (m.r8(kClzTab + (x >> a)) + a);
}

u64 Game::udivmoddi4(u64 n, u64 d, u32 rp) {
    cover(0x8009EA68u);
    Frame f(*this, 8);
    u32 n0 = static_cast<u32>(n), n1 = static_cast<u32>(n >> 32);
    u32 d0 = static_cast<u32>(d), d1 = static_cast<u32>(d >> 32);
    u32 q0 = 0, q1 = 0, r0 = 0, r1 = 0;
    if (d1 == 0) {
        u32 bm;
        if (d0 > n1) {
            bm = libgcc_clz(d0);
            if (bm) {
                d0 <<= bm;
                n1 = (n1 << bm) | (n0 >> (32 - bm));
                n0 <<= bm;
            }
            udiv_qrnnd(q0, n0, n1, n0, d0);
        } else {
            if (d0 == 0) d0 = wrap::divu(1, d0).lo;
            bm = libgcc_clz(d0);
            if (bm == 0) {
                n1 -= d0;
                q1 = 1;
            } else {
                const u32 b = 32 - bm;
                d0 <<= bm;
                const u32 n2 = n1 >> b;
                n1 = (n1 << bm) | (n0 >> b);
                n0 <<= bm;
                udiv_qrnnd(q1, n1, n2, n1, d0);
            }
            udiv_qrnnd(q0, n0, n1, n0, d0);
        }
        r0 = wrap::shr(n0, bm);
        r1 = 0;
    } else if (d1 > n1) {
        r0 = n0;
        r1 = n1;
    } else {
        const u32 bm = libgcc_clz(d1);
        if (bm == 0) {
            if (n1 > d1 || n0 >= d0) {
                q0 = 1;
                const u32 lo = n0 - d0;
                n1 = n1 - d1 - (n0 < lo ? 1u : 0u);
                n0 = lo;
            }
            r0 = n0;
            r1 = n1;
        } else {
            const u32 b = 32 - bm;
            d1 = (d1 << bm) | (d0 >> b);
            d0 <<= bm;
            const u32 n2 = n1 >> b;
            n1 = (n1 << bm) | (n0 >> b);
            n0 <<= bm;
            udiv_qrnnd(q0, n1, n2, n1, d1);
            const u64 mm = static_cast<u64>(q0) * d0;
            u32 m0 = static_cast<u32>(mm), m1 = static_cast<u32>(mm >> 32);
            if (m1 > n1 || (m1 == n1 && m0 > n0)) {
                --q0;
                const u32 lo = m0 - d0;
                m1 = m1 - d1 - (m0 < lo ? 1u : 0u);
                m0 = lo;
            }
            const u32 lo = n0 - m0;
            const u32 hi = n1 - m1 - (n0 < lo ? 1u : 0u);
            r0 = (hi << b) | (lo >> bm);
            r1 = hi >> bm;
        }
    }
    if (rp) {
        m.w32(rp, r0);
        m.w32(rp + 4, r1);
    }
    return (static_cast<u64>(q1) << 32) | q0;
}

s64 Game::divdi3(s64 a, s64 b) {
    cover(0x8009E9B0u);
    Frame f(*this, 0x28);
    bool negative = false;
    u64 ua = static_cast<u64>(a), ub = static_cast<u64>(b);
    if (a < 0) {
        negative = true;
        ua = 0 - ua;
    }
    if (b < 0) {
        negative = !negative;
        ub = 0 - ub;
    }
    m.w32(f.at(0x10), 0);
    const u64 q = udivmoddi4(ua, ub, 0);
    return static_cast<s64>(negative ? 0 - q : q);
}

u64 Game::udivdi3(u64 a, u64 b) {
    cover(0x8009EA48u);
    Frame f(*this, 0x20);
    m.w32(f.at(0x10), 0);
    return udivmoddi4(a, b, 0);
}

u32 Game::libc_setjmp(u32 buf, u32 ra, u32 sp_, u32 fp, const u32* s, u32 gp) {
    m.w32(buf, ra);
    m.w32(buf + 4, sp_);
    m.w32(buf + 8, fp);
    for (u32 k = 0; k < 8; ++k) m.w32(buf + 12 + 4 * k, s[k]);
    m.w32(buf + 44, gp);
    return 0;
}

s32 Game::libc_strcmp(u32 a, u32 b) {
    for (u32 i = 0;; ++i) {
        const s32 ca = m.r8(a + i), cb = m.r8(b + i);
        if (ca != cb) return ca - cb;
        if (!ca) return 0;
    }
}

s32 Game::libc_strncmp(u32 a, u32 b, u32 n) {
    for (u32 i = 0; i < n; ++i) {
        const s32 ca = m.r8(a + i), cb = m.r8(b + i);
        if (ca != cb) return ca - cb;
        if (!ca) break;
    }
    return 0;
}

s32 Game::libc_toupper(u32 c) {
    cover(0x800ADC18u);
    const u32 v = (m.r8(kCtype + (c & 0xFFu)) & kLower) ? c - 0x20 : c;
    return static_cast<s8>(v);
}

s32 Game::libc_tolower(u32 c) {
    cover(0x800ADC4Cu);
    const u32 v = (m.r8(kCtype + (c & 0xFFu)) & kUpper) ? c + 0x20 : c;
    return static_cast<s8>(v);
}
}

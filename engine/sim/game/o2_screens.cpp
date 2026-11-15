#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kO = 0x80112DC4u;
constexpr u32 kScr = 0x80112DB4u;
constexpr u32 kQAddr = 0x80112DC0u;
}

void Game::o2_prims_setup() {
    cover(0x800DE79Cu);
    Frame f(*this, 8);
    {
        u32 q = m.r32(kQAddr);
        for (u32 i = 0; i < 32; ++i, q += 4) {
            m.w32(q + 0xF0, 0);
            m.w32(q + 0x70, 0);
        }
    }
    const u32 q = m.r32(kQAddr);
    const u32 flags = m.r32(q + 0x5C);
    m.w16(q + 0x170, 0);
    m.w16(q + 0x172, 0);
    m.w8(flags + 2, 0);
    u32 t4 = 0x178, t5 = 0x56C8;
    for (u32 set = 0; set < 2; ++set) {
        u32 v1 = m.r32(kQAddr) + t4 + 0xE;
        for (u32 n = 0; n < 0x111; ++n, v1 += 0x28) {
            m.w8(v1 - 0xB, 9);
            m.w8(v1 - 7, 0x2E);
            m.w8(v1 - 0xA, 0x80);
            m.w8(v1 - 9, 0x80);
            m.w8(v1 - 8, 0x80);
            m.w8(v1 - 2, 0xF0);
            m.w8(v1 - 1, 0x7E);
            m.w8(v1 + 6, 0xF8);
            m.w8(v1 + 7, 0x7E);
            m.w8(v1 + 0xE, 0xF0);
            m.w8(v1 + 0xF, 0x8A);
            m.w8(v1 + 0x16, 0xF8);
            m.w8(v1 + 0x17, 0x8A);
            m.w16(v1 + 8, 0x47);
            m.w16(v1, 0x2D8);
        }
        t4 += 0x2AA8;
        const u32 a1 = m.r32(kQAddr) + t5;
        t5 += 0x28;
        m.w8(a1 + 7, 0x2C);
        m.w8(a1 + 0xD, 0x1B);
        m.w8(a1 + 0x15, 0x1B);
        m.w8(a1 + 0x1D, 0x2F);
        m.w8(a1 + 0x25, 0x2F);
        m.w16(a1 + 0x16, 7);
        m.w16(a1 + 0xE, 0x358);
        m.w8(a1 + 3, 9);
        m.w8(a1 + 4, 0x80);
        m.w8(a1 + 5, 0x80);
        m.w8(a1 + 6, 0x80);
        m.w8(a1 + 0xC, 0xD4);
        m.w8(a1 + 0x14, 0xE0);
        m.w8(a1 + 0x1C, 0xD4);
        m.w8(a1 + 0x24, 0xE0);
    }
}

void Game::o2_screens_setup() {
    cover(0x800DE924u);
    Frame f(*this, 0x20);
    m.w32(kQAddr, mem_alloc(0x5718));
    for (u32 k = 0; k < 3; ++k) {
        const u32 a = mem_alloc(0xD8);
        m.w32(m.r32(kQAddr) + 4 * k + 0x20, a);
        const u32 b = mem_alloc(0x48);
        const u32 v1 = m.r32(kQAddr) + 4 * k;
        m.w32(v1 + 0x2C, b);
        for (u32 i = 0; i < 18; ++i) {
            m.w16(m.r32(v1 + 0x20) + 12 * i, 0);
            m.w16(m.r32(v1 + 0x20) + 12 * i + 2, 0);
        }
    }
    for (u32 k = 0; k < 2; ++k) {
        const u32 a = mem_alloc(0x40);
        m.w32(m.r32(kQAddr) + 4 * k + 0x14, a);
    }
    const u32 o = m.r32(kO), q = m.r32(kQAddr);
    m.w32(q + 0x60, 0);
    m.w16(o + 0x184, 0x30);
    m.w16(o + 0x188, 0x56);
    m.w16(o + 0x18C, 0x7C);
    m.w16(o + 0x186, 0x83);
    m.w16(o + 0x18A, 0x58);
    m.w16(o + 0x18E, 0x83);
    m.w16(o + 0x190, 0xA2);
    m.w16(o + 0x192, 0x58);
    for (u32 i = 0; i < 4; ++i) {
        m.w16(o + 0x194 + 2 * i, 0xE00);
        m.w16(o + 0x19C + 2 * i, 0);
    }
    const u32 q2 = m.r32(kQAddr);
    m.w32(q2 + 0x3C, q2 + 0x40);
    m.w16(q2 + 0x40 + 0xE, 1);
    m.w16(m.r32(q2 + 0x3C) + 0xA, 0);
    m.w32(m.r32(kQAddr) + 0x50, mem_alloc(0x20));
    m.w32(m.r32(kQAddr) + 0x58, mem_alloc(0x20));
    m.w32(m.r32(kQAddr) + 0x54, mem_alloc(0x20));
    const u32 q3 = m.r32(kQAddr);
    for (u32 i = 0; i < 16; ++i) {
        m.w16(m.r32(q3 + 0x50) + 2 * i, 0);
        m.w16(m.r32(q3 + 0x58) + 2 * i, 0);
        m.w16(m.r32(q3 + 0x54) + 2 * i, 0);
    }
    m.w32(m.r32(kQAddr) + 0x5C, mem_alloc(0x40));
    for (u32 i = 0; i < 64; ++i) m.w8(m.r32(m.r32(kQAddr) + 0x5C) + i, 0);
    o2_prims_setup();
    m.w32(m.r32(kQAddr) + 0x1C, 0);
}

void Game::o2_set_scripts(u32 table, u32 g) {
    cover(0x800DE908u);
    m.w32(m.r32(kO) + 4 * (g & 0xFF) + 0x160, table);
}

void Game::o2_entry_set(u32 g, u32 i, u32 a, u32 x, u32 y, u32 z) {
    cover(0x800DEB5Cu);
    const u32 group = m.r32(kO) + 4 * g;
    m.w16(m.r32(group + 0x16C) + 12 * i + 0xA, a);
    m.w16(m.r32(group + 0x16C) + 12 * i + 4, x);
    m.w16(m.r32(group + 0x16C) + 12 * i + 6, y);
    m.w16(m.r32(group + 0x16C) + 12 * i + 8, z);
}

void Game::o2_entry_reset(u32 g, u32 i) {
    cover(0x800DECA8u);
    Frame f(*this, 0x20);
    m.w32(f.at(0x10), 0);
    m.w32(f.at(0x14), 0);
    o2_entry_set(g, i, 1, 0, 0, 0);
}

void Game::o2_group_set(u32 g, u32 bits) {
    cover(0x800DEDF0u);
    m.w32(m.r32(kO) + 4 * g + 0x178, bits);
}

u32 Game::o2_fade_set(u32 i, u32 state, s32 value, s32 end) {
    cover(0x800DEE08u);
    const u32 q = m.r32(kQAddr);
    u32 target = m.r32(q + 0x58) + 2 * i;
    m.w16(target, state);
    if (end >= 0) { target = m.r32(q + 0x54) + 2 * i; m.w16(target, static_cast<u32>(end)); }
    if (value >= 0) { target = m.r32(m.r32(kQAddr) + 0x50) + 2 * i; m.w16(target, static_cast<u32>(value)); }
    return target;
}

s32 Game::o2_fade_state(u32 i) {
    cover(0x800DEE84u);
    return m.r16s(m.r32(m.r32(kQAddr) + 0x58) + 2 * i);
}

void Game::o2_fades() {
    cover(0x800DEECCu);
    const u32 q = m.r32(kQAddr);
    for (u32 i = 0; i < 16; ++i) {
        const u32 st = m.r32(q + 0x58) + 2 * i;
        if (m.r16(st) == 1) {
            const u32 v = m.r32(q + 0x50) + 2 * i;
            if (m.r16s(v) >= m.r16s(m.r32(q + 0x54) + 2 * i)) m.w16(st, 2);
            else m.w16(v, m.r16(v) + 1);
        }
        const u32 st2 = m.r32(q + 0x58) + 2 * i;
        if (m.r16(st2) == 3) {
            const u32 v = m.r32(q + 0x50) + 2 * i;
            if (m.r16s(v) <= 0) m.w16(st2, 0);
            else m.w16(v, m.r16(v) - 1);
        }
    }
}

void Game::o2_flag(u32 i, u32 v) {
    cover(0x800DEF94u);
    m.w8(m.r32(m.r32(kQAddr) + 0x5C) + i, v);
}

bool Game::o2_op_on(u32 a, u32 at) {
    const s32 fl = m.r16s(a + at);
    if (!fl) return true;
    return m.r16s(a + at + 2) == static_cast<s32>(m.r8(m.r32(m.r32(kQAddr) + 0x5C) + static_cast<u32>(fl)));
}

u32 Game::o2_op_slot(u32 ot) {
    return ot + 4u * static_cast<u32>(m.r16s(m.r32(m.r32(kQAddr) + 0x3C) + 0xE));
}

bool Game::o2_clip() {
    const u32 c = m.r32(m.r32(kQAddr) + 0x3C);
    switch (m.r16s(c + 0xA)) {
    case 0: cover(0x800DEFECu); return true;
    case 1: return o2_clip_wipe();
    case 4: return o2_clip_ease(true);
    case 5: return o2_clip_ease(false);
    default: {
        const u32 fn = m.r32(0x8010AEACu + 4u * static_cast<u32>(m.r16s(c + 0xA)));
        u32 out = 0;
        out = call_by_address(fn, "a clip function of overlay 2", 0, 0, 0, 0);
        return out != 0;
    }
    }
}

bool Game::o2_clip_wipe() {
    cover(0x800DEFF4u);
    Frame f(*this, 8);
    const u32 q0 = m.r32(kQAddr);
    const u32 c0 = m.r32(q0 + 0x3C);
    const s32 v = m.r16s(m.r32(q0 + 0x50) + 2u * static_cast<u32>(m.r16s(c0 + 0xC)));
    m.w32(f.at(0), m.r16(c0 + 4) | (static_cast<u32>(m.r16s(c0 + 6)) << 16));
    m.w32(f.at(4), m.r16(c0 + 8));
    const u32 c = c0;
    const s32 st = m.r16s(m.r32(q0 + 0x58) + 2u * static_cast<u32>(m.r16s(c + 0xC)));
    const u32 q = m.r32(kScr);
    if (st == 0) return false;
    if (st == 2) return true;
    const s32 t0 = m.r16s(f.at(0));
    if (v < t0) return false;
    const s32 t1 = m.r16s(f.at(2));
    if (v < t1) {
        m.w16(q, m.r16(c) + static_cast<u32>(((m.r16s(q) - m.r16s(c)) * (v - t0)) / (t1 - t0)));
        const u32 c2 = m.r32(m.r32(kQAddr) + 0x3C);
        const s32 dy = m.r16s(q + 2) - m.r16s(c2 + 2);
        const s32 ady = dy < 0 ? -dy : dy;
        if (!ady) {
            unported(0x800DF134u, "screen wipe division by zero");
            return false;
        }
        m.w16(q + 2, m.r16(c2 + 2) + static_cast<u32>(dy / ady));
        m.w16(q + 4, static_cast<u32>((m.r16s(q + 4) * (v - t0)) / (t1 - t0)));
        m.w16(q + 6, 2);
        return true;
    }
    const s32 t2 = m.r16s(f.at(4));
    if (!(v < t2)) return true;
    m.w16(q + 2, m.r16(c + 2) + static_cast<u32>(((m.r16s(q + 2) - m.r16s(c + 2)) * (v - t1)) / (t2 - t1)));
    m.w16(q + 6, static_cast<u32>((m.r16s(q + 6) * (v - t1)) / (t2 - t1)));
    return true;
}

bool Game::o2_clip_ease(bool quad) {
    if (quad) cover(0x800DF5D8u); else cover(0x800DF45Cu);
    Frame f(*this, 8);
    const u32 q0 = m.r32(kQAddr);
    const u32 c = m.r32(q0 + 0x3C);
    const s32 v = m.r16s(m.r32(q0 + 0x50) + 2u * static_cast<u32>(m.r16s(c + 0xC)));
    m.w32(f.at(0), m.r16(c + 4) | (static_cast<u32>(m.r16s(c + 6)) << 16));
    m.w32(f.at(4), m.r16(c + 8));
    const u32 q = m.r32(kScr);
    const s32 st = m.r16(m.r32(m.r32(kQAddr) + 0x58) + 2u * static_cast<u32>(m.r16s(c + 0xC)));
    if (!st) return false;
    if (st == 2) return true;
    const s32 t0 = m.r16s(f.at(0)), t1 = m.r16s(f.at(2));
    if (v < t0) return false;
    if (!(v < t1)) return true;
    if (quad) {
        const s32 d = (t1 - v) * (t1 - v), dd = (t1 - t0) * (t1 - t0);
        m.w16(q, m.r16(q) + static_cast<u32>((m.r16s(c) * d) / dd));
        m.w16(q + 2, m.r16(q + 2) + static_cast<u32>((m.r16s(m.r32(m.r32(kQAddr) + 0x3C) + 2) * d) / dd));
    } else {
        m.w16(q, m.r16(q) + static_cast<u32>((m.r16s(c) * (t1 - v)) / (t1 - t0)));
        m.w16(q + 2, m.r16(q + 2) + static_cast<u32>((m.r16s(m.r32(m.r32(kQAddr) + 0x3C) + 2) * (t1 - v)) / (t1 - t0)));
    }
    return true;
}

u32 Game::o2_op(u32 op, u32 a, u32 ot, u32 e, u32 inheritedScale) {
    auto take = [&](u32 n) {
        const u32 q = m.r32(kQAddr);
        const u32 p = m.r32(q + 4);
        m.w32(q + 4, p + n);
        return p;
    };
    auto head = [&](u32 p, u32 len) {
        const u32 slot = o2_op_slot(ot);
        m.w32(p, m.r32(slot) | (len << 24));
        m.w32(o2_op_slot(ot), p & 0x00FFFFFFu);
    };
    auto rect_in = [&](u32 x, u32 wh) {
        const u32 q = m.r32(kScr);
        m.w32(q, m.r16(a + x) | (static_cast<u32>(m.r16s(a + x + 2)) << 16));
        m.w32(q + 4, m.r16(a + wh) | (static_cast<u32>(m.r16s(a + wh + 2)) << 16));
    };
    auto xy = [&](s32 dx, s32 dy) {
        const u32 q = m.r32(kScr);
        return (static_cast<u32>(m.r16s(q) + m.r16s(e + 4) + dx) & 0xFFFFu) |
               (static_cast<u32>(m.r16s(q + 2) + m.r16s(e + 6) + dy) << 16);
    };
    switch (op) {
    case 0: {
        cover(0x800DF760u);
        Frame f(*this, 0x30);
        if (!o2_op_on(a, 0xC)) return a + 20;
        const u32 p = take(16);
        rect_in(0, 4);
        if (!o2_clip()) return a + 20;
        const u32 q = m.r32(kScr);
        m.w32(p + 8, xy(0, 0));
        m.w32(p + 0xC, m.r16(q + 4) | (static_cast<u32>(m.r16s(q + 6)) << 16));
        m.w8(p + 3, 3);
        m.w32(p + 4, m.r8(a + 8) | (m.r8(a + 9) << 8) | (m.r8(a + 0xA) << 16));
        m.w8(p + 7, m.r8(a + 0xB) ? 0x62u : 0x60u);
        head(p, 3);
        return a + 20;
    }
    case 1: {
        cover(0x800DF940u);
        Frame f(*this, 0x30);
        if (!o2_op_on(a, 0x14)) return a + 0x1C;
        const u32 p = take(20);
        rect_in(0, 0x10);
        if (!o2_clip()) return a + 0x1C;
        m.w32(p + 8, xy(0, 0));
        m.w32(p + 0x10, ((static_cast<u32>(m.r16s(a + 0x10)) << 2) & 0xFFFFu) |
                            (static_cast<u32>(m.r16s(a + 0x12)) << 16));
        const u32 u = static_cast<u32>(static_cast<s32>(static_cast<s16>(m.r16s(a + 0xC) % 64)) << 2);
        const u32 v = static_cast<u32>(static_cast<s32>(static_cast<s16>(m.r16s(a + 0xE) % 256)) << 8);
        const u32 clut = (static_cast<u32>(m.r16s(a + 0xA)) << 6) | ((m.r16(a + 8) >> 4) & 0x3Fu);
        m.w32(p + 0xC, u | v | (clut << 16));
        m.w8(p + 3, 4);
        m.w32(p + 4, m.r8(a + 4) | (m.r8(a + 5) << 8) | (m.r8(a + 6) << 16));
        m.w8(p + 7, m.r8(a + 7) ? 0x66u : 0x64u);
        head(p, 4);
        return a + 0x1C;
    }
    case 2: {
        cover(0x800DFBA8u);
        Frame f(*this, 0x30);
        const u32 p = take(12);
        const u32 py = m.r16(a + 2), px = m.r16(a);
        const u32 tp = ((m.r8(a + 7) & 3u) << 5) | ((py & 0x100u) >> 4) | ((px & 0x3FFu) >> 6) | ((py & 0x200u) << 2);
        m.w32(f.at(0x10), 0);
        set_draw_mode(p, 0, 0, tp, 0);
        ot_link(o2_op_slot(ot), p);
        return a + 12;
    }
    case 3: {
        cover(0x800DFCC8u);
        Frame f(*this, 0x38);
        if (!o2_op_on(a, 0x18)) return a + 0x20;
        const u32 p = take(40);
        const bool semi = m.r8(a + 7) != 0xFF;
        const u32 abr = semi ? m.r8(a + 7) : 0u;
        rect_in(0, 0x14);
        if (!o2_clip()) return a + 0x20;
        const u32 depth = m.r8(a + 0x1E);
        const s32 k = depth == 0 ? 4 : depth == 1 ? 2 : static_cast<s32>(inheritedScale);
        s32 u0 = static_cast<s16>(m.r16s(a + 0xC) % 64) * k;
        if (m.r16s(a + 0x14) < 0) u0 -= 1;
        s32 v0 = m.r16s(a + 0xE) % 256;
        if (m.r16s(a + 0x16) < 0) v0 -= 1;
        const s32 wk = m.r16s(a + 0x10) * k;
        s32 w = wk - 1;
        if (m.r16s(a + 0x14) == wk) w = m.r16s(a + 0x14);
        s32 h = static_cast<s32>(m.r16(a + 0x12)) - 1;
        if (m.r16s(a + 0x16) == m.r16s(a + 0x12)) h = m.r16(a + 0x12);
        if (static_cast<s16>(u0) + static_cast<s16>(w) == 0x100) w -= 1;
        if (static_cast<s16>(v0) + static_cast<s16>(h) == 0x100) h -= 1;
        m.w8(p + 3, 9);
        m.w32(p + 4, m.r8(a + 4) | (m.r8(a + 5) << 8) | (m.r8(a + 6) << 16));
        m.w8(p + 7, semi ? 0x2Eu : 0x2Cu);
        const s32 u = static_cast<s16>(u0), vv = static_cast<s16>(v0);
        const u32 t0 = static_cast<u32>(vv << 8);
        const u32 clut = (static_cast<u32>(m.r16s(a + 0xA)) << 6) | ((m.r16(a + 8) >> 4) & 0x3Fu);
        m.w32(p + 0xC, static_cast<u32>(u) | t0 | (clut << 16));
        const u32 vb = static_cast<u32>((vv + static_cast<s16>(h)) << 8);
        m.w32(p + 0x1C, static_cast<u32>(u) | vb);
        const s32 ur = u + static_cast<s16>(w);
        m.w32(p + 0x24, static_cast<u32>(ur) | vb);
        const u32 py = m.r16(a + 0xE), px = m.r16(a + 0xC);
        const u32 tp = ((m.r8(a + 0x1E) & 3u) << 7) | ((abr & 3u) << 5) | ((py & 0x100u) >> 4) | ((px & 0x3FFu) >> 6) |
                       ((py & 0x200u) << 2);
        m.w32(p + 0x14, (static_cast<u32>(ur) | t0) | (tp << 16));
        const u32 q = m.r32(kScr);
        const s32 qw = m.r16s(q + 4), qh = m.r16s(q + 6);
        m.w32(p + 8, xy(0, 0));
        m.w32(p + 0x10, xy(qw, 0));
        m.w32(p + 0x18, xy(0, qh));
        m.w32(p + 0x20, xy(qw, qh));
        head(p, 9);
        return a + 0x20;
    }
    case 4:
        cover(0x800E019Cu);
        return a + 16;
    case 5:
        cover(0x800E01ACu);
        m.w32(m.r32(kQAddr) + 0x3C, a);
        return a + 16;
    case 6: return o2_line_command(a, ot, e);
    case 8: o2_markers(); return 0;
    case 7: case 9: case 10: case 11:
        cover(0x800E11E4u);
        return 0x800E11E4u;
    default:
        return call_by_address(m.r32(0x8010AEC4u + 4 * op), "a screen command of overlay 2", a, ot, e, 0);
    }
}

void Game::o2_markers() {
    cover(0x800E1B48u);
    Frame f(*this, 0x40);
    const u32 ot = m.r32(m.r32(kO) + 8);
    for (u32 i = 0; i < 4; ++i) {
        const u32 o = m.r32(kO);
        if (i == m.r8(o + 0x7E)) {
            const u32 s = m.r16(o + 2 * i + 0x19C);
            if (s < 0x80) m.w16(o + 2 * i + 0x19C, s + 1);
        } else {
            if (m.r16s(o + 2 * i + 0x19C) > 0) m.w16(o + 2 * i + 0x19C, m.r16(o + 2 * i + 0x19C) - 1);
            const u32 o2 = m.r32(kO);
            if (m.r16(o2 + 2 * i + 0x19C) == 5) m.w16(o2 + 2 * i + 0x194, m.r16(o2 + 2 * i + 0x194) - 32);
            const u32 o3 = m.r32(kO);
            if (m.r16s(o3 + 2 * i + 0x19C) < 5) m.w16(o3 + 2 * i + 0x19C, 0);
        }
        const u32 ob = m.r32(kO);
        m.w16(ob + 2 * i + 0x194, m.r16(ob + 2 * i + 0x194) + m.r16(ob + 2 * i + 0x19C));
        const u32 q = m.r32(kQAddr);
        const u32 p = m.r32(q + 4);
        m.w32(q + 4, p + 24);
        m.w8(p + 3, 5);
        m.w8(p + 7, 0x28);
        m.w8(p + 4, 0);
        m.w8(p + 5, 0);
        m.w8(p + 6, 0);
        const u32 o4 = m.r32(kO);
        const s32 ang = m.r16s(o4 + 2 * i + 0x194);
        const u32 x = m.r16(o4 + 4 * i + 0x184), y = m.r16(o4 + 4 * i + 0x186);
        const u32 el = m.r32(o4 + 0x16C);
        const u32 ex = m.r16(el + 0x10), ey = m.r16(el + 0x12);
        const s32 a0 = ang - 128, a1 = ang + 128;
        m.w16(p + 8, static_cast<u32>(22 * rcos(a0) / 4096) + x + ex);
        m.w16(p + 10, static_cast<u32>(40 * rsin(a0) / 4096) + y + ey);
        m.w16(p + 12, static_cast<u32>(22 * rcos(a1) / 4096) + x + ex);
        m.w16(p + 14, static_cast<u32>(40 * rsin(a1) / 4096) + y + ey);
        m.w16(p + 16, static_cast<u32>(18 * rcos(a0) / 4096) + x + ex);
        m.w16(p + 18, static_cast<u32>(29 * rsin(a0) / 4096) + y + ey);
        m.w16(p + 20, static_cast<u32>(18 * rcos(a1) / 4096) + x + ex);
        m.w16(p + 22, static_cast<u32>(29 * rsin(a1) / 4096) + y + ey);
        ot_link(ot + 4u * static_cast<u32>(m.r16s(m.r32(m.r32(kQAddr) + 0x3C) + 0xE)), p);
    }
}

void Game::o2_entry_motion(u32 i) {
    cover(0x800E08D4u);
    Frame f(*this, 0x20);
    const u32 e = m.r32(m.r32(kO) + 0x16C) + 12 * i;
    auto h = [&](u32 off) { return m.r16s(e + off); };
    auto set = [&](u32 off, s32 v) { m.w16(e + off, static_cast<u32>(v)); };
    auto settle_up = [&]() {
        if (h(8) < 0) set(8, h(8) + 1);
        if (!m.r16(e + 8)) set(0xA, 1);
    };
    switch (i) {
    case 0: return;
    case 1: o2_slide_screen_side(i); return;
    case 5: o2_slide_screen_top(i); return;
    case 7: o2_slide_screen_bottom(i); return;
    case 11: {
        const s32 st = h(0xA);
        if (st == 2) {
            set(4, h(4) + h(8));
            set(8, h(8) + 1);
            if (h(8) >= 26) set(0xA, 3);
        } else if (st == 0) {
            set(4, h(4) + h(8));
            settle_up();
        }
        return;
    }
    case 12: {
        const s32 st = h(0xA);
        if (st == 2) {
            set(4, h(4) + h(8));
            set(8, h(8) - 1);
            if (h(8) < -24) set(0xA, 3);
        } else if (st == 0) {
            set(4, h(4) + h(8));
            if (h(8) > 0) set(8, h(8) - 1);
            if (!m.r16(e + 8)) set(0xA, 1);
            const u32 sc = m.r32(g::kScene);
            if ((m.r32(sc + 0x128) & 8u) && (m.r32(sc + 0x12C) & 8u)) {
                set(8, -h(8));
                set(0xA, 2);
            }
            if ((m.r32(sc + 0x128) & 4u) && (m.r32(sc + 0x12C) & 4u)) {
                set(8, -h(8));
                set(0xA, 2);
            }
        }
        return;
    }
    case 13: {
        const s32 st = h(0xA);
        if (st == 2) {
            set(4, h(4) + h(8));
            set(8, h(8) + 1);
        } else if (st == 0) {
            set(4, h(4) + h(8));
            settle_up();
        }
        return;
    }
    case 14: {
        const s32 st = h(0xA);
        if (st == 0) {
            set(6, h(6) + h(8));
            if (h(8) <= 0) set(0xA, 1);
            else set(8, h(8) - 1);
        } else if (st == 2) {
            set(6, h(6) + h(8));
            if (h(8) >= -24) set(8, h(8) - 1);
            else set(0xA, 3);
        }
        return;
    }
    case 15: {
        const s32 st = h(0xA);
        const u32 o = m.r32(kO);
        if (st == 0) {
            const s32 c = m.r32s(o + 0x1A4);
            if (c >= 38) {
                set(0xA, 1);
            } else if (c >= 36) {
                if (c == 36) sound_play(92);
                set(6, h(6) + 4);
            } else if (c >= 33) {
                set(6, h(6) - 8);
            } else {
                set(6, h(6) + h(8));
                if (h(8) < 0) set(8, h(8) + 1);
            }
            const u32 o2 = m.r32(kO);
            m.w32(o2 + 0x1A4, m.r32(o2 + 0x1A4) + 1);
        } else if (st == 2) {
            set(6, h(6) + h(8));
            if (h(6) >= 480) set(0xA, 3);
            else set(8, h(8) + 2);
            const u32 o2 = m.r32(kO);
            if (m.r32s(o2 + 0x1A4) > 0) m.w32(o2 + 0x1A4, m.r32(o2 + 0x1A4) - 1);
        }
        return;
    }
    default: return;
    }
}

void Game::o2_screen_cb(u32 g, u32 i) {
    const u32 fn = m.r32(m.r32(0x8010B060u + 4 * g) + 4 * i);
    switch (fn) {
    case 0x800E03CCu: case 0x800E11ECu:
    case 0x800E1F04u: case 0x800E1F0Cu: case 0x800E1F14u: case 0x800E1F1Cu:
    case 0x800E1F24u: case 0x800E1F2Cu: case 0x800E1F34u: case 0x800E1F3Cu:
    case 0x800E1F44u: case 0x800E1F4Cu: case 0x800E1F5Cu: case 0x800E1F64u:
    case 0x800E1F74u: case 0x800E1F7Cu:
    case 0x800E11E4u: case 0x800E1F54u: case 0x800E1F6Cu:
    case 0x800E1F84u: case 0x800E1F8Cu:
        cover(fn);
        return;
    case 0x800E1B48u: o2_markers(); return;
    case 0x800E2124u: o2_marker_scan_draw(); return;
    default:
        call_by_address(fn, "a screen callback of overlay 2", 0, 0, 0, 0);
    }
}

void Game::o2_screen(u32 i, u32 g) {
    cover(0x800E22E4u);
    Frame f(*this, 0x30);
    const u32 o = m.r32(kO);
    const u32 ot = m.r32(o + 8);
    u32 at = m.r32(m.r32(o + 4 * g + 0x160) + 4 * i);
    if (!g) o2_entry_motion(i);
    while (m.r32(at) != 0xFFFFFFFFu) {
        const u32 op = m.r32(at);
        const u32 e = m.r32(m.r32(kO) + 4 * g + 0x16C) + 12 * i;
        const u32 fn = m.r32(0x8010AEC4u + 4 * op);
        if (op <= 3 || (fn >= 0x800E0000u && fn < 0x800F0000u)) at = o2_op(op, at + 4, ot, e, g);
        else at = call_by_address(fn, "a screen primitive of package o2_a", at + 4, ot, e);
    }
    o2_screen_cb(g, i);
}

void Game::o2_screens_run() {
    cover(0x800E23FCu);
    Frame f(*this, 0x28);
    for (u32 g = 0; g < 3; ++g)
        for (u32 i = 0; i < 32; ++i)
            if (m.r32(m.r32(kO) + 4 * g + 0x178) & (1u << i)) o2_screen(i, g);
}

u32 Game::o2_screens_frame(u32 p) {
    cover(0x800E2494u);
    Frame f(*this, 0x30);
    m.w32(kScr, g::kScratch);
    hw->vsync(1);
    m.w32(m.r32(kQAddr) + 4, p + 0xC);
    m.w32(f.at(0x10), 0);
    set_draw_mode(p, 0, 0, 0, 0);
    ot_link(m.r32(m.r32(kO) + 8), p);
    o2_screens_run();
    const u32 q = m.r32(kQAddr);
    const u32 p2 = m.r32(q + 4);
    m.w32(q + 4, p2 + 0xC);
    m.w32(f.at(0x10), 0);
    set_draw_mode(p2, 0, 0, 0, 0);
    ot_link(m.r32(m.r32(kO) + 8), p2);
    o2_fades();
    return m.r32(m.r32(kQAddr) + 4);
}
}

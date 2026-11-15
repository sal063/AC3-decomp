#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDigitKinds = 0x800B1074u;
constexpr u32 kGaugeFlash = 0x800C6D6Fu;
}

void Game::ot_link(u32 ot, u32 p) {
    m.w32(p, (m.r32(p) & 0xFF000000u) | (m.r32(ot) & 0x00FFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (p & 0x00FFFFFFu));
}

void Game::hud_digit(s32 n, u32 x, u32 y, u32 kind, u32 p, u32 mode) {
    cover(0x8003B7B0u);
    const u32 t = kDigitKinds + 8u * (kind == 0 ? 0u : kind == 1 ? 1u : 2u);
    if (mode == 0) pkt_rgb(p, 0, 0x83, 0);
    else if (mode == 1) pkt_rgb(p, 0x83, 0, 0);
    else if (mode == 2) pkt_rgb(p, 0x83, 0x83, 0);
    else if (mode == 3) pkt_rgb(p, 0x83, 0x83, 0x83);
    if (n == 10) {
        m.w16(p + 10, y + 3);
        m.w16(p + 16, 4);
        m.w16(p + 8, x);
        m.w16(p + 18, 3);
        m.w8(p + 12, 0);
        m.w8(p + 13, 0);
        return;
    }
    if (n == 11) {
        m.w16(p + 10, y + 2);
        m.w16(p + 16, 6);
        m.w16(p + 18, 2);
        m.w16(p + 8, x);
        m.w8(p + 12, 0x2C);
        m.w8(p + 13, 10);
        return;
    }
    if (n == -1) {
        m.w16(p + 16, 4);
        m.w16(p + 8, x);
        m.w16(p + 10, y);
        m.w16(p + 18, 6);
        m.w8(p + 12, 0x28);
        m.w8(p + 13, 6);
        return;
    }
    m.w16(p + 8, x);
    m.w16(p + 10, y);
    m.w16(p + 16, m.r16(t + 4));
    m.w16(p + 18, m.r8(t + 3));
    m.w8(p + 12, m.r8(t) + static_cast<u32>(n) * m.r32(t + 4));
    m.w8(p + 13, m.r8(t + 1));
}

void Game::gauge_digit(u32 sp_args, s32 n, u32 x, u32 y, u32 kind, u32 p, u32 mode) {
    m.w32(sp_args, p);
    m.w32(sp_args + 4, mode);
    hud_digit(n, x, y, kind, p, mode);
}

u32 Game::gauges_build(u32 st, u32 ot, u32 b) {
    cover(0x8005E40Cu);
    Frame f(*this, 0x20);
    gauge_templates(b);
    gauges_draw(st, ot, b);
    return b + 0x424;
}

void Game::gauges_draw(u32 st, u32 ot, u32 b) {
    cover(0x8005E178u);
    Frame f(*this, 0x20);
    if (m.r32(st) & 1u) gauge_speed_fill(st, ot, b + 8);
    if (m.r32(st) & 2u) gauge_altitude_fill(st, ot, b + 0x9C);
    if (m.r32(st) & 4u) gauge_heading_fill(st, ot, b + 0x1B8);
    if (m.r32(st) & 0x10u) gauge_throttle_fill(st, ot, b + 0x30C);
    if (m.r32(st) & 8u) gauge_weapons_fill(st, ot, b + 0x244);
    if (m.r32(st) & 0x40u) gauge_damage_fill(st, ot, b + 0x384);
    if (m.r32(st) & 0x80u) gauge_timer_fill(st, ot, b + 0x3DC);
    ot_link(ot, b);
}

void Game::gauge_speed_fill(u32 st, u32 ot, u32 a) {
    cover(0x8005C680u);
    Frame f(*this, 0x40);
    u32 p = a + 0x14;
    u32 x = 0x40;
    s32 v = static_cast<s32>(static_cast<u32>(m.r32s(st + 8)) * 28u) >> 12;
    for (s32 i = 0;;) {
        const s32 q = v / 10;
        gauge_digit(f.at(0x10), v - q * 10, x, 0x89, 1, p, static_cast<u32>(m.r16s(st + 36)));
        v = q;
        ot_link(ot, p);
        if (!v) break;
        x -= 10;
        if (++i >= 4) break;
        p += 0x14;
    }
    pkt_rgb(a, m.r8(st + 32), m.r8(st + 33), m.r8(st + 34));
    const u32 q0 = a + 0x64, q1 = a + 0x7C;
    const u32 ang = (m.r32(st + 8) << 2) & 0x3FFFu;
    auto sn = [&](u32 i) { return static_cast<s32>(m.r16s(g::kSinCos + 2 * i)); };
    auto cs = [&](u32 i) { return static_cast<s32>(m.r16s(g::kSinCos + 2 * (i + 0x1000))); };
    auto scale = [](s32 s) { return static_cast<s32>(static_cast<u32>(s) * 37u) >> 16; };
    auto nscale = [](s32 s) { return static_cast<s32>(0u - static_cast<u32>(s) * 37u) >> 16; };
    const u32 a0 = (ang - 0x600u) & 0x3FFFu, a2 = (ang + 0x600u) & 0x3FFFu;
    pkt_rgb(q1, m.r8(st + 28), m.r8(st + 29), m.r8(st + 30));
    pkt_rgb(q0, m.r8(st + 28), m.r8(st + 29), m.r8(st + 30));
    s32 t = scale(sn(a0));
    m.w16(q0 + 8, static_cast<u32>(t + 20));
    m.w16(q1 + 8, static_cast<u32>(20 - t));
    t = nscale(cs(a0));
    m.w16(q0 + 10, static_cast<u32>(t + 0x89));
    m.w16(q1 + 10, static_cast<u32>(0x89 - t));
    t = scale(sn(ang));
    m.w16(q0 + 12, static_cast<u32>(t + 20));
    m.w16(q1 + 12, static_cast<u32>(20 - t));
    t = nscale(cs(ang));
    m.w16(q0 + 14, static_cast<u32>(t + 0x89));
    m.w16(q1 + 14, static_cast<u32>(0x89 - t));
    m.w16(q1 + 16, 20);
    m.w16(q0 + 16, 20);
    m.w16(q1 + 18, 0x89);
    m.w16(q0 + 18, 0x89);
    t = scale(sn(a2));
    m.w16(q0 + 20, static_cast<u32>(t + 20));
    m.w16(q1 + 20, static_cast<u32>(20 - t));
    t = nscale(cs(a2));
    m.w16(q0 + 22, static_cast<u32>(t + 0x89));
    m.w16(q1 + 22, static_cast<u32>(0x89 - t));
    m.w32(a, (m.r32(a) & 0xFF000000u) | (m.r32(ot) & 0x00FFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (q0 & 0x00FFFFFFu));
}

void Game::gauge_altitude_fill(u32 st, u32 ot, u32 a) {
    cover(0x8005CB48u);
    Frame f(*this, 0x50);
    m.w32(f.at(0x54), ot);
    s32 v = static_cast<s32>(0u - static_cast<u32>(m.r32s(st + 4) >> 12) * 2u) / 3;
    s32 tens = (v / 10) % 10;
    if (v > 99999) v = 99999;
    else if (v < -9999) v = -9999;
    u32 p = a + 0x8C;
    if (v < 0) {
        gauge_digit(f.at(0x10), 11, 0xF3, 0x67, 0, a + 0xF0, static_cast<u32>(m.r16s(st + 36)));
        v = -v;
        ot_link(ot, a + 0xF0);
    }
    u32 x = 0x11B;
    for (s32 i = 0;;) {
        const s32 q = v / 10;
        m.w32(f.at(0x20), 0xFF000000u);
        gauge_digit(f.at(0x10), v - q * 10, x, 0x66, 1, p, static_cast<u32>(m.r16s(st + 36)));
        v = q;
        ot_link(ot, p);
        if (!v) break;
        x -= 10;
        if (++i >= 5) break;
        p += 0x14;
    }
    pkt_rgb(a, m.r8(st + 32), m.r8(st + 33), m.r8(st + 34));
    pkt_rgb(a + 0x14, m.r8(st + 32), m.r8(st + 33), m.r8(st + 34));
    if (tens < 0) tens += 10;
    for (s32 i = 0; i < 5; ++i) {
        const u32 s = a + 0x28 + 20u * static_cast<u32>(i);
        pkt_rgb(s, m.r8(st + 32), m.r8(st + 33), m.r8(st + 34));
        m.w16(s + 10, static_cast<u32>(tens + 0x60 - 10 + 10 * i));
    }
    const u32 r = f.at(0x18);
    m.w16(r, 0x12E);
    m.w16(r + 2, 0x60);
    m.w16(r + 4, 6);
    m.w16(r + 6, 0x28);
    if (m.r32(m.r32(g::kScene) + 4) == 1) m.w16(r + 2, 0x160);
    set_draw_area(a + 0x104, r);
    m.w16(r + 4, 0x140);
    m.w16(r, 0);
    m.w16(r + 2, 0);
    m.w16(r + 6, 0xF0);
    if (m.r32(m.r32(g::kScene) + 4) == 1) m.w16(r + 2, 0x100);
    set_draw_area(a + 0x110, r);
    cat_prim(a + 0x14, a + 0x104);
    cat_prim(a + 0x104, a + 0x28);
    cat_prim(a + 0x78, a + 0x110);
    m.w32(a + 0x110, (m.r32(a + 0x110) & 0xFF000000u) | (m.r32(ot) & 0x00FFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (a & 0x00FFFFFFu));
}

void Game::gauge_heading_fill(u32 st, u32 ot, u32 a) {
    cover(0x8005D000u);
    Frame f(*this, 0x40);
    const bool limit = m.r16s(st + 38) == 1;
    s32 hi, lo;
    if (limit && m.r32s(st + 40) < 1800) {
        const s32 t = m.r32s(st + 40);
        hi = t / 30;
        lo = (100 * (t - 30 * hi)) / 30;
    } else {
        const s32 t = m.r32s(st + 40);
        hi = t / 1800;
        lo = t / 30 - 60 * hi;
    }
    const u32 mode = limit ? 1u : (m.r8(st + 32) ? 1u : 0u);
    gauge_digit(f.at(0x10), hi / 10, 0x18, 0xD3, 1, a + 0x28, mode);
    gauge_digit(f.at(0x10), hi % 10, 0x22, 0xD3, 1, a + 0x3C, mode);
    gauge_digit(f.at(0x10), 10, 0x2C, 0xD3, 1, a + 0x50, mode);
    gauge_digit(f.at(0x10), lo / 10, 0x31, 0xD3, 1, a + 0x64, mode);
    gauge_digit(f.at(0x10), lo % 10, 0x3B, 0xD3, 1, a + 0x78, mode);
    if (limit) pkt_rgb(a, 0x83, 0, 0);
    else pkt_rgb(a, m.r8(st + 32), m.r8(st + 33), m.r8(st + 34));
    const u32 s = a + 0x14;
    bool dark;
    if (limit) {
        if (m.r32(st + 40) & 4u) {
            dark = true;
        } else {
            pkt_rgb(s, 0x83, 0, 0);
            dark = false;
        }
    } else {
        dark = (static_cast<s32>(m.r32(st + 40) << 1) / 15) & 1;
        if (!dark) pkt_rgb(s, m.r8(st + 32), m.r8(st + 33), m.r8(st + 34));
    }
    if (dark) pkt_rgb(s, 0, 0, 0);
    m.w32(a + 0x78, (m.r32(a + 0x78) & 0xFF000000u) | (m.r32(ot) & 0x00FFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (a & 0x00FFFFFFu));
}

void Game::gauge_throttle_fill(u32 st, u32 ot, u32 a) {
    cover(0x8005D468u);
    Frame f(*this, 0x30);
    const s32 raw = m.r32s(st + 12);
    const s32 v = raw >= 0x1001 ? 0x1000 : (raw < 0 ? 0 : raw);
    pkt_rgb(a, m.r8(st + 32), m.r8(st + 33), m.r8(st + 34));
    s32 pct = (v * 100) >> 12;
    const u32 bar = a + 0x14;
    u32 r = 0, g = 0;
    const bool flash = m.r8(kGaugeFlash) != 0;
    if (flash && (m.r32(st) & 0x20u)) {
        pkt_rgb(bar, m.r8(st + 32), m.r8(st + 33), m.r8(st + 34));
    } else {
        const s32 k = (131 * pct) / 100;
        r = static_cast<u32>(0x83 - k);
        g = static_cast<u32>(k);
        if (pct < 20 && (m.r32(st + 40) & 1u)) r = (r & 0xFFu) >> 2;
        m.w8(bar + 4, r);
        m.w8(bar + 5, g);
        m.w8(bar + 6, 0);
    }
    s32 c = 15 - (v >> 8);
    if (c < 0) c = 0;
    m.w16(bar + 14, static_cast<u32>((c & 3) + ((c & 0xC) << 4) + 0x7E18));
    m.w32(bar, (m.r32(bar) & 0xFF000000u) | (m.r32(ot) & 0x00FFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (a & 0x00FFFFFFu));
    if (m.r32(st) & 0x20u) {
        const u32 s = a + 0x28;
        if (m.r8(kGaugeFlash)) {
            pkt_rgb(s, m.r8(st + 32), m.r8(st + 33), m.r8(st + 34));
        } else {
            m.w8(s + 4, r);
            m.w8(s + 5, g);
            m.w8(s + 6, 0);
        }
        ot_link(ot, s);
    }
    pct = 100 - pct;
    u32 p = a + 0x3C;
    const u32 mode = static_cast<u32>(m.r16s(st + 36));
    if (pct >= 100) {
        gauge_digit(f.at(0x10), 1, 0xEE, 0xDB, 1, p, mode);
        ot_link(ot, p);
        p = a + 0x50;
    }
    if (pct >= 10) {
        gauge_digit(f.at(0x10), (pct / 10) % 10, 0xF4, 0xDB, 1, p, static_cast<u32>(m.r16s(st + 36)));
        ot_link(ot, p);
        p += 0x14;
    }
    gauge_digit(f.at(0x10), pct % 10, 0xFC, 0xDB, 1, p, static_cast<u32>(m.r16s(st + 36)));
    ot_link(ot, p);
}

void Game::gauge_weapons_fill(u32 st, u32 ot, u32 a) {
    cover(0x8005D998u);
    Frame f(*this, 0x30);
    pkt_rgb(a, m.r8(st + 32), m.r8(st + 33), m.r8(st + 34));
    ot_link(ot, a);
    u32 p = a + 0x14;
    const s32 spent = m.r32s(st + 20) - m.r32s(st + 16);
    if (m.r32s(st + 20) > 0) {
        for (s32 i = 0; i < m.r32s(st + 20); ++i, p += 0x14) {
            if (i < m.r32s(st + 24) || i < spent)
                pkt_rgb(p, m.r8(st + 32) >> 2, m.r8(st + 33) >> 2, m.r8(st + 34) >> 2);
            else
                pkt_rgb(p, m.r8(st + 32), m.r8(st + 33), m.r8(st + 34));
            ot_link(ot, p);
        }
    }
    if (m.r32s(st + 16) >= 100) {
        gauge_digit(f.at(0x10), 1, 0xED, 0xC8, 1, a + 0x8C, static_cast<u32>(m.r16s(st + 36)));
        ot_link(ot, a + 0x8C);
    }
    const s32 n = m.r32s(st + 16);
    if (n >= 10) {
        gauge_digit(f.at(0x10), (n / 10) % 10, 0xF3, 0xC8, 1, a + 0xA0, static_cast<u32>(m.r16s(st + 36)));
        ot_link(ot, a + 0xA0);
    }
    gauge_digit(f.at(0x10), m.r32s(st + 16) % 10, 0xFB, 0xC8, 1, a + 0xB4, static_cast<u32>(m.r16s(st + 36)));
    ot_link(ot, a + 0xB4);
}

void Game::gauge_damage_fill(u32 st, u32 ot, u32 a) {
    cover(0x8005DE40u);
    for (u32 i = 0; i < 2; ++i) pkt_rgb(a + 20 * i, m.r8(st + 32), m.r8(st + 33), m.r8(st + 34));
    for (u32 i = 0; i < 3; ++i) pkt_rgb(a + 0x28 + 16 * i, m.r8(st + 28) >> 1, m.r8(st + 29) >> 1, m.r8(st + 30) >> 1);
    m.w32(a + 0x48, (m.r32(a + 0x48) & 0xFF000000u) | (m.r32(ot) & 0x00FFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (a & 0x00FFFFFFu));
}

void Game::gauge_timer_fill(u32 st, u32 ot, u32 a) {
    cover(0x8005E018u);
    pkt_rgb(a + 0x28, m.r8(st + 28) >> 1, m.r8(st + 29) >> 1, m.r8(st + 30) >> 1);
    pkt_rgb(a + 0x38, m.r8(st + 28) >> 1, m.r8(st + 29) >> 1, m.r8(st + 30) >> 1);
    pkt_rgb(a, m.r8(st + 32) >> 1, m.r8(st + 33) >> 1, m.r8(st + 34) >> 1);
    pkt_rgb(a + 0x14, m.r8(st + 32) >> 1, m.r8(st + 33) >> 1, m.r8(st + 34) >> 1);
    m.w32(a + 0x14, (m.r32(a + 0x14) & 0xFF000000u) | (m.r32(ot) & 0x00FFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | ((a + 0x28) & 0x00FFFFFFu));
}
}

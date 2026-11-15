#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kLoadScreen = 0x800BCBE0u;
constexpr u32 kDisplay = 0x800BCE48u;
constexpr u32 kMissionRecords = 0x800B0B08u;
constexpr u32 kBriefingRecords = 0x800B0B84u;
constexpr u32 kLevelNames = 0x800B0D90u;
constexpr u32 kMissionNames = 0x800B0CA0u;
constexpr u32 kCursor = 0x800BCC0Cu;
constexpr u32 kZoomSizes = 0x800239A0u;
constexpr u32 kSaved = 0x1F800008u;

s32 mul(s32 a, s32 b) { return static_cast<s32>(static_cast<u32>(a) * static_cast<u32>(b)); }
}

void Game::srand(u32 seed) { m.w32(0x800CBF58u, seed); }

u32 Game::loading_frame() {
    cover(0x800357C0u);
    Frame f(*this, 0x18);
    const u32 at = cd_current();
    const u32 sc = m.r32(g::kScene);
    u32 kind = 0;
    if (at == static_cast<u32>(m.r16s(kMissionRecords + 2u * static_cast<u32>(m.r16s(sc + 0x38C))))) kind = 1;
    else if (at == static_cast<u32>(m.r16s(kBriefingRecords + 2u * static_cast<u32>(m.r16s(sc + 0x38E))))) kind = 2;
    if (kind) {
        m.w8(m.r32(kLoadScreen) + 6, kind);
        m.w16(m.r32(kLoadScreen) + 2, static_cast<u32>(load_progress(0)));
    }
    if (m.r16s(m.r32(kLoadScreen)) <= 0) loading_start();
    if (m.r16(m.r32(kLoadScreen)) - 1u < 2u) {
        const u32 s = m.r32(g::kScene);
        m.w32(s + 20, loading_clear(m.r32(s + 12), m.r32(s + 20), 0));
    }
    if (m.r16s(m.r32(kLoadScreen)) >= 3) loading_draw();
    const u32 l = m.r32(kLoadScreen);
    const u32 r = m.r32(l + 8) == 0 ? 0x04000000u : 0u;
    m.w16(l, m.r16(l) + 1);
    return r;
}

void Game::loading_start() {
    cover(0x80037070u);
    Frame f(*this, 0x18);
    m.w8(m.r32(kLoadScreen) + 5, 0);
    const u32 k = m.r8(m.r32(kLoadScreen) + 4);
    if (k == 0 || k == 1) {
        loading_reset();
    } else if (k == 2) {
        const u32 sc = m.r32(g::kScene);
        m.w32(m.r32(g::kScene) + 20, loading_copy(m.r32(sc + 12), m.r32(sc + 20)));
    }
}

void Game::loading_draw() {
    cover(0x80037108u);
    Frame f(*this, 0x18);
    const u32 k = m.r8(m.r32(kLoadScreen) + 4);
    if (k > 2) return;
    const u32 sc = m.r32(g::kScene);
    u32 p;
    if (k == 0) p = loading_panel(m.r32(sc + 8), m.r32(sc + 20));
    else if (k == 1) p = loading_zoom(m.r32(sc + 8), m.r32(sc + 20));
    else p = loading_stars(m.r32(sc + 8), m.r32(sc + 20));
    m.w32(m.r32(g::kScene) + 20, p);
}

void Game::loading_reset() {
    cover(0x800369C8u);
    const u32 d = m.r32(kDisplay);
    m.w8(d + 16, 0);
    m.w32(d, m.r32(d) | 8u);
    m.w8(m.r32(kDisplay) + 17, 0xC);
    m.w8(m.r32(kDisplay) + 18, 0);
    const u32 l = m.r32(kLoadScreen);
    m.w32(l + 0x10, 0xF8000000u);
    m.w32(l + 0x14, 0x00A00000u);
    m.w32(l + 0x1C, 0xC7);
    m.w32(l + 0x20, 4);
    m.w8(l + 0x28, 0);
    m.w32(l + 0x38, 0);
    m.w32(l + 0x0C, 0);
    m.w32(l + 0x18, 0);
    m.w32(l + 0x24, 0);
    m.w32(l + 0x34, 0x2C3B2);
    m.w16(l + 0x32, 0);
    m.w8(l + 0x29, 0xFF);
    m.w8(l + 0x2A, 0);
    m.w8(l + 0x2C, 0);
    m.w8(l + 0x2D, 0x83);
    m.w8(l + 0x2E, 0);
    m.w16(l + 0x30, 0);
}

u32 Game::loading_clear(u32 ot, u32 p, s32 level) {
    cover(0x800362A0u);
    m.w32(p, (((p + 0xC) << 8) >> 8) | 0x02000000u);
    m.w32(p + 4, 0xE6000001u);
    m.w32(p + 8, 0);
    u32 mode = 0xE1000220u;
    if (level < 0) {
        level = -level;
        mode = 0xE1000240u;
    }
    m.w32(p + 0xC, (((p + 0x14) << 8) >> 8) | 0x01000000u);
    m.w32(p + 0x10, mode);
    const u32 t = p + 0x14;
    const u32 lv = static_cast<u32>(level);
    m.w32(t, 0x03000000u);
    m.w32(t + 4, lv | (lv << 8) | (lv << 16) | 0x62000000u);
    m.w16(t + 0xC, 0x140);
    m.w16(t + 0xE, 0xF0);
    m.w32(t + 0x10, 0x02000000u);
    m.w16(t + 8, 0);
    m.w16(t + 0xA, 0);
    m.w32(t + 0x14, 0xE6000000u);
    m.w32(t + 0x18, 0);
    ot_link(ot, t + 0x10);
    ot_link(ot, t);
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (p & 0x00FFFFFFu));
    return t + 0x1C;
}

u32 Game::loading_copy(u32 ot, u32 p) {
    cover(0x80036440u);
    Frame f(*this, 0x28);
    const u32 d = m.r32(kDisplay);
    m.w32(d, m.r32(d) | 4u);
    const bool second = m.r32(m.r32(g::kScene) + 4) != 0;
    const u32 sy = second ? 0x100u : 0u, dy = second ? 0u : 0x100u;
    m.w32(f.at(0x10), 0);
    m.w32(f.at(0x14), dy);
    m.w32(f.at(0x18), 0x140);
    m.w32(f.at(0x1C), 0xF0);
    return vram_copy_packet(ot, p, 0, sy, 0, dy, 0x140, 0xF0);
}

u32 Game::loading_stars(u32 ot, u32 p) {
    cover(0x800364C4u);
    Frame f(*this, 0x58);
    const u32 t = m.r32(m.r32(g::kScene) + 0x20);
    const u32 ang = f.at(0x10), mat = f.at(0x18);
    const u32 s = m.r16(g::kSinCos + ((t << 2) & 0x7FFCu));
    m.w16(ang, s);
    m.w16(ang + 2, s);
    m.w16(ang + 4, s);
    rot_matrix_yxz(ang, mat);
    for (u32 i = 0; i < 5; ++i) gte.ctc2(i, m.r32(mat + 4 * i));
    const u32 sx = m.r16(g::kSinCos + ((t << 4) & 0x7FF0u));
    const u32 sz = m.r16(g::kSinCos + 2 * ((t << 4) & 0x3FFFu));
    const s32 ty = static_cast<s16>(s) >> 1, tx = static_cast<s16>(sx) >> 1, tz = static_cast<s16>(sz) >> 1;
    m.w16(ang + 2, static_cast<u32>(ty));
    m.w16(ang, static_cast<u32>(tx));
    m.w16(ang + 4, static_cast<u32>(tz));
    gte.ctc2(5, static_cast<u32>(tx));
    gte.ctc2(6, static_cast<u32>(ty));
    gte.ctc2(7, static_cast<u32>(tz));
    gte.ctc2(26, static_cast<u32>((rcos(static_cast<s32>(t << 4)) >> 6) + 300));
    set_geom_offset(160, 120);
    return starfield(ot, p);
}

u32 Game::starfield(u32 ot, u32 p) {
    cover(0x80083FE0u);
    Frame f(*this, 0x38);
    const u32 s = g::kScratch;
    for (u32 i = 0; i < 5; ++i) m.w32(kSaved + 4 * i, gte.cfc2(i));
    gte.mtc2(0, 0);
    gte.mtc2(1, 0x1C00);
    gte.op(0x480012);
    m.w32(p, (((p + 0x14) << 8) >> 8) | 0x04000000u);
    m.w32(p + 4, 0xE1000240u);
    m.w32(p + 8, 0x620F1F1Fu);
    m.w32(p + 0xC, 0);
    m.w32(p + 0x10, 0x00F00140u);
    m.w32(p + 0x14, (((p + 0x1C) << 8) >> 8) | 0x01000000u);
    m.w32(p + 0x18, 0xE1000220u);
    m.w32(s + 0x28, static_cast<u32>(rand()));
    srand(0x2FD);
    transpose_matrix(kSaved, kSaved);
    const u32 cx = 0u - gte.mfc2(25), cy = 0u - gte.mfc2(26), cz = 0u - gte.mfc2(27);
    m.w16(s, (cx & 0x1FFFu) - 0x1000u);
    m.w16(s + 2, (cy & 0x1FFFu) - 0x1000u);
    m.w32(s + 0x1C, cx);
    m.w32(s + 0x20, cy);
    m.w32(s + 0x24, cz);
    m.w16(s + 4, (cz & 0x1FFFu) - 0x1000u);
    for (u32 i = 0; i < 5; ++i) gte.ctc2(i, m.r32(kSaved + 4 * i));
    gte.ctc2(5, 0);
    gte.ctc2(6, 0);
    gte.ctc2(7, 0x1C00);
    gte.ctc2(21, 0x360);
    gte.ctc2(22, 0x370);
    gte.ctc2(23, 0x380);
    gte.mtc2(6, 0x6AC0B0A0u);
    u32 n = 0, q = p + 0x1C;
    for (u32 i = 0; i < 0x200; ++i) {
        gte.mtc2(0, m.r32(s));
        gte.mtc2(1, m.r32(s + 4));
        const u32 bx = m.r32(s + 0x1C), by = m.r32(s + 0x20);
        gte.op(0x180001);
        m.w16(s, ((bx + static_cast<u32>(rand())) & 0x1FFFu) - 0x1000u);
        m.w16(s + 2, ((by + static_cast<u32>(rand())) & 0x1FFFu) - 0x1000u);
        m.w16(s + 4, ((m.r16(s + 0x24) + static_cast<u32>(rand())) & 0x1FFFu) - 0x1000u);
        const u32 sxy = gte.mfc2(14);
        const s32 sz = static_cast<s32>(gte.mfc2(19));
        if (static_cast<u32>(static_cast<s32>(sxy) >> 16) >= 0xF1u) continue;
        if (static_cast<u32>(static_cast<s32>(static_cast<s16>(sxy))) >= 0x141u) continue;
        s32 d = sz - 0x1C00;
        if (d < 0) d = 0x1C00 - sz;
        if (d >= 0x1000) continue;
        gte.mtc2(8, static_cast<u32>(d));
        const u32 e = q + 4 + 8 * n;
        gte.op(0x780010);
        m.w32(e + 4, sxy);
        if (++n >= 7) {
            m.w32(q, (((q + 0x3C) << 8) >> 8) | 0x0E000000u);
            q += 0x3C;
            n = 0;
        }
        m.w32(e, gte.mfc2(22));
    }
    u32 end;
    if (n) {
        end = q + 8 * n + 4;
        m.w32(q, n << 25);
    } else {
        end = q;
        q = p + 0x14;
        if (q + 8 != end) q = end - 0x3C;
    }
    ot_link(ot, q);
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (p & 0x00FFFFFFu));
    srand(m.r32(s + 0x28));
    return end;
}

u32 Game::loading_panel(u32 ot, u32 p) {
    cover(0x80036A84u);
    Frame f(*this, 0x30);
    const u32 k = m.r8(m.r32(kLoadScreen) + 6);
    s32 v = k == 1 ? m.r16s(m.r32(kLoadScreen) + 2) : (k == 2 ? 0x1000 : 0);
    p = loading_text(ot, p);
    if (v > 0x1000) v = 0x1000;
    const s32 rest = 0x1000 - v;
    m.w8(m.r32(kLoadScreen) + 0x29, static_cast<u32>(mul(v, 255) / 4096));
    m.w8(m.r32(kLoadScreen) + 0x2D, static_cast<u32>(mul(v, 131) / 4096));
    const u32 l = m.r32(kLoadScreen);
    if (m.r32s(l + 0x38) < 0xA00) m.w32(l + 0xC, 0);
    if (v >= 0x401) m.w32(l + 0xC, m.r32(l + 0xC) | 0x80u);
    if (v >= 0x501) {
        m.w32(l + 0x14, static_cast<u32>(mul(rest, 0xA00)));
        m.w32(l + 0xC, m.r32(l + 0xC) | 1u);
    }
    if (v >= 0x601) {
        const s32 alt = -mul(rest, 0x8000);
        m.w32(l + 0x10, static_cast<u32>(alt));
        m.w32(l + 0xC, m.r32(l + 0xC) | 2u);
        if (alt > 0) m.w32(l + 0x10, 0);
    }
    if (v >= 0x701) {
        m.w32(l + 0xC, m.r32(l + 0xC) | 4u);
        m.w32(l + 0x34, static_cast<u32>(mul(rest, 181170) / 4096));
    }
    if (v >= 0x801) m.w32(l + 0xC, m.r32(l + 0xC) | 0x40u);
    if (v >= 0x901) {
        m.w32(l + 0x18, static_cast<u32>(v));
        m.w32(l + 0xC, m.r32(l + 0xC) | 0x18u);
        m.w32(l + 0x1C, static_cast<u32>(mul(rest, 199) / 4096));
        m.w32(l + 0x20, static_cast<u32>(rest / 1024));
        m.w32(l + 0x24, static_cast<u32>(v % 5));
        if (m.r32s(l + 0x20) < m.r32s(l + 0x24)) m.w32(l + 0x24, m.r32(l + 0x20));
    }
    if (!m.r8(m.r32(g::kScene) + 0x39E)) p = gauges_build(m.r32(kLoadScreen) + 0xC, ot, p);
    s32 a = static_cast<s32>(static_cast<u32>(v) << 12) / 768;
    if (a > 0x1000) a = 0x1000;
    const s32 sv = static_cast<s16>(m.r16(g::kSinCos + 2 * (static_cast<u32>(a) & 0x3FFFu))) >> 2;
    const u32 r = f.at(0x18);
    m.w16(r + 4, static_cast<u32>(mul(sv, 1280) / 4096));
    if (m.r16s(r + 4) > 0x140) m.w16(r + 4, 0x140);
    const s32 h = mul(sv, 240) / 4096;
    m.w16(r + 6, static_cast<u32>(h));
    m.w16(r + 2, static_cast<u32>((0xF0 - static_cast<s16>(h)) >> 1));
    m.w16(r, static_cast<u32>((0x140 - m.r16s(r + 4)) >> 1));
    p = noise_rect(ot, p, r, rest * 2);
    s32 lv = m.r16s(m.r32(kLoadScreen)) << 6;
    if (lv > 0x1000) lv = 0x1000;
    const u32 c0 = (static_cast<u32>(mul(0x1000 - lv, 131)) >> 4) & 0xFF00u;
    const u32 c1 = static_cast<u32>(static_cast<s32>(c0) >> 1) & 0xFF00u;
    m.w32(f.at(0x10), c1);
    p = loading_bars(ot, p, lv, c0, c1);
    const u32 l2 = m.r32(kLoadScreen);
    m.w32(l2 + 0x38, m.r32(l2 + 0x38) + 1);
    return p;
}

u32 Game::loading_zoom(u32 ot, u32 p) {
    cover(0x80036E84u);
    Frame f(*this, 0x70);
    const u32 tab = f.at(0x18);
    for (u32 i = 0; i < 17; ++i) m.w32(tab + 4 * i, m.r32(kZoomSizes + 4 * i));
    const u32 k = m.r8(m.r32(kLoadScreen) + 6);
    s32 v = k == 1 ? m.r16s(m.r32(kLoadScreen) + 2) : (k == 2 ? 0x1000 : 0);
    if (v > 0x1000) v = 0x1000;
    const s32 rest = 0x1000 - v;
    p = loading_text(ot, p);
    s32 i = m.r32s(m.r32(kLoadScreen) + 0x38);
    if (i < 0) i = 0;
    if (i > 0x10) i = 0x10;
    const u32 e = tab + 4u * static_cast<u32>(i);
    const s32 w = mul(m.r16s(e), 320) / 4096;
    m.w16(f.at(0x14), static_cast<u32>(w));
    const s32 h = mul(m.r16s(e + 2), 240) / 4096;
    m.w16(f.at(0x10), static_cast<u32>((0x140 - w) >> 1));
    m.w16(f.at(0x16), static_cast<u32>(h));
    m.w16(f.at(0x12), static_cast<u32>((0xF0 - h) >> 1));
    p = noise_rect(ot, p, f.at(0x10), rest * 2);
    const u32 l = m.r32(kLoadScreen);
    m.w32(l + 0x38, m.r32(l + 0x38) + 1);
    return p;
}

u32 Game::loading_text(u32 ot, u32 p) {
    cover(0x80036824u);
    Frame f(*this, 0x28);
    const u32 sc = m.r32(g::kScene);
    if (m.r16s(m.r32(kLoadScreen)) < 0x28) {
        const u32 s = m.r32(kLevelNames + 4 * (m.r32(sc + 0x394) & 3u));
        m.w32(f.at(0x10), 0x4000);
        m.w32(f.at(0x14), s);
        p = text_line(ot, p, 0xBE, 0x12, 0x4000, s);
    }
    const u32 l = m.r32(kLoadScreen);
    const u32 k = m.r8(l + 6);
    if (!((k == 1 && m.r16s(l + 2) > 0) || k == 2)) return p;
    const u32 s = m.r32(kMissionNames + 4u * static_cast<u32>(m.r16s(m.r32(g::kScene) + 0x38C)));
    u32 n = 0;
    while (m.r8(s + n)) ++n;
    const s32 x = static_cast<s32>(0x140u - 8u * n) >> 1;
    s32 v = m.r8(l + 6) == 1 ? m.r16s(l + 2) : 0x1000 - m.r16s(l + 2);
    if (v < 0) v += 31;
    const u32 color = (static_cast<u32>(v >> 5) & 0xFFu) << 8;
    m.w32(f.at(0x10), color);
    m.w32(f.at(0x14), s);
    p = text_line(ot, p, static_cast<u32>(x), 0x76, color, s);
    if (m.r32(m.r32(g::kScene) + 0x20) & 0x10u) {
        m.w32(f.at(0x10), color);
        m.w32(f.at(0x14), kCursor);
        p = text_line(ot, p, static_cast<u32>(x - 12), 0x76, color, kCursor);
    }
    return p;
}

u32 Game::loading_bars(u32 ot, u32 p, s32 level, u32 c0, u32 c1) {
    cover(0x800365FCu);
    Frame f(*this, 8);
    const s32 w = mul(level, 160) / 4096, h = mul(level, 120) / 4096;
    const s32 r = 0x140 - w, b = 0xF0 - h;
    const s32 xy[4][8] = {
        {0, 0, 0x140, 0, w, h, r, h},
        {0x140, 0xF0, 0, 0xF0, r, b, w, b},
        {0, 0xF0, 0, 0, w, b, w, h},
        {0x140, 0, 0x140, 0xF0, r, h, r, b},
    };
    for (u32 i = 0; i < 4; ++i) {
        const u32 q = p + 0x24 * i;
        m.w8(q + 3, 8);
        m.w8(q + 7, 0x38);
        m.w32(q + 4, c0 | 0x38000000u);
        m.w32(q + 0xC, c0);
        m.w32(q + 0x14, c1);
        m.w32(q + 0x1C, c1);
        for (u32 j = 0; j < 4; ++j) {
            m.w16(q + 8 + 8 * j, static_cast<u32>(xy[i][2 * j]));
            m.w16(q + 10 + 8 * j, static_cast<u32>(xy[i][2 * j + 1]));
        }
        ot_link(ot, q);
    }
    return p + 0x90;
}

u32 Game::noise_rect(u32 ot, u32 p, u32 rect, s32 level) {
    cover(0x800847A8u);
    Frame f(*this, 0x20);
    if (m.r16s(rect + 4) <= 0 || m.r16s(rect + 6) <= 0) return p;
    m.w32(p, (((p + 0x10) << 8) >> 8) | 0x03000000u);
    m.w32(p + 4, 0xE1000218u);
    m.w32(p + 8, 0xE20C4318u);
    m.w32(p + 0xC, 0);
    gte.mtc2(6, 0x64838383u);
    gte.mtc2(8, static_cast<u32>(0x1000 - level));
    const u32 q = noise_sprites(p + 0x10, 0x46D6, m.r32(rect), m.r32(rect + 4));
    m.w32(q + 4, 0xE1000220u);
    m.w32(q, 0x03000000u);
    m.w32(q + 8, 0xE2000000u);
    m.w32(q + 0xC, 0);
    ot_link(ot, q);
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (p & 0x00FFFFFFu));
    return q + 0x10;
}
}

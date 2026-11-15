#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kGaugeSets = 0x800BCFA8u;
constexpr u32 kGaugeCur = 0x800BCFACu;
constexpr u32 kRadarSets = 0x800BCFB8u;
constexpr u32 kRadarCur = 0x800BCFBCu;
constexpr u32 kWarnSets = 0x800BCFC4u;
constexpr u32 kWarnCur = 0x800BCFC8u;
constexpr u32 kLineSets = 0x800BCFE0u;
constexpr u32 kLineCur = 0x800BCFE4u;
constexpr u32 kShapes = 0x800BCC2Cu;
constexpr u32 kFrameXY = 0x800B108Cu;
constexpr u32 kFrameUV = 0x800B10ACu;
}

void Game::pkt(u32 p, u32 len, u32 code) {
    m.w8(p + 3, len);
    m.w8(p + 7, code);
}

void Game::pkt_rgb(u32 p, u32 r, u32 g, u32 b) {
    m.w8(p + 4, r);
    m.w8(p + 5, g);
    m.w8(p + 6, b);
}

u32 Game::set_draw_area(u32 p, u32 rect) {
    auto word = [](u32 cmd, s32 x, s32 y) {
        x = x < 0 ? 0 : (x > 1023 ? 1023 : x);
        y = y < 0 ? 0 : (y > 511 ? 511 : y);
        return (cmd << 24) | ((static_cast<u32>(y) & 0x3FFu) << 10) | (static_cast<u32>(x) & 0x3FFu);
    };
    const s32 x = m.r16s(rect), y = m.r16s(rect + 2), w = m.r16s(rect + 4), h = m.r16s(rect + 6);
    m.w32(p, (2u << 24) | (m.r32(p) & 0x00FFFFFFu));
    m.w32(p + 4, word(0xE3u, x, y));
    m.w32(p + 8, word(0xE4u, x + w - 1, y + h - 1));
    return p;
}

u32 Game::set_draw_tpage(u32 p, u32 dfe, u32 dtd, u32 tpage) {
    m.w32(p, (1u << 24) | (m.r32(p) & 0x00FFFFFFu));
    m.w32(p + 4, (0xE1u << 24) | (tpage & 0x9FFu) | ((dtd ? 1u : 0u) << 9) | ((dfe ? 1u : 0u) << 10));
    return p;
}

u32 Game::set_draw_mode(u32 p, u32 dfe, u32 dtd, u32 tpage, u32 tw) {
    m.w32(p, (2u << 24) | (m.r32(p) & 0x00FFFFFFu));
    m.w32(p + 4, (0xE1u << 24) | (tpage & 0x9FFu) | ((dtd ? 1u : 0u) << 9) | ((dfe ? 1u : 0u) << 10));
    u32 w = 0;
    if (tw) {
        const u32 x = m.r8(tw), y = m.r8(tw + 2);
        const s32 tww = m.r16s(tw + 4), twh = m.r16s(tw + 6);
        w = (0xE2u << 24) | ((y >> 3) << 15) | ((x >> 3) << 10) | (static_cast<u32>(-twh << 2) & 0x3E0u) |
            ((static_cast<u32>(-tww) & 0xFFu) >> 3);
    }
    m.w32(p + 8, w);
    return p;
}

u32 Game::set_draw_load(u32 p, u32 rect) {
    const s32 words = (static_cast<s32>(m.r16s(rect + 4)) * m.r16s(rect + 6) + 1) / 2;
    const u32 len = words < 13 ? static_cast<u32>(words + 4) : 0u;
    m.w32(p, (len << 24) | (m.r32(p) & 0x00FFFFFFu));
    m.w32(p + 4, 0xA0000000u);
    m.w32(p + 8, m.r32(rect));
    m.w32(p + 12, m.r32(rect + 4));
    m.w32(p + 4 * len, 0x01000000u);
    return p;
}

u32 Game::cat_prim(u32 a, u32 b) {
    m.w32(a, (m.r32(a) & 0xFF000000u) | (b & 0x00FFFFFFu));
    return a;
}

void Game::hud_gauge_templates() {
    cover(0x8003B924u);
    Frame f(*this, 0x30);
    m.w32(kGaugeSets, mem_alloc(0x1C));
    for (u32 k = 0; k < 2; ++k) {
        const u32 t = mem_alloc(0x1080);
        m.w32(kGaugeCur, t);
        m.w32(m.r32(kGaugeSets) + 4 * k, t);
        for (u32 i = 0; i < 64; ++i) {
            const u32 p = t + 0x18 + 20 * i;
            pkt(p, 4, 0x66);
            m.w16(p + 14, 0x7F98);
        }
        for (u32 i = 0; i < 64; ++i) pkt(t + 0x518 + 16 * i, 3, 0x62);
        for (u32 i = 0; i < 8; ++i) {
            const u32 p = t + 0x918 + 16 * i;
            pkt(p, 3, 0x42);
            pkt_rgb(p, 0, 0x80, 0);
        }
        for (u32 i = 0; i < 8; ++i) {
            const u32 p = t + 0x998 + 24 * i;
            pkt(p, 5, 0x4A);
            pkt_rgb(p, 0, 0x80, 0);
            m.w32(p + 20, 0x55555555u);
        }
        const u32 rect = f.at(0x10);
        m.w16(rect, 12);
        m.w16(rect + 2, k == 1 ? 270u : 14u);
        m.w16(rect + 4, 64);
        m.w16(rect + 6, 64);
        set_draw_area(m.r32(kGaugeCur), rect);
        m.w16(rect + 4, 320);
        m.w16(rect, 0);
        m.w16(rect + 2, k == 1 ? 256u : 0u);
        m.w16(rect + 6, 240);
        set_draw_area(m.r32(kGaugeCur) + 12, rect);
        const u32 t2 = m.r32(kGaugeCur);
        for (u32 i = 0; i < 2; ++i) pkt(t2 + 0xC54 + 16 * i, 3, 0x42);
        for (u32 i = 0; i < 3; ++i) pkt(t2 + 0xC74 + 20 * i, 4, 0x22);
        for (u32 i = 0; i < 7; ++i) {
            const u32 p = t2 + 0xCD4 + 40 * i;
            pkt(p, 9, 0x2E);
            pkt_rgb(p, 0x83, 0x83, 0x83);
            m.w16(p + 22, 5);
            m.w16(p + 14, 0x5714);
            const u32 x = m.r8(kShapes + i), y = m.r8(kShapes + 8 + i);
            const u32 w = m.r8(kShapes + 16 + i), h = m.r8(kShapes + 24 + i);
            m.w16(p + 8, x);
            m.w16(p + 10, y);
            m.w16(p + 16, x + w);
            m.w16(p + 18, y);
            m.w16(p + 24, x);
            m.w16(p + 26, y + h);
            m.w16(p + 32, x + w);
            m.w16(p + 34, y + h);
        }
        for (u32 i = 0; i < 4; ++i) {
            const u32 p = t2 + 0xDEC + 40 * i;
            pkt(p, 9, 0x2E);
            pkt_rgb(p, 0x83, 0x83, 0x83);
            m.w16(p + 22, 0x36);
            m.w16(p + 14, 0x7F99);
            const u32 x = m.r8(kShapes + 0x20 + i), y = m.r8(kShapes + 0x24 + i);
            m.w16(p + 8, x);
            m.w16(p + 10, y);
            m.w16(p + 16, x + 16);
            m.w16(p + 18, y);
            m.w16(p + 24, x);
            m.w16(p + 26, y + 16);
            m.w16(p + 32, x + 16);
            m.w16(p + 34, y + 16);
            const u32 q = t2 + 0xE8C + 16 * i;
            pkt(q, 3, 0x62);
            pkt_rgb(q, 0, 0, 0);
            m.w16(q + 8, x);
            m.w16(q + 12, 16);
            m.w16(q + 10, y);
            m.w16(q + 14, 16);
        }
        static constexpr u8 kMarkUV[4][8] = {
            {0x70, 0x37, 0x80, 0x37, 0x70, 0x47, 0x80, 0x47},
            {0x7F, 0x37, 0x6F, 0x37, 0x7F, 0x47, 0x6F, 0x47},
            {0x70, 0x46, 0x80, 0x46, 0x70, 0x36, 0x80, 0x36},
            {0x7F, 0x46, 0x6F, 0x46, 0x7F, 0x36, 0x6F, 0x36},
        };
        for (u32 i = 0; i < 4; ++i) {
            const u32 p = t2 + 0xDEC + 40 * i;
            for (u32 v = 0; v < 4; ++v) {
                m.w8(p + 12 + 8 * v, kMarkUV[i][2 * v]);
                m.w8(p + 13 + 8 * v, kMarkUV[i][2 * v + 1]);
            }
        }
        for (u32 i = 0; i < 4; ++i) {
            const u32 p = t2 + 0xECC + 24 * i;
            pkt(p, 5, 0x4A);
            pkt_rgb(p, 0, 0xFF, 0);
            m.w32(p + 20, 0x55555555u);
        }
        static constexpr u16 kBracket[][2] = {
            {0xED6, 0x58}, {0xEDA, 0x58}, {0xEEE, 0x58}, {0xEF2, 0x58}, {0xED8, 0x80}, {0xEDC, 0x80},
            {0xF08, 0x80}, {0xF0C, 0x80}, {0xED4, 0x88}, {0xEDE, 0x60}, {0xEEC, 0xB8}, {0xEF0, 0xC0},
            {0xEF4, 0xC0}, {0xEF6, 0x60}, {0xF04, 0x88}, {0xF06, 0x98}, {0xF0A, 0x98}, {0xF0E, 0x90},
            {0xF1C, 0xB8}, {0xF1E, 0x98}, {0xF20, 0xC0}, {0xF22, 0x98}, {0xF24, 0xC0}, {0xF26, 0x90},
        };
        for (const auto& [off, v] : kBracket) m.w16(t2 + off, v);
        for (u32 i = 0; i < 10; ++i) {
            const u32 p = t2 + 0xF2C + 16 * i;
            pkt(p, 3, 0x42);
            pkt_rgb(p, 0, 0xFF, 0);
        }
        static constexpr u16 kCross[][2] = {
            {0xF38, 0xA4}, {0xF48, 0xA4}, {0xF58, 0xA4}, {0xF68, 0x84}, {0xF34, 0x9C}, {0xF44, 0x9C},
            {0xF54, 0x9C}, {0xF74, 0xBC}, {0xF46, 0x78}, {0xF4A, 0x78}, {0xF66, 0x78}, {0xF6A, 0x78},
            {0xF76, 0x78}, {0xF7A, 0x78}, {0xF8A, 0x7C}, {0xF9A, 0x7C}, {0xFAA, 0x7C}, {0xF36, 0x58},
            {0xF3A, 0x58}, {0xF56, 0x98}, {0xF5A, 0x98}, {0xF64, 0x80}, {0xF78, 0xC0}, {0xF84, 0x80},
            {0xF86, 0x74}, {0xF88, 0x80}, {0xF94, 0xA0}, {0xF96, 0x74}, {0xF98, 0xA0}, {0xFA4, 0xC0},
            {0xFA6, 0x74}, {0xFA8, 0xC0}, {0xFB4, 0xA0}, {0xFBA, 0x5C}, {0xFB6, 0x58}, {0xFB8, 0xA0},
            {0xFC4, 0xA0}, {0xFC6, 0x98}, {0xFC8, 0xA0}, {0xFCA, 0x94},
        };
        for (const auto& [off, v] : kCross) m.w16(t2 + off, v);
        for (u32 i = 0; i < 2; ++i) {
            const u32 p = t2 + 0xFCC + 24 * i;
            pkt(p, 5, 0x2A);
            pkt_rgb(p, 0x10, 0x20, 0x10);
        }
        {
            const u32 p = t2 + 0xFFC;
            pkt(p, 4, 0x66);
            pkt_rgb(p, 0x83, 0x83, 0x83);
            m.w16(p + 14, 0x7F98);
            m.w16(p + 8, 0x9C);
            m.w16(p + 10, 0x74);
            m.w16(p + 16, 8);
            m.w16(p + 18, 9);
        }
        for (u32 i = 0; i < 4; ++i) {
            const u32 p = t2 + 0x1010 + 20 * i;
            pkt(p, 4, 0x66);
            pkt_rgb(p, 0, 0x83, 0);
            m.w16(p + 14, 0x7F98);
            m.w16(p + 8, m.r16(kShapes + 0x28 + 2 * i));
            m.w16(p + 10, m.r16(kShapes + 0x30 + 2 * i));
            m.w8(p + 12, m.r8(kShapes + 0x38 + i));
            m.w8(p + 13, 0x67);
            m.w16(p + 16, 12);
            m.w16(p + 18, 7);
        }
        m.w8(t2 + 0x1063, 1);
        m.w32(t2 + 0x1064, 0xE1000205u);
        m.w8(t2 + 0x106B, 1);
        m.w32(t2 + 0x106C, 0xE1000236u);
        m.w8(t2 + 0x1073, 1);
        m.w32(t2 + 0x1074, 0xE1000236u);
        m.w8(t2 + 0x107B, 1);
        m.w32(t2 + 0x107C, 0xE1000236u);
        m.w32(t2 + 0xCB4, 0xE6000000u);
        m.w32(t2 + 0xCB8, 0);
        m.w32(t2 + 0xCC0, 0xE6000002u);
        m.w32(t2 + 0xCC4, 0);
        m.w32(t2 + 0xCCC, 0xE6000000u);
        m.w32(t2 + 0xCD0, 0);
    }
    hud_frame_templates();
}

void Game::hud_frame_templates() {
    cover(0x8003C250u);
    for (u32 k = 0; k < 2; ++k) {
        const u32 t = m.r32(m.r32(kGaugeSets) + 4 * k);
        m.w32(kGaugeCur, t);
        {
            const u32 p = t + 0xAA4;
            pkt(p, 9, 0x2E);
            pkt_rgb(p, 0x63, 0x63, 0x63);
            m.w16(p + 22, 5);
            m.w16(p + 14, 0x5714);
        }
        for (u32 i = 0; i < 8; ++i) {
            const u32 j = i & 3;
            const u32 p = t + 0xACC + 40 * i;
            const u32 pos = kFrameXY + 8 * j;
            pkt(p, 9, 0x2E);
            m.w16(p + 22, 0x36);
            pkt_rgb(p, 0x80, 0x80, 0x80);
            m.w16(p + 14, 0x7F99);
            const u32 x = m.r16(pos), y = m.r16(pos + 4);
            m.w16(p + 8, x);
            m.w16(p + 10, y);
            m.w16(p + 16, x + 32);
            m.w16(p + 18, y);
            m.w16(p + 24, x);
            m.w16(p + 26, y + 32);
            m.w16(p + 32, x + 32);
            m.w16(p + 34, y + 32);
            const u32 us = kFrameUV + 4 * j, vs = us + 16;
            for (u32 v = 0; v < 4; ++v) {
                m.w8(p + 12 + 8 * v, m.r8(us + v));
                m.w8(p + 13 + 8 * v, m.r8(vs + v));
            }
        }
        for (u32 i = 0; i < 3; ++i) {
            cat_prim(t + 0xACC + 40 * i, t + 0xAF4 + 40 * i);
            cat_prim(t + 0xB6C + 40 * i, t + 0xB94 + 40 * i);
        }
        {
            const u32 p = t + 0xC0C;
            m.w16(p + 14, 0x7F18);
            m.w8(p + 13, 0x37);
            m.w16(p + 16, 0x50);
            m.w16(p + 18, 0x49);
            pkt(p, 4, 0x66);
            m.w8(p + 12, 0);
            m.w16(p + 8, 6);
            m.w16(p + 10, 0xC);
        }
        {
            const u32 p = t + 0xC20;
            m.w16(p + 14, 0x7F98);
            m.w8(p + 13, 0x2D);
            pkt(p, 4, 0x66);
            m.w8(p + 12, 0);
            m.w16(p + 18, 0xA);
            m.w16(p + 8, 0xA);
            m.w16(p + 16, 0xC);
            m.w16(p + 10, 0xC);
        }
        m.w8(t + 0xA97, 1);
        m.w8(t + 0xA9F, 1);
        m.w32(t + 0xA98, 0xE1000236u);
        m.w32(t + 0xAA0, 0xE1000236u);
        for (u32 i = 0; i < 2; ++i) {
            const u32 p = t + 0xC34 + 16 * i;
            pkt(p, 3, 0x62);
            pkt_rgb(p, 0, 0, 0);
            m.w16(p + 8, m.r16(kFrameXY));
            m.w16(p + 12, 0x40);
            m.w16(p + 14, 0x40);
            m.w16(p + 10, m.r16(kFrameXY + 4));
        }
        for (u32 i = 0; i < 5; ++i) {
            m.w32(t + 0xA5C + 12 * i, (i & 1) ? 0xE6000002u : 0xE6000000u);
            m.w32(t + 0xA60 + 12 * i, 0);
        }
    }
}

void Game::hud_radar_templates() {
    cover(0x8003ECBCu);
    Frame f(*this, 0x28);
    m.w32(kRadarSets, mem_alloc(0x18));
    for (u32 k = 0; k < 2; ++k) {
        const u32 t = mem_alloc(0x1558);
        m.w32(kRadarCur, t);
        m.w32(m.r32(kRadarSets) + 4 * k, t);
        for (u32 i = 0; i < 64; ++i) {
            const u32 p = t + 8 + 16 * i;
            pkt(p, 3, 0x42);
            pkt_rgb(p, 0, 0xFF, 0);
        }
        for (u32 i = 0; i < 128; ++i) {
            const u32 p = t + 0x408 + 24 * i;
            pkt(p, 5, 0x4A);
            pkt_rgb(p, 0, 0xFF, 0);
            m.w32(p + 20, 0x55555555u);
        }
        for (u32 i = 0; i < 64; ++i) {
            const u32 p = t + 0x1008 + 20 * i;
            pkt(p, 4, 0x66);
            m.w16(p + 14, 0x7F9B);
        }
        for (u32 i = 0; i < 4; ++i) {
            const u32 p = t + 0x1508 + 20 * i;
            pkt(p, 4, 0x66);
            m.w16(p + 14, 0x7F98);
        }
        m.w8(t + 3, 1);
        m.w32(t + 4, 0xE1000636u);
    }
    radar_blips_clear();
    radar_center_reset();
}

void Game::radar_blips_clear() {
    cover(0x8003F284u);
    for (u32 i = 0; i < 64; ++i) {
        const u32 p = m.r32(0x800C67A0u + 4 * i);
        if (!p) continue;
        m.w8(p, 0);
        m.w8(p + 1, 0);
        m.w16(p + 6, 0);
        m.w16(p + 8, 0);
        m.w16(p + 10, 0);
    }
}

void Game::radar_center_reset() {
    cover(0x800409ACu);
    m.w16(0x800BCECCu, 0xA0);
    m.w16(0x800BCECEu, 0x78);
    m.w8(0x800BCEC8u, 0);
    m.w8(0x800BCEC9u, 0);
    m.w16(0x800BCECAu, 0x40);
}

void Game::hud_warning_templates() {
    cover(0x80041120u);
    Frame f(*this, 0x28);
    m.w32(kWarnSets, mem_alloc(0x10));
    for (u32 k = 0; k < 2; ++k) {
        const u32 t = mem_alloc(0x278);
        m.w32(kWarnCur, t);
        m.w32(m.r32(kWarnSets) + 4 * k, t);
        for (u32 i = 0; i < 16; ++i) pkt(t + 0x68 + 20 * i, 4, 0x66);
        for (u32 i = 0; i < 3; ++i) {
            const u32 p = t + 20 * i;
            pkt(p, 4, 0x66);
            pkt_rgb(p, 0, 0x83, 0);
            m.w16(p + 14, 0x7F98);
        }
        m.w8(t + 0xC, 0xAA);
        m.w8(t + 0xD, 0);
        m.w16(t + 0x10, 0x31);
        m.w16(t + 0x12, 0xF);
        m.w8(t + 0x14 + 0xC, 0xCC);
        m.w8(t + 0x14 + 0xD, 0x10);
        m.w16(t + 0x14 + 0x10, 0xE);
        m.w16(t + 0x14 + 0x12, 0x28);
        for (u32 i = 0; i < 3; ++i) pkt(t + 0x28 + 16 * i, 3, 0x62);
        m.w8(t + 0x5B, 1);
        m.w8(t + 0x63, 1);
        m.w32(t + 0x5C, 0xE1000636u);
        m.w32(t + 0x64, 0xE1000616u);
        for (u32 i = 0; i < 2; ++i) {
            const u32 p = t + 0x1E8 + 40 * i;
            pkt(p, 9, 0x2E);
            pkt_rgb(p, 0x83, 0x83, 0x83);
        }
        m.w16(t + 0x1FE, 0x25);
        {
            const u32 p = t + 0x210;
            m.w16(p + 22, 0x36);
            m.w8(p + 13, 0x57);
            m.w8(p + 21, 0x57);
            m.w8(p + 12, 0x50);
            m.w8(p + 20, 0x7B);
            m.w8(p + 28, 0x50);
            m.w8(p + 29, 0x80);
            m.w8(p + 36, 0x7B);
            m.w8(p + 37, 0x80);
            m.w16(p + 14, 0x7F98);
        }
        for (u32 i = 0; i < 3; ++i) {
            const u32 p = t + 0x238 + 16 * i;
            pkt(p, 3, 0x62);
            pkt_rgb(p, 0, 0, 0);
        }
        m.w8(t + 0x26B, 1);
        m.w8(t + 0x273, 1);
        m.w32(t + 0x26C, 0xE1000616u);
        m.w32(t + 0x274, 0xE1000616u);
    }
    radio_hold_reset();
    subtitle_reset();
    warning_reset();
    prompt_reset();
}

void Game::hud_ladder_templates() {
    cover(0x80042938u);
    Frame f(*this, 0x28);
    m.w32(kLineSets, mem_alloc(0x14));
    for (u32 k = 0; k < 2; ++k) {
        const u32 t = mem_alloc(0x1450);
        m.w32(kLineCur, t);
        m.w32(m.r32(kLineSets) + 4 * k, t);
        for (u32 i = 0; i < 128; ++i) {
            const u32 p = t + 24 * i;
            pkt(p, 5, 0x4A);
            pkt_rgb(p, 0, 0xFF, 0);
            m.w32(p + 20, 0x55555555u);
        }
        for (u32 i = 0; i < 128; ++i) pkt(t + 0xC00 + 16 * i, 3, 0x42);
        for (u32 i = 0; i < 4; ++i) {
            const u32 p = t + 0x1400 + 20 * i;
            pkt(p, 4, 0x22);
            pkt_rgb(p, 0, 0xFF, 0);
        }
    }
}

void Game::sprt(u32 p, u32 x, u32 y, u32 u, u32 v, u32 w, u32 h, u32 clut) {
    pkt(p, 4, 0x66);
    m.w16(p + 8, x);
    m.w16(p + 10, y);
    m.w8(p + 12, u);
    m.w8(p + 13, v);
    m.w16(p + 14, clut);
    m.w16(p + 16, w);
    m.w16(p + 18, h);
}

void Game::line_f2(u32 p, u32 x0, u32 y0, u32 x1, u32 y1) {
    pkt(p, 3, 0x42);
    m.w16(p + 8, x0);
    m.w16(p + 10, y0);
    m.w16(p + 12, x1);
    m.w16(p + 14, y1);
}

void Game::gauge_templates(u32 b) {
    cover(0x8005E10Cu);
    Frame f(*this, 0x18);
    gauge_speed(b + 8);
    gauge_altitude(b + 0x9C);
    gauge_heading(b + 0x1B8);
    gauge_throttle(b + 0x30C);
    gauge_weapons(b + 0x244);
    gauge_damage(b + 0x384);
    gauge_timer(b + 0x3DC);
    set_draw_tpage(b, 0, 1, 0x36);
}

void Game::gauge_speed(u32 a) {
    cover(0x8005C5B0u);
    for (u32 i = 0; i < 2; ++i) pkt(a + 0x64 + 24 * i, 5, 0x2A);
    for (u32 i = 0; i < 5; ++i) {
        pkt(a + 20 * i, 4, 0x66);
        m.w16(a + 20 * i + 14, 0x7F98);
    }
    m.w8(a + 0xD, 0x19);
    m.w16(a + 0x10, 0x44);
    m.w16(a + 0x12, 0x14);
    m.w16(a + 8, 0xA);
    m.w16(a + 0xA, 0x80);
    m.w8(a + 0xC, 0);
    cat_prim(a + 0x64, a + 0x7C);
    cat_prim(a + 0x7C, a);
}

void Game::gauge_altitude(u32 a) {
    cover(0x8005C990u);
    Frame f(*this, 0x30);
    sprt(a, 0xEF, 0x60, 0x82, 0xF, 0x36, 0x10, 0x7F98);
    cat_prim(a, a + 0x14);
    sprt(a + 0x14, 0x125, 0x60, 0xB8, 0xF, 0x13, 0x29, 0x7F98);
    for (u32 i = 0; i < 5; ++i) {
        const u32 p = a + 0x28 + 20 * i;
        pkt(p, 4, 0x66);
        m.w16(p + 14, 0x7F98);
        m.w8(p + 12, 0x24);
        m.w8(p + 13, 0x2D);
        m.w16(p + 16, 6);
        m.w16(p + 18, 0xA);
        m.w16(p + 8, 0x12E);
        if (i < 4) cat_prim(p, p + 0x14);
    }
    for (u32 i = 0; i < 6; ++i) {
        pkt(a + 0x8C + 20 * i, 4, 0x66);
        m.w16(a + 0x8C + 20 * i + 14, 0x7F98);
    }
}

void Game::gauge_heading(u32 a) {
    cover(0x8005CEDCu);
    sprt(a, 0xC, 0xCE, 0x44, 0x19, 0x3C, 0xE, 0x7F98);
    sprt(a + 0x14, 0xE, 0xD2, 0x32, 0x31, 5, 6, 0x7F98);
    for (u32 i = 0; i < 5; ++i) {
        pkt(a + 0x28 + 20 * i, 4, 0x66);
        m.w16(a + 0x28 + 20 * i + 14, 0x7F98);
    }
    for (u32 i = 0; i < 6; ++i) cat_prim(a + 20 * i, a + 20 * (i + 1));
}

void Game::gauge_throttle(u32 a) {
    cover(0x8005D344u);
    sprt(a, 0xEE, 0xD2, 0xDC, 0, 0x24, 0x11, 0x7F98);
    cat_prim(a, a + 0x14);
    sprt(a + 0x14, 0x112, 0xB6, 0xD8, 0xA0, 0x25, 0x30, 0x7E18);
    sprt(a + 0x28, 0x107, 0xAC, 0xD8, 0xD0, 0x28, 0x28, 0x7F5B);
    for (u32 i = 0; i < 3; ++i) {
        pkt(a + 0x3C + 20 * i, 4, 0x66);
        m.w16(a + 0x3C + 20 * i + 14, 0x7F98);
    }
}

void Game::gauge_weapons(u32 a) {
    cover(0x8005D844u);
    sprt(a, 0xEE, 0xBE, 0x9E, 0x21, 0x19, 0x14, 0x7F98);
    for (u32 i = 0; i < 4; ++i) sprt(a + 0x14 + 20 * i, 0xF2 + 5 * i, 0xB6, 0x38, 0x30, 5, 7, 0x7F98);
    for (u32 i = 0; i < 2; ++i) sprt(a + 0x64 + 20 * i, 0xFC + 5 * i, 0xAD, 0x38, 0x30, 5, 7, 0x7F98);
    for (u32 i = 0; i < 3; ++i) {
        pkt(a + 0x8C + 20 * i, 4, 0x66);
        m.w16(a + 0x8C + 20 * i + 14, 0x7F98);
    }
}

void Game::gauge_damage(u32 a) {
    cover(0x8005DCA8u);
    Frame f(*this, 0x20);
    sprt(a, 0xE6, 0xA4, 0xDA, 0x11, 0x26, 0x27, 0x7F18);
    sprt(a + 0x14, 0xDF, 0xDB, 0x2A, 0x2F, 8, 8, 0x7F18);
    line_f2(a + 0x28, 0, 0xE2, 0xDE, 0xE2);
    line_f2(a + 0x38, 0xE6, 0xDA, 0xE6, 0xCB);
    line_f2(a + 0x48, 0x10C, 0xA4, 0x140, 0xA4);
    cat_prim(a, a + 0x14);
    cat_prim(a + 0x14, a + 0x28);
    cat_prim(a + 0x28, a + 0x38);
    cat_prim(a + 0x38, a + 0x48);
}

void Game::gauge_timer(u32 a) {
    cover(0x8005DEFCu);
    Frame f(*this, 0x20);
    line_f2(a + 0x28, 0, 0x2D, 4, 0x2D);
    line_f2(a + 0x38, 0x55, 0xE, 0x140, 0xE);
    sprt(a, 5, 0xC, 0, 0x37, 0x50, 0x49, 0x7F98);
    sprt(a + 0x14, 9, 0xC, 0x18, 0x2D, 0xC, 0xA, 0x7F98);
    cat_prim(a + 0x28, a + 0x38);
    cat_prim(a + 0x38, a);
    cat_prim(a, a + 0x14);
}

void Game::gauge_blocks() {
    cover(0x8005E2C0u);
    Frame f(*this, 0x18);
    m.w32(0x800BD060u, mem_alloc(8));
    for (u32 k = 0; k < 2; ++k) {
        const u32 b = mem_alloc(0x424);
        m.w32(m.r32(0x800BD060u) + 4 * k, b);
        gauge_templates(b);
    }
}

void Game::target_box_templates() {
    cover(0x80063AF0u);
    Frame f(*this, 0x28);
    m.w32(0x800BD074u, mem_alloc(0x28));
    for (u32 k = 0; k < 2; ++k) {
        const u32 t = mem_alloc(0x9D4);
        m.w32(0x800BD078u, t);
        m.w32(m.r32(0x800BD074u) + 4 * k, t);
        for (u32 i = 0; i < 16; ++i) pkt(t + 8 + 24 * i, 5, 0x2A);
        for (u32 i = 0; i < 4; ++i) pkt(t + 0x188 + 28 * i, 6, 0x32);
        for (u32 i = 0; i < 12; ++i) pkt(t + 0x1F8 + 40 * i, 9, 0x2E);
        for (u32 i = 0; i < 64; ++i) pkt(t + 0x3D8 + 16 * i, 3, 0x42);
        for (u32 i = 0; i < 8; ++i) pkt(t + 0x7D8 + 20 * i, 4, 0x52);
        {
            const u32 p = t + 0x878;
            pkt(p, 9, 0x5E);
            m.w8(p + 0x17, 0);
            m.w32(p + 0x24, 0x55555555u);
            m.w8(p + 0x1F, 0);
        }
        for (u32 i = 0; i < 4; ++i) {
            const u32 p = t + 0x954 + 32 * i;
            pkt(p, 7, 0x26);
            pkt_rgb(p, 0x43, 0x13, 0);
            m.w16(p + 22, 0x36);
            m.w16(p + 14, 0x7F98);
            m.w8(p + 12, 0x8A);
            m.w8(p + 13, 0x21);
            m.w8(p + 20, 0x9D);
            m.w8(p + 21, 0x21);
            m.w8(p + 28, 0x9D);
            m.w8(p + 29, 0x35);
            if (i != 3) cat_prim(p, p + 0x20);
        }
        for (u32 i = 0; i < 8; ++i) {
            pkt(t + 0x8A0 + 20 * i, 4, 0x66);
            m.w16(t + 0x8A0 + 20 * i + 14, 0x7F98);
        }
        pkt(t + 0x940, 4, 0x66);
        m.w8(t + 0x94C, 0xEC);
        m.w8(t + 0x94D, 0x8C);
        m.w16(t + 0x952, 0x14);
        m.w16(t + 0x950, 0x14);
        set_draw_tpage(m.r32(0x800BD078u), 0, 1, 0x36);
    }
}
}

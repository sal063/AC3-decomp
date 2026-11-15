#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kBriefing = 0x80112DB0u, kMenu = 0x80112DC4u;
constexpr u32 kState = 134, kTimer = 16, kLayout = 40;
s32 narrow(s32 value) { return static_cast<s16>(value); }
}

void Game::o2_brief_lines() {
    cover(0x800EE9E4u);
    Frame frame(*this, 112);
    const s32 state = m.r16s(m.r32(kMenu) + kState);
    if ((state >= 0 && state < 3) || state == 15) return;
    auto settled = [&]() {
        const u32 phase = m.r16(m.r32(kMenu) + kState);
        return phase - 5u < 9u;
    };
    auto timer = [&]() { return m.r32s(m.r32(kBriefing) + kTimer); };
    auto tile = [&](s32 x, s32 y, s32 width) {
        m.w32(frame.at(16), 0);
        o2_menu_tile(static_cast<u32>(x), static_cast<u32>(y), static_cast<u32>(width), 2, 0);
    };
    auto line = [&](s32 x, s32 y0, s32 y1) {
        o2_menu_line(static_cast<u32>(x), static_cast<u32>(y0), static_cast<u32>(x), static_cast<u32>(y1));
    };
    if (!settled() && timer() < 23) {
        const s32 width = 160 * timer() / 23;
        tile(160 - width, 240, width);
        tile(160, 240, 160 * timer() / 23);
        line(160, 240 - 240 * timer() / 23, 240);
        line(160, 240, 240 + 240 * timer() / 23);
    } else {
        tile(0, 240, 160); tile(160, 240, 160);
        line(160, 0, 240); line(160, 240, 480);
    }
    if (m.r32(m.r32(kBriefing) + kLayout) == 1) {
        if (!settled() && timer() < 11) {
            const s32 width = narrow(120 * timer() / 11), height = narrow(160 * timer() / 11);
            tile(140 - width, 220, width); tile(180, 220, width);
            tile(140 - width, 260, width); tile(180, 260, width);
            line(140, 220 - height, 220); line(180, 220 - height, 220);
            line(140, 260, 260 + height); line(180, 260, 260 + height);
        } else if (!settled() && timer() < 23) {
            const s32 height = narrow(160 * (timer() - 11) / 12);
            const s32 width = narrow(10 * (timer() - 11));
            tile(20, 220, 120); tile(180, 220, 120);
            tile(20, 260, 120); tile(180, 260, 120);
            line(140, 60, 220); line(180, 60, 220);
            line(140, 260, 420); line(180, 260, 420);
            tile(140 - width, 60, width); tile(180, 60, width);
            tile(140 - width, 420, width); tile(180, 420, width);
            line(20, 220 - height, 220); line(300, 220 - height, 220);
            line(20, 260, 260 + height); line(300, 260, 260 + height);
        } else {
            tile(20, 220, 120); tile(180, 220, 120);
            tile(20, 260, 120); tile(180, 260, 120);
            line(140, 60, 220); line(180, 60, 220);
            line(140, 260, 420); line(180, 260, 420);
            tile(20, 60, 120); tile(180, 60, 120);
            tile(20, 420, 120); tile(180, 420, 120);
            line(20, 60, 220); line(300, 60, 220);
            line(20, 260, 420); line(300, 260, 420);
        }
    }
    if (m.r32(m.r32(kBriefing) + kLayout) != 0) return;
    if (!settled() && timer() < 7) {
        const s32 width = 80 * timer() / 7;
        tile(160 - width, 290, width); tile(160, 290, 80 * timer() / 7);
    } else { tile(80, 290, 80); tile(160, 290, 80); }
    if (settled() || timer() >= 30) {
        tile(80, 73, 80); tile(160, 73, 80);
    } else if (timer() >= 23) {
        tile(80, 73, 80 * (timer() - 23) / 7);
        const s32 width = 80 * (timer() - 23) / 7;
        tile(240 - width, 73, width);
    }
    const s32 count = settled() ? 16 : timer() < 7 ? 0 : timer() < 23 ? timer() - 7 : 16;
    m.w32(frame.at(24), static_cast<u32>(count));
    m.w16(frame.at(40), 80); m.w16(frame.at(56), 80);
    m.w16(frame.at(64), 291);
    m.w16(frame.at(32), 240); m.w16(frame.at(48), 240);
    m.w32(frame.at(28), 1);
    s32 lowerY = 290;
    for (s32 segment = 1; segment <= count; ++segment) {
        const s32 angle = 3072 - 128 * segment;
        const s32 radiusX = 54 * rcos(angle) / 4096;
        const s32 radiusY = 108 * rsin(angle) / 4096;
        const s32 x = narrow(radiusX), y = 182 - narrow(radiusY);
        o2_menu_line(static_cast<u32>(m.r16s(frame.at(40))), static_cast<u32>(narrow(lowerY)),
                     static_cast<u32>(80 + x), static_cast<u32>(y));
        o2_menu_line(static_cast<u32>(m.r16s(frame.at(32))), static_cast<u32>(narrow(lowerY)),
                     static_cast<u32>(240 - x), static_cast<u32>(y));
        m.w16(frame.at(40), static_cast<u32>(80 + radiusX));
        m.w16(frame.at(32), static_cast<u32>(240 - radiusX));
        lowerY = 182 - radiusY;
        const s32 upperRadius = 109 * rsin(angle) / 4096;
        const s32 upperY = 182 - narrow(upperRadius);
        const s32 previousY = m.r16s(frame.at(64));
        o2_menu_line(static_cast<u32>(m.r16s(frame.at(56))), static_cast<u32>(previousY),
                     static_cast<u32>(80 + x), static_cast<u32>(upperY));
        o2_menu_line(static_cast<u32>(m.r16s(frame.at(48))), static_cast<u32>(previousY),
                     static_cast<u32>(240 - x), static_cast<u32>(upperY));
        m.w16(frame.at(56), static_cast<u32>(80 + radiusX));
        m.w16(frame.at(48), static_cast<u32>(240 - radiusX));
        m.w16(frame.at(64), static_cast<u32>(182 - upperRadius));
        m.w32(frame.at(28), static_cast<u32>(segment + 1));
    }
}
}

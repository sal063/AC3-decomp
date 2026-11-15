#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kNoise = 0x800C78F0u, kShapes = 0x800C7990u;
constexpr u32 kBright = 0x3AFFFFFFu, kDark = 0x00030303u, kCapColor = 0x3A030303u;
u32 xy(s32 x, s32 y) { return (static_cast<u32>(y) << 16) | static_cast<u32>(x); }
}

u32 Game::movie_draw_border(u32 packet, u32 head) {
    cover(0x8005A2C4u);
    Frame frame(*this, 0x178);
    const u32 patterns = frame.at(0x10), widths = frame.at(0xA8);
    for (u32 offset = 0; offset < 152; offset += 4)
        m.w32(patterns + offset, m.r32(0x80024420u + offset));
    if (m.r32s(m.r32(g::kScene) + 36) % 80 == 79) {
        for (u32 edge = 0; edge < 4; ++edge) {
            m.w32(frame.at(0x14C), 4 * edge);
            for (u32 point = 0; point < 38; ++point) {
                m.w8(kNoise + 38 * edge + point, static_cast<u32>(8 * rand() / 32767));
                m.w32(kShapes + 4 * edge, static_cast<u32>(4 * rand() / 32767));
            }
        }
    }
    const s32 phase = (m.r32s(m.r32(g::kScene) + 36) % 80) / 8;
    for (u32 edge = 0; edge < 4; ++edge) {
        for (u32 point = 0; point < 38; ++point) {
            const u32 index = 38 * edge + point;
            const u32 shape = m.r32(kShapes + 4 * edge);
            const s32 amplitude = m.r8(patterns + 38 * shape + point) + m.r8(kNoise + index);
            const s32 width = phase < 5 ? phase * amplitude / 5 + 16 :
                amplitude + 16 - (phase - 5) * amplitude / 5;
            m.w8(widths + index, static_cast<u32>(width));
        }
    }
    auto quad = [&](u32 edgeTop, u32 edgeBottom, u32 innerTop, u32 innerBottom, bool cap) {
        m.w32(packet + 8, edgeTop);
        m.w32(packet + 24, edgeBottom);
        m.w32(packet + 4, kBright);
        m.w32(packet + 12, cap ? kCapColor : kDark);
        m.w32(packet + 20, cap ? kBright : 0xFFFFFFu);
        m.w32(packet + 28, cap ? kCapColor : kDark);
        m.w32(packet + 16, innerTop);
        m.w32(packet + 32, innerBottom);
        m.w32(packet, m.r32(head) | 0x08000000u);
        m.w32(head, packet & 0xFFFFFFu);
        packet += 36;
    };
    for (u32 edge = 0; edge < 4; ++edge) {
        const u32 base = widths + 38 * edge;
        if (edge < 2) {
            const s32 boundary = edge ? 288 : 32;
            auto inside = [&](s32 width) { return edge ? boundary - width : boundary + width; };
            for (u32 point = 0; point < 36; ++point) {
                const s32 y = 100 + 8 * static_cast<s32>(point);
                quad(xy(boundary, y), xy(boundary, y + 8),
                    xy(inside(m.r8(base + point)), y), xy(inside(m.r8(base + point + 1)), y + 8), false);
            }
            for (s32 point = 0; point < 8; ++point) {
                auto curve = [](s32 t) { return t * t / 2 - 8 * t + 32; };
                const s32 y = 100 + 4 * point;
                quad(xy(boundary, y), xy(boundary, y + 4), xy(inside(curve(point)), y),
                    xy(inside(curve(point + 1)), y + 4), true);
            }
            for (s32 point = 0; point < 8; ++point) {
                const s32 y = 348 + 4 * point;
                quad(xy(boundary, y), xy(boundary, y + 4), xy(inside(point * point / 2), y),
                    xy(inside((point + 1) * (point + 1) / 2), y + 4), true);
            }
        } else {
            const s32 boundary = edge == 2 ? 100 : 380;
            auto inside = [&](s32 width) { return edge == 2 ? boundary + width : boundary - width; };
            for (u32 point = 0; point < 33; ++point) {
                const s32 x = 32 + 8 * static_cast<s32>(point);
                quad(xy(x, boundary), xy(x + 8, boundary), xy(x, inside(m.r8(base + point))),
                    xy(x + 8, inside(m.r8(base + point + 1))), false);
            }
        }
    }
    m.w8(packet + 3, 1);
    m.w32(packet + 4, 0xE1000240u);
    m.w32(packet, (m.r32(packet) & 0xFF000000u) | (m.r32(head) & 0xFFFFFFu));
    m.w32(head, (m.r32(head) & 0xFF000000u) | (packet & 0xFFFFFFu));
    return packet + 8;
}
}

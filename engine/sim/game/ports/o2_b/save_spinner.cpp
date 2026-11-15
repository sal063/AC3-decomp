#include "sim/game/game.hpp"

namespace ac3::sim {
u32 Game::o2_save_spinner(u32 x, u32 y, u32 ot, u32 angle, u32 packet) {
    cover(0x800E6DFCu);
    Frame frame(*this, 56);
    for (u32 layer = 0; layer < 2; ++layer) {
        const u32 quad = packet + 40 * layer;
        m.w8(quad + 3, 9); m.w8(quad + 7, 0x2C);
        m.w16(quad + 22, 31); m.w16(quad + 14, 0x298);
        for (u32 channel = 4; channel <= 6; ++channel) m.w8(quad + channel, 128);
        const u32 u = layer ? 0u : 16u, width = layer ? 13u : 7u, v = layer ? 228u : 240u;
        m.w8(quad + 13, v); m.w8(quad + 21, v);
        m.w8(quad + 12, u); m.w8(quad + 20, u + width);
        m.w8(quad + 28, u); m.w8(quad + 29, 255);
        m.w8(quad + 36, u + width); m.w8(quad + 37, 255);
        const s32 rotation = static_cast<s32>(layer ? 0u - angle : angle), radius = layer ? 15 : 8;
        const s32 cosine = rcos(rotation), sine = rsin(rotation);
        const s32 cx = cosine * radius / 4096, sy = sine * radius / 4096;
        const s32 coordinates[4][2] = {
            {(-cx - sy) / 2, sy - cx}, {(cx - sy) / 2, -sy - cx},
            {(-cx + sy) / 2, sy + cx}, {(cx + sy) / 2, -sy + cx}
        };
        for (u32 vertex = 0; vertex < 4; ++vertex) {
            m.w16(quad + 8 + 8 * vertex, x + static_cast<u32>(coordinates[vertex][0]));
            m.w16(quad + 10 + 8 * vertex, y + static_cast<u32>(coordinates[vertex][1]));
        }
        m.w32(quad, (m.r32(quad) & 0xFF000000u) | (m.r32(ot) & 0xFFFFFFu));
        m.w32(ot, (m.r32(ot) & 0xFF000000u) | (quad & 0xFFFFFFu));
    }
    return packet + 80;
}
}

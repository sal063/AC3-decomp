#include "sim/game/game.hpp"
#include <array>

namespace ac3::sim {
namespace {
constexpr u32 kDepths = 0x294;
constexpr u32 kIndexWord = 4;
constexpr u32 kColours = 8;
}

u32 Game::direct_model_quads(u32 list, u32 vertices, u32 packet, u32 orderTable,
    u32 depthShift, bool gouraud) {
    const s32 count = m.r32s(list);
    const u32 recordBytes = gouraud ? 36u : 24u;
    for (s32 index = 0; index < count; ++index) {
        const u32 face = list + 4 + recordBytes * static_cast<u32>(index);
        const u32 packed = m.r32(face + kIndexWord);
        const std::array<u32, 4> corner = {packed & 255u, (packed >> 8) & 255u,
            (packed >> 16) & 255u, packed >> 24};
        const u32 firstScreen = m.r32(vertices + 4 * corner[0]);
        gte.mtc2(13, m.r32(vertices + 4 * corner[1]));
        gte.mtc2(14, m.r32(vertices + 4 * corner[2]));
        gte.mtc2(12, firstScreen);
        const u32 fourthScreen = m.r32(vertices + 4 * corner[3]);
        gte.op(0x1400006u);
        const s32 area = static_cast<s32>(gte.mfc2(24));
        gte.mtc2(12, fourthScreen);
        if (area <= 0) {
            gte.op(0x1400006u);
            if (static_cast<s32>(gte.mfc2(24)) >= 0) continue;
        }
        s32 sum = 0;
        for (u32 vertex : corner) sum += m.r16s(vertices + kDepths + 2 * vertex);
        const s32 depth = (sum >> 2) >> (depthShift & 31u);
        const u32 minimum = gouraud ? 31u : 16u;
        if (static_cast<u32>(depth) - minimum >= 30712u - minimum) continue;
        const u32 bucket = orderTable + 4u * static_cast<u32>(depth >> 3);
        m.w32(packet, m.r32(bucket) | (gouraud ? 0x0C000000u : 0x09000000u));
        m.w32(bucket, packet & 0xFFFFFFu);
        m.w32(packet + 8, firstScreen);
        m.w32(packet + 4, m.r32(face + kColours));
        if (gouraud) {
            m.w32(packet + 12, m.r32(face + 24));
            m.w32(packet + 16, m.r32(face + 12));
            m.w32(packet + 20, gte.mfc2(13));
            m.w32(packet + 24, m.r32(face + 28));
            m.w32(packet + 28, m.r32(face + 16));
            m.w32(packet + 32, gte.mfc2(14));
            const u32 lastTextures = m.r32(face + 32);
            m.w32(packet + 36, lastTextures);
            m.w32(packet + 40, m.r32(face + 20));
            m.w32(packet + 44, gte.mfc2(12));
            m.w32(packet + 48, lastTextures >> 16);
            packet += 52;
        } else {
            m.w32(packet + 12, m.r32(face + 12));
            m.w32(packet + 16, gte.mfc2(13));
            m.w32(packet + 20, m.r32(face + 16));
            m.w32(packet + 24, gte.mfc2(14));
            const u32 lastTextures = m.r32(face + 20);
            m.w32(packet + 28, lastTextures);
            m.w32(packet + 32, gte.mfc2(12));
            m.w32(packet + 36, lastTextures >> 16);
            packet += 40;
        }
    }
    return packet;
}

u32 Game::direct_flat_quads(u32 list, u32 vertices, u32 packets, u32 orderTable, u32 depthShift) {
    cover(0x800628BCu);
    return direct_model_quads(list, vertices, packets, orderTable, depthShift, false);
}

u32 Game::direct_gouraud_quads(u32 list, u32 vertices, u32 packets, u32 orderTable, u32 depthShift) {
    cover(0x80062A38u);
    return direct_model_quads(list, vertices, packets, orderTable, depthShift, true);
}
}

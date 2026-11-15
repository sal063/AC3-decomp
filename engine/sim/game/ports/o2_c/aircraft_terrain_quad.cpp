#include "sim/game/game.hpp"

namespace ac3::sim {
u32 Game::o2_aircraft_terrain_quad(u32 tile, u32 packet, u32 column, u32 row) {
    cover(0x80104218u);
    Frame frame(*this, 16);
    const s32 edgeX = static_cast<s16>(column), edgeZ = static_cast<s16>(row);
    if (edgeX == -8) {
        gte.mtc2(0, m.r32(tile + 32)); gte.mtc2(1, m.r32(tile + 36)); gte.op(0x180001u);
        m.w32(packet + 8, gte.mfc2(14));
        m.w32(frame.at(0), gte.mfc2(8)); m.w32(frame.at(4), gte.cfc2(31));
        m.w32(frame.at(8), static_cast<s32>(gte.mfc2(19)) >> 2);
    } else m.w32(packet + 8, gte.mfc2(12));
    for (u32 vertex = 0; vertex < 3; ++vertex) {
        gte.mtc2(2 * vertex, m.r32(tile + 40 + 8 * vertex));
        gte.mtc2(2 * vertex + 1, m.r32(tile + 44 + 8 * vertex));
    }
    gte.op(0x280030u);
    for (u32 vertex = 0; vertex < 3; ++vertex) m.w32(packet + 16 + 8 * vertex, gte.mfc2(12 + vertex));
    m.w32(frame.at(8), static_cast<s32>(gte.mfc2(19)) >> 2);
    const u32 panel = m.r32(0x80112DACu);
    for (u32 vertex = 0; vertex < 4; ++vertex) {
        const s32 material = m.r16s(tile + 8 + 2 * vertex) / 128;
        m.w32(tile + 16 + 4 * vertex, m.r32(panel + 0x894 + 4 * static_cast<u32>(material)));
    }
    auto interpolate = [&](u32 destination, u32 source, s32 fraction) {
        const u32 from = tile + 16 + 4 * destination, toward = tile + 16 + 4 * source;
        u32 colour = static_cast<u32>(m.r8(from + 3)) << 24;
        for (u32 channel = 0; channel < 3; ++channel) {
            const s32 start = m.r8(from + channel), end = m.r8(toward + channel);
            colour |= static_cast<u32>(start + (end - start) * fraction / 128) << (8 * channel);
        }
        m.w32(from, colour);
    };
    if (edgeX == -8) {
        interpolate(0, 1, m.r16s(tile + 4)); interpolate(2, 3, m.r16s(tile + 4));
    }
    if (edgeZ == -8) {
        interpolate(0, 2, m.r16s(tile + 6)); interpolate(1, 3, m.r16s(tile + 6));
    }
    if (edgeX == 8) {
        interpolate(1, 0, m.r16s(tile + 4) - 128); interpolate(3, 2, m.r16s(tile + 4) - 128);
    }
    if (edgeZ == 8) {
        interpolate(2, 0, m.r16s(tile + 6) - 128); interpolate(3, 1, m.r16s(tile + 6) - 128);
    }
    m.w32(packet + 4, m.r32(tile + 16) | (static_cast<u32>(m.r8(packet + 7)) << 24));
    m.w32(packet + 12, m.r32(tile + 20)); m.w32(packet + 20, m.r32(tile + 24));
    const u32 last = m.r32(tile + 28); m.w32(packet + 28, last);
    return last;
}
}

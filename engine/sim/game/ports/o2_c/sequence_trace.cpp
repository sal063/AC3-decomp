#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kSequence = 0x80112E04u, kSource = 0x80112E08u, kMenu = 0x80112DC4u;
constexpr u32 kSeconds = 2, kPoints = 12;
void link(Memory& m, u32 packet, u32 ot) {
    m.w32(packet, (m.r32(packet) & 0xFF000000u) | (m.r32(ot) & 0xFFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (packet & 0xFFFFFFu));
}
}

u32 Game::o2_sequence_map(u32 packet) {
    cover(0x800F4C04u);
    Frame frame(*this, 8);
    const s32 sample = m.r16s(m.r32(kSequence) + kSeconds) / 4;
    const u32 point = m.r32(kSource) + 2 * static_cast<u32>(sample) + kPoints;
    const u32 left = 84u - m.r8(point), top = 190u - 2 * (255u - m.r8(point + 1));
    constexpr u32 kQuadBytes = 40, kTexturePage = 31, kClut = 0x603C;
    for (u32 half = 0; half < 2; ++half) {
        const u32 quad = packet + kQuadBytes * half;
        m.w8(quad + 3, 9); m.w8(quad + 7, 0x2C);
        for (u32 channel = 0; channel < 3; ++channel) m.w8(quad + 4 + channel, 128);
        for (u32 corner = 0; corner < 4; ++corner) {
            const u32 vertex = quad + 8 + 8 * corner;
            m.w16(vertex, left + (corner & 1u) * 256);
            m.w16(vertex + 2, top + 256 * (half + corner / 2));
            m.w8(vertex + 4, (corner & 1u) * 128);
            m.w8(vertex + 5, 64 * (half + corner / 2));
        }
        m.w16(quad + 14, kClut); m.w16(quad + 22, kTexturePage);
        link(m, quad, m.r32(m.r32(kMenu) + 8));
    }
    return packet + 2 * kQuadBytes;
}

u32 Game::o2_sequence_trace(u32 packet) {
    cover(0x800F4DF0u);
    Frame frame(*this, 48);
    const s32 sample = m.r16s(m.r32(kSequence) + kSeconds) / 4;
    const u32 current = m.r32(kSource) + 2 * static_cast<u32>(sample) + kPoints;
    const s32 scrollX = m.r8(current), scrollY = m.r8(current + 1);
    for (s32 age = 0; age < 40; ++age) {
        const s32 index = m.r16s(m.r32(kSequence) + kSeconds) / 4 - age;
        if (index <= 0) continue;
        const u32 source = m.r32(kSource);
        if (index >= m.r16s(source + 8)) continue;
        const u32 point = source + 2 * static_cast<u32>(index) + kPoints;
        m.w8(packet + 3, 4); m.w8(packet + 7, 0x52);
        for (u32 channel = 0; channel < 3; ++channel) {
            m.w8(packet + 4 + channel, 128 - 2 * age);
            m.w8(packet + 12 + channel, 126 - 2 * age);
        }
        m.w16(packet + 8, m.r8(point) - scrollX + 83);
        m.w16(packet + 10, 2 * (scrollY - m.r8(point + 1)) + 192);
        m.w16(packet + 16, m.r8(point - 2) - scrollX + 83);
        m.w16(packet + 18, 2 * (scrollY - m.r8(point - 1)) + 192);
        link(m, packet, m.r32(m.r32(kMenu) + 8));
        packet += 20;
    }
    m.w32(frame.at(16), 0);
    set_draw_mode(packet, 0, 0, 32, 0);
    link(m, packet, m.r32(m.r32(kMenu) + 8));
    return packet + 12;
}
}

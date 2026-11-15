#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kAddressMask = 0xFFFFFFu, kLengthMask = 0xFF000000u;
u32 polyline(Memory& m, u32 points, u32 packet, u32 ot, u32 vertices) {
    const u32 colour = 4 * vertices;
    m.w8(packet + 3, vertices + 2);
    m.w8(packet + 7, vertices == 3 ? 0x48 : 0x4C);
    m.w32(packet + 8 + 4 * vertices, 0x55555555u);
    const u32 command = m.r8(packet + 7);
    m.w8(packet + 7, m.r8(points + colour + 3) ? command | 2u : command & ~2u);
    const u32 tag = m.r32(packet);
    for (u32 channel = 0; channel < 3; ++channel)
        m.w8(packet + 4 + channel, m.r8(points + colour + channel));
    for (u32 coordinate = 0; coordinate < 2 * vertices; ++coordinate)
        m.w16(packet + 8 + 2 * coordinate, m.r16(points + 2 * coordinate));
    m.w32(packet, (tag & kLengthMask) | (m.r32(ot) & kAddressMask));
    m.w32(ot, (m.r32(ot) & kLengthMask) | (packet & kAddressMask));
    return packet + 4 * (vertices + 3);
}
}

u32 Game::o2_polyline3(u32 points, u32 packet, u32 ot) {
    cover(0x800F6EC0u);
    return polyline(m, points, packet, ot, 3);
}

u32 Game::o2_polyline3_double(u32 points, u32 packet, u32 ot) {
    cover(0x800F6FB0u);
    Frame frame(*this, 32);
    packet = o2_polyline3(points, packet, ot);
    for (u32 offset : {2u, 10u, 6u}) m.w16(points + offset, m.r16(points + offset) + 1u);
    return o2_polyline3(points, packet, ot);
}

u32 Game::o2_polyline4(u32 points, u32 packet, u32 ot) {
    cover(0x800F7014u);
    return polyline(m, points, packet, ot, 4);
}

u32 Game::o2_polyline4_double(u32 points, u32 packet, u32 ot) {
    cover(0x800F711Cu);
    Frame frame(*this, 32);
    packet = o2_polyline4(points, packet, ot);
    for (u32 offset : {2u, 10u, 6u}) m.w16(points + offset, m.r16(points + offset) + 1u);
    return o2_polyline4(points, packet, ot);
}

u32 Game::o2_hangar_draw_mode(u32 blend, u32 ot, u32 packet) {
    cover(0x80101514u);
    Frame frame(*this, 40);
    m.w32(frame.at(16), 0);
    set_draw_mode(packet, 0, 0, (blend & 3u) << 5, 0);
    m.w32(packet, (m.r32(packet) & kLengthMask) | (m.r32(ot) & kAddressMask));
    m.w32(ot, (m.r32(ot) & kLengthMask) | (packet & kAddressMask));
    return packet + 12;
}
}

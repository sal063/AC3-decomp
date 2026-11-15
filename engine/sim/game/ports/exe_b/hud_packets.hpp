#pragma once
#include "sim/game/game.hpp"

namespace ac3::sim::hud_packets {
constexpr u32 kLockBlocks = 0x800BCFB8u;
constexpr u32 kLockPrims = 0x800BCFBCu;
constexpr u32 kBlips = 0x800C67A0u;
constexpr u32 kRelations = 0x800C6ED0u;
constexpr u32 kLandingBlocks = 0x800BCFE0u;
constexpr u32 kLandingPrims = 0x800BCFE4u;
struct Point { s32 x, y; };
inline void point(Memory& m, u32 packet, u32 offset, Point p) {
    m.w16(packet + offset, static_cast<u32>(p.x));
    m.w16(packet + offset + 2, static_cast<u32>(p.y));
}
inline void color(Memory& m, u32 packet, u32 rgb) {
    m.w8(packet + 4, rgb); m.w8(packet + 5, rgb >> 8); m.w8(packet + 6, rgb >> 16);
}
inline u32 line(Memory& m, u32 blocks, u32 prims, u32 countOffset, u32 start, Point a, Point b, u32 rgb) {
    const u32 state = m.r32(blocks);
    const u32 packet = m.r32(prims) + start + 16 * m.r32(state + countOffset);
    color(m, packet, rgb); point(m, packet, 8, a); point(m, packet, 12, b);
    const u32 count = m.r32(state + countOffset) + 1;
    m.w32(state + countOffset, count);
    return count;
}
inline u32 polyline(Memory& m, u32 rgb, Point a, Point b, Point c) {
    const u32 state = m.r32(kLockBlocks);
    const u32 packet = m.r32(kLockPrims) + 0x408 + 24 * m.r32(state + 12);
    m.w32(packet + 4, rgb | 0x4A000000u);
    point(m, packet, 8, a); point(m, packet, 12, b); point(m, packet, 16, c);
    const u32 count = m.r32(state + 12) + 1;
    m.w32(state + 12, count);
    return count;
}
inline u32 sprite(Memory& m, u32 entry, s32 x, s32 y, u32 rgb) {
    const u32 state = m.r32(kLockBlocks);
    const u32 packet = m.r32(kLockPrims) + 0x1008 + 20 * m.r32(state + 16);
    if (rgb != 0xFFFFFFFFu) color(m, packet, rgb);
    point(m, packet, 8, {x, y});
    m.w8(packet + 12, m.r8(entry)); m.w8(packet + 13, m.r8(entry + 1));
    m.w16(packet + 16, m.r8(entry + 2)); m.w16(packet + 18, m.r8(entry + 3));
    const u32 count = m.r32(state + 16) + 1;
    m.w32(state + 16, count);
    return count;
}
}

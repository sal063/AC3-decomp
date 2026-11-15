#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
u32 Game::raycast_tile(u32 start, u32 end, u32 hit, u32 vertices, u32 tile) {
    cover(0x8009A484u);
    Frame frame(*this, 80);
    if (!tile) {
        for (u32 axis = 0; axis < 3; ++axis) m.w32(hit + 4 * axis, m.r32(start + 4 * axis));
        return 0x7FFFFFFFu;
    }
    const u32 cell = frame.at(24), delta = frame.at(48);
    m.w16(cell, 1024u - ((m.r32(start) >> 12) & 2047u));
    m.w16(cell + 4, 1024u - ((m.r32(start + 8) >> 12) & 2047u));
    m.w32(cell + 8, tile);
    m.w16(cell + 2, static_cast<s32>(0u - m.r32(start + 4)) >> 12);
    m.w32(cell + 20, 0x200000u);
    for (u32 axis = 0; axis < 3; ++axis)
        m.w32(delta + 4 * axis, static_cast<s32>(m.r32(end + 4 * axis) - m.r32(start + 4 * axis)) >> 12);
    m.w32(frame.at(16), cell);
    const u32 distance = static_cast<u32>(ray_hit_tiles(delta, hit, vertices, m.r32(m.r32(g::kWorld) + 588), cell, hit));
    for (u32 axis = 0; axis < 3; ++axis) m.w32(hit + 4 * axis, m.r32(hit + 4 * axis) + m.r32(start + 4 * axis));
    return distance;
}
}

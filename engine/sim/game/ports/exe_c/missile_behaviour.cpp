#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
u32 Game::weapon_cruise(u32 weapon) {
    cover(0x800696D4u);
    Frame frame(*this, 0x18);
    missile_home_second(weapon);
    constexpr s32 kFollowRange = 4097, kLevelRoll = 227;
    if (m.r32s(weapon + 104) < kFollowRange) return 1;
    const s32 roll = m.r16s(weapon + 48);
    if ((roll < 0 ? -roll : roll) >= kLevelRoll) return 0;

    const u32 world = m.r32(g::kWorld), bounds = world + 672;
    const u32 grid = m.r32(world + 584);
    m.w16(weapon + 88, 0);
    auto ahead = [&](u32 axis) {
        s32 coordinate = static_cast<s32>(m.r32(weapon + 28 + 4 * axis)
            + 20u * m.r32(weapon + 56 + 4 * axis));
        const s32 low = m.r32s(bounds + 8 * axis), high = m.r32s(bounds + 8 * axis + 4);
        if (coordinate < low) coordinate = low;
        if (coordinate > high) coordinate = high;
        return coordinate;
    };
    const s32 x = ahead(0), y = ahead(1), z = ahead(2);
    auto tileCoordinate = [](s32 coordinate) {
        s32 cell = (coordinate >> 23) + 128;
        if (cell < 0) cell &= 7;
        if (cell >= 256) cell = (cell & 7) + 248;
        return static_cast<u32>(cell);
    };
    const u32 tile = m.r16(grid + 2u * (tileCoordinate(x) + 256u * tileCoordinate(z))) & 511u;
    if (!tile) return 0;
    const u32 tileData = m.r32(m.r32(world + 588) + 4u * tile);
    const s32 limit = m.r16s(weapon + 124);
    const s32 heightError = (y >> 12) + 400 - m.r16s(tileData + 22);
    const s32 scaledError = static_cast<s32>(static_cast<u32>(heightError)
        * (4u * static_cast<u32>(limit)));
    m.w16(weapon + 88, static_cast<u32>(scaledError / 200));
    if (m.r16s(weapon + 88) < -16 * limit)
        m.w16(weapon + 88, static_cast<u32>(-16 * limit));
    const bool aboveLimit = m.r16s(weapon + 88) > 16 * limit;
    if (aboveLimit) m.w16(weapon + 88, static_cast<u32>(16 * limit));
    return aboveLimit;
}

u32 Game::weapon_split(u32 weapon) {
    cover(0x80069900u);
    Frame frame(*this, 0x38);
    missile_home_second(weapon);
    if (m.r32s(weapon + 104) >= m.r32s(weapon + 152)) return 0;
    const u32 mount = frame.at(0x10), pitch = mount + 16, yaw = mount + 18, roll = mount + 20;
    for (u32 axis = 0; axis < 3; ++axis)
        m.w32(mount + 4 * axis, m.r32(weapon + 28 + 4 * axis));
    for (u32 axis = 0; axis < 3; ++axis)
        m.w16(mount + 16 + 2 * axis, m.r16(weapon + 44 + 2 * axis));
    auto turn = [&](u32 angle, s32 delta) { m.w16(angle, m.r16(angle) + static_cast<u32>(delta)); };
    auto spawn = [&] {
        return submunition_spawn(m.r32(weapon + 108), mount, weapon + 56, m.r32(weapon + 112));
    };
    if (m.r32(weapon + 112) & 32u) {
        turn(pitch, 682); turn(yaw, 682); spawn();
        turn(yaw, -1365); spawn();
        turn(pitch, -682); spawn();
        turn(yaw, 1365);
    } else {
        m.w16(roll, 0); spawn();
        turn(pitch, 1137); spawn();
        turn(pitch, -2275); turn(yaw, 2048); spawn();
        turn(yaw, -4096);
    }
    const u32 result = spawn();
    m.w16(weapon + 24, 0);
    return result;
}
}

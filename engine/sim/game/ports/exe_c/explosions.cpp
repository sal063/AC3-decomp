#include "sim/game/game.hpp"

namespace ac3::sim {
u32 Game::weapon_explode(u32 weapon) {
    cover(0x80069C8Cu);
    Frame frame(*this, 0x20);
    const u32 position = weapon + 28;
    if (m.r32(weapon) & 4u) {
        explosion_big(position, weapon + 140, weapon + 16, 0);
        return sound_at(29, position, 0);
    }
    explosion_air(position, weapon + 140, 0);
    if ((m.r32(weapon + 112) & 63u) >= 2u) return 0;
    return sound_at(28, position, 0);
}

u32 Game::weapon_blast(u32 weapon, u32 size, u32 groundSound) {
    Frame frame(*this, 0x20);
    const u32 position = weapon + 28;
    blast_effect(position, size);
    if (m.r32(weapon) & 4u) return sound_at(groundSound, position, 0);
    if ((m.r32(weapon + 112) & 63u) >= 2u) return 0;
    return sound_at(28, position, 0);
}

u32 Game::weapon_blast_small(u32 weapon) {
    cover(0x80069D88u);
    return weapon_blast(weapon, 0, 37);
}

u32 Game::weapon_blast_large(u32 weapon) {
    cover(0x80069EC0u);
    return weapon_blast(weapon, 1, 29);
}

u32 Game::weapon_explode_target(u32 weapon) {
    cover(0x80069E10u);
    Frame frame(*this, 0x18);
    if (m.r32(weapon) & 1u) return 1;
    const u32 target = slot(m.r32(weapon + 112) & 63u);
    for (u32 axis = 0; axis < 3; ++axis)
        m.w32(weapon + 28 + 4 * axis, m.r32(target + 8 + 4 * axis));
    for (u32 axis = 0; axis < 3; ++axis)
        m.w16(weapon + 44 + 2 * axis, m.r16(target + 24 + 2 * axis));
    explosion_big(weapon + 28, weapon + 140, weapon + 16, 0);
    m.w32(sp - 0x18u, weapon + 28);
    return sound_at(29, weapon + 28, 0);
}
}

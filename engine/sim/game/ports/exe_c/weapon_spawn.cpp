#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"
#include <bit>

namespace ac3::sim {
namespace {
constexpr u32 kActiveMask = 0x1548, kFirstWeapon = 200, kWeaponStride = 164;
constexpr u32 kShortTurn = 0x800BCDDCu, kShortGuidance = 0x800BCDE4u, kShortLife = 0x800BCDECu;
constexpr u32 kLongTurn = 0x800BCDF4u, kLongGuidance = 0x800BCDFCu, kLongLife = 0x800BCE04u;
}

u32 Game::weapon_alloc(u32 owner, u32 position, u32 velocity, s32 target) {
    cover(0x800685C0u);
    Frame frame(*this, 0x28);
    const u32 work = m.r32(g::kWeapons), mask = m.r32(work + kActiveMask);
    if (mask == 0xFFFFFFFFu) return 0;
    const u32 index = static_cast<u32>(std::countr_one(mask));
    m.w32(work + kActiveMask, mask | (1u << index));
    const u32 ownerCount = work + 72 + (owner & 63u);
    m.w8(ownerCount, m.r8(ownerCount) + 1);
    const u32 weapon = m.r32(g::kWeapons) + kFirstWeapon + kWeaponStride * index;
    m.w8(weapon + 4, 0);
    m.w16(weapon + 8, 0); m.w16(weapon + 10, 0);
    m.w32(weapon, 0);
    m.w8(weapon + 52, 1); m.w8(weapon + 53, 0);
    m.w16(weapon + 26, 64); m.w32(weapon + 108, owner);
    for (u32 axis = 0; axis < 3; ++axis) m.w32(weapon + 28 + 4 * axis, m.r32(position + 4 * axis));
    for (u32 axis = 0; axis < 3; ++axis) m.w16(weapon + 44 + 2 * axis, m.r16(position + 16 + 2 * axis));
    for (u32 axis = 0; axis < 3; ++axis) m.w32(weapon + 56 + 4 * axis, m.r32(velocity + 4 * axis));
    const u32 speed = static_cast<u32>(length3(m.r32s(weapon + 56) >> 12,
        m.r32s(weapon + 60) >> 12, m.r32s(weapon + 64) >> 12)) << 12;
    m.w32(weapon + 100, speed); m.w32(weapon + 80, speed);
    m.w32(weapon + 96, 0); m.w32(weapon + 72, 0); m.w32(weapon + 76, 0);
    for (u32 axis = 0; axis < 3; ++axis) m.w16(weapon + 88 + 2 * axis, 0);
    m.w32(weapon + 104, 120000); m.w32(weapon + 112, static_cast<u32>(target));
    u32 targetPosition, targetVelocity;
    if (target == -1) {
        targetPosition = targetVelocity = weapon + 28;
        m.w32(weapon, m.r32(weapon) | 1u);
    } else {
        const u32 targetIndex = static_cast<u32>(target) & 63u, entity = slot(targetIndex);
        targetPosition = entity + 8; targetVelocity = entity + 56;
        const u32 count = m.r32(g::kWeapons) + 136 + targetIndex;
        m.w8(count, m.r8(count) + 1); m.w8(m.r32(g::kWeapons) + 8 + targetIndex, index);
        const u32 bits = m.r32(g::kWeapons) + ((static_cast<u32>(target) & 32u) ? 4u : 0u);
        m.w32(bits, m.r32(bits) | (1u << (targetIndex & 31u)));
    }
    m.w32(weapon + 116, targetPosition); m.w32(weapon + 120, targetVelocity);
    return weapon;
}

u32 Game::spawn_weapon(u32 owner, u32 position, u32 velocity, s32 target, u32 flags) {
    cover(0x80068800u);
    Frame frame(*this, 0x28);
    const u32 weapon = weapon_alloc(owner, position, velocity, target);
    if (!weapon) return 0;
    if (!(flags & 0x01000000u)) m.w8(weapon + 53, m.r8(weapon + 53) | 8u);
    m.w8(weapon + 53, m.r8(weapon + 53) | ((flags & 0x02000000u) ? 32u : 16u));
    if (flags & 0x04000000u) m.w32(weapon, m.r32(weapon) | 0x80u);
    if (flags & 0x08000000u) m.w32(weapon, m.r32(weapon) | 0x100u);
    m.w8(weapon + 6, 0); m.w8(weapon + 7, 0); m.w32(weapon, m.r32(weapon) | 8u);
    u32 range = flags & 255u;
    if (range == 2) range = distance(position, m.r32(weapon + 116)) >= 5000 ? 1 : 0;
    if (range > 1) return weapon;
    const bool longRange = range == 1;
    const u32 model = (flags >> 8) & 255u, performance = (flags >> 16) & 255u;
    m.w8(weapon + 5, range);
    m.w16(weapon + 24, m.r16((longRange ? kLongLife : kShortLife) + 2 * model));
    m.w16(weapon + 124, m.r16((longRange ? kLongTurn : kShortTurn) + 2 * performance));
    const u32 guidance = m.r16((longRange ? kLongGuidance : kShortGuidance) + 2 * performance);
    m.w32(weapon + 128, longRange ? 491520 : 327680);
    m.w32(weapon + 132, longRange ? 5120 : 7680);
    m.w32(weapon + 136, longRange ? 5120 : 3584); m.w16(weapon + 126, guidance);
    return weapon;
}
}

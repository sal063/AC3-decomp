#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kLife = 24, kDamageKind = 26, kVelocity = 56, kBlast = 156, kBlastDivisor = 160;

void configureBomb(Memory& memory, u32 bomb, bool large) {
    memory.w16(bomb + kDamageKind, large ? 1024 : 512);
    memory.w8(bomb + 52, 4);
    memory.w8(bomb + 5, 2);
    memory.w8(bomb + 7, large ? 1 : 3);
    memory.w16(bomb + kLife, 240);
    memory.w32(bomb + kBlast, large ? 1260 : 1024);
    memory.w32(bomb + kBlastDivisor, 192);
    memory.w8(bomb + 6, 0);
    for (u32 axis = 0; axis < 3; ++axis) {
        const u32 at = bomb + kVelocity + 4 * axis;
        const u32 velocity = memory.r32(at);
        memory.w32(at, velocity + static_cast<u32>(static_cast<s32>(velocity) >> 1));
    }
    memory.w32(bomb, memory.r32(bomb) | (large ? 0x50u : 0x10u));
}
}

u32 Game::spawn_fast_bomb(u32 owner, u32 position, u32 velocity, s32 target) {
    cover(0x80068A58u);
    Frame frame(*this, 0x18);
    const u32 bomb = weapon_alloc(owner, position, velocity, target);
    if (bomb) configureBomb(m, bomb, false);
    return bomb;
}

u32 Game::spawn_large_bomb(u32 owner, u32 position, u32 velocity, s32 target) {
    cover(0x80068AFCu);
    Frame frame(*this, 0x18);
    const u32 bomb = weapon_alloc(owner, position, velocity, target);
    if (bomb) configureBomb(m, bomb, true);
    return bomb;
}
}

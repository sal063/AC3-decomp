#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kBulletRecords = 0x15D0u;
constexpr u32 kBulletMasks = 0x33D0u;
constexpr u32 kBulletStride = 60;
constexpr u32 kNoDrag = 1;
constexpr u32 kNoGravity = 2;
constexpr u32 kLifetime = 4;
constexpr u32 kPosition = 8;
constexpr u32 kTrailEnd = 24;
constexpr u32 kVelocity = 40;
}

u32 Game::bullet_replay_update() {
    cover(0x8006C9B8u);
    gte.mtc2(8, 4000);
    const u32 maskBase = m.r32(g::kWeapons) + kBulletMasks;
    for (u32 bank = 0; bank < 4; ++bank) {
        const u32 maskAddress = maskBase + 4 * bank;
        u32 active = m.r32(maskAddress);
        if (!active) continue;
        for (u32 index = 0; index < 32; ++index) {
            const u32 bit = 1u << index;
            if (!(active & bit)) continue;
            const u32 bullet = m.r32(g::kWeapons) + kBulletRecords
                + kBulletStride * (32 * bank + index);
            const s32 remaining = wrap::sub(m.r32s(bullet + kLifetime), 1);
            m.w32(bullet + kLifetime, static_cast<u32>(remaining));
            if (remaining < 0) {
                active &= ~bit;
                continue;
            }
            s32 vx = m.r32s(bullet + kVelocity);
            s32 vy = m.r32s(bullet + kVelocity + 4);
            s32 vz = m.r32s(bullet + kVelocity + 8);
            m.w32(bullet + kPosition, m.r32(bullet + kPosition) + static_cast<u32>(vx));
            m.w32(bullet + kPosition + 8, m.r32(bullet + kPosition + 8) + static_cast<u32>(vz));
            m.w32(bullet + kPosition + 4, m.r32(bullet + kPosition + 4) + static_cast<u32>(vy));
            m.w32(bullet + kTrailEnd + 4, m.r32(bullet + kTrailEnd + 4) + static_cast<u32>(vy));
            m.w32(bullet + kTrailEnd, m.r32(bullet + kTrailEnd) + static_cast<u32>(vx));
            const u32 flags = m.r32(bullet);
            m.w32(bullet + kTrailEnd + 8, m.r32(bullet + kTrailEnd + 8) + static_cast<u32>(vz));
            if (!(flags & kNoGravity)) vy = wrap::add(vy, 0x444);
            vx >>= 6; vy >>= 6; vz >>= 6;
            if (!(flags & kNoDrag)) {
                gte.mtc2(9, static_cast<u32>(vx));
                gte.mtc2(10, static_cast<u32>(vy));
                gte.mtc2(11, static_cast<u32>(vz));
                gte.op(0x198003Du);
                vx = static_cast<s32>(gte.mfc2(25));
                vy = static_cast<s32>(gte.mfc2(26));
                vz = static_cast<s32>(gte.mfc2(27));
            }
            m.w32(bullet + kVelocity, static_cast<u32>(vx) << 6);
            m.w32(bullet + kVelocity + 4, static_cast<u32>(vy) << 6);
            m.w32(bullet + kVelocity + 8, static_cast<u32>(vz) << 6);
        }
        m.w32(maskAddress, active);
    }
    return 0;
}
}

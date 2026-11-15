#include "sim/game/game.hpp"

namespace ac3::sim {
u32 Game::screen_outcode(u32 point) {
    cover(0x800965D4u);
    const s32 x = m.r16s(point), y = m.r16s(point + 2);
    return (x < -160 ? 1u : x > 160 ? 2u : 0u) |
           (y < -120 ? 4u : y > 120 ? 8u : 0u);
}

u32 Game::effect_interpolate(u32 from, u32 to, u32 result, u32 level) {
    cover(0x8009211Cu);
    gte.mtc2(8, level);
    for (u32 i = 0; i < 3; ++i) gte.mtc2(9 + i, m.r32(from + 4 * i));
    const u32 y = m.r32(to + 4), z = m.r32(to + 8), x = m.r32(to);
    gte.ctc2(21, x);
    gte.ctc2(22, y);
    gte.ctc2(23, z);
    gte.op(0x980011u);
    for (u32 i = 0; i < 3; ++i) m.w32(result + 4 * i, gte.mfc2(25 + i));
    return y;
}

u32 Game::transient_particles_clear() {
    cover(0x80092AB8u);
    constexpr u32 kEffectsPointer = 0x800BCF90u;
    const u32 effects = m.r32(kEffectsPointer);
    constexpr u32 kBusy = 1536, kTransient = 4236;
    for (u32 bank = 0; bank < 3; ++bank) {
        const u32 released = m.r32(effects + kTransient + 4 * bank);
        const u32 busy = m.r32(effects + kBusy + 4 * bank);
        m.w32(effects + kTransient + 4 * bank, 0);
        m.w32(effects + kBusy + 4 * bank, busy & ~released);
    }
    const u32 current = m.r32(kEffectsPointer);
    m.w32(current + 13248, 0);
    m.w32(current + 14692, 0);
    m.w32(current + 15144, 0);
    return current;
}

u32 Game::transient_particle_alloc() {
    cover(0x80092B04u);
    const u32 effects = m.r32(0x800BCF90u);
    constexpr u32 kBusy = 1536, kTransient = 4236, kRecords = 1548;
    for (u32 bank = 0; bank < 3; ++bank) {
        const u32 busy = m.r32(effects + kBusy + 4 * bank);
        if (busy == 0xFFFFFFFFu) continue;
        u32 bit = 1, index = 0;
        while (busy & bit) { bit <<= 1; ++index; }
        const u32 pending = m.r32(effects + kTransient + 4 * bank);
        m.w32(effects + kBusy + 4 * bank, busy | bit);
        m.w32(effects + kTransient + 4 * bank, pending | bit);
        return effects + kRecords + 896 * bank + 28 * index;
    }
    return 0;
}
}

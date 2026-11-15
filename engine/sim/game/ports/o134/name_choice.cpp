#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
u32 Game::o1_name_choice() {
    cover(0x800CEB90u);
    Frame frame(*this, 24);
    constexpr u32 kState = 0x800DD9D0u;
    if ((m.r32(m.r32(g::kScene) + 296) & 16u) && (m.r32(m.r32(g::kScene) + 300) & 16u)) {
        m.w16(m.r32(kState) + 76, 2); m.w16(m.r32(kState) + 78, 0);
        const u32 name = o1_save_name_address(0);
        libc_strcpy(m.r32(g::kScene) + 2376, name); t1_pilot_reset();
        const u32 scene = m.r32(g::kScene);
        m.w32(scene + 2404, m.r32(scene + 2404) & ~1u); sound_play(89);
    }
    if (!(m.r32(m.r32(g::kScene) + 296) & 32u)) return 0;
    if (!(m.r32(m.r32(g::kScene) + 300) & 32u)) return 0x800E0000u;
    m.w16(m.r32(kState) + 76, 0); m.w16(m.r32(kState) + 78, 0);
    return sound_play(59);
}
}

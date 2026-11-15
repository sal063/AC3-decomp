#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
u32 Game::formation_depth(u32 n, u32 j, u32 out) {
    cover(0x8004BD68u);
    const u32 place = m.r8(0x800B18D4u + 8 * (n - 2) + j);
    m.w16(out, 0);
    m.w16(out + 2, 0);
    const u32 d = m.r16(0x800B18C0u + 2 * place);
    m.w16(out + 4, d);
    return d;
}

void Game::ai_behaviour_reset(u32 i) {
    cover(0x800506F8u);
    const u32 a = m.r32(g::kAiInfo) + 184 * i;
    m.w8(a + 61, 0);
    m.w8(a + 99, 0);
}
}

#include "sim/game/game.hpp"

namespace ac3::sim {
u32 Game::missile_home(u32 weapon) {
    cover(0x800692C4u);
    if (m.r32s(weapon + 80) == -1 && (m.r32(weapon + 104) << 12) == 0x80000000u)
        unported(0x80069314u, "missile lead division overflow");
    const bool levelRoll = (m.r32(weapon) & 8u) != 0;
    wpn_homing(weapon, true);
    return m.r32(weapon + (levelRoll ? 0u : 80u));
}

u32 Game::missile_home_second(u32 weapon) {
    cover(0x800694CCu);
    const bool levelRoll = (m.r32(weapon) & 8u) != 0;
    wpn_homing(weapon, true);
    return m.r32(weapon + (levelRoll ? 0u : 80u));
}

u32 Game::missile_home_always(u32 weapon) {
    cover(0x80069A9Cu);
    const bool levelRoll = (m.r32(weapon) & 8u) != 0;
    wpn_homing(weapon, false);
    return m.r32(weapon + (levelRoll ? 0u : 80u));
}
}

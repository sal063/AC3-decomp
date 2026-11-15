#include "sim/game/game.hpp"

namespace ac3::sim {
u32 Game::trails_age_weapons() {
    cover(0x8007BF30u);
    constexpr u32 kTrails = 0x800BD0B0u;
    trails_age(m.r32(kTrails), 2312);
    return m.r32(kTrails);
}

u32 Game::trails_age_aircraft() {
    cover(0x8007CEB0u);
    constexpr u32 kTrails = 0x800BD0B0u, kPoolB = 74020;
    trails_age(m.r32(kTrails) + kPoolB, 1288);
    return m.r32(kTrails) + 0x10000u;
}
}

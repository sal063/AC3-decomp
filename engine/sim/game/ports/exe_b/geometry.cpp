#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
s32 Game::special_weapon_none() {
    cover(0x8003A624u);
    return -1;
}

s32 Game::off_axis_angle_max(u32 v) {
    cover(0x80045148u);
    const s32 x = m.r32s(v), y = m.r32s(v + 4);
    const s32 ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
    const s32 a = ratan2(ay < ax ? ax : ay, m.r32s(v + 8));
    return a < 0 ? -a : a;
}
}

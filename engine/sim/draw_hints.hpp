#pragma once

#include "core/types.hpp"

namespace ac3::sim {
struct DrawHints {
    float squeeze = 1.0f;
    bool wide = false;
    float centre = 160.0f;
    s32 screen[4] = {0, 0, 320, 240};
    u32 hud_slot[2] = {};
};
}

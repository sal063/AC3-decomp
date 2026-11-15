#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
u32 Game::overlay_record_done() {
    cover(0x8009B6ACu);
    m.w32(0x800BD0C8u, 0);
    return 0;
}

u32 Game::scene_record_done() {
    cover(0x8009B904u);
    const u32 scene = m.r32(g::kScene);
    m.w32(scene + 68, 0);
    return scene;
}

void Game::scenery_fire_trigger(s32 age) {
    cover(0x80094380u);
    Frame frame(*this, 24);
    if (age == 500) scenery_fires(0);
}
}

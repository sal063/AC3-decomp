#include "sim/game/game.hpp"

namespace ac3::sim {
u32 Game::o2_parent_dialog_tick() {
    cover(0x800DE3CCu);
    Frame frame(*this, 32);
    constexpr u32 kDialog = 0x80112DDCu, kMenu = 0x80112DC4u;
    const u32 original = m.r32(kDialog);
    m.w32(original + 8, m.r32(m.r32(kMenu) + 628));
    if (m.r8(original) >= kEnginePage) {
        o2x_page_tick();
    } else {
        const u32 states = m.r32(0x8010AD00u + 4 * m.r8(original));
        call_guest(m.r32(states + 4 * m.r8(original + 1)), 0, 0);
    }
    if (m.r8(m.r32(kDialog) + 2) == 1) {
        m.w8(m.r32(kMenu) + 124, 0); m.w8(m.r32(kMenu) + 125, 0);
        const u32 menu = m.r32(kMenu);
        m.w16(menu + 452, 0); m.w16(menu + 454, 0); heap_set(m.r32(kDialog));
    }
    call_guest(m.r32(0x8010AC6Cu + 4 * static_cast<u32>(m.r16s(m.r32(kMenu) + 454))), 0, 0);
    const u32 packet = m.r32(m.r32(kDialog) + 8);
    m.w32(m.r32(kMenu) + 628, packet);
    return packet;
}
}

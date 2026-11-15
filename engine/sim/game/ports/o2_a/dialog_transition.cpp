#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDialog = 0x80112DDCu, kView = 0x800BCE48u;
}

void Game::o2_dialog_hangar_fade() {
    cover(0x800DCC80u);
    Frame frame(*this, 24);
    const s32 value = o2_fade_value(1);
    if (o2_fade_state(1) == 2) {
        o2_entry_enter(0, 1); o2_entry_leave(0, 15); o2_hangar_fly();
        const u32 dialog = m.r32(kDialog);
        m.w8(dialog + 1, m.r8(dialog + 1) + 1);
        o2_fade_set(1, 1, 0, 25);
    } else {
        m.w8(m.r32(kView) + 16, value * 143 / 29);
        m.w8(m.r32(kView) + 17, value * 135 / 29);
        m.w8(m.r32(kView) + 18, value * 83 / 29);
        const u32 view = m.r32(kView);
        m.w32(view, m.r32(view) | 8);
    }
}

void Game::o2_metadata_layout_toggle() {
    cover(0x800D5BD4u);
    Frame frame(*this, 32);
    const u32 dialog = m.r32(kDialog);
    const s32 state = m.r16s(dialog + 12);
    if (state == 2) m.w8(dialog + 28, 0);
    else if (state == 0 || state == 1 || state == 3) {
        const u32 normalEntry = state == 0 ? 5 : state == 1 ? 6 : 8;
        const u32 alternateEntry = state == 0 ? 9 : state == 1 ? 10 : 11;
        if (m.r8(dialog + 28) == 0) {
            o2_entry_reset(2, normalEntry); o2_group_set(2, 16 | (1u << normalEntry));
        }
        if (m.r8(m.r32(kDialog) + 28) == 1) {
            o2_entry_reset(2, alternateEntry); o2_group_set(2, 16 | (1u << alternateEntry));
        }
    }
    const u32 current = m.r32(kDialog);
    m.w8(current + 28, 1u - m.r8(current + 28));
}
}

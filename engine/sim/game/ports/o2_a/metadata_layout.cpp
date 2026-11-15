#include "sim/game/game.hpp"

namespace ac3::sim {
namespace { constexpr u32 kDialog = 0x80112DDCu, kScreens = 0x80112DC0u; }

void Game::o2_metadata_layout_reset() {
    cover(0x800D4D20u);
    Frame frame(*this, 24);
    const u32 original = m.r32(kDialog);
    m.w8(original + 30, 0);
    const u32 current = m.r32(kDialog);
    m.w32(original + 16, 1); m.w8(current + 31, 0);
    for (u32 i = 0; i < 15; ++i) m.w8(m.r32(m.r32(kScreens) + 92) + 8 + i, 0);
    auto open_category = [&](u32 compactEntry, u32 fullEntry, u32 compactMask, u32 fullMask) {
        o2_entry_reset(2, 4);
        const bool compact = m.r8(m.r32(kDialog) + 28) != 0;
        o2_entry_reset(2, compact ? compactEntry : fullEntry);
        o2_group_set(2, compact ? compactMask : fullMask);
    };
    if (m.r16s(m.r32(kDialog) + 12) == 0) open_category(5, 9, 0x30, 0x210);
    if (m.r16s(m.r32(kDialog) + 12) == 1) open_category(6, 10, 0x50, 0x410);
    if (m.r16s(m.r32(kDialog) + 12) == 2) {
        o2_entry_reset(2, 4); o2_entry_reset(2, 7); o2_group_set(2, 0x90);
        m.w8(m.r32(kDialog) + 28, 1);
    }
    if (m.r16s(m.r32(kDialog) + 12) == 3) {
        open_category(8, 11, 0x110, 0x810);
        o2_fade_set(4, 2, 10, 10); o2_fade_set(5, 2, 10, 10);
        o2_fade_set(6, 0, 0, 0); o2_fade_set(7, 0, 0, 0); o2_fade_set(2, 0, 0, 0);
    }
    o2_fade_set(1, 0, 0, 0);
}
}

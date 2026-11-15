#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kSelector = 0x80112DD0u, kPanel = 0x80112DB8u, kCooldown = 0x80112DD4u;
constexpr u32 kMenu = 0x80112DC4u, kScreens = 0x80112DC0u, kRecordLoaded = 0x8009B904u;
u32 flags(Memory& m) { return m.r32(m.r32(kScreens) + 92); }
u32 scene(Memory& m) { return m.r32(g::kScene); }
bool repeat(Memory& m, u32 button) { return (m.r32(scene(m) + 304) & button) != 0; }
bool pressed(Memory& m, u32 button, u32 blocked) {
    const u32 state = scene(m); return (m.r32(state + 300) & button) && !(m.r32(state + 296) & blocked);
}
}

void Game::o2_grid_navigate(bool panel) {
    const u32 global = panel ? kPanel : kSelector, countField = panel ? 184 : 596;
    auto grid = [&] { return m.r32(global); };
    auto lastPage = [&] { return static_cast<s32>(m.r32(grid() + countField) - 1) / 4; };
    auto image = [&] {
        const s32 page = m.r8(grid() + 2);
        if (panel) o2_panel_image_column(page); else o2_selector_image_column(page);
    };
    auto palette = [&] {
        const u32 column = m.r8(grid() + 3);
        if (panel) o2_panel_palette(column); else o2_selector_palette(column);
    };
    auto pageTransition = [&](u32 direction) { m.w8(flags(m) + 51, direction); m.w32(kCooldown, 12); };
    if (m.r32(kCooldown) == 0) {
        const u32 page = m.r8(grid() + 2);
        o2_flag(49, (static_cast<s32>(page) < lastPage() ? 2u : 0u) | (page ? 1u : 0u));
        m.w8(flags(m) + 51, 0);
    } else m.w32(kCooldown, m.r32(kCooldown) - 1);
    if (repeat(m, 2)) {
        const u32 current = grid(), column = m.r8(current + 3), index = column + 4 * m.r8(current + 2);
        if (static_cast<s32>(index) < static_cast<s32>(m.r32(current + countField) - 1)) {
            if (column < 3) m.w8(current + 3, column + 1);
            else {
                m.w8(current + 3, 0); const u32 target = grid(); m.w8(target + 2, m.r8(target + 2) + 1);
                image(); pageTransition(2);
            }
            sound_play(61); palette();
        }
    }
    if (repeat(m, 1)) {
        const u32 current = grid(), column = m.r8(current + 3);
        if (column != 0) { m.w8(current + 3, column - 1); sound_play(61); palette(); }
        else if (m.r8(current + 2) != 0) {
            m.w8(current + 3, 3); const u32 target = grid(); m.w8(target + 2, m.r8(target + 2) - 1);
            image(); pageTransition(1); sound_play(61); palette();
        }
    }
    if (repeat(m, 512) && static_cast<s32>(m.r8(grid() + 2)) < lastPage()) {
        const u32 target = grid(); m.w8(target + 2, m.r8(target + 2) + 1);
        const u32 current = grid(), count = m.r32(current + countField), index = 4 * m.r8(current + 2) + m.r8(current + 3);
        if (static_cast<s32>(index) >= static_cast<s32>(count)) {
            const s32 last = static_cast<s32>(count - 1); m.w8(current + 3, static_cast<u32>(last % 4));
        }
        if (!panel) sound_play(61);
        image(); palette();
        if (panel) sound_play(61);
        pageTransition(2);
    }
    if (repeat(m, 256) && m.r8(grid() + 2) != 0) {
        const u32 target = grid(); m.w8(target + 2, m.r8(target + 2) - 1);
        if (!panel) sound_play(61);
        image(); palette();
        if (panel) sound_play(61);
        pageTransition(1);
    }
}

u32 Game::o2_selector_grid_input() {
    cover(0x800CF9A8u);
    Frame frame(*this, 32);
    o2_grid_navigate(false);
    if (pressed(m, 16, 0x2F)) {
        sound_play(60); o2_selector_completion_clear(); m.w8(m.r32(kSelector), 2); m.w8(m.r32(kSelector) + 1, 0);
        const u32 current = m.r32(kSelector), index = 4 * m.r8(current + 2) + m.r8(current + 3);
        const u32 record = m.r32(0x80109998u + 4 * m.r8(current + 516 + index));
        const u32 list = m.r32(0x800BCE50u) + 96; m.w32(list, record);
        const u32 buffer = mem_alloc(records_size(list, 1));
        m.w32(scene(m) + 68, 1); request_records(m.r32(0x800BCE50u) + 96, 1, buffer, kRecordLoaded);
    }
    if (pressed(m, 32, 0x5F)) { sound_play(59); m.w8(m.r32(kSelector), 2); m.w8(m.r32(kSelector) + 1, 1); }
    const u32 unlocks = scene(m) + 3400;
    m.w8(flags(m) + 52, m.r8(m.r32(kSelector) + 3));
    for (u32 i = 0; i < 4; ++i) {
        const u32 current = m.r32(kSelector), index = 4 * m.r8(current + 2) + i;
        const bool present = static_cast<s32>(index) < m.r32s(current + 596);
        if (present) {
            m.w8(flags(m) + 53 + i, m.r8(unlocks + m.r8(current + 516 + index))); m.w8(flags(m) + 57 + i, 1);
        } else { m.w8(flags(m) + 53 + i, 0); m.w8(flags(m) + 57 + i, 0); }
    }
    return 0;
}

u32 Game::o2_panel_grid_input() {
    cover(0x800D0C34u);
    Frame frame(*this, 32);
    const u32 drive = kcd_drive_state();
    o2_group_set(0, 11); o2_grid_navigate(true);
    if (pressed(m, 16, 0x2F) && drive != 1) {
        for (u32 fade = 1; fade <= 5; ++fade) o2_fade_set(fade, 0, 0, 0);
        o2_flag(48, 1);
        const u32 menu = m.r32(kMenu), current = m.r32(kPanel), index = 4 * m.r8(current + 2) + m.r8(current + 3);
        m.w16(menu + 134, 0); m.w8(menu + 125, 4); m.w16(menu + 292, m.r8(current + 4 + index));
    }
    if (pressed(m, 32, 0x5F) && drive != 1) { m.w8(m.r32(kMenu) + 125, 5); m.w16(m.r32(kMenu) + 134, 0); }
    const u32 unlocks = scene(m) + 3480;
    m.w8(flags(m) + 52, m.r8(m.r32(kPanel) + 3));
    for (u32 i = 0; i < 4; ++i) {
        const u32 current = m.r32(kPanel), index = 4 * m.r8(current + 2) + i;
        const bool present = static_cast<s32>(index) < m.r32s(current + 184);
        if (present) {
            m.w8(flags(m) + 53 + i, m.r8(unlocks + m.r8(current + 4 + index))); m.w8(flags(m) + 57 + i, 2);
        } else { m.w8(flags(m) + 53 + i, 0); m.w8(flags(m) + 57 + i, 0); }
    }
    return 0;
}
}

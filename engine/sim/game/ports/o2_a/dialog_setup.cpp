#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kDialog = 0x80112DDCu, kSelector = 0x80112DD0u;
constexpr u32 kView = 0x800BCE48u, kEntryTables = 364, kSceneFlags = 2412;
}

u32 Game::o2_metadata_label(u32 label) {
    cover(0x800D32FCu);
    if (label == 19 && m.r8(m.r32(kDialog) + 180) == 1) return 21;
    if (label == 14 && m.r8(m.r32(kDialog) + 181) == 1) return 22;
    if (label == 15 && m.r8(m.r32(kDialog) + 181) == 1) return 23;
    return label;
}

void Game::o2_dialog_phase_six() {
    cover(0x800DCB5Cu);
    Frame frame(*this, 24);
    const u32 dialog = m.r32(kDialog);
    m.w8(dialog + 1, m.r8(dialog + 1) + 1);
    const u32 menu = m.r32(kMenu);
    m.w16(m.r32(menu + kEntryTables) + 190, 1);
    m.w16(menu + 132, 6); m.w16(menu + 134, 0);
    m.w32(menu + 224, (m.r32(m.r32(g::kScene) + kSceneFlags) & 16) ? 3 : 1);
    o2_group_set(0, 0x8000);
}

void Game::o2_selector_screen_reset() {
    cover(0x800D04F4u);
    Frame frame(*this, 24);
    m.w8(m.r32(kSelector), 0);
    o2_entry_reset(0, 3);
    o2_group_set(0, 11);
    o2_fade_set(5, 0, 0, 0);
    o2_fade_set(3, 0, 0, 0);
    o2_fade_set(4, 0, 0, 0);
    o2_flag(1, m.r8(m.r32(kMenu) + 126) + 1);
}

void Game::o2_metadata_font_dispatch() {
    cover(0x800D3240u);
    Frame frame(*this, 24);
    if (m.r16s(m.r32(kDialog) + 12) == 1) o2_metadata_font_first();
    if (m.r16s(m.r32(kDialog) + 12) == 2) o2_metadata_font_second();
    if (m.r16s(m.r32(kDialog) + 12) == 3) o2_metadata_font_third();
}

void Game::o2_hangar_backdrop_restore() {
    cover(0x800DE4BCu);
    Frame frame(*this, 24);
    const u32 menu = m.r32(kMenu);
    m.w8(menu + 125, m.r8(menu + 125) + 1);
    m.w8(m.r32(kView) + 16, 143);
    m.w8(m.r32(kView) + 17, 135);
    m.w8(m.r32(kView) + 18, 83);
    const u32 view = m.r32(kView);
    m.w32(view, m.r32(view) | 8);
    o2_hangar_effect1_set(2);
    m.w16(m.r32(kMenu) + 454, 10);
}
}

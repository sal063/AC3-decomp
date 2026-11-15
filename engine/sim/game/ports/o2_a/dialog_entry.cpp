#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kDialog = 0x80112DDCu;
constexpr u32 kPressedButtons = 300, kHeldButtons = 296, kEntryMode = 2399;
}

void Game::o2_dialog_page_begin() {
    cover(0x800D240Cu);
    Frame frame(*this, 24);
    o2_upload_pack(m.r32(m.r32(kMenu) + 152));
    o2_group_set(0, 3);
    if (m.r8(m.r32(kDialog) + 5) == 0) {
        o2_group_set(2, 1); m.w8(m.r32(kDialog) + 1, 1);
    } else {
        o2_group_set(2, 2); m.w8(m.r32(kDialog) + 1, 4);
    }
    o2_fade_set(1, 1, 0, 40); o2_fade_set(2, 1, 0, 40);
    o2_flag(1, 4); o2_flag(5, m.r8(m.r32(kDialog) + 4)); o2_flag(62, 1);
}

void Game::o2_options_dialog_begin() {
    cover(0x800DA4F4u);
    Frame frame(*this, 24);
    o2_upload_pack(m.r32(m.r32(kMenu) + 160));
    m.w8(m.r32(kDialog) + 1, 1);
    o2_entry_reset(2, 12);
    o2_group_set(0, 1); o2_group_set(2, 0x1000);
    m.w8(m.r32(kDialog) + 256, 0);
    m.w8(m.r32(kDialog) + 257, m.r8(m.r32(g::kScene) + 922) & 1);
    m.w8(m.r32(kDialog) + 258, m.r8(m.r32(g::kScene) + 920));
    m.w8(m.r32(kDialog) + 259, m.r8(m.r32(g::kScene) + 921));
    m.w8(m.r32(kDialog) + 3, 0);
    o2_flag(5, m.r8(m.r32(kDialog) + 6)); o2_flag(6, 0);
    o2_fade_set(1, 1, 0, 30);
    o2_voice_script(0x8010AC60u);
}

void Game::o2_dialog_launch_input() {
    cover(0x800DE078u);
    Frame frame(*this, 24);
    u32 scene = m.r32(g::kScene);
    if ((m.r32(scene + kPressedButtons) & 16) && !(m.r32(scene + kHeldButtons) & 0x2F)) {
        m.w32(m.r32(kMenu) + 284, 1);
        m.w8(scene + kEntryMode, 1);
        m.w8(m.r32(g::kScene) + 936, m.r8(0x8010A3A8u));
    }
    scene = m.r32(g::kScene);
    if ((m.r32(scene + kPressedButtons) & 32) && !(m.r32(scene + kHeldButtons) & 0x5F)) {
        o2_entry_enter(0, 1);
        m.w8(m.r32(kDialog), 0); m.w8(m.r32(kDialog) + 1, 0);
        const u32 menu = m.r32(kMenu);
        m.w16(menu + 132, 0); m.w16(menu + 134, 0);
        m.w8(m.r32(g::kScene) + kEntryMode, 0);
        heap_set(m.r32(m.r32(kMenu) + 40));
        m.w32(m.r32(g::kScene) + kPressedButtons, 0);
    }
    m.w32(m.r32(g::kWorld) + 4, 0);
}
}

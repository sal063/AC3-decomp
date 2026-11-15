#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kScreens = 0x80112DC0u, kDialog = 0x80112DDCu;
constexpr u32 kHeld = 296, kPressed = 300, kRepeat = 304;
u32 flags(Memory& m) { return m.r32(m.r32(kScreens) + 92); }
}

void Game::o2_settings_flags(u32 settings, bool showDirection) {
    const u32 row = m.r8(settings + 3);
    if (row < 3) m.w8(flags(m) + 50, m.r8(settings + row) + (row == 0 ? 0 : row == 1 ? 2 : 5));
    u32 direction = 0;
    if (showDirection) {
        const u32 held = m.r32(m.r32(g::kScene) + kHeld);
        direction = (held & 1) ? 1 : (held & 2) ? 2 : 0;
    }
    m.w8(flags(m) + 51, direction);
    for (u32 i = 0; i < 4; ++i) m.w8(flags(m) + 52 + i, m.r8(settings + i));
}

u32 Game::o2_settings_menu_begin(u32 settings) {
    cover(0x800DBAECu);
    Frame frame(*this, 24);
    m.w8(settings, m.r8(m.r32(g::kScene) + 924) == 1 ? 0 : 1);
    const u32 controlFlags = m.r32(m.r32(g::kScene) + 288);
    m.w8(settings + 1, (controlFlags & 1) ? ((controlFlags & 2) ? 0 : 1) : 2);
    const u32 last = m.r8(m.r32(g::kScene) + 2394);
    if (last <= 1) m.w8(settings + 2, last);
    m.w8(settings + 3, 0);
    o2_entry_reset(2, 16); o2_group_set(2, 0x10000);
    o2_settings_flags(settings, false);
    return 1;
}

u32 Game::o2_settings_menu_input(u32 settings) {
    cover(0x800DBC94u);
    Frame frame(*this, 32);
    const u32 repeat = m.r32(m.r32(g::kScene) + kRepeat);
    if (repeat & 15) {
        sound_play(61);
        if (repeat & 2) m.w8(settings + 3, (m.r8(settings + 3) + 1) % 3);
        if (repeat & 1) m.w8(settings + 3, (m.r8(settings + 3) + 2) % 3);
        if (repeat & 12) {
            switch (m.r8(settings + 3)) {
            case 0: m.w8(settings, m.r8(settings) ^ 1); break;
            case 1: m.w8(settings + 1, (m.r8(settings + 1) + ((repeat & 8) ? 1 : 2)) % 3); break;
            case 2: m.w8(settings + 2, m.r8(settings + 2) == 0 ? 1 : 0); break;
            default: break;
            }
        }
    }
    u32 scene = m.r32(g::kScene);
    if ((m.r32(scene + kPressed) & 16) && !(m.r32(scene + kHeld) & 0x2F)) {
        sound_play(60); return 1;
    }
    scene = m.r32(g::kScene);
    if ((m.r32(scene + kPressed) & 32) && !(m.r32(scene + kHeld) & 0x5F)) {
        sound_play(59); m.w32(m.r32(g::kScene) + kPressed, 0); return 2;
    }
    o2_settings_flags(settings, true);
    return 0;
}

void Game::o2_settings_dialog_tick() {
    cover(0x800DC0A0u);
    Frame frame(*this, 24);
    const u32 dialog = m.r32(kDialog), settings = dialog + 314;
    const u32 result = call_guest(m.r32(0x8010ACACu + 4 * m.r32(dialog + 320)), settings, 0);
    if (result == 1) {
        const u32 current = m.r32(kDialog);
        m.w32(current + 320, m.r32(current + 320) + 1);
    } else if (result == 2) {
        const u32 current = m.r32(kDialog);
        m.w8(current + 1, 1); m.w32(current + 320, 0);
        o2_fade_set(1, 1, 0, 30); o2_group_set(2, 0x1000);
    }
}
}

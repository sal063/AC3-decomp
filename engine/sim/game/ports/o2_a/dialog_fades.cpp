#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kDialog = 0x80112DDCu, kHangar = 0x80112DA8u;
constexpr u32 kSceneMode = 2388, kSceneFlags = 2412, kDialogTick = 420;
constexpr u32 kHeldButtons = 296, kPressedButtons = 300;
}

u32 Game::o2_hangar_mode_begin() {
    cover(0x800CDBF0u);
    Frame frame(*this, 24);
    const s32 mode = m.r16s(m.r32(g::kScene) + kSceneMode);
    if (mode == 0 || mode == 3) o2_hangar_fly();
    const u32 scene = m.r32(g::kScene);
    if (m.r16s(scene + kSceneMode) == 2 || (m.r32(scene + kSceneFlags) & 32))
        o2_hangar_fly();
    return 1;
}

u32 Game::o2_hangar_mode_active() {
    cover(0x800CDC68u);
    const u32 scene = m.r32(g::kScene);
    const s32 mode = m.r16s(scene + kSceneMode);
    if (mode == 0 || mode == 2 || mode == 3 || (m.r32(scene + kSceneFlags) & 32))
        return m.r32(m.r32(kHangar) + 20) == 1 ? 2 : 0;
    return 0;
}

void Game::o2_dialog_first_fade_tick() {
    cover(0x800D24E8u);
    Frame frame(*this, 24);
    if (o2_entry_state(0, 1) == 0) {
        const u32 menu = m.r32(kMenu);
        m.w32(menu + kDialogTick, m.r32(menu + kDialogTick) + 1);
    }
    if (o2_fade_state(1) == 2) m.w8(m.r32(kDialog) + 1, 2);
    o2_flag(5, m.r8(m.r32(kDialog) + 4));
}

void Game::o2_dialog_second_fade_tick() {
    cover(0x800D2D74u);
    Frame frame(*this, 24);
    if (o2_entry_state(0, 1) == 0) {
        const u32 menu = m.r32(kMenu);
        m.w32(menu + kDialogTick, m.r32(menu + kDialogTick) + 1);
    }
    if (o2_fade_state(1) == 2) m.w8(m.r32(kDialog) + 1, 5);
    o2_flag(5, m.r8(m.r32(kDialog) + 5));
}

void Game::o2_dialog_confirm_fade() {
    cover(0x800DA5ECu);
    Frame frame(*this, 24);
    const u32 scene = m.r32(g::kScene);
    if ((m.r32(scene + kPressedButtons) & 16) && !(m.r32(scene + kHeldButtons) & 0x2F))
        o2_fade_set(1, 2, 30, 30);
    if (o2_fade_state(1) == 2) m.w8(m.r32(kDialog) + 1, 2);
}

void Game::o2_dialog_fade_reset() {
    cover(0x800DCDC4u);
    Frame frame(*this, 24);
    if (o2_fade_state(1) == 2) {
        m.w8(m.r32(kDialog), 0);
        m.w8(m.r32(kDialog) + 1, 0);
    }
}
}

#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kState = 0x800DD9D0u;
constexpr u32 kSubstate = 76, kTimer = 78, kSelectedCard = 90;
constexpr u32 kConfirm = 16, kCancel = 32;
bool unavailable(s32 status) { return status == 2 || status == 6; }
}

u32 Game::o1_account_empty_panel_choice() {
    cover(0x800D076Cu);
    return o1_account_new_choice();
}
u32 Game::o1_account_saved_panel_choice() {
    cover(0x800D0A10u);
    return o1_account_new_choice();
}
u32 Game::o1_account_alternate_panel_choice() {
    cover(0x800D0CECu);
    return o1_account_new_choice();
}
u32 Game::o1_title_missing_account_choice() {
    cover(0x800CFEA4u);
    Frame frame(*this, 24);
    const s32 first = t1_card_status(0), second = t1_card_status(1);
    if (!unavailable(first) || !unavailable(second)) {
        const u32 state = m.r32(kState);
        m.w16(state + kSubstate, 9); m.w16(state + kTimer, 0); return 9;
    }
    const u32 buttons = m.r32(m.r32(g::kScene) + 300);
    if (!(buttons & (kConfirm | kCancel))) return 0;
    const bool confirmed = (buttons & kConfirm) != 0;
    const u32 state = m.r32(kState);
    m.w16(state + kSubstate, confirmed ? 19 : 7); m.w16(state + kTimer, 0);
    if (confirmed) {
        t1_stats_reset(); m.w16(m.r32(g::kScene) + 2388, 2);
        const u32 clock = t1_clock();
        m.w32(m.r32(g::kScene) + 2408, clock); o1_save_name_to_scene();
    }
    return sound_play(confirmed ? 89 : 59);
}
u32 Game::o1_unavailable_card_menu_begin() {
    cover(0x800D1B2Cu);
    Frame frame(*this, 24);
    const s32 first = t1_card_status(0), second = t1_card_status(1);
    if (!unavailable(first) || !unavailable(second)) {
        const u32 state = m.r32(kState);
        m.w16(state + kSubstate, 2); m.w16(state + kTimer, 0); return 2;
    }
    o1_text_item_enable(8); o1_screen_text_select(318); o1_fade_set(4, 1, 0, 60);
    const u32 state = m.r32(kState);
    m.w16(state + kTimer, m.r16(state + kTimer) + 1);
    o1_animation_command(1, 2); o1_animation_command(6, 2); o1_animation_command(7, 2);
    o1_animation_history_push(3); m.w8(m.r32(kState) + 208, 1);
    return o1_animation_command(4, 5);
}
u32 Game::o1_title_saved_card_panel_begin() {
    cover(0x800CFF90u);
    Frame frame(*this, 32);
    const s32 first = t1_card_status(0), second = t1_card_status(1);
    const u32 state = m.r32(kState);
    m.w16(state + kTimer, m.r16(state + kTimer) + 1);
    o1_text_item_enable(18); o1_screen_text_select(0x40036); o1_fade_set(4, 1, 0, 60);
    o1_animation_command(1, 25); o1_animation_command(4, 2);
    o1_marker_set(64, 50, 1, 0); o1_animation_history_push(24);
    if (first != 5 && second == 5) m.w8(m.r32(kState) + kSelectedCard, 1);
    const u32 current = m.r32(kState);
    m.w8(current + 209, 0); return current;
}
u32 Game::o1_save_progress_panel_begin() {
    cover(0x800D2600u);
    Frame frame(*this, 24);
    const u32 state = m.r32(kState), timer = m.r16(state + kTimer);
    m.w32(state + 72, 0); m.w8(state + 211, 0); m.w16(state + kTimer, timer + 1);
    o1_screen_text_select(190); o1_fade_set(1, 1, 0, 20);
    o1_fade_set(2, 3, 0xFFFFFFFFu, 0xFFFFFFFFu);
    o1_fade_set(4, 3, 0xFFFFFFFFu, 0xFFFFFFFFu);
    o1_fade_set(9, 3, 0xFFFFFFFFu, 0xFFFFFFFFu);
    o1_fade_set(1, 1, 0, 60); o1_fade_set(5, 1, 0, 60); o1_fade_set(10, 1, 0, 60);
    o1_marker_mode(1); o1_animation_command(1, 9); o1_animation_command(4, 3);
    return o1_animation_history_push(9);
}
u32 Game::o1_mission_feature_complete() {
    cover(0x800D2BA0u);
    const u32 pilot = m.r32(g::kScene) + 0x95C;
    constexpr u32 groups[] = {0x2E2, 0x2E7, 0x2D2, 0x2F5, 0x2F0};
    for (const u32 group : groups) {
        u32 flags = 0;
        for (u32 route = 0; route < 4; ++route) flags |= m.r8(pilot + group + 60 * route);
        if (!flags) return 0;
    }
    return 1;
}
u32 Game::o1_disc_choice_poll() {
    cover(0x800D1618u);
    Frame frame(*this, 24);
    const s32 disc = disc_number();
    if (disc == 0) o1_animation_command(1, 73);
    if (disc == 1 || disc == 3) {
        const u32 state = m.r32(kState);
        m.w16(state + 12, 6); m.w16(state + 14, 0); return 6;
    }
    if (disc == 4 || disc == -1 || disc == 2) {
        o1_animation_command(1, m.r32(m.r32(kState) + 212) + 70);
        kcd_open_drive();
        const u32 state = m.r32(kState);
        m.w16(state + kTimer, m.r16(state + kTimer) + 1);
        return o1_animation_history_push(37);
    }
    return 2;
}
}

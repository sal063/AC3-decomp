#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kState = 0x800DD9D0u;
constexpr u32 kSubstate = 76, kTimer = 78, kSelectedSlot = 90;
constexpr u32 kConfirm = 16, kCancel = 32, kFormat = 64;
bool cardUnavailable(s32 status) { return status == 2 || status == 6; }
}

u32 Game::o1_title_new_account_choice() {
    cover(0x800CFC94u);
    Frame frame(*this, 24);
    const u32 buttons = m.r32(m.r32(g::kScene) + 300);
    if (!(buttons & (kConfirm | kCancel))) return 0x800E0000u;
    const bool confirmed = (buttons & kConfirm) != 0;
    const u32 state = m.r32(kState);
    m.w16(state + kSubstate, confirmed ? 19 : 0); m.w16(state + kTimer, 0);
    if (confirmed) {
        t1_stats_reset(); m.w16(m.r32(g::kScene) + 2388, 2);
        const u32 clock = t1_clock();
        m.w32(m.r32(g::kScene) + 2408, clock); o1_save_name_to_scene();
    }
    return sound_play(confirmed ? 89 : 59);
}
u32 Game::o1_title_button_choice() {
    cover(0x800CEF44u);
    Frame frame(*this, 24);
    const u32 scene = m.r32(g::kScene), enabled = m.r32(scene + 296), pressed = m.r32(scene + 300);
    if ((enabled & pressed & kConfirm) || (enabled & pressed & kCancel)) {
        const bool confirmed = (enabled & pressed & kConfirm) != 0;
        const u32 state = m.r32(kState);
        m.w16(state + kSubstate, confirmed ? 4 : 7); m.w16(state + kTimer, 0);
        sound_play(confirmed ? 89 : 59);
    }
    const u32 state = m.r32(kState), progress = m.r32(state + 72) + 1;
    m.w32(state + 72, progress);
    return progress;
}
u32 Game::o1_account_create_choice() {
    cover(0x800D1488u);
    Frame frame(*this, 32);
    if (cardUnavailable(t1_card_status(m.r8(m.r32(kState) + kSelectedSlot)))) {
        const u32 state = m.r32(kState);
        m.w16(state + kSubstate, 9); m.w16(state + kTimer, 0); return 9;
    }
    o1_animation_command(1, 40);
    const u32 buttons = m.r32(m.r32(g::kScene) + 300);
    if (buttons & kConfirm) {
        const u32 state = m.r32(kState);
        m.w16(state + kSubstate, 19); m.w16(state + kTimer, 0);
        t1_stats_reset();
        const u32 scene = m.r32(g::kScene);
        m.w16(scene + 2388, 2); m.w16(m.r32(kState) + 16, 0);
        return scene;
    }
    if (buttons & kCancel) {
        const u32 state = m.r32(kState);
        m.w16(state + kSubstate, 11); m.w16(state + kTimer, 0);
    }
    return 11;
}
u32 Game::o1_saved_account_chooser_begin() {
    cover(0x800D1CF0u);
    Frame frame(*this, 32);
    const s32 first = t1_card_status(0), second = t1_card_status(1);
    const u32 state = m.r32(kState);
    m.w16(state + kTimer, m.r16(state + kTimer) + 1);
    o1_text_item_enable(8); o1_screen_text_select(318); o1_fade_set(4, 1, 0, 60);
    o1_animation_command(1, 3); o1_animation_command(4, 2); o1_animation_history_push(4);
    m.w8(m.r32(kState) + 209, 0);
    if (first == 5) {
        if (second == 5) return 5;
        const u32 current = m.r32(kState);
        m.w8(current + kSelectedSlot, 0); return current;
    }
    if (second == 5) m.w8(m.r32(kState) + kSelectedSlot, 1);
    return 1;
}
u32 Game::o1_cards_unavailable_poll() {
    cover(0x800D1C18u);
    Frame frame(*this, 24);
    const s32 first = t1_card_status(0), second = t1_card_status(1);
    if (!cardUnavailable(first) || !cardUnavailable(second)) {
        const u32 state = m.r32(kState);
        m.w16(state + kSubstate, 0); m.w16(state + kTimer, 0); m.w8(state + 208, 0);
        o1_animation_command(1, 7);
        return o1_animation_command(4, 5);
    }
    if (!(m.r32(m.r32(g::kScene) + 300) & kCancel)) return 0;
    const u32 state = m.r32(kState);
    m.w16(state + 12, 2); m.w16(state + 14, 0);
    o1_screen_text_select(158); sound_play(89);
    const u32 current = m.r32(kState);
    m.w8(current + 208, 0);
    return current;
}
u32 Game::o1_account_format_choice() {
    cover(0x800D0548u);
    Frame frame(*this, 24);
    const s32 status = t1_card_status(m.r8(m.r32(kState) + kSelectedSlot));
    if (cardUnavailable(status)) {
        const u32 state = m.r32(kState);
        m.w16(state + kSubstate, 9); m.w16(state + kTimer, 0); return 9;
    }
    if (status == 0) return 0x800C0000u;
    const u32 buttons = m.r32(m.r32(g::kScene) + 300);
    if (buttons & kFormat) {
        o1_animation_command(1, 32);
        const u32 state = m.r32(kState);
        m.w16(state + kTimer, m.r16(state + kTimer) + 1);
        sound_play(89);
        return o1_animation_history_push(28);
    }
    if (buttons & kCancel) {
        const u32 state = m.r32(kState);
        m.w16(state + kSubstate, 7); m.w16(state + kTimer, 0);
        return sound_play(59);
    }
    return 0;
}
u32 Game::o1_save_dialog_choice() {
    cover(0x800D2AE8u);
    Frame frame(*this, 32);
    const s32 status = t1_card_status(m.r8(m.r32(kState) + kSelectedSlot));
    m.w8(m.r32(kState) + 208, 1);
    if (m.r32(m.r32(g::kScene) + 300) & kCancel) {
        const u32 state = m.r32(kState);
        m.w16(state + kSubstate, 2); m.w16(state + kTimer, 0);
        sound_play(89); m.w8(m.r32(kState) + 208, 0); o1_marker_mode(0);
    }
    if (!cardUnavailable(status)) return 6;
    const u32 state = m.r32(kState);
    m.w16(state + 12, 2); m.w16(state + 14, 0); m.w8(state + 208, 0);
    return o1_marker_mode(0);
}
u32 Game::o1_title_unavailable_cards_begin() {
    cover(0x800CFDCCu);
    Frame frame(*this, 24);
    const s32 first = t1_card_status(0), second = t1_card_status(1);
    if (!cardUnavailable(first) || !cardUnavailable(second)) {
        const u32 state = m.r32(kState);
        m.w16(state + kSubstate, 11); m.w16(state + kTimer, 0); return 11;
    }
    o1_fade_set(1, 3, 0xFFFFFFFFu, 0xFFFFFFFFu); o1_fade_set(2, 1, 0, 20);
    const u32 state = m.r32(kState);
    m.w16(state + kTimer, m.r16(state + kTimer) + 1);
    o1_animation_command(1, 24); o1_animation_history_push(23);
    m.w8(m.r32(kState) + 209, 1);
    return o1_screen_text_select(0x20056);
}
u32 Game::o1_account_missing_panel_choice() {
    cover(0x800D0384u);
    return o1_account_new_choice();
}
u32 Game::o1_account_new_choice() {
    Frame frame(*this, 32);
    if (cardUnavailable(t1_card_status(m.r8(m.r32(kState) + kSelectedSlot)))) {
        const u32 state = m.r32(kState);
        m.w16(state + kSubstate, 9); m.w16(state + kTimer, 0); return 9;
    }
    const u32 buttons = m.r32(m.r32(g::kScene) + 300);
    if (buttons & kConfirm) {
        const u32 state = m.r32(kState);
        m.w16(state + kSubstate, 19); m.w16(state + kTimer, 0);
        t1_stats_reset(); m.w16(m.r32(g::kScene) + 2388, 2);
        const u32 clock = t1_clock();
        m.w32(m.r32(g::kScene) + 2408, clock); o1_save_name_to_scene();
        return sound_play(89);
    }
    if (buttons & kCancel) {
        const u32 state = m.r32(kState);
        m.w16(state + kSubstate, 7); m.w16(state + kTimer, 0);
        return sound_play(59);
    }
    return 0;
}
u32 Game::o1_account_unformatted_panel_begin() {
    cover(0x800D0460u);
    Frame frame(*this, 24);
    if (t1_card_status(m.r8(m.r32(kState) + kSelectedSlot)) != 3) {
        m.w16(m.r32(kState) + kSubstate, 14); return 14;
    }
    const u32 state = m.r32(kState);
    m.w16(state + kTimer, m.r16(state + kTimer) + 1);
    o1_text_item_enable(19); o1_screen_text_select(0x80056);
    for (u32 fade = 1; fade <= 3; ++fade) o1_fade_set(fade, 3, 0xFFFFFFFFu, 0xFFFFFFFFu);
    o1_fade_set(4, 1, 0, 20);
    o1_animation_command(1, 30); o1_animation_command(4, 21);
    return o1_animation_history_push(27);
}
}

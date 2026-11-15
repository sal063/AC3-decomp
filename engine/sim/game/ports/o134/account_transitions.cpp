#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kScreens = 0x800DD9DCu, kState = 0x800DD9D0u;
constexpr u32 kSubstate = 76, kTimer = 78, kSelectedCard = 90;
constexpr u32 kPromptActive = 208, kDialogSelection = 212;
}

u32 Game::o1_marker_set(u32 x, u32 y, u32 visible, u32 highlighted) {
    cover(0x800DBF6Cu);
    const u32 screens = m.r32(kScreens);
    m.w16(screens + 80, x); m.w16(screens + 82, y);
    const u32 flags = m.r32(screens + 84);
    m.w32(screens + 84, visible == 1 ? flags | 1u : flags & ~1u);
    const u32 current = m.r32(kScreens), updated = m.r32(current + 84);
    const u32 result = highlighted == 1 ? updated | 4u : updated & ~4u;
    m.w32(current + 84, result);
    return result;
}
u32 Game::o1_marker_mode(u32 mode) {
    cover(0x800DC028u);
    const u32 screens = m.r32(kScreens), flags = m.r32(screens + 84);
    const u32 result = mode == 2 ? flags | 10u : mode == 1 ? (flags | 2u) & ~8u : flags & ~10u;
    m.w32(screens + 84, result);
    return result;
}
u32 Game::o1_drive_prompt_begin() {
    cover(0x800D16D4u);
    Frame frame(*this, 24);
    o1_screen_text_select(0x00400036u);
    o1_animation_command(4, 4); o1_marker_mode(0);
    if (kcd_drive_state() != 3) return 0x800E0000u;
    const u32 state = m.r32(kState), timer = m.r16(state + kTimer) + 1;
    m.w16(state + kTimer, timer);
    return timer;
}
u32 Game::o1_account_marker_begin() {
    cover(0x800D15C4u);
    Frame frame(*this, 24);
    const u32 state = m.r32(kState);
    m.w32(state + kDialogSelection, 0);
    m.w16(state + kTimer, m.r16(state + kTimer) + 1);
    m.w16(m.r32(g::kScene) + 908, 1);
    return o1_marker_set(111, 150, 0, 1);
}
u32 Game::o1_title_cards_poll_finish() {
    cover(0x800CFD60u);
    Frame frame(*this, 24);
    const s32 first = t1_card_status(0), second = t1_card_status(1);
    if (first == 0 || first == 6 || second == 0 || second == 6) return static_cast<u32>(second);
    const u32 state = m.r32(kState);
    m.w16(state + kSubstate, 10); m.w16(state + kTimer, 0);
    return o1_animation_command(4, 21);
}
u32 Game::o1_drive_prompt_poll() {
    cover(0x800D1738u);
    Frame frame(*this, 24);
    const u32 drive = kcd_drive_state();
    if (drive != 1) return drive;
    o1_animation_command(1, 73);
    const u32 state = m.r32(kState);
    m.w16(state + kTimer, m.r16(state + kTimer) - 2);
    o1_animation_history_push(38);
    m.w32(m.r32(kState) + kDialogSelection, 31);
    return 31;
}
u32 Game::o1_account_cards_begin() {
    cover(0x800D1798u);
    Frame frame(*this, 24);
    const u32 state = m.r32(kState);
    m.w32(state + 72, 0); m.w16(state + 16, 0); m.w16(state + 14, 1);
    const u32 work = mem_alloc(4);
    m.w32(0x800DDA30u, work); m.w8(work, 0);
    m.w8(m.r32(0x800DDA30u) + 1, 0);
    m.w8(m.r32(kState) + 210, 0);
    const u32 current = m.r32(kState);
    m.w16(current + kSubstate, 0); m.w16(current + kTimer, 0);
    o1_card_directory_reset(); t1_card_states_reset(); o1_empty_5548();
    return 0;
}
u32 Game::o1_account_drive_prompt_begin() {
    cover(0x800D3A24u);
    Frame frame(*this, 24);
    o1_screen_text_select(0x00400036u); o1_animation_command(4, 4);
    m.w8(m.r32(kState) + 211, 0);
    const u32 drive = kcd_drive_state();
    if (drive != 3) return drive;
    const u32 state = m.r32(kState), timer = m.r16(state + kTimer) + 1;
    m.w16(state + kTimer, timer);
    return timer;
}
u32 Game::o1_card_selection_fade_begin() {
    cover(0x800D1A5Cu);
    Frame frame(*this, 24);
    o1_fade_set(2, 1, 0, 60); o1_fade_set(4, 1, 0, 60); o1_fade_set(9, 1, 0, 60);
    const u32 state = m.r32(kState);
    m.w16(state + kTimer, m.r16(state + kTimer) + 1);
    o1_animation_command(1, 7);
    return o1_animation_command(4, 5);
}
u32 Game::o1_account_confirmation_begin() {
    cover(0x800D34FCu);
    Frame frame(*this, 24);
    const u32 state = m.r32(kState);
    m.w16(state + kTimer, m.r16(state + kTimer) + 1);
    o1_screen_text_select(1086); o1_fade_set(15, 1, 0, 60);
    o1_animation_command(1, 11); o1_animation_command(4, 6);
    o1_marker_set(64, 50, 1, 0); o1_animation_history_push(11);
    return o1_animation_command(4, 6);
}
u32 Game::o1_account_marker_reset() {
    cover(0x800D37FCu);
    Frame frame(*this, 24);
    const u32 state = m.r32(kState);
    m.w32(state + kDialogSelection, 0);
    m.w16(state + kTimer, m.r16(state + kTimer) + 1);
    return o1_marker_set(111, 150, 0, 0);
}
u32 Game::o1_account_exit_panel_begin() {
    cover(0x800D3AE8u);
    Frame frame(*this, 24);
    const u32 state = m.r32(kState);
    m.w32(state + 72, 0); m.w16(state + 14, 1);
    m.w16(state + kSubstate, 0); m.w16(state + kTimer, 0); m.w8(state + 211, 0);
    o1_marker_set(111, 148, 0, 0);
    return o1_animation_history_push(3);
}
u32 Game::o1_card_format_prompt_begin() {
    cover(0x800D2154u);
    Frame frame(*this, 24);
    if (t1_card_status(m.r8(m.r32(kState) + kSelectedCard)) != 3) {
        m.w16(m.r32(kState) + kSubstate, 5); return 5;
    }
    const u32 state = m.r32(kState);
    m.w16(state + kTimer, m.r16(state + kTimer) + 1);
    o1_animation_command(1, 5); o1_animation_history_push(7);
    m.w8(m.r32(kState) + kPromptActive, 1);
    return 1;
}
u32 Game::o1_card_empty_prompt_begin() {
    cover(0x800D2268u);
    Frame frame(*this, 24);
    if (t1_card_status(m.r8(m.r32(kState) + kSelectedCard)) != 4) {
        m.w16(m.r32(kState) + kSubstate, 6); return 6;
    }
    const u32 state = m.r32(kState);
    m.w16(state + kTimer, m.r16(state + kTimer) + 1);
    o1_animation_command(1, 6); o1_animation_history_push(8);
    m.w8(m.r32(kState) + kPromptActive, 1);
    return 1;
}
u32 Game::o1_card_missing_prompt_begin() {
    cover(0x800D202Cu);
    Frame frame(*this, 32);
    const s32 status = t1_card_status(m.r8(m.r32(kState) + kSelectedCard));
    o1_animation_command(1, 7);
    if (status != 1) { m.w16(m.r32(kState) + kSubstate, 4); return 4; }
    const u32 state = m.r32(kState);
    m.w16(state + kTimer, m.r16(state + kTimer) + 1);
    o1_animation_command(1, 4); o1_animation_history_push(6);
    const u32 current = m.r32(kState);
    m.w8(current + kPromptActive, 1);
    return current;
}

u32 Game::o1_card_prompt_poll(u32 cancelledSubstate) {
    Frame frame(*this, 24);
    const s32 status = t1_card_status(m.r8(m.r32(kState) + kSelectedCard));
    const bool unavailable = status == 2 || status == 6;
    if (!unavailable && !(m.r32(m.r32(g::kScene) + 300) & 32)) return 0;
    const u32 state = m.r32(kState);
    m.w16(state + kSubstate, unavailable ? 0 : cancelledSubstate); m.w16(state + kTimer, 0);
    if (!unavailable) sound_play(89);
    const u32 current = unavailable ? state : m.r32(kState);
    m.w8(current + kPromptActive, 0);
    return current;
}
u32 Game::o1_card_missing_prompt_poll() { cover(0x800D20BCu); return o1_card_prompt_poll(0); }
u32 Game::o1_card_format_prompt_poll() { cover(0x800D21D0u); return o1_card_prompt_poll(0); }
u32 Game::o1_card_empty_prompt_poll() { cover(0x800D22E4u); return o1_card_prompt_poll(2); }

u32 Game::o1_account_timeout_poll() {
    cover(0x800D1554u);
    Frame frame(*this, 24);
    o1_animation_command(1, 42);
    const u32 state = m.r32(kState), remaining = m.r32(state + 136);
    if (static_cast<s32>(remaining) < 0 || (m.r32(m.r32(g::kScene) + 300) & 48)) {
        m.w16(state + kSubstate, 19); m.w16(state + kTimer, 0); return 19;
    }
    m.w32(state + 136, remaining - 1);
    return remaining - 1;
}
u32 Game::o1_card_formatted_poll() {
    cover(0x800D0668u);
    Frame frame(*this, 24);
    if (t1_card_status(m.r8(m.r32(kState) + kSelectedCard)) != 4) {
        m.w16(m.r32(kState) + kSubstate, 15);
        return o1_animation_command(1, 26);
    }
    const u32 state = m.r32(kState), timer = m.r16(state + kTimer) + 1;
    m.w16(state + kTimer, timer);
    return timer;
}
}

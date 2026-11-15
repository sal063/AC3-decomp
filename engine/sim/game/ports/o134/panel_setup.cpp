#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kState = 0x800DD9D0u, kScreens = 0x800DD9DCu;
constexpr u32 kTimer = 78, kSubstate = 76, kProgress = 72;
}

u32 Game::o1_title_panel15_begin() {
    cover(0x800CF750u);
    Frame frame(*this, 24);
    const u32 state = m.r32(kState);
    m.w16(state + kTimer, m.r16(state + kTimer) + 1);
    o1_text_item_enable(15); o1_screen_text_select(0x8010);
    o1_animation_command(1, 20); o1_marker_set(64, 50, 0, 0);
    return o1_animation_history_push(19);
}
u32 Game::o1_title_panel16_begin() {
    cover(0x800CFA34u);
    Frame frame(*this, 24);
    const u32 state = m.r32(kState);
    m.w16(state + kTimer, m.r16(state + kTimer) + 1);
    o1_screen_text_select(0x10016);
    o1_animation_command(1, 19); o1_marker_set(64, 50, 1, 0);
    return o1_animation_history_push(20);
}
u32 Game::o1_title_panel14_begin() {
    cover(0x800CF004u);
    Frame frame(*this, 24);
    const u32 state = m.r32(kState);
    const u32 timer = m.r16(state + kTimer) + 1;
    m.w32(state + kProgress, 0); m.w16(state + kTimer, timer);
    o1_text_item_enable(14); o1_screen_text_select(0x4016);
    o1_animation_command(1, 18); o1_marker_set(64, 50, 1, 0);
    return o1_animation_history_push(18);
}
u32 Game::o1_text_item_reverse(u32 index) {
    cover(0x800DA7A8u);
    Frame frame(*this, 32);
    const u32 item = m.r32(m.r32(kScreens) + 32) + 12 * index;
    const u32 reversed = static_cast<u32>(static_cast<s16>(0u - m.r16(item + 8)));
    const u32 x = static_cast<u32>(m.r16s(item + 4)), y = static_cast<u32>(m.r16s(item + 6));
    m.w32(frame.at(16), reversed);
    return o1_text_item_set(index, 2, x, y, reversed);
}
u32 Game::o1_title_panel12_begin() {
    cover(0x800CEB20u);
    Frame frame(*this, 24);
    const u32 state = m.r32(kState);
    m.w16(state + kTimer, m.r16(state + kTimer) + 1);
    o1_text_item_enable(12); o1_screen_text_select(0x105E);
    o1_animation_command(1, 16); o1_animation_command(4, 22);
    o1_marker_set(111, 150, 1, 0);
    return o1_animation_history_push(14);
}
u32 Game::o1_title_panel13_begin() {
    cover(0x800CEECCu);
    Frame frame(*this, 24);
    const u32 state = m.r32(kState), timer = m.r16(state + kTimer) + 1;
    m.w32(state + 80, 0); m.w32(state + kProgress, 0); m.w16(state + kTimer, timer);
    o1_text_item_enable(13); o1_screen_text_select(0x2056);
    o1_animation_command(1, 17); o1_animation_command(4, 23);
    o1_marker_set(64, 50, 1, 0);
    return o1_animation_history_push(17);
}
u32 Game::o1_title_panel_choice_poll() {
    cover(0x800CFBD0u);
    Frame frame(*this, 24);
    const u32 buttons = m.r32(m.r32(g::kScene) + 300);
    if (!(buttons & 48)) return 0;
    const bool confirm = (buttons & 16) != 0;
    const u32 state = m.r32(kState);
    m.w16(state + kSubstate, confirm ? 9 : 8); m.w16(state + kTimer, 0);
    return sound_play(confirm ? 89 : 59);
}
u32 Game::o1_animation_setup(u32 directory) {
    cover(0x800DA418u);
    Frame frame(*this, 24);
    m.w32(0x800DDA68u, mem_alloc(12));
    const u32 commands = ld_relocate_dir(directory);
    m.w32(m.r32(0x800DDA68u) + 8, commands);
    for (u32 i = 0; i < 8; ++i) m.w8(m.r32(0x800DDA68u) + i, 0);
    return o1_menu_delay();
}
u32 Game::o1_title_panel10_begin() {
    cover(0x800CEC70u);
    Frame frame(*this, 24);
    const u32 state = m.r32(kState), timer = m.r16(state + kTimer) + 1;
    m.w32(state + kProgress, 0); m.w16(state + kTimer, timer);
    o1_text_item_enable(10); o1_screen_text_select(1086); o1_fade_set(15, 1, 0, 60);
    o1_animation_command(1, 11); o1_animation_command(4, 6);
    o1_marker_set(64, 50, 1, 0);
    return o1_animation_history_push(16);
}
u32 Game::o1_account_panel21_begin() {
    cover(0x800D1080u);
    Frame frame(*this, 24);
    const u32 state = m.r32(kState);
    m.w16(state + kTimer, m.r16(state + kTimer) + 1);
    o1_text_item_enable(21); o1_screen_text_select(0x200056);
    o1_animation_command(1, 38); o1_animation_command(4, 24);
    o1_fade_set(m.r8(m.r32(kState) + 93) + 6, 1, 0, 10);
    o1_marker_mode(2);
    return o1_animation_history_push(33);
}
u32 Game::o1_title_panel11_begin() {
    cover(0x800CE518u);
    Frame frame(*this, 24);
    const u32 state = m.r32(kState), timer = m.r16(state + kTimer) + 1;
    m.w16(state + 16, 0); m.w32(state + kProgress, 0); m.w16(state + kTimer, timer);
    o1_text_item_enable(11); o1_text_item_enable(6); o1_screen_text_select(0x85E);
    o1_fade_set(15, 1, 0, 60); o1_animation_command(1, 15); o1_animation_command(4, 22);
    o1_marker_set(111, 150, 1, 1);
    return o1_animation_history_push(14);
}
u32 Game::o1_account_panel20_begin() {
    cover(0x800D0DC8u);
    Frame frame(*this, 24);
    const u32 state = m.r32(kState), timer = m.r16(state + kTimer) + 1;
    m.w32(state + kProgress, 0); m.w16(state + kTimer, timer);
    o1_text_item_enable(20); o1_screen_text_select(0x100056);
    o1_animation_command(1, 36); o1_animation_command(4, 22); o1_marker_mode(1);
    m.w8(m.r32(kState) + 210, 0); o1_marker_set(64, 50, 1, 0);
    return o1_animation_history_push(31);
}
u32 Game::o1_account_missing_panel_begin() {
    cover(0x800D02DCu);
    Frame frame(*this, 24);
    o1_animation_command(1, 26);
    if (t1_card_status(m.r8(m.r32(kState) + 90)) != 1) {
        m.w16(m.r32(kState) + kSubstate, 13); return 13;
    }
    const u32 state = m.r32(kState);
    m.w16(state + kTimer, m.r16(state + kTimer) + 1);
    o1_text_item_enable(17); o1_screen_text_select(0x20056);
    o1_animation_command(1, 28); o1_animation_command(4, 21);
    return o1_animation_history_push(26);
}
u32 Game::o1_title_panel17_begin() {
    cover(0x800CFB2Cu);
    Frame frame(*this, 24);
    const u32 state = m.r32(kState);
    m.w16(state + kTimer, m.r16(state + kTimer) + 1);
    o1_text_item_enable(17); o1_screen_text_select(0x20056);
    o1_fade_set(2, 3, 0xFFFFFFFFu, 0xFFFFFFFFu);
    o1_animation_command(2, 1); o1_animation_command(1, 22); o1_animation_command(4, 22);
    o1_marker_set(64, 50, 1, 0); o1_animation_history_push(21);
    m.w8(m.r32(kState) + 209, 1);
    return 1;
}
u32 Game::o1_card_save_prompt_poll() {
    cover(0x800D255Cu);
    Frame frame(*this, 24);
    const s32 status = t1_card_status(m.r8(m.r32(kState) + 90));
    m.w8(m.r32(kState) + 208, 1);
    const bool unavailable = status == 2 || status == 6;
    if (!unavailable && !(m.r32(m.r32(g::kScene) + 300) & 32)) return 0;
    const u32 state = m.r32(kState);
    m.w16(state + kSubstate, unavailable ? 0 : 2); m.w16(state + kTimer, 0);
    if (!unavailable) sound_play(89);
    const u32 current = unavailable ? state : m.r32(kState);
    m.w8(current + 208, 0);
    return current;
}
u32 Game::o1_line_packet(u32 description, u32 packet, u32 orderingTable) {
    cover(0x800D9024u);
    m.w8(packet + 3, 3); m.w8(packet + 7, 0x40);
    m.w8(packet + 7, m.r8(description + 11) ? 0x42 : 0x40);
    for (u32 i = 0; i < 3; ++i) m.w8(packet + 4 + i, m.r8(description + 8 + i));
    for (u32 i = 0; i < 3; ++i) m.w16(packet + 8 + 2 * i, m.r16(description + 2 * i));
    const u32 y1 = m.r16(description + 6), header = m.r32(packet);
    m.w16(packet + 14, y1);
    m.w32(packet, (header & 0xFF000000u) | (m.r32(orderingTable) & 0x00FFFFFFu));
    m.w32(orderingTable, (m.r32(orderingTable) & 0xFF000000u) | (packet & 0x00FFFFFFu));
    return packet + 16;
}
}

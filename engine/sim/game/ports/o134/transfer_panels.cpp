#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kState = 0x800DD9D0u;
constexpr u32 kSelectedCard = 90, kSubstate = 76, kTimer = 78;
}
u32 Game::o1_account_free_blocks_poll() {
    cover(0x800D06D4u);
    Frame frame(*this, 24);
    const s32 free = t1_card_free(m.r8(m.r32(kState) + kSelectedCard));
    if (free == -1) return 1;
    const u32 state = m.r32(kState), timer = m.r16(state + kTimer);
    if (free >= 2) { m.w16(state + kTimer, timer + 2); return timer + 2; }
    m.w16(state + kTimer, timer + 1);
    o1_animation_command(1, 34); o1_animation_history_push(30);
    return o1_screen_text_select(0x20016);
}
u32 Game::o1_account_file_create_poll() {
    cover(0x800D0848u);
    Frame frame(*this, 24);
    const s32 result = static_cast<s32>(o1_card_file_create(m.r8(m.r32(kState) + kSelectedCard)));
    if (result == 1) {
        const u32 state = m.r32(kState), timer = m.r16(state + kTimer) + 1;
        m.w16(state + kTimer, timer); return timer;
    }
    if (result != -1) return 0xFFFFFFFFu;
    const u32 state = m.r32(kState);
    m.w16(state + kTimer, m.r16(state + kTimer) + 3);
    o1_animation_command(1, 35); o1_animation_history_push(32);
    return o1_screen_text_select(0x20016);
}
u32 Game::o1_account_header_write_poll() {
    cover(0x800D08DCu);
    Frame frame(*this, 32);
    const s32 result = t1_save_header(m.r8(m.r32(kState) + kSelectedCard), 1);
    if (result == -1) {
        const u32 state = m.r32(kState);
        m.w16(state + kTimer, m.r16(state + kTimer) + 2);
        o1_animation_command(1, 35); o1_animation_history_push(32); o1_screen_text_select(0x20016);
    }
    if (result != 1) return 1;
    const u32 state = m.r32(kState), timer = m.r16(state + kTimer) + 1;
    m.w16(state + kTimer, timer); return timer;
}
u32 Game::o1_account_empty_directory_write_poll() {
    cover(0x800D0978u);
    Frame frame(*this, 32);
    const s32 result = t1_save_dir(m.r8(m.r32(kState) + kSelectedCard), -1, 1);
    if (result == -1) {
        const u32 state = m.r32(kState);
        m.w16(state + kTimer, m.r16(state + kTimer) + 1);
        o1_animation_command(1, 35); o1_animation_history_push(32); o1_screen_text_select(0x20016);
    }
    if (result == 1) {
        const u32 state = m.r32(kState);
        m.w16(state + kSubstate, 16); m.w16(state + kTimer, 0);
    }
    return 16;
}
u32 Game::o1_account_directory_read_poll() {
    cover(0x800D0B0Cu);
    Frame frame(*this, 32);
    const s32 result = t1_save_read_dir(m.r8(m.r32(kState) + kSelectedCard), 1);
    if (result == -1 || result == 2) {
        const u32 state = m.r32(kState);
        m.w16(state + kTimer, m.r16(state + kTimer) + 2);
        o1_animation_command(1, result == -1 ? 35 : 37); o1_screen_text_select(0x20056);
        o1_animation_history_push(32); o1_animation_command(4, 21);
    }
    if (result == 1) {
        const u32 state = m.r32(kState), timer = m.r16(state + kTimer) + 1;
        m.w16(state + kTimer, timer); return timer;
    }
    return 1;
}
u32 Game::o1_account_pilots_read_poll() {
    cover(0x800D0BFCu);
    Frame frame(*this, 32);
    const s32 result = t1_save_read_pilots(m.r8(m.r32(kState) + kSelectedCard), 1);
    if (result == -1 || result == 2) {
        const u32 state = m.r32(kState);
        m.w16(state + kTimer, m.r16(state + kTimer) + 1);
        o1_animation_command(1, result == -1 ? 35 : 37);
        if (result == 2) o1_text_item_enable(6);
        o1_screen_text_select(0x20056); o1_animation_history_push(32); o1_animation_command(4, 21);
    }
    if (result == 1) {
        const u32 state = m.r32(kState);
        m.w16(state + kSubstate, 16); m.w16(state + kTimer, 0);
    }
    return 16;
}
u32 Game::o1_save_directory_read_poll() {
    cover(0x800D23CCu);
    Frame frame(*this, 32);
    const s32 result = t1_save_read_dir(m.r8(m.r32(kState) + kSelectedCard), 1);
    if (result == -1 || result == 2) {
        const u32 state = m.r32(kState);
        m.w16(state + kTimer, m.r16(state + kTimer) + 2);
        o1_animation_command(1, result == -1 ? 14 : 8); o1_animation_history_push(6);
    }
    if (result != 1) return 1;
    const u32 state = m.r32(kState), timer = m.r16(state + kTimer) + 1;
    m.w16(state + kTimer, timer); return timer;
}
u32 Game::o1_save_pilots_read_poll() {
    cover(0x800D248Cu);
    Frame frame(*this, 32);
    const s32 result = t1_save_read_pilots(m.r8(m.r32(kState) + kSelectedCard), 1);
    if (result == -1 || result == 2) {
        const u32 state = m.r32(kState);
        m.w16(state + kTimer, m.r16(state + kTimer) + 1);
        o1_animation_command(1, result == -1 ? 14 : 8); o1_animation_history_push(6);
        m.w8(m.r32(kState) + 208, 1);
    }
    if (result == 1) {
        const u32 state = m.r32(kState);
        m.w16(state + kSubstate, 7); m.w16(state + kTimer, 0);
    }
    return 7;
}
}

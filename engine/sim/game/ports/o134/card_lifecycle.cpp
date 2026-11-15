#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kCards = 0x800DDA48u, kState = 0x800DD9D0u;
constexpr u32 kDirectory = 744, kSaveSlots = 748, kLoadedSave = 760;
}

u32 Game::o1_card_directory_reset() {
    cover(0x800D51FCu);
    const u32 cards = m.r32(kCards);
    m.w32(m.r32(cards + kDirectory) + 4, 0x00990426u);
    m.w8(m.r32(cards + kDirectory) + 8, 0);
    m.w8(m.r32(m.r32(kCards) + kDirectory) + 9, 0);
    for (u32 i = 0; i < 112; ++i)
        m.w8(m.r32(m.r32(kCards) + kDirectory) + 16 + i, 0);
    const u32 directory = m.r32(m.r32(kCards) + kDirectory);
    m.w32(directory + 12, 0);
    return directory;
}
u32 Game::o1_card_directory_reset_callback() {
    cover(0x800D5270u);
    Frame frame(*this, 24);
    return o1_card_directory_reset();
}
u32 Game::o1_card_saves_reset_callback() {
    cover(0x800D5528u);
    Frame frame(*this, 24);
    t1_card_states_reset();
    return 0;
}
u32 Game::o1_card_software_events_drain() {
    cover(0x800D3F88u);
    Frame frame(*this, 24);
    u32 result = 0;
    for (u32 i = 0; i < 4; ++i) result = hw->test_event(m.r32(m.r32(kCards) + 4 + 4 * i));
    return result;
}
u32 Game::o1_card_hardware_events_drain() {
    cover(0x800D3FF4u);
    Frame frame(*this, 24);
    u32 result = 0;
    for (u32 i = 0; i < 4; ++i) result = hw->test_event(m.r32(m.r32(kCards) + 20 + 4 * i));
    return result;
}
u32 Game::o1_card_save_name_commit(u32 slot) {
    cover(0x800D4D6Cu);
    Frame frame(*this, 24);
    if (slot != 0xFFFFFFFFu && static_cast<s32>(slot) >= 3) return 0x800E0000u;
    const u32 cards = m.r32(kCards), field = slot == 0xFFFFFFFFu ? kLoadedSave : kSaveSlots + 4 * slot;
    m.w16(m.r32(cards + field) + 22, 0);
    return libc_strcpy(m.r32(cards + field) + 8, m.r32(g::kScene) + 2376);
}
u32 Game::o1_card_initial_poll_finish() {
    cover(0x800D1ADCu);
    Frame frame(*this, 24);
    const s32 first = t1_card_status(0), second = t1_card_status(1);
    if (first == 0) return static_cast<u32>(second);
    if (second == 0) return 0x800E0000u;
    const u32 state = m.r32(kState);
    m.w16(state + 76, 1); m.w16(state + 78, 0);
    return 1;
}
u32 Game::o1_card_creation_begin() {
    cover(0x800D237Cu);
    Frame frame(*this, 24);
    const u32 state = m.r32(kState);
    m.w16(state + 78, m.r16(state + 78) + 1);
    o1_animation_command(1, 7);
    o1_card_directory_reset();
    t1_card_states_reset();
    o1_empty_5548();
    return 0;
}
u32 Game::o1_card_events_close() {
    cover(0x800D58B0u);
    Frame frame(*this, 24);
    hw->enter_critical_section();
    for (u32 i = 0; i < 7; ++i) hw->close_event(m.r32(m.r32(kCards) + 4 + 4 * i));
    return hw->exit_critical_section();
}
u32 Game::o1_account_exit_poll() {
    cover(0x800D7600u);
    Frame frame(*this, 24);
    if (!t1_cards_idle()) return 0;
    o1_card_events_close();
    const u32 state = m.r32(kState);
    m.w32(state + 24, 0); m.w8(state + 88, 1);
    return 1;
}
}

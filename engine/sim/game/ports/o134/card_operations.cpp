#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kCards = 0x800DDA48u, kState = 0x800DD9D0u;
constexpr u32 kSoftwareCard = 0xF4000001u, kHardwareCard = 0xF0000011u;
constexpr u32 kEventNoInterrupt = 0x2000;
}

u32 Game::o1_card_events_open() {
    cover(0x800D56E0u);
    Frame frame(*this, 24);
    hw->enter_critical_section();
    constexpr u32 softwareSpecs[] = {4, 0x8000, 0x100, 0x2000};
    constexpr u32 hardwareSpecs[] = {4, 0x8000, 0x100};
    for (u32 i = 0; i < 4; ++i) {
        const u32 event = hw->open_event(kSoftwareCard, softwareSpecs[i], kEventNoInterrupt, 0);
        m.w32(m.r32(kCards) + 4 + 4 * i, event);
    }
    for (u32 i = 0; i < 3; ++i) {
        const u32 event = hw->open_event(kHardwareCard, hardwareSpecs[i], kEventNoInterrupt, 0);
        m.w32(m.r32(kCards) + 20 + 4 * i, event);
    }
    for (u32 i = 0; i < 7; ++i) hw->enable_event(m.r32(m.r32(kCards) + 4 + 4 * i));
    return hw->exit_critical_section();
}
u32 Game::o1_cards_initialise() {
    cover(0x800D5880u);
    Frame frame(*this, 24);
    t1_cards_alloc(); o1_card_events_open(); t1_cards_reset();
    return 0;
}
u32 Game::o1_card_file_create(u32 slot) {
    cover(0x800D5EC4u);
    Frame frame(*this, 32);
    if (!t1_cards_idle()) return 0;
    constexpr u32 kCreateOneBlock = 0x00020200u;
    const u32 file = hw->file_open(m.r32(kCards) + 692 + 26 * slot, kCreateOneBlock);
    m.w32(m.r32(kCards) + 60, file);
    hw->file_close(file);
    t1_card_flags_reset();
    return file == 0xFFFFFFFFu ? 0xFFFFFFFFu : 1;
}
u32 Game::o1_card_format(u32 slot) {
    cover(0x800D5E74u);
    Frame frame(*this, 24);
    if (!t1_cards_idle()) return 0;
    const u32 result = card_format(0x800AD808u, slot << 4);
    t1_card_flags_reset();
    return result == 1 ? 1 : 0xFFFFFFFFu;
}
u32 Game::o1_account_card_format_poll() {
    cover(0x800D061Cu);
    Frame frame(*this, 24);
    if (o1_card_format(m.r8(m.r32(kState) + 90)) == 0) return 0;
    const u32 state = m.r32(kState);
    m.w16(state + 76, 14); m.w16(state + 78, 3);
    return state;
}
}

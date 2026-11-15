#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kScreens = 0x800DD9DCu, kCards = 0x800DDA48u;
constexpr u32 kState = 0x800DD9D0u, kPrims = 0x800DDA58u;
constexpr u32 kAnimationFlags = 76, kTextItems = 32;
constexpr u32 kFadeStates = 72, kFadeSteps = 68, kFadeValues = 64;
}

u32 Game::o1_animation_command(u32 index, u32 value) {
    cover(0x800DA9A8u);
    if (index >= 64) return 0x800E0000u;
    const u32 entry = m.r32(m.r32(kScreens) + kAnimationFlags) + index;
    m.w8(entry, value);
    return entry;
}
u32 Game::o1_card_operation_bit(u32 bit) {
    cover(0x800D5A38u);
    if (bit >= 3) return 0xFFFFFFFFu;
    return (m.r8(m.r32(m.r32(kCards) + 744) + 8) >> bit) & 1;
}
u32 Game::o1_clip_fade_done() {
    cover(0x800DAC98u);
    const u32 screens = m.r32(kScreens), clip = m.r32(screens + 44);
    const u32 index = static_cast<u32>(m.r16s(clip + 12));
    return m.r16s(m.r32(screens + kFadeStates) + 2 * index) == 2;
}
u32 Game::o1_title_animation_begin() {
    cover(0x800CFD28u);
    Frame frame(*this, 24);
    const u32 state = m.r32(kState);
    m.w16(state + 78, m.r16(state + 78) + 1);
    return o1_animation_command(1, 26);
}
u32 Game::o1_save_name_to_scene() {
    cover(0x800D4158u);
    Frame frame(*this, 24);
    return libc_strcpy(m.r32(g::kScene) + 2376, m.r32(m.r32(kCards) + 760) + 8);
}
u32 Game::o1_animation_history_push(u32 command) {
    cover(0x800DA49Cu);
    Frame frame(*this, 24);
    const u32 history = m.r32(0x800DDA68u);
    for (u32 count = 8; count != 0; --count)
        m.w8(history + count, m.r8(history + count - 1));
    m.w8(history, command);
    return o1_menu_delay();
}
u32 Game::o1_account_command_begin() {
    cover(0x800D2954u);
    Frame frame(*this, 24);
    const u32 state = m.r32(kState);
    m.w16(state + 78, m.r16(state + 78) + 1);
    o1_animation_command(1, 12);
    return o1_animation_history_push(12);
}
u32 Game::o1_resource_allocate(u32 size) {
    cover(0x800CD930u);
    const u32 aligned = ((size + 3) >> 2) << 2, block = m.r32(0x800DDA20u);
    m.w32(0x800DDA24u, block);
    const u32 used = m.r32(0x800DDA2Cu);
    m.w32(0x800DDA20u, block + aligned);
    m.w32(0x800DDA2Cu, used + aligned);
    return block;
}
u32 Game::o1_card_account_type(u32 slot) {
    cover(0x800D5A70u);
    const u32 profile = m.r32(m.r32(kCards) + 764 + 4 * (slot & 255));
    return m.r32(0x800DD7F0u + 4 * m.r8(profile + 274));
}
u32 Game::o1_card_secondary_type(u32 slot) {
    cover(0x800D5BF4u);
    const u32 profile = m.r32(m.r32(kCards) + 764 + 4 * (slot & 255));
    return m.r32(0x800DD6A4u + 4 * m.r8(profile + 275));
}
u32 Game::o1_title_selection_begin() {
    cover(0x800CE24Cu);
    Frame frame(*this, 24);
    const u32 state = m.r32(kState), scene = m.r32(g::kScene);
    m.w32(state + 224, 0);
    m.w16(state + 14, m.r16(state + 14) + 1);
    return o1_flow_auxiliary(static_cast<u32>(m.r16s(scene + 908)));
}
u32 Game::o1_drive_transition() {
    cover(0x800DA6B8u);
    Frame frame(*this, 24);
    if (kcd_drive_state() != 1) return 0;
    const u32 flow = m.r32(0x800DD910u);
    m.w32(flow, 1); m.w32(flow + 4, 1);
    return 4;
}
u32 Game::o1_title_attract_begin() {
    cover(0x800CFC38u);
    Frame frame(*this, 24);
    o1_animation_command(2, 2); o1_animation_command(1, 23);
    const u32 state = m.r32(kState);
    m.w16(state + 78, m.r16(state + 78) + 1);
    o1_animation_history_push(22);
    return o1_animation_command(4, 22);
}
u32 Game::o1_save_name_address(u32 slot) {
    cover(0x800D5970u);
    if (slot == 0) return m.r32(m.r32(kCards) + 760) + 8;
    if (static_cast<s32>(slot) < 4) return m.r32(m.r32(kCards) + 748 + 4 * (slot - 1)) + 8;
    return 0x800CC504u;
}
u32 Game::o1_text_style(u32 style) {
    cover(0x800D7C88u);
    const bool enabled = static_cast<s16>(style) != -1;
    m.w8(m.r32(kPrims) + 64, enabled);
    const u32 prims = m.r32(kPrims);
    m.w8(prims + 65, enabled ? style : 0);
    return prims;
}
u32 Game::o1_account_selection_begin() {
    cover(0x800D12A4u);
    Frame frame(*this, 24);
    const u32 state = m.r32(kState);
    m.w16(state + 78, m.r16(state + 78) + 1);
    o1_animation_command(1, 39); o1_animation_command(4, 24);
    const u32 clock = t1_clock();
    m.w32(m.r32(g::kScene) + 2408, clock);
    return o1_save_name_to_scene();
}
u32 Game::o1_fade_set(u32 index, u32 state, u32 value, u32 step) {
    cover(0x800DA838u);
    if (index >= 16) return 0;
    const u32 screens = m.r32(kScreens), offset = 2 * index;
    u32 entry = m.r32(screens + kFadeStates) + offset;
    m.w16(entry, state);
    if (static_cast<s32>(step) >= 0) {
        entry = m.r32(screens + kFadeSteps) + offset;
        m.w16(entry, step);
    }
    if (static_cast<s32>(value) >= 0) {
        entry = m.r32(m.r32(kScreens) + kFadeValues) + offset;
        m.w16(entry, value);
    }
    return entry;
}
u32 Game::o1_text_item_set(u32 index, u32 state, u32 x, u32 y, u32 extra) {
    cover(0x800DA714u);
    if (index >= 32) return 0x800E0000u;
    const u32 screens = m.r32(kScreens), offset = 12 * index;
    m.w16(m.r32(screens + kTextItems) + offset + 10, state);
    m.w16(m.r32(screens + kTextItems) + offset + 4, x);
    m.w16(m.r32(screens + kTextItems) + offset + 6, y);
    const u32 records = m.r32(screens + kTextItems);
    m.w16(records + offset + 8, extra);
    return records;
}
u32 Game::o1_text_item_enable(u32 index) {
    cover(0x800DA77Cu);
    Frame frame(*this, 32);
    m.w32(frame.at(16), 0);
    return o1_text_item_set(index, 1, 0, 0, 0);
}
u32 Game::o1_account_drive_transition() {
    cover(0x800D3A8Cu);
    Frame frame(*this, 24);
    const u32 drive = kcd_drive_state();
    if (drive != 1) return drive;
    o1_animation_command(1, 73); o1_animation_history_push(38);
    const u32 state = m.r32(kState), timer = m.r16(state + 78) - 2;
    m.w32(state + 212, 31); m.w16(state + 78, timer);
    return timer;
}
}

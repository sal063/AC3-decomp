#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kState = 0x800DD9D0u, kCards = 0x800DDA48u;
constexpr u32 kTimer = 78, kSubstate = 76, kSelectedCard = 90, kSelectedSave = 93;
constexpr u32 kProfileIndex = 80, kPending = 822, kRetry = 856, kActive = 858;
constexpr u32 kSavedPilots = 748, kProfiles = 764;
bool unavailable(s32 status) { return status == 2 || status == 6; }
bool unusable(s32 status) { return status == 0 || unavailable(status); }
}
u32 Game::o1_card_profiles_read(u32 slot, u32 saveSlot, u32 reset) {
    cover(0x800D6AB4u);
    Frame frame(*this, 40);
    if (!t1_cards_idle()) return 0;
    const u32 cards = m.r32(kCards), save = m.r32(cards + kSavedPilots + 4 * saveSlot);
    const u32 index = static_cast<u32>(m.r16s(cards + kProfileIndex));
    if ((static_cast<u32>(m.r16s(save + 22)) >> (index & 31u)) & 1u) {
        if (!t1_cards_idle()) return 0;
        t1_io_begin();
        auto retry = [&]() -> u32 {
            const u32 current = m.r32(kCards);
            if (m.r16s(current + kRetry) < 3) {
                m.w16(current + kRetry, m.r16(current + kRetry) + 1); return 0;
            }
            m.w16(current + kActive, 0); t1_card_flags_reset();
            m.w16(m.r32(kCards) + kProfileIndex, 0); return 0xFFFFFFFFu;
        };
        if (!m.r8(m.r32(kCards) + kPending)) {
            const u32 file = hw->file_open(m.r32(kCards) + 692 + 26 * slot, 0x8001);
            m.w32(m.r32(kCards) + 60, file);
            if (file == 0xFFFFFFFFu) return retry();
            const u32 profile = static_cast<u32>(m.r16s(m.r32(kCards) + kProfileIndex));
            hw->file_lseek(file, 4224 + 2304 * saveSlot + 384 * profile, 0);
            const u32 current = m.r32(kCards);
            const u32 destination = m.r32(current + kProfiles + 4 * static_cast<u32>(m.r16s(current + kProfileIndex)));
            hw->file_read(m.r32(current + 60), destination, 384);
            m.w8(m.r32(kCards) + kPending, 1);
        }
        const s32 event = t1_card_wait(4, slot, 0);
        if (event == -1) return 0;
        hw->file_close(m.r32(m.r32(kCards) + 60)); m.w8(m.r32(kCards) + kPending, 0);
        if (event) return retry();
        const u32 current = m.r32(kCards);
        const u32 profile = m.r32(current + kProfiles + 4 * static_cast<u32>(m.r16s(current + kProfileIndex)));
        if (m.r32(profile) != t1_save_sum(profile, 94)) {
            t1_card_flags_reset();
            const u32 after = m.r32(kCards);
            m.w16(after + kActive, 0); m.w16(after + kProfileIndex, 0); return 2;
        }
    }
    const u32 current = m.r32(kCards);
    if (m.r16s(current + kProfileIndex) < 6) {
        m.w16(current + kProfileIndex, m.r16(current + kProfileIndex) + 1); return 0;
    }
    if (reset == 1) t1_card_flags_reset();
    const u32 after = m.r32(kCards);
    m.w16(after + kActive, 0); m.w16(after + kProfileIndex, 0); return 1;
}
u32 Game::o1_save_profiles_read_poll() {
    cover(0x800D29FCu);
    Frame frame(*this, 32);
    const u32 state = m.r32(kState);
    const s32 result = static_cast<s32>(o1_card_profiles_read(m.r8(state + kSelectedCard), m.r8(state + kSelectedSave), 1));
    if (result == -1 || result == 2) {
        const u32 current = m.r32(kState);
        m.w16(current + kTimer, m.r16(current + kTimer) + 1);
        o1_animation_command(1, result == -1 ? 14 : 8);
        o1_animation_history_push(result == -1 ? 13 : 6);
        m.w8(m.r32(kState) + 208, 1);
        if (result == -1) o1_marker_mode(0);
        return 9;
    }
    if (result == 1) {
        const u32 current = m.r32(kState);
        m.w16(current + kSubstate, 9); m.w16(current + kTimer, 0);
        o1_animation_history_push(10); return o1_marker_mode(0);
    }
    return 9;
}
u32 Game::o1_rank_menu_begin() {
    cover(0x800D2E74u);
    Frame frame(*this, 32);
    o1_screen_text_select(542); o1_text_item_enable(9);
    o1_fade_set(1, 1, 0, 20); o1_fade_set(15, 1, 0, 20); o1_animation_command(1, 10);
    const u32 state = m.r32(kState);
    m.w8(state + 211, 1); m.w16(state + kTimer, m.r16(state + kTimer) + 1);
    for (u32 rank = 0; rank < 8; ++rank) o1_animation_command(6 + rank, m.r8(m.r32(kState) + 95) == rank ? 1 : 0);
    const bool eligible = o1_mission_feature_complete() != 0;
    const u32 scene = m.r32(g::kScene), flags = m.r32(scene + 2412);
    m.w32(scene + 2412, eligible ? flags | 16u : flags & ~16u);
    return o1_animation_command(eligible && m.r8(m.r32(kState) + 95) != 6 ? 12 : 13, 2);
}
u32 Game::o1_mission_disc_choice() {
    cover(0x800D3840u);
    Frame frame(*this, 32);
    const s32 disc = disc_number();
    if (disc == 0) return o1_animation_command(1, 73);
    auto required = [&]() { return m.r8(0x800DD8D4u + static_cast<u32>(m.r16s(m.r32(g::kScene) + 908))); };
    const u32 wanted = required();
    if (wanted == 3 || disc == 3 || (disc == 1 && wanted == 1) || (disc == 2 && wanted == 2)) {
        const u32 state = m.r32(kState);
        m.w16(state + 12, 6); m.w16(state + 14, 0); return 6;
    }
    if (disc != 1 && disc != 2 && disc != 4 && disc != -1) return 0x800C0000u;
    const u32 selection = m.r32(m.r32(kState) + 212);
    if (disc == 4 || disc == -1) {
        if (wanted == 1) o1_animation_command(1, selection + 70);
        if (required() == 2) o1_animation_command(1, m.r32(m.r32(kState) + 212) + 71);
    } else {
        o1_animation_command(1, selection + (disc == 1 ? 71 : 70));
    }
    kcd_open_drive(); o1_animation_history_push(37);
    const u32 state = m.r32(kState), timer = m.r16(state + kTimer) + 1;
    m.w16(state + kTimer, timer);
    return disc == 1 ? 0x800C0000u : timer;
}
u32 Game::o1_saved_card_choice() {
    cover(0x800D1DCCu);
    Frame frame(*this, 32);
    const s32 first = t1_card_status(0), second = t1_card_status(1);
    if (unavailable(first) && unavailable(second)) {
        const u32 state = m.r32(kState);
        m.w16(state + kSubstate, 1); m.w16(state + kTimer, 0);
        o1_fade_set(2, 1, 0, 20); o1_fade_set(1, 3, 0xFFFFFFFFu, 0xFFFFFFFFu);
        const u32 current = m.r32(kState); m.w8(current + 209, 1); return current;
    }
    const u32 state = m.r32(kState), old = m.r8(state + kSelectedCard);
    if (m.r32(m.r32(g::kScene) + 304) & 3u) m.w8(state + kSelectedCard, old ^ 1u);
    if (unusable(first)) m.w8(m.r32(kState) + kSelectedCard, 1);
    if (unusable(second)) m.w8(m.r32(kState) + kSelectedCard, 0);
    if (m.r8(m.r32(kState) + kSelectedCard) != old) sound_play(61);
    if (m.r8(m.r32(kState) + kSelectedCard) == 0) { o1_animation_command(6, 0); o1_animation_command(7, 1); }
    if (m.r8(m.r32(kState) + kSelectedCard) == 1) { o1_animation_command(6, 1); o1_animation_command(7, 0); }
    if (unusable(first)) o1_animation_command(6, 2);
    if (unusable(second)) o1_animation_command(7, 2);
    const u32 buttons = m.r32(m.r32(g::kScene) + 300);
    if (buttons & 16u) {
        const u32 current = m.r32(kState);
        m.w16(current + kSubstate, 3); m.w16(current + kTimer, 0);
        sound_play(89); return o1_animation_command(4, 5);
    }
    if (buttons & 32u) {
        const u32 current = m.r32(kState);
        m.w16(current + 12, 2); m.w16(current + 14, 0);
        o1_screen_text_select(190); return sound_play(59);
    }
    return 0x800E0000u;
}
u32 Game::o1_saved_pilot_choice() {
    cover(0x800D26F0u);
    Frame frame(*this, 32);
    const s32 status = t1_card_status(m.r8(m.r32(kState) + kSelectedCard));
    const u32 present = o1_card_operation_status();
    if (unavailable(status)) {
        const u32 state = m.r32(kState);
        m.w16(state + kSubstate, 0); m.w16(state + kTimer, 0); return o1_marker_mode(0);
    }
    if (m.r32(m.r32(g::kScene) + 304) & 2u) {
        const u32 state = m.r32(kState), selected = m.r8(state + kSelectedSave);
        m.w8(state + kSelectedSave, selected < 2 ? selected + 1 : 0); sound_play(61);
    }
    if (m.r32(m.r32(g::kScene) + 304) & 1u) {
        const u32 state = m.r32(kState), selected = m.r8(state + kSelectedSave);
        m.w8(state + kSelectedSave, selected ? selected - 1 : 2); sound_play(61);
    }
    for (u32 slot = 0; slot < 3; ++slot) o1_animation_command(6 + slot, m.r8(m.r32(kState) + kSelectedSave) == slot ? 0 : 1);
    const u32 buttons = m.r32(m.r32(g::kScene) + 300);
    if (buttons & 16u) {
        const u32 state = m.r32(kState), selected = m.r8(state + kSelectedSave);
        if (!((present >> (selected & 31u)) & 1u)) return sound_play(48);
        m.w16(state + kSubstate, 8); m.w16(state + kTimer, 0);
        sound_play(89); return o1_animation_command(4, 5);
    }
    if (buttons & 32u) {
        const u32 state = m.r32(kState);
        m.w16(state + kSubstate, 2); m.w16(state + kTimer, 0);
        o1_fade_set(2, 1, 0, 20); o1_fade_set(1, 3, 0xFFFFFFFFu, 0xFFFFFFFFu);
        o1_marker_mode(0); return sound_play(59);
    }
    return 0;
}
}

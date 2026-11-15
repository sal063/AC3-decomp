#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kState = 0x800DD9D0u;
constexpr u32 kSubstate = 76, kTimer = 78, kCard = 90, kPilot = 93, kRank = 95, kPending = 96;
bool unavailable(s32 status) { return status == 2 || status == 6; }
}
u32 Game::o1_account_card_select() {
    cover(0x800D0074u);
    Frame frame(*this, 40);
    const u32 old = m.r8(m.r32(kState) + kCard);
    if (m.r32(m.r32(g::kScene) + 304) & 3u) m.w8(m.r32(kState) + kCard, old ^ 1u);
    const s32 first = t1_card_status(0), second = t1_card_status(1);
    if (unavailable(first) && unavailable(second)) {
        m.w8(m.r32(kState) + 209, 1);
        const u32 state = m.r32(kState); m.w16(state + kSubstate, 10); m.w16(state + kTimer, 0);
        o1_text_item_enable(17); o1_screen_text_select(0x20056); o1_fade_set(2, 1, 0, 20);
        return o1_fade_set(1, 3, 0xFFFFFFFFu, 0xFFFFFFFFu);
    }
    if (!first || unavailable(first)) m.w8(m.r32(kState) + kCard, 1);
    if (!second || unavailable(second)) m.w8(m.r32(kState) + kCard, 0);
    if (m.r8(m.r32(kState) + kCard) != old) sound_play(61);
    if (!m.r8(m.r32(kState) + kCard)) { o1_animation_command(6, 0); o1_animation_command(7, 1); }
    if (m.r8(m.r32(kState) + kCard) == 1) { o1_animation_command(6, 1); o1_animation_command(7, 0); }
    if (!first || first == 2) o1_animation_command(6, 2);
    if (!second || second == 2) o1_animation_command(7, 2);
    const u32 buttons = m.r32(m.r32(g::kScene) + 300);
    if (buttons & 16u) {
        const u32 state = m.r32(kState); m.w16(state + kSubstate, 12); m.w16(state + kTimer, 0);
        sound_play(89); return o1_animation_history_push(25);
    }
    if (buttons & 32u) {
        const u32 state = m.r32(kState); m.w16(state + kSubstate, 7); m.w16(state + kTimer, 0);
        return sound_play(59);
    }
    return 0;
}
u32 Game::o1_account_pilot_select() {
    cover(0x800D0E54u);
    Frame frame(*this, 24);
    if (unavailable(t1_card_status(m.r8(m.r32(kState) + kCard)))) {
        const u32 state = m.r32(kState); m.w16(state + kSubstate, 9); m.w16(state + kTimer, 0);
        return o1_marker_mode(0);
    }
    if (m.r32(m.r32(g::kScene) + 304) & 2u) {
        const u32 state = m.r32(kState), selected = m.r8(state + kPilot);
        m.w8(state + kPilot, selected < 2 ? selected + 1 : 0); sound_play(61);
    }
    if (m.r32(m.r32(g::kScene) + 304) & 1u) {
        const u32 state = m.r32(kState), selected = m.r8(state + kPilot);
        m.w8(state + kPilot, selected ? selected - 1 : 2); sound_play(61);
    }
    for (u32 i = 0; i < 3; ++i) o1_animation_command(6 + i, m.r8(m.r32(kState) + kPilot) == i ? 0 : 1);
    const u32 buttons = m.r32(m.r32(g::kScene) + 300);
    if (buttons & 64u) {
        const u32 exists = o1_card_operation_bit(m.r8(m.r32(kState) + kPilot)), state = m.r32(kState);
        m.w16(state + kSubstate, exists ? 17 : 18);
        if (!exists) m.w8(state + 210, 1);
        m.w16(m.r32(kState) + kTimer, 0); return sound_play(89);
    }
    if (buttons & 32u) {
        const u32 state = m.r32(kState); m.w16(state + kSubstate, 11); m.w16(state + kTimer, 0);
        o1_marker_mode(0); return sound_play(59);
    }
    return 0;
}
u32 Game::o1_account_overwrite_choice() {
    cover(0x800D1108u);
    Frame frame(*this, 24);
    if (unavailable(t1_card_status(m.r8(m.r32(kState) + kCard)))) {
        const u32 state = m.r32(kState); m.w16(state + kSubstate, 9); m.w16(state + kTimer, 0);
        return o1_marker_mode(0);
    }
    for (u32 i = 0; i < 3; ++i) o1_animation_command(6 + i, m.r8(m.r32(kState) + kPilot) == i ? 0 : 1);
    const u32 buttons = m.r32(m.r32(g::kScene) + 300);
    if (buttons & 96u) {
        const u32 state = m.r32(kState), selected = m.r8(state + kPilot);
        m.w16(state + kSubstate, buttons & 64u ? 18 : 16); m.w16(state + kTimer, 0);
        o1_fade_set(selected + 6, 0, 0, 0); sound_play(buttons & 64u ? 89 : 59);
    }
    const u32 state = m.r32(kState), progress = m.r32(state + 72) + 1u;
    m.w32(state + 72, progress); return progress;
}
u32 Game::o1_saved_rank_choice() {
    cover(0x800D2FB8u);
    Frame frame(*this, 40);
    const s32 fade = t1_fade_state(1); const u32 available = o1_save_flags();
    if (unavailable(t1_card_status(m.r8(m.r32(kState) + kCard)))) {
        const u32 state = m.r32(kState); m.w16(state + kSubstate, 0); m.w16(state + kTimer, 0); m.w8(state + 211, 0);
        return o1_screen_text_select(318);
    }
    if ((m.r32(m.r32(g::kScene) + 304) & 8u) && fade == 2) {
        const u32 state = m.r32(kState), rank = m.r8(state + kRank);
        u32 next;
        if (m.r32(m.r32(g::kScene) + 2412) & 16u)
            next = rank >= 8 || rank == 6 ? 0 : rank == 5 ? 7 : rank == 7 ? 6 : rank + 1;
        else next = rank < 6 ? rank + 1 : 0;
        m.w8(state + kRank, next); sound_play(61);
    }
    if ((m.r32(m.r32(g::kScene) + 304) & 4u) && fade == 2) {
        const u32 state = m.r32(kState), rank = m.r8(state + kRank);
        u32 next;
        if (m.r32(m.r32(g::kScene) + 2412) & 16u)
            next = rank >= 8 ? 0 : rank == 0 || rank == 6 ? 7 : rank == 7 ? 5 : rank - 1;
        else next = rank ? rank - 1 : 6;
        m.w8(state + kRank, next); sound_play(61);
    }
    for (u32 i = 0; i < 8; ++i) o1_animation_command(6 + i, m.r8(m.r32(kState) + kRank) == i ? 1 : 0);
    const bool extra = m.r32(m.r32(g::kScene) + 2412) & 16u;
    o1_animation_command(extra && m.r8(m.r32(kState) + kRank) != 6 ? 12 : 13, 2);
    o1_profile_details(m.r8(m.r32(kState) + kPilot), m.r8(m.r32(kState) + kRank));
    const u32 buttons = m.r32(m.r32(g::kScene) + 300);
    if ((buttons & 48u) == 16u || !fade) {
        const u32 rank = m.r8(m.r32(kState) + kRank);
        if (rank == 6 || rank == 7 || ((static_cast<s32>(available) >> (rank & 31u)) & 1)) {
            if (fade && fade != 3) {
                o1_fade_set(1, 3, 0xFFFFFFFFu, 0xFFFFFFFFu); m.w8(m.r32(kState) + kPending, 1); sound_play(89);
            } else if (m.r8(m.r32(kState) + kPending) == 1) {
                if (rank < 6) {
                    o1_fade_set(1, 0, 0, 0); o1_profile_unpack(rank);
                    const u32 state = m.r32(kState), scene = m.r32(g::kScene);
                    m.w16(state + kSubstate, 11); m.w16(state + kTimer, 0); m.w16(scene + 2388, 2);
                    m.w32(scene + 2412, m.r32(scene + 2412) & ~16u);
                }
                if (m.r8(m.r32(kState) + kRank) == 6) {
                    const u32 state = m.r32(kState), scene = m.r32(g::kScene);
                    m.w16(state + kSubstate, 10); m.w16(state + kTimer, 0); m.w32(scene + 2412, m.r32(scene + 2412) & ~16u);
                }
                if (m.r8(m.r32(kState) + kRank) == 7) {
                    const u32 state = m.r32(kState); m.w16(state + 12, 6); m.w16(state + 14, 0);
                }
                m.w8(m.r32(g::kScene) + 2397, m.r8(m.r32(kState) + kRank));
            }
        } else if (buttons & 16u) sound_play(48);
    }
    if (!(m.r32(m.r32(g::kScene) + 300) & 32u) && fade) return 0;
    if (fade && fade != 3) {
        o1_fade_set(1, 3, 0xFFFFFFFFu, 0xFFFFFFFFu); m.w8(m.r32(kState) + kPending, 2);
        return sound_play(59);
    }
    if (m.r8(m.r32(kState) + kPending) != 2) return 2;
    o1_fade_set(1, 0, 0, 0);
    const u32 state = m.r32(kState); m.w16(state + kSubstate, 7); m.w16(state + kTimer, 0);
    return o1_marker_set(64, 30, 0, 0);
}
}

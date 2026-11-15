#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kCards = 0x800DDA48u, kState = 0x800DD9D0u;
constexpr u32 kDifficulty = 0x800DD5F0u, kTimeText = 0x800DDA38u;
}
u32 Game::o1_profile_time_text(u32 rank) {
    cover(0x800D5AC8u);
    Frame frame(*this, 32);
    const u32 profile = m.r32(m.r32(kCards) + 764 + 4 * (rank & 255u));
    s32 ticks = m.r32s(profile + 276);
    if (ticks > 0x066FF2FF) ticks = 0x066FF2FF;
    if (ticks < 0) ticks = 0;
    const u32 seconds = static_cast<u32>(ticks) / 30;
    const u32 minutes = seconds / 60, hours = minutes / 60;
    m.w32(frame.at(16), seconds % 60);
    libc_sprintf(kTimeText, 0x800CC52Cu, {hours, minutes % 60, seconds % 60});
    for (u32 i = 0; i < 12; ++i)
        if (m.r8(kTimeText + i) == ' ') m.w8(kTimeText + i, '0');
    return kTimeText;
}
u32 Game::o1_profile_details(u32 pilot, u32 rank) {
    cover(0x800D2C84u);
    Frame frame(*this, 32);
    t1_prim_mode(4); o1_text_style(2); o1_text_fold_set(1, 10, 20);
    o1_text_fold_position(t1_fade_value(15)); o1_text_rgb_set(128, 128, 128);
    o1_text_origin_set(64, 50); t1_printf(o1_save_name_address(0));
    t1_prim_mode(3);
    rank &= 255u;
    if (rank == 6 || rank == 7) {
        o1_text_origin_set(140, 327); t1_printf(rank == 6 ? 0x800CC038u : 0x800CC044u);
    } else {
        pilot &= 255u;
        if (o1_save_flag_test(pilot, rank)) {
            o1_text_origin_set(106, 280); t1_printf(o1_card_secondary_type(rank));
            o1_text_origin_set(106, 320); t1_printf(0x800CC050u);
            o1_text_origin_set(150, 320); t1_printf(o1_card_account_type(rank));
            o1_text_origin_set(106, 340); t1_printf(0x800CC058u);
            o1_text_origin_set(150, 340); t1_printf(o1_profile_time_text(rank));
            o1_text_origin_set(106, 360); t1_printf(0x800CC060u);
            o1_text_origin_set(150, 360); t1_printf(o1_card_profile_type(rank));
            o1_text_origin_set(106, 380); t1_printf(0x800CC068u);
            o1_text_origin_set(150, 380); t1_printf(o1_save_route(pilot));
        } else {
            o1_text_origin_set(140, 327); t1_printf(0x800CC070u);
        }
    }
    return 0;
}
u32 Game::o1_new_pilot_difficulty() {
    cover(0x800D3584u);
    Frame frame(*this, 40);
    const s32 cardStatus = t1_card_status(m.r8(m.r32(kState) + 90));
    t1_prim_mode(4); o1_text_rgb_set(128, 128, 128); o1_text_style(2);
    for (u32 i = 0; i < 3; ++i) {
        o1_text_origin_set(106, 166 + 52 * i); t1_printf(0x800CC078u + 8 * i);
    }
    if (m.r32(m.r32(g::kScene) + 304) & 2u) {
        m.w16(kDifficulty, m.r16s(kDifficulty) < 2 ? m.r16(kDifficulty) + 1u : 0u); sound_play(61);
    }
    if (m.r32(m.r32(g::kScene) + 304) & 1u) {
        m.w16(kDifficulty, m.r16s(kDifficulty) > 0 ? m.r16(kDifficulty) - 1u : 2u); sound_play(61);
    }
    const u32 buttons = m.r32(m.r32(g::kScene) + 300);
    if (buttons & 16u) {
        m.w32(m.r32(g::kScene) + 916, static_cast<u32>(m.r16s(kDifficulty)));
        for (u32 i = 0; i < 60; ++i) {
            m.w8(m.r32(g::kScene) + 3340 + i, 0);
            const u32 scene = m.r32(g::kScene);
            m.w16(scene + 2980 + 2 * i, 0xD2F0);
            m.w32(scene + 3660 + 4 * i, 0); m.w32(scene + 3900 + 4 * i, 0);
        }
        const u32 state = m.r32(kState);
        m.w16(state + 76, 11); m.w16(state + 78, 0); sound_play(89);
        const u32 scene = m.r32(g::kScene);
        m.w16(scene + 2388, 2); m.w16(scene + 908, 1);
    } else if (buttons & 32u) {
        const u32 state = m.r32(kState);
        m.w16(state + 76, 9); m.w16(state + 78, 0); sound_play(59);
    }
    for (u32 i = 0; i < 3; ++i) t1_flag(6 + i, static_cast<s32>(i) == m.r16s(kDifficulty) ? 0 : 1);
    if (cardStatus == 2 || cardStatus == 6) {
        const u32 state = m.r32(kState);
        m.w16(state + 76, 2); m.w16(state + 78, 0);
        return o1_marker_set(111, 148, 0, 0);
    }
    return 6;
}
u32 Game::o1_screenshot(u32 arguments) {
    cover(0x800DA0E0u);
    Frame frame(*this, 120);
    constexpr u32 kCapture = 0x80200000u;
    const u32 rect = frame.at(16), filename = frame.at(24);
    m.w32(rect, m.r32(0x800CC58Cu)); m.w32(rect + 4, m.r32(0x800CC590u));
    m.w32(kCapture, 0x10); m.w32(kCapture + 4, 2);
    m.w32(kCapture + 8, 0x2580C); m.w32(kCapture + 12, 0); m.w32(kCapture + 16, 0x1E00140);
    m.w16(rect + 2, 0); hw->store_image(rect, kCapture + 20);
    vsprintf(filename, 0x800CC594u, arguments);
    const u32 file = hw->file_open(filename, 0x202);
    hw->file_write(file, kCapture, 0x4B014);
    return hw->file_close(file);
}
}

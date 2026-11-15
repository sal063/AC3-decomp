#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kPrims = 0x800DDA58u, kScreens = 0x800DD9DCu;
constexpr u32 kState = 0x800DD9D0u, kCards = 0x800DDA48u;
constexpr u32 kOriginX = 32, kOriginY = 34, kCursorX = 36, kCursorY = 38;
}

void Game::o1_empty_5548() { cover(0x800D5548u); }

u32 Game::o1_draw_complete() { cover(0x800DA9D4u); return 1; }

u32 Game::o1_text_fold_position(u32 position) {
    cover(0x800D7CD4u);
    const u32 prims = m.r32(kPrims);
    m.w16(prims + 72, position);
    return prims;
}
u32 Game::o1_flow_auxiliary(u32 value) {
    cover(0x800DA4F4u);
    const u32 flow = m.r32(0x800DD910u);
    m.w32(flow + 8, value);
    return flow;
}
u32 Game::o1_screen_text_select(u32 value) {
    cover(0x800DA828u);
    const u32 screens = m.r32(kScreens);
    m.w32(screens + 28, value);
    return screens;
}
u32 Game::o1_animation_table() {
    cover(0x800DA6FCu);
    return m.r32(m.r32(kScreens) + 16);
}
u32 Game::o1_clip_select(u32 clip) {
    cover(0x800DB910u);
    m.w32(m.r32(kScreens) + 44, clip);
    return clip + 16;
}
u32 Game::o1_menu_timer_advance() {
    cover(0x800D0AECu);
    const u32 state = m.r32(kState), next = m.r16(state + 78) + 1;
    m.w16(state + 78, next);
    return next;
}
u32 Game::o1_card_operation_status() {
    cover(0x800D5C80u);
    return m.r8(m.r32(m.r32(kCards) + 744) + 8);
}
u32 Game::o1_menu_delay() {
    cover(0x800DA484u);
    return 0xFFFFFFFEu;
}
u32 Game::o1_account_exit_begin() {
    cover(0x800D75E4u);
    const u32 state = m.r32(kState);
    m.w32(state + 72, 0);
    m.w16(state + 16, 0);
    m.w16(state + 14, 1);
    return state;
}
u32 Game::o1_text_origin_set(u32 x, u32 y) {
    cover(0x800D7C40u);
    const u32 prims = m.r32(kPrims);
    m.w16(prims + kOriginX, x); m.w16(prims + kCursorX, x);
    m.w16(prims + kOriginY, y); m.w16(prims + kCursorY, y);
    return prims;
}
u32 Game::o1_text_fold_set(u32 enabled, u32 from, u32 to) {
    cover(0x800D7CE4u);
    m.w8(m.r32(kPrims) + 67, enabled);
    const u32 prims = m.r32(kPrims);
    m.w16(prims + 68, from); m.w16(prims + 70, to);
    return prims;
}
u32 Game::o1_text_rgb_set(u32 red, u32 green, u32 blue) {
    cover(0x800D7C60u);
    m.w8(m.r32(kPrims) + 12, red);
    m.w8(m.r32(kPrims) + 13, green);
    const u32 prims = m.r32(kPrims);
    m.w8(prims + 14, blue);
    return prims;
}
u32 Game::o1_text_origin_get(u32 x, u32 y) {
    cover(0x800D8094u);
    m.w16(x, m.r16(m.r32(kPrims) + kOriginX));
    const u32 originY = m.r16(m.r32(kPrims) + kOriginY);
    m.w16(y, originY);
    return originY;
}
}

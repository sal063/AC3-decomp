#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kSelector = 0x80112DD0u, kScreens = 0x80112DC0u;
}

void Game::o2_selector_page_transition() {
    cover(0x800CFF5Cu);
    Frame frame(*this, 24);
    const u32 selector = m.r32(kSelector), choice = m.r8(selector + 1);
    if (choice == 0) {
        m.w8(selector, 3);
        o2_entry_reset(0, 4);
        o2_fade_set(11, 1, 0, 25);
        o2_group_set(0, 0x4011);
        const u32 selected = m.r32(kSelector);
        o2_selector_image(m.r8(selected + 2), m.r8(selected + 3));
        m.w8(m.r32(m.r32(kScreens) + 92) + 3, 0);
    } else if (choice == 1) {
        o2_group_set(0, 3);
        m.w8(m.r32(kMenu) + 124, 0);
        m.w8(m.r32(kMenu) + 125, 0);
        const u32 menu = m.r32(kMenu), buffer = m.r32(menu + 264);
        m.w16(menu + 454, 0);
        heap_set(buffer);
    }
}

u32 Game::o2_selector_dispatch(u32 second) {
    cover(0x800D07C0u);
    Frame frame(*this, 24);
    const u32 selector = m.r32(kSelector), buffer = m.r32(m.r32(g::kScene) + 4);
    m.w32(selector + 512, selector + 8 + 252 * buffer);
    const u32 fn = m.r32(0x80109D78u + 4 * m.r8(selector));
    return call_guest(fn, selector, second);
}

u32 Game::o2_scene_settings_apply(u32 settings) {
    cover(0x800DBFC4u);
    m.w8(m.r32(g::kScene) + 924, m.r8(settings) == 0 ? 1 : 0);
    const u32 setting = m.r8(settings + 1);
    if (setting <= 2) {
        const u32 scene = m.r32(g::kScene), flags = m.r32(scene + 288);
        if (setting == 2) m.w32(scene + 288, flags & ~3u);
        else if (setting == 1) m.w32(scene + 288, (flags | 1) & ~2u);
        else m.w32(scene + 288, flags | 3);
    }
    const u32 last = m.r8(settings + 2);
    if (last <= 1) m.w8(m.r32(g::kScene) + 2394, last);
    return 2;
}

void Game::o2_selector_image_ready() {
    cover(0x800D0034u);
    Frame frame(*this, 32);
    const u32 selector = m.r32(kSelector);
    const u32 id = m.r8(selector + 516 + 4 * m.r8(selector + 2) + m.r8(selector + 3));
    m.w8(m.r32(g::kScene) + 3400 + id, 2);
    o2_flag(49, 0);
    m.w32(m.r32(kSelector) + 604, 0);
    if (m.r32(m.r32(g::kScene) + 68) != 0) return;
    const u32 entries = ld_relocate_dir(m.r32(m.r32(0x800BCE50u) + 96));
    o2_upload_pack(m.r32(entries));
    heap_set(m.r32(entries));
    m.w32(m.r32(kSelector) + 600, 0);
    m.w8(m.r32(m.r32(kScreens) + 92) + 1, 0);
    m.w8(m.r32(m.r32(kScreens) + 92) + 2, 0);
    o2_fade_set(3, 0, 0, 0); o2_fade_set(4, 0, 0, 0);
    m.w8(m.r32(kSelector), 4);
}

void Game::o2_panel_loader_tick() {
    cover(0x800D12E0u);
    Frame frame(*this, 32);
    if (m.r16s(m.r32(kMenu) + 454) == 3) {
        m.w8(m.r32(m.r32(kScreens) + 92) + 50, 1);
        m.w8(m.r32(m.r32(kScreens) + 92) + 51, 0);
        m.w8(m.r32(m.r32(kScreens) + 92) + 52, m.r8(m.r32(0x80112DB8u) + 3));
        o2_panel_image_column(m.r8(m.r32(0x80112DB8u) + 2));
        o2_panel_palette(m.r8(m.r32(0x80112DB8u) + 3));
        m.w8(m.r32(kMenu) + 125, 1);
        o2_group_set(0, 11); o2_fade_set(1, 1, 0, 120); o2_flag(50, 1);
        m.w8(m.r32(g::kScene) + 2398, 0);
        o2_flag(1, m.r8(m.r32(kMenu) + 126) + 1);
    }
    const u32 phase = static_cast<u32>(m.r16s(m.r32(kMenu) + 454));
    call_guest(m.r32(0x8010984Cu + 4 * phase), 0, 0);
}

void Game::o2_selector_screen_begin() {
    cover(0x800CF854u);
    Frame frame(*this, 32);
    const u32 unlocks = m.r32(g::kScene) + 3400;
    m.w8(m.r32(kSelector), 1);
    o2_selector_image_column(m.r8(m.r32(kSelector) + 2));
    for (u32 flag : {2u, 50u, 51u, 52u}) m.w8(m.r32(m.r32(kScreens) + 92) + flag, 0);
    o2_flag(49, 0); m.w32(0x80112DD4u, 0);
    for (u32 i = 0; i < 4; ++i) {
        const u32 selector = m.r32(kSelector);
        const bool present = static_cast<s32>(i) < m.r32s(selector + 596);
        const u32 value = present ? m.r8(unlocks + m.r8(selector + 516 + i)) : 0;
        m.w8(m.r32(m.r32(kScreens) + 92) + 53 + i, value);
        m.w8(m.r32(m.r32(kScreens) + 92) + 57 + i, present ? 1 : 0);
    }
}
}

#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kSelector = 0x80112DD0u, kPanel = 0x80112DB8u;
constexpr u32 kScreens = 0x80112DC0u, kSetupDelay = 624, kIds = 516, kCount = 596;
u32 flags(Memory& m) { return m.r32(m.r32(kScreens) + 92); }
}

void Game::o2_selector_strips() {
    cover(0x800CF6C0u);
    Frame frame(*this, 40);
    auto link = [&](u32 packet) {
        const u32 menu = m.r32(kMenu), table = m.r32(menu + 8);
        m.w32(packet, (m.r32(packet) & 0xFF000000u) | (m.r32(table) & 0x00FFFFFFu));
        const u32 currentTable = m.r32(menu + 8);
        m.w32(currentTable, (m.r32(currentTable) & 0xFF000000u) | (packet & 0x00FFFFFFu));
    };
    for (u32 row = 0; row < 11; ++row) {
        const u32 packet = m.r32(m.r32(kSelector) + 512) + 20 * row;
        m.w8(packet + 3, 4); m.w8(packet + 7, 0x64);
        m.w16(packet + 8, 101); m.w16(packet + 16, 144);
        m.w16(packet + 10, 120 + 24 * row);
        m.w8(packet + 4, 128); m.w8(packet + 5, 128); m.w8(packet + 6, 128);
        m.w8(packet + 12, 0); m.w8(packet + 13, 18 * row);
        const u32 selector = m.r32(kSelector);
        m.w16(packet + 18, 18);
        const u32 palette = ((m.r8(selector + 4) + 210) << 6) | 63;
        m.w16(packet + 14, palette); link(packet);
    }
    const u32 selector = m.r32(kSelector), arena = m.r32(selector + 512);
    const u32 pageX = (((m.r8(selector + 4) >> 1) * 64 + 640) & 1023) >> 6;
    m.w32(frame.at(16), 0);
    set_draw_mode(arena + 240, 0, 0, pageX | 16, 0);
    link(arena + 240);
}

void Game::o2_selector_allocate() {
    cover(0x800D0588u);
    Frame frame(*this, 32);
    const u32 menu = m.r32(kMenu), unlocks = m.r32(g::kScene) + 3400;
    const u32 delay = m.r32(menu + kSetupDelay) + 1;
    m.w32(menu + kSetupDelay, delay);
    if (static_cast<s32>(delay) < 2) return;
    m.w32(menu + kSetupDelay, 0);
    m.w32(kSelector, mem_alloc(608));
    for (u32 field : {0u, 1u, 3u, 2u}) m.w8(m.r32(kSelector) + field, 0);
    m.w8(m.r32(kMenu) + 125, 1); m.w8(flags(m) + 5, 0);
    o2_entry_reset(0, 3); o2_group_set(0, 11);
    m.w32(m.r32(kSelector) + kCount, 0);
    for (u32 id = 0; id < 79; ++id) {
        m.w8(m.r32(kSelector) + kIds + id, 0);
        if (m.r8(unlocks + id) != 0) {
            u32 selector = m.r32(kSelector);
            m.w8(selector + kIds + m.r32(selector + kCount), id);
            selector = m.r32(kSelector);
            m.w32(selector + kCount, m.r32(selector + kCount) + 1);
        }
    }
    o2_selector_image_column(m.r8(m.r32(kSelector) + 2));
    o2_selector_palette(m.r8(m.r32(kSelector) + 3));
    for (u32 flag : {50u, 51u, 52u}) m.w8(flags(m) + flag, 0);
    for (u32 i = 0; i < 4; ++i) {
        const u32 selector = m.r32(kSelector);
        const bool present = static_cast<s32>(i) < m.r32s(selector + kCount);
        m.w8(flags(m) + 53 + i, present ? m.r8(unlocks + m.r8(selector + kIds + i)) : 0);
        m.w8(flags(m) + 57 + i, present ? 1 : 0);
    }
}

void Game::o2_panel_setup() {
    cover(0x800D09E8u);
    Frame frame(*this, 32);
    const u32 menu = m.r32(kMenu), unlocks = m.r32(g::kScene) + 3480;
    const u32 delay = m.r32(menu + kSetupDelay) + 1;
    m.w32(menu + kSetupDelay, delay);
    if (static_cast<s32>(delay) < 2) return;
    m.w32(menu + kSetupDelay, 0); m.w16(menu + 454, 0);
    for (u32 field : {0u, 1u, 3u, 2u}) m.w8(m.r32(kPanel) + field, 0);
    m.w8(m.r32(kMenu) + 125, 1); m.w8(flags(m) + 5, 1);
    o2_entry_reset(0, 3); o2_group_set(0, 11);
    m.w32(m.r32(kPanel) + 184, 0);
    for (u32 id = 26; id < 175; ++id) {
        if (m.r8(m.r32(g::kScene) + 3480 + id) != 0) {
            u32 panel = m.r32(kPanel);
            m.w8(panel + 4 + m.r32(panel + 184), id);
            panel = m.r32(kPanel);
            m.w32(panel + 184, m.r32(panel + 184) + 1);
        }
    }
    o2_panel_image_column(m.r8(m.r32(kPanel) + 2));
    o2_panel_palette(m.r8(m.r32(kPanel) + 3));
    m.w8(flags(m) + 50, 1); m.w8(flags(m) + 51, 0); m.w8(flags(m) + 52, 0);
    for (u32 i = 0; i < 4; ++i) {
        const u32 panel = m.r32(kPanel);
        const bool present = static_cast<s32>(i) < m.r32s(panel + 184);
        m.w8(flags(m) + 53 + i, present ? m.r8(unlocks + m.r8(panel + 4 + i)) : 0);
        m.w8(flags(m) + 57 + i, present ? 2 : 0);
    }
    o2_flag(49, 0); m.w32(0x80112DD4u, 0);
}
}

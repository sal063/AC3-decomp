#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kBriefing = 0x80112DB0u;
constexpr u32 kScreens = 0x80112DC0u, kAircraftPanel = 0x80112DACu;
constexpr u32 kBriefSubstate = 0x86, kScreenEntries = 0x16C;
}

u32 Game::o2_brief_screen_wait() {
    cover(0x800F132Cu);
    const u32 menu = m.r32(kMenu);
    if (m.r16s(m.r32(menu + kScreenEntries) + 0xBE) != 1) return 0x80110000u;
    const u32 briefing = m.r32(kBriefing);
    const u32 next = m.r32(briefing + 12) ? 4u : 15u;
    m.w16(menu + kBriefSubstate, next);
    if (next == 4) m.w32(briefing + 16, 0);
    return next;
}

u32 Game::o2_brief_screen_settled() {
    cover(0x800F1384u);
    const u32 menu = m.r32(kMenu);
    if (m.r16s(m.r32(menu + kScreenEntries) + 0xBE) != 1) return 0x80110000u;
    const u32 briefing = m.r32(kBriefing);
    const u32 next = m.r32(briefing + 12) ? 4u : 15u;
    m.w16(menu + kBriefSubstate, next);
    if (next == 4) m.w32(briefing + 16, 0);
    return next;
}

u32 Game::o2_brief_release() {
    cover(0x800F2990u);
    Frame frame(*this, 24);
    const s32 fade = o2_fade_state(1);
    if (fade) return static_cast<u32>(fade);
    m.w8(m.r32(kMenu) + 0x7C, 3);
    m.w8(m.r32(kMenu) + 0x7D, 3);
    const u32 briefing = m.r32(kBriefing);
    m.w16(m.r32(kMenu) + 0x1C6, 6);
    return heap_set(briefing);
}

u32 Game::o2_pilot_progress_clear() {
    cover(0x800F60A4u);
    const u32 pilot = m.r32(g::kScene) + 0x95C;
    constexpr u32 kLongProgress = 0x43C, kShortProgress = 0x3EC;
    for (u32 count = 180; count; --count) m.w8(pilot + kLongProgress + count - 1, 0);
    for (u32 count = 80; count; --count) m.w8(pilot + kShortProgress + count - 1, 0);
    return pilot - 1;
}

u32 Game::o2_cinematic_ready() {
    cover(0x800FDF38u);
    m.w8(m.r32(0x80112E1Cu), 4);
    m.w8(m.r32(m.r32(kScreens) + 0x5C) + 0x2F, 1);
    return 1;
}

u32 Game::o2_aircraft_bounds_apply() {
    cover(0x80102F9Cu);
    const u32 panel = m.r32(kAircraftPanel), bounds = m.r32(panel + 0x9C8);
    m.w16(panel + 0x9BC, (static_cast<u32>(m.r16s(bounds + 6)) - 128u) << 7);
    m.w16(panel + 0x9BE, m.r16(bounds + 8));
    m.w16(panel + 0x9C0, (static_cast<u32>(m.r16s(bounds + 10)) - 128u) << 7);
    const u32 width = static_cast<u32>(m.r16s(bounds + 4));
    m.w32(panel + 0x9C4, width);
    return width;
}

u32 Game::o2_aircraft_panel_end() {
    cover(0x80107358u);
    Frame frame(*this, 24);
    m.w8(m.r32(kMenu) + 0x7F, 1);
    o2_fade_set(2, 0, 0, 0);
    const u32 scene = m.r32(g::kScene), flags = m.r32(scene + 0x96C) | 2u;
    m.w32(scene + 0x96C, flags);
    return flags;
}

u32 Game::o2_selector_texture_upload(u32 bank) {
    cover(0x80108478u);
    Frame frame(*this, 24);
    if (bank > 1) return 1;
    const u32 menu = m.r32(kMenu);
    hw->load_image(menu + 0xD8, m.r32(menu + 0xD0 + 4 * bank));
    return 0;
}

u32 Game::o2_brief_drive_open() {
    cover(0x800F3020u);
    Frame frame(*this, 24);
    if (kcd_drive_state() != 3) return 0x80110000u;
    const u32 menu = m.r32(kMenu), next = m.r16(menu + kBriefSubstate) + 1;
    m.w16(menu + kBriefSubstate, next);
    return next;
}

u32 Game::o2_brief_drive_close() {
    cover(0x800F3064u);
    Frame frame(*this, 24);
    const u32 drive = kcd_drive_state();
    if (drive == 0 || drive == 2) {
        const u32 menu = m.r32(kMenu), next = m.r16(menu + kBriefSubstate) + 1;
        m.w16(menu + kBriefSubstate, next);
        return next;
    }
    if (drive != 1) return 0x80110000u;
    m.w8(m.r32(m.r32(kScreens) + 0x5C) + 0x3D, 4);
    return 4;
}

u32 Game::o2_hangar_drive_open() {
    cover(0x800FF098u);
    Frame frame(*this, 24);
    return kcd_drive_state() == 3 ? 1u : 0u;
}

u32 Game::o2_hangar_drive_close() {
    cover(0x800FF0BCu);
    Frame frame(*this, 24);
    if (kcd_drive_state() != 1) return 0;
    const u32 transition = m.r32(0x8011278Cu);
    m.w32(transition, 1); m.w32(transition + 4, 1);
    return 4;
}

u32 Game::o2_hangar_stage_exit() {
    cover(0x801071B8u);
    Frame frame(*this, 24);
    const u32 hangar = m.r32(0x80112DDCu);
    m.w8(hangar + 1, m.r8(hangar + 1) + 1);
    o2_fade_set(15, 1, 0, 20);
    return o2_hangar_effect1_set(3);
}
}

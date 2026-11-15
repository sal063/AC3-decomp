#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kSelection = 0x80112E0Cu;
constexpr u32 kSelectionIndex = 4, kSelectionEntries = 0x1018;
constexpr u32 kPanel = 0x80112E18u;
constexpr u32 kPanelReady = 4;
constexpr u32 kHangar = 0x80112DA8u;
constexpr u32 kHangarEffect2 = 0x18, kHangarEffect2Time = 0x24;
constexpr u32 kAircraftPanel = 0x80112DACu;
constexpr u32 kAircraftPanelBytes = 0xA7C, kPanelModel = 0x9C4;
}

s32 Game::o2_selected_id() {
    cover(0x800F787Cu);
    const u32 selection = m.r32(kSelection);
    return m.r16s(m.r32(selection + kSelectionEntries) + 12 * m.r32(selection + kSelectionIndex));
}

u32 Game::o2_loader_fade_ready() {
    cover(0x800FBC0Cu);
    Frame frame(*this, 24);
    return o2_fade_state(1) == 2 ? 3 : 0;
}

u32 Game::o2_panel_ready() {
    cover(0x800FC750u);
    m.w8(m.r32(kPanel) + kPanelReady, 1);
    return 1;
}

u32 Game::o2_hangar_effect2_begin() {
    cover(0x8010097Cu);
    const u32 hangar = m.r32(kHangar);
    m.w32(hangar + kHangarEffect2, 1);
    m.w32(hangar + kHangarEffect2Time, 0);
    return 1;
}

u32 Game::o2_hangar_effect2_exit() {
    cover(0x80100994u);
    const u32 hangar = m.r32(kHangar);
    m.w32(hangar + kHangarEffect2, 3);
    m.w32(hangar + kHangarEffect2Time, 0);
    return 3;
}

u32 Game::o2_aircraft_panel_alloc() {
    cover(0x801020C0u);
    Frame frame(*this, 24);
    const u32 panel = mem_alloc(kAircraftPanelBytes);
    m.w32(panel + kPanelModel, 0xFFFFFFFFu);
    m.w32(kAircraftPanel, panel);
    return panel;
}

u32 Game::o2_aircraft_panel_reset() {
    cover(0x801020F4u);
    const u32 panel = m.r32(kAircraftPanel);
    m.w16(panel + 0x90, 128);
    m.w16(panel + 0x92, 128);
    m.w32(panel + 0x74, 0);
    m.w32(panel + 0x78, 0);
    for (u32 axis = 0; axis < 3; ++axis) m.w16(panel + 0x7C + 2 * axis, 0);
    return panel;
}

u32 Game::o2_aircraft_panel_release() {
    cover(0x80105FD8u);
    Frame frame(*this, 24);
    return heap_set(m.r32(kAircraftPanel));
}

u32 Game::o2_aircraft_panel_records() {
    cover(0x80105F04u);
    Frame frame(*this, 32);
    for (u32 index = 0; index < 32; ++index) {
        const u32 record = mem_alloc(32);
        m.w32(m.r32(kAircraftPanel) + 0x9DC + 4 * index, record);
        m.w16(record + 4, 0);
        m.w16(record + 6, 0);
        for (u32 field : {2u, 28u, 30u}) m.w16(record + field, 0xFFFF);
    }
    return 0;
}

u32 Game::o2_aircraft_panel_slots() {
    cover(0x80105F78u);
    Frame frame(*this, 32);
    m.w16(m.r32(kAircraftPanel) + 0x9CC, 0);
    for (u32 index = 0; index < 8; ++index) {
        const u32 slot = mem_alloc(4);
        m.w32(m.r32(kAircraftPanel) + 0xA5C + 4 * index, slot);
    }
    return 0;
}
u32 Game::o2_hangar_effect1_set(u32 state) {
    cover(0x800FF19Cu);
    Frame frame(*this, 24);
    const u32 effects = m.r32(0x80112DA8u);
    m.w32(effects + 0x14, state);
    m.w32(effects + 0x20, 0);
    if (state != 1 && state != 3) return 3;
    sound_play(state == 1 ? 98 : 99);
    u32 result = 0;
    for (u32 voice = 0; voice < 5; ++voice)
        result = voice_level(m.r32(m.r32(0x80112DC4u) + 0x130 + 4 * voice), state == 1 ? 100 : 0, 120);
    return result;
}
}

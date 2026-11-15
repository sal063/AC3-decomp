#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kAircraftPanel = 0x80112DACu, kMenu = 0x80112DC4u;
constexpr u32 kModelMatrix = 0x50, kMoveSpeed = 0x74, kMoveRemaining = 0x78;
constexpr u32 kYaw = 0x7E, kModelId = 0x9C4, kPreviewState = 0x9D5;
}

u32 Game::o2_aircraft_light_half(u32 matrix) {
    cover(0x80102120u);
    s32 component = 0;
    for (u32 offset : {0u, 2u, 14u, 4u, 12u, 16u}) {
        component = m.r16s(matrix + offset) >> 1;
        m.w16(matrix + offset, static_cast<u32>(component));
    }
    return static_cast<u32>(component);
}

u32 Game::o2_aircraft_materials() {
    cover(0x80102564u);
    Frame frame(*this, 16);
    constexpr u32 colours[] = {
        0x804040, 0x606040, 0x608040, 0x408040, 0x408060,
        0x408080, 0x406080, 0x404080, 0x404060,
        0x849494, 0x90A0A0, 0xACB4B4, 0xB4C4C4,
        0xC8D2D2, 0xD2DCDC, 0xE6F0F0
    };
    const u32 panel = m.r32(kAircraftPanel);
    for (u32 colour = 0; colour < 16; ++colour)
        m.w32(panel + 0x894 + 4 * colour, colours[colour]);
    return panel;
}

u32 Game::o2_aircraft_timer_draw(u32 packet, u32 extraFormatArg) {
    cover(0x80105C20u);
    Frame frame(*this, 56);
    const u32 panel = m.r32(kAircraftPanel);
    if (m.r32s(panel + kModelId) < 0) return packet;
    for (u32 word = 0; word < 8; ++word) gte.ctc2(word, m.r32(panel + kModelMatrix + 4 * word));
    gte.mtc2(0, m.r32(panel + 0x9BC));
    gte.mtc2(1, m.r32(panel + 0x9C0));
    gte.op(0x180001u);
    const u32 screen = frame.at(16);
    m.w32(screen, gte.mfc2(14));
    m.w32(frame.at(24), gte.mfc2(8));
    m.w32(frame.at(28), gte.cfc2(31));
    m.w32(frame.at(32), static_cast<u32>(static_cast<s32>(gte.mfc2(19)) >> 2));
    o2_text_at(static_cast<u32>(m.r16s(screen) - 10), static_cast<u32>(m.r16s(screen + 2)));
    o2_text_rgb(128, 128, 128);
    const s32 seconds = m.r32s(m.r32(kAircraftPanel) + kModelId);
    o2_text_printf(0x800CD77Cu, static_cast<u32>(seconds / 60), static_cast<u32>(seconds % 60), extraFormatArg);
    return packet;
}

u32 Game::o2_aircraft_move() {
    cover(0x80105D54u);
    Frame frame(*this, 32);
    if (m.r32s(m.r32(kAircraftPanel) + kMoveRemaining) <= 0) return 0;
    const s32 sine = rsin(m.r16s(m.r32(kAircraftPanel) + kYaw));
    const s32 dx = wrap::mul(m.r32s(m.r32(kAircraftPanel) + kMoveSpeed), sine) / 4096;
    const s32 cosine = rcos(m.r16s(m.r32(kAircraftPanel) + kYaw));
    const u32 panel = m.r32(kAircraftPanel);
    const s32 dz = wrap::mul(m.r32s(panel + kMoveSpeed), cosine) / 4096;
    m.w32(panel + 0x20, m.r32(panel + 0x20) + static_cast<u32>(dx));
    m.w32(panel + kMoveRemaining, m.r32(panel + kMoveRemaining) - 1);
    m.w32(panel + 0x28, m.r32(panel + 0x28) + static_cast<u32>(dz));
    return 1;
}

u32 Game::o2_aircraft_preview_begin() {
    cover(0x80106000u);
    Frame frame(*this, 24);
    m.w8(m.r32(kAircraftPanel) + kPreviewState, 1);
    m.w32(m.r32(kMenu) + 0x50, 0);
    o2_fade_set(1, 1, 0, 120);
    for (u32 flag = 2; flag < 5; ++flag) o2_flag(flag, 0);
    return 0;
}
u32 Game::o2_aircraft_markers_move() {
    cover(0x80102FF0u);
    Frame frame(*this, 32);
    constexpr u32 kMarkers = 0x9DC, kType = 2, kRemaining = 4, kSpeed = 8, kHeading = 0x16;
    for (u32 slot = 0; slot < 32; ++slot) {
        const u32 marker = m.r32(m.r32(kAircraftPanel) + kMarkers + 4 * slot);
        if (m.r16s(marker + kType) != 2 || m.r16s(marker + kRemaining) <= 0) continue;
        const s32 sine = rsin(m.r16s(marker + kHeading));
        const s32 dx = wrap::mul(m.r16s(marker + kSpeed), sine) / 4096;
        const s32 cosine = rcos(m.r16s(marker + kHeading));
        const s32 dz = wrap::mul(m.r16s(marker + kSpeed), cosine) / 4096;
        m.w16(marker + 12, m.r16(marker + 12) + static_cast<u32>(dx));
        m.w16(marker + kRemaining, m.r16(marker + kRemaining) - 1u);
        m.w16(marker + 16, m.r16(marker + 16) + static_cast<u32>(dz));
    }
    return 0;
}

u32 Game::o2_aircraft_markers_tick() {
    cover(0x801030E0u);
    Frame frame(*this, 24);
    return o2_aircraft_markers_move();
}
u32 Game::o2_aircraft_markers_draw() {
    cover(0x80103834u);
    Frame frame(*this, 24);
    return o2_aircraft_marker_geometry();
}

u32 Game::o2_aircraft_intro() {
    cover(0x8010655Cu);
    Frame frame(*this, 24);
    o2_voice_control(1, static_cast<u32>(m.r16s(m.r32(g::kScene) + 0x38C)));
    o2_aircraft_camera();
    o2_aircraft_markers_draw();
    o2_aircraft_captions_draw();
    m.w32(m.r32(kMenu) + 0x14C, 180);
    const u32 panel = m.r32(kAircraftPanel), next = m.r8(panel + kPreviewState) + 1;
    m.w8(panel + kPreviewState, next);
    return next;
}

u32 Game::o2_aircraft_outro() {
    cover(0x8010673Cu);
    Frame frame(*this, 24);
    const u32 panel = m.r32(kAircraftPanel);
    m.w8(panel + kPreviewState, m.r8(panel + kPreviewState) + 1);
    o2_aircraft_camera();
    o2_aircraft_markers_draw();
    return o2_aircraft_captions_draw();
}

u32 Game::o2_aircraft_idle_draw() {
    cover(0x80106780u);
    Frame frame(*this, 24);
    o2_aircraft_camera();
    o2_aircraft_markers_draw();
    return o2_aircraft_captions_draw();
}
}

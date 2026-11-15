#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kPanel = 0x80112DACu, kMenu = 0x80112DC4u;
constexpr u32 kCommand = 0x9C8, kMarkerTable = 0x9DC, kCaptions = 0xA5C;
constexpr u32 kCaptionCount = 0x9CE, kCommandTable = 0x80112B9Cu;
}

u32 Game::o2_aircraft_marker_move() {
    cover(0x80102AA8u);
    Frame frame(*this, 40);
    const u32 panel = m.r32(kPanel), command = m.r32(panel + kCommand);
    const u32 marker = m.r32(panel + kMarkerTable + 4 * m.r8(command + 3));
    m.w16(marker + 2, m.r8(command + 2));
    m.w16(marker + 4, m.r16(m.r32(m.r32(kPanel) + kCommand) + 4));
    m.w16(marker + 6, m.r16(m.r32(m.r32(kPanel) + kCommand) + 4));
    const u32 current = m.r32(m.r32(kPanel) + kCommand);
    const s32 targetX = (m.r16s(current + 6) - 128) * 128;
    const s32 targetZ = (m.r16s(current + 10) - 128) * 128;
    const s32 heading = ratan2_lib(static_cast<s16>(targetX) - m.r16s(marker + 12),
                                 static_cast<s16>(targetZ) - m.r16s(marker + 16));
    const s32 dx = static_cast<s16>(targetX - m.r16(marker + 12));
    const s32 dz = static_cast<s16>(targetZ - m.r16(marker + 16));
    m.w16(marker + 22, heading);
    const s32 distance = square_root0(wrap::add(wrap::mul(dx, dx), wrap::mul(dz, dz)));
    const s32 duration = m.r16s(m.r32(m.r32(kPanel) + kCommand) + 4);
    if (duration == 0) unported(0x80102BB8u, "marker movement division break");
    if (distance == (-2147483647 - 1) && duration == -1)
        unported(0x80102BD0u, "marker movement division overflow break");
    const u32 speed = wrap::div(distance, duration).lo;
    m.w16(marker + 8, speed);
    return speed;
}

u32 Game::o2_aircraft_caption_add() {
    cover(0x80102BFCu);
    Frame frame(*this, 32);
    const u32 panel = m.r32(kPanel), slot = m.r8(panel + kCaptionCount);
    m.w16(panel + 0x9CC, m.r16(panel + 0x9CC) | (1u << (slot & 31u)));
    const u32 caption = m.r32(panel + kCaptions + 4 * slot);
    m.w8(caption, m.r8(m.r32(panel + kCommand) + 4));
    m.w8(caption + 1, m.r8(m.r32(m.r32(kPanel) + kCommand) + 3));
    const u32 current = m.r32(kPanel);
    m.w8(current + kCaptionCount, m.r8(current + kCaptionCount) + 1);
    o2_fade_set(2, 1, 0, 30);
    o2_flag(5, m.r8(caption));
    if (m.r8(m.r32(kPanel) + kCaptionCount) >= 2) {
        o2_fade_set(3, 1, 0, 30);
        o2_flag(6, m.r8(caption - 4));
    }
    if (m.r8(m.r32(kPanel) + kCaptionCount) >= 3) {
        o2_fade_set(4, 1, 0, 30);
        o2_flag(7, m.r8(caption - 8));
    }
    return sound_play(100);
}

u32 Game::o2_aircraft_wipe_begin() {
    cover(0x80102D1Cu);
    const u32 panel = m.r32(kPanel), command = m.r32(panel + kCommand);
    m.w32(panel + 0x8C, static_cast<u32>(m.r16s(command + 4)));
    const u32 height = m.r16(command + 12);
    m.w32(panel + 0x88, 0);
    m.w16(panel + 0x84, height);
    for (u32 buffer = 0; buffer < 2; ++buffer) {
        const u32 packets = m.r32(m.r32(kMenu) + 0x38 + 4 * buffer);
        m.w16(packets + 0x1274, 32); m.w16(packets + 0x1276, 300);
        m.w16(packets + 0x127C, 288); m.w16(packets + 0x127E, 300);
        m.w16(packets + 0x1284, 32); m.w16(packets + 0x128C, 288);
        m.w16(packets + 0x1286, 300 + 24 * m.r16s(m.r32(kPanel) + 0x84));
        m.w16(packets + 0x128E, 300 + 24 * m.r16s(m.r32(kPanel) + 0x84));
        m.w8(packets + 0x1278, 0); m.w8(packets + 0x1280, 255);
        m.w8(packets + 0x1279, 12 * m.r16s(m.r32(kPanel) + 0x86));
        m.w8(packets + 0x1288, 0);
        m.w8(packets + 0x1281, 12 * m.r16s(m.r32(kPanel) + 0x86));
        m.w8(packets + 0x1290, 255);
        m.w8(packets + 0x1289, 12 * (m.r16s(m.r32(kPanel) + 0x86) + m.r16s(m.r32(kPanel) + 0x84)) - 2);
        m.w8(packets + 0x1291, 12 * (m.r16s(m.r32(kPanel) + 0x86) + m.r16s(m.r32(kPanel) + 0x84)) - 2);
    }
    return 0;
}

u32 Game::o2_aircraft_move_to() {
    cover(0x80102E78u);
    Frame frame(*this, 32);
    const u32 panel = m.r32(kPanel), command = m.r32(panel + kCommand);
    m.w32(panel + 0x78, static_cast<u32>(m.r16s(command + 4)));
    const s32 dx = wrap::sub((m.r16s(command + 6) - 128) * 128, m.r32s(panel + 0x20));
    const s32 dz = wrap::sub((m.r16s(command + 10) - 128) * 128, m.r32s(panel + 0x28));
    const s32 distance = square_root0(wrap::add(wrap::mul(dx, dx), wrap::mul(dz, dz)));
    const u32 current = m.r32(kPanel);
    const s32 duration = m.r32s(current + 0x78);
    if (duration == 0) unported(0x80102F08u, "aircraft movement division break");
    if (distance == (-2147483647 - 1) && duration == -1)
        unported(0x80102F20u, "aircraft movement division overflow break");
    m.w32(current + 0x74, wrap::div(distance, duration).lo);
    const s32 heading = ratan2_lib(dx, dz);
    m.w16(m.r32(kPanel) + 0x7E, heading);
    return static_cast<u32>(heading);
}

void Game::o2_aircraft_command(u32 callback, u32 timestamp) {
    switch (callback) {
        case 0x80102650u: o2_aircraft_material_idle(); break;
        case 0x80102658u: o2_aircraft_marker_add(timestamp); break;
        case 0x80102AA8u: o2_aircraft_marker_move(); break;
        case 0x80102BFCu: o2_aircraft_caption_add(); break;
        case 0x80102D1Cu: o2_aircraft_wipe_begin(); break;
        case 0x80102E78u: o2_aircraft_move_to(); break;
        case 0x80102F5Cu: o2_aircraft_panel_centre(); break;
        case 0x80102F9Cu: o2_aircraft_bounds_apply(); break;
        case 0x80105E18u: o2_aircraft_script_idle(); break;
        case 0x80105E20u: o2_aircraft_script_tick(); break;
        case 0x80106000u: o2_aircraft_preview_begin(); break;
        case 0x8010606Cu: o2_aircraft_preview_tick(); break;
        case 0x801062B8u: o2_aircraft_playback_setup(); break;
        case 0x8010655Cu: o2_aircraft_intro(); break;
        case 0x801065C8u: o2_aircraft_playback_tick(); break;
        case 0x8010673Cu: o2_aircraft_outro(); break;
        case 0x80106780u: o2_aircraft_idle_draw(); break;
        default:
            if (callback >= 0x800F0000u && callback < 0x80110000u)
                unported(callback, "invalid aircraft script callback table entry");
            call_by_address(callback, "aircraft script callback outside o2_c", timestamp);
    }
}

u32 Game::o2_aircraft_script_tick() {
    cover(0x80105E20u);
    Frame frame(*this, 32);
    while (static_cast<u32>(m.r16s(m.r32(m.r32(kPanel) + kCommand))) == m.r32(m.r32(kMenu) + 0x50)) {
        const u32 command = m.r32(m.r32(kPanel) + kCommand);
        o2_aircraft_command(m.r32(kCommandTable + 4 * m.r8(command + 2)), static_cast<u32>(m.r16s(command)));
        const u32 panel = m.r32(kPanel);
        m.w32(panel + kCommand, m.r32(panel + kCommand) + 16);
    }
    o2_aircraft_move();
    o2_aircraft_markers_tick();
    const u32 menu = m.r32(kMenu), tick = m.r32(menu + 0x50) + 1;
    m.w32(menu + 0x50, tick);
    return tick;
}
}

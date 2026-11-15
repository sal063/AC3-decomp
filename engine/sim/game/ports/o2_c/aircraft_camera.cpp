#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kPanel = 0x80112DACu;
}

u32 Game::o2_aircraft_camera() {
    cover(0x80102184u);
    Frame frame(*this, 120);
    u32 menu = m.r32(kMenu), entries = m.r32(menu + 0x16C);
    set_geom_offset(m.r16s(entries + 0x40) + 154, m.r16s(entries + 0x42) + 154);
    u32 panel = m.r32(kPanel);
    const s32 timer = m.r32s(m.r32(kMenu) + 0x50);
    s32 screenX, screenY;
    if (!m.r8(panel + 0x9D4) && timer >= 82 && timer < 114) {
        m.w16(panel + 0x18, 1024 - (timer - 82) * 16);
        m.w16(panel + 0x1C, 0);
        m.w16(panel + 0x1A, timer * 8 + 1392);
        m.w32(panel + 0x70, 32768 - (timer - 82) * 28068 / 32);
        menu = m.r32(kMenu); entries = m.r32(menu + 0x16C);
        const s32 remaining = 114 - m.r32s(menu + 0x50);
        const s32 x = wrap::mul(m.r32s(panel + 0x20) / 256, remaining) / 32;
        const s32 z = wrap::mul(m.r32s(panel + 0x28) / 256, remaining) / 32;
        screenX = wrap::add(x, m.r16s(entries + 0x40) + 154);
        screenY = wrap::sub(m.r16s(entries + 0x42) + 154, wrap::mul(z, 2));
    } else {
        panel = m.r32(kPanel);
        m.w16(panel + 0x18, 512); m.w16(panel + 0x1A, 2304); m.w16(panel + 0x1C, 0);
        m.w32(panel + 0x70, 4700);
        entries = m.r32(m.r32(kMenu) + 0x16C);
        screenX = m.r16s(entries + 0x40) + 154; screenY = m.r16s(entries + 0x42) + 154;
    }
    set_geom_offset(screenX, screenY);
    panel = m.r32(kPanel);
    const u32 offset = frame.at(0x30), matrix = frame.at(0x38), negative = frame.at(0x10), inverseOffset = frame.at(0x20);
    m.w16(offset, 0); m.w16(offset + 2, 0); m.w16(offset + 4, m.r16(panel + 0x70));
    rot_matrix_yxz_gte(panel + 0x18, matrix);
    trans_matrix(matrix, m.r32(kPanel) + 0x20);
    for (u32 word = 0; word < 8; ++word) gte.ctc2(word, m.r32(matrix + 4 * word));
    gte.mtc2(0, m.r32(offset)); gte.mtc2(1, m.r32(offset + 4));
    gte.op(0x480012u);
    panel = m.r32(kPanel);
    for (u32 component = 0; component < 3; ++component) m.w32(panel + 8 + 4 * component, gte.mfc2(25 + component));
    m.w32(frame.at(0x58), gte.cfc2(31));
    panel = m.r32(kPanel);
    const s32 dx = wrap::sub(m.r32s(panel + 0x20), m.r32s(panel + 8)),
              dz = wrap::sub(m.r32s(panel + 0x28), m.r32s(panel + 0x10)),
              dy = wrap::sub(m.r32s(panel + 0x24), m.r32s(panel + 12));
    const s32 horizontal = square_root0(wrap::add(wrap::mul(dx, dx), wrap::mul(dz, dz)));
    const u32 pitch = static_cast<u32>(ratan2_lib(dy, horizontal));
    m.w16(m.r32(kPanel), 0u - pitch);
    const u32 yaw = static_cast<u32>(ratan2_lib(dx, dz));
    panel = m.r32(kPanel);
    m.w16(panel + 2, yaw); m.w16(panel + 4, 0);
    rot_matrix_yxz_gte(panel, panel + 0x30);
    panel = m.r32(kPanel);
    for (u32 component = 0; component < 3; ++component) m.w32(negative + 4 * component, 0u - m.r32(panel + 8 + 4 * component));
    trans_matrix(panel + 0x30, negative);
    panel = m.r32(kPanel);
    for (u32 word = 0; word < 8; ++word) m.w32(panel + 0x50 + 4 * word, m.r32(panel + 0x30 + 4 * word));
    transpose_matrix(panel + 0x30, panel + 0x50);
    o2_aircraft_light_half(m.r32(kPanel) + 0x50);
    apply_matrix_lv(m.r32(kPanel) + 0x50, negative, inverseOffset);
    return trans_matrix(m.r32(kPanel) + 0x50, inverseOffset);
}

u32 Game::o2_aircraft_marker_add(u32 timestamp) {
    cover(0x80102658u);
    const u32 panel = m.r32(kPanel), command = m.r32(panel + 0x9C8);
    const u32 slot = m.r8(command + 3), marker = m.r32(panel + 0x9DC + 4 * slot);
    m.w32(panel + 0x9D0, m.r32(panel + 0x9D0) | (1u << (slot & 31)));
    m.w16(marker + 4, timestamp + 15); m.w16(marker + 6, timestamp + 15);
    m.w16(marker + 2, m.r8(command + 2));
    m.w16(marker, m.r16(m.r32(panel + 0x9C8) + 4));
    m.w16(marker + 12, (m.r16s(m.r32(panel + 0x9C8) + 6) - 128) * 128);
    m.w16(marker + 14, -m.r16s(m.r32(panel + 0x9C8) + 8) / 10);
    const s32 z = (m.r16s(m.r32(panel + 0x9C8) + 10) - 128) * 128;
    m.w16(marker + 20, 0); m.w16(marker + 16, z); m.w16(marker + 24, 0);
    m.w16(marker + 22, m.r16s(m.r32(panel + 0x9C8) + 12) * 4096 / 360);
    for (u32 buffer = 0; buffer < 2; ++buffer) {
        const u32 packets = m.r32(m.r32(kMenu) + 0x38 + 4 * buffer);
        const u32 icon = packets + 0xB8C + 40 * m.r8(m.r32(m.r32(kPanel) + 0x9C8) + 3);
        m.w16(icon + 14, 0x303F);
        for (u32 corner = 0; corner < 4; ++corner) {
            const s32 tileX = (m.r16s(marker) % 10) * 16, tileY = (m.r16s(marker) / 10) * 16;
            m.w8(icon + 12 + 8 * corner, tileX + ((corner & 1) ? 47 : 32));
            m.w8(icon + 13 + 8 * corner, tileY + (corner >= 2 ? -49 : -64));
        }
    }
    return 0;
}
}

#include "core/wrap.hpp"
#include "sim/game/game.hpp"

namespace ac3::sim {
u32 Game::o2_aircraft_marker_geometry() {
    cover(0x80103100u);
    Frame frame(*this, 216);
    constexpr u32 kMenu = 0x80112DC4u, kPanel = 0x80112DACu;
    const u32 packets = m.r32(m.r32(kMenu) + 0x40);
    const u32 corners = frame.at(16), projected = frame.at(48), local = frame.at(80),
              composed = frame.at(112), position = frame.at(144);
    auto magnitude = [](s32 value) { return value < 0 ? wrap::neg(value) : value; };
    for (u32 slot = 0; slot < 32; ++slot) {
        u32 panel = m.r32(kPanel);
        if (!(m.r32(panel + 0x9D0) & (1u << slot))) continue;
        const u32 marker = m.r32(panel + 0x9DC + 4 * slot);
        m.w16(marker + 28, 0xFFFF); m.w16(marker + 30, 0xFFFF);
        if (magnitude(wrap::sub(m.r16s(marker + 12), m.r32s(panel + 0x20))) > 1024) continue;
        if (magnitude(wrap::sub(m.r16s(marker + 16), m.r32s(panel + 0x28))) > 1024) continue;
        s32 extent = 128;
        if (m.r16s(marker + 2) == 1 && m.r16s(marker + 4) > 0) {
            const s32 total = m.r16s(marker + 6), remaining = m.r16s(marker + 4);
            if (!total) unported(0x80103218u, "marker growth divides by zero");
            extent = (total - remaining) * 128 / total;
            m.w16(marker + 4, m.r16(marker + 4) - 1);
        }
        const s32 x[] = {-extent, extent, -extent, extent}, z[] = {extent, extent, -extent, -extent};
        for (u32 corner = 0; corner < 4; ++corner) {
            m.w16(corners + 8 * corner, x[corner]); m.w16(corners + 8 * corner + 2, 0);
            m.w16(corners + 8 * corner + 4, z[corner]);
        }
        rot_matrix_gte(marker + 20, local);
        for (u32 component = 0; component < 3; ++component) m.w32(position + 4 * component, m.r16s(marker + 12 + 2 * component));
        trans_matrix(local, position);
        panel = m.r32(kPanel);
        for (u32 word = 0; word < 5; ++word) gte.ctc2(word, m.r32(panel + 0x50 + 4 * word));
        for (u32 column = 0; column < 3; ++column) {
            for (u32 row = 0; row < 3; ++row) gte.mtc2(9 + row, m.r16(local + 6 * row + 2 * column));
            gte.op(0x49E012u);
            for (u32 row = 0; row < 3; ++row) m.w16(composed + 6 * row + 2 * column, gte.mfc2(9 + row));
        }
        panel = m.r32(kPanel);
        for (u32 component = 0; component < 3; ++component) gte.ctc2(5 + component, m.r32(panel + 0x64 + 4 * component));
        gte.mtc2(0, m.r16(local + 20) | (m.r16(local + 24) << 16));
        gte.mtc2(1, m.r32(local + 28));
        gte.op(0x480012u);
        for (u32 component = 0; component < 3; ++component) m.w32(composed + 20 + 4 * component, gte.mfc2(25 + component));
        for (u32 word = 0; word < 8; ++word) gte.ctc2(word, m.r32(composed + 4 * word));
        for (u32 corner = 0; corner < 3; ++corner) {
            gte.mtc2(2 * corner, m.r32(corners + 8 * corner));
            gte.mtc2(2 * corner + 1, m.r32(corners + 8 * corner + 4));
        }
        gte.op(0x280030u);
        for (u32 corner = 0; corner < 3; ++corner) m.w32(projected + 8 * corner, gte.mfc2(12 + corner));
        m.w32(frame.at(160), gte.mfc2(8)); m.w32(frame.at(164), gte.cfc2(31));
        m.w32(frame.at(168), static_cast<s32>(gte.mfc2(19)) >> 2);
        gte.mtc2(0, m.r32(corners + 24)); gte.mtc2(1, m.r32(corners + 28));
        gte.op(0x180001u);
        m.w32(projected + 24, gte.mfc2(14));
        m.w32(frame.at(160), gte.mfc2(8)); m.w32(frame.at(164), gte.cfc2(31));
        m.w32(frame.at(168), static_cast<s32>(gte.mfc2(19)) >> 2);
        m.w16(marker + 28, (m.r16s(projected) + m.r16s(projected + 24)) / 2);
        m.w16(marker + 30, (m.r16s(projected + 2) + m.r16s(projected + 26)) / 2);
        const u32 packet = packets + 0xB8C + 40 * slot;
        for (u32 corner = 0; corner < 4; ++corner) {
            m.w16(packet + 8 + 8 * corner, m.r16(projected + 8 * corner));
            m.w16(packet + 10 + 8 * corner, m.r16(projected + 8 * corner + 2));
        }
        const u32 menu = m.r32(kMenu), ot = m.r32(menu + 0x14);
        m.w32(packet, (m.r32(packet) & 0xFF000000u) | (m.r32(ot) & 0x00FFFFFFu));
        const u32 currentOt = m.r32(menu + 0x14);
        m.w32(currentOt, (m.r32(currentOt) & 0xFF000000u) | (packet & 0x00FFFFFFu));
    }
    return 0;
}
}

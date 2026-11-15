#include "core/wrap.hpp"
#include "sim/game/game.hpp"

namespace ac3::sim {
u32 Game::o2_aircraft_terrain(u32 packet) {
    cover(0x801048F0u);
    Frame frame(*this, 72);
    constexpr u32 kMenu = 0x80112DC4u, kPanel = 0x80112DACu, kTile = 0x1F800000u;
    auto link = [&](u32 primitive) {
        const u32 ot = m.r32(m.r32(kMenu) + 20) + 0x3FC;
        m.w32(primitive, (m.r32(primitive) & 0xFF000000u) | (m.r32(ot) & 0xFFFFFFu));
        m.w32(ot, (m.r32(ot) & 0xFF000000u) | (primitive & 0xFFFFFFu));
    };
    set_draw_area(packet, 0x800CD774u); link(packet); packet += 12;
    hw->vsync(1);
    const u32 panel = m.r32(kPanel), menu = m.r32(kMenu);
    if (m.r8(panel + 0x9D4) || m.r32s(menu + 80) >= 83) {
        for (u32 word = 0; word < 8; ++word) gte.ctc2(word, m.r32(panel + 80 + 4 * word));
        auto height = [&](s32 x, s32 z) {
            const s32 pixel = wrap::add(wrap::add(x, wrap::mul(wrap::add(z, 128), 258)), 128);
            const u32 packed = m.r8(m.r32(0x80112DC8u) + static_cast<u32>(pixel / 2));
            return ((packed >> ((static_cast<u32>(x) & 1u) * 4)) & 15u) * 128;
        };
        auto vertex = [&](u32 corner, s32 x, s32 z, u32 elevation) {
            const u32 destination = kTile + 32 + 8 * corner;
            m.w16(destination, wrap::mul(x, 128)); m.w16(destination + 2, 0u - elevation);
            m.w16(destination + 4, wrap::mul(z, 128));
        };
        auto interpolateHeight = [&](u32 destination, u32 start, u32 end, s32 fraction) {
            const s32 delta = m.r16s(kTile + 8 + 2 * end) - m.r16s(kTile + 8 + 2 * start);
            const u32 value = m.r16(kTile + 8 + 2 * destination) + static_cast<u32>(delta * fraction / 128);
            m.w16(kTile + 8 + 2 * destination, value);
            m.w16(kTile + 34 + 8 * destination, 0u - m.r16(kTile + 8 + 2 * destination));
        };
        auto interpolateColour = [&](u32 destination, u32 source, s32 fraction) {
            const u32 from = kTile + 16 + 4 * destination, toward = kTile + 16 + 4 * source;
            u32 colour = static_cast<u32>(m.r8(from + 3)) << 24;
            for (u32 channel = 0; channel < 3; ++channel) {
                const s32 start = m.r8(from + channel), end = m.r8(toward + channel);
                colour |= static_cast<u32>(start + (end - start) * fraction / 128) << (8 * channel);
            }
            m.w32(from, colour);
        };
        for (s32 row = -8; row <= 8; ++row) {
            const s32 centreZ = m.r32s(panel + 40);
            m.w16(kTile + 2, row + centreZ / 128 - (centreZ < 0 ? 1 : 0));
            m.w16(kTile + 6, centreZ % 128 + (centreZ < 0 ? 128 : 0));
            for (s32 column = -8; column <= 8; ++column) {
                const s32 centreX = m.r32s(panel + 32); m.w32(frame.at(48), centreX);
                m.w16(kTile, column + centreX / 128 - (centreX < 0 ? 1 : 0));
                m.w16(kTile + 4, centreX % 128 + (centreX < 0 ? 128 : 0));
                const s32 x = m.r16s(kTile), z = m.r16s(kTile + 2);
                if (column == -8) {
                    m.w16(kTile + 8, height(x, z)); vertex(0, x, z, m.r16(kTile + 8));
                } else {
                    m.w32(kTile + 32, m.r32(kTile + 40)); m.w32(kTile + 36, m.r32(kTile + 44));
                    m.w16(kTile + 8, m.r16(kTile + 10));
                }
                m.w16(kTile + 10, height(x + 1, z)); m.w16(kTile + 12, height(x, z + 1));
                m.w16(kTile + 14, height(x + 1, z + 1));
                vertex(1, x + 1, z, m.r16(kTile + 10)); vertex(2, x, z + 1, m.r16(kTile + 12));
                vertex(3, x + 1, z + 1, m.r16(kTile + 14));
                const s32 fractionX = m.r16s(kTile + 4), fractionZ = m.r16s(kTile + 6);
                if (column == -8) {
                    for (u32 corner : {0u, 2u}) m.w16(kTile + 32 + 8 * corner, m.r16(kTile + 32 + 8 * corner) + fractionX);
                    interpolateHeight(0, 0, 1, fractionX); interpolateHeight(2, 2, 3, fractionX);
                }
                if (column == 8) {
                    for (u32 corner : {1u, 3u}) m.w16(kTile + 32 + 8 * corner, m.r16(kTile + 32 + 8 * corner) + fractionX - 128);
                    interpolateHeight(1, 0, 1, fractionX - 128); interpolateHeight(3, 2, 3, fractionX - 128);
                }
                if (row == -8) {
                    for (u32 corner : {0u, 1u}) m.w16(kTile + 36 + 8 * corner, m.r16(kTile + 36 + 8 * corner) + fractionZ);
                    interpolateHeight(0, 0, 2, fractionZ); interpolateHeight(1, 1, 3, fractionZ);
                }
                if (row == 8) {
                    for (u32 corner : {2u, 3u}) m.w16(kTile + 36 + 8 * corner, m.r16(kTile + 36 + 8 * corner) + fractionZ - 128);
                    interpolateHeight(2, 0, 2, fractionZ - 128); interpolateHeight(3, 1, 3, fractionZ - 128);
                }
                if (column == -8) {
                    gte.mtc2(0, m.r32(kTile + 32)); gte.mtc2(1, m.r32(kTile + 36)); gte.op(0x180001u);
                    m.w32(packet + 8, gte.mfc2(14));
                } else m.w32(packet + 8, gte.mfc2(12));
                for (u32 corner = 0; corner < 3; ++corner) {
                    gte.mtc2(2 * corner, m.r32(kTile + 40 + 8 * corner));
                    gte.mtc2(2 * corner + 1, m.r32(kTile + 44 + 8 * corner));
                }
                gte.op(0x280030u);
                for (u32 corner = 0; corner < 4; ++corner)
                    m.w32(kTile + 16 + 4 * corner, m.r32(panel + 0x894 + 4 * static_cast<u32>(m.r16s(kTile + 8 + 2 * corner) / 128)));
                for (u32 corner = 0; corner < 3; ++corner) m.w32(packet + 16 + 8 * corner, gte.mfc2(12 + corner));
                const s32 depth = static_cast<s32>(gte.mfc2(19)) >> 2; m.w32(frame.at(48), depth);
                if (column == -8) { interpolateColour(0, 1, fractionX); interpolateColour(2, 3, fractionX); }
                if (row == -8) { interpolateColour(0, 2, fractionZ); interpolateColour(1, 3, fractionZ); }
                if (column == 8) { interpolateColour(1, 0, fractionX - 128); interpolateColour(3, 2, fractionX - 128); }
                if (row == 8) { interpolateColour(2, 0, fractionZ - 128); interpolateColour(3, 1, fractionZ - 128); }
                m.w32(packet + 4, m.r32(kTile + 16) | 0x38000000u);
                for (u32 corner = 1; corner < 4; ++corner) m.w32(packet + 4 + 8 * corner, m.r32(kTile + 16 + 4 * corner));
                const u32 bucket = m.r32(m.r32(kMenu) + 20) + 4 * static_cast<u32>(depth >> 6);
                m.w32(packet, m.r32(bucket) | 0x08000000u); m.w32(bucket, packet & 0xFFFFFFu); packet += 36;
            }
        }
    }
    if (m.r8(panel + 0x9D4) || m.r32s(menu + 80) < 60) return packet;
    const s32 tick = m.r32s(menu + 80), anchorY = m.r16(m.r32(menu + 0x16C) + 0x42);
    const s32 left = tick < 66 ? 154 - 8 * (tick - 60) : 96;
    const s32 right = tick < 66 ? 8 * tick - 326 : 224;
    const s32 top = tick < 66 ? anchorY + 153 : tick < 82 ? anchorY + 154 - 8 * (tick - 66) : anchorY + 26;
    const s32 bottom = tick < 66 ? anchorY + 155 : tick < 82 ? anchorY + 154 + 8 * (tick - 66) : anchorY + 282;
    for (u32 corner = 0; corner < 4; ++corner) {
        m.w16(frame.at(16 + 8 * corner), (corner & 1u) ? right : left);
        m.w16(frame.at(18 + 8 * corner), (corner & 2u) ? bottom : top);
    }
    if (tick == 60) sound_play(100); if (tick == 82) sound_play(102);
    if (static_cast<u32>(tick - 82) < 32u) {
        m.w8(packet + 3, 3); m.w8(packet + 7, 0x62);
        m.w16(packet + 8, m.r32s(panel + 32) / 256 + 146);
        m.w16(packet + 10, 154 - 2 * (m.r32s(panel + 40) / 256 + 8));
        m.w16(packet + 12, 16); m.w16(packet + 14, 32);
        for (u32 channel = 4; channel < 7; ++channel) m.w8(packet + channel, 128 - 2 * (tick - 82));
        link(packet); packet += 16;
    }
    m.w8(packet + 3, 9); m.w8(packet + 7, 0x2C);
    for (u32 corner = 0; corner < 4; ++corner) {
        m.w16(packet + 8 + 8 * corner, m.r16(frame.at(16 + 8 * corner)));
        m.w16(packet + 10 + 8 * corner, m.r16(frame.at(18 + 8 * corner)));
    }
    const s32 shade = tick < 82 ? 128 : tick < 114 ? 128 - 4 * (tick - 82) : 0;
    for (u32 channel = 4; channel < 7; ++channel) m.w8(packet + channel, shade);
    m.w16(packet + 22, 37); m.w16(packet + 14, 0x5714);
    m.w8(packet + 20, 127); m.w8(packet + 29, 127); m.w8(packet + 36, 127); m.w8(packet + 37, 127);
    m.w8(packet + 12, 0); m.w8(packet + 13, 0); m.w8(packet + 21, 0); m.w8(packet + 28, 0);
    link(packet); return packet + 40;
}
}

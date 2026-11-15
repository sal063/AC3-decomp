#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
u32 Game::draw_terrain_mesh(u32 terrain, u32 ot, u32 first) {
    cover(0x80096A48u);
    Frame frame(*this, 56);
    constexpr u32 kWork = g::kScratch, kHeights = kWork + 148;
    constexpr u32 kVertices = kWork + 252, kCorners = kWork + 156;
    constexpr u32 kPalettes = 0x800B45F4u, kColours = kWork + 76;
    m.w32(frame.at(56), terrain); m.w32(frame.at(60), ot); m.w32(frame.at(64), first);
    m.w16(kWork + 40, 0x4ED6); m.w32(kWork, first); m.w16(kWork + 42, 57);
    const u32 world = m.r32(g::kWorld);
    for (u32 reg = 0; reg < 8; ++reg) gte.ctc2(reg, m.r32(world + 76 + 4 * reg));
    m.w16(kWork + 36, m.r32(terrain + 72) + m.r32(terrain + 56));
    m.w16(kWork + 38, m.r32(terrain + 76) + m.r32(terrain + 60));
    m.w32(kWork + 20, m.r32(terrain + 88)); m.w32(kWork + 24, m.r32(terrain + 92));
    m.w32(kWork + 136, 0x3CA6C8C5u);
    for (u32 axis = 0; axis < 3; ++axis) m.w32(kWork + 4 + 4 * axis, m.r32(terrain + 4 * axis));
    const u32 camera_y = m.r32(terrain + 4);
    m.w16(kHeights, 0u - 3072 - camera_y); m.w16(kHeights + 2, 0u - 2816 - camera_y);
    m.w16(kHeights + 4, 0u - 2048 - camera_y);
    gte.ctc2(21, 3152); gte.ctc2(22, 3200); gte.ctc2(23, 2656); gte.mtc2(6, 0x3C729081u);
    for (u32 shade = 0; shade < 15; ++shade) {
        gte.mtc2(8, 273 * shade); gte.op(0x780010u); m.w32(kColours + 4 * shade, gte.mfc2(22));
    }
    gte.ctc2(21, 464); gte.ctc2(22, 912); gte.ctc2(23, 528);
    m.w32(kWork + 28, 0);
    auto boundary = [](s32 cell, s32 start) -> u32 {
        if (cell == start) return 0x00010002u;
        if (cell == start + 1) return 1;
        if (cell == start + 76) return 0x00010000u;
        return 0x00020001u;
    };
    auto colour_for = [&](s32 column, u32 row_side, u32 height_kind) {
        if (height_kind == 2) return m.r32(kWork + 136);
        const u32 palette = m.r32(kWork + 60 + 8 * row_side + ((static_cast<u32>(column >> 1)) & 4));
        return m.r32(kColours + 4 * ((palette >> (4 * (static_cast<u32>(column) & 7))) & 15));
    };
    auto project = [&](u32 out, u32 xy_field, u32 z_field, s32 column, u32 side, u32 kind) {
        const u32 xy = m.r32(xy_field), z = m.r32(z_field);
        gte.mtc2(0, xy); gte.mtc2(1, z); gte.op(0x180001u);
        m.w32(out, xy); m.w16(out + 4, z);
        const u32 colour = colour_for(column, side, kind), flags = gte.cfc2(31);
        const s32 depth = static_cast<s32>(gte.mfc2(19));
        m.w32(out + 8, gte.mfc2(14)); m.w16(out + 16, depth); gte.mtc2(6, colour);
        if (flags & 0x7E01E000u) { m.w16(out + 6, 0xFFFF); m.w32(out + 12, colour); }
        else { gte.op(0x780010u); m.w16(out + 6, screen_outcode(out + 8)); m.w32(out + 12, gte.mfc2(22)); }
        return depth;
    };
    const s32 row_count = static_cast<s32>(m.r32(kWork + 24));
    s32 row = m.r16s(kWork + 38);
    for (s32 row_index = 0; row_index < row_count; ++row_index, ++row) {
        if (static_cast<u32>(row - 87) < 74) m.w32(kWork + 140, 0);
        else if (static_cast<u32>(row - 85) < 78) m.w32(kWork + 140, boundary(row, 85));
        else { m.w32(kWork + 28, row_index + 1); continue; }
        const s16 z = static_cast<s16>((static_cast<u32>(row) << 11) - m.r16(kWork + 12));
        m.w16(kWork + 48, z);
        if (z >= 30720) { m.w32(kWork + 28, row_index + 1); continue; }
        m.w16(kWork + 56, static_cast<u16>(z) + 2048);
        m.w16(kWork + 46, m.r16(kHeights + 2 * m.r16(kWork + 140)));
        m.w16(kWork + 54, m.r16(kHeights + 2 * m.r16(kWork + 142)));
        for (u32 side = 0; side < 2; ++side) {
            const u32 palette = kPalettes + 8 * ((static_cast<u32>(row) + side - 6) & 15);
            const bool outside = m.r16(kWork + 140 + 2 * side) == 2;
            m.w32(kWork + 60 + 8 * side, outside ? 0xFFFFFFFFu : m.r32(palette));
            m.w32(kWork + 64 + 8 * side, outside ? 0xFFFFFFFFu : m.r32(palette + 4));
        }
        m.w32(kWork + 16, m.r32(terrain + 96 + 4 * row_index)); m.w32(kWork + 32, 0);
        s32 column = m.r16s(kWork + 36), run = 0;
        while (m.r32(kWork + 16) && static_cast<s32>(m.r32(kWork + 32)) < static_cast<s32>(m.r32(kWork + 20))) {
            bool usable = (m.r32(kWork + 16) & 1) != 0;
            if (usable) {
                if (static_cast<u32>(column - 63) < 74) m.w32(kWork + 144, 0);
                else if (static_cast<u32>(column - 61) < 78 && !m.r32(kWork + 140)) m.w32(kWork + 144, boundary(column, 61));
                else usable = false;
            }
            if (usable) {
                const s16 x = static_cast<s16>((static_cast<u32>(column) << 11) - m.r16(kWork + 4));
                m.w16(kWork + 44, x); usable = x < 30720;
                if (usable) m.w16(kWork + 52, x);
            }
            if (!usable) run = 0;
            else {
                s32 farthest;
                if (run == 0) {
                    if (m.r32(kWork + 144)) {
                        const u32 height = m.r16(kHeights + 2 * m.r16(kWork + 144));
                        m.w16(kWork + 54, height); m.w16(kWork + 46, height);
                    } else if (!m.r32(kWork + 140)) {
                        m.w16(kWork + 54, m.r16(kHeights)); m.w16(kWork + 46, m.r16(kHeights));
                    }
                    farthest = project(kVertices, kWork + 44, kWork + 48, column - 4, 0, m.r16(kWork + 144));
                    const s32 bottom = project(kVertices + 40, kWork + 52, kWork + 56, column - 4, 1, m.r16(kWork + 144));
                    if (bottom > farthest) farthest = bottom;
                } else {
                    const s32 top = m.r16s(kVertices + 20 * (run & 1) + 16);
                    const s32 bottom = m.r16s(kVertices + 20 * (run & 1) + 56);
                    farthest = top > bottom ? top : bottom;
                }
                if (m.r32(kWork + 144)) {
                    const u32 height = m.r16(kHeights + 2 * m.r16(kWork + 146));
                    m.w16(kWork + 54, height); m.w16(kWork + 46, height);
                }
                m.w16(kWork + 44, m.r16(kWork + 44) + 2048); m.w16(kWork + 52, m.r16(kWork + 44));
                const u32 right = kVertices + 20 * ((run + 1) & 1);
                const s32 top = project(right, kWork + 44, kWork + 48, column - 3, 0, m.r16(kWork + 146));
                if (top > farthest) farthest = top;
                const s32 bottom = project(right + 40, kWork + 52, kWork + 56, column - 3, 1, m.r16(kWork + 146));
                ++run; if (bottom > farthest) farthest = bottom;
                const u32 left = kVertices + 20 * ((run + 1) & 1);
                const u32 points[4] = {left, right, left + 40, right + 40};
                const u16 ca = m.r16(kVertices + 6), cb = m.r16(kVertices + 26);
                const u16 cc = m.r16(kVertices + 46), cd = m.r16(kVertices + 66);
                bool accepted = static_cast<u32>(farthest - 121) < 0x777Fu && !(ca & cb & cc & cd);
                s32 combined = static_cast<s16>(ca | cb | cc | cd);
                if (accepted && combined >= 0) {
                    gte.mtc2(15, m.r32(right + 8)); gte.mtc2(15, m.r32(left + 48)); gte.op(0x1400006u);
                    const s32 back = static_cast<s32>(gte.mfc2(24)); accepted = back < 0;
                    if (accepted) {
                        gte.mtc2(12, m.r32(left + 8)); gte.op(0x1400006u);
                        const s32 front = static_cast<s32>(gte.mfc2(24)); accepted = front > 0;
                        const s32 area = wrap::sub(front, back);
                        if (accepted && area >= 1025 && ((wrap::mul(area, farthest - combined) >> 16) >= 513 ||
                            (combined > 0 && quad_exceeds_gpu_span(m.r32(kVertices + 8), m.r32(kVertices + 28),
                                                                  m.r32(kVertices + 48), m.r32(kVertices + 68))))) combined = -1;
                    }
                }
                if (accepted) {
                    if (combined < 0) {
                        constexpr u16 kUv[4] = {0x8000, 0x803F, 0xBF00, 0xBF3F};
                        for (u32 corner = 0; corner < 4; ++corner) {
                            m.w32(kCorners + 4 * corner, points[corner]); m.w16(points[corner] + 18, kUv[corner]);
                        }
                        subdivide_terrain_quad(kCorners, kWork + 332, kWork, 4);
                    } else {
                        const u32 packet = m.r32(kWork), end = packet + 52;
                        m.w32(packet, 0x0C000000u | (end & 0xFFFFFFu));
                        m.w16(packet + 36, 0xBF00); m.w32(packet + 12, 0x4ED68000u);
                        m.w32(packet + 24, 0x0039803Fu); m.w16(packet + 48, 0xBF3F);
                        for (u32 corner = 0; corner < 4; ++corner) {
                            m.w32(packet + 4 + 12 * corner, m.r32(points[corner] + 12));
                            m.w32(packet + 8 + 12 * corner, m.r32(points[corner] + 8));
                        }
                        m.w32(kWork, end);
                    }
                }
            }
            ++column; m.w32(kWork + 32, m.r32(kWork + 32) + 1); m.w32(kWork + 16, m.r32(kWork + 16) >> 1);
        }
        m.w32(kWork + 28, row_index + 1);
    }
    const u32 end = m.r32(kWork);
    if (end != first) {
        for (u32 byte = 0; byte < 3; ++byte) {
            m.w8(end - 52 + byte, m.r8(ot + byte)); m.w8(ot + byte, first >> (8 * byte));
        }
    }
    return end;
}
}

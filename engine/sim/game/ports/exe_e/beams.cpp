#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

#include <algorithm>
#include <cstdlib>

namespace ac3::sim {
namespace {
constexpr u32 kBeamWork = 0x1F800000u;
constexpr u32 kBeamColour = 0x2E838383u;
}

u32 Game::draw_beam_segments(u32 segments, u32 ot, u32 packet,
                             s32 width_scale, s32 lifetime, u32 fade_shift,
                             u32 uv0, u32 uv2, u32 uv3, u32 uv1,
                             u32 divide_break0, u32 divide_break1) {
    if (m.r32s(segments + 12) < 0) return packet;
    gte.mtc2(6, kBeamColour);
    for (u32 i = 21; i <= 23; ++i) gte.ctc2(i, 0);
    const s32 h = m.r16s(m.r32(m.r32(g::kWorld) + 40) + 36);
    const s32 width = h * width_scale, near_depth = h >> 2;
    auto divide = [&](s32 depth, u32 break_at) {
        if (depth) return width / depth;
        unported(break_at, "beam projection divides by zero (original BREAK)");
        return width < 0 ? 1 : -1;
    };
    for (u32 segment = segments; m.r32s(segment + 12) >= 0; segment += 16) {
        const s32 age = m.r32s(segment + 12);
        if (!age) continue;
        const s32 depth0 = m.r16s(segment + 8), depth1 = m.r16s(segment + 10);
        if (depth0 <= near_depth || depth1 <= near_depth) continue;
        gte.mtc2(8, age < lifetime ? static_cast<u32>(lifetime - age) << fade_shift : 0);
        const u32 point0 = m.r32(segment), point1 = m.r32(segment + 4);
        gte.op(0x780010);
        m.w32(kBeamWork + 12, point0);
        const s32 size0 = std::max(1, divide(depth0, divide_break0));
        m.w32(kBeamWork + 8, point1);
        const s32 size1 = std::max(1, divide(depth1, divide_break1));
        m.w32(kBeamWork + 16, static_cast<u32>(size1));
        m.w32(kBeamWork + 20, static_cast<u32>(size0));
        m.w32(packet + 4, gte.mfc2(22));
        const s32 ax = m.r16s(kBeamWork + 8), ay = m.r16s(kBeamWork + 10);
        const s32 bx = m.r16s(kBeamWork + 12), by = m.r16s(kBeamWork + 14);
        if ((ax + size0 > 160 && bx + size1 > 160) ||
            (ax - size0 < -160 && bx - size1 < -160) ||
            (ay + size0 > 120 && by + size1 > 120) ||
            (ay - size0 < -120 && by - size1 < -120)) continue;
        const u32 bucket = ot + 4u * static_cast<u32>((depth0 + depth1) >> 4);
        m.w32(packet, m.r32(bucket) | 0x09000000u);
        m.w32(bucket, packet & 0xFFFFFFu);
        m.w16(packet + 28, uv2);
        m.w32(packet + 20, uv1);
        m.w32(packet + 12, uv0);
        m.w16(packet + 36, uv3);
        beam_quad_corners(packet, kBeamWork);
        packet += 40;
    }
    return packet;
}

u32 Game::draw_narrow_beams(u32 segments, u32 ot, u32 packet) {
    cover(0x80090BF8u);
    Frame frame(*this, 56);
    return draw_beam_segments(segments, ot, packet, 2, 28, 7,
                             0x449660B0u, 0x9FB0, 0x9FBF, 0x003860BFu,
                             0x80090CDCu, 0x80090D10u);
}

u32 Game::draw_medium_beams(u32 segments, u32 ot, u32 packet) {
    cover(0x80090ED0u);
    Frame frame(*this, 56);
    return draw_beam_segments(segments, ot, packet, 3, 28, 7,
                             0x44D660C0u, 0x9FC0, 0x9FCF, 0x003860CFu,
                             0x80090FBCu, 0x80090FF0u);
}

u32 Game::draw_broad_beams(u32 segments, u32 ot, u32 packet) {
    cover(0x80091370u);
    Frame frame(*this, 56);
    return draw_beam_segments(segments, ot, packet, 6, 14, 8,
                             0x455660D0u, 0x9FD0, 0x9FDF, 0x003860DFu,
                             0x80091460u, 0x80091494u);
}

u32 Game::draw_beam_squares(u32 segments, u32 ot, u32 packet) {
    cover(0x800911B0u);
    if (m.r32s(segments + 12) < 0) return packet;
    const s32 h = m.r16s(m.r32(m.r32(g::kWorld) + 40) + 36);
    gte.mtc2(6, kBeamColour);
    for (u32 i = 21; i <= 23; ++i) gte.ctc2(i, 0);
    for (u32 segment = segments; m.r32s(segment + 12) >= 0; segment += 16) {
        const s32 age = m.r32s(segment + 12);
        if (!age) continue;
        const s32 depth = m.r16s(segment + 10);
        const u32 point = m.r32(segment + 4);
        const s32 x = static_cast<s16>(point), y = static_cast<s32>(point) >> 16;
        if (depth <= (h >> 1) || std::abs(x) >= 171 || std::abs(y) >= 131) continue;
        gte.mtc2(8, age < 28 ? static_cast<u32>(28 - age) << 7 : 0);
        const u32 bucket = ot + 4u * static_cast<u32>(depth >> 3);
        m.w32(packet, m.r32(bucket) | 0x09000000u);
        m.w32(bucket, packet & 0xFFFFFFu);
        gte.op(0x780010);
        s32 size;
        if (depth) size = 20 * h / depth;
        else {
            unported(0x800912B8u, "beam square divides by zero (original BREAK)");
            size = h < 0 ? 1 : -1;
        }
        size = std::max(2, size);
        const s32 left = x - (size >> 1), top = y - (size >> 1);
        m.w16(packet + 32, static_cast<u32>(left + size));
        m.w16(packet + 16, static_cast<u32>(left + size));
        m.w16(packet + 34, static_cast<u32>(top + size));
        m.w16(packet + 26, static_cast<u32>(top + size));
        m.w16(packet + 28, 0x7FE0);
        m.w16(packet + 24, static_cast<u32>(left));
        m.w16(packet + 8, static_cast<u32>(left));
        m.w16(packet + 18, static_cast<u32>(top));
        m.w16(packet + 10, static_cast<u32>(top));
        m.w32(packet + 12, 0x451660E0u);
        m.w32(packet + 20, 0x003860FFu);
        m.w16(packet + 36, 0x7FFF);
        m.w32(packet + 4, gte.mfc2(22));
        packet += 40;
    }
    return packet;
}
}

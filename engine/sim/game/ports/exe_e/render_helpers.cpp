#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

#include <algorithm>

namespace ac3::sim {
namespace {
constexpr u32 kSkyState = 0x800BCF90u;
constexpr u32 kPacketCursor = 4, kDepthShift = 12, kProjectedPoint = 16;
constexpr u32 kHorizonSegments = 44, kClipCodes = 124, kClipAreas = 152;
constexpr u32 kScreenCorners = 232;
}

u32 Game::packet_fade(u32 at, u32 colour, s32 level) {
    cover(0x80095018u);
    if (level <= 0) return 1;
    if (level >= 4096) {
        m.w32(at, colour);
        return colour & 255u;
    }
    gte.ctc2(21, (colour & 255u) << 4);
    gte.ctc2(22, ((colour >> 8) & 255u) << 4);
    const u32 blue = (colour >> 12) & 0xFF0u;
    gte.ctc2(23, blue);
    gte.mtc2(6, m.r32(at));
    gte.mtc2(8, static_cast<u32>(level));
    gte.op(0x780010);
    m.w32(at, gte.mfc2(22));
    return blue;
}

u32 Game::projected_glow(u32 context) {
    cover(0x8008D4E8u);
    gte.mtc2(0, 0);
    gte.mtc2(1, static_cast<u32>(-30));
    const u32 packet = m.r32(context + kPacketCursor);
    gte.op(0x180001);
    m.w32(packet + 20, 0x0037203Fu);
    m.w16(packet + 28, 0x3F20);
    m.w32(packet + 12, 0x46562020u);
    m.w16(packet + 36, 0x3F3F);
    m.w32(packet + 4, 0x2E838383u);
    m.w32(context + kProjectedPoint, gte.mfc2(14));
    const s32 depth = wrap::sar(static_cast<s32>(gte.mfc2(19)), m.r32(context + kDepthShift));
    const u32 scale = gte.cfc2(26) << 3;
    if (static_cast<u32>(depth) - 31u >= 0x77D9u) return scale;
    const s32 size = std::max(3, static_cast<s32>(scale) / depth);
    const Gte::Exact centre = gte.exact_fifo[2];
    const u32 word = gte.mfc2(14);
    const u32 bucket = m.r32(context) + 4u * static_cast<u32>(depth >> 3);
    m.w32(packet, m.r32(bucket) | 0x09000000u);
    m.w32(m.r32(context) + 4u * static_cast<u32>(depth >> 3), packet & 0xFFFFFFu);
    m.w32(context + kPacketCursor, packet + 40);
    m.w16(context + kProjectedPoint, m.r16(context + kProjectedPoint) - static_cast<u32>(size >> 1));
    m.w16(context + kProjectedPoint + 2, m.r16(context + kProjectedPoint + 2) - static_cast<u32>(size >> 1));
    for (u32 corner = 0; corner < 4; ++corner) {
        m.w16(packet + 8 + 8 * corner, m.r16(context + kProjectedPoint) + ((corner & 1u) ? size : 0));
        m.w16(packet + 10 + 8 * corner, m.r16(context + kProjectedPoint + 2) + ((corner & 2u) ? size : 0));
    }
    if (centre.ok && centre.vz > 0) {
        const double exact = std::max(3.0, static_cast<double>(scale) /
                                               (static_cast<double>(centre.vz) / static_cast<double>(1u << (m.r32(context + kDepthShift) & 31u))));
        fx_begin(word, centre, exact / size);
        fx_record(packet, packet + 40);
        fx_end();
    }
    return m.r16(context + kProjectedPoint + 2) + static_cast<u32>(size);
}

u32 Game::beam_quad_corners(u32 packet, u32 work) {
    cover(0x8008DE18u);
    Frame frame(*this, 32);
    m.w16(work, m.r16(work + 12) - m.r16(work + 8));
    m.w16(work + 2, m.r16(work + 14) - m.r16(work + 10));
    const bool coincident = m.r32(work) == 0;
    m.w16(work + 4, 0);
    if (coincident) m.w16(work, 4096);
    else vector_normal_ss(work, work);
    u32 result = 0;
    for (u32 end = 0; end < 2; ++end) {
        const u32 point = work + 8 + 4 * end;
        const u32 width = m.r32(work + 16 + 4 * end);
        const u32 first = packet + 8 + 16 * end;
        if (!width) {
            result = m.r32(point);
            m.w32(first + 8, result);
            m.w32(first, result);
            continue;
        }
        gte.mtc2(8, width);
        for (u32 side = 0; side < 2; ++side) {
            gte.mtc2(25, m.r16(point));
            gte.mtc2(26, m.r16(point + 2));
            const s32 dx = m.r16s(work), dy = m.r16s(work + 2);
            const s32 x = end ? (side ? dx + dy : dx - dy) : (side ? dy - dx : -dx - dy);
            const s32 y = end ? (side ? dy - dx : dx + dy) : (side ? -dx - dy : dx - dy);
            gte.mtc2(9, static_cast<u32>(x));
            gte.mtc2(10, static_cast<u32>(y));
            gte.op(0x1A8003E);
            m.w16(first + 8 * side, gte.mfc2(25));
            result = gte.mfc2(26);
            m.w16(first + 8 * side + 2, result);
        }
    }
    return result;
}

u32 Game::draw_fading_lines(u32 segments, u32 ot, u32 packet) {
    cover(0x80090A74u);
    if (m.r32s(segments + 12) < 0) return packet;
    for (u32 i = 21; i <= 23; ++i) gte.ctc2(i, 0);
    gte.mtc2(6, 0x52000000u);
    const s32 near_depth = m.r16s(m.r32(m.r32(g::kWorld) + 40) + 36) >> 2;
    for (u32 segment = segments; m.r32s(segment + 12) >= 0; segment += 16) {
        const s32 age = m.r32s(segment + 12), depth = m.r16s(segment + 8);
        if (!age || depth <= near_depth) continue;
        gte.mtc2(8, age < 28 ? static_cast<u32>(28 - age) << 7 : 0);
        gte.mtc2(20, 0x0006433Du);
        gte.mtc2(21, 0x0014CFBFu);
        const u32 a = m.r32(segment), b = m.r32(segment + 4);
        gte.op(0xF8002A);
        const s32 ax = static_cast<s16>(a), bx = static_cast<s16>(b);
        const s32 ay = static_cast<s32>(a) >> 16, by = static_cast<s32>(b) >> 16;
        if ((ay > 120 && by > 120) || (ay < -120 && by < -120) ||
            (ax > 160 && bx > 160) || (ax < -160 && bx < -160)) continue;
        m.w32(packet + 8, a);
        m.w32(packet + 16, b);
        const u32 bucket = ot + 4u * static_cast<u32>((depth + m.r16s(segment + 10)) >> 4);
        m.w32(packet, m.r32(bucket) | 0x04000000u);
        m.w32(bucket, packet & 0xFFFFFFu);
        m.w32(packet + 4, gte.mfc2(20));
        m.w32(packet + 12, gte.mfc2(21));
        packet += 20;
    }
    return packet;
}

u32 Game::horizon_view_setup(u32 context, u32 work) {
    cover(0x80094718u);
    Frame frame(*this, 32);
    const u32 world = m.r32(g::kWorld);
    for (u32 i = 0; i < 3; ++i)
        m.w32(context + 8 + 4 * i, static_cast<u32>(m.r32s(m.r32(world + 40) + 4 + 4 * i) >> 12));
    for (u32 i = 0; i < 3; ++i)
        m.w16(context + 20 + 2 * i, m.r16(m.r32(world + 40) + 20 + 2 * i));
    const u32 pitch = m.r16(context + 20);
    m.w16(context + 26, m.r16(m.r32(world + 40) + 36));
    m.w32(work, pitch);
    m.w16(work + 4, m.r16(context + 24));
    rot_matrix_yxz_t(work, work + 8);
    for (u32 i = 0; i < 5; ++i) gte.ctc2(i, m.r32(work + 8 + 4 * i));
    for (u32 i = 5; i < 8; ++i) gte.ctc2(i, 0);
    m.w32(work + 40, 0);
    for (u32 off : {244u, 232u}) m.w16(work + off, static_cast<u32>(-160));
    for (u32 off : {240u, 236u}) m.w16(work + off, 160);
    for (u32 off : {238u, 234u}) m.w16(work + off, static_cast<u32>(-120));
    for (u32 off : {246u, 242u}) m.w16(work + off, 120);
    const u32 flags = m.r32(m.r32(kSkyState) + 1480);
    m.w32(work + 360, flags);
    m.w32(work + 364, (flags & 1u) || (m.r16(m.r32(m.r32(g::kWorld) + 580) + 2) & 0x400u) ? 1 : 0);
    return 1;
}

u32 Game::horizon_clip_codes(u32 work) {
    cover(0x800949ACu);
    s32 mask = m.r16s(work + 40);
    m.w32(work + kClipCodes, 0xAAA);
    u32 count = 0;
    for (; mask && count < 5; ++count, mask >>= 1) {
        const u32 code = work + kClipCodes + 4 + 4 * count;
        m.w32(code, 0);
        if (!(mask & 1)) continue;
        gte.mtc2(12, m.r32(work + kHorizonSegments + 8 * count));
        gte.mtc2(13, m.r32(work + kHorizonSegments + 8 * count + 4));
        for (u32 corner = 0; corner < 4; ++corner) {
            gte.mtc2(14, m.r32(work + kScreenCorners + 4 * corner));
            gte.op(0x1400006);
            const u32 area = work + kClipAreas + 16 * count + 4 * corner;
            m.w32(area, gte.mfc2(24));
            const s32 sign = m.r32s(area);
            const u32 bits = sign == 0 ? 3u : sign > 0 ? 2u : 1u;
            m.w32(code, m.r32(code) | (bits << (2 * corner)));
        }
        m.w32(code, m.r32(code) | (m.r32(code) << 8));
    }
    for (u32 i = count + 1; i < 7; ++i) m.w32(work + kClipCodes + 4 * i, 0x555);
    return 0;
}
}

#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"
#include "core/wrap.hpp"

#include <algorithm>

namespace ac3::sim {
namespace {
constexpr u32 kClipCodes = 124, kIntersections = 76, kScreenCorners = 232;
constexpr u32 kBandColours = 248, kPolygonPoints = 276;
constexpr u32 kPolygonColours = 308, kSkyState = 0x800BCF90u;
}

u32 Game::horizon_flat_band(u32 context, u32 work, u32 band) {
    cover(0x80095298u);
    s32 lower = m.r32s(work + kClipCodes + 4 * band);
    s32 upper = m.r32s(work + kClipCodes + 4 * (band + 1));
    u32 count = 0;
    auto append = [&](u32 from) {
        m.w32(work + kPolygonPoints + 4 * count++, m.r32(from));
    };
    for (u32 corner = 0; corner < 4; ++corner, lower >>= 2, upper >>= 2) {
        if (((lower | upper) & 3) == 3) append(work + kScreenCorners + 4 * corner);
        if ((lower & 15) == 9) append(work + kIntersections + 8 * band + 4);
        if ((upper & 15) == 9) append(work + kIntersections + 8 * band + 12);
        if ((upper & 15) == 6) append(work + kIntersections + 8 * band + 8);
        if ((lower & 15) == 6) append(work + kIntersections + 8 * band);
    }
    if (count < 3) return 4 * band;
    const u32 colour = m.r32(work + kBandColours + 4 * band);
    while (count >= 4) {
        const u32 packet = m.r32(context + 4);
        m.w32(context + 4, packet + 24);
        m.w32(packet, ((packet + 24) & 0xFFFFFFu) | 0x05000000u);
        m.w32(packet + 4, colour | 0x28000000u);
        m.w32(packet + 8, m.r32(work + kPolygonPoints));
        m.w32(packet + 12, m.r32(work + kPolygonPoints + 4 * (count - 3)));
        m.w32(packet + 16, m.r32(work + kPolygonPoints + 4 * (count - 1)));
        m.w32(packet + 20, m.r32(work + kPolygonPoints + 4 * (count - 2)));
        count -= 2;
    }
    if (count == 3) {
        const u32 packet = m.r32(context + 4);
        m.w32(context + 4, packet + 20);
        m.w32(packet, ((packet + 20) & 0xFFFFFFu) | 0x04000000u);
        m.w32(packet + 4, colour | 0x20000000u);
        m.w32(packet + 8, m.r32(work + kPolygonPoints));
        m.w32(packet + 12, m.r32(work + kPolygonPoints + 4));
        const u32 last = m.r32(work + kPolygonPoints + 8);
        m.w32(packet + 16, last);
        return last;
    }
    return 1;
}

u32 Game::horizon_shaded_band(u32 context, u32 work, u32 band) {
    cover(0x8009546Cu);
    Frame frame(*this, 16);
    const u32 upper_colour = m.r32(work + kBandColours + 4 * band);
    const u32 lower_colour = m.r32(work + kBandColours + 4 * (band - 1));
    gte.ctc2(21, (upper_colour & 255u) << 4);
    gte.ctc2(22, (upper_colour >> 4) & 0xFF0u);
    gte.ctc2(23, (upper_colour >> 12) & 0xFF0u);
    gte.mtc2(6, lower_colour);
    for (u32 i = 0; i < 3; ++i) gte.mtc2(12 + i, m.r32(work + 36 + 8 * band + 4 * i));
    s32 lower = m.r32s(work + kClipCodes + 4 * band);
    s32 upper = m.r32s(work + kClipCodes + 4 * (band + 1));
    gte.op(0x1400006);
    const s32 area = static_cast<s32>(gte.mfc2(24)) >> 4;
    u32 count = 0;
    auto append = [&](u32 point, u32 colour) {
        m.w32(work + kPolygonColours + 4 * count, colour);
        m.w32(work + kPolygonPoints + 4 * count++, m.r32(point));
    };
    for (u32 corner = 0; corner < 4; ++corner, lower >>= 2, upper >>= 2) {
        if (((lower | upper) & 3) == 3) {
            m.w32(work + kPolygonPoints + 4 * count, m.r32(work + kScreenCorners + 4 * corner));
            const s32 corner_area = m.r32s(work + 152 + 16 * (band - 1) + 4 * corner);
            u32 colour = lower_colour;
            if (area && corner_area > 0) {
                const s32 scaled = static_cast<s32>(static_cast<u32>(corner_area) << 8);
                if (area == -1 && scaled == (-2147483647 - 1))
                    unported(0x80095580u, "horizon colour division overflows (original BREAK)");
                gte.mtc2(8, wrap::div(scaled, area).lo);
                gte.op(0x780010);
                colour = gte.mfc2(22);
            }
            m.w32(work + kPolygonColours + 4 * count++, colour);
        }
        if ((lower & 15) == 9) append(work + kIntersections + 8 * band + 4, lower_colour);
        if ((upper & 15) == 9) append(work + kIntersections + 8 * band + 12, upper_colour);
        if ((upper & 15) == 6) append(work + kIntersections + 8 * band + 8, upper_colour);
        if ((lower & 15) == 6) append(work + kIntersections + 8 * band, lower_colour);
    }
    while (count >= 4) {
        const u32 packet = m.r32(context + 4);
        m.w32(context + 4, packet + 36);
        m.w32(packet, ((packet + 36) & 0xFFFFFFu) | 0x08000000u);
        m.w32(packet + 4, m.r32(work + kPolygonColours) | 0x38000000u);
        m.w32(packet + 8, m.r32(work + kPolygonPoints));
        m.w32(packet + 12, m.r32(work + kPolygonColours + 4 * (count - 3)));
        m.w32(packet + 16, m.r32(work + kPolygonPoints + 4 * (count - 3)));
        m.w32(packet + 20, m.r32(work + kPolygonColours + 4 * (count - 1)));
        m.w32(packet + 24, m.r32(work + kPolygonPoints + 4 * (count - 1)));
        m.w32(packet + 28, m.r32(work + kPolygonColours + 4 * (count - 2)));
        m.w32(packet + 32, m.r32(work + kPolygonPoints + 4 * (count - 2)));
        count -= 2;
    }
    if (count == 3) {
        const u32 packet = m.r32(context + 4);
        m.w32(context + 4, packet + 28);
        m.w32(packet, ((packet + 28) & 0xFFFFFFu) | 0x06000000u);
        m.w32(packet + 4, m.r32(work + kPolygonColours) | 0x30000000u);
        m.w32(packet + 8, m.r32(work + kPolygonPoints));
        m.w32(packet + 12, m.r32(work + kPolygonColours + 4));
        m.w32(packet + 16, m.r32(work + kPolygonPoints + 4));
        m.w32(packet + 20, m.r32(work + kPolygonColours + 8));
        const u32 last = m.r32(work + kPolygonPoints + 8);
        m.w32(packet + 24, last);
        return last;
    }
    return 1;
}

u32 Game::sky_altitude_colour(u32 colour, s32 camera_y) {
    cover(0x80094F50u);
    s32 level;
    if (camera_y > -41192) {
        gte.mtc2(6, m.r32(colour));
        level = wrap::sub(-33000, camera_y) >> 1;
    } else {
        gte.mtc2(6, 0);
        level = wrap::sub(4096, wrap::sub(-41192, camera_y) >> 3);
    }
    gte.mtc2(8, static_cast<u32>(level));
    gte.ctc2(21, static_cast<u32>(std::max(0, static_cast<s32>(m.r8(colour)) - 96)) << 4);
    gte.ctc2(22, static_cast<u32>(std::max(0, static_cast<s32>(m.r8(colour + 1)) - 96)) << 4);
    const s32 blue = static_cast<s32>(m.r8(colour + 2)) - 32;
    gte.ctc2(23, static_cast<u32>(std::max(0, blue)) << 4);
    gte.op(0x780010);
    m.w32(colour, gte.mfc2(22));
    return static_cast<u32>(blue) << 4;
}

u32 Game::horizon_colours(u32 context, u32 work) {
    cover(0x80094D10u);
    Frame frame(*this, 48);
    auto environment = [&]() { return m.r32(m.r32(g::kWorld) + 580); };
    m.w32(work + 248, m.r32(environment() + 28) | 0x030303u);
    if (m.r32s(context + 12) < -33000) sky_altitude_colour(work + 248, m.r32s(context + 12));
    s32 blend = (2 * m.r16s(environment() + 62) - (m.r16s(context + 22) >> 1)) & 0x1FFF;
    if (blend > 4096) blend = 8192 - blend;
    m.w32(work + 252, m.r32(environment() + 20));
    m.w32(work + 256, m.r32(environment() + 36));
    m.w32(work + 260, m.r32(environment() + 24));
    m.w32(work + 264, m.r32(environment() + 40));
    if (blend >= 4096) {
        m.w32(work + 252, m.r32(work + 260));
        m.w32(work + 256, m.r32(work + 264));
    } else if (blend) {
        for (u32 i = 0; i < 2; ++i) {
            const u32 target = m.r32(work + 260 + 4 * i);
            if (m.r32(work + 252 + 4 * i) != target) packet_fade(work + 252 + 4 * i, target, blend);
        }
    }
    const u32 ground_colour = m.r32(environment() + 8);
    m.w32(work + 264, ground_colour);
    m.w32(work + 260, ground_colour);
    m.w32(work + 268, m.r32(environment() + 4));
    if (!(m.r32(work + 360) & 2u)) return 0;
    const u32 sky = m.r32(kSkyState), flash = m.r32(sky + 1484);
    if (!flash) return sky;
    gte.mtc2(6, 0);
    gte.mtc2(8, flash);
    for (u32 i = 0; i < 3; ++i) {
        const u32 colour = m.r32(work + 248 + 4 * i);
        for (u32 ir = 9; ir <= 11; ++ir) gte.mtc2(ir, 1456);
        gte.mtc2(25, (colour & 255u) << 4);
        gte.mtc2(26, ((colour >> 8) & 255u) << 4);
        gte.mtc2(27, (colour >> 12) & 0xFF0u);
        gte.op(0x1A8003E);
        m.w32(work + 248 + 4 * i, gte.mfc2(22));
    }
    return 0;
}

u32 Game::horizon_boundaries(u32 context, u32 work) {
    cover(0x80094AC8u);
    Frame frame(*this, 32);
    const s32 altitude = wrap::neg(m.r32s(context + 12));
    const u32 shift = altitude > 36862 ? 3 : 2;
    const u32 world = m.r32(g::kWorld);
    auto environment = [&]() { return m.r32(world + 580); };
    m.w16(work + 46, static_cast<u32>(wrap::add(altitude, 2 * m.r16s(environment() + 34)) >> shift));
    m.w16(work + 54, static_cast<u32>(wrap::add(altitude, 2 * m.r16s(environment() + 32)) >> shift));
    m.w16(work + 62, static_cast<u32>(wrap::add(altitude, 2 * m.r16s(environment() + 16)) >> shift));
    m.w16(work + 78, static_cast<u32>(altitude >> shift));
    m.w16(work + 70, static_cast<u32>(wrap::add(altitude, m.r16s(environment() + 16) >> 1) >> shift));
    const s32 h = m.r16s(context + 26);
    if (!h) unported(0x80094BA8u, "horizon span divides by zero (original BREAK)");
    s32 span = static_cast<s32>(wrap::div(3932040, h).lo);
    for (u32 i = 0; i < 5; ++i) {
        const u32 line = work + 44 + 8 * i;
        const u32 y = m.r16(line + 2);
        m.w16(line, static_cast<u32>(-span));
        m.w16(line + 4, static_cast<u32>(span));
        m.w16(line + 6, y);
        if (i == 2) span >>= 1;
    }
    m.w32(work + 128, 32767u >> (shift - 1));
    m.w32(work + 124, 32767u >> (shift - 1));
    m.w32(work + 132, 30720u >> (shift - 1));
    m.w32(work + 136, 30720u >> shift);
    if (m.r32(work + 368)) {
        m.w32(work + 272, 26624);
        const u32 y = m.r16(work + 78) + 3072;
        m.w16(work + 82, y);
        m.w16(work + 78, y);
    } else if (m.r32(work + 364)) {
        const u32 view = m.r32(m.r32(g::kWorld) + 40);
        const s32 radius = (m.r16s(view + 40) + m.r16s(view + 42)) >> 1;
        m.w32(work + 272, static_cast<u32>(radius));
        if (radius < 13312) {
            const s32 left = m.r16s(work + 76) >> 1, right = m.r16s(work + 80) >> 1;
            m.w16(work + 76, static_cast<u32>(left));
            m.w16(work + 80, static_cast<u32>(right));
        }
    } else m.w32(work + 272, 26624);
    if (altitude >= m.r32s(work + 272)) return 4;
    gte.mtc2(9, m.r32(work + 272));
    gte.mtc2(10, static_cast<u32>(altitude));
    gte.op(0xA00428);
    const s32 squared = static_cast<s32>(gte.mfc2(25) - gte.mfc2(26));
    m.w32(work + 140, static_cast<u32>(isqrt(squared) >> shift));
    return 5;
}

u32 Game::horizon_project(u32 work, s32 count) {
    cover(0x800948A0u);
    Frame frame(*this, 48);
    gte.mtc2(4, 0);
    gte.mtc2(5, 0);
    bool previous_valid = false;
    u32 result = 0;
    s32 i = 0;
    auto clip = [&](s32 index) {
        const u32 line = work + 84 + 8u * static_cast<u32>(index);
        return clip_screen_line(line, line + 4, 160, 120);
    };
    for (; i < count; ++i) {
        const u32 source = work + 44 + 8u * static_cast<u32>(i);
        const u32 depth = m.r32(work + 124 + 4u * static_cast<u32>(i));
        gte.mtc2(0, m.r32(source));
        gte.mtc2(2, m.r32(source + 4));
        gte.mtc2(1, depth);
        gte.mtc2(3, depth);
        gte.op(0x280030);
        if (previous_valid) clip(i - 1);
        previous_valid = !(gte.cfc2(31) & 0x6000u);
        if (previous_valid) {
            u32 a = gte.mfc2(12), b = gte.mfc2(13);
            if (gte.x_squeeze != 0x10000 && gte.x_squeeze > 0) {
                auto at = [](u32 xy, u32 axis) { return static_cast<s32>(static_cast<s16>(xy >> (16 * axis))); };
                auto out = [&](u32 from, u32 other) {
                    u32 xy = 0;
                    for (u32 axis = 0; axis < 2; ++axis) {
                        const s32 mid = (at(from, axis) + at(other, axis)) / 2;
                        const s64 far = mid + static_cast<s64>(at(from, axis) - mid) * 0x10000 / gte.x_squeeze + (at(from, axis) > mid ? 1 : at(from, axis) < mid ? -1 : 0);
                        xy |= (static_cast<u32>(std::clamp<s64>(far, -32768, 32767)) & 0xFFFFu) << (16 * axis);
                    }
                    return xy;
                };
                const u32 wa = out(a, b), wb = out(b, a);
                a = wa;
                b = wb;
            }
            m.w32(source, a);
            m.w32(source + 4, b);
            m.w32(work + 84 + 8u * static_cast<u32>(i), a);
            m.w32(work + 88 + 8u * static_cast<u32>(i), b);
            m.w16(work + 40, m.r16(work + 40) | (1u << (static_cast<u32>(i) & 31u)));
        }
        result = 0;
    }
    if (previous_valid) result = clip(i - 1);
    return result;
}
}

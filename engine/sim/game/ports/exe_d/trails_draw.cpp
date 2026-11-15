#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kTrails = 0x800BD0B0u;
constexpr u32 kScratch = 0x1F800000u;
constexpr u32 kPoolB = 74020;
constexpr u32 kSamples = 264;
constexpr s32 kMaxDistance = 0xB000;
constexpr s32 kLastBucket = 22528;

u32 neighbours(u32 mask) {
    return mask | static_cast<u32>(static_cast<s32>(mask) >> 1) | (mask << 1);
}
s32 magnitude(s32 value) { return value < 0 ? wrap::neg(value) : value; }

struct TrailProjection {
    Memory& m;
    Gte& gte;
    u32 x, y, z;

    struct Vector { s32 x, y, z; };
    Vector relative(u32 point, u32 shift) const {
        return {wrap::sar(m.r32(point) - x, shift),
                wrap::sar(m.r32(point + 4) - y, shift),
                wrap::sar(m.r32(point + 8) - z, shift)};
    }
    static s32 distance(Vector v) {
        return (magnitude(v.x) + magnitude(v.y) + magnitude(v.z)) >> 2;
    }
    static u32 scale(s32 distance, s32 threshold) {
        return distance > threshold ? 2 : distance > (threshold >> 1) ? 1 : 0;
    }
    void load(u32 vertex, Vector v) {
        gte.mtc2(vertex * 2, (static_cast<u32>(v.x) & 0xFFFFu) | (static_cast<u32>(v.y) << 16));
        gte.mtc2(vertex * 2 + 1, static_cast<u32>(v.z));
    }
    Vector interpolate(Vector from, Vector to, u32 t) {
        gte.mtc2(8, t);
        gte.mtc2(9, static_cast<u32>(from.x));
        gte.mtc2(10, static_cast<u32>(from.y));
        gte.mtc2(11, static_cast<u32>(from.z));
        gte.ctc2(21, static_cast<u32>(to.x));
        gte.ctc2(22, static_cast<u32>(to.y));
        gte.ctc2(23, static_cast<u32>(to.z));
        gte.op(0x980011u);
        return {static_cast<s32>(gte.mfc2(9)), static_cast<s32>(gte.mfc2(10)), static_cast<s32>(gte.mfc2(11))};
    }
    void link(u32 ot, u32 packet, u32 words) {
        m.w32(packet, m.r32(ot) | (words << 24));
        m.w32(ot, packet & 0xFFFFFFu);
    }
};
}

u32 Game::trails_draw_weapons(u32 ot, u32 packets) {
    cover(0x8007C00Cu);
    Frame frame(*this, 88);
    const u32 pool = m.r32(kTrails), mask = m.r32(pool);
    if (!mask) return packets;
    const u32 view = m.r32(m.r32(g::kWorld) + 40);
    const s32 width = (m.r16s(view + 32) >> 1) + 56;
    const s32 height = (m.r16s(view + 34) >> 1) + 56;
    TrailProjection projection{m, gte, m.r32(view + 4), m.r32(view + 8), m.r32(view + 12)};
    const s32 range = static_cast<s32>(gte.cfc2(26) << 2);
    const s32 detailedRange = range * 3, nearRange = range * 4;
    gte.mtc2(6, 0);
    for (u32 ringIndex = 0; ringIndex < 32; ++ringIndex) {
        const u32 environment = m.r32(m.r32(g::kWorld) + 616);
        for (u32 axis = 0; axis < 3; ++axis) gte.ctc2(21 + axis, m.r8(environment + 32 + axis) << 4);
        if (!(mask & (1u << ringIndex))) continue;
        const u32 ring = pool + 4 + ringIndex * 2312;
        u32 count = 0, visible = 0, near = 0, clipped = 0, newest = 0;
        for (s32 sampleIndex = m.r32s(ring + 4); sampleIndex < m.r32s(ring); ++sampleIndex) {
            const u32 index = static_cast<u32>(sampleIndex) & 31u;
            const u32 sample = ring + kSamples + 64 * index;
            const auto center = projection.relative(sample, 10);
            const s32 distance = TrailProjection::distance(center);
            if (distance > kMaxDistance) continue;
            const u32 shift = TrailProjection::scale(distance, detailedRange);
            projection.load(0, {center.x >> shift, center.y >> shift, center.z >> shift});
            gte.op(0x180001u);
            const u32 entry = kScratch + 28 * count, bit = 1u << count;
            m.w16(entry + 26, shift);
            m.w32(entry, sample);
            if (distance < nearRange) near |= bit;
            if (sampleIndex == m.r32s(ring) - 1) newest = bit;
            const s32 age = m.r32s(ring + 8 + index * 4), life = m.r32s(ring + 136 + index * 4);
            const u32 luminance = static_cast<u32>((age * 16 < life ? age * 4 : life) << 5);
            const s32 depth = static_cast<s32>(gte.mfc2(11)) >> (2 - shift);
            const u32 flags = gte.cfc2(31);
            gte.mtc2(8, luminance);
            gte.op(0x780010u);
            u32 candidate = bit;
            if (flags & 0x20000u) { clipped |= bit; candidate = 0; }
            const u32 xy = gte.mfc2(14);
            if (magnitude(static_cast<s32>(static_cast<s16>(xy))) > width ||
                magnitude(static_cast<s32>(xy) >> 16) > height) candidate = 0;
            visible |= candidate;
            m.w32(entry + 8, xy);
            m.w16(entry + 24, static_cast<u32>(depth));
            m.w32(entry + 4, gte.mfc2(22));
            ++count;
        }
        near |= neighbours(near) & ~visible;
        visible = neighbours(visible);
        for (u32 i = 0; i < count; ++i) {
            const u32 entry = kScratch + i * 28, bit = 1u << i;
            const s32 depth = m.r16s(entry + 24);
            if (!(visible & bit)) continue;
            if (!(near & bit)) {
                if (depth < 0) m.w16(entry + 24, 0);
                continue;
            }
            const u32 sample = m.r32(entry), shift = static_cast<u32>(m.r16s(entry + 26) + 10);
            if (clipped & bit) {
                const u32 neighbour = i < count - 1 ? entry + 28 : entry - 28;
                const s32 neighbourDepth = m.r16s(neighbour + 24);
                const s32 numerator = (depth - (static_cast<s32>(gte.cfc2(26)) >> 3)) << 12;
                const u32 fraction = wrap::div(numerator, depth - neighbourDepth).lo;
                const u32 neighbourSample = m.r32(neighbour);
                projection.load(0, projection.interpolate(projection.relative(sample, shift),
                    projection.relative(neighbourSample, shift), fraction));
                gte.op(0x180001u);
                m.w32(entry + 8, gte.mfc2(14));
                for (u32 point = 1; point < 4; ++point) {
                    projection.load(point - 1, projection.interpolate(projection.relative(sample + point * 16, shift),
                        projection.relative(neighbourSample + point * 16, shift), fraction));
                }
                m.w32(entry + 4, 0);
            } else {
                for (u32 point = 1; point < 4; ++point) projection.load(point - 1, projection.relative(sample + point * 16, shift));
            }
            gte.op(0x280030u);
            if (depth < 0) m.w16(entry + 24, 0);
            for (u32 point = 0; point < 3; ++point)
                m.w32(entry + 12 + point * 4, newest & bit ? m.r32(entry + 8) : gte.mfc2(12 + point));
            if (newest & bit) (void)gte.mfc2(27);
        }
        for (u32 i = 1; i < count; ++i) {
            const u32 previous = kScratch + (i - 1) * 28, next = previous + 28;
            const u32 pair = 3u << (i - 1);
            const s32 depth = m.r16s(previous + 24);
            if ((visible & pair) != pair || depth >= kLastBucket) continue;
            const u32 bucket = ot + static_cast<u32>(depth >> 3) * 4;
            if ((near & pair) != pair) {
                m.w32(packets + 4, m.r32(previous + 4) | 0x52000000u);
                m.w32(packets + 8, m.r32(previous + 8));
                m.w32(packets + 12, m.r32(next + 4));
                m.w32(packets + 16, m.r32(next + 8));
                projection.link(bucket, packets, 4);
                packets += 20;
            } else {
                for (u32 side = 0; side < 3; ++side) {
                    m.w32(packets + 4, m.r32(previous + 4) | 0x3A000000u);
                    m.w32(packets + 8, m.r32(previous + 8));
                    m.w32(packets + 12, 0);
                    m.w32(packets + 16, m.r32(previous + 12 + side * 4));
                    m.w32(packets + 20, m.r32(next + 4));
                    m.w32(packets + 24, m.r32(next + 8));
                    m.w32(packets + 28, 0);
                    m.w32(packets + 32, m.r32(next + 12 + side * 4));
                    projection.link(bucket, packets, 8);
                    packets += 36;
                }
            }
        }
    }
    return packets;
}

u32 Game::trails_draw_aircraft(u32 ot, u32 packets) {
    cover(0x8007CFA4u);
    Frame frame(*this, 96);
    const u32 pool = m.r32(kTrails), mask = m.r32(pool + kPoolB);
    if (!mask) return packets;
    const u32 view = m.r32(m.r32(g::kWorld) + 40);
    const s32 width = m.r16s(view + 32) >> 1, height = m.r16s(view + 34) >> 1;
    const s32 detailedRange = m.r16s(view + 36) * 12, nearRange = m.r16s(view + 36) * 8;
    TrailProjection projection{m, gte, m.r32(view + 4), m.r32(view + 8), m.r32(view + 12)};
    for (u32 ringIndex = 0; ringIndex < 32; ++ringIndex) {
        if (!(mask & (1u << ringIndex))) continue;
        const u32 ring = pool + kPoolB + 4 + ringIndex * 1288;
        u32 count = 0, visible = 0xFFFFFFFFu, near = 0xFFFFFFFFu;
        for (s32 sampleIndex = m.r32s(ring + 4); sampleIndex < m.r32s(ring); ++sampleIndex) {
            const u32 index = static_cast<u32>(sampleIndex) & 31u;
            const u32 sample = ring + kSamples + 32 * index;
            TrailProjection::Vector center{
                wrap::sar(static_cast<u32>(wrap::sar(m.r32(sample) + m.r32(sample + 16), 1)) - projection.x, 10),
                wrap::sar(static_cast<u32>(wrap::sar(m.r32(sample + 4) + m.r32(sample + 20), 1)) - projection.y, 10),
                wrap::sar(static_cast<u32>(wrap::sar(m.r32(sample + 8) + m.r32(sample + 24), 1)) - projection.z, 10)};
            const s32 distance = TrailProjection::distance(center);
            if (distance > kMaxDistance) continue;
            const u32 shift = TrailProjection::scale(distance, detailedRange);
            projection.load(0, {center.x >> shift, center.y >> shift, center.z >> shift});
            gte.op(0x180001u);
            const u32 entry = kScratch + 24 * count, bit = 1u << count;
            m.w32(entry + 20, shift);
            m.w32(entry, sample);
            s32 intensity = m.r32s(ring + 136 + 4 * index);
            if (distance > nearRange) { near &= ~bit; intensity >>= 1; }
            if (intensity > 32) intensity = 32;
            const s32 depth = static_cast<s32>(gte.mfc2(19)) >> (2 - shift);
            const u32 xy = gte.mfc2(14);
            if (depth < (detailedRange >> 3)) intensity >>= 1;
            if (depth < (detailedRange >> 4)) intensity >>= 1;
            if (depth < (detailedRange >> 5)) intensity = 0;
            const u32 rgb = static_cast<u32>(intensity) | (static_cast<u32>(intensity) << 8) | (static_cast<u32>(intensity) << 16);
            m.w32(entry + 16, rgb);
            if (magnitude(static_cast<s32>(static_cast<s16>(xy))) > width ||
                magnitude(static_cast<s32>(xy) >> 16) > height || !magnitude(depth)) visible &= ~bit;
            m.w32(entry + 12, static_cast<u32>(depth));
            ++count;
        }
        near = neighbours(near);
        visible = neighbours(visible);
        for (u32 i = 0; i < count; ++i) {
            if (!(visible & (1u << i))) continue;
            const u32 entry = kScratch + i * 24, sample = m.r32(entry);
            const u32 shift = m.r32(entry + 20) + 10;
            projection.load(0, projection.relative(sample, shift));
            projection.load(1, projection.relative(sample + 16, shift));
            gte.op(0x280030u);
            m.w32(entry + 4, gte.mfc2(12));
            m.w32(entry + 8, gte.mfc2(13));
        }
        for (u32 i = 1; i < count; ++i) {
            const u32 previous = kScratch + (i - 1) * 24, next = previous + 24;
            const u32 pair = 3u << (i - 1);
            const s32 depth = m.r32s(previous + 12);
            if ((visible & pair) != pair || depth >= kLastBucket) continue;
            const u32 bucket = ot + static_cast<u32>(depth >> 3) * 4;
            const u32 previousColor = m.r32(previous + 16), nextColor = m.r32(next + 16);
            if ((near & pair) != pair) {
                for (u32 side = 0; side < 2; ++side) {
                    m.w32(packets + 4, previousColor | 0x52000000u);
                    m.w32(packets + 8, m.r32(previous + 4 + side * 4));
                    m.w32(packets + 12, nextColor);
                    m.w32(packets + 16, m.r32(next + 4 + side * 4));
                    projection.link(bucket, packets, 4);
                    packets += 20;
                }
                continue;
            }
            const TrailProjection::Vector previousLeft{m.r16s(previous + 4), m.r16s(previous + 6), 0};
            const TrailProjection::Vector previousRight{m.r16s(previous + 8), m.r16s(previous + 10), 0};
            const TrailProjection::Vector nextLeft{m.r16s(next + 4), m.r16s(next + 6), 0};
            const TrailProjection::Vector nextRight{m.r16s(next + 8), m.r16s(next + 10), 0};
            auto feather = [&](TrailProjection::Vector left, u32 t, u32 output) {
                gte.mtc2(8, t);
                gte.mtc2(9, static_cast<u32>(left.x));
                gte.mtc2(10, static_cast<u32>(left.y));
                gte.op(0x980011u);
                m.w16(output, gte.mfc2(9));
                m.w16(output + 2, gte.mfc2(10));
            };
            auto edge = [&](TrailProjection::Vector left, TrailProjection::Vector right, u32 offset) {
                gte.ctc2(21, static_cast<u32>(right.x));
                gte.ctc2(22, static_cast<u32>(right.y));
                feather(left, static_cast<u32>(-192), packets + offset);
                m.w16(packets + offset + 8, static_cast<u32>(left.x));
                m.w16(packets + offset + 10, static_cast<u32>(left.y));
                m.w16(packets + offset + 36, static_cast<u32>(left.x));
                m.w16(packets + offset + 38, static_cast<u32>(left.y));
                feather(left, 192, packets + offset + 44);
                feather(left, 3904, packets + offset + 72);
                m.w16(packets + offset + 80, static_cast<u32>(right.x));
                m.w16(packets + offset + 82, static_cast<u32>(right.y));
                m.w16(packets + offset + 108, static_cast<u32>(right.x));
                m.w16(packets + offset + 110, static_cast<u32>(right.y));
                feather(left, 4288, packets + offset + 116);
            };
            edge(previousLeft, previousRight, 8);
            edge(nextLeft, nextRight, 24);
            for (u32 ribbon = 0; ribbon < 4; ++ribbon) {
                const u32 packet = packets + 36 * ribbon;
                const bool outerFirst = !(ribbon & 1u);
                m.w32(packet + 4, 0x3A000000u | (outerFirst ? 0 : previousColor));
                m.w32(packet + 12, outerFirst ? previousColor : 0);
                m.w32(packet + 20, outerFirst ? 0 : nextColor);
                m.w32(packet + 28, outerFirst ? nextColor : 0);
                projection.link(bucket, packet, 8);
            }
            packets += 144;
        }
    }
    return packets;
}

u32 Game::trails_draw(u32 ot, u32 packets) {
    cover(0x8007DB00u);
    Frame frame(*this, 24);
    packets = trails_draw_weapons(ot, packets);
    return trails_draw_aircraft(ot, packets);
}
}

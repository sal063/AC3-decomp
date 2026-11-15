#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"
#include <array>

namespace ac3::sim {
namespace {
constexpr u32 kWeaponBlock = 0x800BD080u;
constexpr u32 kSegments = 5584;
constexpr u32 kSegmentMasks = 13264;
constexpr u32 kSegmentBytes = 60;
}

u32 Game::effect_segment_lists(u32 first, u32 second, u32 third, u32 fourth, u32 fifth, u32 sixth) {
    cover(0x8006C20Cu);
    Frame frame(*this, 0x20);
    std::array<u32, 6> cursor = {first, second, third, fourth, fifth, sixth};
    const u32 view = m.r32(m.r32(g::kWorld) + 40);
    const std::array<s32, 3> origin = {m.r32s(view + 4), m.r32s(view + 8), m.r32s(view + 12)};
    const s32 scaleRange = 12 * m.r16s(view + 36);
    auto relative = [&](u32 position) {
        return std::array<s32, 3>{wrap::sub(m.r32s(position), origin[0]) >> 12,
            wrap::sub(m.r32s(position + 4), origin[1]) >> 12,
            wrap::sub(m.r32s(position + 8), origin[2]) >> 12};
    };
    auto fits = [](const auto& point) {
        for (s32 coordinate : point) if (coordinate < -32767 || coordinate > 32767) return false;
        return true;
    };
    auto load = [&](u32 vertex, const auto& point, u32 scale) {
        const u32 x = static_cast<u32>(point[0]) << scale, y = static_cast<u32>(point[1]) << scale;
        gte.mtc2(2 * vertex, (x & 0xFFFFu) | (y << 16));
        gte.mtc2(2 * vertex + 1, static_cast<u32>(point[2]) << scale);
    };
    for (u32 group = 0; group < 4; ++group) {
        const u32 mask = m.r32(m.r32(kWeaponBlock) + kSegmentMasks + 4 * group);
        for (u32 bit = 0; bit < 32; ++bit) {
            if (!(mask & (1u << bit))) continue;
            const u32 segment = m.r32(kWeaponBlock) + kSegments + kSegmentBytes * (32 * group + bit);
            const auto start = relative(segment + 8);
            if (!fits(start)) continue;
            s32 distance = 0;
            for (s32 coordinate : start) distance += coordinate < 0 ? -coordinate : coordinate;
            const u32 scale = distance > scaleRange ? 0u : distance > (scaleRange >> 1) ? 1u : 2u;
            load(0, start, scale);
            const auto end = relative(segment + 24);
            if (!fits(end)) continue;
            load(1, end, scale);
            gte.op(0x280030);
            const u32 flags = m.r32(segment);
            u32 style = 0;
            while (style < 5 && !(flags & (0x10u << style))) ++style;
            const u32 output = cursor[style];
            cursor[style] += 16;
            u32 attribute = m.r32(segment + 4);
            const u32 depth0 = gte.mfc2(17);
            if (depth0 - 1u >= 0x77F7u) attribute = 0;
            m.w32(output, gte.mfc2(12));
            m.w16(output + 8, static_cast<u32>(static_cast<s32>(depth0) >> scale));
            const u32 depth1 = gte.mfc2(18);
            if (depth1 - 1u >= 0x77F7u) attribute = 0;
            m.w32(output + 4, gte.mfc2(13));
            m.w16(output + 10, static_cast<u32>(static_cast<s32>(depth1) >> scale));
            m.w32(output + 12, attribute);
        }
    }
    for (u32 output : cursor) m.w32(output + 12, 0xFFFFFFFFu);
    return 0xFFFFFFFFu;
}

u32 Game::draw_effect_segments(u32 orderTable, u32 packets) {
    cover(0x8006C5ECu);
    Frame frame(*this, 0x3098);
    constexpr std::array<u32, 6> offsets = {0x18, 0x828, 0x1038, 0x1848, 0x2058, 0x2868};
    m.w32(frame.at(0x10), frame.at(offsets[4]));
    m.w32(frame.at(0x14), frame.at(offsets[5]));
    effect_segment_lists(frame.at(offsets[0]), frame.at(offsets[1]), frame.at(offsets[2]),
        frame.at(offsets[3]), frame.at(offsets[4]), frame.at(offsets[5]));
    packets = draw_fading_lines(frame.at(offsets[0]), orderTable, packets);
    packets = draw_narrow_beams(frame.at(offsets[1]), orderTable, packets);
    packets = draw_medium_beams(frame.at(offsets[2]), orderTable, packets);
    packets = draw_beam_squares(frame.at(offsets[3]), orderTable, packets);
    return draw_broad_beams(frame.at(offsets[5]), orderTable, packets);
}
}

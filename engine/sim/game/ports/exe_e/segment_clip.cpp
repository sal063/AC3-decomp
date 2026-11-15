#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
s32 Game::effect_plane_distance(u32 normal, u32 point) {
    cover(0x80092014u);
    const s32 x = m.r32s(normal), y = m.r32s(normal + 4), z = m.r32s(normal + 8);
    gte.mtc2(9, static_cast<u32>(x));
    gte.mtc2(10, static_cast<u32>(y));
    gte.mtc2(11, static_cast<u32>(z));
    const s32 px = m.r32s(point), py = m.r32s(point + 4);
    gte.op(0xA00428u);
    const s32 dot = wrap::add(wrap::add(wrap::mul(x, px), wrap::mul(y, py)), wrap::mul(z, m.r32s(point + 8)));
    const s32 squared = wrap::add(wrap::add(static_cast<s32>(gte.mfc2(25)), static_cast<s32>(gte.mfc2(26))), static_cast<s32>(gte.mfc2(27)));
    gte.mtc2(30, static_cast<u32>(squared));
    if (squared <= 0) return 0x7FFFFFFF;
    const u32 leading = gte.mfc2(31) & 0xFEu;
    const u32 index = leading < 24 ? static_cast<u32>(squared >> (24 - leading)) :
                                    wrap::shl(static_cast<u32>(squared), leading - 24);
    const u32 root = wrap::shl(static_cast<u32>(m.r16s(g::kSqrtTable + 2 * (index - 64))), (31 - leading) >> 1);
    const s32 length = static_cast<s32>(root) >> 12;
    if (!length) unported(0x800920F4u, "zero plane normal length");
    if (length == -1 && static_cast<u32>(dot) == 0x80000000u)
        unported(0x8009210Cu, "plane distance divide overflow");
    return static_cast<s32>(wrap::div(dot, length).lo);
}

u32 Game::clip_effect_segment(u32 a, u32 b, s32 near_depth, u32 work) {
    cover(0x80092168u);
    Frame frame(*this, 48);
    const s32 az = m.r32s(a + 8), bz = m.r32s(b + 8);
    if (az < near_depth && bz < near_depth) return 0;
    auto interpolate_crossing = [&](s32 numerator, s32 denominator, u32 target, u32 zero_break, u32 overflow_break) {
        if (!denominator) unported(zero_break, "segment clipping divide by zero");
        if (denominator == -1 && static_cast<u32>(numerator) == 0x80000000u)
            unported(overflow_break, "segment clipping divide overflow");
        effect_interpolate(a, b, target, wrap::div(numerator, denominator).lo);
    };
    if (az < near_depth || bz < near_depth) {
        interpolate_crossing(static_cast<s32>((static_cast<u32>(near_depth) - static_cast<u32>(az)) << 12),
                             wrap::sub(bz, az), az < bz ? a : b, 0x800921E0u, 0x800921F8u);
    }
    const u32 h = gte.cfc2(26);
    constexpr s32 kCorners[4][2] = {{184, 144}, {-184, 144}, {-184, -144}, {184, -144}};
    m.w32(work + 16, static_cast<u32>(-184));
    m.w32(work + 32, static_cast<u32>(-184));
    for (u32 i = 0; i < 4; ++i) {
        m.w32(work + 16 * i, static_cast<u32>(kCorners[i][0]));
        m.w32(work + 16 * i + 4, static_cast<u32>(kCorners[i][1]));
        m.w32(work + 16 * i + 8, h);
    }
    for (u32 edge = 0; edge < 4; ++edge) {
        outer_product0(work + 16 * edge, work + 16 * ((edge + 1) & 3u), work + 64);
        const s32 da = effect_plane_distance(work + 64, a);
        const s32 db = effect_plane_distance(work + 64, b);
        if (da < 0 && db < 0) return 0;
        if ((da < 0) != (db < 0))
            interpolate_crossing(static_cast<s32>(static_cast<u32>(da) << 12), wrap::sub(da, db),
                                 da < db ? a : b, 0x800922E8u, 0x80092300u);
    }
    return 1;
}
}

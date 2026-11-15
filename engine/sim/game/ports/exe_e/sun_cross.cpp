#include "core/wrap.hpp"
#include "sim/game/game.hpp"

namespace ac3::sim {
u32 Game::draw_sun_cross(u32 context) {
    cover(0x8008D678u);
    constexpr u32 kSunColour = 0x00C04343u, kRayColour = 0x3A010101u;
    for (u32 channel = 21; channel < 24; ++channel) gte.ctc2(channel, 0);
    gte.mtc2(6, kSunColour); gte.mtc2(8, 1118);
    const u32 packet = m.r32(context + 4);
    gte.op(0x780010u);
    for (u32 ray = 0; ray < 3; ++ray) {
        const u32 p = packet + 68 * ray;
        m.w32(p, 0x10000000u | ((p + 68) & 0xFFFFFFu));
        m.w32(p + 4, kRayColour);
    }
    m.w32(context + 44, kSunColour);
    m.w32(context + 48, gte.mfc2(22));
    gte.mtc2(0, 0); gte.mtc2(1, static_cast<u32>(-32)); gte.op(0x180001u);
    for (u32 ray = 0; ray < 3; ++ray)
        for (u32 offset : {20u, 28u, 36u, 52u, 60u}) m.w32(packet + 68 * ray + offset, kRayColour);
    m.w32(context + 24, gte.mfc2(14));
    const s32 depth = static_cast<s32>(gte.mfc2(19)) >> (m.r32(context + 12) & 31u);
    const u32 focal = gte.cfc2(26);
    if (static_cast<u32>(depth) - 31u >= 0x77D9u) return focal << 4;
    m.w32(context + 40, wrap::div(static_cast<s32>(focal * 30u), depth).lo);
    const u32 ot = m.r32(context) + 4u * static_cast<u32>(depth >> 3);
    for (u32 byte = 0; byte < 3; ++byte) {
        m.w8(packet + 136 + byte, m.r8(ot + byte));
        m.w8(ot + byte, packet >> (8 * byte));
    }
    m.w32(context + 4, packet + 204);
    if (m.r32s(context + 40) < 8) m.w32(context + 40, 8);
    u32 colour = m.r32(context + 48);
    for (u32 ray = 0; ray < 3; ++ray) {
        const u32 p = packet + 68 * ray;
        m.w32(p + 48, m.r32(context + 24)); m.w32(p + 16, m.r32(context + 24));
        m.w32(p + 44, colour); m.w32(p + 12, colour);
        if (ray == 1) colour = m.r32(context + 44);
    }
    const s32 radius = m.r32s(context + 40);
    const s32 thin = (radius >> 4) ? radius >> 4 : 1;
    auto point = [&](u32 p, u32 offset, s32 dx, s32 dy) {
        m.w16(p + offset, m.r16(context + 24) + dx);
        m.w16(p + offset + 2, m.r16(context + 26) + dy);
    };
    for (u32 offset : {8u, 40u}) point(packet, offset, -radius, -radius);
    for (u32 offset : {32u, 64u}) point(packet, offset, radius, radius);
    point(packet, 24, thin, -thin); point(packet, 56, -thin, thin);
    const u32 diagonal = packet + 68;
    for (u32 offset : {8u, 40u}) point(diagonal, offset, radius >> 1, -(radius >> 1));
    for (u32 offset : {32u, 64u}) point(diagonal, offset, -(radius >> 1), radius >> 1);
    point(diagonal, 24, -thin, -thin); point(diagonal, 56, thin, thin);
    const u32 cross = packet + 136;
    for (u32 offset : {24u, 56u}) m.w16(cross + offset, m.r16(context + 24));
    for (u32 offset : {8u, 40u}) m.w16(cross + offset, m.r16(context + 24) - (radius >> 2));
    for (u32 offset : {32u, 64u}) m.w16(cross + offset, m.r16(context + 24) + (radius >> 2));
    for (u32 offset : {10u, 34u, 42u, 66u}) m.w16(cross + offset, m.r16(context + 26));
    m.w16(cross + 26, m.r16(context + 26) - (radius >> 2));
    const u32 result = m.r16(context + 26) + (radius >> 2);
    m.w16(cross + 58, result);
    return result;
}
}

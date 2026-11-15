#include "core/wrap.hpp"
#include "sim/game/globals.hpp"
#include "hud_packets.hpp"

namespace ac3::sim {
using namespace hud_packets;

u32 Game::lock_draw_hexagon(u32 index, u32 rgb) {
    cover(0x8003FD48u);
    Frame frame(*this, 32);
    const u32 blip = m.r32(kBlips + 4 * index);
    if (!blip) return 1;
    const u32 relation = m.r32(kRelations + 4 * index);
    if (!relation) return 1;
    const s32 radius = m.r16s(blip + 6), x = m.r16s(blip + 8), y = m.r16s(blip + 10);
    const s32 left = x - radius, right = x + radius, top = y - radius, bottom = y + radius;
    const s32 upper = y - (radius >> 1), lower = y + (radius >> 1);
    for (const auto [off, value] : {std::pair{0u, left}, {4u, x}, {8u, right}, {16u, top},
                                  {20u, upper}, {24u, lower}, {28u, bottom}})
        m.w32(frame.at(off), static_cast<u32>(value));
    if (m.r32(relation) & 0x100u) {
        polyline(m, rgb, {left, upper}, {x, top}, {right, upper});
        polyline(m, rgb, {left, upper}, {left, lower}, {x, bottom});
        return polyline(m, rgb, {right, upper}, {right, lower}, {x, bottom});
    }
    const s32 quarter = radius >> 2, eighth = radius >> 3, slope = (9 * eighth) >> 2;
    polyline(m, rgb, {x - quarter, top + eighth}, {x, top}, {x + quarter, top + eighth});
    polyline(m, rgb, {left, upper + slope}, {left, upper}, {left + quarter, upper - eighth});
    polyline(m, rgb, {left, lower - slope}, {left, lower}, {left + quarter, lower + eighth});
    polyline(m, rgb, {x - quarter, bottom - eighth}, {x, bottom}, {x + quarter, bottom - eighth});
    polyline(m, rgb, {right, upper + slope}, {right, upper}, {right - quarter, upper - eighth});
    return polyline(m, rgb, {right, lower - slope}, {right, lower}, {right - quarter, lower + eighth});
}

u32 Game::lock_draw_cross(u32 index, u32 rgb) {
    cover(0x800401ACu);
    Frame frame(*this, 16);
    const u32 blip = m.r32(kBlips + 4 * index);
    if (!blip) return 1;
    const u32 relation = m.r32(kRelations + 4 * index);
    if (!relation) return 1;
    const s32 radius = m.r16s(blip + 6), x = m.r16s(blip + 8), y = m.r16s(blip + 10);
    const s32 left = x - radius, right = x + radius, top = y - radius, bottom = y + radius;
    m.w32(frame.at(0), static_cast<u32>(left)); m.w32(frame.at(4), static_cast<u32>(right));
    m.w32(frame.at(8), static_cast<u32>(top)); m.w32(frame.at(12), static_cast<u32>(bottom));
    const auto draw = [&](Point a, Point b) {
        const u32 state = m.r32(kLockBlocks);
        const u32 packet = m.r32(kLockPrims) + 8 + 16 * m.r32(state + 8);
        m.w32(packet + 4, rgb | 0x42000000u);
        point(m, packet, 8, a); point(m, packet, 12, b);
        const u32 count = m.r32(state + 8) + 1;
        m.w32(state + 8, count);
        return count;
    };
    const s32 length = radius >> 2;
    draw({left, top}, {left + length, top + length});
    draw({right, top}, {right - length, top + length});
    draw({left, bottom}, {left + length, bottom - length});
    draw({right, bottom}, {right - length, bottom - length});
    if (!(m.r32(relation) & 0x20u)) return 0;
    const s32 cross = radius >> 3;
    draw({left - cross, top + cross}, {left + cross, top - cross});
    draw({right - cross, top - cross}, {right + cross, top + cross});
    draw({left - cross, bottom - cross}, {left + cross, bottom + cross});
    draw({right - cross, bottom + cross}, {right + cross, bottom - cross});
    const u32 center = m.r32(blip + 8);
    return draw({static_cast<s16>(center), static_cast<s16>(center >> 16)},
                {static_cast<s16>(center), static_cast<s16>(center >> 16)});
}

u32 Game::lock_draw_targets(u32 inheritedColor) {
    cover(0x8003F520u);
    Frame frame(*this, 0x78);
    u32 rgb = inheritedColor;
    for (u32 index = 0; index < 64; ++index) {
        const u32 flags = m.r32(g::kSlotFlags + 4 * index);
        if (!(flags & 0x1000u)) continue;
        const u32 blip = m.r32(kBlips + 4 * index);
        if (!blip || !m.r8(blip)) continue;
        const u32 state = m.r8(blip);
        if (state == 1) rgb = 0x00FF00;
        else if (state == 2) rgb = (m.r32(0x800C6D60u) & 2u) ? 0x40FF40u : 0;
        else if (state == 3) rgb = 0x0000FF;
        const u32 radius = m.r16(blip + 6), x = m.r16(blip + 8), y = m.r16(blip + 10);
        const bool flattened = (flags & 0x40000u) && m.r8(blip + 5) != 1;
        m.w16(frame.at(0x20), x - radius); m.w16(frame.at(0x28), x + radius);
        m.w16(frame.at(0x30), flattened ? y - 4 : y - radius - 4);
        m.w16(frame.at(0x38), flattened ? y - 4 : y + radius);
        if (state != 1) {
            const u32 relation = m.r32(kRelations + 4 * index);
            if (!relation) continue;
            if (flags & 0x400u) lock_draw_heading(index);
            const s32 distance = m.r32s(relation + 4) >> 2;
            const s32 thousands = distance / 1000, hundreds = distance / 100, tens = distance / 10;
            const s32 digits[] = {thousands, hundreds - 10 * thousands, tens - 10 * hundreds, distance - 10 * tens};
            const u32 block = m.r32(kLockBlocks);
            u32 packet = m.r32(kLockPrims) + 0x1508 + 20 * m.r32(block + 20);
            for (u32 i = 0; i < 4; ++i, packet += 20) {
                gauge_digit(frame.at(0x10), digits[i], static_cast<u32>(m.r16s(frame.at(0x28)) + 2 + 5 * i),
                            static_cast<u32>(m.r16s(frame.at(0x38))), 2, packet, 0);
                const u32 counts = m.r32(kLockBlocks);
                m.w32(counts + 20, m.r32(counts + 20) + 1);
            }
            lock_draw_type(m.r8(blip + 3), m.r16s(frame.at(0x20)), m.r16s(frame.at(0x30)));
        }
        if (m.r8(blip + 4) != 255)
            lock_draw_character(m.r8(blip + 4), m.r16s(frame.at(0x28)), m.r16s(frame.at(0x30)));
        const u32 status = m.r8(blip + 5);
        if (status == 3) {
            rgb = 0xFF4000;
            lock_draw_type(m.r8(blip + 3), m.r16s(frame.at(0x20)), m.r16s(frame.at(0x30)));
        }
        if (status >= 1 && status <= 3)
            lock_draw_status(status, m.r16s(frame.at(0x20)), m.r16s(frame.at(0x38)), 0);
        if ((flags & 0x40000u) && state == 1) {
            lock_draw_type(m.r8(blip + 3), m.r16s(frame.at(0x20)), m.r16s(frame.at(0x30)));
            continue;
        }
        const u32 outline = m.r8(blip + 2) & 15u;
        if (m.r32(m.r32(g::kMission)) & 4u) lock_draw_cross(index, rgb);
        else if (outline == 1) lock_draw_rectangle(index, rgb);
        else if (outline == 2) lock_draw_hexagon(index, rgb);
        else if (outline == 3) lock_draw_cross(index, rgb);
    }
    return 0;
}

u32 Game::lock_draw(u32 inheritedColor) {
    cover(0x8003EFE0u);
    Frame frame(*this, 24);
    lock_draw_begin(); lock_draw_targets(inheritedColor); lock_draw_acquisition();
    return lock_draw_link();
}

u32 Game::hud_draw(u32 hud, u32 inheritedColor) {
    cover(0x8003A0B4u);
    Frame frame(*this, 24);
    hud_colors(hud); hud_name_tag(); hud_warnings(hud); radar_update(hud);
    lock_draw(inheritedColor); hud_markers(hud); gauges_args(hud);
    const u32 landing = m.r32(m.r32(g::kWorld) + 4) & 0x800u;
    if (landing) return landing_guidance(hud);
    return landing;
}
}

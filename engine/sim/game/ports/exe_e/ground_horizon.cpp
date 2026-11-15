#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"
#include <algorithm>

namespace ac3::sim {
u32 Game::ground_horizon_setup(u32 context, u32 work) {
    cover(0x80095FA0u);
    const u32 world = m.r32(g::kWorld);
    m.w32(work + 248, m.r32(m.r32(world + 580) + 28));
    const u32 middle = m.r32(m.r32(world + 580) + 8);
    m.w32(work + 256, middle); m.w32(work + 252, middle);
    const u32 outer = m.r32(m.r32(world + 580) + 4);
    m.w32(work + 264, outer); m.w32(work + 260, outer);
    if (!(m.r16(m.r32(world + 580) + 2) & 0x400) && !(m.r32(work + 360) & 1))
        m.w32(work + 264, m.r32(work + 264) | 0x01000000u);
    const u32 parameters = m.r32(m.r32(g::kWorld) + 40);
    const s32 inner_depth = std::min<s32>(m.r16s(parameters + 42), 26624) >> 1;
    const s32 outer_depth = std::max<s32>(m.r16s(parameters + 40), 1000) >> 1;
    const s32 focal = m.r16s(context + 26);
    if (!focal) unported(0x80096094u, "ground horizon has zero focal length");
    const s32 inner_extent = static_cast<s32>(wrap::div(200 * inner_depth, focal).lo);
    const s32 outer_extent = static_cast<s32>(wrap::div(200 * outer_depth, focal).lo);
    s32 height = wrap::sub(wrap::neg(m.r32s(context + 12)), 3584) >> 1;
    for (u32 boundary = 0; boundary < 4; ++boundary) {
        const bool inner = boundary == 1 || boundary == 2;
        const s32 extent = inner ? inner_extent : outer_extent;
        const u32 endpoints = work + 44 + 8 * boundary;
        if (!inner) m.w32(work + 124 + 4 * boundary, outer_depth);
        m.w16(endpoints, wrap::neg(extent)); m.w16(endpoints + 4, extent);
        if (inner) m.w32(work + 124 + 4 * boundary, inner_depth);
        m.w16(endpoints + 6, height); m.w16(endpoints + 2, height);
        if (boundary == 1) height = wrap::sub(512, m.r32s(context + 12)) >> 1;
    }
    return 0;
}

u32 Game::ground_horizon_draw(u32 context, u32 work) {
    cover(0x80096184u);
    Frame frame(*this, 40);
    const u32 bands = m.r16(work + 40);
    const s32 hidden = static_cast<s8>(m.r8(work + 267));
    if (bands) {
        if (!hidden) {
            if (bands & 1) horizon_flat_band(context, work, 0);
            if (bands & 8) horizon_flat_band(context, work, 4);
            if (bands & 1) horizon_shaded_band(context, work, 1);
            if (bands & 4) horizon_shaded_band(context, work, 3);
        }
        return bands & 6 ? horizon_flat_band(context, work, 2) : 0;
    }
    if (!hidden) return 0;
    const u32 packet = m.r32(context + 4);
    m.w32(packet, ((packet + 16) & 0x00FFFFFFu) | 0x03000000u);
    m.w32(packet + 4, m.r32(work + 252) | 0x60000000u);
    const u32 origin = m.r32(work + 232);
    m.w32(packet + 12, 0x00F00140u); m.w32(packet + 8, origin);
    m.w32(context + 4, packet + 16);
    return origin;
}
u32 Game::draw_ground_horizon(u32 ot, u32 packet) {
    cover(0x800962B8u);
    Frame frame(*this, 40);
    const u32 context = g::kScratch, work = context + 28;
    m.w32(context, ot); m.w32(context + 4, packet + 8);
    horizon_view_setup(context, work);
    ground_horizon_setup(context, work);
    horizon_project(work, 4);
    horizon_clip_codes(work);
    ground_horizon_draw(context, work);
    const u32 mask = (m.r32(context + 388) & 4) ? 1 : 0;
    u32 tail = packet;
    if (m.r32(context + 4) == packet + 8) {
        m.w32(packet, 0x01000000u);
        m.w32(packet + 4, 0xE6000000u | mask);
    } else {
        m.w32(packet, ((packet + 8) & 0x00FFFFFFu) | 0x01000000u);
        m.w32(packet + 4, 0xE6000001u);
        tail = m.r32(context + 4);
        m.w32(tail, 0x01000000u); m.w32(tail + 4, 0xE6000000u | mask);
    }
    const u32 link = static_cast<u32>(m.r8(ot)) | (static_cast<u32>(m.r8(ot + 1)) << 8) | (static_cast<u32>(m.r8(ot + 2)) << 16);
    for (u32 byte = 0; byte < 3; ++byte) m.w8(tail + byte, link >> (8 * byte));
    for (u32 byte = 0; byte < 3; ++byte) m.w8(ot + byte, packet >> (8 * byte));
    m.w32(context + 4, tail + 8);
    m.w32(m.r32(0x800BCF90u) + 1480, 0);
    return m.r32(context + 4);
}

u32 Game::draw_terrain_and_horizon(u32 terrain, u32 ot, u32 packet) {
    cover(0x8009754Cu);
    Frame frame(*this, 24);
    const u32 next = draw_terrain_mesh(terrain, ot, packet);
    return draw_ground_horizon(ot, next);
}
}

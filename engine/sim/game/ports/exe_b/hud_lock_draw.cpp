#include "core/wrap.hpp"
#include "sim/game/globals.hpp"
#include "hud_packets.hpp"

namespace ac3::sim {
using namespace hud_packets;

u32 Game::lock_draw_begin() {
    cover(0x8003F018u);
    const u32 state = m.r32(kLockBlocks);
    for (u32 off : {20u, 16u, 12u, 8u}) m.w32(state + off, 0);
    const u32 packet = m.r32(state + 4 * m.r32(m.r32(g::kScene) + 4));
    m.w32(kLockPrims, packet);
    return packet;
}

u32 Game::lock_draw_link() {
    cover(0x8003F054u);
    const u32 ot = m.r32(m.r32(g::kWorld) + 16);
    struct Pool { u32 count, start, stride; };
    for (const Pool pool : {Pool{8, 8, 16}, {12, 0x408, 24}, {16, 0x1008, 20}, {20, 0x1508, 20}}) {
        const u32 state = m.r32(kLockBlocks);
        if (m.r32s(state + pool.count) <= 0) continue;
        const u32 packets = m.r32(kLockPrims);
        for (s32 i = 0; i < m.r32s(state + pool.count); ++i)
            ot_link(ot, packets + pool.start + static_cast<u32>(i) * pool.stride);
    }
    ot_link(ot, m.r32(kLockPrims));
    return m.r32(ot);
}

u32 Game::lock_draw_rectangle(u32 index, u32 rgb) {
    cover(0x8003FA80u);
    Frame frame(*this, 16);
    const u32 blip = m.r32(kBlips + 4 * index);
    if (!blip) return 1;
    const u32 relation = m.r32(kRelations + 4 * index);
    if (!relation) return 1;
    const s32 radius = m.r16s(blip + 6), x = m.r16s(blip + 8), y = m.r16s(blip + 10);
    const s32 left = x - radius, right = x + radius, top = y - radius, bottom = y + radius;
    m.w32(frame.at(0), static_cast<u32>(left)); m.w32(frame.at(4), static_cast<u32>(right));
    m.w32(frame.at(8), static_cast<u32>(top)); m.w32(frame.at(12), static_cast<u32>(bottom));
    if (m.r32(relation) & 0x100u) {
        polyline(m, rgb, {left, bottom}, {left, top}, {right, top});
        return polyline(m, rgb, {right, top}, {right, bottom}, {left, bottom});
    }
    const s32 length = radius >> 2;
    polyline(m, rgb, {left + length, top}, {left, top}, {left, top + length});
    polyline(m, rgb, {right - length, top}, {right, top}, {right, top + length});
    polyline(m, rgb, {left + length, bottom}, {left, bottom}, {left, bottom - length});
    return polyline(m, rgb, {right - length, bottom}, {right, bottom}, {right, bottom - length});
}

u32 Game::lock_draw_heading(u32 index) {
    cover(0x80040530u);
    Frame frame(*this, 16);
    const u32 blip = m.r32(kBlips + 4 * index);
    if (!blip) return 1;
    const u32 entity = slot(index), view = m.r32(m.r32(g::kWorld) + 40);
    const u32 angle = (m.r16(entity + 26) - m.r16(view + 22)) & 0x3FFFu;
    constexpr u32 kSine = 0x800104ACu;
    const s32 sine = m.r16s(kSine + 2 * angle), cosine = m.r16s(kSine + 2 * (angle + 0x1000));
    const s32 radius = m.r16s(blip + 6), half = radius >> 1;
    const s32 dx = (cosine - sine) * half >> 14, dy = (sine + cosine) * half >> 14;
    point(m, frame.at(0), 0, {0, radius - 4});
    point(m, frame.at(0), 4, {4, radius + 4});
    point(m, frame.at(0), 8, {-4, radius + 4});
    for (u32 off : {0u, 4u, 8u}) {
        const s32 x = m.r16s(frame.at(off)), y = m.r16s(frame.at(off + 2));
        point(m, frame.at(0), off, {
            static_cast<s32>(m.r16(blip + 8)) + ((cosine * x - sine * y) >> 14) + dx,
            static_cast<s32>(m.r16(blip + 10)) + ((sine * x + cosine * y) >> 14) + dy});
    }
    const auto p = [&](u32 off) { return Point{m.r16s(frame.at(off)), m.r16s(frame.at(off + 2))}; };
    line(m, kLockBlocks, kLockPrims, 8, 8, p(0), p(4), 0x00FF00);
    line(m, kLockBlocks, kLockPrims, 8, 8, p(0), p(8), 0x00FF00);
    return 0;
}

u32 Game::lock_draw_status(u32 kind, s32 x, s32 y, u32 previous) {
    cover(0x80040740u);
    kind &= 255;
    if (!kind) return previous;
    const u32 entry = 0x800B10CCu + 4 * kind;
    const u32 rgb = kind == 1 ? 0x000083 : kind == 2 ? 0x008383 : kind == 3 ? 0x832300 : 0xFFFFFFFFu;
    return sprite(m, entry, x - m.r8(entry + 2) - 2, y, rgb);
}

u32 Game::lock_draw_type(u32 kind, s32 x, s32 y) {
    cover(0x8004084Cu);
    const u32 entry = 0x800B10DCu + 4 * kind;
    if (!m.r8(entry + 2)) return 0;
    return sprite(m, entry, x - m.r8(entry + 2) - 2, y, 0x008300);
}

u32 Game::lock_draw_character(u32 kind, s32 x, s32 y) {
    cover(0x80040900u);
    const u32 entry = 0x800B1234u + 4 * kind;
    if (!m.r8(entry + 2)) return 0;
    return sprite(m, entry, x + 2, y, 0x008300);
}

u32 Game::lock_draw_acquisition() {
    cover(0x80040E70u);
    constexpr u32 kLock = 0x800BCEC8u;
    if (!m.r8(kLock)) return 0;
    const s32 radius = m.r16s(kLock + 2), x = m.r16s(kLock + 4), y = m.r16s(kLock + 6);
    if (m.r8(kLock + 1) < 32) {
        const s32 extent = radius + (radius >> 3);
        polyline(m, 0x0000FF, {x, y - extent}, {x + extent, y}, {x, y + extent});
        return polyline(m, 0x0000FF, {x, y - extent}, {x - extent, y}, {x, y + extent});
    }
    const s32 inset = radius >> 2;
    const s32 left = x - radius, right = x + radius, top = y - radius, bottom = y + radius;
    const s32 innerLeft = x - inset, innerRight = x + inset, innerTop = y - inset, innerBottom = y + inset;
    const auto draw = [&](Point a, Point b) { line(m, kLockBlocks, kLockPrims, 8, 8, a, b, 0x0000FF); };
    draw({x, top}, {x, innerTop}); draw({x, bottom}, {x, innerBottom});
    draw({left, y}, {innerLeft, y}); draw({right, y}, {innerRight, y});
    draw({innerLeft, top}, {innerRight, top}); draw({innerLeft, bottom}, {innerRight, bottom});
    draw({left, innerTop}, {left, innerBottom});
    return line(m, kLockBlocks, kLockPrims, 8, 8, {right, innerTop}, {right, innerBottom}, 0x0000FF);
}
}

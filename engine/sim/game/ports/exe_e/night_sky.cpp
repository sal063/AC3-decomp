#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"
#include "core/wrap.hpp"
#include <algorithm>

namespace ac3::sim {
u32 Game::draw_night_sky(u32 ot, u32 packet) {
    cover(0x80097EB4u);
    Frame frame(*this, 40);
    const u32 first = packet + 8, context = g::kScratch;
    m.w32(context, ot); m.w32(context + 4, first);
    m.w32(first, 0x03000000u | ((packet + 24) & 0xFFFFFFu));
    m.w32(first + 8, 0xFF88FF60u); m.w32(first + 12, 0x00F00140u);
    const u32 world = m.r32(g::kWorld);
    m.w32(first + 4, 0x60000000u); m.w32(context + 4, packet + 24);
    for (u32 angle = 0; angle < 3; ++angle)
        m.w16(context + 20 + 2 * angle, m.r16(m.r32(world + 40) + 20 + 2 * angle));
    m.w16(context + 26, m.r16(m.r32(world + 40) + 36));
    sky_points(context, context + 28, 256);
    draw_sky_rings();
    const u32 restore = (m.r32(m.r32(0x800BCF90u) + 1480) & 4) ? 1u : 0u;
    u32 last = packet;
    if (m.r32(context + 4) == first) {
        m.w32(packet, 0x01000000u); m.w32(packet + 4, 0xE6000000u | restore);
    } else {
        m.w32(packet, 0x01000000u | (first & 0xFFFFFFu)); m.w32(packet + 4, 0xE6000001u);
        last = m.r32(context + 4);
        m.w32(last, 0x01000000u); m.w32(last + 4, 0xE6000000u | restore);
    }
    for (u32 byte = 0; byte < 3; ++byte) {
        m.w8(last + byte, m.r8(ot + byte)); m.w8(ot + byte, packet >> (8 * byte));
    }
    m.w32(m.r32(0x800BCF90u) + 1480, 0);
    return last + 8;
}

u32 Game::draw_warp_sky(u32 ot, u32 packet) {
    cover(0x80098044u);
    Frame frame(*this, 48);
    const u32 phase = m.r32(m.r32(g::kScene) + 40), context = g::kScratch;
    s32 progress = wrap::mul(static_cast<s32>(phase), 7680) / 450 % 7680;
    if (progress >= 2049) progress = 2048;
    s32 cosine = m.r16s(0x800B7106u + 4 * (static_cast<u32>(progress) & 4095));
    if (cosine < 0) cosine = -cosine;
    const u32 base = framebuffer_blend(ot - 16380, packet, 0, std::max(1838, 4778 - cosine));
    m.w32(context, ot); m.w32(context + 4, base + 8);
    const u32 world = m.r32(g::kWorld), params = m.r32(world + 40);
    for (u32 axis = 0; axis < 3; ++axis) m.w32(context + 8 + 4 * axis, m.r32(params + 4 + 4 * axis));
    m.w16(context + 26, m.r16(params + 36));
    m.w32(base + 8, 0x01000000u | ((base + 16) & 0xFFFFFFu)); m.w32(base + 12, 0xE1000200u);
    m.w32(base + 16, 0x04000000u | ((base + 32) & 0xFFFFFFu)); m.w32(base + 20, 0x60000000u);
    m.w32(base + 24, 0xFF88FF60u); m.w32(base + 28, 0x00F00140u);
    m.w32(context + 4, base + 32);
    for (u32 reg = 0; reg < 8; ++reg) gte.ctc2(reg, m.r32(world + 76 + 4 * reg));
    draw_ring_mesh(context, phase); draw_slice_mesh(context);
    const u32 restore = (m.r32(m.r32(0x800BCF90u) + 1480) & 4) ? 1u : 0u;
    u32 last = base;
    if (m.r32(context + 4) == base + 8) {
        m.w32(base, 0x01000000u); m.w32(base + 4, 0xE6000000u | restore);
    } else {
        m.w32(base, 0x01000000u | ((base + 8) & 0xFFFFFFu)); m.w32(base + 4, 0xE6000001u);
        const u32 mode = m.r32(context + 4);
        m.w32(mode, 0x01000000u | ((mode + 8) & 0xFFFFFFu)); m.w32(mode + 4, 0xE1000220u);
        last = mode + 8; m.w32(last, 0x01000000u); m.w32(last + 4, 0xE6000000u | restore);
    }
    for (u32 byte = 0; byte < 3; ++byte) {
        m.w8(last + byte, m.r8(ot + byte)); m.w8(ot + byte, base >> (8 * byte));
    }
    m.w32(m.r32(0x800BCF90u) + 1480, 0);
    ot_skip_empty(ot - 16376, 4095);
    return last + 8;
}
}

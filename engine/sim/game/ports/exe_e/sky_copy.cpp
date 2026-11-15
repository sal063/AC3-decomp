#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
u32 Game::sky_copy_strips(u32 ot, u32 packet, u32 area, u32 offset) {
    cover(0x800943A8u);
    if (fog_off()) return packet;
    Frame frame(*this, 16);
    const u32 world = m.r32(g::kWorld);
    const u32 buckets = m.r32(world + 648);
    const u32 restore_mask = 0xE6000000u | ((m.r32(m.r32(0x800BCF90u) + 1480) & 4u) ? 1u : 0u);
    auto copy = [&](u32 at, u32 xy, u32 dimensions, u32 size) {
        m.w32(at, 0x01000000u);
        m.w32(at + 4, 0x80000000u);
        m.w32(at + 8, xy);
        m.w32(at + 12, dimensions);
        m.w32(at + 16, size);
    };
    for (u32 i = 0; i < 16; ++i) {
        const u32 bucket = ot + 4u * static_cast<u32>(m.r16s(buckets + 2 * i));
        if (i & 1u) {
            m.w32(packet, ((packet + 52) & 0xFFFFFFu) | 0x0C000000u);
            m.w32(packet + 52, 0x0C000000u);
            for (u32 b = 0; b < 3; ++b) m.w8(packet + 52 + b, m.r8(bucket + b));
            m.w16(bucket, packet);
            m.w8(bucket + 2, packet >> 16);
            const u32 x = 32 * (i & 6u), y = (32 * (i & 8u) + 240) << 16;
            m.w32(packet + 4, 0xE6000000u);
            m.w32(packet + 48, restore_mask);
            m.w32(packet + 56, 0xE6000000u);
            m.w32(packet + 100, restore_mask);
            copy(packet + 8, x | y, 0x01000140u, 0x00100010u);
            copy(packet + 28, (x + 16) | y, 0x01100140u, 0x00100010u);
            copy(packet + 60, (x + 32) | y, 0x01200140u, 0x00100010u);
            copy(packet + 80, (x + 48) | y, 0x01300140u, 0x00100010u);
            packet += 104;
        } else {
            m.w32(packet, m.r32(bucket) | 0x07000000u);
            m.w32(bucket, packet & 0xFFFFFFu);
            m.w32(packet + 4, 0xE6000000u);
            m.w32(packet + 28, restore_mask);
            const u32 xy = (8 * (i & 6u) + 320) | ((8 * (i & 8u) + 384) << 16);
            copy(packet + 8, xy, 0x01000140u, 0x00400010u);
            packet += 32;
        }
    }
    m.w32(packet + 12, 0xE5080140u);
    m.w32(packet + 8, 0xE404FD4Fu);
    m.w32(packet, ((packet + 20) & 0xFFFFFFu) | 0x04000000u);
    m.w32(packet + 4, 0xE3040140u);
    m.w32(packet + 16, 0);
    const u32 colour = m.r32(m.r32(m.r32(g::kWorld) + 580) + 8);
    m.w32(packet + 20, ((packet + 44) & 0xFFFFFFu) | 0x05000000u);
    m.w32(packet + 24, 0xE6000001u);
    m.w32(packet + 32, 0);
    m.w32(packet + 36, 0x00400010u);
    m.w32(packet + 40, restore_mask);
    m.w32(packet + 28, (colour & 0xFFFFFFu) | 0x60000000u);
    const u32 restore = packet + 44;
    m.w32(restore, 0x04000000u);
    const s32 x = m.r16s(area), y = m.r16s(area + 2);
    m.w32(restore + 4, 0xE3000000u | (static_cast<u32>(x) & 1023u) | ((static_cast<u32>(y) & 1023u) << 10));
    const u32 right = static_cast<u32>(x + m.r16s(area + 4) - 1) & 1023u;
    const u32 bottom = static_cast<u32>(y + m.r16s(area + 6) - 1) & 1023u;
    m.w32(restore + 8, 0xE4000000u | right | (bottom << 10));
    const u32 draw_offset = m.r32(offset);
    m.w32(restore + 16, 0);
    m.w32(restore + 12, 0xE5000000u | (draw_offset & 0x7FFu) | ((draw_offset >> 5) & 0x3FF800u));
    const u32 bucket = ot + 4u * static_cast<u32>(m.r16s(buckets + 32));
    m.w32(restore, (m.r32(restore) & 0xFF000000u) | (m.r32(bucket) & 0xFFFFFFu));
    m.w32(bucket, (m.r32(bucket) & 0xFF000000u) | (packet & 0xFFFFFFu));
    return packet + 64;
}
}

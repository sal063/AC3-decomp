#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"
#include <algorithm>

namespace ac3::sim {
u32 Game::draw_ring_mesh(u32 context, u32 phase) {
    cover(0x80098414u);
    Frame frame(*this, 56);
    constexpr u32 kWork = 28, kPoints = 76, kProjected = 160, kRingBytes = 252;
    const u32 mesh = context + kWork;
    m.w32(frame.at(56), context);
    for (u32 reg = 0; reg < 5; ++reg) m.w32(mesh + 4 * reg, gte.cfc2(reg));
    const s32 cosine = m.r16s(0x800B7106u + ((phase << 7) & 0x3F80u));
    const u32 focal = static_cast<u32>((cosine + 4096) / 64 + 128);
    const s32 old_focal = m.r16s(context + 26);
    gte.ctc2(26, focal); m.w32(mesh + 72, focal << 11);
    depth_cue(10000, 20000, old_focal);
    for (u32 vertex = 0; vertex < 20; ++vertex) {
        const u32 trig = m.r32(0x800B7104u + 816 * vertex);
        m.w16(mesh + kPoints + 4 * vertex, static_cast<s32>(trig) >> 17);
        m.w16(mesh + kPoints + 4 * vertex + 2, static_cast<s16>(trig) >> 1);
    }
    m.w32(mesh + kPoints + 80, m.r32(mesh + kPoints));
    gte.mtc2(6, 0x3C878787u);
    for (u32 channel = 21; channel < 24; ++channel) gte.ctc2(channel, 0);
    s32 depth = 12288 - static_cast<s32>((phase << 7) & 1023u);
    u32 packet = m.r32(context + 4);
    mesh_ring_project(mesh, mesh + kProjected, static_cast<u32>(depth));
    for (u32 band = 0; band < 24; ++band) {
        const u32 a = mesh + kProjected + kRingBytes * (band & 1u);
        const u32 b = mesh + kProjected + kRingBytes * ((band + 1u) & 1u);
        depth -= 1024;
        mesh_ring_project(mesh, b, static_cast<u32>(depth));
        u32 previous_a = m.r32(a), previous_b = m.r32(b);
        for (u32 vertex = 0; vertex < 20; ++vertex) {
            const u32 p = a + 12 * vertex, q = b + 12 * vertex;
            const u32 next_a = m.r32(p + 12), next_b = m.r32(q + 12);
            const bool visible = previous_a != 0xFFFFFFFFu && previous_b != 0xFFFFFFFFu &&
                next_a != 0xFFFFFFFFu && next_b != 0xFFFFFFFFu && !(previous_a & previous_b & next_a & next_b);
            previous_a = next_a; previous_b = next_b;
            if (!visible) continue;
            m.w32(packet, 0x0C000000u | ((packet + 52) & 0xFFFFFFu));
            m.w32(packet + 4, m.r32(p + 8)); m.w32(packet + 16, m.r32(p + 20));
            m.w32(packet + 8, m.r32(p + 4)); m.w32(packet + 20, m.r32(p + 16));
            m.w32(packet + 28, m.r32(q + 8)); m.w32(packet + 40, m.r32(q + 20));
            const u32 screen_a = m.r32(q + 4), screen_b = m.r32(q + 16);
            m.w16(packet + 36, 0xBF00u); m.w32(packet + 12, 0x4ED68000u);
            m.w32(packet + 24, 0x0019803Fu); m.w16(packet + 48, 0xBF3Fu);
            m.w32(packet + 32, screen_a); m.w32(packet + 44, screen_b);
            packet += 52;
        }
    }
    m.w32(context + 4, packet);
    for (u32 reg = 0; reg < 5; ++reg) gte.ctc2(reg, m.r32(mesh + 4 * reg));
    for (u32 reg = 5; reg < 8; ++reg) gte.ctc2(reg, 0);
    gte.ctc2(26, m.r16(context + 26));
    return 0;
}

u32 Game::draw_slice_mesh(u32 context) {
    cover(0x80098850u);
    Frame frame(*this, 64);
    constexpr u32 kProjected = 52, kSliceBytes = 204;
    const u32 mesh = context + 28;
    m.w32(frame.at(64), context);
    for (u32 channel = 21; channel < 24; ++channel) gte.ctc2(channel, 0);
    depth_cue(2000, 20000, m.r16s(context + 26));
    for (u32 reg = 0; reg < 8; ++reg) m.w32(mesh + 4 * reg, gte.cfc2(reg));
    const u32 world = m.r32(g::kWorld);
    for (u32 reg = 0; reg < 5; ++reg) gte.ctc2(reg, m.r32(world + 44 + 4 * reg));
    gte.mtc2(0, 0); gte.mtc2(1, 4096); gte.op(0x486012u);
    m.w32(mesh + 32, (static_cast<s32>(0u - m.r32(context + 8)) >> 11) & 1023u);
    m.w32(mesh + 40, (static_cast<s32>(0u - m.r32(context + 16)) >> 11) & 1023u);
    m.w32(mesh + 36, static_cast<u32>((static_cast<s32>(0u - m.r32(context + 12)) >> 11) & 2047u) - 5324u);
    m.w32(mesh + 48, gte.mfc2(11));
    for (u32 reg = 0; reg < 5; ++reg) gte.ctc2(reg, m.r32(mesh + 4 * reg));
    gte.mtc2(6, 0x00C05020u);
    u32 packet = m.r32(context + 4);
    m.w32(packet, 0x01000000u | ((packet + 8) & 0xFFFFFFu)); m.w32(packet + 4, 0xE1000220u);
    packet += 8;
    for (u32 sheet = 0; sheet < 4; ++sheet) {
        m.w32(frame.at(16), sheet);
        const s32 axis_depth = m.r32s(mesh + 48) >> 10;
        const s32 first = std::max(0, 4 - axis_depth), last = std::min(16, 13 - axis_depth);
        m.w32(frame.at(20), static_cast<u32>(last));
        auto slice_buffer = [&](s32 index) { return mesh + kProjected + kSliceBytes * (static_cast<u32>(index) & 1u); };
        mesh_slice_project(mesh, slice_buffer(first + 1), static_cast<u32>(first));
        for (s32 slice = first + 1; slice <= last; ++slice) {
            const u32 a = slice_buffer(slice), b = slice_buffer(slice + 1);
            if (slice == last) {
                for (s32 vertex = 16; vertex >= 0; --vertex) m.w32(b + 12 * static_cast<u32>(vertex), 0xFFFFFFFFu);
            } else mesh_slice_project(mesh, b, static_cast<u32>(slice));
            u32 previous = m.r32(a);
            for (u32 vertex = 0; vertex < 16; ++vertex) {
                const u32 p = a + 12 * vertex, q = b + 12 * vertex;
                const u32 next = m.r32(p + 12), across = m.r32(q), common = next & across;
                const bool visible = previous != 0xFFFFFFFFu && common != 0xFFFFFFFFu && !(previous & common);
                previous = next;
                if (!visible) continue;
                const bool clipped = static_cast<s32>(next | across) < 0;
                if (clipped) {
                    const u32 end = packet + 20;
                    const u32 colour = m.r32(p + 8), screen = m.r32(p + 4);
                    m.w32(packet, 0x04000000u | (end & 0xFFFFFFu));
                    m.w32(packet + 4, colour | 0x52000000u); m.w32(packet + 8, screen);
                    const u32 endpoint = static_cast<s32>(next) >= 0 ? p + 12 : q;
                    m.w32(packet + 12, m.r32(endpoint + 8)); m.w32(packet + 16, m.r32(endpoint + 4));
                    packet = end;
                } else {
                    const u32 end = packet + 32;
                    const u32 colour = m.r32(p + 20), screen = m.r32(p + 16);
                    m.w32(packet, 0x07000000u | (end & 0xFFFFFFu));
                    m.w32(packet + 4, colour | 0x5A000000u); m.w32(packet + 8, screen);
                    m.w32(packet + 12, m.r32(p + 8)); m.w32(packet + 16, m.r32(p + 4));
                    m.w32(packet + 20, m.r32(q + 8)); m.w32(packet + 24, m.r32(q + 4));
                    rand(); rand(); m.w32(packet + 28, 0x55555555u); packet = end;
                }
            }
        }
        m.w32(mesh + 36, m.r32(mesh + 36) + 2048u);
    }
    m.w32(context + 4, packet);
    return 0;
}
}

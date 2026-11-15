#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"
#include <algorithm>
#include <cstdlib>

namespace ac3::sim {
u32 Game::draw_cloud_layers() {
    cover(0x8008D9F4u);
    Frame frame(*this, 24);
    constexpr u32 kWork = g::kScratch + 28;
    constexpr u32 kColour = 44, kCounts = 48, kHeights = 56, kSlopes = 64;
    u32 packet = m.r32(g::kScratch + 4);
    const u32 settings = m.r32(m.r32(g::kWorld) + 616) + 8;
    const u32 altitude = 0u - static_cast<u32>(m.r32s(g::kScratch + 12) >> 12);
    for (u32 layer = 0; layer < 2; ++layer) {
        const u32 definition = m.r32(settings + 4 * layer);
        const s32 height = static_cast<s32>(altitude + static_cast<u32>(m.r16s(settings + 4 * layer)));
        m.w32(kWork + kHeights + 4 * layer, static_cast<u32>(height >> (layer + 1)));
        m.w32(kWork + kSlopes + 4 * layer, 0u - (definition >> 25));
        m.w32(kWork + kCounts + 4 * layer, std::min(128u, (definition >> 16) & 255u));
    }
    const u32 cloud_colour = m.r32(settings + 12);
    m.w32(kWork + kColour, (cloud_colour & 0xFFFFFFu) | 0x2E000000u);
    const u32 page = (cloud_colour >> 24) ? 89u : 57u;
    for (u32 i = 21; i < 24; ++i) gte.ctc2(i, 0);
    for (u32 layer = 0; layer < 2; ++layer) {
        s32 camera_x = m.r32s(g::kScratch + 8) >> 13;
        s32 camera_z = m.r32s(g::kScratch + 16) >> 13;
        if (!layer) {
            camera_x += m.r32s(g::kScratch + 8) >> 15;
            camera_z += m.r32s(g::kScratch + 16) >> 15;
        }
        u32 height = m.r32(kWork + kHeights + 4 * layer);
        u32 colour = m.r32(kWork + kColour);
        const s32 fade = static_cast<s32>(height) >> 2;
        if (fade >= 4096) continue;
        if (fade > 0) {
            gte.mtc2(6, colour);
            gte.mtc2(8, static_cast<u32>(fade));
            gte.op(0x780010u);
            colour = gte.mfc2(22);
        }
        if (!(colour & 0xFFFFFFu)) continue;
        gte.mtc2(6, colour);
        m.w32(kWork + 40, colour);
        const u32 count = m.r32(kWork + kCounts + 4 * layer);
        const u32 samples = m.r32(0x800BCF90u);
        for (u32 n = count, sample = samples; n; --n, sample += 4) {
            auto cloud = [&]() {
                const u32 noise = m.r32(sample);
                m.w16(kWork + 2, height);
                m.w16(kWork, ((noise - static_cast<u32>(camera_x)) & 0x7FFFu) - 0x4000u);
                m.w16(kWork + 4, (((noise >> 16) - static_cast<u32>(camera_z)) & 0x7FFFu) - 0x4000u);
                gte.mtc2(0, m.r32(kWork));
                gte.mtc2(1, m.r32(kWork + 4));
                s32 depth_width = 618 + 16 * (n & 7u);
                gte.op(0x180001u);
                if (n & 1u) depth_width = -depth_width;
                const u32 z0 = m.r32(kWork + 4), z1 = z0 + static_cast<u32>(depth_width);
                const u32 xy0 = m.r32(kWork), xy1 = xy0 + 1365 + 16 * (n & 15u);
                m.w32(kWork + 28, z1);
                m.w32(kWork + 20, z1);
                m.w32(kWork + 12, z0);
                m.w32(kWork + 16, xy0);
                m.w32(kWork + 24, xy1);
                m.w32(kWork + 8, xy1);
                if (static_cast<s32>(gte.cfc2(31)) < 0) return;
                const u32 centre = gte.mfc2(14);
                if (std::abs(static_cast<s16>(centre)) >= 241 ||
                    std::abs(static_cast<s32>(centre) >> 16) >= 201) return;
                for (u32 i = 0; i < 6; ++i) gte.mtc2(i, m.r32(kWork + 8 + 4 * i));
                m.w32(packet + 8, centre);
                gte.op(0x280030u);
                const u32 clut = 0x4DD6u + (layer << 6) + (static_cast<s32>(height) > 0 ? 128u : 0u);
                const u32 narrow_u = (n & 3u) << 5, wide_u = (n & 3u) << 6;
                m.w32(packet + 12, (clut << 16) | 0x2000u | narrow_u);
                m.w32(packet + 20, (page << 16) | 0x2000u | (narrow_u + 31));
                m.w16(packet + 28, 0x2F00u | narrow_u);
                m.w16(packet + 36, 0x2F00u | (narrow_u + 31));
                m.w32(kWork + 100, (clut << 16) | wide_u);
                m.w32(kWork + 108, (page << 16) | (wide_u + 63));
                m.w16(kWork + 116, 0x1F00u | wide_u);
                m.w16(kWork + 124, 0x1F00u | (wide_u + 63));
                if (static_cast<s32>(gte.cfc2(31)) < 0) return;
                gte.op(0x168002Eu);
                if (page == 89) m.w32(packet + 40, 0xE1000220u);
                u32 depth = gte.mfc2(7);
                if (depth >= 557) {
                    m.w32(packet + 4, m.r32(kWork + 40));
                    depth = std::min(depth, 4094u);
                } else {
                    if (depth < 300) return;
                    gte.mtc2(8, (556 - depth) << 4);
                    gte.op(0x780010u);
                    m.w32(packet + 4, gte.mfc2(22));
                }
                const u32 bucket = m.r32(g::kScratch) + 4 * depth;
                m.w32(packet, m.r32(bucket) | (page == 89 ? 0x0A000000u : 0x09000000u));
                m.w32(bucket, packet & 0xFFFFFFu);
                if (depth < 1536) {
                    m.w32(packet + 12, m.r32(kWork + 100));
                    m.w32(packet + 20, m.r32(kWork + 108));
                    m.w32(packet + 28, m.r32(kWork + 116));
                    m.w32(packet + 36, m.r32(kWork + 124));
                }
                for (u32 i = 0; i < 3; ++i) m.w32(packet + 16 + 8 * i, gte.mfc2(12 + i));
                packet += page == 89 ? 44 : 40;
            };
            cloud();
            height += m.r32(kWork + kSlopes + 4 * layer);
        }
    }
    m.w32(g::kScratch + 4, packet);
    return 0;
}

u32 Game::draw_speed_streaks() {
    cover(0x8008E038u);
    Frame frame(*this, 72);
    constexpr u32 kWork = g::kScratch + 28;
    const u32 world = m.r32(g::kWorld), view = m.r32(world + 40);
    s32 speed = 0;
    for (u32 i = 0; i < 3; ++i) {
        const s32 velocity = m.r32s(view + 156 + 4 * i) >> 7;
        speed += std::abs(velocity);
        m.w16(kWork + 2 * i, static_cast<u32>(velocity));
    }
    if (static_cast<u32>(speed) - 512u >= 0x2E01u) return 0;
    u32 colour = 0x2E0F0F0Fu;
    for (u32 i = 21; i < 24; ++i) gte.ctc2(i, 0);
    gte.mtc2(6, colour);
    const s32 fade = 4 * (2048 - speed);
    if (fade > 0) {
        gte.mtc2(8, static_cast<u32>(fade));
        gte.op(0x780010u);
        colour = gte.mfc2(22);
        gte.mtc2(6, colour);
    }
    for (u32 i = 0; i < 5; ++i) gte.ctc2(i, m.r32(world + 44 + 4 * i));
    gte.mtc2(0, 0);
    gte.mtc2(1, 4096);
    const u32 start = m.r32(g::kScratch + 4), samples = m.r32(0x800BCF90u);
    gte.op(0x486012u);
    const s32 width_scale = 140 * m.r16s(g::kScratch + 26);
    u32 camera_relative[3];
    for (u32 i = 0; i < 3; ++i)
        camera_relative[i] = 0u - static_cast<u32>(m.r32s(g::kScratch + 8 + 4 * i) >> 9) - gte.mfc2(25 + i);
    for (u32 i = 0; i < 5; ++i) gte.ctc2(i, m.r32(world + 76 + 4 * i));
    gte.ctc2(7, 4096);
    gte.ctc2(26, static_cast<u32>(160 - std::clamp((speed - 2048) >> 8, 0, 10)));
    u32 packet = start;
    for (u32 streak = 0; streak < 16; ++streak) {
        const u32 sample = samples + 8 * streak;
        for (u32 i = 0; i < 3; ++i)
            m.w16(kWork + 8 + 2 * i, ((m.r16(sample + 2 * i) + camera_relative[i]) & 0x3FFFu) - 0x2000u);
        gte.mtc2(0, m.r32(kWork + 8));
        gte.mtc2(1, m.r32(kWork + 12));
        for (u32 i = 0; i < 3; ++i) {
            m.w16(kWork + 16 + 2 * i, m.r16(kWork + 8 + 2 * i) - m.r16(kWork + 2 * i));
            m.w16(kWork + 24 + 2 * i, m.r16(kWork + 8 + 2 * i) + m.r16(kWork + 2 * i));
        }
        for (u32 i = 2; i < 6; ++i) gte.mtc2(i, m.r32(kWork + 8 + 4 * i));
        m.w32(packet, ((packet + 40) & 0xFFFFFFu) | 0x09000000u);
        gte.op(0x280030u);
        const u32 u = (2 * streak) & 0xE0u;
        m.w32(packet + 20, 0x0037C000u | (u + 63));
        m.w32(packet + 12, 0x40D6C000u | u);
        m.w16(packet + 28, 0xFF00u | u);
        m.w16(packet + 36, 0xFF00u | (u + 63));
        if (static_cast<s32>(gte.cfc2(31)) < 0) continue;
        m.w32(kWork + 40, gte.mfc2(13));
        if (std::abs(m.r16s(kWork + 40)) >= 241 || std::abs(m.r16s(kWork + 42)) >= 201) continue;
        m.w32(packet + 4, colour);
        m.w32(kWork + 44, gte.mfc2(14));
        for (u32 i = 0; i < 2; ++i) {
            const u32 depth = gte.mfc2(18 + i);
            m.w32(kWork + 48 + 4 * i, depth ? static_cast<u32>(width_scale / static_cast<s32>(depth)) : 0u);
        }
        m.w32(frame.at(16), sample);
        m.w32(frame.at(20), g::kScratch);
        m.w32(frame.at(24), colour);
        beam_quad_corners(packet, kWork + 32);
        packet += 40;
    }
    u32 result = 0;
    if (packet != m.r32(g::kScratch + 4)) {
        const u32 bucket = m.r32(g::kScratch) + 112;
        result = bucket;
        const u32 original_start = m.r32(g::kScratch + 4);
        for (u32 i = 0; i < 3; ++i) {
            m.w8(packet - 40 + i, m.r8(bucket + i));
            m.w8(bucket + i, original_start >> (8 * i));
        }
        m.w32(g::kScratch + 4, packet);
    }
    gte.ctc2(26, m.r16(g::kScratch + 26));
    gte.ctc2(7, 0);
    return result;
}
}

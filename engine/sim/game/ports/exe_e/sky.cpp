#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"
#include "sim/precise.hpp"
#include "core/wrap.hpp"

#include <algorithm>
#include <cstdlib>

namespace ac3::sim {
namespace {
constexpr u32 kSkyState = 0x800BCF90u;
constexpr u32 kScratch = 0x1F800000u, kSkyWork = kScratch + 28;
constexpr u32 kCircle = 0x800B7104u;
constexpr u32 kMaskOff = 0xE6000000u, kMaskOn = 0xE6000001u;
constexpr u32 kDrawMode = 0xE1000220u;
}

u32 Game::sky_points(u32 context, u32 work, s32 count) {
    cover(0x80095CB8u);
    Frame frame(*this, 8);
    if (count <= 0) return 0;
    count = std::min(count, 256);
    const u32 world = m.r32(g::kWorld);
    for (u32 i = 0; i < 5; ++i) gte.ctc2(i, m.r32(world + 44 + 4 * i));
    gte.mtc2(0, 0);
    gte.mtc2(1, 8192);
    const u32 start = m.r32(context + 4);
    u32 packet = start + 8;
    gte.op(0x486012);
    m.w32(start, (packet & 0xFFFFFFu) | 0x01000000u);
    m.w32(start + 4, kDrawMode);
    const u32 samples = m.r32(kSkyState);
    const s32 fade = 32 * (128 - static_cast<s32>(m.r8(m.r32(world + 616) + 19)));
    const u32 x = 0u - gte.mfc2(25), y = 0u - gte.mfc2(26), z = 0u - gte.mfc2(27);
    gte.ctc2(21, 96);
    gte.ctc2(22, 112);
    gte.ctc2(23, 128);
    gte.mtc2(6, 0x6A807060u);
    gte.mtc2(8, static_cast<u32>(fade));
    m.w32(work + 344, x);
    m.w32(work + 348, y);
    gte.op(0x780010);
    m.w32(work + 352, z);
    m.w16(work, (x & 0x1FFFu) - 4096u);
    m.w16(work + 2, (y & 0x1FFFu) - 4096u);
    m.w16(work + 4, (z & 0x1FFFu) - 4096u);
    const u32 colour = gte.mfc2(22);
    gte.mtc2(6, colour);
    for (u32 i = 0; i < 5; ++i) gte.ctc2(i, m.r32(world + 76 + 4 * i));
    gte.ctc2(5, 0);
    gte.ctc2(6, 0);
    gte.ctc2(7, 8192);
    u32 points = 0;
    for (s32 i = 0; i < count; ++i) {
        gte.mtc2(0, m.r32(work));
        gte.mtc2(1, m.r32(work + 4));
        const s32 sample = m.r32s(samples + 4u * static_cast<u32>(i));
        gte.op(0x180001);
        m.w16(work, ((m.r16(work + 344) + static_cast<u32>(sample)) & 0x1FFFu) - 4096u);
        m.w16(work + 2, ((m.r16(work + 348) + static_cast<u32>(sample >> 8)) & 0x1FFFu) - 4096u);
        m.w16(work + 4, ((m.r16(work + 352) + static_cast<u32>(sample >> 16)) & 0x1FFFu) - 4096u);
        const u32 screen = gte.mfc2(14);
        const s32 depth = static_cast<s32>(gte.mfc2(19));
        if (std::abs(static_cast<s32>(screen) >> 16) >= 121 ||
            std::abs(static_cast<s32>(static_cast<s16>(screen))) >= 161) continue;
        const s32 distance = std::abs(depth - 8192);
        if (distance > 4096) continue;
        gte.mtc2(8, static_cast<u32>(distance));
        const u32 point = packet + 4 + 8 * points;
        gte.op(0x780010);
        m.w32(point + 4, screen);
        if (++points == 7) {
            m.w32(packet, ((packet + 60) & 0xFFFFFFu) | 0x0E000000u);
            packet += 60;
            points = 0;
        }
        m.w32(point, gte.mfc2(22));
        if (!(i & 15)) m.w8(point, colour >> 8);
    }
    if (points) {
        const u32 end = packet + 4 + 8 * points;
        m.w32(context + 4, end);
        m.w32(packet, (points << 25) | (end & 0xFFFFFFu));
        return end & 0xFFFFFFu;
    }
    if (packet != start + 8) m.w32(context + 4, packet);
    return start + 8;
}

u32 Game::sky_cylinder(u32 context, u32 work) {
    cover(0x80095930u);
    Frame frame(*this, 40);
    const u32 world = m.r32(g::kWorld), sky = m.r32(world + 616);
    const u32 settings = m.r32(sky + 4), colour_word = m.r32(sky + 24);
    const bool tall = settings & 1u;
    const s32 height = wrap::sub(static_cast<s32>(settings) >> 16, m.r32s(context + 12));
    const s32 fade = wrap::sub(height, 4000) >> 3;
    if (fade >= 4096) return 0;
    for (u32 i = 0; i < 5; ++i) gte.ctc2(i, m.r32(world + 76 + 4 * i));
    for (u32 i = 5; i < 8; ++i) gte.ctc2(i, 0);
    const u32 page = colour_word >> 24 ? 89u : 57u;
    gte.mtc2(8, static_cast<u32>(fade));
    for (u32 i = 21; i <= 23; ++i) gte.ctc2(i, 0);
    gte.mtc2(6, (colour_word & 0xFFFFFFu) | 0x2E000000u);
    gte.op(0x780010);
    s32 half_diagonal = 200;
    if (gte.x_squeeze != 0x10000 && gte.x_squeeze > 0) half_diagonal = static_cast<s32>(200LL * 0x10000 / gte.x_squeeze) + 1;
    const s32 half_view = ratan2_lib(half_diagonal, m.r16s(context + 26));
    u32 angle = static_cast<u32>((m.r16s(context + 22) >> 2) - half_view) & 0xF80u;
    const u32 colour = gte.mfc2(22);
    const s32 half_height = tall ? 6144 : 3072;
    m.w16(work + 278, static_cast<u32>(wrap::sub(height, half_height) >> 2));
    m.w16(work + 282, static_cast<u32>(wrap::add(height, half_height) >> 2));
    auto project = [&](u32 at_angle) {
        const u32 x = m.r16(kCircle + 4 * at_angle) << 2;
        m.w16(work + 280, x);
        m.w16(work + 276, x);
        const u32 z = static_cast<u32>(m.r16s(kCircle + 4 * at_angle + 2)) << 2;
        gte.mtc2(0, m.r32(work + 276));
        gte.mtc2(2, m.r32(work + 280));
        gte.mtc2(1, z);
        gte.mtc2(3, z);
    };
    project(angle);
    gte.mtc2(4, 0);
    gte.mtc2(5, 0);
    const u32 start = m.r32(context + 4);
    u32 packet = start;
    gte.op(0x280030);
    s32 remaining = (half_view >> 6) + 2;
    u32 texture = (angle >> 7) - 1u;
    m.w32(work + 284, gte.mfc2(12));
    m.w32(work + 288, gte.mfc2(13));
    bool previous_visible = false;
    while (remaining-- > 0) {
        angle = (angle + 128) & 0xFFFu;
        project(angle);
        texture = (texture + 1) & 15u;
        gte.op(0x280030);
        m.w32(packet + 8, m.r32(work + 284));
        m.w32(packet + 24, m.r32(work + 288));
        const u32 u = (texture & 3u) << 6;
        m.w8(packet + 28, u);
        m.w8(packet + 12, u);
        m.w8(packet + 36, u + 63);
        m.w8(packet + 20, u + 63);
        const u32 v = tall ? ((texture << 4) & 64u) : ((texture << 3) & 96u);
        m.w8(packet + 21, v + 128);
        m.w8(packet + 13, v + 128);
        m.w8(packet + 37, v + (tall ? 191u : 159u));
        m.w8(packet + 29, v + (tall ? 191u : 159u));
        m.w32(work + 284, gte.mfc2(12));
        m.w32(work + 288, gte.mfc2(13));
        const bool visible = std::abs(static_cast<s32>(m.r16s(work + 284))) + std::abs(static_cast<s32>(m.r16s(work + 288))) < 361 &&
                             std::abs(static_cast<s32>(m.r16s(work + 286))) + std::abs(static_cast<s32>(m.r16s(work + 290))) < 281;
        if (!visible && !previous_visible) {
            if (packet != m.r32(context + 4)) break;
            continue;
        }
        previous_visible = visible;
        m.w32(packet, ((packet + 40) & 0xFFFFFFu) | 0x09000000u);
        m.w32(packet + 4, colour);
        m.w16(packet + 14, 0x4ED6);
        m.w16(packet + 22, page);
        m.w32(packet + 16, m.r32(work + 284));
        m.w32(packet + 32, m.r32(work + 288));
        packet += 40;
    }
    u32 result = 89;
    if (packet != m.r32(context + 4)) {
        if (page == 89) {
            const u32 mode = packet;
            packet += 8;
            result = (packet & 0xFFFFFFu) | 0x01000000u;
            m.w32(mode, result);
            m.w32(mode + 4, kDrawMode);
        }
        m.w32(context + 4, packet);
    }
    return result;
}

void Game::horizon_draw(u32 context, u32 work) {
    cover(0x80095074u);
    Frame frame(*this, 40);
    const u32 world = m.r32(g::kWorld);
    s32 stars = (m.r16(m.r32(world + 580) + 2) & 0x100u) ? m.r8(m.r32(world + 616) + 28) + 1 : 0;
    const u32 mask = static_cast<u32>(m.r16s(work + 40));
    if (mask) {
        if (mask & 1u) horizon_flat_band(context, work, 0);
        if (mask & 3u) horizon_shaded_band(context, work, 1);
        if (mask & 6u) horizon_shaded_band(context, work, 2);
        if (stars) sky_points(context, work, stars);
        if (mask & 12u) horizon_shaded_band(context, work, 3);
        if (mask & 8u) {
            if (m.r32(work + 368)) {
                m.w32(work + 264, m.r32(work + 268));
                horizon_shaded_band(context, work, 4);
            } else horizon_flat_band(context, work, 4);
        }
        if ((mask & 16u) && m.r32(work + 364)) horizon_flat_band(context, work, 5);
        return;
    }
    s32 colour;
    const s32 angle = static_cast<s32>(static_cast<u32>(ratan2_lib(m.r32s(context + 12), 32767)) << 2);
    if (angle < m.r16s(context + 20)) colour = m.r32s(work + 248);
    else {
        stars = 0;
        if (wrap::neg(m.r32s(context + 12)) < m.r32s(work + 272))
            colour = m.r32(work + 364) ? m.r32s(work + 268) : -1;
        else colour = m.r32s(work + 264);
    }
    if (colour >= 0) {
        const u32 packet = m.r32(context + 4);
        m.w32(packet, ((packet + 16) & 0xFFFFFFu) | 0x03000000u);
        m.w32(packet + 4, static_cast<u32>(colour) | 0x60000000u);
        m.w32(packet + 12, 0x00F00140u);
        m.w32(packet + 8, m.r32(work + 232));
        m.w32(context + 4, packet + 16);
    }
    if (stars) sky_points(context, work, stars);
}

u32 Game::draw_sky(u32 ot, u32 packet) {
    cover(0x800957A4u);
    Frame frame(*this, 40);
    m.w32(kScratch, ot);
    m.w32(kScratch + 4, packet + 8);
    horizon_view_setup(kScratch, kSkyWork);
    m.w32(kSkyWork + 368, m.r16s(m.r32(g::kScene) + 908) == 6 ? 1 : 0);
    horizon_colours(kScratch, kSkyWork);
    if (gte.precise && player_settings) {
        PreciseCache::Water& w = gte.precise->water;
        const u32 environment = m.r32(m.r32(g::kWorld) + 580);
        for (u32 band = 0; band < 4; ++band)
            for (u32 c = 0; c < 3; ++c) w.sky[band][c] = static_cast<float>(m.r8(kSkyWork + 248 + 4 * band + c) / 255.0);
        static constexpr u32 kHeights[3] = {34, 32, 16};
        for (u32 i = 0; i < 3; ++i) w.sky_tan[i] = static_cast<float>(-m.r16s(environment + kHeights[i]) / 32767.0);
        w.sky_age = 0;
    }
    horizon_project(kSkyWork, static_cast<s32>(horizon_boundaries(kScratch, kSkyWork)));
    horizon_clip_codes(kSkyWork);
    horizon_draw(kScratch, kSkyWork);
    if (m.r16(m.r32(m.r32(g::kWorld) + 580) + 2) & 0x200u) sky_cylinder(kScratch, kSkyWork);
    const u32 mask = (m.r32(kSkyWork + 360) & 4u) ? kMaskOn : kMaskOff;
    u32 tail = packet;
    if (m.r32(kScratch + 4) == packet + 8) {
        m.w32(packet, 0x01000000u);
        m.w32(packet + 4, mask);
    } else {
        m.w32(packet, ((packet + 8) & 0xFFFFFFu) | 0x01000000u);
        m.w32(packet + 4, kMaskOn);
        tail = m.r32(kScratch + 4);
        m.w32(tail, 0x01000000u);
        m.w32(tail + 4, mask);
    }
    for (u32 i = 0; i < 3; ++i) m.w8(tail + i, m.r8(ot + i));
    m.w16(ot, packet);
    m.w8(ot + 2, packet >> 16);
    m.w32(kScratch + 4, tail + 8);
    m.w32(m.r32(kSkyState) + 1480, 0);
    return m.r32(kScratch + 4);
}
}

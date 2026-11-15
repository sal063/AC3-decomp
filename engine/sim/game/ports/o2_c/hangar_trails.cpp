#include "core/wrap.hpp"
#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kViewMatrix = 0x40, kProjectedPoints = 0x60, kPreviousPoints = 0x74;
constexpr u32 kAngleTracks = 0xF0, kPositionTracks = 0xF4, kCrossSection = 0xF8;
constexpr u32 kSamples = 15, kCorners = 5, kStrips = 4;
}

u32 Game::o2_hangar_project_mesh(u32 vertices, u32 view, u32 count) {
    cover(0x801017A8u);
    for (u32 word = 0; word < 8; ++word) gte.ctc2(word, m.r32(view + kViewMatrix + 4 * word));
    u32 processed = 0;
    for (;;) {
        const u32 first = processed;
        u32 loaded = 0;
        do {
            gte.mtc2(2 * loaded, m.r32(vertices));
            gte.mtc2(2 * loaded + 1, m.r32(vertices + 4));
            vertices += 8;
            ++loaded; ++processed;
        } while (loaded < 3 && static_cast<s32>(processed) < static_cast<s32>(count));
        gte.op(0x280030u);
        for (u32 vertex = 0; vertex < loaded; ++vertex)
            m.w32(view + kProjectedPoints + 4 * (first + vertex), gte.mfc2(12 + vertex));
        if (static_cast<s32>(processed) >= static_cast<s32>(count))
            return loaded == 3 ? 0 : view + kProjectedPoints + 4 * (processed - 1);
    }
}

u32 Game::o2_hangar_trail_strip(u32 hangar, u32 points, u32 age, u32 packet) {
    cover(0x801018D8u);
    const u32 ot = m.r32(hangar + 8);
    auto shade = [](u32 step) {
        const s32 faded = 64 - static_cast<s32>(step << 6) / 15;
        const u32 value = static_cast<u32>(static_cast<s16>(faded));
        return value | (value << 8) | (value << 16);
    };
    const u32 nearColour = shade(age), farColour = shade(age + 1);
    for (u32 strip = 0; strip < kStrips; ++strip) {
        m.w32(packet + 4, nearColour); m.w32(packet + 12, nearColour);
        m.w32(packet + 20, farColour); m.w32(packet + 28, farColour);
        m.w32(packet + 8, m.r32(points));
        m.w32(packet + 16, m.r32(points + 4 + 4 * strip));
        m.w32(packet + 24, m.r32(points + 20));
        m.w32(packet + 32, m.r32(points + 24 + 4 * strip));
        m.w8(packet + 7, 0x3A);
        m.w32(packet, m.r32(ot) | 0x08000000u);
        m.w32(ot, packet & 0x00FFFFFFu);
        packet += 36;
    }
    return packet;
}

u32 Game::o2_hangar_trails(u32 hangar, u32 view, u32 packet) {
    cover(0x80101AD4u);
    Frame frame(*this, 48);
    packet = o2_hangar_draw_mode(0, m.r32(hangar + 8), packet);
    for (u32 trail = 0; trail < 3; ++trail) {
        const u32 track = 4 * trail;
        for (u32 sample = kSamples - 1; sample > 0; --sample) {
            for (u32 component = 0; component < 3; ++component) {
                const u32 position = m.r32(m.r32(hangar + kPositionTracks) + track) + 16 * sample;
                m.w32(position + 4 * component, m.r32(position - 16 + 4 * component));
            }
            for (u32 component = 0; component < 3; ++component) {
                const u32 angles = m.r32(m.r32(hangar + kAngleTracks) + track) + 8 * sample;
                m.w16(angles + 2 * component, m.r16(angles - 8 + 2 * component));
            }
        }
        for (u32 component = 0; component < 3; ++component) {
            const u32 delta = static_cast<u32>(rand()) & 31;
            const u32 angles = m.r32(m.r32(hangar + kAngleTracks) + track);
            m.w16(angles + 2 * component, m.r16(angles + 2 * component) + delta);
        }
        rot_matrix(m.r32(m.r32(hangar + kAngleTracks) + track), view);
        trans_matrix(view, m.r32(m.r32(hangar + kPositionTracks) + track) + 16);
        for (u32 word = 0; word < 8; ++word) gte.ctc2(word, m.r32(view + 4 * word));
        gte.mtc2(0, m.r32(hangar + 0xFC));
        gte.mtc2(1, m.r32(hangar + 0x100));
        gte.op(0x480012u);
        const u32 newest = m.r32(m.r32(hangar + kPositionTracks) + track);
        for (u32 component = 0; component < 3; ++component) m.w32(newest + 4 * component, gte.mfc2(25 + component));
        if (!o2_hangar_points_visible(m.r32(m.r32(hangar + kPositionTracks) + track))) {
            o2_hangar_streak(trail, hangar);
            continue;
        }
        for (u32 sample = 0; sample < kSamples; ++sample) {
            rot_matrix(m.r32(m.r32(hangar + kAngleTracks) + track) + 8 * sample, view);
            trans_matrix(view, m.r32(m.r32(hangar + kPositionTracks) + track) + 16 * sample);
            comp_matrix(hangar + 0x2C, view, view + kViewMatrix);
            o2_hangar_project_mesh(m.r32(hangar + kCrossSection), view, kCorners);
            if (sample) packet = o2_hangar_trail_strip(hangar, view + kProjectedPoints, sample - 1, packet);
            for (u32 corner = 0; corner < kCorners; ++corner)
                m.w32(view + kPreviousPoints + 4 * corner, m.r32(view + kProjectedPoints + 4 * corner));
        }
    }
    return o2_hangar_draw_mode(1, m.r32(hangar + 8) + 0x3FC, packet);
}

u32 Game::o2_hangar_trails_tick(u32 hangar, u32 view, u32 packet) {
    cover(0x80101E4Cu);
    Frame frame(*this, 32);
    if (m.r32(hangar + 0x1C) == 1) m.w32(hangar + 0x1C, 2);
    if (m.r32(hangar + 0x1C) == 3) {
        const u32 timer = m.r32(hangar + 0x28);
        if (static_cast<s32>(timer) < 60) m.w32(hangar + 0x28, timer + 1);
        else m.w32(hangar + 0x1C, 0);
    }
    o2_hangar_camera_reset(hangar, view);
    return o2_hangar_trails(hangar, view, packet);
}
}

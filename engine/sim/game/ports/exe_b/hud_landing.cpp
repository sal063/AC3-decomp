#include <algorithm>
#include "core/wrap.hpp"
#include "sim/game/globals.hpp"
#include "hud_packets.hpp"

namespace ac3::sim {
using namespace hud_packets;
namespace {
u32 landing_line(Memory& m, Point a, Point b, u32 rgb) {
    return line(m, kLandingBlocks, kLandingPrims, 12, 0xC00, a, b, rgb);
}
u32 landing_polyline(Memory& m, u32 a, u32 b, u32 c, u32 green) {
    const u32 counts = m.r32(kLandingBlocks);
    const u32 packet = m.r32(kLandingPrims) + 24 * m.r32(counts + 8);
    color(m, packet, green << 8);
    m.w32(packet + 8, a); m.w32(packet + 12, b); m.w32(packet + 16, c);
    const u32 count = m.r32(counts + 8) + 1;
    m.w32(counts + 8, count);
    return count;
}
}

u32 Game::landing_draw_begin() {
    cover(0x80042BF4u);
    const u32 counts = m.r32(kLandingBlocks);
    for (u32 offset : {16u, 12u, 8u}) m.w32(counts + offset, 0);
    const u32 packet = m.r32(counts + 4 * m.r32(m.r32(g::kScene) + 4));
    m.w32(kLandingPrims, packet);
    return packet;
}

u32 Game::landing_draw_link() {
    cover(0x80042C2Cu);
    const u32 ot = m.r32(m.r32(g::kWorld) + 16);
    struct Pool { u32 count, start, stride; };
    u32 result = 0;
    for (const Pool pool : {Pool{8, 0, 24}, {12, 0xC00, 16}, {16, 0x1400, 20}}) {
        const u32 counts = m.r32(kLandingBlocks);
        result = m.r32(counts + pool.count);
        if (static_cast<s32>(result) <= 0) continue;
        const u32 packets = m.r32(kLandingPrims);
        for (s32 i = 0; i < m.r32s(counts + pool.count); ++i)
            ot_link(ot, packets + pool.start + pool.stride * static_cast<u32>(i));
        result = 0;
    }
    return result;
}

u32 Game::landing_draw_chevron(u32 a, u32 b, u32 c, u32 shade) {
    cover(0x80043E0Cu);
    const u32 green = ((m.r32(m.r32(g::kWorld) + 12) + shade) & 15u) << 4;
    return landing_polyline(m, a, b, c, green);
}

u32 Game::landing_draw_rectangle(u32 a, u32 b, u32 c, u32 d) {
    cover(0x80043E74u);
    landing_polyline(m, a, b, d, 255);
    return landing_polyline(m, a, c, d, 255);
}

u32 Game::landing_draw_arrow(u32 kind, s32 x, s32 y) {
    cover(0x80043EFCu);
    constexpr u32 kArrows = 0x800B14F8u;
    const u32 shape = kArrows + 8 * kind, counts = m.r32(kLandingBlocks);
    const u32 packet = m.r32(kLandingPrims) + 0x1400 + 20 * m.r32(counts + 16);
    point(m, packet, 8, {x, y});
    point(m, packet, 12, {x + m.r8s(shape), y + m.r8s(shape + 1)});
    point(m, packet, 16, {x + m.r8s(shape + 2), y + m.r8s(shape + 3)});
    m.w32(counts + 16, m.r32(counts + 16) + 1);
    return landing_line(m, {x + m.r8s(shape + 4), y + m.r8s(shape + 5)},
                        {x + m.r8s(shape + 6), y + m.r8s(shape + 7)}, 0x00FF00);
}

u32 Game::landing_draw_none(u32 previous) {
    cover(0x80043BF0u);
    return previous;
}

u32 Game::landing_draw_attitude() {
    cover(0x80043BF8u);
    Frame frame(*this, 32);
    const u32 player = slot(0);
    const s32 roll = m.r16s(player + 28);
    if (roll >= 456) { landing_draw_arrow(2, 80, 120); landing_draw_arrow(3, 240, 120); }
    if (roll < -455) { landing_draw_arrow(3, 80, 120); landing_draw_arrow(2, 240, 120); }
    const u32 rawPitch = 1820u - m.r16(player + 24);
    const s32 pitch = static_cast<s16>(rawPitch);
    if (pitch >= 228) landing_draw_arrow(2, 160, 60);
    if (pitch < -227) landing_draw_arrow(3, 160, 180);
    const s32 offset = m.r16s(0x800104ACu + 2 * ((-pitch) & 0x3FFF)) >> 8;
    const bool level = ((rawPitch + 227u) & 0xFFFFu) < 455;
    landing_line(m, {140, 120 + offset}, {180, 120 + offset}, level ? 0x008000 : 0x000080);
    landing_line(m, {140, 120}, {180, 120}, 0x004000);
    landing_line(m, {140, 115}, {180, 115}, 0x002000);
    return landing_line(m, {140, 125}, {180, 125}, 0x002000);
}

u32 Game::landing_prompts_carrier(u32 hud) {
    cover(0x8004357Cu);
    Frame frame(*this, 32);
    const u32 guidance = m.r32(hud + 56), player = slot(0);
    if (player_mode() & 0x80u) return 0;
    const auto tick = [&] { return m.r32(m.r32(g::kWorld) + 12) & 127u; };
    if (m.r32s(guidance + 24) >= 8193) {
        const u32 heading = (m.r16(player + 26) + static_cast<u32>(ratan2(m.r32s(guidance + 28) >> 12,
                                                    -m.r32s(guidance + 36) >> 12))) & 0x3FFFu;
        const s32 error = static_cast<s16>(heading >= 0x2000 ? heading | 0xC000u : heading);
        if (error >= 257) {
            if (error >= 1025 && tick() == 0) hud_message(13);
            landing_draw_arrow(0, 80, 120);
        } else if (error < -256) {
            if (error < -1024 && tick() == 0) hud_message(12);
            landing_draw_arrow(1, 240, 120);
        }
        if (m.r32s(guidance + 32) > 0 && m.r16s(player + 24) < 0) {
            if (m.r16s(player + 24) < -2048 && tick() == 64) hud_message(10);
            landing_draw_arrow(2, 160, 60);
        }
        if (m.r32s(guidance + 32) < -1 && m.r16s(player + 24) > 0) {
            if (m.r16s(player + 24) >= 2049 && tick() == 64) hud_message(11);
            landing_draw_arrow(3, 160, 180);
        }
    }
    const s32 lateral = std::clamp(-(m.r32s(guidance + 28) >> 12) / 50, -64, 64);
    const s32 vertical = std::clamp(-(m.r32s(guidance + 32) >> 12) / 80, -64, 64);
    const s32 range = std::clamp(-(m.r32s(guidance + 36) >> 12) / 150, -24, 24);
    landing_line(m, {160 + lateral, 156}, {160 + lateral, 204}, lateral ? 0x00FF00 : 0xFF0000);
    landing_line(m, {136, 180 + vertical}, {184, 180 + vertical}, vertical ? 0x00FF00 : 0xFF0000);
    const u32 rangeColor = range ? 0x00FF00 : 0xFF0000;
    const Point diamond[] = {{160, 156 - range}, {184 + range, 180}, {160, 204 + range}, {136 - range, 180}};
    for (u32 i = 0; i < 4; ++i) landing_line(m, diamond[i], diamond[(i + 1) & 3], rangeColor);
    const Point reference[] = {{160, 156}, {184, 180}, {160, 204}, {136, 180}};
    for (u32 i = 0; i < 4; ++i) landing_line(m, reference[i], reference[(i + 1) & 3], rangeColor);
    return 0;
}

u32 Game::landing_prompts_runway(u32 hud) {
    cover(0x80042D98u);
    Frame frame(*this, 0xA8);
    m.w32(frame.at(0xA8), hud);
    const u32 guidance = m.r32(hud + 56), player = slot(0);
    const u32 vector = frame.at(0x48), local = frame.at(0x58), matrix = frame.at(0x10);
    const u32 view = m.r32(m.r32(g::kWorld) + 40);
    for (u32 axis = 0; axis < 3; ++axis)
        m.w32(vector + 4 * axis, static_cast<u32>((m.r32s(guidance + 4 * axis) - m.r32s(view + 4 + 4 * axis)) >> 12));
    const u32 shift = m.r16s(m.r32(g::kScene) + 908) == 25 ? 3u : 8u;
    m.w32(vector, m.r32(vector) + static_cast<u32>(rsin(m.r16s(guidance + 18)) >> shift));
    m.w32(vector + 8, m.r32(vector + 8) + static_cast<u32>(rcos(m.r16s(guidance + 18)) >> shift));
    apply_matrix_lv(m.r32(g::kWorld) + 76, vector, local);
    for (u32 axis = 0; axis < 3; ++axis) m.w32(matrix + 20 + 4 * axis, m.r32(local + 4 * axis));
    rot_matrix_yxz(guidance + 16, matrix);
    set_mul_matrix(m.r32(g::kWorld) + 76, matrix);
    set_trans_matrix(matrix);
    const u32 vertex = frame.at(0x30), projected = frame.at(0x38), depth = frame.at(0x70), flags = frame.at(0x74);
    for (u32 side = 0; side < 3; ++side) {
        for (u32 row = 0; row < 16; ++row) {
            bool visible = true;
            const u32 vertices = side == 2 ? 4u : 3u;
            for (u32 corner = 0; corner < vertices; ++corner) {
                const u32 xTable = side == 0 ? 0x800B14C8u : side == 1 ? 0x800B14D0u : 0x800B14E8u;
                const u32 yTable = side == 2 ? 0x800B14F0u : 0x800B14D8u;
                m.w16(vertex, m.r16(xTable + 2 * corner));
                m.w16(vertex + 2, m.r16(yTable + 2 * corner) - 8 * row * row);
                m.w16(vertex + 4, (side == 2 ? 0u : m.r16(0x800B14E0u + 2 * corner)) - 1024 * row);
                rot_trans_pers(vertex, projected + 4 * corner, depth, flags);
                if (m.r32s(flags) < 0) { visible = false; break; }
            }
            if (!visible) continue;
            if (side == 2) landing_draw_rectangle(m.r32(projected), m.r32(projected + 4), m.r32(projected + 8), m.r32(projected + 12));
            else landing_draw_chevron(m.r32(projected), m.r32(projected + 4), m.r32(projected + 8), row);
        }
    }
    const u32 angles = frame.at(0x68);
    m.w32(angles, m.r32(guidance + 16)); m.w32(angles + 4, m.r32(guidance + 20));
    m.w16(angles + 2, 0x4000u - m.r16(angles + 2));
    for (u32 axis = 0; axis < 3; ++axis)
        m.w32(vector + 4 * axis, static_cast<u32>((m.r32s(player + 8 + 4 * axis) - m.r32s(guidance + 4 * axis)) >> 12));
    rot_matrix_yxz(angles, matrix);
    apply_matrix_lv(matrix, vector, local);
    const auto tick = [&] { return m.r32(m.r32(g::kWorld) + 12) & 127u; };
    const u32 heading = (2u * static_cast<u32>(ratan2(m.r32s(guidance + 28) >> 12, -m.r32s(guidance + 36) >> 12))
                        + m.r16(guidance + 46)) & 0x3FFFu;
    if (heading - 513u < 0x1DFFu) {
        if (heading >= 2049 && tick() == 0) hud_message(13);
        landing_draw_arrow(0, 80, 120);
    } else if (heading - 0x2000u < 0x1E00u) {
        if (heading < 14336 && tick() == 0) hud_message(12);
        landing_draw_arrow(1, 240, 120);
    }
    if (m.r32(guidance + 52) == 1) {
        if (tick() == 0) hud_message(3);
    } else {
        const s32 error = static_cast<s16>(m.r16(player + 24) - static_cast<u32>(ratan2(m.r32s(guidance + 32) >> 12,
                                                                              -m.r32s(guidance + 36) >> 12)));
        const s32 distance = m.r32s(guidance + 24);
        if (error >= 513 && distance >= 2049 && distance < 16384) {
            if (m.r32s(guidance + 36) < 0 && error >= 2049 && tick() == 64) hud_message(11);
            landing_draw_arrow(3, 160, 180);
        } else if (m.r32s(hud + 48) <= 0xFFFFF || error < -512) {
            if (tick() == 64) hud_message(10);
            landing_draw_arrow(2, 160, 60);
        }
    }
    const s32 lateral = std::clamp(-m.r32s(local) / 100, -64, 64);
    const s32 vertical = std::clamp((-m.r32s(local + 4) - 70) / 100, -64, 64);
    landing_line(m, {160 + lateral, 100}, {160 + lateral, 140}, 0x00FF00);
    landing_line(m, {140, 120 + vertical}, {180, 120 + vertical}, 0x00FF00);
    landing_line(m, {160, 100}, {160, 140}, 0x004000);
    return landing_line(m, {140, 120}, {180, 120}, 0x004000);
}

u32 Game::landing_guidance(u32 hud) {
    cover(0x80042B20u);
    Frame frame(*this, 32);
    const u32 entry = 0x800B1FC4u + 4 * m.r32(0x800BCEA8u);
    landing_draw_begin();
    if (!(player_mode() & 0x240u)) {
        const u32 kind = m.r8(entry);
        if (kind < 2) landing_prompts_runway(hud);
        else if (kind - 2u < 2) landing_prompts_carrier(hud);
        else if (kind == 5 || kind == 6) landing_draw_none(1);
        else if (kind == 4) landing_draw_attitude();
    }
    return landing_draw_link();
}
}

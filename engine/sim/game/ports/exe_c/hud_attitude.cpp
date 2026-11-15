#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"
#include <array>

namespace ac3::sim {
namespace {
constexpr u32 kHudCounts = 0x800BD074u;
constexpr u32 kHudPackets = 0x800BD078u;
constexpr u32 kHudSecondaryColor = 0x800C6D68u;
constexpr u32 kAttitudeVertices = 0x800B2C18u;
constexpr u32 kEntitySlots = 0x800BCE5Cu;
}

u32 Game::hud_attitude_if_tilted(u32 hud) {
    cover(0x80064400u);
    Frame frame(*this, 0x18);
    const u32 angles = m.r32(hud + 16);
    for (u32 axis = 0; axis < 3; ++axis)
        if (((m.r16(angles + 2 * axis) + 2u) & 0xFFFFu) >= 5u) return hud_attitude_matrix();
    return 1;
}

u32 Game::hud_attitude_matrix() {
    cover(0x8006466Cu);
    Frame frame(*this, 0x88);
    const u32 angles = frame.at(0x70), viewMatrix = frame.at(0x50);
    const u32 aircraftMatrix = frame.at(0x30), withoutYaw = frame.at(0x10);
    const u32 world = m.r32(g::kWorld);
    for (u32 axis = 0; axis < 3; ++axis) m.w16(angles + 2 * axis, m.r16(m.r32(world + 40) + 20 + 2 * axis));
    rot_matrix_yxz_t(angles, viewMatrix);
    const u32 slots = m.r32(kEntitySlots);
    for (u32 axis = 0; axis < 3; ++axis) m.w16(angles + 2 * axis, m.r16(m.r32(slots) + 24 + 2 * axis));
    rot_matrix_yxz(angles, aircraftMatrix);
    m.w16(angles + 2, 0);
    rot_matrix_yxz(angles, withoutYaw);
    for (u32 axis = 0; axis < 3; ++axis) {
        const u32 value = axis == 2 ? 1024u : 0u;
        m.w32(viewMatrix + 20 + 4 * axis, value);
        m.w32(withoutYaw + 20 + 4 * axis, value);
        m.w32(aircraftMatrix + 20 + 4 * axis, value);
    }
    set_mul_matrix(viewMatrix, aircraftMatrix);
    set_trans_matrix(aircraftMatrix);
    return hud_attitude_shape(160, 200);
}

u32 Game::hud_attitude_shape(s32 x, s32 y) {
    cover(0x8006477Cu);
    Frame frame(*this, 0x50);
    const u32 screens = frame.at(0x10), areas = frame.at(0x28);
    set_geom_offset(x, y);
    for (u32 vertex = 0; vertex < 4; ++vertex)
        rot_trans_pers(kAttitudeVertices + 8 * vertex, screens + 4 * vertex, frame.at(0x38), frame.at(0x3C));
    set_geom_offset(160, 120);
    constexpr std::array<std::array<u32, 3>, 4> faces = {{{0, 2, 1}, {0, 1, 3}, {0, 3, 2}, {1, 2, 3}}};
    for (u32 face = 0; face < 4; ++face) {
        const auto& v = faces[face];
        gte.mtc2(12, m.r32(screens + 4 * v[0]));
        gte.mtc2(14, m.r32(screens + 4 * v[2]));
        gte.mtc2(13, m.r32(screens + 4 * v[1]));
        gte.op(0x1400006);
        m.w32(areas + 4 * face, gte.mfc2(24));
    }
    u32 packet = m.r32(kHudPackets) + 392 + 28 * m.r32(m.r32(kHudCounts) + 12);
    for (u32 face = 0; face < 4; ++face) {
        if (m.r32s(areas + 4 * face) <= 0) continue;
        const auto& v = faces[face];
        for (u32 corner = 0; corner < 3; ++corner) {
            m.w8(packet + 4 + 8 * corner, 0);
            m.w8(packet + 5 + 8 * corner, v[corner] == 1 ? 64 : 16);
            m.w8(packet + 6 + 8 * corner, 0);
        }
        for (u32 corner = 0; corner < 3; ++corner) m.w32(packet + 8 + 8 * corner, m.r32(screens + 4 * v[corner]));
        packet += 28;
        const u32 counts = m.r32(kHudCounts);
        m.w32(counts + 12, m.r32(counts + 12) + 1);
    }
    packet = m.r32(kHudPackets) + 2008 + 20 * m.r32(m.r32(kHudCounts) + 24);
    constexpr std::array<std::array<u32, 4>, 6> edges = {{{0, 1, 0, 1}, {0, 2, 0, 2},
        {0, 3, 1, 2}, {1, 2, 0, 3}, {1, 3, 1, 3}, {2, 3, 2, 3}}};
    u32 result = 0;
    for (u32 edge = 0; edge < edges.size(); ++edge) {
        const auto& e = edges[edge];
        const bool visible = m.r32s(areas + 4 * e[2]) >= 0 || m.r32s(areas + 4 * e[3]) >= 0;
        if (edge == 5 && !visible) result = m.r32(areas + 12);
        if (!visible) continue;
        for (u32 endpoint = 0; endpoint < 2; ++endpoint) {
            m.w8(packet + 4 + 8 * endpoint, 0);
            m.w8(packet + 5 + 8 * endpoint, e[endpoint] == 1 ? 64 : 24);
            m.w8(packet + 6 + 8 * endpoint, 0);
        }
        for (u32 endpoint = 0; endpoint < 2; ++endpoint) m.w32(packet + 8 + 8 * endpoint, m.r32(screens + 4 * e[endpoint]));
        const u32 counts = m.r32(kHudCounts);
        result = m.r32(counts + 24) + 1;
        m.w32(counts + 24, result);
        packet += 20;
    }
    return result;
}

u32 Game::hud_horizontal_marks() {
    cover(0x80064CB4u);
    u32 packet = m.r32(kHudPackets) + 984 + 16 * m.r32(m.r32(kHudCounts) + 20);
    const std::array<u32, 3> colour = {static_cast<u32>(m.r8(kHudSecondaryColor) >> 1),
        static_cast<u32>(m.r8(kHudSecondaryColor + 1) >> 1), static_cast<u32>(m.r8(kHudSecondaryColor + 2) >> 1)};
    constexpr std::array<std::array<u32, 2>, 3> positions = {{{0, 60}, {260, 290}, {314, 320}}};
    u32 count = 0;
    for (const auto& ends : positions) {
        for (u32 component = 0; component < 3; ++component) m.w8(packet + 4 + component, colour[component]);
        const u32 counts = m.r32(kHudCounts);
        m.w16(packet + 8, ends[0]); m.w16(packet + 12, ends[1]);
        m.w16(packet + 14, 120); m.w16(packet + 10, 120);
        count = m.r32(counts + 20) + 1;
        m.w32(counts + 20, count);
        packet += 16;
    }
    return count;
}
}

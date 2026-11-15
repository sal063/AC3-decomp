#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include <algorithm>

namespace ac3::sim {
u32 Game::subdivide_terrain_quad(u32 corners, u32 work, u32 draw, s32 levels) {
    cover(0x80096630u);
    Frame frame(*this, 56);
    constexpr u32 kVertexSize = 20, kProjectionErrors = 0x7E01E000u;
    auto midpoint = [&](u32 index, u32 a, u32 b) {
        mesh_vertex_midpoint(work + kVertexSize * index, m.r32(corners + 4 * a), m.r32(corners + 4 * b));
    };
    midpoint(0, 0, 1);
    gte.mtc2(0, m.r32(work)); gte.mtc2(1, m.r32(work + 4));
    midpoint(1, 0, 2); gte.op(0x180001u);
    midpoint(2, 1, 3); midpoint(3, 2, 3); midpoint(4, 1, 2);
    for (u32 vertex = 0; vertex < 5; ++vertex) {
        const u32 point = work + kVertexSize * vertex;
        m.w32(point + 8, gte.mfc2(14)); m.w16(point + 16, gte.mfc2(19));
        const u32 flags = gte.cfc2(31);
        if (vertex < 4) {
            gte.mtc2(0, m.r32(point + 20)); gte.mtc2(1, m.r32(point + 24)); gte.op(0x180001u);
        }
        m.w16(point + 6, flags & kProjectionErrors ? 0xFFFFu : screen_outcode(point + 8));
    }
    const s32 next_level = wrap::sub(levels, 1);
    for (u32 corner = 0; corner < 4; ++corner) {
        s32 combined = 0, common = -1, farthest = 0, nearest = 32767;
        for (u32 vertex = 0; vertex < 4; ++vertex) {
            const u32 sum = corner + vertex;
            const u32 point = corner == vertex ? m.r32(corners + 4 * vertex) :
                              work + (sum == 3 ? 80 : sum < 3 ? 20 * sum - 20 : 20 * sum - 40);
            m.w32(corners + 16 + 4 * vertex, point);
            const s32 code = m.r16s(point + 6), depth = m.r16s(point + 16);
            common &= code; combined |= code;
            if (farthest < depth) farthest = depth;
            else if (depth < nearest) nearest = depth;
        }
        if (static_cast<u32>(farthest) - 121u >= 0x777Fu || common) continue;
        if (combined >= 0) {
            auto screen = [&](u32 index) { return m.r32(m.r32(corners + 16 + 4 * index) + 8); };
            gte.mtc2(12, screen(3)); gte.mtc2(13, screen(1)); gte.mtc2(14, screen(2));
            const s32 span = farthest - nearest;
            gte.op(0x1400006u);
            const s32 first_area = static_cast<s32>(gte.mfc2(24));
            if (first_area >= 0) continue;
            gte.mtc2(12, screen(0)); gte.op(0x1400006u);
            const s32 second_area = static_cast<s32>(gte.mfc2(24));
            if (second_area <= 0) continue;
            const s32 area = wrap::sub(second_area, first_area);
            if (area >= 1025 && ((wrap::mul(area, span) >> 16) >= 513 ||
                (combined > 0 && quad_exceeds_gpu_span(screen(0), screen(1), screen(2), screen(3))))) combined = -1;
        }
        if (combined < 0 && next_level >= 0) {
            subdivide_terrain_quad(corners + 16, work + 100, draw, next_level);
            continue;
        }
        const u32 packet = m.r32(draw), end = packet + 52;
        m.w32(packet, 0x0C000000u | (end & 0xFFFFFFu)); m.w32(draw, end);
        for (u32 vertex = 0; vertex < 4; ++vertex) {
            const u32 point = m.r32(corners + 16 + 4 * vertex), offset = 4 + 12 * vertex;
            const u32 colour = m.r32(point + 12), screen = m.r32(point + 8);
            m.w32(packet + offset, colour); m.w32(packet + offset + 4, screen);
        }
        m.w16(packet + 14, m.r16(draw + 40)); m.w16(packet + 26, m.r16(draw + 42));
        for (u32 vertex = 0; vertex < 4; ++vertex)
            m.w16(packet + 12 + 12 * vertex, m.r16(m.r32(corners + 16 + 4 * vertex) + 18));
    }
    return 0;
}
}

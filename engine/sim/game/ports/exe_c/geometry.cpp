#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kOutlineStride = 16;
constexpr u32 kHudBuffers = 0x800BD074u;
constexpr u32 kHudCursor = 0x800BD078u;
}

s32 Game::outline_max_x(u32 points, s32 count) {
    cover(0x8005F124u);
    s32 best = 0;
    for (s32 i = 1; i < count; ++i)
        if (m.r32s(points + kOutlineStride * best) < m.r32s(points + kOutlineStride * i)) best = i;
    return best;
}

s32 Game::outline_min_x(u32 points, s32 count) {
    cover(0x8005F174u);
    s32 best = 0;
    for (s32 i = 1; i < count; ++i)
        if (m.r32s(points + kOutlineStride * i) < m.r32s(points + kOutlineStride * best)) best = i;
    return best;
}

u32 Game::outline_same_facing(u32 points, s32 count) {
    cover(0x8005F1C4u);
    Frame f(*this, 0x38);
    const u32 triangle = f.at(0x10);
    m.w32(triangle, 0);
    auto point = [&](u32 at) {
        m.w16(triangle + 8, static_cast<u32>(m.r32s(at) >> 1));
        m.w16(triangle + 10, static_cast<u32>(m.r32s(at + 8) >> 1));
    };
    point(points + kOutlineStride * static_cast<u32>(count) - kOutlineStride);
    s32 positive = 0;
    for (s32 i = 0; i < count; ++i) {
        m.w32(triangle + 4, m.r32(triangle + 8));
        point(points + kOutlineStride * i);
        gte.mtc2(12, m.r32(triangle));
        gte.mtc2(14, m.r32(triangle + 8));
        gte.mtc2(13, m.r32(triangle + 4));
        gte.op(0x1400006);
        if (static_cast<s32>(gte.mfc2(24)) > 0) ++positive;
    }
    return positive == 0 || positive == count;
}

u32 Game::unproject_rotated(u32 point, u32 out, s32 projection) {
    cover(0x8005F734u);
    Frame f(*this, 0x28);
    const s32 depth = m.r16s(point + 4);
    for (u32 axis = 0; axis < 2; ++axis) {
        const s32 product = m.r16s(point + 2 * axis) * depth;
        if (!projection) unported(axis ? 0x8005F7B4u : 0x8005F764u, "unprojection division by zero");
        m.w32(f.at(0x10 + 4 * axis), wrap::div(product, projection).lo);
    }
    m.w32(f.at(0x18), static_cast<u32>(depth));
    return apply_rot_matrix_lv(f.at(0x10), out);
}

u32 Game::project_rotated(u32 point, u32 out) {
    cover(0x8005F7FCu);
    Frame f(*this, 0x38);
    const s32 projection = static_cast<s32>(gte.cfc2(26));
    m.w32(f.at(0x20), static_cast<u32>(static_cast<s32>(gte.cfc2(24)) >> 16));
    m.w32(f.at(0x24), static_cast<u32>(static_cast<s32>(gte.cfc2(25)) >> 16));
    apply_rot_matrix_lv(point, f.at(0x10));
    if (!m.r32(f.at(0x18))) m.w32(f.at(0x18), 1);
    u32 screen = 0;
    for (u32 axis = 0; axis < 2; ++axis) {
        const s32 product = wrap::mul(m.r32s(f.at(0x10 + 4 * axis)), projection);
        const s32 depth = m.r32s(f.at(0x18));
        if (depth == -1 && static_cast<u32>(product) == 0x80000000u)
            unported(axis ? 0x8005F8DCu : 0x8005F888u, "projection division overflow");
        screen = m.r16(f.at(0x20 + 4 * axis)) + wrap::div(product, depth).lo;
        m.w16(out + 2 * axis, screen);
    }
    return screen;
}

u32 Game::hud_buffers_begin() {
    cover(0x80063F80u);
    const u32 buffers = m.r32(kHudBuffers);
    const u32 scene = m.r32(g::kScene);
    for (u32 offset = 36; offset >= 8; offset -= 4) m.w32(buffers + offset, 0);
    const u32 cursor = m.r32(buffers + 4 * m.r32(scene + 4));
    m.w32(kHudCursor, cursor);
    return cursor;
}

u32 Game::morph_vertices_twice(s32 first, s32 second, u32 base, u32 from, u32 to, u32 out, u32 work) {
    cover(0x80066C18u);
    Frame f(*this, 0x28);
    morph_vertices(first, from, to, work);
    return morph_vertices(second, base, work, out);
}

u32 Game::morph_pose_pair(s32 blend, u32 poses, u32 out) {
    cover(0x8006701Cu);
    Frame f(*this, 0x18);
    return morph_vertices(blend, m.r32(poses), m.r32(poses + 4), out);
}

u32 Game::morph_pose_signed(s32 blend, u32 poses, u32 out) {
    cover(0x8006704Cu);
    Frame f(*this, 0x18);
    const u32 target = m.r32(poses + (blend < 0 ? 8u : 4u));
    return morph_vertices(blend < 0 ? wrap::neg(blend) : blend, m.r32(poses), target, out);
}
}

#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include <algorithm>

namespace ac3::sim {
namespace {
u32 packed_outcode(u32 screen) {
    const s32 x = static_cast<s16>(screen), y = static_cast<s16>(screen >> 16);
    return (x < -160 ? 1u : x > 160 ? 2u : 0u) | (y < -120 ? 4u : y > 120 ? 8u : 0u);
}
}

u32 Game::quad_exceeds_gpu_span(u32 a, u32 b, u32 c, u32 d) {
    cover(0x800963F8u);
    const s32 ya = static_cast<s16>(a >> 16), yb = static_cast<s16>(b >> 16);
    const s32 yc = static_cast<s16>(c >> 16), yd = static_cast<s16>(d >> 16);
    if (std::max({ya, yb, yc, yd}) - std::min({ya, yb, yc, yd}) >= 512) return 1;
    const s32 xa = static_cast<s16>(a), xb = static_cast<s16>(b);
    const s32 xc = static_cast<s16>(c), xd = static_cast<s16>(d);
    return std::max({xa, xb, xc, xd}) - std::min({xa, xb, xc, xd}) >= 1024 ? 1u : 0u;
}

u32 Game::mesh_vertex_midpoint(u32 out, u32 a, u32 b) {
    cover(0x80096518u);
    for (u32 axis = 0; axis < 3; ++axis) m.w16(out + 2 * axis, (m.r16s(a + 2 * axis) + m.r16s(b + 2 * axis)) >> 1);
    constexpr u32 kEvenBytes = 0x00FF00FFu, kOddBytes = 0xFF00FF00u;
    const u32 colour_a = m.r32(a + 12), colour_b = m.r32(b + 12);
    const u32 even = (((colour_a & kEvenBytes) + (colour_b & kEvenBytes)) >> 1) & kEvenBytes;
    const u32 odd = (((colour_a & kOddBytes) + (colour_b & kOddBytes)) >> 1) & kOddBytes;
    m.w32(out + 12, even | odd);
    const u32 uv_a = m.r16(a + 18), uv_b = m.r16(b + 18);
    const u32 uv_low = ((uv_a & kEvenBytes) + (uv_b & kEvenBytes)) >> 1;
    const u32 uv_high = (((uv_a & kOddBytes) + (uv_b & kOddBytes)) >> 1) & 0xFF00u;
    m.w16(out + 18, uv_low | uv_high);
    return uv_high;
}

u32 Game::sky_ring_project(u32 out, u32 points, s32 level) {
    cover(0x8009757Cu);
    Frame frame(*this, 48);
    const s32 cosine = rcos(level < 9 ? wrap::mul(100, level) : 862);
    const s32 scale = level < 9 ? wrap::mul(508, level) : 4096;
    const s32 depth = wrap::sub(cosine, cosine >> 2);
    const u32 first = out;
    for (u32 vertex = 0; vertex < 24; ++vertex, out += 16, points += 4) {
        const s32 x = wrap::mul(scale, -m.r16s(points + 2));
        const s32 y = wrap::mul(scale, -m.r16s(points));
        const u32 packed = (static_cast<u32>(x >> 12) & 65535) | (static_cast<u32>(y >> 12) << 16);
        gte.mtc2(0, packed); gte.mtc2(1, static_cast<u32>(depth));
        gte.op(0x180001u);
        m.w16(out + 8, x >> 16); m.w16(out + 10, static_cast<s32>(packed) >> 20);
        if (static_cast<s32>(gte.cfc2(31)) < 0) {
            m.w32(out, 0xFFFFFFFFu);
        } else {
            m.w32(out + 4, gte.mfc2(14));
            gte.mtc2(1, static_cast<u32>(wrap::neg(cosine)));
            gte.op(0x108041Bu);
            m.w32(out, screen_outcode(out + 4));
            m.w32(out + 12, gte.mfc2(22));
        }
    }
    const u32 code = m.r32(first), screen = m.r32(first + 4), uv = m.r32(first + 8), colour = m.r32(first + 12);
    m.w32(out, code); m.w32(out + 4, screen); m.w32(out + 8, uv); m.w32(out + 12, colour);
    return 0;
}

u32 Game::mesh_ring_project(u32 mesh, u32 out, u32 depth) {
    cover(0x8009830Cu);
    Frame frame(*this, 40);
    gte.mtc2(0, 0); gte.mtc2(1, depth); gte.op(0x486012u);
    const s32 z = static_cast<s32>(gte.mfc2(27));
    const u32 x = gte.mfc2(25), y = gte.mfc2(26);
    if (z < -2048) {
        for (s32 vertex = 19; vertex >= 0; --vertex) m.w32(out + 12 * static_cast<u32>(vertex), 0xFFFFFFFFu);
        return 0xFFFFFFFFu;
    }
    gte.ctc2(5, x); gte.ctc2(6, y); gte.ctc2(7, static_cast<u32>(z));
    gte.mtc2(1, 0);
    const u32 first = out;
    for (u32 vertex = 0; vertex < 20; ++vertex, out += 12) {
        gte.mtc2(0, m.r32(mesh + 76 + 4 * vertex)); gte.op(0x180001u);
        if (static_cast<s32>(gte.cfc2(31)) < 0) {
            m.w32(out, 0xFFFFFFFFu);
        } else {
            m.w32(out + 4, gte.mfc2(14)); gte.op(0x780010u);
            m.w32(out, screen_outcode(out + 4)); m.w32(out + 8, gte.mfc2(22));
        }
    }
    const u32 code = m.r32(first), screen = m.r32(first + 4), colour = m.r32(first + 8);
    m.w32(out, code); m.w32(out + 4, screen); m.w32(out + 8, colour);
    return 0;
}

u32 Game::mesh_slice_project(u32 mesh, u32 out, u32 slice) {
    cover(0x80098714u);
    const u32 x = m.r32(mesh + 32), z = m.r32(mesh + 40), y = m.r32(mesh + 36);
    const s32 depth = static_cast<s32>(z + 8192u - (slice << 10));
    gte.mtc2(1, static_cast<u32>(depth));
    const s32 distance = depth < 0 ? wrap::neg(depth) : depth;
    const s32 taper = wrap::sub(4096, wrap::mul(3840, distance) / 8192);
    const u32 screen_y = static_cast<u32>(wrap::mul(static_cast<s32>(y), taper) / 4096) << 16;
    s32 screen_x = wrap::mul(static_cast<s32>(x - 8192u), taper);
    for (u32 vertex = 0; vertex < 16; ++vertex, out += 12) {
        gte.mtc2(0, (static_cast<u32>(screen_x / 4096) & 65535) | screen_y);
        screen_x = wrap::add(screen_x, static_cast<s32>(static_cast<u32>(taper) << 10));
        gte.op(0x180001u);
        if (static_cast<s32>(gte.cfc2(31)) < 0) {
            m.w32(out, 0xFFFFFFFFu);
        } else {
            const u32 screen = gte.mfc2(14);
            gte.op(0x780010u); m.w32(out + 4, screen);
            m.w32(out, packed_outcode(screen)); m.w32(out + 8, gte.mfc2(22));
        }
    }
    m.w32(out, 0xFFFFFFFFu);
    return 0xFFFFFFFFu;
}
}

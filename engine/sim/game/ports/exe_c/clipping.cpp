#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include <utility>

namespace ac3::sim {
u32 Game::clip_screen_line(u32 a, u32 b, s32 halfWidth, s32 halfHeight) {
    cover(0x8005F2B4u);
    auto outside = [&](u32 point) {
        const s32 x = m.r16s(point), y = m.r16s(point + 2);
        const u32 horizontal = x < wrap::neg(halfWidth) ? 1u : x > halfWidth ? 2u : 0u;
        return horizontal | (y < wrap::neg(halfHeight) ? 4u : y > halfHeight ? 8u : 0u);
    };
    u32 codeA = outside(a), codeB = outside(b);
    for (u32 attempt = 0; attempt < 8; ++attempt) {
        if (codeA & codeB) return 0;
        if (!(codeA | codeB)) return 1;
        if (!codeA) {
            std::swap(a, b);
            codeB = 0;
        } else {
            const bool horizontal = (codeA & 3u) != 0;
            const u32 along = horizontal ? 0u : 2u, across = horizontal ? 2u : 0u;
            const s32 extent = horizontal ? halfWidth : halfHeight;
            const s32 edge = (codeA & (horizontal ? 1u : 4u)) ? wrap::neg(extent) : extent;
            const s32 origin = m.r16s(a + along);
            const s32 denominator = m.r16s(b + along) - origin;
            const s32 numerator = wrap::mul(m.r16s(b + across) - m.r16s(a + across), wrap::sub(edge, origin));
            if (!denominator) unported(horizontal ? 0x8005F3F4u : 0x8005F474u, "line clipping division by zero");
            if (denominator == -1 && static_cast<u32>(numerator) == 0x80000000u)
                unported(horizontal ? 0x8005F40Cu : 0x8005F48Cu, "line clipping division overflow");
            const u32 value = m.r16(a + across) + wrap::div(numerator, denominator).lo;
            m.w16(a + along, static_cast<u32>(edge));
            m.w16(a + across, value);
        }
        codeA = outside(a);
    }
    return 1;
}

u32 Game::clip_segment_height(u32 start, u32 delta, s32 height, u32 out) {
    cover(0x8005F510u);
    const s32 startY = m.r32s(start + 4), dy = m.r32s(delta + 4);
    const s32 distance = wrap::sub(height, startY);
    if ((dy > 0 && height < startY) || (dy < 0 && startY < height) || (!dy && height != startY)) return 0;
    s32 z;
    if (dy) {
        auto quotient = [&](s32 value, s32 divisor, bool last) {
            const s32 product = wrap::mul(value, distance);
            if (!divisor) unported(dy > 0 ? 0x8005F5A0u : 0x8005F64Cu, "height clipping division by zero");
            if (divisor == -1 && static_cast<u32>(product) == 0x80000000u)
                unported(dy > 0 ? (last ? 0x8005F5B8u : 0x8005F56Cu) : (last ? 0x8005F664u : 0x8005F618u),
                         "height clipping division overflow");
            return static_cast<s32>(wrap::div(product, divisor).lo);
        };
        m.w32(out, static_cast<u32>(quotient(m.r32s(delta), dy, false)));
        z = quotient(m.r32s(delta + 8), m.r32s(delta + 4), true);
    } else {
        m.w32(out, m.r32(delta));
        z = m.r32s(delta + 8);
    }
    m.w32(out + 4, static_cast<u32>(distance));
    m.w32(out + 8, static_cast<u32>(z));
    for (u32 axis : {0u, 8u}) {
        const s32 endpoint = m.r32s(delta + axis), clipped = m.r32s(out + axis);
        if (endpoint < 0 ? clipped < endpoint : endpoint < clipped) {
            m.w32(out + axis, static_cast<u32>(endpoint));
            m.w32(out + 4, m.r32(delta + 4));
        }
    }
    return m.r32(out + 4) != m.r32(delta + 4);
}

u32 Game::terrain_triangle(u32 a, u32 b, u32 c, u32 work) {
    cover(0x80061ED4u);
    u32 vertex = 0;
    for (u32 point : {a, b, c}) {
        gte.mtc2(2 * vertex, m.r32(point));
        gte.mtc2(2 * vertex + 1, m.r32(point + 4));
        ++vertex;
    }
    const u32 colourA = m.r32(a + 8);
    const u32 packet = m.r32(work + 4);
    gte.op(0x280030);
    Gte::Exact exact[3];
    if (terrain_exact_on_)
        for (u32 i = 0; i < 3; ++i) exact[i] = terrain_exact(gte.exact_fifo[i]);
    const u32 colourB = m.r32(b + 8);
    m.w32(packet + 4, colourA);
    m.w32(packet + 16, colourB);
    const u32 uvC = m.r16(c + 6), colourC = m.r32(c + 8);
    m.w16(packet + 36, uvC);
    m.w32(packet + 28, colourC);
    const u32 uvA = m.r16(a + 6), uvB = m.r16(b + 6);
    m.w16(packet + 12, uvA);
    m.w16(packet + 24, uvB);
    const u32 palette = m.r16(work + 618), page = m.r16(work + 616);
    m.w8(packet + 7, 0x34);
    m.w16(packet + 14, palette);
    m.w16(packet + 26, page);
    const u32 depth = m.r32(work + 612);
    const u32 flags = gte.cfc2(31);
    if (static_cast<s32>(flags) < 0) return flags;
    m.w32(work + 4, m.r32(work + 4) + 40);
    m.w32(packet, m.r32(m.r32(work) + 4 * depth) | 0x09000000u);
    m.w32(m.r32(work) + 4 * depth, packet & 0xFFFFFFu);
    for (u32 i = 0; i < 3; ++i) m.w32(packet + 8 + 12 * i, gte.mfc2(12 + i));
    if (terrain_exact_on_) {
        const Gte::Exact* corners[3] = {&exact[0], &exact[1], &exact[2]};
        terrain_record(packet, corners, 3);
    }
    return packet & 0xFFFFFFu;
}
}

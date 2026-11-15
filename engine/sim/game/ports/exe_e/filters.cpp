#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
void filter_link(Memory& memory, u32 at, u32 next) {
    for (u32 i = 0; i < 3; ++i) memory.w8(at + i, next >> (8 * i));
}
u32 filter_offset(u32 xy) {
    return 0xE5000000u | (xy & 0x7FFu) | (((xy << 5) >> 21) << 11);
}
}

u32 Game::framebuffer_filter(u32 ot, u32 packet, u32 offset) {
    cover(0x8008F950u);
    const u32 page_y = m.r32(m.r32(g::kScene) + 4) == 0 ? 256u : 0u;
    m.w32(packet, ((packet + 8) & 0xFFFFFFu) | 0x01000000u);
    m.w32(packet + 4, 0xE6000001u);
    u32 cursor = packet + 8;
    if (offset) {
        m.w32(cursor, ((packet + 20) & 0xFFFFFFu) | 0x02000000u);
        m.w32(cursor + 4, 0xE5000000u | (((page_y + 256) & 256u) << 11));
        m.w32(cursor + 8, 0);
        cursor += 12;
    }
    for (u32 x = 0; x < 320; x += 256, cursor += 24) {
        m.w32(cursor, ((cursor + 24) & 0xFFFFFFu) | 0x05000000u);
        m.w32(cursor + 4, 0xE1000300u | (page_y >> 4) | (x >> 6));
        m.w32(cursor + 8, 0x66838383u);
        m.w32(cursor + 12, x);
        m.w16(cursor + 16, 0);
        m.w32(cursor + 20, 0x00F00100u);
    }
    u32 tail = cursor - 24;
    if (offset) {
        tail = cursor;
        m.w32(cursor, 0x02000000u);
        m.w32(cursor + 8, 0);
        m.w32(cursor + 4, filter_offset(m.r32(offset)));
        cursor += 12;
    }
    filter_link(m, tail, m.r32(ot));
    filter_link(m, ot, packet);
    return cursor;
}

u32 Game::framebuffer_blend(u32 ot, u32 packet, u32 offset, s32 level) {
    cover(0x8008FAA8u);
    Frame frame(*this, 8);
    const s32 fade = static_cast<s32>(4096u - static_cast<u32>(level));
    for (u32 i = 21; i < 24; ++i) gte.ctc2(i, 0);
    gte.mtc2(6, 0x007F7F7Fu);
    gte.mtc2(8, fade < 0 ? 0u : static_cast<u32>(fade));
    const u32 page_y = m.r32(m.r32(g::kScene) + 4) == 0 ? 256u : 0u;
    gte.op(0x780010u);
    m.w32(packet, ((packet + 8) & 0xFFFFFFu) | 0x01000000u);
    m.w32(packet + 4, 0xE6000001u);
    u32 cursor = packet + 8;
    if (offset) {
        m.w32(cursor, ((packet + 20) & 0xFFFFFFu) | 0x02000000u);
        m.w32(cursor + 4, 0xE5000000u | (((page_y + 256) & 256u) << 11));
        m.w32(cursor + 8, 0);
        cursor += 12;
    }
    const u32 colour = gte.mfc2(22);
    for (u32 x = 0; x < 320; x += 256, cursor += 40) {
        m.w32(cursor, ((cursor + 40) & 0xFFFFFFu) | 0x09000000u);
        const u32 page = (page_y >> 4) | (x >> 6);
        m.w32(cursor + 4, 0xE1000340u | page);
        m.w32(cursor + 20, 0xE1000320u | page);
        m.w32(cursor + 8, colour | 0x62000000u);
        m.w32(cursor + 24, colour | 0x66000000u);
        m.w32(cursor + 28, x);
        m.w32(cursor + 12, x);
        m.w16(cursor + 32, 0);
        const u32 size = 0x00F00000u | (x ? 64u : 256u);
        m.w32(cursor + 36, size);
        m.w32(cursor + 16, size);
    }
    u32 tail = cursor - 40;
    if (offset) {
        tail = cursor;
        m.w32(cursor, 0x02000000u);
        m.w32(cursor + 8, 0);
        m.w32(cursor + 4, filter_offset(m.r32(offset)));
        cursor += 12;
    }
    filter_link(m, tail, m.r32(ot));
    filter_link(m, ot, packet);
    return cursor;
}

u32 Game::draw_view_filters(u32 context) {
    cover(0x8008FFACu);
    Frame frame(*this, 32);
    const u32 scene = m.r32(g::kScene), effects = m.r32(0x800BCF90u);
    u32 mode = static_cast<u32>(m.r16s(scene + 942));
    const u32 pad = m.r16(scene + 96);
    if (mode & 4u) {
        mode &= ~4u;
        m.w32(effects + 1480, m.r32(effects + 1480) | 1u);
        m.w16(scene + 942, mode);
    }
    if (pad & 0x300u) {
        if (pad & 0x200u) mode ^= 1u;
        if (pad & 0x100u) {
            if (mode & 2u) mode |= 4u;
            mode ^= 2u;
        }
        m.w16(m.r32(g::kScene) + 942, mode);
    }
    if (mode & 1u)
        m.w32(context + 4, framebuffer_filter(m.r32(context) - 4, m.r32(context + 4), 0));
    u32 result = mode & 6u;
    if (result) {
        m.w32(effects + 1480, m.r32(effects + 1480) | 1u);
        result = mode & 2u;
        if (result) {
            result = noise_overlay_green(m.r32(context), m.r32(context + 4), 1024);
            m.w32(context + 4, result);
        }
    }
    return result;
}
}

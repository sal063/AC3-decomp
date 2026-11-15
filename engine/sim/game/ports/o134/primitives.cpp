#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
u32 Game::o1_polyline_packet(u32 description, u32 packet, u32 orderingTable, u32 vertices) {
    const u32 rgb = 4 * vertices, terminator = packet + 8 + 4 * vertices;
    m.w8(packet + 3, vertices + 2); m.w8(packet + 7, vertices == 3 ? 0x48 : 0x4C);
    m.w32(terminator, 0x55555555u);
    const u32 command = m.r8(packet + 7);
    m.w8(packet + 7, m.r8(description + rgb + 3) ? command | 2u : command & ~2u);
    const u32 red = m.r8(description + rgb), header = m.r32(packet);
    m.w8(packet + 4, red);
    m.w8(packet + 5, m.r8(description + rgb + 1));
    m.w8(packet + 6, m.r8(description + rgb + 2));
    for (u32 i = 0; i < 2 * vertices; ++i) m.w16(packet + 8 + 2 * i, m.r16(description + 2 * i));
    m.w32(packet, (header & 0xFF000000u) | (m.r32(orderingTable) & 0x00FFFFFFu));
    m.w32(orderingTable, (m.r32(orderingTable) & 0xFF000000u) | (packet & 0x00FFFFFFu));
    return terminator + 4;
}
u32 Game::o1_polyline3(u32 description, u32 packet, u32 orderingTable) {
    cover(0x800D9140u);
    return o1_polyline_packet(description, packet, orderingTable, 3);
}
u32 Game::o1_polyline4(u32 description, u32 packet, u32 orderingTable) {
    cover(0x800D9294u);
    return o1_polyline_packet(description, packet, orderingTable, 4);
}
u32 Game::o1_double_line(u32 description, u32 packet, u32 orderingTable) {
    cover(0x800D90E8u);
    Frame frame(*this, 32);
    const u32 next = o1_line_packet(description, packet, orderingTable);
    const u32 y0 = m.r16(description + 2), y1 = m.r16(description + 6);
    m.w16(description + 2, y0 + 1); m.w16(description + 6, y1 + 1);
    return o1_line_packet(description, next, orderingTable);
}
u32 Game::o1_double_polyline3(u32 description, u32 packet, u32 orderingTable) {
    cover(0x800D9230u);
    Frame frame(*this, 32);
    const u32 next = o1_polyline3(description, packet, orderingTable);
    const u32 y0 = m.r16(description + 2), y2 = m.r16(description + 10);
    m.w16(description + 2, y0 + 1);
    const u32 y1 = m.r16(description + 6);
    m.w16(description + 10, y2 + 1); m.w16(description + 6, y1 + 1);
    return o1_polyline3(description, next, orderingTable);
}
u32 Game::o1_double_polyline4(u32 description, u32 packet, u32 orderingTable) {
    cover(0x800D939Cu);
    Frame frame(*this, 32);
    const u32 next = o1_polyline4(description, packet, orderingTable);
    const u32 y0 = m.r16(description + 2), y2 = m.r16(description + 10);
    m.w16(description + 2, y0 + 1);
    const u32 y1 = m.r16(description + 6);
    m.w16(description + 10, y2 + 1); m.w16(description + 6, y1 + 1);
    return o1_polyline4(description, next, orderingTable);
}
u32 Game::o1_tile_packet(u32 description, u32 packet, u32 orderingTable) {
    cover(0x800D9400u);
    m.w8(packet + 3, 3); m.w8(packet + 7, 0x60);
    m.w8(packet + 7, m.r8(description + 11) ? 0x62 : 0x60);
    for (u32 i = 0; i < 3; ++i) m.w8(packet + 4 + i, m.r8(description + 8 + i));
    for (u32 i = 0; i < 3; ++i) m.w16(packet + 8 + 2 * i, m.r16(description + 2 * i));
    const u32 height = m.r16(description + 6), header = m.r32(packet);
    m.w16(packet + 14, height);
    m.w32(packet, (header & 0xFF000000u) | (m.r32(orderingTable) & 0x00FFFFFFu));
    m.w32(orderingTable, (m.r32(orderingTable) & 0xFF000000u) | (packet & 0x00FFFFFFu));
    return packet + 16;
}
u32 Game::o1_glyph_frame_begin(u32 orderingTable) {
    cover(0x800D81F0u);
    Frame frame(*this, 32);
    const u32 scene = m.r32(g::kScene), prims = m.r32(0x800DDA58u);
    m.w32(prims + 8, m.r32(prims + 4 * m.r32(scene + 4)));
    const u32 tag = m.r32(prims + 20 + 4 * m.r32(scene + 4));
    m.w32(prims + 28, tag);
    clear_otag_r(tag, 1);
    const u32 current = m.r32(0x800DDA58u), selectedTag = m.r32(current + 28);
    m.w32(selectedTag, (m.r32(selectedTag) & 0xFF000000u) | (m.r32(orderingTable) & 0x00FFFFFFu));
    const u32 result = m.r32(current + 28) & 0x00FFFFFFu;
    m.w32(orderingTable, (m.r32(orderingTable) & 0xFF000000u) | result);
    return result;
}
u32 Game::o1_card_matching_slot() {
    cover(0x800D5DB4u);
    Frame frame(*this, 40);
    for (u32 slot = 0; slot < 3; ++slot) {
        const u32 cards = m.r32(0x800DDA48u);
        if (!(m.r8(m.r32(cards + 744) + 8) & (1u << slot))) continue;
        const u32 save = m.r32(cards + 748 + 4 * slot);
        if (libc_strcmp(save + 8, m.r32(g::kScene) + 2376) != 0) continue;
        const u32 current = m.r32(m.r32(0x800DDA48u) + 748 + 4 * slot);
        if (m.r32(current + 4) == m.r32(m.r32(g::kScene) + 2408)) return slot;
    }
    return 0xFFFFFFFFu;
}
}

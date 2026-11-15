#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kPacketAddress = 0x00FFFFFFu;
constexpr u32 kPacketLength = 0xFF000000u;
constexpr u32 kLineBytes = 16;
constexpr u32 kLineColour = 8;
constexpr u32 kLineBlend = 11;
}

u32 Game::o2_line_flat(u32 line, u32 packet, u32 ot) {
    cover(0x800F6DA4u);
    m.w8(packet + 3, 3);
    m.w8(packet + 7, 0x40);
    m.w8(packet + 7, m.r8(line + kLineBlend) ? 0x42 : 0x40);
    for (u32 component = 0; component < 3; ++component)
        m.w8(packet + 4 + component, m.r8(line + kLineColour + component));
    for (u32 coordinate = 0; coordinate < 4; ++coordinate)
        m.w16(packet + 8 + 2 * coordinate, m.r16(line + 2 * coordinate));
    m.w32(packet, (m.r32(packet) & kPacketLength) | (m.r32(ot) & kPacketAddress));
    m.w32(ot, (m.r32(ot) & kPacketLength) | (packet & kPacketAddress));
    return packet + kLineBytes;
}

u32 Game::o2_line_double(u32 line, u32 packet, u32 ot) {
    cover(0x800F6E68u);
    Frame frame(*this, 32);
    packet = o2_line_flat(line, packet, ot);
    m.w16(line + 2, m.r16(line + 2) + 1);
    m.w16(line + 6, m.r16(line + 6) + 1);
    return o2_line_flat(line, packet, ot);
}

u32 Game::o2_draw_mode_packet(u32 mode, u32 packet, u32 ot) {
    cover(0x800F6CBCu);
    Frame frame(*this, 40);
    const u32 dither = m.r8(mode), display = m.r8(mode + 1);
    const u32 depth = m.r8(mode + 2), blend = m.r8(mode + 3);
    const u32 y = m.r16(mode + 6), x = m.r16(mode + 4);
    const u32 page = ((depth & 3) << 7) | ((blend & 3) << 5)
        | ((y & 0x100) >> 4) | ((x & 0x3FF) >> 6) | ((y & 0x200) << 2);
    m.w32(frame.at(16), 0);
    set_draw_mode(packet, dither, display, page, 0);
    m.w32(packet, (m.r32(packet) & kPacketLength) | (m.r32(ot) & kPacketAddress));
    m.w32(ot, (m.r32(ot) & kPacketLength) | (packet & kPacketAddress));
    return packet + 12;
}

u32 Game::o2_rectangle_flat(u32 rectangle, u32 packet, u32 ot) {
    cover(0x800F7180u);
    m.w8(packet + 3, 3);
    m.w8(packet + 7, 0x60);
    m.w8(packet + 7, m.r8(rectangle + kLineBlend) ? 0x62 : 0x60);
    for (u32 component = 0; component < 3; ++component)
        m.w8(packet + 4 + component, m.r8(rectangle + kLineColour + component));
    for (u32 coordinate = 0; coordinate < 4; ++coordinate)
        m.w16(packet + 8 + 2 * coordinate, m.r16(rectangle + 2 * coordinate));
    m.w32(packet, (m.r32(packet) & kPacketLength) | (m.r32(ot) & kPacketAddress));
    m.w32(ot, (m.r32(ot) & kPacketLength) | (packet & kPacketAddress));
    return packet + 16;
}

u32 Game::o2_triangle_gouraud(u32 triangle, u32 packet, u32 ot) {
    cover(0x800F73A4u);
    m.w8(packet + 3, 6);
    m.w8(packet + 7, 0x30);
    m.w8(packet + 7, m.r8(triangle + 19) ? 0x32 : 0x30);
    for (u32 vertex = 0; vertex < 3; ++vertex)
        for (u32 colour = 0; colour < 3; ++colour)
            m.w8(packet + 4 + 8 * vertex + colour, m.r8(triangle + 16 + 4 * vertex + colour));
    for (u32 vertex = 0; vertex < 3; ++vertex) {
        m.w16(packet + 8 + 8 * vertex, m.r16(triangle + 4 * vertex));
        m.w16(packet + 10 + 8 * vertex, m.r16(triangle + 2 + 4 * vertex));
    }
    m.w32(packet, (m.r32(packet) & kPacketLength) | (m.r32(ot) & kPacketAddress));
    m.w32(ot, (m.r32(ot) & kPacketLength) | (packet & kPacketAddress));
    return packet + 28;
}

u32 Game::o2_menu_draw_area(u32 packet, u32 viewport) {
    cover(0x800F6774u);
    Frame frame(*this, 40);
    const u32 rectangle = frame.at(16);
    if (viewport == 0 || viewport == 1) {
        const bool preview = viewport == 1;
        m.w16(rectangle, preview ? 20 : 0);
        m.w16(rectangle + 2, preview ? 70 : 0);
        m.w16(rectangle + 4, preview ? 128 : 320);
        m.w16(rectangle + 6, preview ? 240 : 480);
    }
    set_draw_area(packet, rectangle);
    const u32 menu = m.r32(0x80112DC4u);
    const u32 table = m.r32(menu + 8);
    m.w32(packet, (m.r32(packet) & kPacketLength) | (m.r32(table) & kPacketAddress));
    const u32 currentTable = m.r32(menu + 8);
    m.w32(currentTable, (m.r32(currentTable) & kPacketLength) | (packet & kPacketAddress));
    return packet + 12;
}
}

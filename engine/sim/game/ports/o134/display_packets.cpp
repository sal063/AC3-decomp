#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kState = 0x800DD9D0u, kScreens = 0x800DD9DCu;
}
u32 Game::o1_gouraud_packet(u32 description, u32 packet, u32 table, u32 vertices) {
    const u32 command = vertices == 4 ? 0x38 : 0x30, words = 2 * vertices;
    m.w8(packet + 3, words); m.w8(packet + 7, command);
    m.w8(packet + 7, m.r8(description + 19) ? command | 2u : command);
    for (u32 vertex = 0; vertex < vertices; ++vertex)
        for (u32 channel = 0; channel < 3; ++channel)
            m.w8(packet + 4 + 8 * vertex + channel, m.r8(description + 16 + 4 * vertex + channel));
    for (u32 vertex = 0; vertex < vertices; ++vertex) {
        m.w16(packet + 8 + 8 * vertex, m.r16(description + 4 * vertex));
        m.w16(packet + 10 + 8 * vertex, m.r16(description + 4 * vertex + 2));
    }
    m.w32(packet, (m.r32(packet) & 0xFF000000u) | (m.r32(table) & 0xFFFFFFu));
    m.w32(table, (m.r32(table) & 0xFF000000u) | (packet & 0xFFFFFFu));
    return packet + 4 + 8 * vertices;
}
u32 Game::o1_gouraud_quad(u32 description, u32 packet, u32 table) {
    cover(0x800D94C4u);
    return o1_gouraud_packet(description, packet, table, 4);
}
u32 Game::o1_gouraud_triangle(u32 description, u32 packet, u32 table) {
    cover(0x800D9624u);
    return o1_gouraud_packet(description, packet, table, 3);
}
u32 Game::o1_textured_quad(u32 description, u32 packet, u32 table) {
    cover(0x800D9748u);
    m.w8(packet + 3, 9); m.w8(packet + 7, 0x2C);
    m.w8(packet + 7, m.r8(description + 19) ? 0x2E : 0x2C);
    for (u32 channel = 0; channel < 3; ++channel) m.w8(packet + 4 + channel, m.r8(description + 16 + channel));
    for (u32 vertex = 0; vertex < 4; ++vertex) {
        m.w16(packet + 8 + 8 * vertex, m.r16(description + 4 * vertex));
        m.w16(packet + 10 + 8 * vertex, m.r16(description + 4 * vertex + 2));
    }
    auto u = [&]() { return static_cast<u32>(m.r16s(description + 32) % 64) * 4; };
    auto v = [&]() { return static_cast<u32>(m.r16s(description + 34) % 256); };
    m.w8(packet + 12, u()); m.w8(packet + 13, v());
    m.w8(packet + 20, u() + m.r8(description + 36)); m.w8(packet + 21, v());
    m.w8(packet + 28, u()); m.w8(packet + 29, v() + m.r8(description + 38));
    m.w8(packet + 36, u() + m.r8(description + 36)); m.w8(packet + 37, v() + m.r8(description + 38));
    const u32 y = m.r16(description + 34), x = m.r16(description + 32);
    const u32 page = ((m.r8(description + 23) & 3u) << 5) | ((y & 256u) >> 4)
                   | ((x & 1023u) >> 6) | ((y & 512u) << 2);
    m.w16(packet + 22, page);
    const u32 paletteY = m.r16(description + 42), paletteX = m.r16(description + 40);
    m.w16(packet + 14, (paletteY << 6) | ((paletteX >> 4) & 63u));
    m.w32(packet, (m.r32(packet) & 0xFF000000u) | (m.r32(table) & 0xFFFFFFu));
    m.w32(table, (m.r32(table) & 0xFF000000u) | (packet & 0xFFFFFFu));
    return packet + 40;
}
u32 Game::o1_colour_bars() {
    cover(0x800DBB30u);
    Frame frame(*this, 88);
    const u32 description = frame.at(16);
    constexpr u32 left[] = {90, 102, 113, 129};
    constexpr u32 colour[][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}};
    u32 next = 0;
    for (u32 bar = 0; bar < 4; ++bar) {
        for (u32 vertex = 0; vertex < 4; ++vertex) {
            m.w16(description + 4 * vertex, left[bar] + (vertex & 1u ? 5 : 0));
            m.w16(description + 4 * vertex + 2, vertex < 2 ? 213 : 381);
        }
        if (!bar) {
            m.w8(description + 19, 0);
            for (u32 channel = 0; channel < 3; ++channel) {
                m.w8(description + 16 + channel, 0); m.w8(description + 20 + channel, 0);
            }
        }
        for (u32 channel = 0; channel < 3; ++channel) {
            m.w8(description + 24 + channel, colour[bar][channel]);
            m.w8(description + 28 + channel, colour[bar][channel]);
        }
        next = o1_gouraud_quad(description, m.r32(m.r32(kState) + 8), m.r32(m.r32(kScreens) + 16));
        m.w32(m.r32(kState) + 8, next);
    }
    return next;
}
u32 Game::o1_display_position_choice() {
    cover(0x800CFA94u);
    Frame frame(*this, 24);
    const u32 buttons = m.r32(m.r32(g::kScene) + 300);
    if (buttons & 16u) {
        const u32 state = m.r32(kState), display = m.r32(0x800BCE48u);
        m.w16(state + 76, 5); m.w16(state + 78, 0);
        m.w16(state + 220, m.r16(display + 8)); m.w16(state + 222, m.r16(display + 12)); sound_play(89);
    } else if (buttons & 32u) {
        const u32 state = m.r32(kState);
        m.w16(state + 76, 4); m.w16(state + 78, 0); sound_play(59);
    }
    return o1_colour_bars();
}
u32 Game::o1_pilot_name(u32 x, u32 y, u32 slot) {
    cover(0x800DBD20u);
    Frame frame(*this, 56);
    t1_prim_mode(4); o1_text_style(2); o1_text_fold_set(1, 30, 40);
    o1_text_fold_position(t1_fade_value(15)); o1_text_rgb_set(128, 128, 128);
    o1_text_origin_set(static_cast<u32>(static_cast<s16>(x)), static_cast<u32>(static_cast<s16>(y)));
    t1_printf(o1_save_name_address(slot));
    if (!(m.r32(m.r32(kScreens) + 84) & 4u)) return 0;
    const u32 description = frame.at(16), pen = frame.at(32);
    m.w8(description + 11, 0);
    for (u32 channel = 0; channel < 3; ++channel) m.w8(description + 8 + channel, 40);
    o1_text_origin_get(pen, pen + 2);
    const u32 px = m.r16(pen), py = m.r16(pen + 2);
    const u32 segments[][4] = {{px, py, px + 8, py}, {px, py + 9, px + 8, py + 9},
        {px, py + 18, px + 8, py + 18}, {px, py, px, py + 18},
        {px + 4, py, px + 4, py + 18}, {px + 8, py, px + 8, py + 18}};
    u32 next = 0;
    for (const auto& segment : segments) {
        for (u32 coordinate = 0; coordinate < 4; ++coordinate) m.w16(description + 2 * coordinate, segment[coordinate]);
        next = o1_double_line(description, m.r32(m.r32(kState) + 8), m.r32(m.r32(kScreens) + 16));
        m.w32(m.r32(kState) + 8, next);
    }
    return next;
}
u32 Game::o1_new_pilot_draw() {
    cover(0x800DBFE4u);
    Frame frame(*this, 24);
    const u32 screens = m.r32(kScreens);
    if (!(m.r32(screens + 84) & 1u)) return 0;
    return o1_pilot_name(static_cast<u32>(m.r16s(screens + 80)), static_cast<u32>(m.r16s(screens + 82)), 0);
}
}

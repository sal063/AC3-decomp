#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace { constexpr u32 kKanji = 0x800DDA50u; }
u32 Game::o1_kanji_setup(u32 paletteX, u32 paletteY) {
    cover(0x800D7644u);
    Frame frame(*this, 88);
    m.w32(kKanji, mem_alloc(12));
    const u32 palette = (paletteY << 6) | ((static_cast<u32>(static_cast<s32>(paletteX) >> 4)) & 63u);
    for (u32 field = 0; field < 2; ++field) {
        const u32 packets = mem_alloc(4000), state = m.r32(kKanji);
        m.w32(state + 4 * field, packets); m.w32(state + 8, packets);
        for (u32 glyph = 0; glyph < 100; ++glyph) {
            m.w8(m.r32(m.r32(kKanji) + 8) + 3, 9);
            m.w8(m.r32(m.r32(kKanji) + 8) + 7, 0x2C);
            const u32 packet = m.r32(m.r32(kKanji) + 8);
            m.w8(packet + 7, m.r8(packet + 7) & 0xFDu);
            for (u32 channel = 0; channel < 3; ++channel) m.w8(m.r32(m.r32(kKanji) + 8) + 4 + channel, 128);
            const u32 current = m.r32(kKanji);
            m.w16(m.r32(current + 8) + 22, 29); m.w16(m.r32(current + 8) + 14, palette);
            m.w32(current + 8, m.r32(current + 8) + 40);
        }
    }
    const u32 colours = frame.at(16), rect = frame.at(48);
    m.w16(rect, paletteX); m.w16(rect + 2, paletteY); m.w16(rect + 4, 16); m.w16(rect + 6, 1);
    m.w16(colours, 0); m.w16(colours + 2, 0x7FFF);
    for (u32 i = 2; i < 16; ++i) m.w16(colours + 2 * i, 0x8000);
    hw->load_image(rect, colours);
    return 0;
}
u32 Game::o1_kanji_upload(u32 text) {
    cover(0x800D7868u);
    Frame frame(*this, 168);
    const u32 pixels = frame.at(16), rect = frame.at(144);
    m.w32(rect, m.r32(0x800CC568u)); m.w32(rect + 4, m.r32(0x800CC56Cu));
    while (m.r8(text)) {
        const u32 source = o1_glyph_rom_address(text);
        if (source == 0xFFFFFFFFu) {
            for (u32 i = 0; i < 128; ++i) m.w8(pixels + i, 0);
        } else {
            for (u32 row = 0; row < 15; ++row) {
                u32 destination = pixels + row * 8;
                for (u32 half = 0; half < 2; ++half)
                    for (s32 bit = 7 + 8 * half; bit >= static_cast<s32>(8 * half); --bit) {
                        const u32 value = (m.r16(source + 2 * row) >> bit) & 1u;
                        if (bit & 1) m.w8(destination, value);
                        else { if (value) m.w8(destination, m.r8(destination) | 16u); ++destination; }
                    }
            }
            for (u32 i = 120; i < 128; ++i) m.w8(pixels + i, 0);
        }
        hw->load_image(rect, pixels);
        const u32 nextX = (m.r16(rect) + 4) & 65535u;
        m.w16(rect, nextX);
        if (!(nextX & 63u)) { m.w16(rect, nextX - 64); m.w16(rect + 2, m.r16(rect + 2) + 16); }
        text += 2;
    }
    return 0;
}
u32 Game::o1_kanji_strip(u32 x, u32 y, u32 first, u32 count, u32 table) {
    cover(0x800D7A58u);
    const s32 start = static_cast<s16>(first), length = static_cast<s16>(count);
    for (s32 i = 0; i < length; ++i) {
        const s32 cell = start + i, row = cell / 16 * 16;
        const u32 u = static_cast<u32>(cell - row) * 16, v = static_cast<u32>(row);
        auto writeUV = [&](u32 offset, u32 value) { m.w8(m.r32(m.r32(kKanji) + 8) + offset, value); };
        writeUV(12, u); writeUV(13, v); writeUV(20, u + 15); writeUV(21, v);
        writeUV(28, u); writeUV(29, v + 15); writeUV(36, u + 15); writeUV(37, v + 15);
        const u32 state = m.r32(kKanji), left = x + 16 * static_cast<u32>(i);
        for (u32 vertex = 0; vertex < 4; ++vertex) {
            m.w16(m.r32(state + 8) + 8 + 8 * vertex, left + (vertex & 1u ? 16u : 0u));
            m.w16(m.r32(state + 8) + 10 + 8 * vertex, y + (vertex < 2 ? 0u : 32u));
        }
        const u32 packet = m.r32(state + 8), prior = m.r32(table), tag = m.r32(packet);
        m.w32(packet, (tag & 0xFF000000u) | (prior & 0xFFFFFFu));
        m.w32(table, (m.r32(table) & 0xFF000000u) | (m.r32(state + 8) & 0xFFFFFFu));
        m.w32(state + 8, m.r32(state + 8) + 40);
    }
    return 0;
}
}

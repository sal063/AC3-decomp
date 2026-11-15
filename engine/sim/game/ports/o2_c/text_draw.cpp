#include "core/wrap.hpp"
#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kText = 0x80112E00u;
constexpr u32 kQuadCursor = 0x5DC0, kColour = 0xA8CC, kLength = 0xA8D0;
constexpr u32 kOrderingTable = 0xA8DC, kPosition = 0xA8E4, kPen = 0xA8E8;
constexpr u32 kPage = 0xA8EC, kClut = 0xA8EE, kColumns = 0xA8F0, kScaleY = 0xA8F2;
constexpr u32 kTextureU = 0xA8F8, kTextureV = 0xA8FA;
constexpr u32 kWidth = 0xA900, kHeight = 0xA901, kFirstGlyph = 0xA902;
constexpr u32 kVariableWidth = 0xA903, kWidths = 0xA904, kBlend = 0xA908;
constexpr u32 kSpace = 0xA90A, kFold = 0xA90B, kFoldStart = 0xA90C;
constexpr u32 kFoldEnd = 0xA90E, kFrame = 0xA910;
u32 xy(s32 x, s32 y) { return (static_cast<u32>(x) & 0xFFFFu) | (static_cast<u32>(y) << 16); }
}

u32 Game::o2_text_glyph(u32 character) {
    cover(0x800F4124u);
    const u32 text = m.r32(kText);
    const u32 glyph = (character - m.r8(text + kFirstGlyph)) & 255;
    const s32 columns = m.r16s(text + kColumns);
    if (columns == 0) unported(0x800F4150u, "zero font columns");
    const auto cell = wrap::div(static_cast<s32>(glyph), columns);
    const s32 width = m.r8(text + kWidth), height = m.r8(text + kHeight);
    const s32 u = static_cast<s16>(m.r16(text + kTextureU) + wrap::mul(static_cast<s32>(cell.hi), width));
    const s32 v = static_cast<s16>(m.r16(text + kTextureV) + wrap::mul(static_cast<s32>(cell.lo), height));
    const u32 packet = m.r32(text + kQuadCursor);
    m.w32(packet + 12, static_cast<u32>(u) | (static_cast<u32>(v) << 8)
                       | (static_cast<u32>(m.r16s(text + kClut)) << 16));
    m.w32(packet + 20, static_cast<u32>(u - 1 + m.r8(text + kWidth)) | (static_cast<u32>(v) << 8)
                       | (static_cast<u32>(m.r16s(text + kPage)) << 16));
    m.w32(packet + 28, static_cast<u32>(u) | (static_cast<u32>(v - 1 + m.r8(text + kHeight)) << 8));
    m.w32(packet + 36, static_cast<u32>(u - 1 + m.r8(text + kWidth))
                       | (static_cast<u32>(v - 1 + m.r8(text + kHeight)) << 8));
    m.w32(packet + 4, m.r8(text + kColour) | (m.r8(text + kColour + 1) << 8)
                       | (m.r8(text + kColour + 2) << 16));

    s32 topOffset = 0;
    s32 bottomOffset = wrap::mul(m.r8(text + kHeight) - 1, m.r16s(text + kScaleY));
    if (m.r8(text + kFold) == 1) {
        const s32 time = m.r16s(text + kFrame), start = m.r16s(text + kFoldStart);
        if (time < start) return 0;
        const s32 end = m.r16s(text + kFoldEnd);
        if (time < end) {
            const s32 halfHeight = wrap::mul(m.r8(text + kHeight), m.r16s(text + kScaleY)) / 2;
            topOffset = wrap::mul(halfHeight, end - time) / (end - start);
            bottomOffset = wrap::add(topOffset,
                wrap::mul(bottomOffset, time - start) / (end - start));
        }
    }
    m.w32(packet + 8, xy(m.r16(text + kPosition), wrap::add(m.r16s(text + kPosition + 2), topOffset)));
    m.w32(packet + 16, xy(m.r16s(text + kPosition) - 1 + m.r8(text + kWidth),
                         wrap::add(m.r16s(text + kPosition + 2), topOffset)));
    m.w32(packet + 24, xy(m.r16(text + kPosition), wrap::add(m.r16s(text + kPosition + 2), bottomOffset)));
    m.w32(packet + 32, xy(m.r16s(text + kPosition) - 1 + m.r8(text + kWidth),
                         wrap::add(m.r16s(text + kPosition + 2), bottomOffset)));
    m.w8(packet + 3, 9);
    m.w8(packet + 7, 0x2C);
    m.w8(packet + 7, m.r8(m.r32(kText) + kBlend) ? 0x2E : 0x2C);
    const u32 table = m.r32(m.r32(kText) + kOrderingTable);
    m.w32(packet, m.r32(table) | 0x09000000u);
    m.w32(m.r32(m.r32(kText) + kOrderingTable), packet & 0x00FFFFFFu);

    const u32 current = m.r32(kText);
    const u32 advance = m.r8(current + kVariableWidth)
        ? m.r8(m.r32(current + kWidths) + glyph) : m.r8(current + kWidth) + 1;
    m.w16(current + kPosition, m.r16(current + kPosition) + advance);
    const u32 block = m.r32(kText);
    const u32 next = m.r32(block + kQuadCursor) + 40;
    m.w32(block + kQuadCursor, next);
    return next;
}

u32 Game::o2_text_string(u32 x, u32 y, u32 string) {
    cover(0x800F4928u);
    Frame frame(*this, 40);
    (void)x; (void)y;
    if (m.r32s(m.r32(kText) + kLength) <= 0) return m.r32(m.r32(kText) + kLength);
    for (s32 index = 0; index < m.r32s(m.r32(kText) + kLength); ++index) {
        const u32 character = m.r8(string + static_cast<u32>(index));
        const u32 text = m.r32(kText);
        if (character == '\n') {
            const s32 step = wrap::mul(m.r8(text + kHeight), m.r16s(text + kScaleY));
            m.w16(text + kPosition, m.r16(text + kPen));
            m.w16(text + kPosition + 2, m.r16(text + kPosition + 2) + static_cast<u32>(step));
        } else if (character == '\t') {
            m.w16(text + kPosition, m.r16(text + kPen));
        } else if (character == ' ') {
            const u32 step = m.r8(text + kVariableWidth) ? m.r8(text + kSpace) : m.r8(text + kWidth) + 1;
            m.w16(text + kPosition, m.r16(text + kPosition) + step);
        } else {
            o2_text_glyph(character);
        }
    }
    return 0;
}

u32 Game::o2_text_format(u32 format, u32 first, u32 second, u32 third, bool zeroSpaces) {
    m.w32(sp, format);
    m.w32(sp + 4, first);
    m.w32(sp + 8, second);
    m.w32(sp + 12, third);
    Frame frame(*this, 0x218);
    const u32 buffer = frame.at(16);
    const u32 count = vsprintf(buffer, format, frame.at(0x21C));
    m.w32(m.r32(kText) + kLength, count);
    if (zeroSpaces)
        for (s32 i = 0; i < m.r32s(m.r32(kText) + kLength); ++i)
            if (m.r8(buffer + static_cast<u32>(i)) == ' ') m.w8(buffer + static_cast<u32>(i), '0');
    const u32 text = m.r32(kText);
    return o2_text_string(static_cast<u32>(m.r16s(text + kPosition)),
                           static_cast<u32>(m.r16s(text + kPosition + 2)), buffer);
}

u32 Game::o2_text_printf(u32 format, u32 first, u32 second, u32 third) {
    cover(0x800F4AE0u);
    return o2_text_format(format, first, second, third, false);
}

u32 Game::o2_text_printf_zero(u32 format, u32 first, u32 second, u32 third) {
    cover(0x800F4B40u);
    return o2_text_format(format, first, second, third, true);
}
u32 Game::o2_text_sprite_glyph(u32 character) {
    cover(0x800F3F2Cu);
    constexpr u32 kSpriteCursor = 0x8CA4;
    const u32 text = m.r32(kText), glyph = (character - m.r8(text + kFirstGlyph)) & 255u;
    const s32 columns = m.r16s(text + kColumns);
    if (!columns) unported(0x800F3F5Cu, "zero font columns");
    const auto cell = wrap::div(static_cast<s32>(glyph), columns);
    const u32 packet = m.r32(text + kSpriteCursor);
    m.w8(packet + 12, m.r16(text + kTextureU) + static_cast<u32>(wrap::mul(cell.hi, m.r8(text + kWidth))));
    m.w8(packet + 13, m.r16(text + kTextureV) + static_cast<u32>(wrap::mul(cell.lo, m.r8(text + kHeight))));
    u32 current = m.r32(kText);
    m.w32(packet + 16, m.r8(current + kWidth) | (m.r8(current + kHeight) << 16));
    m.w32(packet + 8, m.r16(current + kPosition) | (static_cast<u32>(m.r16s(current + kPosition + 2)) << 16));
    m.w16(packet + 14, m.r16(current + kClut));
    for (u32 channel = 0; channel < 3; ++channel)
        m.w8(packet + 4 + channel, m.r8(m.r32(kText) + kColour + channel));
    const u32 command = m.r8(packet + 7);
    m.w8(packet + 7, m.r8(m.r32(kText) + kBlend) ? command | 2u : command & ~2u);
    current = m.r32(kText);
    m.w32(packet, m.r32(m.r32(current + kOrderingTable)) | 0x04000000u);
    m.w32(m.r32(current + kOrderingTable), packet & 0xFFFFFFu);
    current = m.r32(kText);
    const u32 advance = m.r8(current + kVariableWidth)
        ? m.r8(m.r32(current + kWidths) + glyph) : m.r8(current + kWidth) + 1u;
    m.w16(current + kPosition, m.r16(current + kPosition) + advance);
    current = m.r32(kText);
    const u32 next = m.r32(current + kSpriteCursor) + 20;
    m.w32(current + kSpriteCursor, next);
    return next;
}

u32 Game::o2_text_sprite_string(u32 source) {
    cover(0x800F46E4u);
    Frame frame(*this, 48);
    constexpr u32 kModeCursor = 0xA8C8;
    auto drawMode = [&](u32 texturePage) {
        m.w32(frame.at(16), 0);
        set_draw_mode(m.r32(m.r32(kText) + kModeCursor), 0, 0, texturePage, 0);
        const u32 text = m.r32(kText), packet = m.r32(text + kModeCursor);
        u32 ot = m.r32(text + kOrderingTable);
        m.w32(packet, (m.r32(packet) & 0xFF000000u) | (m.r32(ot) & 0xFFFFFFu));
        ot = m.r32(text + kOrderingTable);
        m.w32(ot, (m.r32(ot) & 0xFF000000u) | (m.r32(text + kModeCursor) & 0xFFFFFFu));
        const u32 next = m.r32(text + kModeCursor) + 12;
        m.w32(text + kModeCursor, next);
        return next;
    };
    drawMode(0);
    for (s32 index = 0; index < m.r32s(m.r32(kText) + kLength); ++index) {
        const u32 character = m.r8(source + static_cast<u32>(index)), text = m.r32(kText);
        if (character == '\n') {
            const s32 step = wrap::mul(m.r8(text + kHeight), m.r16s(text + kScaleY));
            m.w16(text + kPosition, m.r16(text + kPen));
            m.w16(text + kPosition + 2, m.r16(text + kPosition + 2) + static_cast<u32>(step));
        } else if (character == '\t') m.w16(text + kPosition, m.r16(text + kPen));
        else if (character == ' ') {
            const u32 step = m.r8(text + kVariableWidth) ? m.r8(text + kSpace) : m.r8(text + kWidth) + 1u;
            m.w16(text + kPosition, m.r16(text + kPosition) + step);
        } else o2_text_sprite_glyph(character);
    }
    return drawMode(static_cast<u32>(m.r16s(m.r32(kText) + kPage)));
}

u32 Game::o2_text_sprite_label(u32 source) {
    cover(0x800F4A78u);
    Frame frame(*this, 40);
    const u32 text = m.r32(kText);
    const u32 length = libc_strlen(source);
    m.w32(text + kLength, length);
    return o2_text_sprite_string(source);
}
}

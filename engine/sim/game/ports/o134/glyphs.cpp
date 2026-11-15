#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"
#include <limits>

namespace ac3::sim {
namespace {
constexpr u32 kPrims = 0x800DDA58u;
constexpr u32 kPacket = 8, kPenX = 32, kPenY = 34, kColumns = 44, kLineScale = 46;
constexpr u32 kPageX = 48, kPageY = 50, kWidth = 56, kHeight = 57, kFirstGlyph = 58;
constexpr u32 kProportional = 59, kWidths = 60, kFoldEnabled = 67;
constexpr u32 kFoldStart = 68, kFoldEnd = 70, kFoldTime = 72;
s32 psxDivide(s32 numerator, s32 denominator) {
    if (!denominator) return numerator < 0 ? 1 : -1;
    if (numerator == std::numeric_limits<s32>::min() && denominator == -1) return numerator;
    return numerator / denominator;
}
}
u32 Game::o1_glyph(u32 character) {
    cover(0x800D82A4u);
    auto state = [&]() { return m.r32(kPrims); };
    auto packet = [&]() { return m.r32(state() + kPacket); };
    const u32 initial = state(), glyph = (character - m.r8(initial + kFirstGlyph)) & 255u;
    const s32 columns = m.r16s(initial + kColumns);
    const s32 row = psxDivide(static_cast<s32>(glyph), columns);
    const s32 column = columns ? static_cast<s32>(glyph) % columns : static_cast<s32>(glyph);
    if (!columns) unported(0x800D82CCu, "a break in glyph column division");
    const u32 u = static_cast<u32>(column) * m.r8(initial + kWidth)
                + 4 * static_cast<u32>(m.r16s(initial + kPageX) % 64);
    const u32 v = static_cast<u32>(row) * m.r8(initial + kHeight)
                + static_cast<u32>(m.r16s(initial + kPageY) % 256);
    m.w8(packet() + 12, u); m.w8(packet() + 13, v);
    m.w8(packet() + 20, m.r8(state() + kWidth) + u - 1); m.w8(packet() + 21, v);
    m.w8(packet() + 28, u); m.w8(packet() + 29, m.r8(state() + kHeight) + v - 1);
    m.w8(packet() + 36, m.r8(state() + kWidth) + u - 1);
    m.w8(packet() + 37, m.r8(state() + kHeight) + v - 1);
    const u32 blendPacket = packet();
    m.w8(blendPacket + 7, m.r8(state() + 64) ? m.r8(blendPacket + 7) | 2u : m.r8(blendPacket + 7) & 0xFDu);
    m.w8(packet() + 4, m.r8(state() + 12));
    m.w8(packet() + 5, m.r8(state() + 13));
    m.w8(packet() + 6, m.r8(state() + 14));
    const u32 current = state(), pageY = m.r16(current + kPageY), pageX = m.r16(current + kPageX);
    const u32 page = ((m.r8(current + 65) & 3u) << 5) | ((pageY & 256u) >> 4)
                   | ((pageX & 1023u) >> 6) | ((pageY & 512u) << 2);
    m.w16(m.r32(current + kPacket) + 22, page);
    m.w16(m.r32(current + kPacket) + 14, m.r16(current + 42));
    bool folding = false;
    if (m.r8(current + kFoldEnabled) == 1) {
        const s32 time = m.r16s(current + kFoldTime);
        if (time < m.r16s(current + kFoldStart)) return 0;
        folding = time < m.r16s(current + kFoldEnd);
    }
    auto top = [&]() -> u32 {
        if (!folding) return m.r16(current + kPenY);
        const s32 height = static_cast<s32>(m.r8(current + kHeight)) * m.r16s(current + kLineScale);
        const s32 half = (height + static_cast<s32>(static_cast<u32>(height) >> 31)) >> 1;
        const s32 end = m.r16s(current + kFoldEnd), time = m.r16s(current + kFoldTime);
        const s32 start = m.r16s(current + kFoldStart);
        const s32 offset = psxDivide(half * (end - time), end - start);
        return m.r16(current + kPenY) + static_cast<u32>(offset);
    };
    auto bottom = [&]() -> u32 {
        if (!folding) return m.r16(current + kPenY)
            + (m.r8(current + kHeight) - 1u) * static_cast<u32>(m.r16s(current + kLineScale));
        const s32 offsetTop = static_cast<s32>(top() - m.r16(current + kPenY));
        const s32 height = (static_cast<s32>(m.r8(current + kHeight)) - 1) * m.r16s(current + kLineScale);
        const s32 time = m.r16s(current + kFoldTime), start = m.r16s(current + kFoldStart);
        const s32 end = m.r16s(current + kFoldEnd);
        const s32 offsetBottom = psxDivide(height * (time - start), end - start);
        return m.r16(current + kPenY) + static_cast<u32>(offsetTop) + static_cast<u32>(offsetBottom);
    };
    auto position = [&](u32 offset, bool right, bool lower) {
        const u32 x = m.r16(current + kPenX) + (right ? m.r8(current + kWidth) - 1u : 0u);
        m.w16(m.r32(current + kPacket) + offset, x);
        m.w16(m.r32(current + kPacket) + offset + 2, lower ? bottom() : top());
    };
    position(8, false, false); position(16, true, false);
    position(24, false, true); position(32, true, true);
    const u32 linkedState = state(), quad = m.r32(linkedState + kPacket);
    const u32 table = m.r32(linkedState + 28);
    m.w32(quad, (m.r32(quad) & 0xFF000000u) | (m.r32(table) & 0xFFFFFFu));
    const u32 tableAfter = m.r32(linkedState + 28), quadAfter = m.r32(linkedState + kPacket);
    m.w32(tableAfter, (m.r32(tableAfter) & 0xFF000000u) | (quadAfter & 0xFFFFFFu));
    const u32 width = m.r8(linkedState + kProportional)
        ? m.r8(m.r32(linkedState + kWidths) + glyph) : m.r8(linkedState + kWidth);
    m.w16(linkedState + kPenX, m.r16(linkedState + kPenX) + width);
    const u32 after = state(), next = m.r32(after + kPacket) + 40;
    m.w32(after + kPacket, next);
    return next;
}
u32 Game::o1_screens_frame_begin(u32 orderingTable) {
    cover(0x800DC9ECu);
    Frame frame(*this, 32);
    constexpr u32 kScreens = 0x800DD9DCu;
    const u32 scene = m.r32(g::kScene), screens = m.r32(kScreens);
    m.w32(screens + 16, m.r32(screens + 20 + 4 * m.r32(scene + 4)));
    const u32 table = m.r32(screens + 16);
    m.w32(screens + 4, m.r32(screens + 8 + 4 * m.r32(scene + 4)));
    clear_otag_r(table, 16);
    const u32 current = m.r32(kScreens), own = m.r32(current + 16);
    m.w32(own, (m.r32(own) & 0xFF000000u) | (m.r32(orderingTable) & 0xFFFFFFu));
    const u32 last = (m.r32(current + 16) + 60) & 0xFFFFFFu;
    m.w32(orderingTable, (m.r32(orderingTable) & 0xFF000000u) | last);
    return last;
}
}

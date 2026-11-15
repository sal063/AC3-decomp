#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
u32 Game::effect_letterbox(u32 ot, u32 packet, s32 level) {
    cover(0x800939D4u);
    if (level <= 0) return packet;
    const s32 easing = level < 4096 ? m.r16s(0x800B7104u + 4 * static_cast<u32>(level >> 2)) : 4096;
    const s32 top = 32 * easing >> 12, bottom = 48 * easing >> 12;
    const u32 page_y = m.r32(m.r32(g::kScene) + 4) ? 256 : 0;
    m.w32(packet, 0x02000000u);
    m.w32(packet + 4, 0xE3000000u | (((page_y + top) & 1023) << 10));
    m.w32(packet + 8, 0xE400013Fu | (((page_y + 239 - bottom) & 1023) << 10));
    m.w32(packet, m.r32(ot + 16380) | 0x02000000u);
    m.w32(ot + 16380, packet & 0x00FFFFFFu);
    m.w32(packet + 12, ((packet + 24) & 0x00FFFFFFu) | 0x02000000u);
    m.w32(packet + 16, 0xE3000000u | (page_y << 10));
    m.w32(packet + 20, 0xE400013Fu | ((page_y + 239) << 10));
    m.w32(packet + 24, 0x06000000u);
    m.w32(packet + 28, 0x60000000u); m.w32(packet + 40, 0x60000000u);
    m.w16(packet + 44, 0); m.w16(packet + 32, 0); m.w16(packet + 34, 0);
    m.w16(packet + 46, 240 - bottom);
    m.w32(packet + 36, (static_cast<u32>(top) << 16) | 320);
    m.w32(packet + 48, (static_cast<u32>(bottom) << 16) | 320);
    const u32 link = static_cast<u32>(m.r8(ot + 4)) | (static_cast<u32>(m.r8(ot + 5)) << 8) | (static_cast<u32>(m.r8(ot + 6)) << 16);
    for (u32 byte = 0; byte < 3; ++byte) m.w8(packet + 24 + byte, link >> (8 * byte));
    for (u32 byte = 0; byte < 3; ++byte) m.w8(ot + 4 + byte, (packet + 12) >> (8 * byte));
    return packet + 52;
}

u32 Game::effect_flash_tile(u32 ot, u32 packet, u32 rect, s32 level) {
    cover(0x80093B2Cu);
    if (!level) return packet;
    const u32 mode = level < 0 ? 2 : 1;
    if (level < 0) level = wrap::neg(level);
    for (u32 channel = 21; channel < 24; ++channel) gte.ctc2(channel, 0);
    gte.mtc2(6, 0x62FFFFFFu);
    gte.mtc2(8, 4096u - static_cast<u32>(level));
    m.w32(packet, m.r32(ot) | 0x04000000u);
    m.w32(ot, packet & 0x00FFFFFFu);
    gte.op(0x780010u);
    m.w32(packet + 4, 0xE1000200u | (mode << 5));
    const u32 xy = m.r32(rect), wh = m.r32(rect + 4);
    m.w32(packet + 12, xy); m.w32(packet + 16, wh);
    m.w32(packet + 8, gte.mfc2(22));
    return packet + 20;
}

u32 Game::effect_masked_flash(u32 ot, u32 packet, u32 additive, s32 fade) {
    cover(0x80093BC4u);
    Frame frame(*this, 24);
    if (fade >= 4096) return packet;
    s32 level = wrap::sub(4096, fade);
    m.w32(g::kScratch, 0);
    m.w32(g::kScratch + 4, 0x00F00140u);
    if (!additive) level = wrap::neg(level);
    m.w32(m.r32(0x800BCF90u) + 1480, 1);
    const u32 tail = effect_flash_tile(ot, packet, g::kScratch, level);
    m.w32(tail, 0x01000000u); m.w32(tail + 4, 0xE6000001u);
    m.w32(tail, (m.r32(ot) & 0x00FFFFFFu) | 0x01000000u);
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (tail & 0x00FFFFFFu));
    return tail + 8;
}

u32 Game::effect_red_flash(u32 ot, u32 packet, s32 level) {
    cover(0x80093C90u);
    if (!level) return packet;
    gte.ctc2(21, 48); gte.ctc2(22, 0); gte.ctc2(23, 0);
    gte.mtc2(6, 0x6203135Fu);
    gte.mtc2(8, 4096u - static_cast<u32>(level));
    gte.op(0x780010u);
    m.w32(packet, m.r32(ot) | 0x03000000u);
    m.w32(ot, packet & 0x00FFFFFFu);
    m.w32(packet + 8, 0xFF88FF60u); m.w32(packet + 12, 0x00F00140u);
    m.w32(packet + 4, gte.mfc2(22));
    return packet + 16;
}
}

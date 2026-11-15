#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kWarnBlocks = 0x800BCFC4u;
constexpr u32 kWarnPrims = 0x800BCFC8u;
constexpr u32 kHold = 0x800BCFD4u;
constexpr u32 kHudColor = 0x800C6D68u;
constexpr u32 kPanel = 0x800C7170u;
constexpr u32 kMessages = 0x800BCFECu;
constexpr u32 kPrompt = 0x800BCFD8u;
constexpr u32 kMessagePrims = 0x800BCFE8u;
}

u32 Game::hud_warn_begin() {
    cover(0x800414A4u);
    const u32 blocks = m.r32(kWarnBlocks);
    const u32 buffer = m.r32(m.r32(g::kScene) + 4);
    m.w32(blocks + 12, 0);
    m.w32(blocks + 8, 0);
    const u32 block = m.r32(blocks + 4 * buffer);
    m.w32(kWarnPrims, block);
    return block;
}

void Game::hud_warn_link() {
    cover(0x800414D8u);
    const u32 b = m.r32(kWarnPrims);
    const u32 ot = m.r32(m.r32(g::kWorld) + 16);
    ot_link(ot, b);
    if (!m.r8(kHold)) return;
    for (u32 off : {20u, 40u, 72u, 88u, 56u, 96u}) ot_link(ot, b + off);
}

void Game::radio_hold_draw(s32 slide) {
    cover(0x80041794u);
    const u32 b = m.r32(kWarnPrims);
    m.w8(b + 4, m.r8(kHudColor + 3));
    m.w8(b + 5, m.r8(kHudColor + 4));
    m.w16(b + 8, static_cast<u32>(271 - slide));
    m.w16(b + 10, 14);
    m.w8(b + 6, m.r8(kHudColor + 5));
    m.w8(b + 12, slide ? 120u : 0xAAu);
    const u32 cap = m.r32(kWarnPrims) + 20;
    m.w8(cap + 4, m.r8(kHudColor + 3));
    m.w8(cap + 5, m.r8(kHudColor + 4));
    m.w16(cap + 8, static_cast<u32>(320 - slide));
    m.w16(cap + 10, 14);
    m.w8(cap + 6, m.r8(kHudColor + 5));
    const s32 width = slide >= 15 ? slide - 14 : 0;
    const u32 top = m.r32(kWarnPrims) + 40;
    m.w16(top + 8, static_cast<u32>(334 - slide));
    m.w16(top + 10, 14);
    m.w16(top + 12, static_cast<u32>(width));
    m.w16(top + 14, 3);
    m.w8(top + 4, m.r8(kHudColor));
    m.w8(top + 5, m.r8(kHudColor + 1));
    m.w8(top + 6, m.r8(kHudColor + 2));
    const u32 body = m.r32(kWarnPrims) + 56;
    m.w8(body + 4, 0);
    m.w8(body + 5, 0);
    m.w8(body + 6, 0);
    const u32 b2 = m.r32(kWarnPrims);
    m.w16(body + 8, static_cast<u32>(329 - slide));
    m.w16(body + 10, 17);
    m.w16(body + 12, static_cast<u32>(width + 5));
    m.w16(body + 14, 34);
    m.w16(b2 + 80, static_cast<u32>(334 - slide));
    m.w16(b2 + 82, 51);
    m.w16(b2 + 84, static_cast<u32>(width));
    m.w16(b2 + 86, 3);
    m.w8(b2 + 76, m.r8(kHudColor));
    m.w8(b2 + 77, m.r8(kHudColor + 1));
    m.w8(b2 + 78, m.r8(kHudColor + 2));
}

void Game::subtitle_panel_draw() {
    cover(0x80041ADCu);
    if (!m.r8(kPanel + 2)) return;
    s32 inner_w = static_cast<s32>(m.r8(kPanel + 6)) - 11;
    s32 inner_h = static_cast<s32>(m.r8(kPanel + 7)) - 5;
    const s32 x = static_cast<s32>(m.r16(kPanel + 8)) + (43 - static_cast<s32>(m.r8(kPanel + 6))) / 2;
    const s32 tx = x + 6;
    const s32 y = static_cast<s32>(m.r16(kPanel + 10)) + (41 - static_cast<s32>(m.r8(kPanel + 7))) / 2;
    const s32 ty = y + 3;
    if (inner_w <= 0) inner_w = 1;
    if (inner_h <= 0) inner_h = 1;
    const u32 frame = m.r32(kWarnPrims) + 528;
    m.w8(frame + 4, m.r8(kHudColor + 3));
    m.w8(frame + 5, m.r8(kHudColor + 4));
    m.w16(frame + 8, static_cast<u32>(x));
    m.w16(frame + 10, static_cast<u32>(y));
    m.w8(frame + 6, m.r8(kHudColor + 5));
    m.w16(frame + 18, static_cast<u32>(y));
    m.w16(frame + 24, static_cast<u32>(x));
    m.w16(frame + 16, static_cast<u32>(x) + m.r8(kPanel + 6));
    m.w16(frame + 26, static_cast<u32>(y) + m.r8(kPanel + 7));
    m.w16(frame + 32, static_cast<u32>(x) + m.r8(kPanel + 6));
    const s32 tx1 = inner_w + x + 5;
    m.w16(frame + 34, static_cast<u32>(y) + m.r8(kPanel + 7));
    const u32 text = m.r32(kWarnPrims) + 488;
    const s32 ty1 = inner_h + y + 2;
    const u32 line = m.r8(kPanel + 3);
    m.w16(text + 8, static_cast<u32>(tx));
    m.w16(text + 10, static_cast<u32>(ty));
    m.w16(text + 16, static_cast<u32>(tx1));
    m.w16(text + 18, static_cast<u32>(ty));
    m.w16(text + 24, static_cast<u32>(tx));
    m.w16(text + 26, static_cast<u32>(ty1));
    m.w16(text + 32, static_cast<u32>(tx1));
    m.w16(text + 34, static_cast<u32>(ty1));
    m.w16(text + 14, (line << 6) + 12822u);
    const u32 u = 32u * (m.r8(kPanel + 3) & 7u);
    const u32 v = 36u * (m.r8(kPanel + 3) >> 3);
    m.w8(text + 12, u);
    m.w8(text + 13, v + 0x80u);
    m.w8(text + 20, u + 31u);
    m.w8(text + 21, v + 0x80u);
    m.w8(text + 28, u);
    m.w8(text + 29, v - 93u);
    m.w8(text + 36, u + 31u);
    m.w8(text + 37, v - 93u);
    s16 body_w = static_cast<s16>(inner_w - 2);
    if (body_w <= 0) body_w = 1;
    s16 body_h = static_cast<s16>(inner_h - 3);
    if (body_h <= 0) body_h = 1;
    const s32 bx = x + 7;
    const u32 b = m.r32(kWarnPrims);
    const u32 w = m.r32(g::kWorld);
    const u32 body = b + 568;
    m.w16(b + 576, static_cast<u32>(bx));
    m.w16(body + 10, static_cast<u32>(y + 4));
    m.w16(body + 12, static_cast<u32>(body_w));
    m.w16(body + 14, static_cast<u32>(body_h));
    m.w16(b + 592, static_cast<u32>(bx));
    const s32 scan1 = wrap::div(static_cast<s32>(4u * m.r32(w + 12)), body_h).hi;
    m.w16(b + 596, static_cast<u32>(body_w));
    m.w16(b + 598, 1);
    m.w16(b + 594, static_cast<u32>(ty + scan1));
    m.w16(b + 608, static_cast<u32>(bx));
    const s32 scan2 = wrap::div(static_cast<s32>(5u * m.r32(w + 12)), body_h).hi;
    m.w16(b + 614, 1);
    m.w16(b + 612, static_cast<u32>(body_w));
    m.w16(b + 610, static_cast<u32>(ty + scan2));
    const u32 ot = m.r32(w + 16);
    for (u32 off : {528u, 488u, 584u, 600u, 616u, 568u, 624u}) ot_link(ot, b + off);
}

u32 Game::messages_begin() {
    cover(0x8004C824u);
    const u32 ms = m.r32(kMessages);
    const u32 buffer = m.r32(m.r32(g::kScene) + 4);
    m.w32(ms + 268, 0);
    m.w32(ms + 264, 0);
    const u32 block = m.r32(ms + 4 * buffer);
    m.w32(kMessagePrims, block);
    return block;
}

void Game::messages_link() {
    cover(0x8004C86Cu);
    const u32 ot = m.r32(m.r32(g::kWorld) + 16);
    const u32 block = m.r32(kMessagePrims);
    for (s32 i = 0; i < m.r32s(m.r32(kMessages) + 264); ++i) ot_link(ot, block + 640 + 40 * static_cast<u32>(i));
    for (s32 i = 0; i < m.r32s(m.r32(kMessages) + 268); ++i) ot_link(ot, block + 20 * static_cast<u32>(i));
    ot_link(ot, m.r32(kMessagePrims) + 1920);
}

u32 Game::message_shown() {
    cover(0x8004C858u);
    return m.r32(m.r32(kMessages) + 284);
}

u32 Game::prompt_blink() {
    cover(0x800428F8u);
    Frame f(*this, 24);
    return alert_blink(kPrompt);
}
}

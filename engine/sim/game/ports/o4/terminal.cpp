#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kState = 0x800D1544u;
constexpr u32 kColumns = 44, kRows = 25;
constexpr u32 kText = 4, kCursorX = 0x450, kCursorY = 0x451, kCursorLook = 0x452, kBlink = 0x453;
constexpr u32 kDelay = 0x460, kCommand = 0x462;
constexpr u32 kPurgeLine = 0x800D1520u;
constexpr u32 kPurgeLines = 0x800D0FC4u;
constexpr u32 kNewline = 0x800CD690u;
constexpr u32 kEspecial = 0x800CD694u, kSpecial = 0x800CD6A4u, kParameter = 0x800CD6B4u,
              kSubsystem = 0x800CD6C0u, kPurged = 0x800CD6CCu;
constexpr u32 kSpinner = 0x800CD6D4u;
constexpr u32 kStreamRequest = 0x800D14F8u;
constexpr u32 kKeySound = 0x3F;
constexpr u32 kPlayerName = 0x948;
constexpr u32 kGlyphCode = 0x66000000u, kGlyphClut = 0x18580000u, kGlyphSize = 0x00100006u;
}

u32 Game::libc_strcat(u32 d, u32 s) {
    u32 n = 0;
    while (m.r8(d + n)) ++n;
    for (u32 i = 0;; ++i) {
        const u32 c = m.r8(s + i);
        m.w8(d + n + i, c);
        if (!c) break;
    }
    return d;
}

u32 Game::o4_text_clear() {
    cover(0x800CDEFCu);
    const u32 st = m.r32(kState);
    m.w8(st + kCursorY, 0);
    m.w8(st + kCursorX, 0);
    const u32 text = m.r32(kState) + kText;
    for (u32 k = 0; k < kColumns * kRows / 4; ++k) m.w32(text + 4 * k, 0);
    return 0;
}

u32 Game::o4_text_scroll() {
    cover(0x800CDF38u);
    const u32 text = m.r32(kState) + kText;
    const u32 kept = kColumns * (kRows - 1) / 4;
    for (u32 k = 0; k < kept; ++k) m.w32(text + 4 * k, m.r32(text + kColumns + 4 * k));
    for (u32 k = 0; k < kColumns / 4; ++k) m.w32(text + 4 * (kept + k), 0);
    return 0;
}

u32 Game::o4_text_newline() {
    cover(0x800CDF88u);
    Frame f(*this, 24);
    m.w8(m.r32(kState) + kCursorX, 0);
    const u32 st = m.r32(kState);
    const u32 row = m.r8(st + kCursorY);
    if (row >= kRows - 1) return o4_text_scroll();
    m.w8(st + kCursorY, row + 1);
    return row + 1;
}

u32 Game::o4_text_return() {
    cover(0x800CDFDCu);
    m.w8(m.r32(kState) + kCursorX, 0);
    return m.r32(kState);
}

u32 Game::o4_cursor_back() {
    cover(0x800CDFECu);
    const u32 st = m.r32(kState);
    const u32 col = m.r8(st + kCursorX);
    if (col) {
        m.w8(st + kCursorX, col - 1);
        return col - 1;
    }
    m.w8(st + kCursorX, kColumns - 1);
    const u32 st2 = m.r32(kState);
    const u32 row = m.r8(st2 + kCursorY);
    if (row) m.w8(st2 + kCursorY, row - 1);
    else m.w8(st2 + kCursorX, 0);
    return row - 1;
}

u32 Game::o4_text_backspace() {
    cover(0x800CE040u);
    Frame f(*this, 24);
    o4_cursor_back();
    o4_put_char(' ');
    return o4_cursor_back();
}

u32 Game::o4_text_locate(s32 col, s32 row) {
    cover(0x800CE070u);
    if (col < 0) col = 0;
    if (col >= static_cast<s32>(kColumns)) col = kColumns - 1;
    if (row < 0) row = 0;
    if (row >= static_cast<s32>(kRows)) row = kRows - 1;
    m.w8(m.r32(kState) + kCursorX, static_cast<u32>(col));
    m.w8(m.r32(kState) + kCursorY, static_cast<u32>(row));
    return m.r32(kState);
}

u32 Game::o4_text_control(u32 c) {
    cover(0x800CE0C0u);
    Frame f(*this, 24);
    switch (c) {
    case 1: m.w8(m.r32(kState) + kCursorLook, 1); return 1;
    case 2: m.w8(m.r32(kState) + kCursorLook, 2); return 2;
    case 8: return o4_text_backspace();
    case 10: return o4_text_newline();
    case 13: return o4_text_return();
    default: return c - 1 < 13 ? 0x800CE144u : 0;
    }
}

u32 Game::o4_cell_put(u32 c) {
    const u32 st = m.r32(kState);
    m.w8(st + kText + kColumns * m.r8(st + kCursorY) + m.r8(st + kCursorX), c);
    const u32 st2 = m.r32(kState);
    const u32 col = m.r8(st2 + kCursorX) + 1;
    m.w8(st2 + kCursorX, col);
    if ((col & 0xFFu) >= kColumns) return o4_text_newline();
    return 1;
}

u32 Game::o4_put_char(u32 c) {
    cover(0x800CE154u);
    Frame f(*this, 32);
    if (static_cast<s32>(c) < 32) {
        const u32 result = o4_text_control(c);
        if (!c) return result;
    } else {
        o4_cell_put(c);
        m.w8(m.r32(kState) + kBlink, 0);
    }
    return voice_for_repeat(kKeySound);
}

u32 Game::o4_print_text(u32 s, u32 result) {
    for (u32 c = m.r8(s++); c; c = m.r8(s++)) {
        if (c < 32) result = o4_text_control(c);
        else result = o4_cell_put(c);
    }
    return result;
}

u32 Game::o4_print(u32 s, u32 incoming) {
    cover(0x800CE218u);
    Frame f(*this, 32);
    return o4_print_text(s, incoming);
}

u32 Game::o4_purge_line(u32 i) {
    cover(0x800CE2E4u);
    Frame f(*this, 0x120);
    const u32 line = f.at(0x10);
    m.w8(line, m.r8(kNewline));
    m.w8(line + 1, m.r8(kNewline + 1));
    memset(line + 2, 0, 254);
    libc_strcat(line, m.r32(kPurgeLines + 4 * i));
    if (i < 10) libc_strcat(line, kEspecial);
    if (i - 10 < 73) libc_strcat(line, kSpecial);
    if (i - 83 < 21) libc_strcat(line, kParameter);
    if (static_cast<s32>(i) >= 104) libc_strcat(line, kSubsystem);
    libc_strcat(line, kPurged);
    o4_print_text(line);
    return m.r32(kPurgeLines + 4 * (i + 1)) == 0 ? 1u : 0u;
}

u32 Game::o4_name_char(u32 cmd) {
    cover(0x800CE498u);
    Frame f(*this, 24);
    const u32 c = m.r8(m.r32(g::kScene) + kPlayerName + m.r8(cmd + 3));
    if (c < 32) return 1;
    o4_cell_put(c);
    m.w8(m.r32(kState) + kBlink, 0);
    return voice_for_repeat(kKeySound);
}

u32 Game::o4_draw_text(u32 ot, u32 p, s32 x0, s32 y0, u32 color) {
    cover(0x800CE554u);
    Frame f(*this, 0x28);
    auto glyph = [&](u32 c, s32 x, s32 y) {
        m.w32(p + 4, color | kGlyphCode);
        m.w32(p + 12, ((c & 0xFu) << 3) | (static_cast<u32>(static_cast<s32>(c) >> 4) << 12) | kGlyphClut);
        m.w32(p + 16, kGlyphSize);
        m.w16(p + 8, static_cast<u32>(x));
        m.w16(p + 10, static_cast<u32>(y));
        m.w32(p, m.r32(ot) | 0x04000000u);
        m.w32(ot, p & 0x00FFFFFFu);
        p += 20;
    };
    u32 text = m.r32(kState) + kText;
    s32 y = y0;
    do {
        s32 x = x0;
        do {
            const u32 c = m.r8(text++);
            if (c > 32) glyph(c - 32, x, y);
            x += 6;
        } while (x < x0 + 6 * static_cast<s32>(kColumns));
        y += 16;
    } while (y < y0 + 16 * static_cast<s32>(kRows));
    const u32 st = m.r32(kState);
    m.w8(st + kBlink, m.r8(st + kBlink) + 1);
    const u32 look = m.r8(m.r32(kState) + kCursorLook);
    if (look) {
        const u32 s2 = m.r32(kState);
        u32 c = ' ';
        if (look == 1) c = (m.r8(s2 + kBlink) & 0x10u) ? ' ' : 0x7F;
        else if (look == 2) c = m.r32(kSpinner + ((m.r8(s2 + kBlink) >> 1) & 0xCu));
        const u32 s3 = m.r32(kState);
        const s32 cursor_y = y0 + static_cast<s32>(m.r8(s3 + kCursorY) << 4);
        const s32 cursor_x = x0 + static_cast<s32>(6 * m.r8(s3 + kCursorX));
        if (c != ' ') glyph(c - 32, cursor_x, cursor_y);
    }
    m.w8(p + 3, 1);
    m.w32(p + 4, 0xE1000006u);
    m.w32(p, (m.r32(p) & 0xFF000000u) | (m.r32(ot) & 0x00FFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (p & 0x00FFFFFFu));
    return p + 8;
}

u32 Game::o4_script_step(u32 script, u32 texts) {
    cover(0x800CE808u);
    Frame f(*this, 32);
    const u32 st = m.r32(kState);
    if (m.r16s(st + kDelay)) {
        const u32 left = m.r16(st + kDelay) - 1;
        m.w16(st + kDelay, left);
        if (left & 0xFFFFu) return 0;
    }
    u32 cmd = script + 4 * static_cast<u32>(m.r16s(m.r32(kState) + kCommand));
    auto next_command = [&] {
        const u32 s = m.r32(kState);
        m.w16(s + kCommand, m.r16(s + kCommand) + 1);
        cmd += 4;
    };
    u32 kind = m.r8(cmd);
    if (kind < 6) {
        switch (kind) {
        case 0: return 1;
        case 1: o4_put_char(m.r8(cmd + 2)); break;
        case 2: o4_print(m.r32(texts + 4 * m.r8(cmd + 3))); break;
        case 3: o4_name_char(cmd); break;
        case 4:
            if (o4_purge_line(m.r32(kPurgeLine))) {
                next_command();
            } else {
                const u32 second = m.r32(kPurgeLine) + 1;
                m.w32(kPurgeLine, second);
                if (o4_purge_line(second)) next_command();
                else m.w32(kPurgeLine, m.r32(kPurgeLine) + 1);
            }
            break;
        case 5:
            cd_reset(0, 0);
            cd_request_stream(kStreamRequest, 0, 0);
            break;
        }
        kind = m.r8(cmd);
    }
    if (kind != 4) next_command();
    m.w16(m.r32(kState) + kDelay, m.r8(cmd + 1));
    return 0;
}
}

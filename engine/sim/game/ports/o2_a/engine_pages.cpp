#include "core/settings.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

#include <algorithm>

namespace ac3::sim {
namespace {
constexpr u32 kDialog = 0x80112DDCu, kMenu = 0x80112DC4u, kText = 0x80112E00u;
constexpr u32 kHeld = 296, kPressed = 300, kRepeat = 304;
constexpr u32 kQuadCursor = 0x5DC0, kPosition = 0xA8E4, kWidth = 0xA900, kFirstGlyph = 0xA902;
constexpr u32 kVariableWidth = 0xA903, kWidths = 0xA904, kSpace = 0xA90A;
constexpr u32 kSmall = 7;
constexpr u32 kRowsShown = 9;
constexpr u32 kBlack = 0x000000u, kGrey = 0xB0B0B0u, kOrange = 0xF08818u, kDim = 0x603008u;
constexpr s32 kBorderX = 2, kBorderY = 4;

u32 dialog(Memory& m) { return m.r32(kDialog); }
u32 scene(Memory& m) { return m.r32(g::kScene); }
}

void Game::o2x_rect(s32 x, s32 y, s32 w, s32 h, u32 rgb) {
    if (w <= 0 || h <= 0) return;
    const u32 d = dialog(m), packet = m.r32(d + 8), ot = m.r32(m.r32(kMenu) + 8);
    m.w8(packet + 3, 3);
    m.w32(packet + 4, ((rgb >> 16) & 0xFFu) | (rgb & 0xFF00u) | ((rgb & 0xFFu) << 16) | 0x60000000u);
    m.w16(packet + 8, static_cast<u32>(x));
    m.w16(packet + 10, static_cast<u32>(y));
    m.w16(packet + 12, static_cast<u32>(w));
    m.w16(packet + 14, static_cast<u32>(h));
    m.w32(packet, (m.r32(packet) & 0xFF000000u) | (m.r32(ot) & 0x00FFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (packet & 0x00FFFFFFu));
    m.w32(d + 8, packet + 16);
}

void Game::o2x_box(s32 x, s32 y, s32 w, s32 h, u32 border, u32 fill) {
    const s32 fx = x + kBorderX, fy = y + kBorderY, fw = w - 2 * kBorderX, fh = h - 2 * kBorderY;
    o2x_rect(fx + 1, fy, fw - 2, fh, fill);
    o2x_rect(fx, fy + 2, fw, fh - 4, fill);
    o2x_rect(x + 2, y, w - 4, h, border);
    o2x_rect(x, y + 4, w, h - 8, border);
}

s32 Game::o2x_text_width(const char* s, u32 font) {
    o2_text_font(font);
    const u32 t = m.r32(kText);
    const bool variable = m.r8(t + kVariableWidth) != 0;
    s32 w = 0;
    for (; *s; ++s) {
        if (*s == ' ') {
            w += variable ? m.r8(t + kSpace) : m.r8(t + kWidth) + 1;
            continue;
        }
        const u32 glyph = (static_cast<u8>(*s) - m.r8(t + kFirstGlyph)) & 255u;
        w += variable ? m.r8(m.r32(t + kWidths) + glyph) : m.r8(t + kWidth) + 1;
    }
    return w;
}

void Game::o2x_text(s32 x, s32 y, const char* s, u32 rgb, u32 font, s32 scale) {
    o2_text_font(font);
    o2_text_semi(-1);
    o2_text_rgb(((rgb >> 16) & 0xFFu) / 2 + 0x40, ((rgb >> 8) & 0xFFu) / 2 + 0x40, (rgb & 0xFFu) / 2 + 0x40);
    o2_text_at(static_cast<u32>(x) & 0xFFFFu, static_cast<u32>(y) & 0xFFFFu);
    const u32 t = m.r32(kText);
    const u32 end = t + 12000u * (m.r32(scene(m) + 4) & 1u) + 12000u;
    const bool variable = m.r8(t + kVariableWidth) != 0;
    for (; *s; ++s) {
        const u32 from = m.r16(t + kPosition);
        if (*s == ' ') {
            m.w16(t + kPosition, from + static_cast<u32>(scale) * (variable ? m.r8(t + kSpace) : m.r8(t + kWidth) + 1u));
            continue;
        }
        const u32 quad = m.r32(t + kQuadCursor);
        if (quad + 40 > end) break;
        o2_text_glyph(static_cast<u8>(*s));
        if (scale > 1) {
            const s32 x0 = m.r16s(quad + 8), y0 = m.r16s(quad + 10);
            const s32 x1 = x0 + scale * (m.r16s(quad + 16) - x0 + 1) - 1, y1 = y0 + scale * (m.r16s(quad + 26) - y0 + 1) - 1;
            m.w16(quad + 16, static_cast<u32>(x1));
            m.w16(quad + 32, static_cast<u32>(x1));
            m.w16(quad + 26, static_cast<u32>(y1));
            m.w16(quad + 34, static_cast<u32>(y1));
            m.w16(t + kPosition, from + static_cast<u32>(scale) * (m.r16(t + kPosition) - from));
        }
    }
}

void Game::o2x_options_buttons() {
    const u32 choice = m.r8(dialog(m) + 4);
    struct Button {
        s32 y;
        const char* label;
        u32 id;
    };
    static const Button kButtons[] = {{218, "GRAPHICS", 6}, {303, "ENHANCE", 7}};
    for (const Button& b : kButtons) {
        const bool on = choice == b.id;
        const s32 x = 233, w = 78, h = 72;
        o2x_text(x + (w - o2x_text_width(b.label, kSmall)) / 2, b.y + (h - 10) / 2, b.label, on ? kOrange : 0xFFFFFFu,
                 kSmall);
        o2x_box(x, b.y, w, h, on ? kOrange : kGrey, kBlack);
    }
    o2x_rect(231, 213, 82, 168, kBlack);
    o2x_rect(229, 211, 86, 172, 0xE0E0E0u);
}

void Game::o2x_page_tick() {
    const u32 d = dialog(m), page = m.r8(d) - kEnginePage;
    const SettingInfo* rows[16];
    u32 n = 0;
    for (u32 i = 0; i < kSettingCount && n < 16; ++i)
        if (kSettings[i].page == page) rows[n++] = &kSettings[i];
    if (m.r8(d + 1) == 0) {
        o2x_cursor_ = 0;
        o2x_scroll_ = 0;
        m.w8(d + 1, 1);
    }
    if (!n || !player_settings) return;
    Settings& s = *player_settings;
    if (o2x_cursor_ >= n) o2x_cursor_ = 0;

    const u32 repeat = m.r32(scene(m) + kRepeat), pressed = m.r32(scene(m) + kPressed);
    const u32 held = m.r32(scene(m) + kHeld);
    if (repeat & 2) {
        o2x_cursor_ = (o2x_cursor_ + 1) % n;
        sound_play(0x3D);
    }
    if (repeat & 1) {
        o2x_cursor_ = (o2x_cursor_ + n - 1) % n;
        sound_play(0x3D);
    }
    const SettingInfo& row = *rows[o2x_cursor_];
    u32& value = s.*row.field;
    const bool forward = (repeat & 8) || ((pressed & 0x10) && !(held & 0x2F));
    if (forward || (repeat & 4)) {
        const u32 before = value;
        if (row.count == 2) value ^= 1u;
        else if (forward && value + 1 < row.count) ++value;
        else if (!forward && value > 0) --value;
        if (value != before) {
            s.changed = true;
            sound_play(0x2E);
        }
    }
    if ((pressed & 0x20) && !(held & 0x5F)) {
        o2_entry_enter(0, 1);
        m.w8(d, 0);
        m.w8(d + 1, 0);
        sound_play(0x3B);
        m.w32(scene(m) + kPressed, 0);
        return;
    }

    const s32 left = 14, top = 214, width = 298, height = 166;
    o2x_text(left + 12, top + 6, page == 0 ? "GRAPHICS" : "ENHANCEMENTS", kOrange, kSmall, 2);
    o2x_text(left + width - 12 - o2x_text_width("X  BACK", kSmall), top + 11, "X  BACK", 0x909090u, kSmall);
    const u32 shown = std::min(n, kRowsShown);
    if (o2x_cursor_ < o2x_scroll_) o2x_scroll_ = o2x_cursor_;
    if (o2x_cursor_ >= o2x_scroll_ + shown) o2x_scroll_ = o2x_cursor_ + 1 - shown;
    if (o2x_scroll_ + shown > n) o2x_scroll_ = n - shown;
    const bool scrolls = n > shown;
    const s32 first = shown > 6 ? 37 : 40;
    const s32 step = shown > 6 ? std::clamp((height - first - 16) / static_cast<s32>(shown - 1), 14, 18) : 20;
    const s32 right = left + width - (scrolls ? 22 : 16);
    for (u32 k = 0; k < shown; ++k) {
        const u32 i = o2x_scroll_ + k;
        const s32 y = top + first + step * static_cast<s32>(k);
        const bool on = i == o2x_cursor_;
        o2x_text(left + 14, y, rows[i]->name, on ? kOrange : 0xFFFFFFu, kSmall);
        const char* v = rows[i]->labels[s.*rows[i]->field];
        o2x_text(right - o2x_text_width(v, kSmall), y, v, on ? kOrange : 0xC8C8C8u, kSmall);
        if (on) o2x_rect(left + 6, y - (step - 10) / 2, width - (scrolls ? 20 : 12), step, kDim);
    }
    if (scrolls) {
        const s32 y0 = top + first - (step - 10) / 2, track = step * static_cast<s32>(shown);
        const s32 thumb = track * static_cast<s32>(shown) / static_cast<s32>(n);
        const s32 at = (track - thumb) * static_cast<s32>(o2x_scroll_) / static_cast<s32>(n - shown);
        o2x_rect(left + width - 11, y0 + at, 3, thumb, kOrange);
        o2x_rect(left + width - 11, y0, 3, track, kDim);
    }
    o2x_rect(left + 6, top + 30, width - 12, 2, kOrange);
    o2x_box(left, top, width, height, kGrey, kBlack);
}
}

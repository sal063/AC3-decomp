#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kState = 0x800DD9D0u;
constexpr u32 kSprites = 0x800DDA18u;
constexpr u32 kWidths = 0x800DD288u;
u32 state(Memory& m) { return m.r32(kState); }
u32 scene(Memory& m) { return m.r32(g::kScene); }
void link(Memory& m, u32 ot, u32 p) {
    m.w32(p, (m.r32(p) & 0xFF000000u) | (m.r32(ot) & 0xFFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (p & 0xFFFFFFu));
}
}

u32 Game::o1_text_sprites(u32 x0, u32 y0, u32 text, u32 ot, u32 rgb, u32 flags) {
    cover(0x800CD2F0u);
    Frame frame(*this, 0x38);
    const s32 period = static_cast<s16>(libc_strlen(text)) + 4;
    const u32 base[3] = {rgb & 0xFFu, (rgb >> 8) & 0xFFu, (rgb >> 16) & 0xFFu};
    const auto clock = [&] { return m.r32s(m.r32(kSprites) + 0x20); };
    u32 x = 0, line = 0, index = 0;
    for (u32 at = text; m.r8(at); ++at) {
        if (m.r8(at) == '\n') {
            x = 0;
            ++line;
            continue;
        }
        if (m.r8(at) - 0x21u >= 0x5Au) m.w8(at, 0x20);
        const u32 c = m.r8(at);
        if (c == 0x20) {
            x += (flags & 1) ? 4 : 6;
            ++index;
            continue;
        }
        u32 colour[3] = {base[0], base[1], base[2]};
        if (flags & 2)
            for (u32 k = 0; k < 3; ++k) {
                const s32 t = clock() % 120 - 60;
                colour[k] = base[k] + static_cast<u32>((t < 0 ? -t : t) * static_cast<s32>(base[k]) / 60) - (base[k] >> 1);
            }
        const s32 position = static_cast<s16>(index);
        if (flags & 4) {
            const s32 sweep = static_cast<s32>(wrap::div(clock() / 4, period).hi);
            for (u32 k = 0; k < 3; ++k) colour[k] = position < sweep ? base[k] << 1 : base[k] >> 2;
        }
        if (flags & 8) {
            const s32 blink = static_cast<s32>(wrap::div(clock() / 5, period).hi);
            for (u32 k = 0; k < 3; ++k) colour[k] = position == blink ? base[k] >> 2 : base[k] << 1;
        }
        const u32 sprite = m.r32(m.r32(kSprites) + 8);
        for (u32 k = 0; k < 3; ++k) m.w8(sprite + 4 + k, colour[k]);
        m.w16(sprite + 8, (x & 0xFFu) + x0);
        m.w16(sprite + 0xA, (line & 0xFFu) + y0);
        const s32 column = static_cast<s32>(c) - 0x41;
        const s32 rounded = column >= 0 ? column : static_cast<s32>(c) - 0x22;
        m.w8(sprite + 0xC, static_cast<u32>(column - ((rounded >> 5) << 5)) << 3);
        m.w8(sprite + 0xD, static_cast<u32>((static_cast<s32>(c) - 0x21) >> 5) * 14);
        link(m, ot, sprite);
        m.w32(m.r32(kSprites) + 8, m.r32(m.r32(kSprites) + 8) + 20);
        x += (flags & 1) ? m.r8(kWidths + c - 0x21) : 6;
        ++index;
    }
    const u32 mode = m.r32(m.r32(kSprites) + 0x14);
    link(m, ot, mode);
    m.w32(m.r32(kSprites) + 0x14, mode + 12);
    return mode + 12;
}

u32 Game::o1_transition_step() {
    cover(0x800CE290u);
    Frame frame(*this, 0x18);
    u32 result = 0;
    call_address(m.r32(0x800DD914u + 4 * m.r32(state(m) + 0xE0)), 0, 0, 0, 0, result);
    if (result & 1) m.w32(state(m) + 0xE0, m.r32(state(m) + 0xE0) + 1);
    if (result & 2) {
        m.w8(state(m) + 0x58, 1);
        return 1;
    }
    if (result & 4) m.w32(state(m) + 0xE0, 1);
    const u32 flow = t1_flow();
    if (m.r32s(state(m) + 0xE0) >= 2) {
        o1_screen_text_select(0x400036);
        o1_animation_command(4, 4);
    }
    static constexpr u32 kPrompt[7] = {0, 0x49, 0x46, 0x47, 0x65, 0x66, 0};
    if (flow < 1 || flow > 6) return 6;
    const u32 out = o1_animation_command(1, kPrompt[flow]);
    return flow == 6 ? out : 6;
}

u32 Game::o1_saved_account_run() {
    cover(0x800D3B3Cu);
    Frame frame(*this, 0x18);
    const u32 place = m.r8(state(m) + 0xD3);
    if (place <= 1) {
        const s32 t = m.r32s(scene(m) + 0x20);
        const s32 a = (t - ((t < 0 ? t + 0x3F : t) >> 6 << 6)) << 6;
        const s32 x = place ? 0x33 : 0x46, y = place ? 0x18D : 0x19A;
        m.w32(state(m) + 8, t1_logo(x, y, a, m.r32(state(m) + 8)));
    }
    {
        const u32 st = state(m);
        const u32 table = m.r32(0x800DD66Cu + 4u * static_cast<u32>(m.r16s(st + 0x4C)));
        u32 ignored = 0;
        call_address(m.r32(table + 4u * static_cast<u32>(m.r16s(st + 0x4E))), st, 0, 0, 0, ignored);
    }
    t1_new_pilot();
    t1_new_flags(0, 0);
    t1_flag(0x11, m.r8(state(m) + 0xD0));
    t1_flag(0x10, m.r8(state(m) + 0xD1) == 1 ? 2u : m.r8(state(m) + 0x5A));
    t1_flag(3, m.r32(scene(m) + 0x394));
    t1_flag(5, (m.r32(scene(m) + 0x124) & 0x10u) ? 1u : 0u);
    if (const u32 pending = cd_pending(1)) return pending;
    cd_request_stream(state(m) + 0x7C, 1, 0);
    return 0;
}
}

#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace { constexpr u32 kMenu = 0x80112DC4u, kSelector = 0x80112E10u, kFreeSelection = 16; }

u32 Game::o2_selector_description() {
    cover(0x800F9CF0u);
    Frame frame(*this, 32);
    u32 menu = m.r32(kMenu);
    const u32 packet = m.r32(menu + 0x274);
    m.w16(packet + 12, 132); m.w16(packet + 14, 318); m.w16(packet + 10, 29);
    m.w8(packet + 3, 3);
    for (u32 channel = 0; channel < 3; ++channel) m.w8(packet + 4 + channel, 0);
    m.w8(packet + 7, 0x60); m.w16(packet + 8, 0);
    menu = m.r32(kMenu);
    const u32 ot = m.r32(menu + 8);
    m.w32(packet, (m.r32(packet) & 0xFF000000u) | (m.r32(ot) & 0xFFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (packet & 0xFFFFFFu));
    m.w32(menu + 0x274, packet + 16);
    s32 aircraft = m.r16s(menu + 0x1E0 + 2 * m.r32(menu + 0xC0));
    if (aircraft <= 0) aircraft = 1;
    if (aircraft >= 37) aircraft = 36;
    o2_text_font(7);
    o2_text_rgb(0, 128, 0);
    o2_text_at(12, 64);
    o2_text_sprite_label(0x800CD66Cu);
    const u32 description = 0x8010EB3Cu + 12 * static_cast<u32>(aircraft);
    u32 result = 0;
    constexpr u32 lineY[] = {96, 112, 144};
    for (u32 line = 0; line < 3; ++line) {
        o2_text_at(12, lineY[line]);
        result = o2_text_sprite_label(m.r32(description + 4 * line));
    }
    return result;
}

u32 Game::o2_selector_begin() {
    cover(0x800F9E68u);
    Frame frame(*this, 24);
    const u32 selector = m.r32(kSelector);
    m.w16(selector + 2, 0); m.w16(selector, 1);
    o2_flag(4, 0);
    return m.r32(m.r32(g::kScene) + 0x96C) & kFreeSelection ? o2_selector_description() : 0;
}

u32 Game::o2_selector_fade_in() {
    cover(0x800FA860u);
    Frame frame(*this, 24);
    o2_flag(1, 1); o2_flag(4, 0);
    if (o2_fade_state(8) == 2) {
        const u32 selector = m.r32(kSelector);
        m.w16(selector, 4); m.w16(selector + 2, 0);
    }
    return m.r32(m.r32(g::kScene) + 0x96C) & kFreeSelection ? o2_selector_description() : 0;
}

u32 Game::o2_selector_confirm() {
    cover(0x800FA8E0u);
    Frame frame(*this, 24);
    if (m.r32(m.r32(g::kScene) + 0x96C) & kFreeSelection) o2_selector_description();
    u32 scene = m.r32(g::kScene);
    if ((m.r32(scene + 0x12C) & 16u) && !(m.r32(scene + 0x128) & 47u)) {
        for (u32 fade : {8u, 2u, 8u, 1u, 7u}) o2_fade_set(fade, 3, -1, -1);
        sound_play(91); sound_play(60);
        const u32 selector = m.r32(kSelector);
        m.w16(selector, 5); m.w16(selector + 2, 1);
    }
    scene = m.r32(g::kScene);
    u32 result = m.r32(scene + 0x12C) & 32u;
    if (result) {
        result = m.r32(scene + 0x128) & 95u;
        if (!result) {
            o2_fade_set(8, 3, -1, -1);
            sound_play(91); sound_play(59);
            const u32 selector = m.r32(kSelector);
            m.w16(selector, 5); m.w16(selector + 2, 2);
            return 2;
        }
    }
    return result;
}
u32 Game::o2_selector_exit() {
    cover(0x800FAA48u);
    Frame frame(*this, 24);
    o2_flag(4, m.r16s(m.r32(kSelector) + 2) == 2 ? 0 : 1);
    if (o2_fade_state(8)) {
        o2_selector_palette_draw(static_cast<u32>(o2_fade_value(8)));
    } else {
        u32 selector = m.r32(kSelector);
        if (m.r16s(selector + 2) == 1) {
            m.w32(selector + 16, 0);
            const u32 menu = m.r32(kMenu);
            m.w16(menu + 0x84, 3); m.w16(menu + 0x86, 0);
            o2_selector_commit_variants();
            heap_set(m.r32(kSelector));
            o2_voice_control(3, 0);
            sound_play(90);
        }
        selector = m.r32(kSelector);
        if (m.r16s(selector + 2) == 2) {
            if (!(m.r32(m.r32(g::kScene) + 0x96C) & kFreeSelection)) {
                for (u32 fade : {3u, 5u, 6u}) o2_fade_set(fade, 1, 0, 25);
                o2_fade_set(8, 1, 0, 25);
            } else o2_fade_set(3, 1, 0, 25);
            o2_group_set(0, 0x7701);
            sound_play(90);
            selector = m.r32(kSelector);
            m.w16(selector, 0);
            m.w32(selector + 16, m.r32(selector + 16) | 2u);
        }
    }
    const u32 freeSelection = m.r32(m.r32(g::kScene) + 0x96C) & kFreeSelection;
    return freeSelection ? o2_selector_description() : 0;
}
}

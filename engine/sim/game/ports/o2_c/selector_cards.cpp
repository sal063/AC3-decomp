#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
u32 Game::o2_selector_cards(u32 packet, u32 ot) {
    cover(0x800F8FA8u);
    Frame frame(*this, 120);
    if (!o2_fade_state(8)) return packet;
    constexpr u32 kSelector = 0x80112E10u;
    auto label = [&](u32 category, u32 offset) {
        const u32 scene = m.r32(g::kScene);
        return o2_aircraft_label(m.r16s(scene + 0x38C), m.r16s(scene + 0x38E), category,
                                 m.r8(m.r32(kSelector) + offset));
    };
    m.w8(frame.at(64), label(0, 20)); m.w8(frame.at(65), label(2, 21));
    const u32 upperIncoming = label(0, 22);
    m.w8(frame.at(66), label(2, 23));
    auto fadeValue = [&](u32 fade) { return o2_fade_value(fade); };
    const s32 upperFade = fadeValue(9), lowerFade = fadeValue(10);
    m.w32(frame.at(56), lowerFade);
    const u32 upperState = o2_fade_state(9); m.w32(frame.at(72), upperState);
    const u32 lowerState = o2_fade_state(10); m.w32(frame.at(60), lowerState);
    const s32 panelFade = fadeValue(8);
    const u32 panelState = o2_fade_state(8);
    m.w32(frame.at(24), m.r32(0x800CD398u)); m.w32(frame.at(28), m.r32(0x800CD39Cu));
    auto link = [&](u32 primitive) {
        m.w32(primitive, (m.r32(primitive) & 0xFF000000u) | (m.r32(ot + 4) & 0xFFFFFFu));
        m.w32(ot + 4, (m.r32(ot + 4) & 0xFF000000u) | (primitive & 0xFFFFFFu));
    };
    set_draw_area(packet, frame.at(24)); link(packet); packet += 12;
    const s32 remaining = wrap::sub(25, panelFade);
    const s32 slide = panelState == 2 ? 0 : wrap::mul(wrap::mul(remaining, 325), remaining) / 625;
    auto sprite = [&](u32 card, u32 palette, s32 x, s32 y, s32 textureSlide) {
        m.w8(packet + 3, 4); m.w8(packet + 7, 0x66);
        for (u32 channel = 4; channel < 7; ++channel) m.w8(packet + channel, 128);
        m.w16(packet + 16, 48); m.w16(packet + 18, 88);
        m.w16(packet + 14, ((palette >> 3) + 24) * 64 | 27);
        m.w16(packet + 8, x); m.w16(packet + 10, y);
        m.w8(packet + 12, 48 * (card & 3u) + textureSlide);
        m.w8(packet + 13, 10 + 88 * ((card >> 2) & 1u));
        link(packet); packet += 20;
    };
    const s32 upperOffset = wrap::mul(upperFade, 48) / 10;
    const u32 upperOld = m.r8(frame.at(64)), upperNew = upperIncoming & 255u;
    sprite(upperState == 3 ? upperNew : upperOld, upperNew, wrap::add(slide + 234, upperOffset), 68, 0);
    sprite(upperState == 3 ? upperOld : upperNew, upperNew, wrap::add(slide + 186, upperOffset), 68, 0);
    auto cardArea = [&](s32 y) {
        const s32 x = wrap::add(slide, 234), narrowed = static_cast<s16>(x);
        m.w16(frame.at(32), x); m.w16(frame.at(34), y);
        m.w16(frame.at(36), narrowed + 48 < 320 ? 48 : wrap::sub(320, x));
        m.w16(frame.at(38), 88);
        if (narrowed < 320) { set_draw_area(packet, frame.at(32)); link(packet); }
        packet += 12;
    };
    cardArea(68);
    const u32 lowerOld = m.r8(frame.at(65)), lowerNew = m.r8(frame.at(66));
    const s32 lowerOffset = wrap::mul(lowerFade, 48) / 10;
    const u32 first = lowerState == 3 ? lowerNew : lowerOld, second = lowerState == 3 ? lowerOld : lowerNew;
    sprite(first, first, wrap::add(slide + 234, lowerOffset), 184, lowerState == 3 ? slide : 0);
    sprite(second, second, wrap::add(slide + 186, lowerOffset), 184, lowerState == 3 ? slide : 0);
    cardArea(184);
    m.w32(frame.at(16), 0); set_draw_mode(packet, 0, 0, 76, 0); link(packet); packet += 12;
    const u32 ratings = 0x8010EAC0u + 4 * m.r8(m.r32(kSelector) + 0x119);
    const u32 bar = frame.at(40);
    m.w8(frame.at(48), 229); m.w8(frame.at(49), 229); m.w8(frame.at(50), 213); m.w8(frame.at(51), 0);
    for (u32 rating = 0; rating < 3; ++rating) {
        m.w16(bar, slide + 266); m.w16(bar + 2, 314 + 19 * rating);
        m.w16(bar + 4, 38 * m.r8(ratings + rating) / 15); m.w16(bar + 6, 8);
        const u32 world = m.r32(g::kWorld);
        m.w32(world + 20, o2_rectangle_flat(bar, m.r32(world + 20), ot + 4));
    }
    o2_flag(12, m.r8(ratings + 3)); o2_flag(13, m.r8(m.r32(kSelector) + 0x119));
    return packet;
}
}

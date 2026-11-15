#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
u32 Game::o2_selector_choose() {
    cover(0x800F9EC8u);
    Frame frame(*this, 32);
    constexpr u32 kMenu = 0x80112DC4u, kSelector = 0x80112E10u;
    auto scene = [&] { return m.r32(g::kScene); };
    auto freeSelection = [&] { return (m.r32(scene() + 0x96C) & 16u) != 0; };
    if (freeSelection() && m.r16s(m.r32(kMenu) + 0x1C6) != 3) {
        m.w32(scene() + 0x130, m.r32(scene() + 0x130) & 0xFFFFFFC3u);
        m.w32(scene() + 0x12C, m.r32(scene() + 0x12C) & 0xFFFFFFC3u);
    }
    o2_flag(1, 0);
    const u32 menu = m.r32(kMenu);
    if (freeSelection()) {
        if (m.r16s(scene() + 0x38C) == 42) { o2_flag(3, 2); o2_flag(2, 2); }
        else {
            o2_flag(2, (m.r32(scene() + 0x128) & 4u) ? 1 : 0);
            o2_flag(3, (m.r32(scene() + 0x128) & 8u) ? 1 : 0);
        }
    } else {
        o2_flag(2, m.r32(menu + 0xC0) == 0 ? 2 : (m.r32(scene() + 0x128) & 4u) ? 1 : 0);
        o2_flag(3, m.r32(menu + 0xC0) == m.r32(menu + 0xE4) - 1 ? 2 :
                          (m.r32(scene() + 0x128) & 8u) ? 1 : 0);
    }
    o2_flag(4, m.r16s(m.r32(menu + 0x16C) + 0x6A) == 2 ? 1 : 0);
    if (m.r16s(scene() + 0x38C) != 42) {
        const u32 repeat = m.r32(scene() + 0x130);
        const bool right = (repeat & 8u) != 0, left = !right && (repeat & 4u) != 0;
        if (right || left) {
            bool changed = false;
            if (freeSelection()) {
                o2_aircraft_group(0, m.r16s(menu + 0x1E0 + 2 * m.r32(menu + 0xC0)));
                const s32 selected = m.r32s(menu + 0xC0), last = m.r16s(menu + 0x262) - 1;
                const s32 next = right ? (selected < last ? wrap::add(selected, 1) : 0) :
                                        (selected > 0 ? wrap::sub(selected, 1) : last);
                m.w32(menu + 0xC0, next); m.w32(menu + 0xC4, 0); changed = true;
            } else {
                const s32 selected = m.r32s(menu + 0xC0), last = wrap::sub(m.r32s(menu + 0xE4), 1);
                if ((right && selected < last) || (left && selected > 0)) {
                    m.w32(menu + 0xC0, wrap::add(selected, right ? 1 : -1)); changed = true;
                }
            }
            if (changed) { sound_play(61); o2_fade_set(2, 3, -1, -1); o2_fade_set(8, 3, -1, -1); }
        }
    }
    if (freeSelection()) o2_selector_description(); else o2_selector_model_stage(1);
    if ((m.r32(scene() + 0x12C) & 16u) && !(m.r32(scene() + 0x128) & 0x2Fu)) {
        bool confirm = freeSelection();
        if (!confirm) {
            const bool ready = m.r16s(m.r32(kMenu) + 0x1C6) == 3;
            confirm = o2_fade_state(2) == 2 && ready;
            if (confirm) for (u32 fade : {5u, 6u, 3u, 8u}) o2_fade_set(fade, 3, -1, -1);
        }
        if (confirm) {
            sound_play(91); sound_play(60);
            m.w16(m.r32(kSelector) + 2, 1); m.w16(m.r32(kSelector), 2);
        }
    }
    if (!(m.r32(scene() + 0x12C) & 32u)) return 0;
    if (const u32 held = m.r32(scene() + 0x128) & 0x5Fu) return held;
    for (u32 fade : {2u, 1u, 3u, 11u, 5u, 6u, 7u, 8u}) o2_fade_set(fade, 3, -1, -1);
    sound_play(91); sound_play(59); o2_hangar_effect2_exit();
    m.w16(m.r32(kSelector) + 2, 2); m.w16(m.r32(kSelector), 2);
    return 2;
}
}

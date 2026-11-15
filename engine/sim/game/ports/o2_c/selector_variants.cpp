#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kSelector = 0x80112E10u, kMenu = 0x80112DC4u;
constexpr u32 kCurrentVariant = 20, kWantedVariant = 22, kActiveRow = 0x118, kVariantCounts = 0x11A;
}

u32 Game::o2_selector_variants_tick() {
    cover(0x800F8C0Cu);
    Frame frame(*this, 32);
    const s32 primaryFade = o2_fade_state(9), secondaryFade = o2_fade_state(10);
    if (primaryFade == 0 || primaryFade == 2) m.w8(m.r32(kSelector) + 20, m.r8(m.r32(kSelector) + 22));
    if (secondaryFade == 0 || secondaryFade == 2) m.w8(m.r32(kSelector) + 21, m.r8(m.r32(kSelector) + 23));
    if (primaryFade == 2) o2_fade_set(9, 0, 0, 0);
    if (secondaryFade == 2) o2_fade_set(10, 0, 0, 0);
    if (primaryFade || secondaryFade) {
        if (primaryFade == 2 || secondaryFade == 2)
            return m.r32(m.r32(0x80112DC0u) + 80) + (secondaryFade == 2 ? 20 : 18);
        return secondaryFade == 0 ? m.r8(m.r32(kSelector) + 23) : 2;
    }
    const u32 scene = m.r32(g::kScene);
    if (m.r32(scene + 0x12C) & 3) {
        const u32 selector = m.r32(kSelector);
        m.w8(selector + kActiveRow, 1 - m.r8(selector + kActiveRow));
    }
    for (u32 flag : {9u, 8u, 11u, 10u}) o2_flag(flag, 0);
    if (m.r16s(m.r32(kSelector)) != 5) {
        const u32 buttons = m.r32(m.r32(g::kScene) + 0x130);
        const bool forward = buttons & 8, backward = !forward && (buttons & 4);
        if (forward || backward) {
            for (u32 row = 0; row < 2; ++row) {
                const u32 selector = m.r32(kSelector);
                if (m.r8(selector + kActiveRow) != row) continue;
                const u32 current = m.r8(selector + kCurrentVariant + row);
                const s32 last = static_cast<s32>(m.r8(selector + kVariantCounts + row)) - 1;
                if (forward ? static_cast<s32>(current) >= last : current == 0) continue;
                m.w8(selector + kWantedVariant + row, forward ? current + 1 : current - 1);
                const u32 fade = 9 + row;
                o2_fade_set(fade, forward ? 3 : 1, forward ? 10 : 0, 10);
                o2_flag(forward ? (row ? 11 : 9) : (row ? 10 : 8), 1);
                sound_play(61);
            }
        }
    }
    for (u32 row = 0; row < 2; ++row) {
        const u32 selector = m.r32(kSelector), current = m.r8(selector + kCurrentVariant + row);
        const s32 last = static_cast<s32>(m.r8(selector + kVariantCounts + row)) - 1;
        o2_flag(row ? 14 : 7, (static_cast<s32>(current) < last ? 2u : 0u) | (current ? 1u : 0u));
    }
    o2_flag(15, m.r8(m.r32(kSelector) + kActiveRow));
    const u32 selector = m.r32(kSelector), row = m.r8(selector + kActiveRow);
    if (row <= 1) {
        const u32 currentScene = m.r32(g::kScene);
        const u32 label = o2_aircraft_label(m.r16s(currentScene + 0x38C), m.r16s(currentScene + 0x38E),
                                          row ? 2 : 0, m.r8(selector + kWantedVariant + row));
        m.w8(m.r32(kSelector) + 0x119, label);
        return label;
    }
    return 0x800C0000u;
}

u32 Game::o2_selector_commit_tick() {
    cover(0x800FA4E4u);
    Frame frame(*this, 40);
    o2_flag(1, 0); o2_flag(4, m.r16s(m.r32(kSelector) + 2) == 1 ? 1 : 0);
    if (o2_fade_state(6)) {
        const s32 value = o2_fade_value(6);
        o2_selector_palette_draw(static_cast<u32>(wrap::sub(25, value)));
    } else {
        if (m.r16s(m.r32(kSelector) + 2) == 1) {
            o2_fade_set(8, 1, 0, 25); o2_entry_reset(0, 11); o2_group_set(0, 0x7A01); sound_play(90);
            const u32 selector = m.r32(kSelector);
            m.w16(selector, 3); m.w32(selector + 16, m.r32(selector + 16) & ~2u);
            u32 scene = m.r32(g::kScene);
            const s32 aircraft = o2_aircraft_id(m.r16s(scene + 0x38C), m.r32(m.r32(kMenu) + 0xC0));
            scene = m.r32(g::kScene); m.w16(scene + 0x38E, aircraft);
            for (u32 row = 0; row < 2; ++row) {
                scene = m.r32(g::kScene);
                const u32 variant = o2_aircraft_default_variant(m.r16s(scene + 0x38C), m.r16s(scene + 0x38E), row);
                m.w8(m.r32(kSelector) + kWantedVariant + row, variant);
                m.w8(m.r32(kSelector) + kCurrentVariant + row, variant);
            }
            for (u32 row = 0; row < 2; ++row) {
                scene = m.r32(g::kScene);
                m.w8(m.r32(kSelector) + kVariantCounts + row,
                      o2_aircraft_variant(m.r16s(scene + 0x38C), m.r16s(scene + 0x38E), row));
            }
        }
        if (m.r16s(m.r32(kSelector) + 2) == 2) {
            cd_reset(0, 0);
            u32 scene = m.r32(g::kScene);
            const s32 mission = m.r16s(scene + 0x38C);
            if (m.r16s(0x801127ACu + 12 * static_cast<u32>(mission)) != -1 && !(m.r32(scene + 0x96C) & 16)) {
                o2_entry_enter(0, 5); o2_entry_enter(0, 6);
                m.w32(frame.at(16), 496); m.w32(frame.at(20), 0);
                o2_entry_set(0, 7, 0, 0, 496, 0); o2_group_set(0, 0xE1); sound_play(90);
                m.w16(m.r32(kMenu) + 0x84, 1);
            } else {
                for (u32 offset : {0x7Cu, 0x7Du, 0x7Eu}) m.w8(m.r32(kMenu) + offset, 0);
                o2_entry_enter(0, 1);
                scene = m.r32(g::kScene);
                if (m.r32(scene + 0x96C) & 16) m.w16(scene + 0x38C, 59);
            }
            for (u32 soundId = 93; soundId <= 97; ++soundId)
                m.w32(m.r32(kMenu) + 0x130 + 4 * (soundId - 93), sound_play(soundId));
            m.w16(m.r32(kMenu) + 0x86, 0); o2_fade_set(1, 1, 0, 40);
            const u32 selector = m.r32(kSelector); m.w32(selector + 16, 0); heap_set(selector);
            m.w32(m.r32(g::kWorld) + 36, 0); m.w32(m.r32(g::kWorld) + 32, 0);
        }
    }
    if (m.r32(m.r32(g::kScene) + 0x96C) & 16) return o2_selector_description();
    return 0;
}

u32 Game::o2_selector_alloc() {
    cover(0x800FABE8u);
    Frame frame(*this, 32);
    const u32 rank = o2_campaign_rank_mask();
    m.w32(m.r32(kMenu) + 0x26C, rank); cd_reset(0, 0);
    m.w32(kSelector, mem_alloc(0x13C));
    const u32 modelBuffer = mem_alloc(0x4000), selector = m.r32(kSelector);
    m.w32(selector + 4, modelBuffer); m.w16(selector, 0); m.w16(selector + 2, 0);
    m.w32(selector + 0x120, 0); m.w32(m.r32(kMenu) + 0xC0, 0);
    u32 scene = m.r32(g::kScene);
    if (m.r32(scene + 0x96C) & 16) {
        if (rank & 4) m.w8(scene + 0x98D, 1);
        if (rank & 2) m.w8(m.r32(g::kScene) + 0x98F, 1);
        if (rank & 1) for (u32 offset : {0x993u, 0x994u, 0x995u, 0x996u, 0x997u, 0x998u, 0x975u, 0x976u, 0x986u, 0x987u})
            m.w8(m.r32(g::kScene) + offset, 1);
        const u32 menu = m.r32(kMenu);
        scene = m.r32(g::kScene); m.w16(menu + 0x262, 0); m.w16(menu + 0x260, 0);
        if (m.r16s(scene + 0x38C) == 42) {
            m.w16(menu + 0x1E0, 24); m.w16(menu + 0x262, 1);
        } else {
            for (u32 aircraft = 0; aircraft < 64; ++aircraft) {
                if (m.r8(scene + 0x974 + aircraft) != 1 || aircraft == 24) continue;
                const s32 index = m.r16s(menu + 0x262);
                m.w16(menu + 0x1E0 + 2 * static_cast<u32>(index), aircraft);
                if (m.r16s(scene + 0x38E) == static_cast<s32>(aircraft)) m.w16(menu + 0x260, m.r16(menu + 0x262));
                m.w16(menu + 0x262, m.r16(menu + 0x262) + 1);
            }
        }
        const s32 selected = m.r16s(menu + 0x260); m.w32(menu + 0xC0, selected);
        if (m.r16s(menu + 0x1E0 + 2 * static_cast<u32>(selected)) != m.r16s(scene + 0x38E)) {
            m.w32(menu + 0xC0, 0); m.w16(menu + 0x260, 0);
        }
        m.w32(m.r32(kMenu) + 0xC4, 0);
    }
    o2_panel_scene_alloc(); o2_panel_camera_init();
    u32 menu = m.r32(kMenu), current = m.r32(kSelector);
    m.w32(current + 12, 0x8010EB0C); m.w8(current + kActiveRow, 0);
    m.w32(current + 8, m.r32(menu + 0xB8) + 24 * m.r32(menu + 0xC0));
    if (!(m.r32(m.r32(g::kScene) + 0x96C) & 16)) o2_voice_script(0x8010EB00);
    for (u32 offset = 0x130; offset <= 0x140; offset += 4) voice_release(m.r32(m.r32(kMenu) + offset));
    if (!(m.r32(m.r32(g::kScene) + 0x96C) & 16)) o2_voice_control(1, 0);
    sound_play(90);
    if (m.r32(m.r32(g::kScene) + 0x96C) & 16) {
        o2_fade_set(3, 1, 0, 25); o2_fade_set(11, 1, 0, 25);
    } else for (u32 fade : {2u, 8u, 1u, 3u, 11u, 5u, 6u}) o2_fade_set(fade, 1, 0, 25);
    o2_fade_set(7, 1, 0, 25); o2_group_set(0, 0x7701);
    const u32 object = m.r32(m.r32(g::kSlots) + 8);
    m.w32(object, m.r32(object) | 0x1C000); m.w8(object + 4, 2);
    m.w32(object + 8, 0); m.w32(object + 12, 0); m.w32(object + 16, 0);
    m.w16(object + 24, 0); m.w16(object + 26, 0x1000); m.w16(object + 28, 0x1000);
    menu = m.r32(kMenu); hw->load_image(menu + 0xD8, m.r32(menu + 0xD4));
    m.w32(m.r32(kSelector) + 16, 3); o2_selector_packets_init(); o2_selector_model_stage(0);
    if (m.r32(m.r32(g::kScene) + 0x96C) & 16) {
        o2_fade_set(2, 3, 0xFFFFFFFFu, 0xFFFFFFFFu); o2_fade_set(8, 3, 0xFFFFFFFFu, 0xFFFFFFFFu);
    }
    menu = m.r32(kMenu); const u32 next = m.r16(menu + 0x86) + 1; m.w16(menu + 0x86, next);
    return next;
}
}

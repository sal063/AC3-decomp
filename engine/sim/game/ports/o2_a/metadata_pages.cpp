#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDialog = 0x80112DDCu, kScreens = 0x80112DC0u, kMenu = 0x80112DC4u;
constexpr u32 kCategory = 12, kFocus = 14, kPage = 16, kCompact = 28, kPreset = 29;
constexpr u32 kLabelCounts = 0x8010AC5Cu;
u32 dialog(Memory& m) { return m.r32(kDialog); }
u32 scene(Memory& m) { return m.r32(g::kScene); }
s32 focus(Memory& m) { return m.r16s(dialog(m) + kFocus); }
s32 category(Memory& m) { return m.r16s(dialog(m) + kCategory); }
u32 flags(Memory& m) { return m.r32(m.r32(kScreens) + 92); }
bool pressed(Memory& m, u32 button, u32 blocked) {
    const u32 state = scene(m); return (m.r32(state + 300) & button) && !(m.r32(state + 296) & blocked);
}
bool repeat(Memory& m, u32 button) { return (m.r32(scene(m) + 304) & button) != 0; }
}

void Game::o2_metadata_parent(u32 group) {
    if (group == 0 || group == 2) { o2_entry_enter(0, 1); m.w8(dialog(m), 0); }
    if (group == 1 || group == 3) { o2_group_set(2, 8); m.w8(dialog(m), 2); }
    m.w8(dialog(m) + 1, 0);
}

u32 Game::o2_metadata_summary_input() {
    cover(0x800D4F78u);
    Frame frame(*this, 40);
    const s32 group = category(m);
    if (group == 2 && m.r16(dialog(m) + kFocus) - 2u < 2) m.w16(dialog(m) + kFocus, 0);
    if (pressed(m, 64, 0x2F) && focus(m) != 4) {
        sound_play(60); m.w32(scene(m) + 300, 0);
        const u32 current = dialog(m); m.w8(current + 30, 0); m.w16(current + kFocus, 4); m.w8(dialog(m) + 31, 0);
    }
    if (group < 4) {
        if (repeat(m, 2)) {
            sound_play(61);
            const u32 current = dialog(m); m.w16(current + kFocus, focus(m) < 4 ? m.r16(current + kFocus) + 1 : 0);
            if (group == 2 && focus(m) == 2) m.w16(dialog(m) + kFocus, m.r16(dialog(m) + kFocus) + 2);
        }
        if (repeat(m, 1)) {
            sound_play(61);
            const u32 current = dialog(m); m.w16(current + kFocus, focus(m) > 0 ? m.r16(current + kFocus) - 1 : 4);
            if (group == 2 && focus(m) == 3) m.w16(dialog(m) + kFocus, m.r16(dialog(m) + kFocus) - 2);
        }
    } else m.w16(dialog(m) + kFocus, 0);
    for (u32 direction : {8u, 4u}) if (repeat(m, direction)) {
        if (focus(m) < 4) sound_play(61);
        if (focus(m) == 0 && group < 4) o2_metadata_layout_reset();
        if (focus(m) == 1) {
            const u32 current = dialog(m); m.w8(current + 20, 1 - m.r8(current + 20));
        }
        if (focus(m) == 2) {
            const u32 target = dialog(m) + 21 + static_cast<u32>(group), old = m.r8(target);
            m.w8(target, direction == 8 ? (old < 2 ? old + 1 : 0) : (old ? old - 1 : 2));
        }
        if (focus(m) == 3) {
            const u32 target = dialog(m) + 27, old = m.r8(target);
            m.w8(target, direction == 8 ? (old < 2 ? old + 1 : 0) : (old ? old - 1 : 2));
        }
    }
    if (pressed(m, 16, 0x2F) && focus(m) == 4) {
        o2_campaign_presets_apply(); sound_play(60); o2_metadata_parent(static_cast<u32>(group));
    }
    if (pressed(m, 32, 0x5F)) {
        sound_play(59); o2_entry_enter(0, 1); m.w8(dialog(m), 0); m.w8(dialog(m) + 1, 0); m.w32(scene(m) + 300, 0);
    }
    m.w8(flags(m) + 1, m.r8(dialog(m) + kFocus));
    auto choiceFlag = [&](u32 flag, u32 field, u32 value, s32 row) {
        const u32 current = dialog(m);
        m.w8(flags(m) + flag, m.r8(current + field) == value ? (m.r16s(current + kFocus) == row ? 2 : 0) : 1);
    };
    choiceFlag(2, 20, 0, 1); choiceFlag(3, 20, 1, 1);
    for (u32 value = 0; value < 3; ++value) choiceFlag(4 + value, 21 + static_cast<u32>(group), value, 2);
    choiceFlag(7, 27, 0, 3); choiceFlag(34, 27, 1, 3); choiceFlag(9, 27, 2, 3);
    m.w8(flags(m) + 10, focus(m) == 4 ? 2 : 0);
    if (static_cast<u32>(group) < 2 || group == 3) for (u32 flag : {12u, 13u, 14u}) m.w8(flags(m) + flag, 0);
    if (group == 2) {
        m.w8(flags(m) + 12, 0);
        for (u32 flag : {13u, 14u, 4u, 5u, 6u, 7u, 34u, 9u}) m.w8(flags(m) + flag, 1);
    }
    if (static_cast<u32>(group) - 4u < 2) for (u32 flag : {12u, 13u, 14u, 2u, 3u, 4u, 5u, 6u, 7u, 34u, 9u}) m.w8(flags(m) + flag, 1);
    if (group < 4) {
        switch (focus(m)) {
        case 0: m.w8(flags(m) + 20, 0); break;
        case 1: m.w8(flags(m) + 20, m.r8(dialog(m) + 20) + 1); break;
        case 2: m.w8(flags(m) + 20, m.r8(dialog(m) + 21 + static_cast<u32>(group)) + 3); break;
        case 3: m.w8(flags(m) + 20, m.r8(dialog(m) + 27) + 101); break;
        case 4: m.w8(flags(m) + 20, group == 1 ? 9 : 8); break;
        default: break;
        }
    } else if (group == 4 || group == 5) m.w8(flags(m) + 20, group == 4 ? 86 : 87);
    if (group < 4) return focus(m) == 4 ? (group == 1 ? 9u : 8u) : 4u;
    return group == 4 ? 86u : group == 5 ? 87u : 5u;
}

void Game::o2_metadata_connectors(Frame& frame) {
    for (u32 i = 28; i < 33; ++i) m.w8(frame.at(i), 0);
    for (u32 i = 0; i < m.r8(kLabelCounts + static_cast<u32>(category(m))); ++i) {
        const u32 current = dialog(m), group = static_cast<u32>(m.r16s(current + kCategory));
        const u32 record = m.r32(current + 184 + 4 * group) + 16 * i, fade = m.r32(record);
        if (fade != 0 && o2_fade_state(fade) != 2) continue;
        const u32 packet = m.r32(dialog(m) + 8);
        for (u32 word = 0; word < 3; ++word) m.w32(frame.at(16 + 4 * word), m.r32(record + 4 + 4 * word));
        const u32 next = o2_polyline3_double(frame.at(16), packet, m.r32(m.r32(kMenu) + 8) + 4);
        m.w32(dialog(m) + 8, next);
    }
    if (category(m) != 3) return;
    for (u32 i = 56; i < 61; ++i) m.w8(frame.at(i), 0);
    for (u32 i = 0; i < 3; ++i) {
        const u32 record = 0x8010AC20u + 20 * i, fade = m.r32(record);
        if (fade != 0 && o2_fade_state(fade) != 2) continue;
        const u32 packet = m.r32(dialog(m) + 8);
        for (u32 word = 0; word < 4; ++word) m.w32(frame.at(40 + 4 * word), m.r32(record + 4 + 4 * word));
        const u32 next = o2_polyline4_double(frame.at(40), packet, m.r32(m.r32(kMenu) + 8) + 4);
        m.w32(dialog(m) + 8, next);
    }
}

u32 Game::o2_metadata_detail_input() {
    cover(0x800D7DB0u);
    Frame frame(*this, 88);
    const s32 originalGroup = category(m);
    if (pressed(m, 64, 0x2F) && (focus(m) != 2 || m.r8(dialog(m) + kPreset) == 1) && !(m.r32(dialog(m) + 32) & 0xFFFF0000u)) {
        sound_play(60); m.w32(scene(m) + 300, 0);
        const u32 current = dialog(m); m.w8(current + 30, 0); m.w16(current + kFocus, 2); m.w8(dialog(m) + 31, 0); m.w8(dialog(m) + kPreset, 0);
        const s32 group = category(m);
        if (group == 0 || group == 3) o2_entry_position(2, group == 0 ? 9 : 11, 0, 0);
        m.w8(flags(m) + 8, 0);
    }
    if (focus(m) < 3) {
        if (repeat(m, 2)) {
            sound_play(61);
            const u32 current = dialog(m); m.w16(current + kFocus, m.r16(current + kFocus) + 1);
            if (focus(m) == 3) {
                if (m.r8(current + kCompact) == 0) m.w8(current + 30, category(m) == 1 ? 4 : 1);
                else m.w8(current + 31, 1);
            }
        }
        if (repeat(m, 1)) {
            sound_play(61);
            const u32 current = dialog(m);
            if (focus(m) > 0) m.w16(current + kFocus, m.r16(current + kFocus) - 1);
            else {
                m.w16(current + kFocus, 3);
                const u32 compact = m.r8(current + kCompact);
                if (compact == 0) m.w8(current + 30, 4);
                if (compact == 1) {
                    m.w8(current + 31, m.r8(kLabelCounts + static_cast<u32>(m.r16s(current + kCategory))));
                    if (category(m) == 3) { o2_fade_set(5, 3, -1, -1); o2_fade_set(7, 1, 0, 10); }
                }
            }
        }
        const bool preset = focus(m) == 2 && m.r8(dialog(m) + kPreset) == 1;
        if (preset && pressed(m, 16, 0x6F)) {
            sound_play(60);
            if (category(m) == 0) o2_dialog_preset_full();
            if (category(m) == 1) o2_dialog_preset_first();
            if (category(m) == 2) o2_dialog_preset_second();
            if (category(m) == 3) o2_dialog_preset_third();
        } else if ((!preset || (m.r32(scene(m) + 300) & 16)) && pressed(m, 16, 0x2F) && focus(m) == 2) {
            sound_play(60); o2_campaign_presets_apply(); o2_metadata_parent(static_cast<u32>(originalGroup)); m.w32(dialog(m) + kPage, 0);
        }
    } else if (m.r8(dialog(m) + kCompact) == 0) {
        switch (m.r8(dialog(m) + 34)) {
        case 0: o2_metadata_icon_input(); break;
        case 1: o2_metadata_icon_edit(); break;
        case 2: o2_metadata_icon_option(); break;
        default: break;
        }
    } else {
        switch (m.r8(dialog(m) + 35)) {
        case 0: o2_metadata_label_input(); break;
        case 1: o2_metadata_label_edit(); break;
        case 2: o2_metadata_unlock_input(); break;
        default: break;
        }
        const u32 current = dialog(m), stage = m.r8(current + 35);
        u32 caption = 48;
        if (stage == 1) caption += m.r8(current + 33);
        else if (stage == 0 || stage == 2) {
            const u32 group = static_cast<u32>(m.r16s(current + kCategory)), selection = m.r8(current + 31);
            const u32 mapping = m.r32(current + 200 + 4 * group);
            caption += m.r8(current + 36 + 16 * group + m.r8(mapping + selection - 1));
        }
        if (stage == 2) {
            const bool unlocked = (m.r32(scene(m) + 292) & 16) != 0;
            if (caption == 62 || caption == 63) caption = unlocked ? 93 : 88;
            if (caption == 67) caption = unlocked ? 94 : 89;
        } else {
            if (caption == 62 && m.r8(current + 181) == 1) caption = 91;
            if (caption == 63 && m.r8(dialog(m) + 181) == 1) caption = 92;
            if (caption == 67 && m.r8(dialog(m) + 180) == 1) caption = 90;
        }
        m.w8(flags(m) + 7, caption);
    }
    if (pressed(m, 32, 0x5F)) {
        sound_play(59); o2_entry_enter(0, 1); m.w8(dialog(m), 0); m.w8(dialog(m) + 1, 0); m.w32(dialog(m) + kPage, 0); m.w32(scene(m) + 300, 0);
    }
    if (m.r8(dialog(m) + kCompact) == 1) o2_metadata_connectors(frame);
    for (u32 direction : {8u, 4u}) if (repeat(m, direction)) {
        if (focus(m) < 3 && m.r32(dialog(m) + kCategory) != 0x00010002u) sound_play(61);
        if (focus(m) == 0) {
            m.w32(dialog(m) + kPage, 0); o2_entry_reset(2, 2); o2_entry_reset(2, 3); o2_group_set(2, 12);
            m.w8(flags(m) + 7, 0); m.w8(flags(m) + 20, 0);
        }
        if (focus(m) == 1) o2_metadata_layout_toggle();
        if (focus(m) == 2) { const u32 current = dialog(m); m.w8(current + kPreset, 1 - m.r8(current + kPreset)); }
    }
    if (focus(m) != 2) m.w8(dialog(m) + kPreset, 0);
    m.w8(flags(m) + 1, m.r8(dialog(m) + kFocus));
    m.w8(flags(m) + 59, category(m) == 2 ? 1 : 0);
    m.w8(flags(m) + 2, focus(m) == 0 ? 0 : 1);
    auto choiceFlag = [&](u32 flag, u32 field, u32 value, s32 row) {
        const u32 current = dialog(m);
        m.w8(flags(m) + flag, m.r8(current + field) == value ? (m.r16s(current + kFocus) == row ? 2 : 0) : 1);
    };
    choiceFlag(3, kCompact, 0, 1); choiceFlag(4, kCompact, 1, 1);
    choiceFlag(5, kPreset, 0, 2); choiceFlag(6, kPreset, 1, 2);
    m.w8(flags(m) + 21, 0);
    if (focus(m) == 0) m.w8(flags(m) + 21, 11);
    if (focus(m) == 1) m.w8(flags(m) + 21, m.r8(dialog(m) + kCompact) == 0 ? 12 : 13);
    if (focus(m) == 2) m.w8(flags(m) + 21, m.r8(dialog(m) + kPreset) != 0 ? 16 : (m.r32(scene(m) + 292) & 16) ? 10 : 9);
    if (focus(m) < 3) m.w8(flags(m) + 7, 0);
    return focus(m) < 3 ? flags(m) : 0;
}

u32 Game::o2_metadata_page_tick(u32 inheritedOffset) {
    cover(0x800D8CA8u);
    Frame frame(*this, 32);
    auto reset = [&] { m.w32(dialog(m) + kPage, 0); o2_entry_reset(2, 2); o2_entry_reset(2, 3); o2_group_set(2, 12); };
    if (m.r16(scene(m) + 342) & 0xB) { m.w32(dialog(m) + 224, 1); reset(); }
    if (m.r32(dialog(m) + kPage) == 0) o2_metadata_font_dispatch();
    if (m.r32(dialog(m) + kPage) == 1) {
        if (m.r8(dialog(m) + kCompact) == 0) o2_metadata_icons_draw(inheritedOffset);
        if (m.r8(dialog(m) + kCompact) == 1) o2_metadata_labels_draw();
    }
    if (m.r32(dialog(m) + kPage) == 0) o2_metadata_summary_input();
    else if (m.r32(dialog(m) + kPage) == 1) o2_metadata_detail_input();
    if (m.r32(dialog(m) + 224) != 0) {
        if (m.r16s(scene(m) + 342) == 0) {
            constexpr u32 masks[] = {4, 2, 16, 1, 8, 32}, groups[] = {0, 0, 1, 2, 3, 5};
            for (u32 i = 0; i < 6; ++i) if (m.r32(scene(m) + 292) & masks[i]) m.w16(dialog(m) + kCategory, groups[i]);
            if (category(m) < 4) { m.w32(dialog(m) + 228, 1); m.w8(flags(m) + 11, 4); m.w32(dialog(m) + 224, 0); }
            m.w32(dialog(m) + 232, m.r32(scene(m) + 292));
            if (m.r32(scene(m) + 292) == 0) m.w16(dialog(m) + kCategory, 4);
            return 0;
        }
        if (m.r16(scene(m) + 342) & 8) m.w8(flags(m) + 11, 5);
    } else if (m.r16s(scene(m) + 342) != 0 || m.r32(scene(m) + 292) != m.r32(dialog(m) + 232)) m.w32(dialog(m) + 224, 1);
    m.w32(dialog(m) + 232, m.r32(scene(m) + 292));
    if (m.r32(scene(m) + 292) == 0) m.w16(dialog(m) + kCategory, 4);
    if (m.r32(dialog(m) + 228) != 0) {
        reset();
        if (category(m) < 4) o2_upload_tim(m.r32(m.r32(m.r32(kMenu) + 444) + 4 * static_cast<u32>(category(m))));
        m.w8(flags(m) + 11, 4); m.w8(dialog(m) + 31, 0); m.w32(dialog(m) + 228, 0);
        for (u32 field : {30u, 34u, 35u, 32u, 33u}) m.w8(dialog(m) + field, 0);
        m.w16(dialog(m) + kFocus, 0); o2_campaign_presets();
        m.w8(flags(m) + 62, 0); m.w8(flags(m) + 63, 0);
        o2_fade_set(1, 3, -1, -1); o2_fade_set(2, 3, -1, -1);
        return m.r32(m.r32(kScreens) + 88) + 4;
    }
    const u32 group = m.r8(dialog(m) + kCategory);
    m.w8(flags(m) + 11, group);
    return group;
}
}

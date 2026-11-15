#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDialog = 0x80112DDCu, kScreens = 0x80112DC0u;
constexpr u32 kCategory = 12, kLabelSelection = 31, kLabelChoice = 33, kEditStage = 35;
u32 dialog(Memory& m) { return m.r32(kDialog); }
u32 scene(Memory& m) { return m.r32(g::kScene); }
u32 flags(Memory& m) { return m.r32(m.r32(kScreens) + 92); }
bool pressed(Memory& m, u32 button, u32 blocked) {
    const u32 state = scene(m); return (m.r32(state + 300) & button) && !(m.r32(state + 296) & blocked);
}
}

void Game::o2_metadata_edit_close(bool clearPressed) {
    m.w8(dialog(m) + kEditStage, 0);
    o2_fade_set(1, 3, -1, -1); o2_fade_set(2, 3, -1, -1);
    if (clearPressed) m.w32(scene(m) + 300, 0);
}

void Game::o2_metadata_presence_flags() {
    for (u32 i = 10; i < 31; ++i) m.w8(flags(m) + i, 0);
    m.w8(flags(m) + 33, 0); m.w8(flags(m) + 35, 0);
    for (u32 i = 0; i < 15; ++i) {
        const u32 current = dialog(m), category = static_cast<u32>(m.r16s(current + kCategory));
        const u32 mapping = m.r32(current + 200 + 4 * category);
        const u32 label = m.r8(current + 36 + 16 * category + m.r8(mapping + i));
        u32 flag = label + 10;
        if (flag == 21) flag = 33;
        if (flag == 11) flag = 35;
        m.w8(flags(m) + flag, 1);
    }
    for (u32 i = 0; i < 4; ++i) {
        const u32 current = dialog(m), category = static_cast<u32>(m.r16s(current + kCategory));
        const u32 palette = m.r8(current + 132 + 4 * category + i);
        if (palette >= 8) continue;
        if (palette == 0) { m.w8(flags(m) + 10, 1); m.w8(flags(m) + 35, 1); }
        else { m.w8(flags(m) + palette + 10, 1); m.w8(flags(m) + palette + 11, 1); }
    }
}

void Game::o2_metadata_unlock_input() {
    cover(0x800D5D64u);
    Frame frame(*this, 24);
    const u32 original = dialog(m), choice = m.r8(original + kLabelChoice);
    const u32 unlock = choice == 14 || choice == 15 ? 181 : choice == 19 ? 180 : 0;
    if (unlock) {
        if (pressed(m, 16, 0x6F)) { m.w8(original + unlock, 1); sound_play(60); }
        if (pressed(m, 32, 0x5F)) { sound_play(60); m.w8(dialog(m) + unlock, 0); }
    }
    if (pressed(m, 16, 0x6F) || pressed(m, 32, 0x5F)) o2_metadata_edit_close(true);
    o2_metadata_presence_flags();
}

void Game::o2_metadata_label_edit() {
    cover(0x800D60FCu);
    Frame frame(*this, 40);
    const u32 original = dialog(m), category = static_cast<u32>(m.r16s(original + kCategory));
    const u32 labels = original + 36 + 16 * category, palettes = original + 132 + 4 * category;
    if (pressed(m, 32, 0x5F)) { sound_play(59); o2_metadata_edit_close(true); }
    if (m.r32(scene(m) + 304) & 2) {
        sound_play(61); const u32 current = dialog(m), choice = m.r8(current + kLabelChoice);
        m.w8(current + kLabelChoice, choice < 20 ? choice + 1 : 0);
    }
    if (m.r32(scene(m) + 304) & 1) {
        sound_play(61); const u32 current = dialog(m), choice = m.r8(current + kLabelChoice);
        m.w8(current + kLabelChoice, choice ? choice - 1 : 20);
    }
    if (pressed(m, 16, 0x6F)) {
        sound_play(60);
        for (u32 i = 0; i < 15; ++i)
            if (m.r8(labels + i) == m.r8(dialog(m) + kLabelChoice)) m.w8(labels + i, 20);
        for (u32 i = 0; i < 4; ++i) {
            const u32 choice = m.r8(dialog(m) + kLabelChoice), palette = m.r8(palettes + i);
            if (palette == (choice & 0xFE) && palette < 8) m.w8(palettes + i, 12);
        }
        u32 current = dialog(m), currentCategory = static_cast<u32>(m.r16s(current + kCategory));
        const u32 mapping = m.r32(current + 200 + 4 * currentCategory);
        const u32 index = m.r8(mapping + m.r8(current + kLabelSelection) - 1);
        m.w8(labels + index, m.r8(current + kLabelChoice));
        current = dialog(m); const u32 choice = m.r8(current + kLabelChoice);
        if (choice == 14 || choice == 19 || choice == 15) m.w8(dialog(m) + kEditStage, 2);
        else o2_metadata_edit_close(false);
    }
    u32 current = dialog(m);
    m.w8(flags(m) + 9, m.r8(current + kEditStage) ? m.r8(current + kLabelChoice) + 1 : 0);
    current = dialog(m); m.w8(flags(m) + 7, m.r8(current + kLabelChoice) + 48);
    o2_metadata_presence_flags();
}

void Game::o2_metadata_label_input() {
    cover(0x800D65BCu);
    Frame frame(*this, 32);
    const s32 category = m.r16s(dialog(m) + kCategory);
    if (category >= 0 && category <= 2 && pressed(m, 16, 0x2F)) {
        if (category == 1) { o2_fade_set(1, 1, 0, 10); sound_play(60); }
        else { sound_play(60); o2_fade_set(1, 1, 0, 10); }
    }
    if (category == 3) {
        auto transfer = [&](u32 selected, u32 from, u32 to) {
            if (m.r8(dialog(m) + kLabelSelection) != selected) return;
            o2_fade_set(from, 3, -1, -1); o2_fade_set(to, 1, 0, 10);
        };
        if (m.r32(scene(m) + 304) & 2) {
            sound_play(61); transfer(2,4,6); transfer(4,6,4); transfer(11,5,7); transfer(13,7,5);
        }
        if (m.r32(scene(m) + 304) & 1) {
            sound_play(61); transfer(5,4,6); transfer(3,6,4); transfer(12,7,5);
        }
        if (pressed(m, 16, 0x2F)) {
            sound_play(60);
            if (m.r8(dialog(m) + kLabelSelection) - 6u >= 4) o2_fade_set(1, 1, 0, 20);
            if (m.r8(dialog(m) + kLabelSelection) - 6u < 4) o2_fade_set(2, 1, 0, 20);
        }
    }
    if (m.r32(scene(m) + 304) & 2) {
        const u32 current = dialog(m), currentCategory = static_cast<u32>(m.r16s(current + kCategory));
        const u32 selected = m.r8(current + kLabelSelection);
        if (selected < m.r8(0x8010AC5Cu + currentCategory)) m.w8(current + kLabelSelection, selected + 1);
        else { m.w8(current + kLabelSelection, 0); m.w16(dialog(m) + 14, 0); }
        sound_play(61);
    }
    if (m.r32(scene(m) + 304) & 1) {
        sound_play(61); const u32 current = dialog(m), selected = m.r8(current + kLabelSelection);
        if (selected >= 2) m.w8(current + kLabelSelection, selected - 1);
        else { m.w8(current + kLabelSelection, 0); m.w16(dialog(m) + 14, 2); }
    }
    if (pressed(m, 16, 0x2F)) {
        sound_play(60); m.w8(dialog(m) + kEditStage, 1);
        const u32 current = dialog(m), currentCategory = static_cast<u32>(m.r16s(current + kCategory));
        const u32 mapping = m.r32(current + 200 + 4 * currentCategory);
        const u32 index = m.r8(mapping + m.r8(current + kLabelSelection) - 1);
        m.w8(current + kLabelChoice, m.r8(current + 36 + 16 * currentCategory + index));
    }
    m.w8(flags(m) + 8, m.r8(dialog(m) + kLabelSelection));
}
void Game::o2_metadata_icon_presence(u32 labelPairFlag) {
    for (u32 i = 10; i < 18; ++i) m.w8(flags(m) + i, 0);
    const u32 initial = dialog(m), category = static_cast<u32>(m.r16s(initial + kCategory));
    if (category == 1) {
        u32 flag = (m.r8(initial + 139) >> 1) + 10;
        if (flag == 11) flag = 17;
        m.w8(flags(m) + flag, 1);
    } else for (u32 i = 0; i < 4; ++i) {
        const u32 current = dialog(m), currentCategory = static_cast<u32>(m.r16s(current + kCategory));
        u32 flag = (m.r8(current + 132 + 4 * currentCategory + i) >> 1) + 10;
        if (flag == 11) flag = 17;
        m.w8(flags(m) + flag, 1);
    }
    for (u32 i = 0; ; ++i) {
        const u32 current = dialog(m), currentCategory = static_cast<u32>(m.r16s(current + kCategory));
        if (i >= m.r8(0x8010AC5Cu + currentCategory)) break;
        const u32 mapping = m.r32(current + 200 + 4 * currentCategory);
        const u32 label = m.r8(current + 36 + 16 * currentCategory + m.r8(mapping + i));
        if (label >= 8) continue;
        u32 flag = (label >> 1) + 10;
        if (flag == 11) flag = labelPairFlag;
        m.w8(flags(m) + flag, 1);
    }
}

u32 Game::o2_metadata_icon_option() {
    cover(0x800D6B3Cu);
    Frame frame(*this, 32);
    const u32 original = dialog(m), category = static_cast<u32>(m.r16s(original + kCategory));
    const u32 option = original + 156 + 4 * category + m.r8(original + 30) - 1;
    if (pressed(m, 32, 0x5F)) {
        m.w8(original + 34, 1); m.w8(flags(m) + 62, 0); m.w8(flags(m) + 63, 0);
        sound_play(60); m.w32(scene(m) + 300, 0); return 0;
    }
    if ((m.r32(scene(m) + 304) & 3) && ((m.r8(dialog(m) + 30) - 1u) & 1) == 0) {
        sound_play(61); m.w8(option, 1 - m.r8(option)); m.w32(scene(m) + 304, 0);
    }
    if ((m.r32(scene(m) + 304) & 12) && (static_cast<s32>(m.r8(dialog(m) + 30)) - 1) % 2 == 1) {
        sound_play(61); m.w8(option, 1 - m.r8(option)); m.w32(scene(m) + 304, 0);
    }
    if (pressed(m, 16, 0x6F)) {
        m.w8(dialog(m) + 34, 0);
        if (m.r8(dialog(m) + 30) - 1u < 2) o2_fade_set(1, 3, -1, -1);
        if (m.r8(dialog(m) + 30) - 3u < 2) o2_fade_set(2, 3, -1, -1);
        o2_fade_set(1, 3, -1, -1); o2_fade_set(2, 3, -1, -1);
        m.w8(flags(m) + 62, 0); m.w8(flags(m) + 63, 0); sound_play(60); return 0;
    }
    m.w8(flags(m) + 7, 5 * m.r8(dialog(m) + 32) + 18);
    const s32 currentCategory = m.r16s(dialog(m) + kCategory);
    auto base_flag = [&] {
        return currentCategory == 1 || currentCategory == 2 ? 15u
            : static_cast<u32>(7 * ((static_cast<s32>(m.r8(dialog(m) + 30)) - 1) % 2) + 1);
    };
    if (m.r8(option) == 0) {
        m.w8(flags(m) + 62, base_flag());
        m.w8(flags(m) + 63, base_flag() + 1 + m.r8(dialog(m) + 32));
    } else {
        m.w8(flags(m) + 63, base_flag());
        m.w8(flags(m) + 62, base_flag() + 1 + m.r8(dialog(m) + 32));
    }
    o2_metadata_icon_presence(35);
    return 1;
}

void Game::o2_metadata_icon_edit() {
    cover(0x800D7194u);
    Frame frame(*this, 32);
    const u32 original = dialog(m), category = static_cast<u32>(m.r16s(original + kCategory));
    const u32 palettes = original + 132 + 4 * category, labels = original + 36 + 16 * category;
    if (pressed(m, 32, 0x5F)) {
        m.w8(original + 34, 0); o2_fade_set(1, 3, -1, -1); o2_fade_set(2, 3, -1, -1);
        sound_play(59); m.w32(scene(m) + 300, 0);
    }
    if (m.r32(scene(m) + 304) & 2) {
        sound_play(61); const u32 current = dialog(m), choice = m.r8(current + 32);
        m.w8(current + 32, choice < 6 ? choice + 1 : 0);
    }
    if (m.r32(scene(m) + 304) & 1) {
        sound_play(61); const u32 current = dialog(m), choice = m.r8(current + 32);
        m.w8(current + 32, choice ? choice - 1 : 6);
    }
    if (pressed(m, 16, 0x6F)) {
        sound_play(60);
        for (u32 i = 0; i < 15; ++i)
            if (m.r8(palettes + i) == 2 * m.r8(dialog(m) + 32)) m.w8(palettes + i, 12);
        for (u32 i = 0; ; ++i) {
            u32 current = dialog(m), currentCategory = static_cast<u32>(m.r16s(current + kCategory));
            if (i >= m.r8(0x8010AC5Cu + currentCategory)) break;
            const u32 pair = 2 * m.r8(current + 32);
            if (pair >= 8) continue;
            u32 mapping = m.r32(current + 200 + 4 * currentCategory), slot = labels + m.r8(mapping + i);
            if (m.r8(slot) == pair) m.w8(slot, 20);
            current = dialog(m); currentCategory = static_cast<u32>(m.r16s(current + kCategory));
            mapping = m.r32(current + 200 + 4 * currentCategory); slot = labels + m.r8(mapping + i);
            if (m.r8(slot) == 2 * m.r8(current + 32) + 1) m.w8(slot, 20);
        }
        u32 current = dialog(m); m.w8(palettes + m.r8(current + 30) - 1, 2 * m.r8(current + 32));
        current = dialog(m); const u32 choice = m.r8(current + 32);
        if (choice == 0 || choice == 3 || choice == 4) m.w8(dialog(m) + 34, 2);
        else { m.w8(current + 34, 0); o2_fade_set(1, 3, -1, -1); o2_fade_set(2, 3, -1, -1); }
    }
    u32 current = dialog(m);
    m.w8(flags(m) + 9, m.r8(current + 34) ? m.r8(current + 32) + 1 : 0);
    const u32 choice = m.r8(dialog(m) + 32);
    m.w8(flags(m) + 7, choice == 6 ? 68 : 5 * choice + 17);
    o2_metadata_icon_presence(17);
}
}

#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDialog = 0x80112DDCu, kScreens = 0x80112DC0u;
u32 dialog(Memory& m) { return m.r32(kDialog); }
u32 scene(Memory& m) { return m.r32(g::kScene); }
u32 flags(Memory& m) { return m.r32(m.r32(kScreens) + 92); }
}

u32 Game::o2_metadata_icon_input() {
    cover(0x800D7750u);
    Frame frame(*this, 32);
    const s32 category = m.r16s(dialog(m) + 12);
    auto position = [&](u32 selection, s32 x) {
        if (m.r8(dialog(m) + 30) == selection)
            o2_entry_position(2, category == 0 ? 9 : 11, static_cast<u32>(x), 0);
    };
    auto begin = [&] {
        m.w8(dialog(m) + 34, 1);
        const u32 current = dialog(m), group = static_cast<u32>(m.r16s(current + 12));
        m.w8(current + 32, m.r8(current + 132 + 4 * group + m.r8(current + 30) - 1) >> 1);
    };
    auto confirm = [&] {
        const u32 state = scene(m);
        return (m.r32(state + 300) & 16) && !(m.r32(state + 296) & 0x2F);
    };
    if (category == 0 || category == 3) {
        if (m.r32(scene(m) + 304) & 2) {
            sound_play(61);
            const u32 current = dialog(m), choice = m.r8(current + 30);
            if (choice < 4) m.w8(current + 30, choice + 1);
            else { m.w8(current + 30, 0); m.w16(current + 14, 0); }
        }
        position(0, 0); position(1, 40); position(3, -40);
        if (m.r32(scene(m) + 304) & 1) {
            sound_play(61);
            const u32 current = dialog(m), choice = m.r8(current + 30);
            if (choice >= 2) m.w8(current + 30, choice - 1);
            else { m.w8(current + 30, 0); m.w16(current + 14, 2); }
        }
        position(0, 0); position(2, 40); position(4, -40);
        if (confirm()) {
            sound_play(60);
            const u32 choice = m.r8(dialog(m) + 30);
            if (choice - 1u < 2) o2_fade_set(1, 1, 0, 10);
            if (m.r8(dialog(m) + 30) - 3u < 2) o2_fade_set(2, 1, 0, 10);
            begin();
        }
    } else if (category == 1) {
        if (m.r32(scene(m) + 304) & 2) {
            sound_play(61); m.w8(dialog(m) + 30, 0); m.w16(dialog(m) + 14, 0);
        }
        if (m.r32(scene(m) + 304) & 1) {
            sound_play(61); m.w8(dialog(m) + 30, 0); m.w16(dialog(m) + 14, 2);
        }
        if (confirm()) { sound_play(60); o2_fade_set(1, 1, 0, 10); begin(); }
    }
    const u32 current = dialog(m), group = static_cast<u32>(m.r16s(current + 12));
    const u32 palette = m.r8(current + 132 + 4 * group + m.r8(current + 30) - 1);
    m.w8(flags(m) + 7, palette == 12 ? 68 : 5 * (palette >> 1) + 17);
    const u32 target = dialog(m), selection = m.r8(target + 30);
    const bool lastSlot = m.r16s(target + 12) == 1 && selection == 4;
    m.w8(flags(m) + 8, lastSlot ? 1 : selection);
    return lastSlot ? flags(m) : selection;
}
}

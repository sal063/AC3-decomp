#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDialog = 0x80112DDCu, kMenu = 0x80112DC4u;
constexpr u32 kFirstLabels = 52, kSecondLabels = 68, kThirdLabels = 84;
constexpr u32 kFirstPalette = 136, kThirdPalette = 144;
constexpr u32 kFirstOptions = 160, kThirdOptions = 168;
constexpr u32 kUnlockFirst = 180, kUnlockSecond = 181;
constexpr u32 kColourGrey = 236, kColourNeutral = 240, kColourDark = 244, kColourLight = 248;
}

void Game::o2_dialog_preset_full() {
    cover(0x800D180Cu);
    constexpr u8 labels[] = {4, 7, 8, 11, 10, 9, 20, 5, 6, 14, 16, 12, 13, 18, 19};
    const u32 table = m.r32(kDialog) + 36;
    for (u32 i = 0; i < sizeof(labels); ++i) m.w8(table + i, labels[i]);
    const u32 palette = m.r32(kDialog) + 132;
    m.w8(palette, 0); m.w8(palette + 1, 2); m.w8(palette + 2, 8); m.w8(palette + 3, 10);
    const u32 options = m.r32(kDialog) + 156;
    m.w8(options, 1); m.w8(options + 1, 0); m.w8(options + 2, 0); m.w8(options + 3, 0);
    m.w8(m.r32(kDialog) + kUnlockSecond, 1);
    m.w8(m.r32(kDialog) + kUnlockFirst, 1);
}

void Game::o2_dialog_preset_first() {
    cover(0x800D18E4u);
    constexpr u8 labels[] = {20, 14, 1, 5, 4, 0, 20, 20, 13, 16, 7, 12, 6, 20, 20};
    const u32 table = m.r32(kDialog) + kFirstLabels;
    for (u32 i = 0; i < sizeof(labels); ++i) m.w8(table + i, labels[i]);
    const u32 palette = m.r32(kDialog) + kFirstPalette;
    m.w8(palette, 12); m.w8(palette + 1, 12); m.w8(palette + 2, 12); m.w8(palette + 3, 2);
    const u32 options = m.r32(kDialog) + kFirstOptions;
    for (u32 i = 0; i < 4; ++i) m.w8(options + i, 0);
    m.w8(m.r32(kDialog) + kUnlockSecond, 1);
    m.w8(m.r32(kDialog) + kUnlockFirst, 1);
}

void Game::o2_dialog_preset_second() {
    cover(0x800D19A8u);
    constexpr u8 labels[] = {4, 7, 1, 3, 2, 0, 20, 5, 6, 14, 16, 12, 13, 20, 19};
    const u32 table = m.r32(kDialog) + kSecondLabels;
    for (u32 i = 0; i < sizeof(labels); ++i) m.w8(table + i, labels[i]);
    m.w8(m.r32(kDialog) + kUnlockSecond, 1);
    m.w8(m.r32(kDialog) + kUnlockFirst, 1);
}

void Game::o2_dialog_preset_third() {
    cover(0x800D1A3Cu);
    constexpr u8 labels[] = {20, 20, 8, 11, 10, 9, 20, 20, 20, 16, 13, 14, 12, 20, 19};
    const u32 table = m.r32(kDialog) + kThirdLabels;
    for (u32 i = 0; i < sizeof(labels); ++i) m.w8(table + i, labels[i]);
    const u32 palette = m.r32(kDialog) + kThirdPalette;
    m.w8(palette, 6); m.w8(palette + 1, 4); m.w8(palette + 2, 0); m.w8(palette + 3, 2);
    const u32 options = m.r32(kDialog) + kThirdOptions;
    m.w8(options, 0); m.w8(options + 1, 0); m.w8(options + 2, 1); m.w8(options + 3, 0);
    m.w8(m.r32(kDialog) + kUnlockSecond, 1);
    m.w8(m.r32(kDialog) + kUnlockFirst, 1);
}

void Game::o2_dialog_colours_reset() {
    cover(0x800D911Cu);
    for (u32 i = 0; i < 4; ++i) {
        m.w8(m.r32(kDialog) + kColourGrey + i, 136);
        m.w8(m.r32(kDialog) + kColourNeutral + i, 128);
        m.w8(m.r32(kDialog) + kColourDark + i, 16);
        m.w8(m.r32(kDialog) + kColourLight + i, 240);
    }
    m.w8(m.r32(kDialog) + 282, 240);
    m.w8(m.r32(kDialog) + 283, 110);
    m.w8(m.r32(kDialog) + 284, 0);
    m.w8(m.r32(kDialog) + 286, 240);
    m.w8(m.r32(kDialog) + 287, 110);
    m.w8(m.r32(kDialog) + 288, 0);
    const u32 selection = m.r32(kDialog);
    m.w8(selection + 289, 0); m.w8(selection + 285, 0);
    const u32 dialog = m.r32(kDialog);
    const u32 stage = m.r8(dialog + 1);
    m.w32(dialog + 252, 0);
    m.w8(dialog + 1, stage + 1);
}

void Game::o2_dialog_page_after_fade() {
    cover(0x800D2FA8u);
    Frame frame(*this, 24);
    if (o2_fade_state(1) != 0 || m.r16s(m.r32(kMenu) + 454) != 0) return;
    const u32 dialog = m.r32(kDialog);
    const u32 choice = m.r8(dialog + 5);
    if (choice == 0) {
        m.w8(dialog + 1, 1);
        o2_fade_set(1, 1, 0, 40);
        o2_group_set(2, 1);
        return;
    }
    if (choice == 1) m.w8(dialog, 7);
    u32 current = m.r32(kDialog);
    u32 selected = m.r8(current + 5);
    if (selected == 2) {
        m.w8(current, 8);
        current = m.r32(kDialog);
        selected = m.r8(current + 5);
    }
    if (selected == 3) m.w8(current, 9);
    m.w8(m.r32(kDialog) + 1, 0);
}
}

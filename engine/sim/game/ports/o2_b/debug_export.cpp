#include "sim/game/game.hpp"

namespace ac3::sim {
void Game::o2_debug_frame_export() {
    cover(0x800E5ED0u);
    Frame frame(*this, 32);
    const u32 rect = frame.at(16), image = 0x80200000u;
    m.w32(rect, m.r32(0x800CCE1Cu));
    m.w32(rect + 4, m.r32(0x800CCE20u));
    m.w32(image, 16); m.w32(image + 4, 2);
    m.w32(image + 8, 0x2580C); m.w32(image + 12, 0);
    m.w32(image + 16, 0x01E00140);
    m.w16(rect + 2, 0);
    hw->store_image(rect, image + 20);
    const u32 handle = hw->file_open(0x800CCE24u, 0x202);
    hw->file_write(handle, image, 0x4B014);
    hw->file_close(handle);
}

void Game::o2_debug_palette_export(u32 index) {
    cover(0x800EC11Cu);
    Frame frame(*this, 152);
    const u32 header = frame.at(16), pixels = frame.at(24), palette = frame.at(40);
    m.w32(header, 16); m.w32(header + 4, 9);
    m.w32(palette, 0x20C); m.w16(palette + 4, 0x300);
    m.w16(palette + 6, 5); m.w16(palette + 8, 0x100); m.w16(palette + 10, 1);
    m.w32(pixels, 0x4C); m.w16(pixels + 4, 0x380);
    m.w16(pixels + 6, 0x100); m.w16(pixels + 8, 4); m.w16(pixels + 10, 8);
    const u32 name = frame.at(56);
    for (u32 offset = 0; offset < 24; offset += 4) m.w32(name + offset, m.r32(0x800CCE9Cu + offset));
    m.w16(name + 24, m.r16(0x800CCEB4u)); m.w8(name + 26, m.r8(0x800CCEB6u));
    const u32 digits = name + libc_strlen(name);
    const s32 number = static_cast<s32>(index);
    const s32 thousands = number / 1000, hundreds = number / 100, tens = number / 10;
    m.w8(digits, static_cast<u32>(thousands) + 48);
    m.w8(digits + 1, static_cast<u32>(hundreds - 10 * thousands) + 48);
    m.w8(digits + 2, static_cast<u32>(tens - 10 * hundreds) + 48);
    m.w8(digits + 3, index - 10u * static_cast<u32>(tens) + 48);
    m.w8(digits + 4, 0);
    libc_strcat(name, 0x800CCEB8u);
    const u32 handle = hw->file_open(name, 0x202);
    hw->file_write(handle, header, 8);
    hw->file_write(handle, palette, 12);
    constexpr u32 kPaletteManager = 0x80112DFCu, kAtlas = 428, kTiles = 516;
    hw->file_write(handle, m.r32(m.r32(kPaletteManager) + kAtlas) + 4, 512);
    hw->file_write(handle, pixels, 12);
    const u32 atlas = m.r32(m.r32(kPaletteManager) + kAtlas);
    hw->file_write(handle, m.r32(atlas + kTiles + 4 * index), 64);
    hw->file_close(handle);
}
}

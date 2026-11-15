#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenuState = 0x80112DC4u;
constexpr u32 kFrontendLoader = 0x800BCE50u;
constexpr u32 kUlzMagic = 0x1A7A6C55u;
constexpr u32 kTimWorkspaceBytes = 0x9000u;
constexpr u32 kSavedPalette = 0xD0u, kWorkingPalette = 0xD4u;
constexpr u32 kPaletteRect = 0xD8u;
}

u32 Game::o2_upload_tim_positioned(u32 tim, u32 imageX, u32 imageY, u32 paletteX, u32 paletteY) {
    cover(0x800ED000u);
    Frame frame(*this, 56);
    const u32 workspace = mem_alloc(kTimWorkspaceBytes);
    u32 source = tim;
    if (m.r32(tim) == kUlzMagic) {
        ulz_decode(tim, workspace);
        source = workspace;
    }
    m.w32(frame.at(16), static_cast<u32>(static_cast<s16>(paletteY)));
    load_tim_at(source,
                static_cast<u32>(static_cast<s16>(imageX)), static_cast<u32>(static_cast<s16>(imageY)),
                static_cast<u32>(static_cast<s16>(paletteX)), static_cast<u32>(static_cast<s16>(paletteY)));
    return heap_set(workspace);
}

u32 Game::o2_restore_frontend_heap() {
    cover(0x800ECD20u);
    Frame frame(*this, 0x18);
    return heap_set(m.r32(m.r32(kFrontendLoader) + 0x60));
}

u32 Game::o2_upload_pack_sized(u32 pack, u32 bytes) {
    cover(0x800ECE38u);
    Frame frame(*this, 0x20);
    const u32 workspace = mem_alloc(bytes);
    u32 directory = pack;
    if (m.r32(pack) == kUlzMagic) {
        ulz_decode(pack, workspace);
        directory = workspace;
    }
    const u32 entries = ld_relocate_dir(directory);
    ld_upload_tims(entries, m.r32s(directory));
    heap_set(workspace);
    return 1;
}

u32 Game::o2_upload_packed_tims(u32 pack) {
    cover(0x800ECEC8u);
    Frame frame(*this, 0x28);
    const s32 count = m.r32s(pack);
    const u32 workspace = mem_alloc(kTimWorkspaceBytes);
    const u32 entries = ld_relocate_dir(pack);
    for (s32 i = 0; i < count; ++i) {
        u32 tim = m.r32(entries + 4 * static_cast<u32>(i));
        if (m.r32(tim) == kUlzMagic) {
            ulz_decode(tim, workspace);
            tim = workspace;
        }
        load_tim(tim);
    }
    heap_set(workspace);
    return 1;
}

u32 Game::o2_upload_menu_palette(u32 pack) {
    cover(0x800ED0D4u);
    Frame frame(*this, 0x18);
    o2_upload_pack_sized(pack, 0x10000);
    u32 menu = m.r32(kMenuState);
    hw->store_image(menu + kPaletteRect, m.r32(menu + kSavedPalette));
    for (u32 i = 0; i < 64; ++i) {
        menu = m.r32(kMenuState);
        m.w32(m.r32(menu + kWorkingPalette) + 4 * i, 0xCE73CE73u);
    }
    menu = m.r32(kMenuState);
    hw->load_image(menu + kPaletteRect, m.r32(menu + kWorkingPalette));
    return 0;
}

u32 Game::o2_find_halfword(u32 values, s32 value, s32 count) {
    cover(0x800EDC80u);
    const s16 needle = static_cast<s16>(value);
    for (s32 i = 0; i < count; ++i)
        if (m.r16s(values + 2 * static_cast<u32>(i)) == needle)
            return static_cast<u32>(i);
    return 0;
}
}

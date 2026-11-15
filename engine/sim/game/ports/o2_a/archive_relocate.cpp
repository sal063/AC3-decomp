#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kLoader = 0x800BCE50u, kDialog = 0x80112DDCu;
constexpr u32 kRecordSlot = 96, kArchive = 40, kLoadStage = 454;
constexpr u32 kImages = 468, kSecondImages = 472, kPalettes = 476;
bool load_pending(Memory& m) { return m.r32(m.r32(g::kScene) + 68) == 1; }
}

void Game::o2_frontend_archive_replace() {
    cover(0x800CE4D0u);
    Frame frame(*this, 24);
    heap_set(m.r32(m.r32(m.r32(kMenu) + 36)));
    const u32 loader = m.r32(kLoader);
    m.w32(loader + kRecordSlot, 115);
    const u32 buffer = mem_alloc(records_size(loader + kRecordSlot, 1));
    m.w32(m.r32(g::kScene) + 68, 1);
    request_records(m.r32(kLoader) + kRecordSlot, 1, buffer, 0x8009B904u);
}

void Game::o2_frontend_images_relocate() {
    cover(0x800CEA6Cu);
    Frame frame(*this, 32);
    if (load_pending(m)) return;
    const u32 archive = m.r32(m.r32(kLoader) + kRecordSlot);
    m.w32(m.r32(kMenu) + kArchive, archive);
    const u32 entries = ld_relocate_dir(archive);
    const u32 images = ld_relocate_dir(m.r32(entries));
    m.w32(m.r32(kMenu) + kImages, images);
    const u32 palettes = ld_relocate_dir(m.r32(entries + 4));
    const u32 menu = m.r32(kMenu);
    m.w32(menu + kPalettes, palettes); m.w16(menu + kLoadStage, 3);
}

void Game::o2_frontend_images_upload() {
    cover(0x800CE7ECu);
    Frame frame(*this, 32);
    if (load_pending(m)) return;
    const u32 archive = m.r32(m.r32(kLoader) + kRecordSlot);
    m.w32(m.r32(kMenu) + kArchive, archive);
    const u32 entries = ld_relocate_dir(archive);
    o2_upload_pack(m.r32(entries + 8));
    heap_set(m.r32(entries + 8));
    const u32 images = ld_relocate_dir(m.r32(entries));
    m.w32(m.r32(kMenu) + kImages, images);
    const u32 palettes = ld_relocate_dir(m.r32(entries + 4));
    const u32 menu = m.r32(kMenu);
    m.w32(menu + kPalettes, palettes); m.w16(menu + kLoadStage, 3);
}

void Game::o2_frontend_three_images_upload() {
    cover(0x800CE924u);
    Frame frame(*this, 32);
    if (load_pending(m)) return;
    const u32 archive = m.r32(m.r32(kLoader) + kRecordSlot);
    m.w32(m.r32(kMenu) + kArchive, archive);
    const u32 entries = ld_relocate_dir(archive);
    o2_upload_pack(m.r32(entries + 12));
    heap_set(m.r32(entries + 12));
    const u32 images = ld_relocate_dir(m.r32(entries));
    m.w32(m.r32(kMenu) + kImages, images);
    const u32 second = ld_relocate_dir(m.r32(entries + 4));
    m.w32(m.r32(kMenu) + kSecondImages, second);
    const u32 palettes = ld_relocate_dir(m.r32(entries + 8));
    const u32 menu = m.r32(kMenu);
    m.w32(menu + kPalettes, palettes); m.w16(menu + kLoadStage, 3);
}

void Game::o2_dialog_archive_ready() {
    cover(0x800DDFA4u);
    Frame frame(*this, 32);
    o2_entry_reset(2, 22);
    o2_group_set(2, 0x400000);
    if (load_pending(m)) return;
    const u32 archive = m.r32(m.r32(kLoader) + kRecordSlot);
    m.w32(m.r32(kMenu) + kArchive, archive);
    const u32 entries = ld_relocate_dir(archive);
    o2_upload_pack(m.r32(entries + 8));
    heap_set(m.r32(entries + 8));
    const u32 images = ld_relocate_dir(m.r32(entries));
    m.w32(m.r32(kMenu) + kImages, images);
    const u32 palettes = ld_relocate_dir(m.r32(entries + 4));
    m.w32(m.r32(kMenu) + kPalettes, palettes);
    const u32 dialog = m.r32(kDialog);
    m.w8(dialog + 1, m.r8(dialog + 1) + 1);
}

void Game::o2_dialog_archive_install() {
    cover(0x800CEB94u);
    Frame frame(*this, 24);
    if (load_pending(m)) return;
    if (!m.r8(m.r32(g::kScene) + 2399)) {
        m.w16(m.r32(kMenu) + kLoadStage, 3);
        return;
    }
    const u32 entries = ld_relocate_dir(m.r32(m.r32(kLoader) + kRecordSlot));
    m.w32(m.r32(kMenu) + 28, entries);
    const u32 scripts = ld_relocate_dir(m.r32(entries + 4));
    o2_set_scripts(scripts, 2);
    const u32 menu = m.r32(kMenu), current = m.r32(menu + 28);
    m.w32(menu + 152, m.r32(current + 8));
    const u32 saved = m.r32(menu + 28);
    m.w32(menu + 160, m.r32(saved + 16));
    m.w32(menu + 164, m.r32(saved + 20));
    const u32 stage = m.r16(menu + kLoadStage), last = m.r32(saved + 24);
    m.w16(menu + kLoadStage, stage + 1);
    m.w32(menu + 168, last);
}
}

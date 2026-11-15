#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kSelection = 0x80112E30u, kMenu = 0x80112DC4u, kFront = 0x80112DDCu;
constexpr u32 kRecords = 0x800BCE50u, kObjects = 0x800BCE5Cu, kModelTable = 0x80112C08u;
constexpr u32 kPending = 0x4001, kChoice = 0x4003, kSavedAsset = 0x4004;
void resetView(Memory& m) {
    const u32 menu = m.r32(kMenu);
    m.w32(menu + 0xFC, 0xFFF00000u);
    m.w16(menu + 0x100, 0xC00); m.w16(menu + 0x102, 0x2800); m.w16(menu + 0x104, 0);
}
void linkWorldOt(Memory& m) {
    const u32 worldOt = m.r32(m.r32(g::kWorld) + 16), menuOt = m.r32(m.r32(kMenu) + 8) + 12;
    m.w32(worldOt, (m.r32(worldOt) & 0xFF000000u) | (m.r32(menuOt) & 0xFFFFFFu));
    m.w32(menuOt, (m.r32(menuOt) & 0xFF000000u) | ((worldOt + 252) & 0xFFFFFFu));
}
}

u32 Game::o2_catalogue_model_ready() {
    cover(0x80108334u);
    Frame frame(*this, 32);
    if (!m.r8(m.r32(kSelection) + kPending)) return 0xFFFFFFFFu;
    if (m.r32(m.r32(g::kScene) + 0x44) == 1) return 0;
    if (o2_fade_state(2)) return 0xFFFFFFFFu;
    const u32 asset = m.r32(m.r32(kRecords) + 0x60);
    const u32 root = ld_relocate_dir(asset), pack = ld_relocate_dir(m.r32(root));
    o2_upload_menu_palette(m.r32(pack));
    ulz_decode(m.r32(pack + 4), m.r32(kSelection));
    const u32 model = m.r32(m.r32(g::kWorld) + 0x250);
    m.w32(model + 32, m.r32(kSelection));
    const u32 destination = m.r32(m.r32(m.r32(g::kWorld) + 0x250) + 32);
    ld_relocate_words(destination, destination + 4, 6);
    resetView(m);
    m.w8(m.r32(kSelection) + kPending, 0);
    o2_fade_set(2, 1, 0, 25); o2_fade_set(8, 1, 0, 25);
    heap_set(asset);
    return 1;
}

u32 Game::o2_catalogue_begin() {
    cover(0x80108B7Cu);
    Frame frame(*this, 24);
    m.w32(kSelection, mem_alloc(0x4008));
    m.w8(m.r32(kSelection) + 0x4000, 0); m.w8(m.r32(kSelection) + kPending, 1);
    m.w8(m.r32(kSelection) + 0x4002, 0); m.w8(m.r32(kSelection) + kChoice, 0);
    o2_panel_scene_alloc(); o2_panel_camera_init(); o2_selector_model_request();
    o2_upload_pack_sized(m.r32(m.r32(kMenu) + 0xA4), 0x10000);
    o2_entry_reset(2, 20); o2_group_set(2, 0x100000);
    for (u32 entry : {9u, 12u, 10u, 14u, 8u}) o2_entry_reset(0, entry);
    o2_group_set(0, 0x5701);
    m.w8(m.r32(kFront) + 1, 1);
    o2_fade_set(2, 0, 0, 0); o2_flag(1, 0);
    const u32 objects = m.r32(kObjects), first = m.r32(objects), third = m.r32(objects + 8);
    m.w32(first, 0); m.w32(third, 0); m.w8(first + 4, 2); m.w8(third + 4, 2);
    m.w8(first + 5, m.r8(first + 5) | 1);
    for (u32 object : {first, third}) {
        m.w32(object + 8, 0); m.w32(object + 12, 0); m.w32(object + 16, 0);
        m.w16(object + 24, 0); m.w16(object + 26, 0x1000); m.w16(object + 28, 0x1000);
    }
    m.w32(m.r32(m.r32(g::kWorld) + 0x28) + 0xC4, 1);
    return 1;
}

u32 Game::o2_catalogue_load_tick() {
    cover(0x80108D90u);
    Frame frame(*this, 24);
    if (o2_catalogue_model_ready() == 1) {
        m.w8(m.r32(kFront) + 1, 2);
        for (u32 fade : {1u, 3u, 11u, 5u, 8u}) o2_fade_set(fade, 1, 0, 25);
    }
    const u32 objects = m.r32(kObjects), background = m.r32(objects + 8);
    m.w32(background, m.r32(background) | 0x1C000);
    m.w32(m.r32(objects), 0);
    for (u32 flag : {1u, 3u, 2u, 4u}) o2_flag(flag, 0);
    return m.r32(m.r32(0x80112DC0u) + 0x5C) + 4;
}

u32 Game::o2_catalogue_choice_tick() {
    cover(0x801092BCu);
    Frame frame(*this, 24);
    if (!o2_fade_state(2)) {
        const u32 choice = m.r8(m.r32(kSelection) + kChoice);
        if (choice == 1) {
            m.w8(m.r32(kFront) + 1, 4); o2_group_set(2, 0x200000); return m.r32(kMenu) + 8;
        }
        if (choice == 2) {
            o2_entry_enter(0, 1);
            m.w8(m.r32(kFront), 0); m.w8(m.r32(kFront) + 1, 0);
            return heap_set(m.r32(kSelection));
        }
        return 2;
    }
    clear_otag_r(m.r32(m.r32(g::kWorld) + 16), 64); linkWorldOt(m);
    o2_selector_model_view(0);
    return o2_selector_model_draw(1);
}

u32 Game::o2_catalogue_details_begin() {
    cover(0x801093E8u);
    Frame frame(*this, 40);
    const u32 selection = m.r32(kSelection), entry = kModelTable + 8 * m.r8(selection + 0x4000);
    if (!m.r16s(entry + 4)) {
        m.w8(m.r32(kFront) + 1, 5);
        return o2_selector_texture_upload(0);
    }
    const u32 loading = m.r32(m.r32(g::kScene) + 0x44);
    if (loading) return loading;
    const u32 records = m.r32(kRecords), asset = m.r32(records + 0x60);
    m.w32(selection + kSavedAsset, asset);
    const u32 directory = ld_relocate_dir(asset);
    heap_set(m.r32(directory + 12));
    o2_upload_pack_sized(m.r32(directory + 8), 0x10000);
    heap_set(m.r32(directory + 8));
    const u32 models = mem_alloc(0x10000);
    ulz_decode(m.r32(directory), models);
    m.w32(records + 8, ld_relocate_dir(models));
    for (s32 index = 0; index < m.r32s(models); ++index) {
        const u32 model = m.r32(m.r32(records + 8) + 4 * static_cast<u32>(index));
        ld_relocate_words(model, model + 4, 6);
    }
    m.w32(records + 12, m.r32(directory + 4));
    m.w32(m.r32(m.r32(g::kWorld) + 0x250) + 12, m.r32(m.r32(kRecords) + 8));
    const u32 objects = m.r32(kObjects), first = m.r32(objects);
    m.w32(first, m.r32(first) | 0x1C000); m.w32(m.r32(objects + 8), 0);
    m.w8(m.r32(kFront) + 1, 5);
    return 5;
}

u32 Game::o2_catalogue_details_tick() {
    cover(0x80109588u);
    Frame frame(*this, 24);
    clear_otag_r(m.r32(m.r32(g::kWorld) + 16), 64); linkWorldOt(m);
    const u32 scene = m.r32(g::kScene);
    if ((m.r32(scene + 0x12C) & 32) && !(m.r32(scene + 0x128) & 0x5F)) {
        m.w8(m.r32(kFront) + 1, 6); return 6;
    }
    o2_selector_model_view(1);
    return o2_selector_model_draw(0);
}

u32 Game::o2_catalogue_details_end() {
    cover(0x80109660u);
    Frame frame(*this, 24);
    o2_group_set(2, 0x100000);
    const u32 selection = m.r32(kSelection), entry = kModelTable + 8 * m.r8(selection + 0x4000);
    if (m.r16s(entry + 4)) {
        const u32 objects = m.r32(kObjects), third = m.r32(objects + 8);
        m.w32(m.r32(objects), 0); m.w32(third, m.r32(third) | 0x1C000);
        heap_set(m.r32(selection + kSavedAsset)); resetView(m); o2_selector_model_request();
        m.w8(m.r32(kFront) + 1, 1); return 1;
    }
    for (u32 fade : {2u, 1u, 3u, 11u, 5u, 8u}) o2_fade_set(fade, 1, 0, 25);
    m.w8(m.r32(kFront) + 1, 2); return 2;
}
u32 Game::o2_catalogue_browse() {
    cover(0x80108E88u);
    Frame frame(*this, 32);
    auto pressed = [&] { return m.r32(m.r32(g::kScene) + 0x12C); };
    auto held = [&] { return m.r32(m.r32(g::kScene) + 0x128); };
    auto hide = [&](u32 fade) { o2_fade_set(fade, 3, 0xFFFFFFFFu, 0xFFFFFFFFu); };
    auto hideView = [&] { for (u32 fade : {2u, 8u, 1u, 3u, 11u, 5u}) hide(fade); };
    o2_flag(1, 0); o2_flag(2, (held() & 4) ? 1 : 0); o2_flag(3, (held() & 8) ? 1 : 0);
    o2_flag(4, m.r16s(m.r32(m.r32(kMenu) + 0x16C) + 0x6A) == 2 ? 1 : 0);
    if (pressed() & 8) {
        const u32 selection = m.r32(kSelection), id = m.r8(selection + 0x4000);
        if (id < 48) m.w8(selection + 0x4000, id + 1);
        o2_selector_model_request(); hide(2); hide(8);
    }
    if (pressed() & 4) {
        const u32 selection = m.r32(kSelection), id = m.r8(selection + 0x4000);
        if (id) m.w8(selection + 0x4000, id - 1);
        o2_selector_model_request(); hide(2); hide(8);
    }
    if ((pressed() & 16) && !(held() & 0x2F)) {
        const u32 entry = kModelTable + 8 * m.r8(m.r32(kSelection) + 0x4000);
        const s32 detailRecord = m.r16s(entry + 4);
        if (detailRecord) {
            const u32 record = m.r32(kRecords) + 0x60;
            m.w32(record, static_cast<u32>(detailRecord));
            const u32 destination = mem_alloc(records_size(record, 1));
            m.w32(m.r32(g::kScene) + 0x44, 1);
            request_records(m.r32(kRecords) + 0x60, 1, destination, 0x8009B904u);
        }
        m.w8(m.r32(kFront) + 1, 3); hideView();
        m.w8(m.r32(kSelection) + kChoice, 1);
    }
    o2_catalogue_model_ready();
    clear_otag_r(m.r32(m.r32(g::kWorld) + 16), 64); linkWorldOt(m);
    o2_selector_model_view(0); o2_selector_model_draw(1);
    if (!(pressed() & 32)) return 0;
    const u32 excluded = held() & 0x5F;
    if (excluded) return excluded;
    m.w8(m.r32(kFront) + 1, 3);
    for (u32 fade : {8u, 2u, 1u, 3u, 11u, 5u}) hide(fade);
    m.w8(m.r32(kSelection) + kChoice, 2);
    return 2;
}
}

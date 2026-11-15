#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDialog = 0x80112DDCu, kMenu = 0x80112DC4u;
constexpr u32 kSetupDelay = 624, kLoadStage = 454;
}

void Game::o2_dialog_metadata_tables() {
    o2_hangar_effect2_exit();
    const u32 dialog = m.r32(kDialog);
    constexpr u32 labelPointers[] = {0x8010A784u, 0x8010A794u, 0x8010A7A0u, 0x8010A7B0u};
    constexpr u32 iconPointers[] = {0x8010A8F0u, 0x8010A9E0u, 0x8010AA80u, 0x8010AB50u};
    for (u32 i = 0; i < 4; ++i) m.w32(dialog + 200 + 4 * i, labelPointers[i]);
    for (u32 i = 0; i < 3; ++i) m.w32(dialog + 184 + 4 * i, iconPointers[i]);
    const u32 category = static_cast<u32>(m.r16s(dialog + 12));
    m.w32(dialog + 228, 0); m.w32(dialog + 196, iconPointers[3]);
    if (static_cast<s32>(category) < 4)
        o2_upload_tim(m.r32(m.r32(m.r32(kMenu) + 444) + 4 * category));
    o2_dialog_preset_full(); o2_dialog_preset_first();
    o2_dialog_preset_second(); o2_dialog_preset_third();
}

void Game::o2_parent_dialog_setup() {
    cover(0x800DE188u);
    Frame frame(*this, 32);
    const u32 menu = m.r32(kMenu), delay = m.r32(menu + kSetupDelay) + 1;
    m.w32(menu + kSetupDelay, delay);
    if (static_cast<s32>(delay) < 2) return;
    const u32 first = m.r32(kDialog);
    m.w32(menu + kSetupDelay, 0); m.w8(first, 0);
    const u32 next = m.r32(kDialog);
    m.w16(first + 12, 0); m.w8(next + 1, 0);
    for (u32 field : {4u, 5u, 6u, 2u, 28u}) m.w8(m.r32(kDialog) + field, 0);
    const u32 target = m.r32(kDialog), flagsTarget = m.r32(kDialog);
    m.w32(target + 320, 0); m.w32(target + 16, 0); m.w8(flagsTarget + 29, 0);
    for (u32 field = 30; field < 36; ++field) m.w8(m.r32(kDialog) + field, 0);
    o2_dialog_metadata_tables();
    const u32 activeMenu = m.r32(kMenu);
    m.w8(activeMenu + 125, 1);
    const u32 stageMenu = m.r32(kMenu), loader = m.r32(0x800BCE50u);
    m.w32(activeMenu + 80, 0); m.w16(stageMenu + kLoadStage, 0);
    const u32 entries = ld_relocate_dir(m.r32(loader + 96));
    m.w32(m.r32(kMenu) + 28, entries);
    o2_set_scripts(ld_relocate_dir(m.r32(entries + 4)), 2);
    const u32 loadedMenu = m.r32(kMenu), directory = m.r32(loadedMenu + 28);
    const u32 directoryAgain = m.r32(loadedMenu + 28);
    m.w32(loadedMenu + 152, m.r32(directory + 8));
    m.w32(loadedMenu + 160, m.r32(directoryAgain + 16));
    m.w32(loadedMenu + 164, m.r32(directoryAgain + 20));
    m.w32(loadedMenu + 168, m.r32(directoryAgain + 24));
}

void Game::o2_parent_dialog_resume() {
    cover(0x800DE540u);
    Frame frame(*this, 32);
    const u32 stage = static_cast<u32>(m.r16s(m.r32(kMenu) + kLoadStage));
    call_guest(m.r32(0x8010984Cu + 4 * stage), 0, 0);
    if (m.r16s(m.r32(kMenu) + kLoadStage) != 3) return;
    m.w8(m.r32(kDialog), 9); m.w8(m.r32(kDialog) + 1, 0);
    const u32 first = m.r32(kDialog);
    m.w8(first + 4, 0);
    const u32 second = m.r32(kDialog);
    m.w16(first + 12, 0); m.w8(second + 5, 0);
    for (u32 field : {6u, 2u, 28u}) m.w8(m.r32(kDialog) + field, 0);
    const u32 target = m.r32(kDialog), flagsTarget = m.r32(kDialog);
    m.w32(target + 16, 0); m.w8(flagsTarget + 29, 0);
    for (u32 field = 30; field < 36; ++field) m.w8(m.r32(kDialog) + field, 0);
    o2_dialog_metadata_tables();
    const u32 menu = m.r32(kMenu);
    m.w32(menu + 80, 0); m.w8(menu + 125, 1);
}

void Game::o2_frontend_diagnostics() {
    cover(0x800CDD84u);
    Frame frame(*this, 32);
    const u32 menu = m.r32(kMenu);
    m.w16(menu + 438, m.r16(menu + 436));
    m.w16(m.r32(kMenu) + 436, hw->vsync(1));
    o2_text_font(1); o2_text_semi(2); o2_text_fold(1, 1, 1);
    o2_text_frame_mode(1); o2_text_at(10, 10); o2_text_rgb(128, 128, 128);
    auto percentage = [](u32 ticks) { return static_cast<u32>(static_cast<s32>(ticks * 100u) / 240); };
    u32 state = m.r32(g::kScene), ticks = m.r32(state + 72);
    o2_text_printf(0x800CBFECu, percentage(ticks), ticks, 0);
    state = m.r32(g::kScene); ticks = m.r32(state + 76);
    o2_text_printf(0x800CBFFCu, percentage(ticks), ticks, 0);
    state = m.r32(g::kScene);
    const u32 start = m.r32(state + 24), end = m.r32(state + 28);
    o2_text_printf(0x800CC00Cu, start, end, end - start);
    ticks = m.r32(m.r32(kMenu) + 440);
    o2_text_printf(0x800CC028u, percentage(ticks), ticks, 0);
    o2_text_printf(0x800CC038u, static_cast<u32>(m.r16s(m.r32(kMenu) + 438)), 0, 0);
    const u32 now = hw->vsync(1), currentMenu = m.r32(kMenu);
    m.w16(currentMenu + 436, now - m.r16(currentMenu + 436));
}
}

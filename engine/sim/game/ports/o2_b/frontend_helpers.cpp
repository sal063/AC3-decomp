#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kScreens = 0x80112DC0u;
constexpr u32 kDiscMenu = 0x80112DF8u, kBriefing = 0x80112DB0u;
}

u32 Game::o2_launch_direct() {
    cover(0x800E65A4u);
    const u32 scene = m.r32(g::kScene);
    if (!(m.r32(scene + 300) & 16)) return 0;
    const u32 held = m.r32(scene + 296) & 47;
    if (held) return held;
    m.w32(scene + 944, 2);
    m.w32(m.r32(kMenu) + 148, 0x60000002u);
    return 2;
}

u32 Game::o2_launch_prompt_begin() {
    cover(0x800E65F8u);
    m.w16(m.r32(kMenu) + 134, 1);
    return 1;
}

u32 Game::o2_launch_prompt_input() {
    cover(0x800E660Cu);
    const u32 scene = m.r32(g::kScene);
    if (!(m.r32(scene + 300) & 16)) return 0;
    if (m.r32(scene + 296) & 47) return 0x80110000u;
    const u32 menu = m.r32(kMenu);
    m.w16(menu + 132, 5); m.w16(menu + 134, 0);
    return 5;
}

u32 Game::o2_save_exit(s32 mode) {
    cover(0x800E69A8u);
    switch (mode) {
    case 0: return 18;
    case 1: return 17;
    case 2: {
        const u32 scene = m.r32(g::kScene), menu = m.r32(kMenu), dialog = m.r32(kDiscMenu);
        m.w32(scene + 2404, m.r32(scene + 2404) & ~1u);
        m.w32(menu + 148, 0x60000004u); m.w8(dialog + 2, 1);
        return 0;
    }
    case 3: {
        const u32 scene = m.r32(g::kScene), dialog = m.r32(kDiscMenu);
        m.w16(scene + 2388, 3); m.w32(scene + 2412, m.r32(scene + 2412) | 16);
        m.w8(dialog + 2, 1);
        return 18;
    }
    default: return 0;
    }
}

u32 Game::o2_disc_wait_open() {
    cover(0x800EB944u);
    Frame frame(*this, 24);
    const u32 state = kcd_drive_state();
    if (state != 3) return state;
    const u32 menu = m.r32(kDiscMenu), next = m.r8(menu + 1) + 1;
    m.w8(menu + 1, next);
    return next;
}

u32 Game::o2_disc_wait_ready() {
    cover(0x800EB98Cu);
    Frame frame(*this, 24);
    const u32 state = kcd_drive_state();
    if (state != 1) return state;
    m.w8(m.r32(m.r32(kScreens) + 92) + 1, 73);
    const u32 menu = m.r32(kDiscMenu), next = m.r8(menu + 1) - 2;
    m.w8(menu + 1, next);
    return next;
}

u32 Game::o2_brief_animation_reset() {
    cover(0x800ED15Cu);
    const u32 briefing = m.r32(kBriefing);
    m.w16(briefing + 2, 0);
    return briefing;
}

u32 Game::o2_brief_image_command() {
    cover(0x800ED16Cu);
    const u32 briefing = m.r32(kBriefing), next = m.r32(briefing + 20) + 4;
    m.w32(briefing + 3896, 1); m.w32(briefing + 20, next);
    return next;
}

u32 Game::o2_brief_image_close() {
    cover(0x800ED468u);
    Frame frame(*this, 24);
    o2_restore_frontend_heap();
    const u32 briefing = m.r32(kBriefing);
    m.w32(briefing + 3896, 0);
    return briefing;
}
}

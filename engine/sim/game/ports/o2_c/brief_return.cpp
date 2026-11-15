#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kBrief = 0x80112DB0u, kMenu = 0x80112DC4u, kScreens = 0x80112DC0u;
constexpr u32 kDisplay = 0x800BCE48u, kHangar = 0x80112DA8u;
constexpr u32 kMissionDisc = 0x80112750u, kSubstate = 0x86, kScenePhase = 0x954;
u32 missionDisc(Memory& m) {
    return m.r8(kMissionDisc + static_cast<u32>(m.r16s(m.r32(g::kScene) + 0x38C)));
}
u32 sceneDisc(Memory& m, u32 booted, u32 drive) {
    if (!booted) return drive;
    const u32 need = missionDisc(m);
    return need == 1 || need == 2 ? need : booted;
}
void resetMenu(Memory& m) {
    m.w8(m.r32(kMenu) + 0x7C, 0);
    m.w8(m.r32(kMenu) + 0x7D, 0);
    m.w16(m.r32(kMenu) + 0x1C6, 0);
}
}

u32 Game::o2_brief_background_resume() {
    cover(0x800F108Cu);
    Frame frame(*this, 24);
    const u32 brief = m.r32(kBrief);
    const s32 previous = m.r32s(brief + 16);
    m.w32(brief + 16, static_cast<u32>(previous) + 1);
    if (previous >= 32) {
        o2_hangar_fly();
        m.w32(m.r32(kBrief) + 16, 0);
        m.w16(m.r32(kMenu) + kSubstate, 2);
        return 2;
    }
    constexpr s32 channels[] = {143, 135, 83};
    for (u32 channel = 0; channel < 3; ++channel) {
        const s32 tick = wrap::add(m.r32s(m.r32(kBrief) + 16), 1);
        m.w8(m.r32(kDisplay) + 16 + channel, wrap::mul(tick, channels[channel]) / 32);
    }
    const u32 display = m.r32(kDisplay), flags = m.r32(display) | 8u;
    m.w32(display, flags);
    return flags;
}

u32 Game::o2_brief_flight_arrived() {
    cover(0x800F11BCu);
    Frame frame(*this, 24);
    if (m.r32(m.r32(kHangar) + 0x14) != 2) return 2;
    const s32 phase = m.r16s(m.r32(g::kScene) + kScenePhase);
    if (phase == 1) {
        const u32 state = m.r32(m.r32(kBrief) + 12) ? 3u : 15u;
        if (state == 3) {
            o2_entry_enter(0, 15);
            o2_group_set(0, 0x8000);
        } else o2_entry_leave(0, 15);
        m.w16(m.r32(kMenu) + kSubstate, state);
        return state;
    }
    if (static_cast<u16>(phase - 2) < 2 && !m.r32(m.r32(kBrief) + 12)) {
        const u32 menu = m.r32(kMenu);
        m.w8(menu + 0x7C, 0); m.w8(menu + 0x7D, 0); m.w8(menu + 0x7E, 4);
        heap_set(m.r32(kBrief));
        o2_entry_enter(0, 0);
        o2_entry_enter(0, 1);
        o2_group_set(0, 3);
        o2_hangar_effect2_begin();
        m.w8(m.r32(g::kScene) + 0x95E, 0);
        const u32 scene = m.r32(g::kScene);
        m.w16(scene + kScenePhase, 0);
        return scene;
    }
    m.w16(m.r32(kMenu) + kSubstate, 3);
    o2_entry_enter(0, 15);
    o2_group_set(0, 0x8000);
    return m.r32(kMenu);
}

u32 Game::o2_brief_route_exit() {
    cover(0x800F26ECu);
    Frame frame(*this, 24);
    const u32 menu = m.r32(kMenu);
    m.w16(m.r32(menu + 0x16C) + 190, 1);
    m.w8(menu + 0x7E, 0);
    m.w16(menu + 0x84, 6); m.w16(menu + kSubstate, 0); m.w32(menu + 0xE0, 0);
    const u32 brief = m.r32(kBrief);
    if (m.r32(brief + 12)) {
        const s32 page = m.r16s(m.r32(brief + 0x189C));
        if (m.r8(0x8010B400u + 8 * static_cast<u32>(page) + 6) == 10) {
            m.w32(m.r32(kMenu) + 0x11C, 1);
            const u32 scene = m.r32(g::kScene);
            m.w8(scene + 0x95E, m.r8(scene + 0x95E) + 1);
            m.w16(scene + kScenePhase, 2);
            const s32 current = m.r16s(m.r32(m.r32(kBrief) + 0x189C));
            m.w8(m.r32(g::kScene) + 0x3A8, m.r8(0x8010B360u + static_cast<u32>(current)));
            o2_unlock_by_id(m.r16s(m.r32(m.r32(kBrief) + 0x189C)));
            return 0;
        }
    }
    const u32 scene = m.r32(g::kScene);
    m.w8(scene + 0x95E, 0);
    m.w32(scene + 0x96C, m.r32(scene + 0x96C) & ~64u);
    heap_set(m.r32(kBrief));
    o2_group_set(0, 0x8000);
    return m.r32(kMenu);
}

u32 Game::o2_brief_disc_check() {
    cover(0x800F2EB0u);
    Frame frame(*this, 24);
    const u32 disc = sceneDisc(m, hw ? hw->cd_both_discs() : 0u, static_cast<u32>(disc_number()));
    if (hw && hw->cd_both_discs() && (missionDisc(m) == 1 || missionDisc(m) == 2)) hw->cd_pack_follow(missionDisc(m));
    if (disc == 1 || disc == 2) {
        const u32 kind = missionDisc(m);
        if (kind == disc || kind == 3) {
            o2_group_set(0, 3);
            resetMenu(m);
            heap_set(m.r32(0x80112DB8u));
            return sound_play(59);
        }
    } else if (disc != 4 && disc != 0xFFFFFFFFu) return 0xFFFFFFFFu;
    kcd_open_drive();
    o2_group_set(0, 0x0200000B);
    m.w8(m.r32(m.r32(kScreens) + 0x5C) + 61, disc == 1 ? 1 : 0);
    const u32 menu = m.r32(kMenu), next = m.r16(menu + kSubstate) + 1;
    m.w16(menu + kSubstate, next);
    return next;
}

u32 Game::o2_brief_disc_retry() {
    cover(0x800F30D4u);
    Frame frame(*this, 32);
    const u32 disc = sceneDisc(m, hw ? hw->cd_both_discs() : 0u, static_cast<u32>(disc_number()));
    if (hw && hw->cd_both_discs() && (missionDisc(m) == 1 || missionDisc(m) == 2)) hw->cd_pack_follow(missionDisc(m));
    if (disc == 1 || disc == 2) {
        const u32 kind = missionDisc(m);
        if (kind == disc || kind == 3) {
            o2_group_set(0, 3);
            resetMenu(m);
            heap_set(m.r32(0x80112DB8u));
            return sound_play(59);
        }
    } else if (disc != 4 && disc != 0xFFFFFFFFu) return 0xFFFFFFFFu;
    kcd_open_drive();
    o2_group_set(0, 0x0200000B);
    m.w8(m.r32(m.r32(kScreens) + 0x5C) + 61, disc == 1 ? 3 : 2);
    const u32 menu = m.r32(kMenu);
    m.w16(menu + kSubstate, 1);
    return menu;
}
}

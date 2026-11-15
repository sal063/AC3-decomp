#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kScreens = 0x80112DC0u;
constexpr u32 kSubstate = 0x86, kTopState = 0x7D, kDialogFlag = 61;
u32 pageStream(Memory& m) {
    const u32 page = static_cast<u32>(m.r16s(m.r32(kMenu) + 0x124));
    return static_cast<u32>(m.r16s(0x8010B400u + 8 * page + 2));
}
u32 dialogFlag(Memory& m) { return m.r32(m.r32(kScreens) + 0x5C) + kDialogFlag; }
u32 advance(Memory& m) {
    const u32 menu = m.r32(kMenu), next = m.r16(menu + kSubstate) + 1;
    m.w16(menu + kSubstate, next);
    return next;
}
}

u32 Game::o2_disc_page_check() {
    cover(0x800F29E8u);
    Frame frame(*this, 24);
    if (!kcd_stream_on_this_disc(pageStream(m))) return advance(m);
    o2_group_set(0, 0);
    heap_set(m.r32(m.r32(kMenu) + 0x28));
    sound_play(60);
    m.w8(m.r32(kMenu) + kTopState, 2);
    const u32 menu = m.r32(kMenu);
    m.w16(menu + kSubstate, 0);
    return menu;
}

u32 Game::o2_disc_page_retry() {
    cover(0x800F2D1Cu);
    Frame frame(*this, 24);
    if (!kcd_stream_on_this_disc(pageStream(m))) return advance(m);
    o2_group_set(0, 0);
    heap_set(m.r32(m.r32(kMenu) + 0x28));
    sound_play(60);
    m.w8(m.r32(kMenu) + kTopState, 2);
    const u32 menu = m.r32(kMenu);
    m.w16(menu + kSubstate, 0);
    return menu;
}

u32 Game::o2_disc_prompt_begin() {
    cover(0x800F2A8Cu);
    Frame frame(*this, 32);
    const u32 disc = stream_kind(pageStream(m));
    if (disc == 0) return 2;
    kcd_open_drive();
    o2_group_set(0, 0x0200000B);
    m.w8(dialogFlag(m), disc == 2 ? 1 : 0);
    return advance(m);
}

u32 Game::o2_disc_prompt_retry() {
    cover(0x800F2DC0u);
    Frame frame(*this, 32);
    const u32 disc = stream_kind(pageStream(m));
    if (disc == 0) return 2;
    kcd_open_drive();
    o2_group_set(0, 0x0200000B);
    m.w8(dialogFlag(m), disc == 2 ? 3 : 2);
    const u32 menu = m.r32(kMenu);
    m.w16(menu + kSubstate, 2);
    return disc == 2 ? menu : 2;
}

u32 Game::o2_disc_prompt_open() {
    cover(0x800F2B6Cu);
    Frame frame(*this, 32);
    const u32 drive = kcd_drive_state();
    if (drive == 3) advance(m);
    const u32 cancel = m.r32(m.r32(g::kScene) + 0x12C) & 32u;
    if (!cancel || drive == 1) return cancel;
    const bool stopped = kcd_lid_closed() == 1;
    m.w8(m.r32(kMenu) + kTopState, stopped ? 1 : 5);
    const u32 menu = m.r32(kMenu);
    m.w16(menu + kSubstate, stopped ? 0 : 3);
    return menu;
}

u32 Game::o2_disc_prompt_close() {
    cover(0x800F2C34u);
    Frame frame(*this, 24);
    const u32 drive = kcd_drive_state();
    if (drive == 0 || drive == 2) advance(m);
    else if (drive == 1) m.w8(dialogFlag(m), 4);
    const u32 cancel = m.r32(m.r32(g::kScene) + 0x12C) & 32u;
    if (!cancel || drive == 1) return cancel;
    const bool stopped = kcd_lid_closed() == 1;
    m.w8(m.r32(kMenu) + kTopState, stopped ? 1 : 5);
    const u32 menu = m.r32(kMenu);
    m.w16(menu + kSubstate, stopped ? 0 : 3);
    return stopped ? menu : 3;
}
}

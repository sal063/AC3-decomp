#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kView = 0x800BCE48u, kDialog = 0x80112DDCu;
constexpr u32 kSubstate = 134, kBackdropTick = 80;
u32 callback(Memory& m, u32 table) {
    return m.r32(table + 4u * static_cast<u32>(m.r16s(m.r32(kMenu) + kSubstate)));
}
}

u32 Game::o2_hangar_backdrop_fade() {
    cover(0x800CDAF4u);
    constexpr u32 colours[] = {143, 135, 83};
    for (u32 channel = 0; channel < 3; ++channel) {
        const u32 product = m.r32(m.r32(kMenu) + kBackdropTick) * colours[channel];
        m.w8(m.r32(kView) + 16 + channel, static_cast<s32>(product) / 32);
    }
    const u32 view = m.r32(kView);
    m.w32(view, m.r32(view) | 8);
    const u32 menu = m.r32(kMenu), next = m.r32(menu + kBackdropTick) + 1;
    m.w32(menu + kBackdropTick, next);
    if (static_cast<s32>(next) < 33) return 0;
    m.w32(menu + kBackdropTick, 0);
    return 1;
}

u32 Game::o2_brief_setup_dispatch(u32 first, u32 second) {
    cover(0x800D1214u);
    Frame frame(*this, 24);
    return call_guest(callback(m, 0x8010BA98u), first, second);
}

u32 Game::o2_brief_disc_dispatch(u32 first, u32 second) {
    cover(0x800D1258u);
    Frame frame(*this, 24);
    return call_guest(callback(m, 0x8010B9F0u), first, second);
}

u32 Game::o2_brief_resume_dispatch(u32 first, u32 second) {
    cover(0x800D129Cu);
    Frame frame(*this, 24);
    return call_guest(callback(m, 0x8010BA08u), first, second);
}

void Game::o2_dialog_save_dispatch(u32 first, u32 second) {
    cover(0x800DCBE0u);
    Frame frame(*this, 24);
    call_guest(callback(m, 0x8010B358u), first, second);
    if (m.r16s(m.r32(kMenu) + 132) != 7) return;
    o2_group_set(1, 0);
    const u32 menu = m.r32(kMenu);
    const u32 dialog = m.r32(kDialog);
    m.w16(menu + 132, 0);
    m.w8(dialog + 1, m.r8(dialog + 1) + 1);
    o2_fade_set(1, 1, 0, 30);
    o2_hangar_effect3_exit();
}
}

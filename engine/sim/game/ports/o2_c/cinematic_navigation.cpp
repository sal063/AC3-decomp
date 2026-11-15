#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kCinematic = 0x80112E1Cu, kMenu = 0x80112DC4u, kScreens = 0x80112DC0u;
constexpr u32 kPageDelay = 0x80112DD4u, kTransition = 0x8011278Cu, kDiscKinds = 0x80112750u;
u32 flags(Memory& m) { return m.r32(m.r32(kScreens) + 0x5C); }
}

u32 Game::o2_cinematic_navigation() {
    cover(0x800FD9CCu);
    Frame frame(*this, 24);
    auto lastPage = [&] { return (m.r16s(m.r32(kCinematic) + 0x42) - 1) / 4; };
    auto column = [&] { return m.r8(m.r32(kCinematic) + 4); };
    auto page = [&] { return m.r8(m.r32(kCinematic) + 5); };
    auto repeat = [&] { return m.r32(m.r32(g::kScene) + 0x130); };
    auto pageChanged = [&](u32 direction) {
        m.w8(flags(m) + 0x33, direction); m.w32(kPageDelay, 12);
    };
    if (!m.r32(kPageDelay)) {
        o2_flag(49, (static_cast<s32>(page()) < lastPage() ? 2u : 0u) | (page() ? 1u : 0u));
        m.w8(flags(m) + 0x33, 0);
    } else m.w32(kPageDelay, m.r32(kPageDelay) - 1);
    if (repeat() & 2) {
        u32 state = m.r32(kCinematic);
        if (column() < 3) {
            if (static_cast<s32>(column() + 4 * page()) < m.r16s(state + 0x42) - 1) {
                m.w8(state + 4, column() + 1); sound_play(61);
            }
        } else if (static_cast<s32>(page()) < lastPage()) {
            m.w8(state + 4, 0);
            state = m.r32(kCinematic); m.w8(state + 5, page() + 1);
            o2_cinematic_page(page()); pageChanged(2); sound_play(61);
        }
        o2_cinematic_detail(column());
    }
    if (repeat() & 1) {
        u32 state = m.r32(kCinematic);
        if (column()) { m.w8(state + 4, column() - 1); sound_play(61); }
        else if (page()) {
            m.w8(state + 4, 3);
            state = m.r32(kCinematic); m.w8(state + 5, page() - 1);
            o2_cinematic_page(page()); pageChanged(1); sound_play(61);
        }
        o2_cinematic_detail(column());
    }
    if (repeat() & 0x200) {
        if (static_cast<s32>(page()) < lastPage()) {
            u32 state = m.r32(kCinematic); m.w8(state + 5, page() + 1);
            state = m.r32(kCinematic);
            const s32 last = m.r16s(state + 0x42) - 1;
            if (static_cast<s32>(column() + 4 * page()) >= last) m.w8(state + 4, last % 4);
            sound_play(61); pageChanged(2);
        }
        o2_cinematic_page(page()); o2_cinematic_detail(column());
    }
    if (repeat() & 0x100) {
        if (page()) {
            const u32 state = m.r32(kCinematic); m.w8(state + 5, page() - 1);
            sound_play(61); pageChanged(1);
        }
        o2_cinematic_page(page()); o2_cinematic_detail(column());
    }
    u32 scene = m.r32(g::kScene);
    if ((m.r32(scene + 0x12C) & 16) && !(m.r32(scene + 0x128) & 0x2F)) {
        const u32 state = m.r32(kCinematic);
        m.w16(state + 2, 1); m.w8(state, 2); sound_play(60);
    }
    scene = m.r32(g::kScene);
    if ((m.r32(scene + 0x12C) & 32) && !(m.r32(scene + 0x128) & 0x5F)) {
        const u32 state = m.r32(kCinematic);
        m.w16(state + 2, 2); m.w8(state, 2); sound_play(59);
    }
    const u32 selected = column(); m.w8(flags(m) + 0x34, selected);
    return selected;
}

u32 Game::o2_transition_disc() {
    cover(0x800FEF24u);
    Frame frame(*this, 24);
    const s32 disc = disc_number();
    u32 transition = m.r32(kTransition);
    if (!disc) { m.w32(transition, 1); return 0; }
    const u32 wanted = m.r8(kDiscKinds + m.r32(transition + 8));
    if (wanted == 3 || disc == 3) { m.w32(transition, 0); return 2; }
    if (disc == 4 || disc == -1) {
        if (wanted == 1) m.w32(transition, m.r32(transition + 4) ? 4 : 2);
        transition = m.r32(kTransition);
        if (m.r8(kDiscKinds + m.r32(transition + 8)) == 2)
            m.w32(transition, m.r32(transition + 4) ? 5 : 3);
    } else if (disc == 1) {
        if (wanted == 1) { m.w32(transition, 6); return 2; }
        m.w32(transition, m.r32(transition + 4) ? 5 : 3);
    } else if (disc == 2) {
        if (wanted == 2) { m.w32(transition, 6); return 2; }
        m.w32(transition, m.r32(transition + 4) ? 4 : 2);
    } else return 0;
    kcd_lid_opened();
    return 1;
}

u32 Game::o2_cinematic_play_tick() {
    cover(0x800FE1E8u);
    Frame frame(*this, 32);
    const s32 disc = disc_number();
    const u32 menu = m.r32(kMenu), phase = m.r32(menu + 0x264);
    const u32 callback = m.r32(0x80112790u + 4 * phase);
    u32 result = 0;
    switch (callback) {
    case 0x800FEF08u: result = o2_transition_reset(); break;
    case 0x800FEF24u: result = o2_transition_disc(); break;
    case 0x800FF098u: result = o2_hangar_drive_open(); break;
    case 0x800FF0BCu: result = o2_hangar_drive_close(); break;
    default:
        if (callback >= 0x800F0000u && callback < 0x80110000u)
            unported(callback, "invalid cinematic drive callback");
        result = call_by_address(callback, "cinematic drive callback outside o2_c"); break;
    }
    if (result & 1) m.w32(m.r32(kMenu) + 0x264, m.r32(m.r32(kMenu) + 0x264) + 1);
    if (result & 2) { m.w8(m.r32(kCinematic) + 1, 1); return 1; }
    if (result & 4) m.w32(m.r32(kMenu) + 0x264, 1);
    const u32 prompt = o2_transition_state();
    if (prompt) o2_group_set(0, 0x0A000003);
    if (prompt < 1 || prompt > 6) return 0x800D0000u;
    constexpr u32 promptFlags[] = {4, 0, 1, 2, 3, 5};
    m.w8(flags(m) + 0x3D, promptFlags[prompt - 1]);
    if (prompt == 1 || prompt == 6) return promptFlags[prompt - 1];
    const u32 scene = m.r32(g::kScene);
    if (!(m.r32(scene + 0x12C) & 32)) return 0;
    if (m.r32(scene + 0x128) & 0x5F) return 4;
    if (disc == 4 || disc == -1) return 0xFFFFFFFFu;
    if (kcd_lid_closed()) m.w8(m.r32(kCinematic) + 1, 2);
    return sound_play(59);
}

u32 Game::o2_cinematic_tick() {
    cover(0x800FE584u);
    Frame frame(*this, 24);
    const u32 callback = m.r32(0x80111758u + 4 * m.r8(m.r32(kCinematic)));
    switch (callback) {
    case 0x800FD958u: o2_cinematic_begin(); break;
    case 0x800FD9CCu: o2_cinematic_navigation(); break;
    case 0x800FDE78u: o2_cinematic_leave(); break;
    case 0x800FDF38u: o2_cinematic_ready(); break;
    case 0x800FDF64u: o2_cinematic_difficulty(); break;
    case 0x800FE0D4u: o2_cinematic_transition(); break;
    case 0x800FE180u: o2_cinematic_play_begin(); break;
    case 0x800FE1E8u: o2_cinematic_play_tick(); break;
    case 0x800FE444u: o2_cinematic_catalogue(); break;
    case 0x800FE564u: o2_brief_advance(); break;
    case 0x800FE584u: o2_cinematic_tick(); break;
    default:
        if (callback >= 0x800F0000u && callback < 0x80110000u)
            unported(callback, "invalid cinematic state callback");
        call_by_address(callback, "cinematic state outside o2_c"); break;
    }
    const u32 state = m.r32(kCinematic), exit = m.r8(state + 1);
    if (exit == 1) {
        const u32 menu = m.r32(kMenu);
        m.w16(menu + 0x84, 2); m.w16(menu + 0x86, 0);
        m.w16(m.r32(g::kScene) + 0x38C, m.r8(state + 6 + m.r8(state + 4) + 4 * m.r8(state + 5)));
        return heap_set(m.r32(menu + 0x28));
    }
    if (exit == 2) {
        m.w8(m.r32(kMenu) + 0x7C, 0); m.w8(m.r32(kMenu) + 0x7D, 0);
        return heap_set(m.r32(m.r32(kMenu) + 0x28));
    }
    return 2;
}
}

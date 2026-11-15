#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDialog = 0x80112DF8u, kMenu = 0x80112DC4u, kScreens = 0x80112DC0u;
constexpr u32 kPhase = 0, kStep = 1, kExit = 2, kCard = 4, kMode = 8, kReady = 9;
constexpr u32 kPacket = 12, kBuffers = 16, kCountdown = 24;
u32 dialog(Memory& m) { return m.r32(kDialog); }
u32 menu(Memory& m) { return m.r32(kMenu); }
u32 scene(Memory& m) { return m.r32(g::kScene); }
u32 flags(Memory& m) { return m.r32(m.r32(kScreens) + 92); }
u32 required_disc(Memory& m) { return m.r8(0x80112750u + static_cast<u32>(m.r16s(scene(m) + 908))); }
bool menu_packets(u32 mode) { return mode == 0 || mode == 2 || mode == 3; }
}

void Game::o2_save_disc_prompt() {
    cover(0x800EB73Cu);
    Frame frame(*this, 32);
    m.w8(scene(m) + 2398, 0);
    if (!o2_cards_poll_end()) return;
    const s32 disc = disc_number();
    if (!disc) { m.w8(flags(m) + 1, 73); return; }
    const u32 required = required_disc(m);
    if (required == 3 || disc == 3) { m.w8(dialog(m) + kExit, 1); return; }
    if (disc == 4 || disc == -1) {
        if (required == 1) m.w8(flags(m) + 1, m.r8(dialog(m) + kCountdown) + 70);
        if (required_disc(m) == 2) m.w8(flags(m) + 1, m.r8(dialog(m) + kCountdown) + 71);
    } else if (disc == 1) {
        if (required == 1) { m.w8(dialog(m) + kExit, 1); return; }
        m.w8(flags(m) + 1, m.r8(dialog(m) + kCountdown) + 71);
    } else if (disc == 2) {
        if (required_disc(m) == 2) { m.w8(dialog(m) + kExit, 1); return; }
        m.w8(flags(m) + 1, m.r8(dialog(m) + kCountdown) + 70);
    } else return;
    kcd_lid_opened();
    const u32 state = dialog(m), step = m.r8(state + kStep);
    m.w16(state + kCountdown, 31);
    m.w8(state + kStep, step + 1);
}

void Game::o2_save_menu_init() {
    cover(0x800EB9ECu);
    Frame frame(*this, 32);
    m.w16(menu(m) + 134, m.r16(menu(m) + 134) + 1);
    m.w32(kDialog, mem_alloc(28));
    for (u32 buffer = 0; buffer < 2; ++buffer) {
        const u32 packets = mem_alloc(8192);
        m.w32(dialog(m) + kBuffers + 4 * buffer, packets);
    }
    const u32 state = dialog(m), packets = m.r32(state + kBuffers + 4 * m.r32(scene(m) + 4));
    m.w8(state + kPhase, 0); m.w32(state + kPacket, packets);
    for (u32 field = 1; field < 8; ++field) m.w8(dialog(m) + field, 0);
    m.w8(dialog(m) + kMode, 1); m.w8(dialog(m) + kReady, 0); m.w8(dialog(m) + 10, 0);
    m.w16(dialog(m) + kCountdown, 0); m.w16(dialog(m) + 26, 0);
    o2_hangar_effect3_begin();
    o2_set_scripts(m.r32(menu(m) + 344), 1);
    o2_group_set(0, 0); o2_group_set(1, 0); o2_group_set(2, 0);
    o2_upload_pack(m.r32(menu(m) + 172));
    o2_upload_pack(m.r32(m.r32(menu(m) + 24) + 24));
    o2_pilot_state_init(); o2_card_events_open(); o2_cards_poll_begin();
    o2_fade_set(15, 1, 0, 20);
    o2_hangar_effect1_set(m.r32(m.r32(0x80112DA8u) + 20) ? 3u : 0u);
}

void Game::o2_save_menu_tick() {
    cover(0x800EBD48u);
    Frame frame(*this, 40);
    u32 mode = m.r32(menu(m) + 224);
    if (mode < 5) {
        const u32 packets = menu_packets(mode) ? m.r32(menu(m) + 628) : m.r32(m.r32(0x80112DDCu) + 8);
        m.w32(dialog(m) + kPacket, packets);
    }
    o2_card_poll();
    o2_flag(40, (m.r32(scene(m) + 292) & 16) ? 1u : 0u);
    if (!m.r8(dialog(m) + kMode)) o2_flag(16, m.r8(dialog(m) + kCard));
    else m.w8(flags(m) + 16, 2);
    o2_flag(17, 0);
    o2_text_font(4); o2_text_semi(2); o2_text_fold(1, 10, 20);
    o2_text_frame_mode(static_cast<u32>(o2_fade_value(15)));
    o2_text_rgb(128, 128, 128); o2_text_at(50, 73);
    o2_text_printf(scene(m) + 2376, 0, 0, 0);
    const u32 state = dialog(m);
    const u32 callbacks = m.r32(0x8010B30Cu + 4 * m.r8(state + kPhase));
    call_guest(m.r32(callbacks + 4 * m.r8(state + kStep)), state, 0);
    if (m.r8(dialog(m) + kReady) != 1) {
        const u32 cursor = m.r32(dialog(m) + kPacket);
        m.w32(frame.at(16), cursor);
        const u32 angle = static_cast<u32>(m.r32s(scene(m) + 32) % 64) << 6;
        const u32 next = o2_save_spinner(227, 343, m.r32(menu(m) + 8), angle, cursor);
        m.w32(dialog(m) + kPacket, next);
    }
    mode = m.r32(menu(m) + 224);
    if (mode < 5) {
        const u32 destination = menu_packets(mode) ? menu(m) + 628 : m.r32(0x80112DDCu) + 8;
        m.w32(destination, m.r32(dialog(m) + kPacket));
    }
    const u32 exit = m.r8(dialog(m) + kExit);
    if (exit == 1) {
        o2_card_events_close();
        o2_set_scripts(m.r32(menu(m) + 348), 0);
        const u32 released = dialog(m), menus = menu(m);
        m.w16(menus + 132, 7); m.w16(menus + 134, 0);
        heap_set(released);
        if (!m.r32(menu(m) + 224)) {
            const s32 mission = m.r16s(scene(m) + 908);
            if (mission == 20 || mission == 35 || mission == 40 || mission == 45) {
                const u32 current = scene(m);
                m.w16(current + 2388, 2);
                m.w32(current + 2412, m.r32(current + 2412) | 1);
                m.w32(menu(m) + 148, 0x60000003);
            } else m.w32(scene(m) + 2412, m.r32(scene(m) + 2412) & ~1u);
        }
        m.w32(scene(m) + 2412, m.r32(scene(m) + 2412) & ~8u);
    } else if (exit == 2) {
        o2_card_events_close(); m.w32(menu(m) + 148, 0x60000004);
    }
}
}

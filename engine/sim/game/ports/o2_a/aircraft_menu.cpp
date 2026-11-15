#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kLoad = 0x800BCE50u;
constexpr u32 kChoice = 126, kMainPhase = 452, kLoadPhase = 454;
u32 menu(Memory& m) { return m.r32(kMenu); }
u32 scene(Memory& m) { return m.r32(g::kScene); }
bool replay_choice(Memory& m) { return (m.r32(scene(m) + 2412) & 16) != 0; }
void advance_load(Memory& m) {
    const u32 state = menu(m); m.w16(state + kLoadPhase, m.r16(state + kLoadPhase) + 1);
}
}

void Game::o2_aircraft_load_begin(u32 mode, u32 previousBuffer) {
    cover(0x800CE554u);
    Frame frame(*this, 40);
    const u32 group = replay_choice(m) ? m.r32(menu(m) + 196)
        : o2_aircraft_group(static_cast<u32>(m.r16s(scene(m) + 908)), 0);
    u32 buffer = previousBuffer;
    if (mode == 0 || mode == 1) {
        const u32 loader = m.r32(kLoad);
        m.w32(loader + 96, static_cast<u32>(m.r16s(0x80109840u + 2 * group)));
        if (!mode) {
            buffer = mem_alloc(records_size(loader + 96, 1));
            m.w32(menu(m) + 264, buffer);
        } else buffer = m.r32(menu(m) + 264);
    }
    const u32 count = o2_aircraft_count(static_cast<u32>(m.r16s(scene(m) + 908)), group);
    m.w32(menu(m) + 228, count);
    m.w32(scene(m) + 68, 1);
    request_records(m.r32(kLoad) + 96, 1, buffer, 0x8009B904u);
    advance_load(m);
}

void Game::o2_aircraft_load_wait() {
    cover(0x800CE6B0u);
    if (!m.r32(scene(m) + 68)) advance_load(m);
}

void Game::o2_aircraft_load_relocate() {
    cover(0x800CE6ECu);
    Frame frame(*this, 24);
    const u32 directory = ld_relocate_dir(m.r32(m.r32(kLoad) + 96));
    const u32 state = menu(m);
    m.w32(state + 36, directory); m.w32(state + 184, m.r32(directory));
    const u32 nested = ld_relocate_dir(m.r32(m.r32(state + 36) + 4));
    m.w32(menu(m) + 244, nested); m.w16(menu(m) + kLoadPhase, 3);
}

u32 Game::o2_aircraft_load_idle(u32 previousResult) {
    cover(0x800CE754u);
    return previousResult;
}

void Game::o2_aircraft_screen_wait() {
    cover(0x800CED38u);
    Frame frame(*this, 24);
    if (!o2_entry_state(0, 1)) {
        const u32 state = menu(m); m.w32(state + 420, m.r32(state + 420) + 1);
    }
    if (o2_fade_state(1) == 2) {
        const u32 state = menu(m); m.w16(state + kMainPhase, m.r16(state + kMainPhase) + 1);
    }
}

void Game::o2_aircraft_choice_input() {
    cover(0x800CEDA8u);
    Frame frame(*this, 32);
    o2_marker_grid_draw();
    if ((m.r32(scene(m) + 300) & 16) && !(m.r32(scene(m) + 296) & 0x2F)
        && m.r16s(menu(m) + kLoadPhase) == 3) {
        const u32 state = menu(m);
        m.w16(state + kMainPhase, m.r16(state + kMainPhase) + 1);
        if (!m.r8(state + kChoice)) {
            if (!replay_choice(m)) o2_entry_leave(0, 1);
            if (!m.r8(menu(m) + kChoice)
                && (m.r16s(scene(m) + 908) == 59 || m.r16s(scene(m) + 2388) == 3))
                m.w16(menu(m) + kLoadPhase, 8);
        }
        if (m.r8(menu(m) + kChoice) == 1) {
            if (replay_choice(m)) m.w16(menu(m) + kLoadPhase, 3);
            else {
                heap_set(m.r32(menu(m) + 264));
                m.w16(menu(m) + kLoadPhase, 4);
            }
        }
        if (m.r8(menu(m) + kChoice) == 2) {
            heap_set(m.r32(menu(m) + 264));
            m.w32(0x80112DB8u, mem_alloc(188));
            m.w16(menu(m) + kLoadPhase, 6);
        }
        o2_fade_set(1, 3, -1, -1); o2_fade_set(2, 3, -1, -1);
        if (!m.r8(menu(m) + kChoice)) o2_hangar_effect2_exit();
        if (m.r8(menu(m) + kChoice) == 3) {
            heap_set(m.r32(menu(m) + 264));
            m.w16(menu(m) + kLoadPhase, 10);
        }
        sound_play(60);
    }
    const u32 repeat = m.r32(scene(m) + 304);
    if ((repeat & 12) && m.r16s(menu(m) + kLoadPhase) == 3
        && o2_entry_state(0, 1) != 2 && m.r16s(menu(m) + kLoadPhase) == 3) {
        const u32 state = menu(m), choice = m.r8(state + kChoice);
        const bool restricted = replay_choice(m);
        u32 next;
        if (repeat & 8) next = restricted ? (choice ? 0u : 1u) : (choice < 3 ? choice + 1 : 0u);
        else next = choice ? choice - 1 : (restricted ? 1u : 3u);
        m.w8(state + kChoice, next); sound_play(61);
    }
    o2_marker_spread(); o2_marker_map_draw(); o2_marker_tick();
}

void Game::o2_aircraft_choice_apply() {
    cover(0x800CF1DCu);
    Frame frame(*this, 32);
    if (o2_fade_state(1) || m.r16s(menu(m) + kLoadPhase) != 3) return;
    const u32 state = menu(m), choice = m.r8(state + kChoice);
    switch (choice) {
    case 0:
        m.w8(state + 124, 1);
        if (m.r16s(0x801127ACu + 12 * static_cast<u32>(m.r16s(scene(m) + 908))) != -1) {
            o2_entry_enter(0, 5); o2_entry_enter(0, 6);
            m.w32(frame.at(16), 506); m.w32(frame.at(20), 0);
            o2_entry_set(0, 7, 0, 0, 506, 0);
            o2_group_set(0, 0xE1); o2_fade_set(1, 1, 0, 40); sound_play(90);
        }
        break;
    case 1:
        if (replay_choice(m)) {
            m.w8(state + 127, 1);
            const u32 current = scene(m); m.w32(current + 2412, m.r32(current + 2412) | 2);
        } else m.w8(state + 124, 2);
        break;
    case 2: m.w8(state + 124, 3); break;
    case 3: m.w8(state + 124, 4); sound_play(90); break;
    default: break;
    }
    m.w8(menu(m) + 125, 0);
}

void Game::o2_aircraft_screen_begin() {
    cover(0x800CF364u);
    Frame frame(*this, 24);
    const u32 state = menu(m);
    m.w16(state + kMainPhase, 0); m.w32(state + 80, 0); m.w8(state + 125, 1);
    o2_fade_set(1, 1, 0, 40); o2_fade_set(2, 1, 0, 40);
    o2_prims_setup(); o2_marker_select(1, 0); o2_entry_reset(0, 2);
    if (m.r8(menu(m) + kChoice) == 4) {
        o2_entry_enter(0, 1); o2_entry_reset(0, 0); o2_fade_set(15, 1, 0, 25);
        m.w8(menu(m) + kChoice, 0);
    }
    o2_group_set(0, 7); o2_group_set(2, 0); o2_flag(1, m.r8(menu(m) + kChoice) + 1);
    const u32 current = scene(m);
    const u32 group = o2_aircraft_group(static_cast<u32>(m.r16s(current + 908)), m.r16s(current + 910));
    m.w32(menu(m) + 196, group); sound_play(90); o2_hangar_sounds_restart(0);
}

void Game::o2_aircraft_menu_tick() {
    cover(0x800CF494u);
    Frame frame(*this, 24);
    call_guest(m.r32(0x80109884u + 4 * static_cast<u32>(m.r16s(menu(m) + kMainPhase))), 0, 0);
    call_guest(m.r32(0x8010984Cu + 4 * static_cast<u32>(m.r16s(menu(m) + kLoadPhase))), 0, 0);
}
}

#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDialog = 0x80112DDCu, kMenu = 0x80112DC4u;
constexpr u32 kHeld = 296, kPressed = 300, kRepeat = 304;
u32 dialog(Memory& m) { return m.r32(kDialog); }
u32 scene(Memory& m) { return m.r32(g::kScene); }
}

void Game::o2_dialog_first_choice_apply() {
    cover(0x800D2BCCu);
    Frame frame(*this, 32);
    if (o2_fade_state(1) != 0 || m.r16s(m.r32(kMenu) + 454) != 0) return;
    u32 current = dialog(m);
    if (m.r8(current + 5) == 1) { m.w8(current + 1, 4); return; }
    const u32 action = m.r8(current + 3);
    if (action == 1) { m.w8(current + 2, action); return; }
    if (action != 0) return;
    if (m.r8(current + 4) == 0) m.w8(current, 1);
    current = dialog(m);
    if (m.r8(current + 4) == 1) m.w8(current, 3);
    current = dialog(m);
    if (m.r8(current + 4) == 2) m.w8(current, 5);
    current = dialog(m);
    if (m.r8(current + 4) == 3) {
        m.w16(m.r32(kMenu) + 134, 0); m.w8(current, 6);
        o2_group_set(2, 0); o2_group_set(0, 0x8000); o2_entry_enter(0, 15);
    }
    current = dialog(m);
    const u32 choice = m.r8(current + 4);
    if (choice == 4 || choice == 5) {
        m.w16(m.r32(kMenu) + 134, 0); m.w8(current, 10);
        o2_group_set(2, 0); o2_group_set(0, 0);
    }
    if (player_settings && (choice == 6 || choice == 7)) m.w8(current, kEnginePage + choice - 6);
    o2_fade_set(1, 1, 0, 40);
    m.w8(dialog(m) + 1, 0);
    o2_fade_set(2, 1, 0, 40); sound_play(90);
}

u32 Game::o2_dialog_first_choice_input() {
    cover(0x800D2568u);
    Frame frame(*this, 32);
    const bool locked = (m.r32(scene(m) + 0x96C) & 0x10u) != 0;
    o2_flag(0x3E, 1);
    const auto repeat = [&] { return m.r32(scene(m) + kRepeat); };
    bool moved = false;
    const auto move = [&](u32 choice) { moved = true; m.w8(dialog(m) + 4, choice); };
    const bool more = player_settings != nullptr;
    switch (m.r8(dialog(m) + 4)) {
    case 0:
        if ((repeat() & 3) && !locked) move(2);
        if (repeat() & 8) move(1);
        if (repeat() & 4) move(more ? 6 : 4);
        break;
    case 1:
        if (repeat() & 3) move(3);
        if (repeat() & 8) move(4);
        if (repeat() & 4) move(0);
        break;
    case 2:
        if (repeat() & 3) move(0);
        if (repeat() & 8) move(3);
        if (repeat() & 4) move(more ? 7 : 5);
        break;
    case 3:
        if (repeat() & 3) move(1);
        if (repeat() & 8) move(5);
        if (repeat() & 4) move(locked ? 5 : 2);
        break;
    case 4:
        o2_flag(0x3E, 0);
        if (repeat() & 8) move(more ? 6 : 0);
        if (repeat() & 4) move(1);
        break;
    case 5:
        o2_flag(0x3E, 0);
        if (repeat() & 8) move(more ? 7 : locked ? 3 : 2);
        if (repeat() & 4) move(3);
        break;
    case 6:
        if (repeat() & 3) move(7);
        if (repeat() & 8) move(0);
        if (repeat() & 4) move(4);
        break;
    case 7:
        if (repeat() & 3) move(6);
        if (repeat() & 8) move(locked ? 3 : 2);
        if (repeat() & 4) move(5);
        break;
    default: break;
    }
    if (m.r8(dialog(m) + 4) - 4u >= 2u && (repeat() & 3) && moved) sound_play(0x3D);
    if ((repeat() & 0xC) && moved) sound_play(0x3D);
    o2_flag(5, m.r8(dialog(m) + 4));
    if (more) o2x_options_buttons();
    const u32 state = scene(m);
    const bool lower = m.r8(dialog(m) + 4) - 4u < 2u;
    if ((m.r32(state + kPressed) & (lower ? 0x40u : 0x10u)) && !(m.r32(state + kHeld) & 0x2F)
        && m.r32(m.r32(kMenu) + 0x90) != 5) {
        m.w8(dialog(m) + 1, 3);
        m.w8(dialog(m) + 3, 0);
        o2_fade_set(1, 3, -1, -1);
        if (lower) {
            o2_entry_leave(0, 1);
            o2_fade_set(2, 3, -1, -1);
            o2_fade_set(15, 3, -1, -1);
        } else {
            if (m.r8(dialog(m) + 4) == 2) m.w16(m.r32(kMenu) + 0x1C6, 1);
            if (m.r8(dialog(m) + 4) == 3) o2_fade_set(15, 3, -1, -1);
            o2_entry_leave(0, 1);
            o2_fade_set(2, 3, -1, -1);
        }
        sound_play(0x3C);
        sound_play(0x5B);
    }
    const u32 now = scene(m);
    if (!(m.r32(now + kPressed) & 0x20)) return 0;
    if (const u32 held = m.r32(now + kHeld) & 0x5F) return held;
    m.w8(dialog(m) + 1, 3);
    m.w8(dialog(m) + 3, 1);
    o2_fade_set(1, 3, -1, -1);
    o2_fade_set(2, 3, -1, -1);
    sound_play(0x3B);
    sound_play(0x5B);
    m.w32(scene(m) + kPressed, 0);
    return scene(m);
}

void Game::o2_dialog_second_choice_input() {
    cover(0x800D2DF0u);
    Frame frame(*this, 24);
    if (m.r32(scene(m) + kRepeat) & 8) {
        const u32 current = dialog(m), choice = m.r8(current + 5);
        m.w8(current + 5, choice < 3 ? choice + 1 : 1);
    }
    if (m.r32(scene(m) + kRepeat) & 4) {
        const u32 current = dialog(m), choice = m.r8(current + 5);
        m.w8(current + 5, choice < 2 ? 3 : choice - 1);
    }
    o2_flag(5, m.r8(dialog(m) + 5));
    u32 state = scene(m);
    if ((m.r32(state + kPressed) & 16) && !(m.r32(state + kHeld) & 0x2F)) {
        if (m.r8(dialog(m) + 5) == 3) m.w16(m.r32(kMenu) + 454, 3);
        m.w8(dialog(m) + 1, 6);
        o2_fade_set(1, 3, -1, -1); o2_fade_set(2, 3, -1, -1); o2_entry_leave(0, 1);
    }
    state = scene(m);
    if ((m.r32(state + kPressed) & 32) && !(m.r32(state + kHeld) & 0x5F)) {
        m.w8(dialog(m) + 5, 0); m.w8(dialog(m) + 1, 6);
        o2_fade_set(1, 3, -1, -1); m.w32(scene(m) + kPressed, 0);
    }
}

void Game::o2_options_choice_input() {
    cover(0x800DA66Cu);
    Frame frame(*this, 32);
    if (m.r32(scene(m) + kRepeat) & 2) {
        const u32 current = dialog(m), choice = m.r8(current + 6);
        m.w8(current + 6, choice < 3 ? choice + 1 : 0); sound_play(61);
    }
    if (m.r32(scene(m) + kRepeat) & 1) {
        const u32 current = dialog(m), choice = m.r8(current + 6);
        m.w8(current + 6, choice ? choice - 1 : 3); sound_play(61);
    }
    u32 state = scene(m);
    if ((m.r32(state + kPressed) & 16) && !(m.r32(state + kHeld) & 0x2F)) {
        o2_fade_set(1, 3, -1, -1);
        m.w8(dialog(m) + 1, 3); m.w8(dialog(m) + 3, 1);
        sound_play(60); sound_play(91);
    }
    o2_flag(5, m.r8(dialog(m) + 6)); o2_flag(6, 0);
    if (m.r32(scene(m) + kHeld) & 1) o2_flag(6, 1);
    if (m.r32(scene(m) + kHeld) & 2) o2_flag(6, 2);
    state = scene(m);
    if ((m.r32(state + kPressed) & 32) && !(m.r32(state + kHeld) & 0x5F)) {
        o2_fade_set(1, 3, -1, -1);
        m.w8(dialog(m) + 1, 3); m.w8(dialog(m) + 3, 2);
        sound_play(59); m.w32(scene(m) + kPressed, 0);
    }
}

void Game::o2_options_choice_apply() {
    cover(0x800DA870u);
    Frame frame(*this, 24);
    const u32 state = scene(m);
    if ((m.r32(state + kPressed) & 16) && !(m.r32(state + kHeld) & 0x2F)) o2_fade_set(1, 0, 0, 0);
    if (o2_fade_state(1) != 0) return;
    const u32 current = dialog(m), action = m.r8(current + 3);
    if (action == 1) {
        switch (m.r8(current + 6)) {
        case 0:
            m.w8(current + 1, 4); o2_entry_reset(2, 13); o2_group_set(2, 0x2000);
            for (u32 i = 0; i < 5; ++i) voice_release(m.r32(m.r32(kMenu) + 304 + 4 * i));
            break;
        case 1: m.w8(current + 1, 5); o2_entry_reset(2, 14); o2_group_set(2, 0x4000); break;
        case 2: {
            m.w8(current + 1, 6); o2_entry_reset(2, 15);
            o2_group_set(0, 0); o2_group_set(2, 0x8000);
            const u32 view = m.r32(0x800BCE48u), target = dialog(m);
            m.w16(target + 262, m.r16(view + 8)); m.w16(target + 264, m.r16(view + 12));
            break;
        }
        case 3: m.w8(current + 1, 7); break;
        default: break;
        }
    } else if (action == 2) {
        o2_entry_enter(0, 1); m.w8(dialog(m), 0); m.w8(dialog(m) + 1, 0);
    }
}
}

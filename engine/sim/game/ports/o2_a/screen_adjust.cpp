#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDialog = 0x80112DDCu, kView = 0x800BCE48u, kMenu = 0x80112DC4u;
u32 scene(Memory& m) { return m.r32(g::kScene); }
u32 view(Memory& m) { return m.r32(kView); }
}

void Game::o2_alignment_test() {
    cover(0x800DB3F8u);
    Frame frame(*this, 40);
    if (m.r32(scene(m) + 300) & 48) {
        o2_fade_set(1, 1, 0, 30); m.w8(m.r32(kDialog) + 1, 1);
        o2_group_set(2, 0x1000); sound_play(59);
    }
    m.w8(m.r32(kDialog) + 285, 0);
    u32 dialog = m.r32(kDialog);
    m.w16(dialog + 266, 60); m.w16(dialog + 274, 60); m.w8(dialog + 282, 0);
    const u32 colourTarget = m.r32(kDialog);
    m.w16(dialog + 268, 152); m.w16(dialog + 270, 65); m.w16(dialog + 272, 152);
    m.w16(dialog + 276, 320); m.w16(dialog + 278, 65); m.w16(dialog + 280, 320);
    m.w8(colourTarget + 283, 0);
    for (u32 field : {284u, 286u, 287u, 288u}) m.w8(m.r32(kDialog) + field, 0);
    constexpr u16 left[] = {60,72,83,99}, right[] = {65,77,88,104};
    constexpr u8 colours[][3] = {{255,0,0},{0,255,0},{0,0,255},{255,255,255}};
    for (u32 bar = 0; bar < 4; ++bar) {
        if (bar != 0) {
            dialog = m.r32(kDialog); m.w8(dialog + 290, colours[bar][0]);
            const u32 greenTarget = m.r32(kDialog);
            m.w16(dialog + 266, left[bar]); m.w16(dialog + 268, 152);
            m.w16(dialog + 270, right[bar]); m.w16(dialog + 272, 152);
            m.w16(dialog + 274, left[bar]); m.w16(dialog + 276, 320);
            m.w16(dialog + 278, right[bar]); m.w16(dialog + 280, 320);
            m.w8(greenTarget + 291, colours[bar][1]);
            m.w8(m.r32(kDialog) + 292, colours[bar][2]);
        } else {
            m.w8(m.r32(kDialog) + 290, colours[bar][0]);
            m.w8(m.r32(kDialog) + 291, colours[bar][1]);
            m.w8(m.r32(kDialog) + 292, colours[bar][2]);
        }
        for (u32 channel = 0; channel < 3; ++channel) m.w8(m.r32(kDialog) + 294 + channel, colours[bar][channel]);
        dialog = m.r32(kDialog);
        const u32 next = o2_poly_g4(dialog + 266, m.r32(dialog + 8), m.r32(m.r32(kMenu) + 8) + 4);
        m.w32(m.r32(kDialog) + 8, next);
    }
}

void Game::o2_screen_adjust_input() {
    cover(0x800DB714u);
    Frame frame(*this, 32);
    bool sounded = false;
    struct Direction { u32 button, field, flag; s32 bound, delta; };
    constexpr Direction directions[] = {{8,8,7,12,1},{4,8,9,-12,-1},{2,12,8,8,1},{1,12,6,0,-1}};
    for (const auto& direction : directions) {
        bool moved = false;
        if (m.r32(scene(m) + 304) & direction.button) {
            const u32 currentView = view(m), value = m.r32(currentView + direction.field);
            const bool within = direction.delta > 0 ? static_cast<s32>(value) < direction.bound
                                                    : static_cast<s32>(value) > direction.bound;
            if (within) { m.w32(currentView + direction.field, value + static_cast<u32>(direction.delta)); moved = true; }
        }
        o2_flag(direction.flag, moved ? 1 : 0);
        if (moved && !sounded) { sound_play(46); sounded = true; }
    }
    u32 state = scene(m);
    auto finish = [&] {
        o2_fade_set(1, 1, 0, 30); m.w8(m.r32(kDialog) + 1, 1);
        o2_group_set(0, 3); o2_group_set(2, 0x1000);
        const u32 currentView = view(m); m.w32(currentView, m.r32(currentView) | 2);
    };
    if ((m.r32(state + 300) & 16) && !(m.r32(state + 296) & 0x2F)) {
        finish(); sound_play(60); return;
    }
    state = scene(m);
    if ((m.r32(state + 300) & 32) && !(m.r32(state + 296) & 0x5F)) {
        const u32 dialog = m.r32(kDialog), currentView = view(m);
        m.w32(currentView + 8, static_cast<u32>(m.r16s(dialog + 262)));
        m.w32(currentView + 12, static_cast<u32>(m.r16s(dialog + 264)));
        finish(); sound_play(59); m.w32(scene(m) + 300, 0); return;
    }
    state = scene(m);
    if (m.r32(state + 292) & 16) {
        if (m.r16s(state + 104) >= 129) {
            const u32 currentView = view(m); m.w32(currentView + 8, -2); m.w32(currentView + 12, 0);
        }
        if (m.r32(scene(m) + 292) & 16) return;
    }
    if (m.r16(scene(m) + 96) & 0x1000) {
        const u32 currentView = view(m); m.w32(currentView + 8, -2); m.w32(currentView + 12, 0);
        sound_play(48);
    }
}
}

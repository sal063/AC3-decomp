#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDialog = 0x80112DDCu, kMenu = 0x80112DC4u;
constexpr u32 kHeld = 296, kPressed = 300, kRepeat = 304;
constexpr u32 kMusic = 0x398, kEffects = 0x399, kMono = 0x39A;
u32 scene(Memory& m) { return m.r32(g::kScene); }
u32 dialog(Memory& m) { return m.r32(kDialog); }
u32 slider(u32 v) { return 221 * v / 255; }
}

u32 Game::o2_sound_settings() {
    cover(0x800DAA6Cu);
    Frame frame(*this, 0x38);
    const u32 state = scene(m);
    const u32 mono = m.r8(state + kMono) & 1u;
    u32 action = 1;
    if (!mono) {
        req_stereo_set(0);
        req_volume_apply();
        o2_flag(6, m.r8(dialog(m) + 0x100) ? 1 : 0);
    } else {
        o2_flag(6, 2);
    }
    if (mono == 1) {
        req_stereo_set(1);
        req_volume_apply();
        o2_flag(7, m.r8(dialog(m) + 0x100) ? 1 : 0);
    } else {
        o2_flag(7, 2);
    }
    o2_flag(0x32, 0);
    const auto rectangle = [&](u32 entry) {
        const u32 d = dialog(m);
        m.w32(d + 8, o2_rectangle_flat(d + 0x12A, m.r32(d + 8), m.r32(m.r32(kMenu) + 8) + entry));
    };
    const auto colour = [&](u32 r, u32 g, u32 b) {
        m.w8(dialog(m) + 0x132, r);
        m.w8(dialog(m) + 0x133, g);
        m.w8(dialog(m) + 0x134, b);
    };
    for (u32 k = 1; k <= 2; ++k) {
        const u32 volume = state + (k == 1 ? kMusic : kEffects), y = k == 1 ? 0xC2u : 0x10Cu;
        u32 d = dialog(m);
        colour(0, 0, 0);
        m.w16(d + 0x12C, y);
        m.w16(d + 0x12A, slider(m.r8(volume)) + 0x37);
        m.w16(d + 0x12E, 2);
        m.w16(d + 0x130, 0x40);
        if (k == 1) m.w8(d + 0x135, 0);
        rectangle(8);
        d = dialog(m);
        m.w16(d + 0x12A, 0x38);
        m.w16(d + 0x12C, y);
        if (m.r8(d + 0x100) == k) {
            colour(0xCF, 0x8B, 0x17);
            o2_flag(0x32, k);
        } else {
            colour(0xE5, 0xE5, 0xD5);
        }
        m.w16(dialog(m) + 0x130, 0x40);
        m.w16(dialog(m) + 0x12E, slider(m.r8(volume)));
        rectangle(8);
    }
    const s32 amplitude = static_cast<s32>(o2_voice_amplitude() * 15u);
    {
        const u32 d = dialog(m);
        m.w16(d + 0x12A, 0x32);
        m.w16(d + 0x12C, 0xBD);
        m.w16(d + 0x130, 0xC);
        m.w16(d + 0x12E, static_cast<u32>(7 * (amplitude / 32767)));
        colour(0xCF, 0x8B, 0x17);
        rectangle(4);
    }
    if (m.r32(scene(m) + kRepeat) & 2) {
        const u32 k = m.r8(dialog(m) + 0x100);
        m.w8(dialog(m) + 0x100, k < 2 ? k + 1 : 0);
        sound_play(0x3D);
    }
    if (m.r32(scene(m) + kRepeat) & 1) {
        const u32 k = m.r8(dialog(m) + 0x100);
        m.w8(dialog(m) + 0x100, k ? k - 1 : 2);
        sound_play(0x3D);
    }
    o2_flag(5, 0);
    const u32 held = m.r32(scene(m) + kHeld);
    if (held & 1) o2_flag(5, 1);
    else if (held & 2) o2_flag(5, 2);
    for (const bool up : {true, false}) {
        if (!(m.r32(scene(m) + kRepeat) & (up ? 8u : 4u))) continue;
        const u32 k = m.r8(dialog(m) + 0x100);
        if (k == 0) {
            m.w8(state + kMono, mono ? 0 : m.r8(state + kMono) | 1u);
            sound_play(0x3D);
        } else if (k == 1 || k == 2) {
            const u32 volume = state + (k == 1 ? kMusic : kEffects), v = m.r8(volume);
            if (up ? v == 0xFF : v == 0) continue;
            m.w8(volume, up ? (v < 0xF0 ? v + 0x10 : 0xFF) : (v >= 0x11 ? v - 0x10 : 0));
            sound_play(0x2E);
        }
    }
    const auto leave = [&](u32 cue) {
        o2_fade_set(1, 1, 0, 30);
        m.w8(dialog(m) + 1, 1);
        o2_group_set(2, 0x1000);
        sound_play(cue);
        o2_flag(5, m.r8(dialog(m) + 6));
        o2_flag(6, 0);
        u32 voice = sound_play(0x5D);
        for (u32 n = 0; n < 4; ++n) {
            m.w32(m.r32(kMenu) + 0x130 + 4 * n, voice);
            voice = sound_play(0x5E + n);
        }
        m.w32(m.r32(kMenu) + 0x140, voice);
        action = 3;
    };
    if ((m.r32(scene(m) + kPressed) & 0x10) && !(m.r32(scene(m) + kHeld) & 0x2F)) {
        m.w8(dialog(m) + 0x101, m.r8(scene(m) + kMono) & 1u);
        m.w8(dialog(m) + 0x102, m.r8(scene(m) + kMusic));
        m.w8(dialog(m) + 0x103, m.r8(scene(m) + kEffects));
        leave(0x3C);
    }
    const u32 now = scene(m);
    if ((m.r32(now + kPressed) & 0x20) && !(m.r32(now + kHeld) & 0x5F)) {
        const u32 flags = m.r8(now + kMono);
        m.w8(now + kMono, m.r8(dialog(m) + 0x101) == 1 ? flags | 1u : flags & 0xFEu);
        m.w8(scene(m) + kMusic, m.r8(dialog(m) + 0x102));
        m.w8(scene(m) + kEffects, m.r8(dialog(m) + 0x103));
        leave(0x3B);
    }
    return o2_voice_control(action, 0);
}
}

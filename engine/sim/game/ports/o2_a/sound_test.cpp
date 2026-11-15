#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDialog = 0x80112DDCu, kMenu = 0x80112DC4u, kPanel = 0x80112DC0u;
constexpr u32 kScreen = 0x80112DD8u;
constexpr u32 kPlayer = 0x80112E0Cu;
constexpr u32 kHeld = 296, kPressed = 300, kRepeat = 304;
u32 scene(Memory& m) { return m.r32(g::kScene); }
u32 screen(Memory& m) { return m.r32(kScreen); }
u32 player(Memory& m) { return m.r32(kPlayer); }
u32 buffer(Memory& m) { return m.r32(scene(m) + 4); }
u32 panel(Memory& m) { return m.r32(m.r32(kPanel) + 0x5C); }
void add_prim(Memory& m, u32 prim) {
    const u32 entry = m.r32(m.r32(kMenu) + 8) + 4;
    m.w32(prim, (m.r32(prim) & 0xFF000000u) | (m.r32(entry) & 0xFFFFFFu));
    m.w32(entry, (m.r32(entry) & 0xFF000000u) | (prim & 0xFFFFFFu));
}
s32 magnitude(s32 v) { return v < 0 ? -v : v; }
u32 random_track(Memory& m) { return static_cast<u32>(m.r32s(scene(m) + 0x20) % 32); }
}

u32 Game::o2_sound_test() {
    cover(0x800DD1D0u);
    Frame frame(*this, 0x90);
    const u32 bands = frame.at(0x10), command = frame.at(0x60);
    m.w32(command, 0);
    for (u32 k = 0; k < 18; ++k) m.w32(bands + 4 * k, 0);
    u32 left = o2_voice_samples(0), right = o2_voice_samples(1), band = bands;
    for (s32 i = 0; i < 100; ++i) {
        s32 sum = m.r32s(band) + magnitude(m.r16s(left));
        m.w32(band, static_cast<u32>(sum));
        sum += magnitude(m.r16s(right));
        m.w32(band, static_cast<u32>(sum));
        left += 2;
        right += 2;
        if (i % 5 != 4) continue;
        m.w32(band, static_cast<u32>(sum / 10));
        m.w32(band, static_cast<u32>(7 * (sum / 10) / 32768));
        if (i / 5 < 18) band += 4;
    }
    const u32 bars = screen(m) + 2160 * buffer(m), tops = bars + 0x10E0;
    const auto each_bar = [&](u32 base) {
        for (u32 k = 0; k < 18; ++k)
            for (s32 j = 0; j < m.r32s(bands + 4 * k); ++j) add_prim(m, base + 20 * (6 * k + static_cast<u32>(j)));
    };
    each_bar(bars);
    add_prim(m, screen(m) + 12 * buffer(m) + 0x21C0);
    each_bar(tops);
    add_prim(m, screen(m) + 12 * buffer(m) + 0x21D8);
    u32 mark = screen(m) + 100 * buffer(m) + 0x21F0;
    o2_voice_amplitude();
    for (u32 t = 0; t < 5; ++t, mark += 20) {
        const s32 speed = m.r16s(screen(m) + 0x23AC + 2 * t);
        const s32 rx = m.r16s(screen(m) + 0x2398 + 2 * t), ry = m.r16s(screen(m) + 0x23A2 + 2 * t);
        const s32 c = rcos(wrap::mul(m.r16s(screen(m) + 0x23B6), speed));
        m.w16(mark + 8, static_cast<u32>(static_cast<s32>(wrap::div(76 * c, rx).lo) / 4096 + 0x25));
        const s32 s = rsin(wrap::mul(m.r16s(screen(m) + 0x23B6), speed));
        m.w16(mark + 10, static_cast<u32>(static_cast<s32>(wrap::div(84 * s, ry).lo) / 4096 + 0x12E));
        add_prim(m, mark);
    }
    add_prim(m, screen(m) + 12 * buffer(m) + 0x2380);
    const u32 cursor_at = screen(m) + 0x23B8;
    if (m.r32(scene(m) + kRepeat) & 8) {
        const u32 c = m.r8(cursor_at);
        m.w8(cursor_at, c == 1 ? 0u : c == 7 ? 2u : c + 1);
    }
    if (m.r32(scene(m) + kRepeat) & 4) {
        const u32 c = m.r8(cursor_at);
        m.w8(cursor_at, c == 0 ? 1u : c == 2 ? 7u : c - 1);
    }
    if (m.r32(scene(m) + kRepeat) & 3) {
        static constexpr u8 kVertical[8] = {2, 4, 0, 0, 1, 1, 1, 1};
        const u32 c = m.r8(cursor_at);
        if (c < 8) m.w8(cursor_at, kVertical[c]);
    }
    const auto next_track = [&](bool step, u32 delta) {
        if (step && !m.r8(screen(m) + 0x23B9)) m.w32(player(m) + 8, m.r32(player(m) + 8) + delta);
        if (m.r8(screen(m) + 0x23B9) == 1) m.w32(player(m) + 8, random_track(m));
    };
    const auto leave = [&](u32 voices) {
        o2_entry_enter(0, 1);
        m.w8(m.r32(kDialog), 0);
        m.w8(m.r32(kDialog) + 1, 0);
        cd_reset(0, 0);
        u32 voice = sound_play(0x5D);
        for (u32 n = 0; n + 1 < voices; ++n) {
            m.w32(m.r32(kMenu) + 0x130 + 4 * n, voice);
            voice = sound_play(0x5E + n);
        }
        m.w32(m.r32(kMenu) + 0x130 + 4 * (voices - 1), voice);
        return voice;
    };
    if ((m.r32(scene(m) + kRepeat) & 0x10) && !(m.r32(scene(m) + kHeld) & 0x20)) {
        switch (m.r8(cursor_at)) {
        case 0: next_track(true, 0xFFFFFFFFu); break;
        case 1: next_track(true, 1); break;
        case 2: m.w8(screen(m) + 0x23B9, 0); break;
        case 3: m.w8(screen(m) + 0x23B9, 1); break;
        case 4: m.w32(command, 1); break;
        case 5: m.w32(command, 3); break;
        case 6: m.w32(command, 2); break;
        case 7: leave(5); break;
        default: break;
        }
    }
    m.w8(panel(m) + 1, m.r8(cursor_at));
    if (!cd_pending(1) && (m.r32(player(m)) & 1)) next_track(true, 1);
    {
        const s32 track = m.r32s(player(m) + 8);
        if (track < 0) m.w32(player(m) + 8, 32);
        else if (track >= 33) m.w32(player(m) + 8, 0);
    }
    if (kcd_stream_flags() & 0x40000000u) m.w32(player(m), m.r32(player(m)) | 2u);
    else m.w32(player(m), m.r32(player(m)) & ~2u);
    o2_voice_control(m.r32(command), m.r32(player(m) + 8));
    const s32 played = static_cast<s32>(stream_played());
    o2_text_font(3);
    o2_text_semi(2);
    o2_text_rgb(0x80, 0x80, 0x80);
    o2_text_at(0x66, 0x90);
    o2_text_printf_zero(0x800CC834u, m.r32(player(m) + 8), 0, 0);
    o2_text_at(0x7C, 0x90);
    o2_text_printf(m.r32(0x8010A568u + 4 * m.r32(player(m) + 8)), 0, 0, 0);
    o2_text_font(4);
    o2_text_semi(2);
    o2_text_at(0xC8, 0x18A);
    bool shown = true;
    if (kcd_stream_flags() & 0x40000000u) {
        const s32 r = m.r32s(scene(m) + 0x20) % 60;
        shown = ((wrap::mult(r, static_cast<s32>(0x88888889u)).hi + r) >> 4) != (r >> 31);
    }
    if (shown)
        o2_text_printf_zero(0x800CC838u, static_cast<u32>(played / 3600),
                            static_cast<u32>(played / 60 - 60 * (played / 3600)),
                            static_cast<u32>(100 * (played % 60) / 60));
    if (!(kcd_stream_flags() & 0x2000000u) || (kcd_stream_flags() & 0x40000000u))
        m.w16(screen(m) + 0x23B6, m.r16(screen(m) + 0x23B6) + 1);
    const u32 now = scene(m);
    if (!(m.r32(now + kPressed) & 0x20)) return 0;
    if (const u32 held = m.r32(now + kHeld) & 0x5F) return held;
    const u32 voice = leave(3);
    m.w32(scene(m) + kPressed, 0);
    return voice;
}
}

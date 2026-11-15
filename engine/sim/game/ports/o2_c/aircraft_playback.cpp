#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
u32 Game::o2_aircraft_playback_tick() {
    cover(0x801065C8u);
    Frame frame(*this, 24);
    constexpr u32 kMenu = 0x80112DC4u, kPanel = 0x80112DACu;
    o2_aircraft_camera();
    o2_aircraft_script_tick();
    o2_aircraft_markers_draw();
    o2_aircraft_captions_draw();
    if (!stream_played()) {
        const u32 menu = m.r32(kMenu), timer = m.r32(menu + 0x14C);
        if (static_cast<s32>(timer) >= 0) {
            m.w32(menu + 0x14C, timer - 1);
            return 0;
        }
    }
    const u32 menu = m.r32(kMenu);
    const u32 packet = face_draw(m.r32(menu + 8), m.r32(menu + 0x274));
    m.w32(m.r32(kMenu) + 0x274, packet);
    const u32 played = stream_played_of(static_cast<u32>(o2_selected_id()));
    auto startGuard = [&]() {
        const u32 scene = m.r32(g::kScene);
        return m.r32(scene + 0x128) & 0x2Fu;
    };
    if (played != 0xFFFFFFFFu) {
        if (!(m.r32(m.r32(g::kScene) + 0x12C) & 16)) return 0;
        const u32 held = startGuard();
        if (held) return held;
    }
    const u32 panel = m.r32(kPanel);
    m.w8(panel + 0x9D4, m.r8(panel + 0x9D4) + 1);
    m.w8(m.r32(kPanel) + 0x9D5, 0); m.w8(m.r32(kPanel) + 0x9D6, 1);
    o2_entry_leave(0, 5); o2_entry_leave(0, 7);
    o2_fade_set(1, 3, 0xFFFFFFFFu, 0xFFFFFFFFu);
    sound_play(91);
    if (!(m.r32(m.r32(g::kScene) + 0x12C) & 16)) return 0;
    const u32 held = startGuard();
    if (held) return held;
    return sound_play(60);
}

u32 Game::o2_aircraft_preview_tick() {
    cover(0x8010606Cu);
    Frame frame(*this, 32);
    constexpr u32 kMenu = 0x80112DC4u, kPanel = 0x80112DACu;
    o2_aircraft_camera();
    const s32 timer = m.r32s(m.r32(kMenu) + 0x50);
    m.w8(m.r32(kPanel) + 0x9D9, timer < 40 ? 1 : timer < 270 ? 2 : 0);
    const u32 hint = timer < 30 ? 0 : timer < 35 ? 1 : timer < 45 ? 2 : timer < 50 ? 3 : timer < 55 ? 4 : 0;
    m.w8(m.r32(kPanel) + 0x9D8, hint);
    o2_flag(1, m.r8(m.r32(kPanel) + 0x9D8));
    if (m.r32s(m.r32(kMenu) + 0x50) >= 121) {
        const u32 panel = m.r32(kPanel);
        m.w8(panel + 0x9D4, m.r8(panel + 0x9D4) + 1);
        m.w8(m.r32(kPanel) + 0x9D5, 0);
    }
    if (m.r32s(m.r32(kMenu) + 0x50) < 121) {
        const u32 scene = m.r32(g::kScene);
        if ((m.r32(scene + 0x12C) & 16) && !(m.r32(scene + 0x128) & 0x2F)) {
            const u32 panel = m.r32(kPanel);
            m.w8(panel + 0x9D4, m.r8(panel + 0x9D4) + 1); m.w8(m.r32(kPanel) + 0x9D5, 0);
            o2_entry_reset(0, 5); o2_entry_reset(0, 7);
            o2_fade_set(1, 2, 120, 120);
            o2_flag(1, 0); o2_flag(2, 0);
            m.w32(m.r32(kMenu) + 0x1A4, 400);
        }
    }
    const u32 menu = m.r32(kMenu), next = m.r32(menu + 0x50) + 1;
    m.w32(menu + 0x50, next);
    return next;
}

u32 Game::o2_aircraft_playback_setup() {
    cover(0x801062B8u);
    Frame frame(*this, 64);
    constexpr u32 kMenu = 0x80112DC4u, kPanel = 0x80112DACu;
    const u32 panel = m.r32(kPanel), menu = m.r32(kMenu);
    m.w16(0x80112E2Eu, 0); m.w16(0x80112E2Cu, 0);
    m.w32(panel + 0x9D0, 0); m.w32(menu + 0x50, 0);
    m.w8(panel + 0x9D5, m.r8(panel + 0x9D5) + 1); m.w8(m.r32(kPanel) + 0x9D8, 0);
    o2_voice_control(1, static_cast<u32>(m.r16s(m.r32(g::kScene) + 0x38C)));
    for (u32 buffer = 0; buffer < 2; ++buffer) {
        const u32 packets = m.r32(m.r32(kMenu) + 0x38 + 4 * buffer);
        for (u32 marker = 0; marker < 32; ++marker) {
            const u32 packet = packets + 0xB8C + 40 * marker;
            m.w8(packet + 3, 9); m.w8(packet + 7, 0x2C); m.w16(packet + 22, 15);
            m.w8(packet + 4, 128); m.w8(packet + 5, 128); m.w8(packet + 6, 128);
        }
    }
    auto sprite = [&](u32 packet, u32 command, u32 width, u32 height) {
        m.w8(packet + 3, 4); m.w8(packet + 7, command);
        m.w8(packet + 4, 128); m.w8(packet + 5, 128); m.w8(packet + 6, 128);
        m.w16(packet + 16, width); m.w16(packet + 18, height);
    };
    for (u32 buffer = 0; buffer < 2; ++buffer) {
        const u32 current = m.r32(kPanel), modes = current + 0x924 + buffer * 36;
        for (u32 mode = 0; mode < 3; ++mode) {
            m.w32(frame.at(16), 0);
            set_draw_mode(modes + 12 * mode, 0, 0, mode == 0 ? 0 : mode == 1 ? 0x4C : 0x0B, 0);
        }
        sprite(current + 0x8D4 + 40 * buffer, 0x66, 60, 69);
        sprite(current + 0x8E8 + 40 * buffer, 0x66, 60, 69);
    }
    for (u32 buffer = 0; buffer < 2; ++buffer) {
        const u32 current = m.r32(kPanel);
        sprite(current + 0x96C + 40 * buffer, 0x64, 56, 12);
        sprite(current + 0x980 + 40 * buffer, 0x64, 56, 12);
    }
    o2_aircraft_camera(); o2_aircraft_markers_draw();
    return o2_aircraft_captions_draw();
}

u32 Game::o2_aircraft_script_begin() {
    cover(0x801067B0u);
    Frame frame(*this, 32);
    constexpr u32 kMenu = 0x80112DC4u, kPanel = 0x80112DACu, kScripts = 0x801127ACu;
    const s32 mission = m.r16s(m.r32(g::kScene) + 0x38C);
    if (m.r16s(kScripts + static_cast<u32>(mission) * 12) == -1) {
        const u32 menu = m.r32(kMenu);
        m.w16(menu + 0x84, m.r16(menu + 0x84) + 1);
        return 0;
    }
    o2_aircraft_panel_alloc(); o2_aircraft_panel_records(); o2_aircraft_panel_slots(); o2_aircraft_panel_reset();
    o2_voice_script(kScripts);
    auto faceMode = [&]() { return m.r16s(0x80112AACu + static_cast<u32>(m.r16s(m.r32(g::kScene) + 0x38C)) * 4 + 2); };
    if (faceMode() == 1) {
        radio_face(2, 0, 0);
        snd_nothing_a();
    }
    if (faceMode() == 0) radio_face(2, 0, 0);
    o2_upload_packed_tims(m.r32(m.r32(m.r32(kMenu) + 0x20) + 12));
    m.w32(m.r32(kPanel) + 0x9C8, m.r32(m.r32(kMenu) + 0x1C0));
    while (m.r16s(m.r32(m.r32(kPanel) + 0x9C8)) == -1) {
        const u32 command = m.r32(m.r32(kPanel) + 0x9C8);
        o2_aircraft_command(m.r32(0x80112B9Cu + 4 * m.r8(command + 2)), 0xFFFFFFFFu);
        const u32 panel = m.r32(kPanel);
        m.w32(panel + 0x9C8, m.r32(panel + 0x9C8) + 16);
    }
    m.w8(m.r32(kPanel) + 0x9CE, 0);
    o2_aircraft_materials();
    const u32 menu = m.r32(kMenu);
    m.w16(menu + 0x86, m.r16(menu + 0x86) + 1);
    m.w8(m.r32(kPanel) + 0x9D4, 0); m.w8(m.r32(kPanel) + 0x9D5, 0); m.w8(m.r32(kPanel) + 0x9D6, 0);
    o2_fade_set(2, 0, 0, 0); o2_fade_set(3, 0, 0, 0); o2_fade_set(4, 0, 0, 0);
    o2_text_font(7); o2_text_semi(0xFFFFFFFFu); o2_text_fold(1, 1, 1); o2_text_frame_mode(1);
    return o2_hangar_sounds_restart(256);
}
}

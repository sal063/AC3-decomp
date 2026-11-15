#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
u32 Game::o2_aircraft_preview_driver() {
    cover(0x80106A34u);
    Frame frame(*this, 64);
    constexpr u32 kPanel = 0x80112DACu, kMenu = 0x80112DC4u;
    const u32 panel = m.r32(kPanel), table = m.r32(0x80112BE0u + 4 * m.r8(panel + 0x9D4));
    const u32 callback = m.r32(table + 4 * m.r8(panel + 0x9D5));
    switch (callback) {
    case 0x80106000u: o2_aircraft_preview_begin(); break;
    case 0x8010606Cu: o2_aircraft_preview_tick(); break;
    case 0x801062B8u: o2_aircraft_playback_setup(); break;
    case 0x8010655Cu: o2_aircraft_intro(); break;
    case 0x801065C8u: o2_aircraft_playback_tick(); break;
    case 0x8010673Cu: o2_aircraft_outro(); break;
    case 0x80106780u: o2_aircraft_idle_draw(); break;
    default:
        if (callback >= 0x800F0000u && callback < 0x8010A000u) unported(callback, "invalid aircraft preview callback");
        call_by_address(callback, "aircraft preview callback outside o2_c", panel); break;
    }
    if (!m.r8(m.r32(kPanel) + 0x9D6)) {
        const u32 menu = m.r32(kMenu);
        m.w32(m.r32(kMenu) + 0x274, o2_aircraft_terrain(m.r32(menu + 0x274)));
    }
    u32 menu = m.r32(kMenu);
    m.w32(m.r32(kMenu) + 0x274, o2_aircraft_timer_draw(m.r32(menu + 0x274), 0));
    auto prepend = [&](u32 packet, u32 ot) {
        m.w32(packet, (m.r32(packet) & 0xFF000000u) | (m.r32(ot) & 0xFFFFFFu));
        m.w32(ot, (m.r32(ot) & 0xFF000000u) | (packet & 0xFFFFFFu));
    };
    auto drawArea = [&](u32 source, u32 local) {
        m.w32(local, m.r32(source)); m.w32(local + 4, m.r32(source + 4));
        const u32 currentMenu = m.r32(kMenu), packet = m.r32(currentMenu + 0x274), ot = m.r32(currentMenu + 0x14);
        m.w32(currentMenu + 0x274, packet + 12);
        set_draw_area(packet, local); prepend(packet, ot);
    };
    drawArea(0x800CD784u, frame.at(16));
    const u32 currentPanel = m.r32(kPanel);
    if (m.r8(currentPanel + 0x9D4) && m.r8(currentPanel + 0x9D5)) {
        const s32 centreX = wrap::add(m.r32s(currentPanel + 32) / 128, 128);
        const s32 centreY = wrap::add(wrap::neg(m.r32s(currentPanel + 40)) / 128, 128);
        const s32 originX = static_cast<s16>(centreX) - 128;
        menu = m.r32(kMenu);
        u32 packet = m.r32(menu + 0x274) + 16;
        const u32 ot = m.r32(menu + 0x14);
        constexpr u32 colours[] = {0xC86038, 0xC86038, 0xFFFFFF, 0xFFFFFF, 0x68D0E8, 0x68D0E8,
                                   0x6850F0, 0x6850F0, 0xD0D090, 0, 0xC86038};
        for (u32 slot = 0; slot < 32; ++slot) {
            const u32 activePanel = m.r32(kPanel);
            if (!(m.r32(activePanel + 0x9D0) & (1u << slot))) continue;
            const u32 marker = m.r32(activePanel + 0x9DC + 4 * slot);
            const s32 icon = m.r16s(marker);
            if (icon == 9 || icon >= 11) continue;
            m.w8(packet + 3, 3); m.w8(packet + 7, 0x60);
            m.w16(packet + 8, (m.r16s(marker + 12) / 128 - originX) / 2 + 275);
            m.w16(packet + 10, wrap::add(wrap::sub(-m.r16s(marker + 16) / 128, centreY), 292));
            m.w16(packet + 12, 3); m.w16(packet + 14, 6);
            if (icon >= 0) {
                const u32 colour = colours[icon];
                m.w8(packet + 4, colour); m.w8(packet + 5, colour >> 8); m.w8(packet + 6, colour >> 16);
            }
            if (!m.r8(m.r32(kPanel) + 0x9D6)) prepend(packet, ot);
            packet += 16;
        }
        m.w32(m.r32(kMenu) + 0x274, packet);
        m.w8(packet + 3, 9); m.w8(packet + 7, 0x2C);
        for (u32 channel = 0; channel < 3; ++channel) m.w8(packet + 4 + channel, 128);
        const s32 halfX = static_cast<s16>(centreX) / 2;
        for (u32 corner = 0; corner < 4; ++corner) {
            m.w16(packet + 8 + 8 * corner, (corner & 1 ? 403 : 275) - halfX);
            m.w16(packet + 10 + 8 * corner, wrap::sub(corner & 2 ? 420 : 164, centreY));
            m.w8(packet + 12 + 8 * corner, corner & 1 ? 128 : 0);
            m.w8(packet + 13 + 8 * corner, corner & 2 ? 128 : 0);
        }
        m.w16(packet + 14, 0x5714); m.w16(packet + 22, 5);
        if (!m.r8(m.r32(kPanel) + 0x9D6)) prepend(packet, ot);
        m.w32(m.r32(kMenu) + 0x274, packet + 40);
    }
    drawArea(0x800CD78Cu, frame.at(24));
    const u32 scene = m.r32(g::kScene);
    if ((m.r32(scene + 0x12C) & 32) && !(m.r32(scene + 0x128) & 0x5F)) {
        radio_face(255, 0, 0);
        const u32 entry = m.r32(m.r32(kMenu) + 0x16C);
        if (m.r16s(entry + 0x46) == 2) {
            m.w16(entry + 0x46, 3); o2_fade_set(1, 0, 0, 0);
        } else {
            o2_hangar_effect2_begin(); o2_fade_set(1, 3, -1, -1);
            menu = m.r32(kMenu);
            s32 ticks = 29;
            if (!m.r8(m.r32(kPanel) + 0x9D4)) {
                const s32 kind = m.r16s(m.r32(menu + 0x16C) + 0x46);
                if (!kind) ticks = 0;
                else if (kind == 1) {
                    ticks = wrap::mul(wrap::sub(m.r32s(menu + 0x1A4), 258), 29) / 64;
                    if (ticks < 0) ticks = 0;
                    if (ticks >= 30) ticks = 29;
                }
            }
            m.w32(menu + 0x1A4, ticks);
            o2_entry_leave(0, 5); o2_entry_leave(0, 7); sound_play(91); sound_play(59);
        }
        m.w8(m.r32(kPanel) + 0x9D6, 2);
    }
    if (m.r16s(m.r32(m.r32(kMenu) + 0x16C) + 0x46) != 3) return 3;
    if (m.r8(m.r32(kPanel) + 0x9D6) == 1) {
        o2_voice_control(3, m.r16s(m.r32(g::kScene) + 0x38C));
        menu = m.r32(kMenu); m.w16(menu + 0x84, 2); m.w16(menu + 0x86, 0);
    }
    if (m.r8(m.r32(kPanel) + 0x9D6) == 2) {
        for (u32 offset : {0x7Cu, 0x7Du, 0x7Eu}) m.w8(m.r32(kMenu) + offset, 0);
        o2_entry_enter(0, 1); o2_voice_control(3, m.r16s(m.r32(g::kScene) + 0x38C));
    }
    return o2_aircraft_panel_release();
}
}

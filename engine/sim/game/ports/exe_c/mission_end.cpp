#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kStatus = 0x800BD058u, kPhaseKind = 0x800BCEA8u;
u32 mission(Memory& m) { return m.r32(g::kMission); }
u32 world(Memory& m) { return m.r32(g::kWorld); }
u32 scene(Memory& m) { return m.r32(g::kScene); }
}

u32 Game::mission_ending(u32 retainedPhrase) {
    cover(0x8005BC64u);
    Frame frame(*this, 0x20);
    const u32 status = m.r32(kStatus);
    if (m.r8(mission(m) + 63) == 1 && (status & 0x6000u) && m.r32s(world(m) + 12) < 16) {
        const u32 packet = noise_fade(m.r32(world(m) + 16) + 8, m.r32(world(m) + 20),
            static_cast<s32>(4096u - (m.r32(world(m) + 12) << 8)));
        m.w32(world(m) + 20, packet);
    }
    if (m.r32s(world(m) + 12) >= 61 && !radio_busy()) {
        if (status & 0x8000u) player_mode_800762D0();
        else player_mode_first_stage();
        u32 phrase = retainedPhrase;
        if (status & 0x300u) phrase = 11;
        else if (status & 0x400u) phrase = 19;
        else if (status & 0x800u) phrase = 29;
        else if (status & 0x2000u) phrase = 15;
        else if (status & 0x4000u) phrase = 27;
        else if (status & 0x1000u) phrase = 39;
        else if (status & 0x8000u) phrase = 45;
        message_show(phrase); request_exit(0x800u, m.r32(world(m) + 8));
    }
    if (!(player_mode() & 0x7000u)) return 0;
    radio_stop(); m.w8(mission(m) + 37, m.r8(mission(m) + 40)); request_exit(0x2000u, 1);
    const u32 flags = m.r32(scene(m)) | 0x10000000u;
    m.w32(scene(m), flags); return flags;
}

u32 Game::mission_end_transition(u32 retainedEvent, u32 retainedDelay) {
    cover(0x8005BE0Cu);
    Frame frame(*this, 0x28);
    u32 mode = player_mode(), phrase = 255;
    const u32 variant = m.r32(world(m) + 8);
    auto handoff = [&] {
        radio_stop(); m.w8(mission(m) + 37, m.r8(mission(m) + 40));
        m.w32(world(m) + 8, m.r8(mission(m) + 40)); request_exit(0x2000u, 1);
        const u32 flags = m.r32(scene(m)) | 0x10000000u;
        m.w32(scene(m), flags); return flags;
    };
    auto animation = [&] {
        m.w8(mission(m) + 49, 2); m.w8(mission(m) + 48, 1);
        m.w16(mission(m) + 50, 30); m.w16(mission(m) + 26, 0);
    };
    auto finish = [&] {
        wingmen_route_reset();
        if (m.r8(mission(m) + 41)) sound_reset(); else snd_nothing_b();
        mission_end();
    };
    const u32 status = m.r32(kStatus);
    if (variant & 2u) {
        if (mode & 0x7000u) return handoff();
        u32 event = retainedEvent, delay = retainedDelay;
        if (status & 0x300u) {
            phrase = 12; event = delay = 270;
            if (!m.r32(world(m) + 12)) {
                voice_level(0, 0, 150); voice_level(1, 30, 150); voice_level(2, 20, 150);
            }
        } else if (status & 0x400u) { phrase = 20; event = delay = 360; }
        else if (status & 0x800u) { phrase = 43; event = delay = 360; }
        else if (status & 0x6000u) { phrase = 49; event = 0xFFFFFFFFu; delay = 120; }
        else if (status & 0x1000u) { phrase = 47; event = delay = 180; }
        else if (status & 0x8000u) { event = delay = 90; mode = 0x200; }
        if (m.r32(world(m) + 12) == event) animation();
        if ((mode & 0x200u) && m.r32s(world(m) + 12) > static_cast<s32>(delay)
            && !radio_busy() && !m.r8(mission(m) + 48)) finish();
    } else if (variant & 0x20u) {
        if (mode & 0x7000u) handoff();
        if (m.r32(world(m) + 12) == 120) animation();
        if (m.r32s(world(m) + 12) >= 120 && !radio_busy() && !m.r8(mission(m) + 48)) finish();
    } else if (variant) {
        if (status & 0x300u) phrase = 41;
        else if (status & 0x400u) phrase = 42;
        else if (status & 0x800u) phrase = 44;
        else if (status & 0x1000u) phrase = 48;
        if (!m.r32(world(m) + 12)) radio_stop();
        if (m.r32s(world(m) + 12) >= 120) {
            wingmen_route_reset(); sound_reset(); radio_stop();
            m.w8(mission(m) + 37, m.r8(mission(m) + 40));
            request_exit(0x1000u, m.r8(mission(m) + 40)); view_player();
        }
    } else if (m.r32s(world(m) + 12) >= 150 && !radio_busy()) finish();
    if (!m.r32(world(m) + 12) && phrase != 255) { message_show(phrase); return 0; }
    return 255;
}

u32 Game::mission_end_dispatch(u32 retainedEvent, u32 retainedDelay) {
    cover(0x8005C270u);
    Frame frame(*this, 0x28);
    const u32 record = 0x800B1FC4u + 4 * m.r32(kPhaseKind);
    const u32 phase = m.r32(world(m) + 4);
    if (phase == 0x400u) return mission_ending(retainedDelay);
    if (phase == 0x2000u) return mission_end_transition(retainedEvent, retainedDelay);
    if (phase == 0x1000u) return 0x1000u;
    const u32 mode = player_mode(), kind = m.r8(record);
    const bool offer = kind == 2 || kind == 3 ||
        ((kind < 2 || kind == 4 || kind == 5 || kind == 6) && m.r32s(world(m) + 12) < 300);
    if (kind != 7 && !(mode & 0x200u) && offer && !(mode & 0x40u)) {
        if (m.r32(scene(m) + 300) & 0x40u) {
            player_mode_second_stage(); hud_message(9); rumble_clear();
            m.w32(mission(m) + 20, 0); m.w8(mission(m) + 28, 255);
        }
        if (m.r32(world(m) + 12) & 0x10u) {
            m.w32(frame.at(0x10), 33536); m.w32(frame.at(0x14), 0x80024548u);
            const u32 first = text_line(m.r32(world(m) + 16), m.r32(world(m) + 20), 80, 208, 33536, 0x80024548u);
            m.w32(world(m) + 20, first);
            m.w32(frame.at(0x10), 33536); m.w32(frame.at(0x14), 0x8002455Cu);
            const u32 second = text_line(m.r32(world(m) + 16), m.r32(world(m) + 20), 96, 216, 33536, 0x8002455Cu);
            m.w32(world(m) + 20, second);
        }
    }
    if (m.r32(world(m) + 12) == 3600) message_show(10);
    auto queue = [&](u32 outcome) { request_exit(0x2000u, outcome); return 0u; };
    if (mode & 0x7000u) return queue(1);
    if (m.r32s(world(m) + 12) >= 5400) return queue(4);
    if (mode & 0x80u) return queue(2);
    if (mode & 0x100u) return queue(16);
    if (mode & 0x200u) return queue(0);
    if (m.r8(record) != 7) return 7;
    if (m.r16s(mission(m) + 8) >= 390) { queue(2); m.w8(mission(m) + 47, 1); }
    if (m.r16s(mission(m) + 8) < 0) { queue(32); m.w8(mission(m) + 47, 0); }
    if (m.r32(world(m) + 12) != 900) return 900;
    const bool success = m.r8(mission(m) + 10) == 1;
    queue(success ? 2u : 32u); const u32 config = mission(m); m.w8(config + 47, success);
    return config;
}
}

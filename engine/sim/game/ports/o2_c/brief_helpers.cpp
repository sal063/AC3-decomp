#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kBriefing = 0x80112DB0u, kMenu = 0x80112DC4u;
constexpr u32 kVoiceAvailable = 0x1698, kVoiceRequest = 0x169C;
constexpr u32 kVoiceStreamOverride = 0x12C, kDefaultVoiceStream = 0x912;
constexpr u32 kSequence = 0x80112E04u, kSequenceSource = 0x80112E08u;
constexpr u32 kSequenceDurations = 0x8010E634u;
constexpr u32 kSequenceTime = 0x3D4, kSequenceFinished = 0x3D2;
constexpr u32 kAircraftPanel = 0x80112DACu, kPanelBounds = 0x9C8;
constexpr u32 kMoviePanel = 0x80112E18u;
}

u32 Game::o2_brief_voice_request() {
    cover(0x800F072Cu);
    Frame frame(*this, 24);
    const u32 briefing = m.r32(kBriefing);
    if (!m.r32(briefing + kVoiceAvailable)) return 0;
    const u32 menu = m.r32(kMenu);
    const u32 stream = m.r32(menu + kVoiceStreamOverride)
        ? m.r16(menu + kVoiceStreamOverride) : kDefaultVoiceStream;
    const u32 request = briefing + kVoiceRequest;
    m.w16(request, stream);
    for (u32 field : {2u, 4u, 6u}) m.w16(request + field, 0);
    m.w32(request + 8, 0);
    cd_request_stream(request, 1, 0);
    return 1;
}

u32 Game::o2_brief_voice_stop() {
    cover(0x800F07B0u);
    Frame frame(*this, 24);
    if (!m.r32(m.r32(kBriefing) + kVoiceAvailable)) return 0;
    cd_reset(0, 0);
    return 1;
}

u32 Game::o2_sequence_tick() {
    cover(0x800F59B4u);
    const u32 sequence = m.r32(kSequence);
    const u32 stage = m.r8(sequence);
    const u32 time = m.r16(sequence + kSequenceTime) + 1;
    m.w16(sequence + kSequenceTime, time);
    if (static_cast<s16>(time) < m.r16s(kSequenceDurations + 2 * stage)) return 1;
    if (stage == 0) m.w8(sequence, 1);
    else m.w8(sequence + kSequenceFinished, 1);
    const u32 current = m.r32(kSequence);
    m.w16(current + kSequenceTime, 0);
    return current;
}

u32 Game::o2_sequence_seconds() {
    cover(0x800F5A24u);
    const u32 sequence = m.r32(kSequence);
    if (!m.r8(sequence)) return 0;
    const u32 seconds = m.r16(m.r32(g::kScene) + 0x3BA) / 30;
    if (m.r16s(sequence + 2) >= static_cast<s32>(seconds)) return 0;
    const u32 time = m.r16(sequence + 2) + 1;
    m.w16(sequence + 2, time);
    return time;
}

u32 Game::o2_sequence_reset() {
    cover(0x800F5AC4u);
    m.w32(kSequenceSource, m.r32(g::kScene) + 0x3B8);
    for (u32 entry = 0; entry < 64; ++entry)
        m.w8(m.r32(kSequence) + 0x13 + 4 * entry, 0);
    return 0;
}

u32 Game::o2_panel_video_request() {
    cover(0x800FC854u);
    Frame frame(*this, 32);
    constexpr u32 kReadyCallback = 0x800FC750u;
    m.w32(frame.at(16), kReadyCallback);
    const u32 panel = m.r32(kMoviePanel);
    return kcd_request_video(m.r32(panel + 0x11058), m.r32(panel + 0x11060),
                             m.r32(panel + 0x1105C), 0x1000, kReadyCallback);
}

u32 Game::o2_aircraft_panel_centre() {
    cover(0x80102F5Cu);
    const u32 panel = m.r32(kAircraftPanel);
    const u32 bounds = m.r32(panel + kPanelBounds);
    m.w32(panel + 0x24, 0);
    m.w32(panel + 0x20, static_cast<u32>(m.r16s(bounds + 6) - 128) << 7);
    const u32 z = static_cast<u32>(m.r16s(bounds + 10) - 128) << 7;
    m.w32(panel + 0x28, z);
    return z;
}

u32 Game::o2_sequence_clear_banks() {
    cover(0x800F5FF8u);
    for (u32 bank : {0x110u, 0x270u})
        for (u32 entry = 0; entry < 8; ++entry)
            for (u32 field : {41u, 40u, 0u})
                m.w8(m.r32(kSequence) + bank + 44 * entry + field, 0);
    m.w8(m.r32(kSequence) + 0x3D0, 0);
    const u32 sequence = m.r32(kSequence);
    m.w8(sequence + 0x3D1, 0);
    return sequence;
}

u32 Game::o2_panel_decoder_init() {
    cover(0x800FC5A8u);
    Frame frame(*this, 24);
    hw->dec_dct_reset(0);
    hw->dec_dct_out_callback(0x800FCB44u);
    dct_vlc_build(m.r32(kMoviePanel) + 0x50);
    m.w8(m.r32(kMoviePanel) + 4, 0);
    m.w8(m.r32(kMoviePanel) + 3, 1);
    return 1;
}

u32 Game::o2_aircraft_label(u32 category, u32 aircraft, u32 mode, u32 variant) {
    cover(0x800FEBB8u);
    constexpr u32 kAircraftLabels = 0x80111A54u;
    if (aircraft >= 37) aircraft = 0;
    const u32 record = kAircraftLabels + 12 * aircraft;
    if (mode == 0) {
        if (variant >= 4) variant = 0;
        return m.r8(record + 4 + variant);
    }
    u32 label;
    if (category == 13) label = 8;
    else {
        const u32 primary = m.r8(record + 1);
        if (variant == primary + 1) label = 6;
        else if (variant == primary) label = 7;
        else {
            if (variant >= 4) variant = 0;
            label = m.r8(record + 8 + variant);
        }
    }
    return mode == 2 ? label + 6 : label;
}

u32 Game::o2_hangar_sounds_restart(u32 pitchOffset) {
    cover(0x800F3218u);
    Frame frame(*this, 32);
    constexpr u32 kSounds = 0x130, kFirstSound = 93;
    for (u32 voice = 0; voice < 5; ++voice)
        voice_release(m.r32(m.r32(kMenu) + kSounds + 4 * voice));
    for (u32 voice = 0; voice < 5; ++voice) {
        const u32 handle = sound_play(kFirstSound + voice);
        m.w32(m.r32(kMenu) + kSounds + 4 * voice, handle);
    }
    u32 result = 0;
    for (u32 voice = 0; voice < 5; ++voice) {
        const u32 pitch = (pitchOffset + static_cast<u32>(static_cast<s16>(sound_def_8(kFirstSound + voice)))) & 0xFFFF;
        result = voice_pitch(m.r32(m.r32(kMenu) + kSounds + 4 * voice), static_cast<s32>(pitch));
    }
    return result;
}

u32 Game::o2_campaign_rank_mask() {
    cover(0x800F78B0u);
    const u32 scene = m.r32(g::kScene);
    u32 mask = 15;
    for (u32 mission = 1; mission < 56; ++mission) {
        if (mission == 19 || mission == 49 || mission == 54) continue;
        u32 rank = m.r8(scene + 0xC1C + mission);
        if (!rank) rank = 5;
        for (u32 bank : {0xC58u, 0xC94u}) {
            const u32 candidate = m.r8(scene + bank + mission);
            if (candidate && candidate < rank) rank = candidate;
        }
        if (rank == 5) return 0;
        if (rank >= 2) mask &= ~8u;
        if (rank >= 3) mask &= ~4u;
        if (rank >= 4) mask &= ~2u;
    }
    return mask;
}

u32 Game::o2_panel_strip_wait(u32 panel, u32 poll) {
    cover(0x800FCA70u);
    Frame frame(*this, 8);
    constexpr u32 kStripTimeout = 0x40000;
    m.w32(frame.at(0), kStripTimeout);
    if (poll || m.r8(panel + 3)) return m.r8(panel + 3);
    do {
        m.w32(frame.at(4), m.r8(m.r32(kMoviePanel) + 4));
        m.w32(frame.at(0), m.r32(frame.at(0)) - 1);
        if (m.r32s(frame.at(0)) <= 0 || m.r32(frame.at(4))) {
            const u32 page = m.r8(panel + 2) == 0 ? 1 : 0;
            m.w8(panel + 3, 1);
            m.w8(panel + 2, page);
            const u32 x = m.r16(panel + 0x38 + 8 * page);
            const u32 y = m.r16(panel + 0x3A + 8 * m.r8(panel + 2));
            m.w16(panel + 0x48, x);
            m.w16(panel + 0x4A, y);
        }
    } while (!m.r8(panel + 3));
    return m.r8(panel + 3);
}

u32 Game::o2_brief_picture_fade(u32 kind) {
    cover(0x800F0174u);
    Frame frame(*this, 24);
    for (u32 fade = 1; fade <= 5; ++fade) o2_fade_set(fade, 0, 0, 0);
    return kind >= 3 && kind <= 6 ? kind - 1 : 0;
}
}

#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kBriefing = 0x80112DB0u, kMenu = 0x80112DC4u;
constexpr u32 kScreens = 0x80112DC0u, kDisplay = 0x800BCE48u;
constexpr u32 kFadeValues = 80u;
s32 fade_value(Memory& m, u32 index) {
    return m.r16s(m.r32(m.r32(kScreens) + kFadeValues) + 2 * index);
}
}

void Game::o2_launch_begin() {
    cover(0x800E6288u);
    Frame frame(*this, 32);
    m.w16(m.r32(g::kScene) + 2388, 1);
    o2_group_set(0, 0x01004001u);
    m.w16(m.r32(kMenu) + 134, 1);
    o2_hangar_effect1_set(3);
    o2_hangar_effect2_exit();
    sound_play(104);
    const u32 flags = m.r32(m.r32(kScreens) + 92);
    m.w8(flags + 46, (m.r32(m.r32(g::kScene) + 2412) & 16) ? 0u : 1u);
}

void Game::o2_launch_wait() {
    cover(0x800E6334u);
    Frame frame(*this, 24);
    const u32 scene = m.r32(g::kScene);
    if ((m.r32(scene + 300) & 16) && !(m.r32(scene + 296) & 47))
        o2_hangar_effect1_set(0);
    if (m.r32(m.r32(0x80112DA8u) + 20)) return;
    o2_fade_set(1, 3, 30, 0);
    o2_fade_set(15, 3, -1, -1);
    o2_fade_set(11, 3, -1, -1);
    const u32 menu = m.r32(kMenu);
    m.w16(menu + 134, m.r16(menu + 134) + 1);
}

void Game::o2_launch_fade() {
    cover(0x800E63F4u);
    Frame frame(*this, 24);
    const s32 value = fade_value(m, 1);
    if (o2_fade_state(1)) {
        for (u32 channel = 0; channel < 3; ++channel) {
            constexpr s32 colours[] = {143, 135, 83};
            m.w8(m.r32(kDisplay) + 16 + channel, static_cast<u32>((colours[channel] * value) / 30));
        }
        const u32 display = m.r32(kDisplay);
        m.w32(display, m.r32(display) | 8);
        return;
    }
    const u32 scene = m.r32(g::kScene);
    const s32 mission = m.r16s(scene + 908);
    u32 result = 0x60000002u;
    if (mission == 16 || mission == 37) {
        m.w8(scene + 936, mission == 16 ? 8u : 26u);
        if (mission == 37) m.w16(m.r32(g::kScene) + 908, 57);
        m.w32(m.r32(g::kScene) + 928, 2);
        result = 0x60000005u;
    } else if (mission == 42) m.w16(scene + 908, 56);
    m.w32(m.r32(kMenu) + 148, result);
}

void Game::o2_brief_image_open(u32 kind) {
    cover(0x800EDCC0u);
    Frame frame(*this, 72);
    const u32 definition = frame.at(24);
    m.w16(definition, 896); m.w16(definition + 2, 256);
    m.w16(definition + 12, 768); m.w16(definition + 14, 1);
    switch (kind) {
    case 0: case 1: case 6: m.w16(definition + 4, 80); m.w16(definition + 6, 80); break;
    case 2: m.w16(definition + 4, 160); m.w16(definition + 6, 80); break;
    case 3: m.w16(definition + 4, 144); m.w16(definition + 6, 64); break;
    case 4: case 9: m.w16(definition + 4, 128); m.w16(definition + 6, 96); break;
    case 5: m.w16(definition + 4, 160); m.w16(definition + 6, 64); break;
    default: break;
    }
    const u32 imageRect = frame.at(48), paletteRect = frame.at(56);
    for (u32 half = 0; half < 4; ++half) m.w16(imageRect + 2 * half, m.r16(definition + 2 * half));
    m.w16(paletteRect, m.r16(definition + 12)); m.w16(paletteRect + 2, m.r16(definition + 14));
    m.w16(paletteRect + 4, 256); m.w16(paletteRect + 6, 1);
    const u32 briefing = m.r32(kBriefing), pack = m.r32(briefing + 3892);
    const s32 count = m.r32s(briefing + 5712);
    m.w32(frame.at(16), pack); m.w32(frame.at(20), static_cast<u32>(count));
    const u32 updates = o2_image_open(imageRect, briefing + 5748, paletteRect, briefing + 5716, pack, count);
    m.w32(m.r32(kBriefing) + 3900, updates);
}

void Game::o2_brief_choices_init() {
    cover(0x800EDDF4u);
    Frame frame(*this, 56);
    const u32 briefing = m.r32(kBriefing);
    m.w16(briefing + 5810, 0xFFFF); m.w8(briefing + 5809, 0);
    o2_flag(1, 0); o2_flag(2, 0);
    o2_fade_set(3, 0, 0, 0);
    o2_fade_set(4, 0, 0, 0);
    const u32 current = m.r32(m.r32(kBriefing) + 6300);
    const s32 category = m.r16s(0x8010B400u + 8 * static_cast<u32>(m.r16s(current)));
    m.w8(m.r32(kBriefing) + 5808, o2_find_halfword(0x8010B37Cu, category, 6));
    for (u32 buffer = 0; buffer < 2; ++buffer) {
        const u32 state = m.r32(kBriefing);
        m.w32(frame.at(16), 0);
        set_draw_mode(state + 6252 + 24 * buffer, 0, 0, 0, 0);
        const u32 sprites = state + 5812 + 220 * buffer;
        for (u32 i = 0; i < 11; ++i) {
            const u32 packet = sprites + 20 * i;
            m.w8(packet + 3, 4); m.w8(packet + 7, 0x64);
            for (u32 channel = 0; channel < 3; ++channel) m.w8(packet + 4 + channel, 0x80);
            m.w16(packet + 8, 112); m.w16(packet + 10, 88 + 24 * i);
            m.w16(packet + 16, 135); m.w16(packet + 18, 18);
            m.w8(packet + 12, 0); m.w8(packet + 13, 18 * i);
        }
    }
    sound_play(62);
}

void Game::o2_brief_progress_draw() {
    cover(0x800EDF84u);
    Frame frame(*this, 56);
    const u32 quad = frame.at(16), briefing = m.r32(kBriefing);
    m.w8(quad + 19, 0);
    m.w16(quad, 107); m.w16(quad + 2, 151);
    m.w16(quad + 4, 109); m.w16(quad + 6, 151);
    m.w16(quad + 8, 107);
    const s32 numerator = 106 * m.r32s(briefing + 3904), denominator = m.r32s(briefing + 3900);
    if (!denominator) unported(0x800EDFECu, "zero briefing progress total");
    if (numerator == static_cast<s32>(0x80000000u) && denominator == -1)
        unported(0x800EE004u, "overflowing briefing progress division");
    const u32 bottom = static_cast<u32>(151 + numerator / denominator);
    m.w16(quad + 10, bottom); m.w16(quad + 12, 109); m.w16(quad + 14, bottom);
    for (u32 point = 0; point < 4; ++point) {
        m.w8(quad + 16 + 4 * point, 0xE5); m.w8(quad + 17 + 4 * point, 0xE5);
        m.w8(quad + 18 + 4 * point, 0xD5);
    }
    const u32 menu = m.r32(kMenu);
    const u32 next = o2_poly_g4(quad, m.r32(menu + 628), m.r32(menu + 8));
    m.w32(m.r32(kMenu) + 628, next);
}
}

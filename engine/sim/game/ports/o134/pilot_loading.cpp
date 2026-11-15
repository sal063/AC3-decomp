#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kCards = 0x800DDA48u, kState = 0x800DD9D0u, kDisplay = 0x800BCE48u;
constexpr u32 kPilotOffset = 2396, kLoadedSave = 760;
}
u32 Game::o1_pilot_load(u32 rankOut) {
    cover(0x800D4808u);
    Frame frame(*this, 40);
    const u32 save = m.r32(m.r32(kCards) + kLoadedSave), pilot = m.r32(g::kScene) + kPilotOffset;
    m.w8(rankOut, m.r8(save + 20));
    libc_strcpy(m.r32(g::kScene) + 2376, m.r32(m.r32(kCards) + kLoadedSave) + 8);
    m.w32(m.r32(g::kScene) + 2408, m.r32(m.r32(m.r32(kCards) + kLoadedSave) + 4));
    constexpr u32 packedScores[] = {0x22, 0x9A, 0x112, 0x18A};
    constexpr u32 pilotScores[] = {0x68, 0xE0, 0x158, 0x1D0};
    for (u32 mission = 0; mission < 60; ++mission)
        for (u32 family = 0; family < 4; ++family)
            m.w16(pilot + pilotScores[family] + 2 * mission, m.r16(save + packedScores[family] + 2 * mission));
    constexpr u32 packedRanks[] = {0x202, 0x220, 0x23E, 0x25C};
    constexpr u32 pilotRanks[] = {0x2C0, 0x2FC, 0x338, 0x374};
    for (u32 pair = 0; pair < 30; ++pair)
        for (u32 half = 0; half < 2; ++half)
            for (u32 family = 0; family < 4; ++family)
                m.w8(pilot + pilotRanks[family] + 2 * pair + half, (m.r8(save + packedRanks[family] + pair) >> (4 * half)) & 15u);
    m.w32(pilot + 0x6D4, static_cast<u32>(m.r16s(save + 0x2C0)));
    m.w32(pilot + 0x6D8, static_cast<u32>(m.r16s(save + 0x2C2))); m.w8(pilot + 1, m.r8(save + 20));
    for (u32 member = 0; member < 6; ++member) {
        const u32 source = save + 0x2C4 + 0x44 * member, scene = m.r32(g::kScene);
        for (u32 i = 0; i < 20; ++i) m.w16(scene + 0x172 + 0x5C * member + 2 * i, m.r16(source + 2 * i));
        const u32 destination = m.r32(g::kScene) + 0x19C + 0x5C * member;
        const u32 a = m.r32(source + 40), b = m.r32(source + 44), c = m.r32(source + 48), d = m.r32(source + 52);
        m.w32(destination, a); m.w32(destination + 4, b); m.w32(destination + 8, c); m.w32(destination + 12, d);
        const u32 e = m.r32(source + 56), f = m.r32(source + 60);
        m.w32(destination + 16, e); m.w32(destination + 20, f); m.w32(destination + 24, m.r32(source + 64));
    }
    {
        const u32 scene = m.r32(g::kScene), flags = m.r32(scene + 288);
        m.w32(scene + 288, m.r8(save + 1124) ? flags & ~4u : flags | 4u);
    }
    const u32 cards = m.r32(kCards), display = m.r32(kDisplay);
    m.w32(display + 8, static_cast<u32>(m.r16s(m.r32(cards + kLoadedSave) + 1120)));
    m.w32(display + 12, static_cast<u32>(m.r16s(m.r32(cards + kLoadedSave) + 1122)));
    m.w8(m.r32(g::kScene) + 920, m.r8(save + 1117));
    m.w8(m.r32(g::kScene) + 921, m.r8(save + 1118));
    m.w8(m.r32(g::kScene) + 922, m.r8(save + 1116));
    req_stereo_set(m.r8(m.r32(g::kScene) + 922) & 1u); req_volume_apply();
    auto unpackBits = [&](u32 packedOffset, u32 destinationOffset, u32 bytes, u32 bits) {
        const u32 values = 8 / bits, mask = (1u << bits) - 1u;
        for (u32 i = 0; i < bytes; ++i)
            for (u32 value = 0; value < values; ++value)
                m.w8(pilot + destinationOffset + values * i + value, (m.r8(save + packedOffset + i) >> (bits * value)) & mask);
    };
    unpackBits(0x1A, 0x18, 8, 1); unpackBits(0x18, 0x58, 2, 1);
    unpackBits(0x27A, 0x3EC, 20, 2); unpackBits(0x28E, 0x43C, 45, 2);
    {
        const u32 scene = m.r32(g::kScene), value = m.r8(scene + 924);
        m.w8(scene + 924, m.r32(save + 1128) & 1u ? value | 1u : value & 254u);
    }
    for (u32 bit = 1; bit <= 2; ++bit) {
        const u32 enabled = m.r32(save + 1128) & (1u << bit), scene = m.r32(g::kScene), flags = m.r32(scene + 288);
        m.w32(scene + 288, enabled ? flags | (1u << (bit - 1)) : flags & ~(1u << (bit - 1)));
    }
    m.w8(m.r32(g::kScene) + 2394, m.r32(save + 1128) & 8u ? 1u : 0u);
    const u32 currentDisplay = m.r32(kDisplay), flags = m.r32(currentDisplay);
    m.w32(currentDisplay, flags | 2u);
    return flags | 2u;
}
u32 Game::o1_saved_pilot_load() {
    cover(0x800D2994u);
    Frame frame(*this, 24);
    o1_save_slot_load(m.r8(m.r32(kState) + 93)); o1_pilot_load(m.r32(kState) + 95);
    const u32 scene = m.r32(g::kScene), state = m.r32(kState);
    m.w32(scene + 2404, m.r32(scene + 2404) & ~1u);
    const u32 timer = m.r16(state + 78) + 1u; m.w16(state + 78, timer);
    return timer;
}
u32 Game::o1_account_directory_write_poll() {
    cover(0x800D1300u);
    Frame frame(*this, 32);
    const u32 state = m.r32(kState);
    const s32 result = t1_save_dir(m.r8(state + 90), m.r8(state + 93), 1);
    if (result == -1) {
        m.w16(m.r32(kState) + 78, m.r16(m.r32(kState) + 78) + 2);
        t1_flag(1, 40); o1_screen_text_select(0x20016); o1_animation_history_push(35); o1_marker_mode(0);
    } else if (result == 1) {
        o1_save_name_to_scene(); t1_pilot_serialise();
        const u32 current = m.r32(kState), timer = m.r16(current + 78) + 1u;
        m.w16(current + 78, timer); return timer;
    }
    return 1;
}
u32 Game::o1_account_pilot_write_poll() {
    cover(0x800D13B8u);
    Frame frame(*this, 32);
    const u32 state = m.r32(kState);
    const s32 result = t1_save_pilot(m.r8(state + 90), m.r8(state + 93), 6, 1);
    if (result == -1) {
        m.w16(m.r32(kState) + 78, m.r16(m.r32(kState) + 78) + 1);
        t1_flag(1, 40); o1_screen_text_select(0x20056); o1_animation_history_push(35); o1_marker_mode(0);
    } else if (result == 1) {
        const u32 current = m.r32(kState);
        m.w32(current + 136, 120); m.w16(current + 78, m.r16(current + 78) + 2); t1_stats_reset();
        m.w16(m.r32(g::kScene) + 2388, 2); m.w16(m.r32(kState) + 16, 0);
        return o1_animation_history_push(36);
    }
    return 1;
}
}

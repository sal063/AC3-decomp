#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kCinematic = 0x80112E1Cu, kMenu = 0x80112DC4u, kScreens = 0x80112DC0u;
constexpr u32 kSectionIds = 6, kSections = 0x42, kPage = 5, kColumn = 4;
u32 flags(Memory& m) { return m.r32(m.r32(kScreens) + 0x5C); }
u32 texture(Memory& m, u32 idsOffset, u32 bank) {
    const u32 id = m.r8(m.r32(kCinematic) + idsOffset + kSectionIds);
    return m.r32(m.r32(m.r32(kMenu) + bank) + 4 * id);
}
}

u32 Game::o2_cinematic_section(u32 page, u32 column) {
    cover(0x800FD770u);
    Frame frame(*this, 32);
    m.w32(frame.at(16), 6);
    return o2_upload_tim_positioned(texture(m, 4 * page + column, 0x1D4), 960, 340, 384, 6);
}

u32 Game::o2_cinematic_page(u32 page) {
    cover(0x800FD7D0u);
    Frame frame(*this, 48);
    for (u32 column = 0; column < 4; ++column) m.w8(flags(m) + 53 + column, 0);
    const s32 sections = m.r16s(m.r32(kCinematic) + kSections), fullPages = sections / 4;
    const s32 remainder = sections - 4 * fullPages;
    const s32 count = static_cast<s32>(page) < fullPages ? 4 : remainder;
    if (count <= 0) return static_cast<u32>(remainder) << 16;
    for (s32 column = 0; column < count; ++column) {
        m.w32(frame.at(16), 6);
        o2_upload_tim_positioned(texture(m, 4 * page + static_cast<u32>(column), 0x1D4),
                                 960, 340 + 18 * static_cast<u32>(column), 384, 6);
        m.w8(flags(m) + 53 + static_cast<u32>(column), 2);
    }
    return 0;
}

u32 Game::o2_cinematic_detail(u32 column) {
    cover(0x800FD8F4u);
    Frame frame(*this, 32);
    const u32 page = m.r8(m.r32(kCinematic) + kPage);
    m.w32(frame.at(16), 1);
    return o2_upload_tim_positioned(texture(m, 4 * page + column, 0x1DC), 960, 256, 768, 1);
}

u32 Game::o2_cinematic_begin() {
    cover(0x800FD958u);
    Frame frame(*this, 32);
    m.w8(m.r32(kCinematic), 1);
    o2_cinematic_page(m.r8(m.r32(kCinematic) + kPage));
    o2_cinematic_detail(m.r8(m.r32(kCinematic) + kColumn));
    const u32 screenFlags = flags(m);
    m.w8(screenFlags + 47, 1);
    return screenFlags;
}

u32 Game::o2_cinematic_leave() {
    cover(0x800FDE78u);
    Frame frame(*this, 24);
    if (m.r16s(m.r32(kCinematic) + 2) == 1) {
        m.w8(m.r32(kCinematic), 3);
        o2_group_set(0, 0x08000003);
        const u32 state = m.r32(kCinematic);
        o2_cinematic_section(m.r8(state + kPage), m.r8(state + kColumn));
    }
    const u32 state = m.r32(kCinematic);
    if (m.r16s(state + 2) == 2) m.w8(state + 1, 2);
    for (u32 flag = 49; flag < 57; ++flag) m.w8(flags(m) + flag, 0);
    const u32 screenFlags = flags(m);
    m.w8(screenFlags + 47, 0);
    return screenFlags;
}
u32 Game::o2_cinematic_transition() {
    cover(0x800FE0D4u);
    Frame frame(*this, 24);
    const u32 state = m.r32(kCinematic);
    const s32 action = m.r16s(state + 2);
    if (action == 1) m.w8(state, m.r8(state) + 1);
    else if (action == 2) {
        o2_cinematic_page(m.r8(state + kPage));
        o2_cinematic_detail(m.r8(m.r32(kCinematic) + kColumn));
        o2_group_set(0, 0x04000003);
        m.w8(m.r32(kCinematic), 0);
    }
    const u32 screenFlags = flags(m);
    m.w8(screenFlags + 47, 0);
    return screenFlags;
}

u32 Game::o2_cinematic_play_begin() {
    cover(0x800FE180u);
    Frame frame(*this, 24);
    m.w32(m.r32(kMenu) + 0x264, 0);
    const u32 state = m.r32(kCinematic);
    const u32 id = m.r8(state + kSectionIds + 4 * m.r8(state + kPage) + m.r8(state + kColumn));
    o2_transition_duration(id);
    const u32 current = m.r32(kCinematic), next = m.r8(current) + 1;
    m.w8(current, next);
    return next;
}

u32 Game::o2_cinematic_catalogue() {
    cover(0x800FE444u);
    Frame frame(*this, 24);
    const u32 pilot = m.r32(g::kScene) + 0x95C;
    m.w32(kCinematic, mem_alloc(68));
    m.w8(m.r32(kCinematic), 0); m.w8(m.r32(kCinematic) + 1, 0);
    m.w8(m.r32(kCinematic) + kColumn, 0);
    m.w16(m.r32(kCinematic) + 2, 0); m.w8(m.r32(kCinematic) + kPage, 0);
    m.w16(m.r32(kCinematic) + kSections, 0);
    for (u32 mission = 0; mission < 60; ++mission) {
        bool completed = false;
        for (u32 difficulty = 0; difficulty < 4; ++difficulty)
            if (m.r8(pilot + 0x2C0 + 60 * difficulty + mission)) { completed = true; break; }
        if (completed) {
            const u32 state = m.r32(kCinematic);
            m.w8(state + kSectionIds + static_cast<u32>(m.r16s(state + kSections)), mission);
            const u32 current = m.r32(kCinematic);
            m.w16(current + kSections, m.r16(current + kSections) + 1);
        }
    }
    o2_group_set(0, 0x04000003);
    const u32 menu = m.r32(kMenu), next = m.r16(menu + 0x86) + 1;
    m.w16(menu + 0x86, next);
    return next;
}
u32 Game::o2_cinematic_difficulty() {
    cover(0x800FDF64u);
    Frame frame(*this, 24);
    m.w8(flags(m) + 51, 0);
    u32 scene = m.r32(g::kScene);
    if ((m.r32(scene + 0x130) & 2u) && m.r32s(scene + 0x394) < 2) {
        m.w32(scene + 0x394, m.r32(scene + 0x394) + 1);
        sound_play(61);
    }
    scene = m.r32(g::kScene);
    if ((m.r32(scene + 0x130) & 1u) && m.r32s(scene + 0x394) > 0) {
        m.w32(scene + 0x394, m.r32(scene + 0x394) - 1);
        sound_play(61);
    }
    scene = m.r32(g::kScene);
    if ((m.r32(scene + 0x130) & 16u) && !(m.r32(scene + 0x128) & 32u)) {
        const u32 state = m.r32(kCinematic);
        m.w16(state + 2, 1);
        m.w8(state, 5);
        sound_play(60);
    }
    scene = m.r32(g::kScene);
    if ((m.r32(scene + 0x130) & 32u) && !(m.r32(scene + 0x128) & 16u)) {
        const u32 state = m.r32(kCinematic);
        m.w16(state + 2, 2);
        m.w8(state, 5);
        sound_play(59);
    }
    const u32 difficulty = m.r8(m.r32(g::kScene) + 0x394);
    m.w8(flags(m) + 52, difficulty);
    return difficulty;
}
}

#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kBrief = 0x80112DB0u, kMenu = 0x80112DC4u;
constexpr u32 kPageTable = 0x8010B400u, kMissionKinds = 0x8010B9B4u;
constexpr u32 kPageCursor = 0x189C, kRemaining = 12;
}

u32 Game::o2_campaign_brief_begin() {
    cover(0x800F0A7Cu);
    Frame frame(*this, 32);
    const u32 scene = m.r32(g::kScene);
    m.w32(scene + 0x96C, (m.r32(scene + 0x96C) | 0x80u) & ~0x40u);
    const s32 mission = m.r16s(scene + 0x38C);
    const u32 missionKind = m.r8(kMissionKinds + static_cast<u32>(mission));
    if (missionKind == 0) {
        m.w32(m.r32(kMenu) + 0x94, 0x60000002u);
        m.w16(scene + 0x954, 1);
    } else if (missionKind == 2) {
        if (mission == 16) m.w8(scene + 0x3A8, 8);
        if (m.r16s(m.r32(g::kScene) + 0x38C) == 37) {
            m.w8(m.r32(g::kScene) + 0x3A8, 26);
            m.w16(m.r32(g::kScene) + 0x38C, 57);
        }
        m.w32(m.r32(g::kScene) + 0x3A0, missionKind);
        m.w16(m.r32(g::kScene) + 0x954, 1);
        m.w32(m.r32(kMenu) + 0x94, 0x60000005u);
    }
    m.w32(kBrief, mem_alloc(0x18A0));
    kcd_voice_reset();
    const u32 brief = m.r32(kBrief);
    m.w16(brief, 0); m.w16(brief + 2, 1); m.w32(brief + 16, 0);
    const u32 list = m.r32(m.r32(m.r32(kMenu) + 0x120));
    m.w32(brief + 8, list); m.w32(brief + kRemaining, m.r32(list));
    m.w32(brief + 24, list + 4);
    const u32 seen = m.r8(m.r32(g::kScene) + 0x95E);
    m.w32(brief + kPageCursor, list + 4 + 4 * seen);
    m.w32(brief + kRemaining, m.r32(brief + kRemaining) - seen);
    o2_page_request();
    m.w8(m.r32(kBrief) + 0xF2E, 0); m.w32(m.r32(kBrief) + 0xF38, 0);
    o2_group_set(1, 0);
    m.w32(m.r32(kBrief) + 0xF30, 0x8000);
    o2_set_scripts(m.r32(m.r32(kMenu) + 0x15C), 0);
    m.w8(m.r32(kBrief) + 0xF48, 0); m.w8(m.r32(kBrief) + 0xF49, 0);
    for (s32 entry = 15; entry >= 0; --entry) m.w16(m.r32(kBrief) + 0x52C + 160 * entry, 0);
    for (s32 entry = 15; entry >= 0; --entry) m.w16(m.r32(kBrief) + 0x1450 + 32 * entry, 0);
    m.w16(m.r32(kMenu) + 0x86, 1);
    auto pageKind = [&] {
        const s32 page = m.r16s(m.r32(m.r32(kBrief) + kPageCursor));
        return m.r8(kPageTable + 8 * static_cast<u32>(page) + 6);
    };
    m.w32(m.r32(kBrief) + 0x28, o2_page_has_picture(pageKind()));
    m.w32(m.r32(kBrief) + 0x1C, o2_page_layout(pageKind()));
    if (pageKind() != 10) return 10;
    if (!m.r32(m.r32(kBrief) + kRemaining)) return 0;
    m.w32(m.r32(kMenu) + 0x94, 0x60000005u);
    const u32 currentScene = m.r32(g::kScene);
    m.w32(currentScene + 0x3A0, 3);
    m.w8(currentScene + 0x95E, m.r8(currentScene + 0x95E) + 1);
    m.w16(m.r32(g::kScene) + 0x954, 2);
    const s32 page = m.r16s(m.r32(m.r32(kBrief) + kPageCursor));
    m.w8(m.r32(g::kScene) + 0x3A8, m.r8(0x8010B360u + static_cast<u32>(page)));
    o2_unlock_by_id(m.r16s(m.r32(m.r32(kBrief) + kPageCursor)));
    return 0;
}

u32 Game::o2_single_brief_begin() {
    cover(0x800F0DC4u);
    Frame frame(*this, 32);
    m.w32(kBrief, mem_alloc(0x18A0));
    kcd_voice_reset();
    const u32 brief = m.r32(kBrief), page = m.r32(kMenu) + 0x124;
    m.w16(brief + 2, 1); m.w16(brief, 0); m.w32(brief + 16, 0);
    m.w32(brief + kRemaining, 1); m.w32(brief + 24, page); m.w32(brief + kPageCursor, page);
    o2_page_request();
    m.w8(m.r32(kBrief) + 0xF2E, 0); m.w32(m.r32(kBrief) + 0xF38, 0);
    o2_entry_reset(0, 21); o2_entry_reset(0, 14); o2_fade_set(11, 1, 0, 25);
    m.w32(m.r32(kBrief) + 0xF30, 0x204001); o2_group_set(0, 0x204001);
    m.w8(m.r32(kBrief) + 0xF48, 0); m.w8(m.r32(kBrief) + 0xF49, 0);
    for (s32 entry = 15; entry >= 0; --entry) m.w16(m.r32(kBrief) + 0x52C + 160 * entry, 0);
    for (s32 entry = 15; entry >= 0; --entry) m.w16(m.r32(kBrief) + 0x1450 + 32 * entry, 0);
    auto pageKind = [&] {
        const s32 id = m.r16s(m.r32(m.r32(kBrief) + kPageCursor));
        return m.r8(kPageTable + 8 * static_cast<u32>(id) + 6);
    };
    m.w32(m.r32(kBrief) + 0x1C, o2_page_has_picture(pageKind()));
    m.w32(m.r32(kBrief) + 0x1C, o2_page_layout(pageKind()));
    m.w16(m.r32(kMenu) + 0x86, 4);
    return sound_play(90);
}
}

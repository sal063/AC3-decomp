#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace { constexpr u32 kCards = 0x800DDA48u; }
u32 Game::o1_profile_pack() {
    cover(0x800D4658u);
    Frame frame(*this, 32);
    const u32 pilot = m.r32(g::kScene) + 2396, profile = m.r32(m.r32(kCards) + 788);
    m.w32(profile + 276, m.r32(pilot + 1744));
    m.w8(profile + 275, m.r8(m.r32(g::kScene) + 908));
    m.w8(profile + 274, m.r8(m.r32(g::kScene) + 916));
    for (u32 mission = 0; mission < 60; ++mission) {
        m.w16(profile + 4 + 2 * mission, m.r16(pilot + 584 + 2 * mission));
        m.w8(profile + 154 + mission, m.r8(pilot + 1264 + 4 * mission));
        m.w8(profile + 214 + mission, m.r8(pilot + 1504 + 4 * mission));
        m.w8(profile + 280, m.r8(m.r32(g::kScene) + 910));
        m.w8(profile + 281, m.r8(m.r32(g::kScene) + 937));
        m.w8(profile + 282, m.r8(m.r32(g::kScene) + 938));
        m.w8(profile + 283, m.r8(m.r32(g::kScene) + 939));
    }
    for (u32 pair = 0; pair < 30; ++pair) {
        const u32 low = m.r8(pilot + 944 + 2 * pair), high = m.r8(pilot + 945 + 2 * pair);
        m.w8(profile + 124 + pair, (low & 15u) | ((high & 15u) << 4));
    }
    for (u32 i = 100; i > 0; --i) m.w8(profile + 284 + i - 1, 0);
    return 0;
}
u32 Game::o1_profile_unpack(u32 rank) {
    cover(0x800D4DF4u);
    const u32 profile = m.r32(m.r32(kCards) + 764 + 4 * (rank & 255u));
    const u32 pilot = m.r32(g::kScene) + 2396;
    m.w8(pilot + 1, rank);
    const u32 ticks = m.r32(profile + 276), scene = m.r32(g::kScene);
    m.w32(pilot + 1744, ticks); m.w16(scene + 908, m.r8(profile + 275));
    m.w32(scene + 916, m.r8(profile + 274));
    for (u32 mission = 0; mission < 60; ++mission) {
        m.w16(pilot + 584 + 2 * mission, m.r16(profile + 4 + 2 * mission));
        m.w8(pilot + 944 + mission, m.r8(profile + 124 + mission));
        m.w32(pilot + 1264 + 4 * mission, m.r8(profile + 154 + mission));
        const u32 extra = m.r8(profile + 214 + mission), currentScene = m.r32(g::kScene);
        m.w32(pilot + 1504 + 4 * mission, extra);
        m.w16(currentScene + 910, m.r8(profile + 280));
        m.w8(currentScene + 937, m.r8(profile + 281));
        m.w8(m.r32(g::kScene) + 938, m.r8(profile + 282));
        m.w8(m.r32(g::kScene) + 939, m.r8(profile + 283));
    }
    for (u32 pair = 0; pair < 30; ++pair) {
        m.w8(pilot + 944 + 2 * pair, m.r8(profile + 124 + pair) & 15u);
        m.w8(pilot + 945 + 2 * pair, m.r8(profile + 124 + pair) >> 4);
    }
    return 0;
}
u32 Game::o1_profile_write(u32 slot, u32 pilot, u32 rank, u32 reset) {
    cover(0x800D73B8u);
    Frame frame(*this, 40);
    if (!t1_cards_idle()) return 0;
    t1_io_begin();
    if (!m.r8(m.r32(kCards) + 822)) {
        const u32 profile = m.r32(m.r32(kCards) + 788), sum = t1_save_sum(profile, 94);
        const u32 cards = m.r32(kCards);
        m.w32(m.r32(cards + 788), sum);
        const u32 file = hw->file_open(cards + 692 + 26 * slot, 0x8002);
        m.w32(m.r32(kCards) + 60, file);
        if (file == 0xFFFFFFFFu) return static_cast<u32>(t1_io_retry());
        hw->file_lseek(file, 4224 + 2304 * pilot + 384 * rank, 0);
        const u32 current = m.r32(kCards);
        hw->file_write(m.r32(current + 60), m.r32(current + 788), 384);
        m.w8(m.r32(kCards) + 822, 1);
    }
    const s32 event = t1_card_wait(4, slot, 0);
    if (event == -1) return 0;
    hw->file_close(m.r32(m.r32(kCards) + 60)); m.w8(m.r32(kCards) + 822, 0);
    if (event) return static_cast<u32>(t1_io_retry());
    if (reset == 1) t1_card_flags_reset();
    return 1;
}
}

#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kPilotInScene = 0x95C, kRanks = 0x2C0, kRankStride = 60;
constexpr u32 kLatestRank = 0x3B0, kLatestTime = 0x248, kBestTime = 0x68, kTimeStride = 0x78;
}

u32 Game::o2_campaign_milestones(u32 mission) {
    cover(0x800F5B04u);
    const u32 scene = m.r32(g::kScene), pilot = scene + kPilotInScene;
    if (m.r16s(scene + 0x954) != 1) return 0;
    constexpr u32 milestones[] = {34, 39, 18, 53, 48};
    for (u32 required : milestones) {
        bool recorded = false;
        for (u32 difficulty = 0; difficulty < 4; ++difficulty)
            if (m.r8(pilot + kRanks + kRankStride * difficulty + required)) { recorded = true; break; }
        if (!recorded && mission != required) return 0;
    }
    return 1;
}

u32 Game::o2_mission_result_store() {
    cover(0x800F5CA4u);
    Frame frame(*this, 48);
    const u32 scene = m.r32(g::kScene), pilot = scene + kPilotInScene;
    const u32 mission = m.r8(scene + 0x3BC), difficulty = m.r32(scene + 0x394);
    const u32 rank = m.r8(scene + 0x3C3), rawTime = m.r16(scene + 0x3BA);
    const s32 time = m.r16s(scene + 0x3BA);
    if (time) {
        m.w16(pilot + kLatestTime + 2 * mission, rawTime);
        if (difficulty < 4) {
            const u32 best = pilot + kBestTime + kTimeStride * difficulty + 2 * mission;
            if (time < static_cast<s32>(m.r16(best))) m.w16(best, rawTime);
        }
        m.w32(pilot + 0x6D0, m.r32(pilot + 0x6D0) + static_cast<u32>(time));
    }
    o2_aircraft_unlock_mission(static_cast<u32>(m.r16s(m.r32(g::kScene) + 0x38C)));
    o2_aircraft_unlock_variants(static_cast<u32>(m.r16s(m.r32(g::kScene) + 0x38C)));
    m.w8(pilot + kLatestRank + mission, rank);
    if (difficulty < 4) {
        const u32 best = pilot + kRanks + kRankStride * difficulty + mission;
        const u32 oldRank = m.r8(best);
        if (!oldRank || rank < oldRank) m.w8(best, rank);
    }
    m.w8(m.r32(m.r32(0x80112DC0u) + 0x5C) + 1, rank);
    const u32 currentScene = m.r32(g::kScene), perMission = currentScene + 4 * mission;
    const u32 score = m.r8(scene + 0x3C2);
    m.w32(perMission + 0xE4C, score);
    m.w32(currentScene + 0x1030, m.r32(currentScene + 0x1030) + score);
    m.w32(perMission + 0xF3C, m.r32(perMission + 0xF3C) + static_cast<u32>(m.r16s(currentScene + 0x958)));
    const u32 total = m.r32(currentScene + 0x1034) + static_cast<u32>(m.r16s(currentScene + 0x958));
    m.w32(currentScene + 0x1034, total);
    return total;
}
}

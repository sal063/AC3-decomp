#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMissionAircraft = 0x80111784u;
constexpr u32 kAircraftVariants = 0x80111A54u;
constexpr u32 kMenu = 0x80112DC4u;
constexpr u32 kAircraftUnlocked = 0x974;
constexpr u32 kGroupStart[] = {10, 1, 18, 26, 31};
constexpr u32 kGroupEnd[] = {18, 10, 26, 31, 37};
bool free_selection(Memory& m) { return (m.r32(m.r32(g::kScene) + 0x96C) & 0x10) != 0; }
bool alternate_variant(u32 mission) {
    return mission == 14 || mission == 28 || mission == 29 || mission == 43 || mission == 44;
}
}

u32 Game::o2_aircraft_group(u32 mission, s32 aircraft) {
    cover(0x800FE6E0u);
    if (free_selection(m)) {
        if (aircraft < 10) return 1;
        if (aircraft < 18) return 0;
        if (aircraft < 26) return 2;
        if (aircraft < 31) return 3;
        return 4;
    }
    if (mission >= 60) mission = 0;
    return m.r32(kMissionAircraft + 12 * mission);
}

s32 Game::o2_aircraft_id(u32 mission, u32 index) {
    cover(0x800FE8D8u);
    if (free_selection(m)) {
        if (index >= 64) index = 0;
        return m.r16s(m.r32(kMenu) + 0x1E0 + 2 * index);
    }
    if (index >= 7) index = 0;
    return m.r8(kMissionAircraft + 12 * mission + 5 + index);
}

u32 Game::o2_aircraft_unlock_mission(u32 mission) {
    cover(0x800FE950u);
    if (mission >= 60) mission = 0;
    const u32 record = kMissionAircraft + 12 * mission;
    const u32 count = m.r8(record + 4);
    for (u32 index = 0; index < count; ++index) {
        const u32 aircraft = m.r8(record + 5 + index);
        m.w8(m.r32(g::kScene) + kAircraftUnlocked + aircraft, 1);
    }
    return count ? 0 : record;
}

u32 Game::o2_aircraft_variant(u32 mission, u32 aircraft, u32 mode) {
    cover(0x800FE9C0u);
    if (aircraft >= 37) aircraft = 0;
    const u32 record = kAircraftVariants + 12 * aircraft;
    if (mode == 0) return m.r8(record);
    if (mission == 13) return 1;
    u32 variant = m.r8(record + 1);
    if (mission == 42) return variant;
    if (free_selection(m)) {
        ++variant;
        if (mission != 32 && mission != 47 && mission != 53 && mission != 55
            && (m.r32(m.r32(kMenu) + 0x26C) & 8)) ++variant;
    } else if (alternate_variant(mission)) {
        ++variant;
    }
    return variant;
}

u32 Game::o2_aircraft_default_variant(u32 mission, u32 aircraft, u32 mode) {
    cover(0x800FEAD4u);
    if (aircraft >= 37) aircraft = 0;
    const u32 record = kAircraftVariants + 12 * aircraft;
    u32 bank, value;
    if (mode == 0) {
        value = m.r8(record + 2);
        bank = record + 4;
    } else {
        if (mission == 13) return 0;
        if (alternate_variant(mission)) return m.r8(record + 1);
        value = m.r8(record + 3);
        bank = record + 8;
    }
    for (u32 index = 0; index < 4; ++index)
        if (m.r8(bank + index) == value) return index;
    return 0;
}

u32 Game::o2_aircraft_group_index(u32 mission, u32 aircraft) {
    cover(0x800FE668u);
    Frame frame(*this, 24);
    const u32 group = o2_aircraft_group(mission, static_cast<s32>(aircraft));
    return group < 5 ? aircraft - kGroupStart[group] : aircraft;
}

u32 Game::o2_aircraft_count(u32 mission, u32 group) {
    cover(0x800FE764u);
    if (!free_selection(m)) return m.r8(kMissionAircraft + 12 * mission + 4);
    if (group >= 5) return 0;
    const u32 scene = m.r32(g::kScene);
    u32 count = 0;
    for (u32 aircraft = kGroupStart[group]; aircraft < kGroupEnd[group]; ++aircraft)
        if (m.r8(scene + kAircraftUnlocked + aircraft) == 1) ++count;
    return count;
}

u32 Game::o2_mission_aircraft_group_index(u32 mission, u32 index) {
    cover(0x800FE864u);
    Frame frame(*this, 24);
    if (free_selection(m)) return 0;
    if (mission >= 60) mission = 0;
    if (index >= 7) index = 0;
    return o2_aircraft_group_index(mission, m.r8(kMissionAircraft + 12 * mission + 5 + index));
}

u32 Game::o2_aircraft_unlock_variants(u32 mission) {
    cover(0x800FEC84u);
    if (mission >= 60) mission = 0;
    if (mission == 13) m.w8(m.r32(g::kScene) + 0x9C2, 1);
    else if (alternate_variant(mission)) m.w8(m.r32(g::kScene) + 0x9C1, 1);
    const u32 record = kMissionAircraft + 12 * mission;
    const u32 count = m.r8(record + 4);
    for (u32 index = 0; index < count; ++index) {
        const u32 aircraft = m.r8(record + 5 + index);
        if (aircraft < 1 || aircraft >= 37) continue;
        const u32 variants = kAircraftVariants + 12 * aircraft;
        const u32 firstCount = m.r8(variants);
        for (u32 variant = 0; variant < firstCount; ++variant)
            m.w8(m.r32(g::kScene) + 0x9B4 + m.r8(variants + 4 + variant), 1);
        const u32 secondCount = m.r8(variants + 1);
        for (u32 variant = 0; variant < secondCount; ++variant)
            m.w8(m.r32(g::kScene) + 0x9BA + m.r8(variants + 8 + variant), 1);
    }
    return count ? count + 12 * mission : record;
}
}

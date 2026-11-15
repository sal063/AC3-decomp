#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDialog = 0x80112DDCu, kSceneProgress = 348, kProgressStride = 92;
constexpr u32 kGroupOrder = 0x8010A520u, kMissionLabels = 0x8010A508u;
constexpr u32 kLabelMasks = 0x8010A540u, kIconKinds = 0x8010A560u;
}

void Game::o2_campaign_presets() {
    cover(0x800D1B6Cu);
    Frame frame(*this, 48);
    const s32 selected = static_cast<s32>(o2_selection_bit_index(m.r32(m.r32(g::kScene) + 292)));
    for (u32 row = 0; row < 5; ++row) {
        const u32 group = m.r8(kGroupOrder + row);
        const u32 labels = m.r32(kDialog) + 36 + 16 * group;
        for (s32 i = 14; i >= 0; --i) m.w8(labels + static_cast<u32>(i), 20);
        for (u32 mission = 11; mission < 31; ++mission) {
            for (u32 i = 0; i < 15; ++i) {
                const u32 state = m.r32(g::kScene);
                const u32 mask = static_cast<u32>(m.r16s(kLabelMasks + 2 * i));
                if (mask & m.r16(state + kSceneProgress + kProgressStride * row + 2 * mission))
                    m.w8(labels + i, m.r8(kMissionLabels + mission - 11));
            }
        }
        const u32 progress = m.r32(g::kScene) + kSceneProgress + kProgressStride * row + 64;
        const u32 iconGroup = m.r8(kGroupOrder + row), dialog = m.r32(kDialog);
        const u32 icons = dialog + 132 + 4 * iconGroup, options = dialog + 156 + 4 * iconGroup;
        for (s32 i = 3; i >= 0; --i) m.w8(icons + static_cast<u32>(i), 12);
        for (u32 kind = 0; kind < 6; ++kind) for (u32 slot = 0; slot < 4; ++slot) {
            if (m.r16s(kIconKinds + 2 * slot) != m.r8(progress + 18 + kind)) continue;
            const u32 icon = 2 * kind;
            m.w8(icons + slot, icon);
            if (icon == 0) m.w8(options + slot, (m.r16(progress + 16) & 4) ? 0 : 1);
            if (icon == 6) m.w8(options + slot, (m.r16(progress + 16) & 8) ? 0 : 1);
            if (icon == 8) m.w8(options + slot, (m.r16(progress + 16) & 16) ? 1 : 0);
        }
        const u32 completion = m.r16(progress + 16);
        const u32 target = m.r32(kDialog), targetGroup = m.r8(kGroupOrder + row);
        m.w8(target + 21 + targetGroup, (completion & 2) ? 0 : (completion & 1) ? 2 : 1);
    }
    const bool complete = (m.r32(m.r32(g::kScene) + 288) & 4) != 0;
    m.w8(m.r32(kDialog) + 20, complete ? 0 : 1);
    if (selected < 0) return;
    u32 state = m.r32(g::kScene), progress = state + 412 + kProgressStride * static_cast<u32>(selected);
    const u32 category = m.r32(state + 292) == 4 ? m.r8(progress + 3) / 38 : m.r8(progress + 3) >> 4;
    m.w8(m.r32(kDialog) + 27, category - 1);
    state = m.r32(g::kScene);
    const u32 unlocked = m.r32(state + 436 + kProgressStride * static_cast<u32>(selected));
    m.w8(m.r32(kDialog) + 180, (unlocked & 8) ? 1 : 0);
    m.w8(m.r32(kDialog) + 181, (unlocked & 16) ? 1 : 0);
}

void Game::o2_campaign_dialog_begin() {
    cover(0x800D3088u);
    Frame frame(*this, 24);
    const u32 dialog = m.r32(kDialog);
    m.w8(dialog + 1, m.r8(dialog + 1) + 1); m.w8(m.r32(kDialog) + 2, 0);
    o2_fade_set(1, 0, 0, 0); o2_fade_set(2, 0, 0, 0);
    o2_group_set(2, 12); o2_group_set(0, 1);
    o2_entry_reset(2, 2); o2_entry_reset(2, 3);
    o2_upload_pack(m.r32(m.r32(m.r32(0x80112DC4u) + 28) + 12));
    const u32 current = m.r32(kDialog);
    m.w16(current + 14, 0); m.w8(current + 20, 0);
    for (u32 i = 0; i < 4; ++i) m.w8(m.r32(kDialog) + 21 + i, 0);
    const u32 target = m.r32(kDialog), screens = m.r32(0x80112DC0u);
    m.w32(target + 224, 1); m.w8(m.r32(screens + 92) + 11, 4);
    m.w8(m.r32(m.r32(0x80112DC0u) + 92) + 10, 0);
    o2_flag(62, 0); o2_campaign_presets();
}
u32 Game::o2_campaign_presets_apply() {
    cover(0x800D1F54u);
    Frame frame(*this, 24);
    const s32 selected = static_cast<s32>(o2_selection_bit_index(m.r32(m.r32(g::kScene) + 292)));
    constexpr u32 kMissionFromLabel = 0x8010A528u;
    for (u32 row = 0; row < 5; ++row) {
        const u32 group = m.r8(kGroupOrder + row), labels = m.r32(kDialog) + 36 + 16 * group;
        const u32 clearRecord = m.r32(g::kScene) + kSceneProgress + kProgressStride * row;
        for (u32 mission = 11; mission <= 31; ++mission) m.w16(clearRecord + 2 * mission, 0);
        const u32 record = m.r32(g::kScene) + kSceneProgress + kProgressStride * row;
        for (u32 i = 0; i < 15; ++i) {
            const u32 mission = m.r8(kMissionFromLabel + m.r8(labels + i));
            if (mission != 31) m.w16(record + 2 * mission, m.r16(kLabelMasks + 2 * i));
        }
        const u32 tail = m.r32(g::kScene) + kSceneProgress + kProgressStride * row + 64;
        const u32 iconGroup = m.r8(kGroupOrder + row), current = m.r32(kDialog);
        const u32 icons = current + 132 + 4 * iconGroup, options = current + 156 + 4 * iconGroup;
        for (s32 kind = 5; kind >= 0; --kind) m.w8(tail + 18 + static_cast<u32>(kind), 255);
        for (u32 slot = 0; slot < 4; ++slot) {
            const u32 kind = m.r8(icons + slot) >> 1;
            if (kind >= 6) continue;
            m.w8(tail + 18 + kind, m.r8(kIconKinds + 2 * slot));
            const u32 icon = m.r8(icons + slot);
            const u32 bit = icon == 0 ? 4 : icon == 6 ? 8 : icon == 8 ? 16 : 0;
            if (bit) {
                const bool set = m.r8(options + slot) == (icon == 8 ? 1u : 0u);
                const u32 old = m.r16(tail + 16);
                m.w16(tail + 16, set ? old | bit : old & ~bit);
            }
        }
        m.w16(tail + 16, m.r16(tail + 16) & ~3u);
        const u32 completion = m.r8(m.r32(kDialog) + 21 + m.r8(kGroupOrder + row));
        if (completion == 0) m.w16(tail + 16, m.r16(tail + 16) | 2);
        if (completion == 2) m.w16(tail + 16, m.r16(tail + 16) | 1);
    }
    const u32 state = m.r32(g::kScene), completion = m.r32(state + 288) & ~4u;
    m.w32(state + 288, completion);
    if (m.r8(m.r32(kDialog) + 20) == 0) m.w32(state + 288, completion | 4);
    if (selected < 0) return completion | 4;
    const u32 record = m.r32(g::kScene) + kSceneProgress + kProgressStride * static_cast<u32>(selected);
    for (u32 option = 0; option < 2; ++option) {
        const u32 bit = 8u << option, old = m.r32(record + 88);
        m.w32(record + 88, m.r8(m.r32(kDialog) + 180 + option) == 1 ? old | bit : old & ~bit);
    }
    const u32 currentState = m.r32(g::kScene), category = m.r8(m.r32(kDialog) + 27);
    if (category < 3) {
        const u32 value = (category + 1) * (m.r32(currentState + 292) == 4 ? 38 : 16);
        const u32 target = currentState + kSceneProgress + kProgressStride * static_cast<u32>(selected);
        for (u32 field : {79u, 75u, 71u, 67u}) m.w8(target + field, value);
        return target + 60;
    }
    return 4;
}
}

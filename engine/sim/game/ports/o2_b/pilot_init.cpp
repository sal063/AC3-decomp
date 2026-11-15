#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kPilots = 0x80112DF0u;
constexpr u32 kControllerDefaults = 348, kControllerStride = 92;
constexpr u32 kSavedControllers = 708, kSavedControllerStride = 68;
void copy_word_group(Memory& m, u32 destination, u32 source, u32 count) {
    u32 words[4];
    for (u32 i = 0; i < count; ++i) words[i] = m.r32(source + 4 * i);
    for (u32 i = 0; i < count; ++i) m.w32(destination + 4 * i, words[i]);
}
}

u32 Game::o2_card_status(u32 port) {
    cover(0x800E267Cu);
    const u32 pilots = m.r32(kPilots);
    const s32 primary = m.r16s(pilots + 44 + 2 * port);
    if (!m.r8(pilots + 840 + port)) return primary == 2 ? 6u : 0u;
    if (primary == 1 || primary == 2) return static_cast<u32>(primary);
    if (primary != 0 && primary != 3) return 0xFFFFFFFFu;
    const u32 current = m.r32(kPilots);
    const s32 secondary = m.r16s(current + 48 + 2 * port);
    if (secondary >= 1 && secondary <= 3) return static_cast<u32>(secondary);
    if (secondary != 0) return 0xFFFFFFFFu;
    const u32 format = m.r8(current + 842 + port);
    return format == 1 ? 5u : format == 0 ? 4u : 1u;
}

u32 Game::o2_card_event_software(u32 port, u32 pending) {
    cover(0x800E2794u);
    Frame frame(*this, 40);
    for (u32 event = 0; event < 4; ++event) {
        if (hw->test_event(m.r32(m.r32(kPilots) + 4 + 4 * event)) != 1) continue;
        m.w8(m.r32(kPilots) + 844 + port, pending);
        return event;
    }
    return 0xFFFFFFFFu;
}

u32 Game::o2_card_event_hardware(u32 port, u32 pending) {
    cover(0x800E294Cu);
    Frame frame(*this, 40);
    for (u32 event = 0; event < 4; ++event) {
        if (hw->test_event(m.r32(m.r32(kPilots) + 20 + 4 * event)) != 1) continue;
        m.w8(m.r32(kPilots) + 844 + port, pending);
        return event;
    }
    return 0xFFFFFFFFu;
}

u32 Game::o2_pilot_records_reset() {
    cover(0x800E3B7Cu);
    Frame frame(*this, 8);
    for (u32 slot = 0; slot < 4; ++slot) {
        const u32 record = m.r32(m.r32(kPilots) + 748 + 4 * slot);
        const u32 name = m.r32(0x800CCD6Cu), last = m.r8(0x800CCD70u);
        m.w32(record + 8, name); m.w8(record + 12, last);
        m.w16(record + 20, 0); m.w16(record + 22, 0);
        for (u32 mission = 0; mission < 60; ++mission) {
            for (u32 campaign = 0; campaign < 4; ++campaign)
                m.w16(record + 34 + 120 * (3 - campaign) + 2 * mission, 0);
            for (u32 campaign = 0; campaign < 4; ++campaign)
                m.w8(record + 514 + 30 * campaign + mission, 0);
        }
        m.w16(record + 704, 0); m.w16(record + 706, 0);
        for (u32 pad = 0; pad < 6; ++pad) {
            const u32 scene = m.r32(g::kScene);
            const u32 controller = record + kSavedControllers + kSavedControllerStride * pad;
            for (u32 binding = 0; binding < 20; ++binding)
                m.w16(controller + 2 * binding,
                      m.r16(scene + kControllerDefaults + 22 + kControllerStride * pad + 2 * binding));
            const u32 source = m.r32(g::kScene) + 412 + kControllerStride * pad;
            copy_word_group(m, controller + 40, source, 4);
            copy_word_group(m, controller + 56, source + 16, 2);
            m.w32(controller + 64, m.r32(m.r32(g::kScene) + 436 + kControllerStride * pad));
        }
        m.w8(record + 1116, 0); m.w8(record + 1117, 204); m.w8(record + 1118, 204);
        m.w16(record + 1122, 0); m.w16(record + 1120, 0); m.w16(record + 22, 0);
        m.w8(record + 1124, (m.r32(m.r32(g::kScene) + 288) & 4) ? 0u : 1u);
        m.w32(record + 1128, 0);
        if (m.r8(m.r32(g::kScene) + 924) & 1) m.w32(record + 1128, 1);
        if (m.r32(m.r32(g::kScene) + 288) & 1) m.w32(record + 1128, m.r32(record + 1128) | 2);
        if (m.r32(m.r32(g::kScene) + 288) & 2) m.w32(record + 1128, m.r32(record + 1128) | 4);
        if (m.r8(m.r32(g::kScene) + 2394) == 1) m.w32(record + 1128, m.r32(record + 1128) | 8);
        for (s32 byte = 16; byte >= 0; --byte) m.w8(record + 1132 + static_cast<u32>(byte), 0);
    }
    return 0;
}

u32 Game::o2_pilot_records_init() {
    cover(0x800E3E14u);
    Frame frame(*this, 24);
    return o2_pilot_records_reset();
}

u32 Game::o2_pilot_state_init() {
    cover(0x800E3E3Cu);
    Frame frame(*this, 24);
    const u32 initial = m.r32(kPilots);
    m.w16(initial + 856, 0); m.w16(initial + 858, 0); m.w16(initial + 860, 0);
    o2_pilot_header_reset(); o2_pilot_records_reset();
    cover(0x800E3E34u);
    o2_cards_poll_begin();
    const u32 pilots = m.r32(kPilots);
    m.w8(pilots + 822, 0); m.w32(pilots + 800, 0);
    for (u32 status = 76; status <= 82; status += 2) m.w16(pilots + status, 0);
    m.w8(m.r32(kPilots) + 820, 0);
    const u32 state = m.r32(kPilots);
    copy_word_group(m, state + 824, 0x800CCD74u, 3);
    m.w16(state + 836, m.r16(0x800CCD80u));
    const u32 firstName = m.r32(kPilots) + 692;
    copy_word_group(m, firstName, 0x800CCD84u, 3);
    copy_word_group(m, firstName + 12, 0x800CCD90u, 2);
    m.w16(firstName + 20, m.r16(0x800CCD98u)); m.w8(firstName + 22, m.r8(0x800CCD9Au));
    const u32 secondName = m.r32(kPilots) + 718;
    copy_word_group(m, secondName, 0x800CCD9Cu, 3);
    copy_word_group(m, secondName + 12, 0x800CCDA8u, 2);
    m.w16(secondName + 20, m.r16(0x800CCDB0u)); m.w8(secondName + 22, m.r8(0x800CCDB2u));
    for (u32 port = 0; port < 2; ++port) {
        m.w16(state + 44 + 2 * port, 2); m.w16(state + 48 + 2 * port, 2);
    }
    return 0;
}

u32 Game::o2_card_events_open() {
    cover(0x800E3FCCu);
    Frame frame(*this, 24);
    hw->enter_critical_section();
    constexpr u32 specifications[] = {4, 0x8000, 0x100, 0x2000, 4, 0x8000, 0x100};
    for (u32 event = 0; event < 7; ++event) {
        const u32 handle = hw->open_event(event < 4 ? 0xF4000001u : 0xF0000011u,
                                        specifications[event], 0x2000, 0);
        m.w32(m.r32(kPilots) + 4 + 4 * event, handle);
    }
    for (u32 event = 0; event < 7; ++event) hw->enable_event(m.r32(m.r32(kPilots) + 4 + 4 * event));
    return hw->exit_critical_section();
}

u32 Game::o2_pilots_init() {
    cover(0x800E416Cu);
    Frame frame(*this, 24);
    o2_pilots_alloc(); o2_card_events_open();
    return o2_pilot_state_init();
}
}

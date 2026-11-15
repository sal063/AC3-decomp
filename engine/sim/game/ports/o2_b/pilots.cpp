#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kPilots = 0x80112DF0u;
constexpr u32 kHeader = 744, kSlots = 748, kActive = 760, kRecords = 764;
constexpr u32 kPollEnabled = 40, kName = 8, kProgressBits = 22;
}

u32 Game::o2_cards_poll_begin() {
    cover(0x800E25D4u);
    const u32 pilots = m.r32(kPilots);
    m.w8(pilots + 844, 1); m.w32(pilots + kPollEnabled, 1);
    m.w8(pilots + 840, 0); m.w8(pilots + 842, 0); m.w8(pilots + 848, 0);
    m.w16(pilots + 48, 0);
    m.w8(pilots + 845, 1);
    m.w8(pilots + 841, 0); m.w8(pilots + 843, 0); m.w8(pilots + 849, 0);
    m.w16(pilots + 50, 0); m.w32(pilots + 852, 0);
    return 1;
}

u32 Game::o2_cards_poll_end() {
    cover(0x800E264Cu);
    const u32 pilots = m.r32(kPilots);
    const u32 first = m.r8(pilots + 844);
    m.w32(pilots + kPollEnabled, 0);
    return first == 0 && m.r8(pilots + 845) == 0;
}

u32 Game::o2_card_events_software() {
    cover(0x800E2874u);
    Frame frame(*this, 24);
    u32 result = 0;
    for (u32 field = 4; field <= 16; field += 4)
        result = hw->test_event(m.r32(m.r32(kPilots) + field));
    return result;
}

u32 Game::o2_card_events_hardware() {
    cover(0x800E28E0u);
    Frame frame(*this, 24);
    u32 result = 0;
    for (u32 field = 20; field <= 32; field += 4)
        result = hw->test_event(m.r32(m.r32(kPilots) + field));
    return result;
}

u32 Game::o2_card_clock() {
    cover(0x800E2A2Cu);
    return m.r32(m.r32(g::kScene) + 32);
}

u32 Game::o2_pilot_name_load() {
    cover(0x800E2A44u);
    Frame frame(*this, 24);
    return libc_strcpy(m.r32(g::kScene) + 2376, m.r32(m.r32(kPilots) + kActive) + kName);
}

u32 Game::o2_pilot_name_store() {
    cover(0x800E2A7Cu);
    Frame frame(*this, 24);
    return libc_strcpy(m.r32(m.r32(kPilots) + kActive) + kName, m.r32(g::kScene) + 2376);
}

u32 Game::o2_pilot_slot_load(u32 slot) {
    cover(0x800E30A4u);
    const u32 pilots = m.r32(kPilots);
    const u32 source = m.r32(pilots + kSlots + 4 * slot);
    const u32 destination = m.r32(pilots + kActive);
    for (u32 offset = 0; offset < 1152; offset += 16) {
        const u32 words[] = {m.r32(source + offset), m.r32(source + offset + 4),
                             m.r32(source + offset + 8), m.r32(source + offset + 12)};
        for (u32 word = 0; word < 4; ++word) m.w32(destination + offset + 4 * word, words[word]);
    }
    return destination + 1152;
}

u32 Game::o2_pilot_slots_clear() {
    cover(0x800E3640u);
    const u32 header = m.r32(m.r32(kPilots) + kHeader);
    m.w8(header + 8, 0);
    return header;
}

u32 Game::o2_pilot_slot_reset(s32 slot) {
    cover(0x800E3658u);
    Frame frame(*this, 24);
    if (slot >= 3) return 0x80110000u;
    const u32 pointer = m.r32(kPilots) + (slot == -1 ? kActive : kSlots + 4 * static_cast<u32>(slot));
    m.w16(m.r32(pointer) + kProgressBits, 0);
    return libc_strcpy(m.r32(pointer) + kName, m.r32(g::kScene) + 2376);
}

u32 Game::o2_pilot_header_reset() {
    cover(0x800E3AE8u);
    m.w32(m.r32(m.r32(kPilots) + kHeader) + 4, 0x00990426u);
    m.w8(m.r32(m.r32(kPilots) + kHeader) + 8, 0);
    m.w8(m.r32(m.r32(kPilots) + kHeader) + 9, 0);
    for (u32 byte = 0; byte < 112; ++byte)
        m.w8(m.r32(m.r32(kPilots) + kHeader) + 16 + byte, 0);
    const u32 header = m.r32(m.r32(kPilots) + kHeader);
    m.w32(header + 12, 0);
    return header;
}

u32 Game::o2_pilot_header_init() {
    cover(0x800E3B5Cu);
    Frame frame(*this, 24);
    return o2_pilot_header_reset();
}

u32 Game::o2_cards_poll_disable() {
    cover(0x800E415Cu);
    const u32 pilots = m.r32(kPilots);
    m.w32(pilots + kPollEnabled, 0);
    return pilots;
}

u32 Game::o2_card_events_close() {
    cover(0x800E419Cu);
    Frame frame(*this, 24);
    hw->enter_critical_section();
    for (u32 field = 4; field <= 28; field += 4)
        hw->close_event(m.r32(m.r32(kPilots) + field));
    return hw->exit_critical_section();
}

u32 Game::o2_pilot_name(u32 slot) {
    cover(0x800E425Cu);
    if (slot == 0) return m.r32(m.r32(kPilots) + kActive) + kName;
    if (static_cast<s32>(slot) < 4)
        return m.r32(m.r32(kPilots) + kSlots + 4 * (slot - 1)) + kName;
    return 0x800CCDB4u;
}

u32 Game::o2_pilot_progress_bit(u32 slot, u32 bit) {
    cover(0x800E42ACu);
    const u32 field = slot == 0xFFFFFFFFu ? kActive : kSlots + 4 * slot;
    const s32 bits = m.r16s(m.r32(m.r32(kPilots) + field) + kProgressBits);
    return static_cast<u32>(bits >> (bit & 31)) & 1;
}

u32 Game::o2_card_selection() {
    cover(0x800E430Cu);
    return static_cast<u32>(m.r16s(m.r32(kPilots) + 78));
}

u32 Game::o2_pilot_slot_used(u32 slot) {
    cover(0x800E4324u);
    if (slot >= 3) return 0xFFFFFFFFu;
    return (m.r8(m.r32(m.r32(kPilots) + kHeader) + 8) >> slot) & 1;
}

u32 Game::o2_pilot_record_name(u32 index) {
    cover(0x800E435Cu);
    const u32 record = m.r32(m.r32(kPilots) + kRecords + 4 * (index & 255));
    return m.r32(0x8010B1B8u + 4 * m.r8(record + 274));
}

u32 Game::o2_pilot_record_code() {
    cover(0x800E4394u);
    return m.r8(m.r32(m.r32(kPilots) + 788) + 274);
}
u32 Game::o2_pilot_record_label(u32 index) {
    cover(0x800E44E0u);
    const u32 record = m.r32(m.r32(kPilots) + kRecords + 4 * (index & 255));
    return m.r32(0x8010B06Cu + 4 * m.r8(record + 275));
}

u32 Game::o2_pilot_record_category(u32 index) {
    cover(0x800E4518u);
    const u32 record = m.r32(m.r32(kPilots) + kRecords + 4 * (index & 255));
    const u32 category = m.r8(0x8010B178u + m.r8(record + 275));
    return m.r32(0x8010B160u + 4 * category);
}

u32 Game::o2_pilot_default_name() {
    cover(0x800E4560u);
    return 0x800CCDB4u;
}

u32 Game::o2_pilot_slot_mask() {
    cover(0x800E456Cu);
    return m.r8(m.r32(m.r32(kPilots) + kHeader) + 8);
}

u32 Game::o2_pilot_progress() {
    cover(0x800E458Cu);
    return static_cast<u32>(m.r16s(m.r32(m.r32(kPilots) + kActive) + kProgressBits));
}

u32 Game::o2_pilot_ending_label(u32 slot) {
    cover(0x800E45ACu);
    const u32 record = m.r32(m.r32(kPilots) + kSlots + 4 * slot);
    return m.r32(0x8010B1C8u + 4 * m.r8(record + 1124));
}

u32 Game::o2_pilot_option_first() {
    cover(0x800E45E0u);
    return m.r32(m.r32(kPilots) + kActive) + 1116;
}

u32 Game::o2_pilot_option_second() {
    cover(0x800E45F8u);
    return m.r32(m.r32(kPilots) + kActive) + 1117;
}

u32 Game::o2_pilot_option_third() {
    cover(0x800E4610u);
    return m.r32(m.r32(kPilots) + kActive) + 1118;
}

u32 Game::o2_pilot_option_fourth() {
    cover(0x800E4628u);
    return m.r32(m.r32(kPilots) + kActive) + 1120;
}

u32 Game::o2_pilot_record_code_set(u32 value) {
    cover(0x800E4640u);
    const u32 record = m.r32(m.r32(kPilots) + 788);
    m.w8(record + 274, value);
    return record;
}

u32 Game::o2_pilot_option_first_set(u32 value) {
    cover(0x800E4658u);
    const u32 record = m.r32(m.r32(kPilots) + kActive);
    m.w8(record + 1116, value);
    return record;
}

u32 Game::o2_pilot_option_second_set(u32 value) {
    cover(0x800E4670u);
    const u32 record = m.r32(m.r32(kPilots) + kActive);
    m.w8(record + 1117, value);
    return record;
}

u32 Game::o2_pilot_option_third_set(u32 value) {
    cover(0x800E4688u);
    const u32 record = m.r32(m.r32(kPilots) + kActive);
    m.w8(record + 1118, value);
    return record;
}

u32 Game::o2_pilot_slot_find() {
    cover(0x800E46A0u);
    Frame frame(*this, 40);
    for (u32 slot = 0; slot < 3; ++slot) {
        const u32 pilots = m.r32(kPilots);
        if (!(m.r8(m.r32(pilots + kHeader) + 8) & (1u << slot))) continue;
        const u32 record = m.r32(pilots + kSlots + 4 * slot);
        if (libc_strcmp(record + kName, m.r32(g::kScene) + 2376)) continue;
        const u32 selected = m.r32(m.r32(kPilots) + kSlots + 4 * slot);
        if (m.r32(selected + 4) == m.r32(m.r32(g::kScene) + 2408)) return slot;
    }
    return 0xFFFFFFFFu;
}

u32 Game::o2_card_file_create(u32 port) {
    cover(0x800E47B0u);
    Frame frame(*this, 32);
    if (!o2_cards_poll_end()) return 0;
    const u32 handle = hw->file_open(m.r32(kPilots) + 692 + 26 * port, 0x20200u);
    m.w32(m.r32(kPilots) + 60, handle);
    hw->file_close(handle);
    o2_cards_poll_begin();
    return handle == 0xFFFFFFFFu ? 0xFFFFFFFFu : 1u;
}

u32 Game::o2_card_format(u32 slot) {
    cover(0x800E4760u);
    Frame frame(*this, 24);
    if (!o2_cards_poll_end()) return 0;
    const u32 result = card_format(0x800AD808u, slot << 4);
    o2_cards_poll_begin();
    return result == 1 ? 1u : 0xFFFFFFFFu;
}

u32 Game::o2_pilot_name_assign(u32 name) {
    cover(0x800E5EA0u);
    Frame frame(*this, 24);
    return libc_strcpy(m.r32(m.r32(kPilots) + kActive) + kName, name);
}
void Game::o2_pilot_summary_store() {
    cover(0x800E2F44u);
    Frame frame(*this, 32);
    const u32 selected = m.r32(g::kScene) + 2396;
    const u32 record = m.r32(m.r32(kPilots) + kRecords + 6 * 4);
    m.w32(record + 276, m.r32(selected + 1744));
    m.w8(record + 275, m.r8(m.r32(g::kScene) + 908));
    m.w8(record + 274, m.r8(m.r32(g::kScene) + 916));
    for (u32 mission = 0; mission < 60; ++mission) {
        m.w16(record + 4 + 2 * mission, m.r16(selected + 584 + 2 * mission));
        m.w8(record + 154 + mission, m.r8(selected + 1264 + 4 * mission));
        m.w8(record + 214 + mission, m.r8(selected + 1504 + 4 * mission));
        m.w8(record + 280, m.r8(m.r32(g::kScene) + 910));
        m.w8(record + 281, m.r8(m.r32(g::kScene) + 937));
        m.w8(record + 282, m.r8(m.r32(g::kScene) + 938));
        m.w8(record + 283, m.r8(m.r32(g::kScene) + 939));
    }
    for (u32 pair = 0; pair < 30; ++pair) {
        const u32 low = m.r8(selected + 944 + 2 * pair) & 15;
        const u32 high = m.r8(selected + 945 + 2 * pair) & 15;
        m.w8(record + 124 + pair, low | (high << 4));
    }
    for (s32 byte = 99; byte >= 0; --byte) m.w8(record + 284 + static_cast<u32>(byte), 0);
}

u32 Game::o2_card_directory(u32 port) {
    cover(0x800E4DB0u);
    Frame frame(*this, 104);
    if (!o2_cards_poll_end()) return 0xFFFFFFFFu;
    const u32 pattern = frame.at(16), entry = frame.at(32);
    m.w16(m.r32(kPilots) + 84 + 2 * port, 15);
    libc_sprintf(pattern, 0x800CCE10u, {port});
    u32 files = 0;
    if (hw->first_file(pattern, entry)) {
        do {
            libc_strcpy(m.r32(kPilots) + 92 + 300 * port + 20 * files, entry);
            const s32 roundedSize = static_cast<s32>(m.r32(frame.at(56)) + 8191u);
            const u32 freeBlocks = m.r32(kPilots) + 84 + 2 * port;
            m.w16(freeBlocks, m.r16(freeBlocks) - static_cast<u32>(roundedSize / 8192));
            ++files;
        } while (hw->next_file(entry));
    }
    m.w16(m.r32(kPilots) + 88 + 2 * port, files);
    o2_cards_poll_begin();
    return static_cast<u32>(m.r16s(m.r32(kPilots) + 84 + 2 * port));
}
}

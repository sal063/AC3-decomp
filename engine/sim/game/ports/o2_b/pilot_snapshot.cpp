#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kPilots = 0x80112DF0u, kAudio = 0x800BCE48u, kActive = 760;
constexpr u32 kSelectedPilot = 2396, kControllers = 708, kControllerBytes = 68;
struct SavedArray { u32 record, selected; };
constexpr SavedArray kMissionStats[] = {{34,104}, {154,224}, {274,344}, {394,464}};
constexpr SavedArray kMissionNibbles[] = {{514,704}, {544,764}, {574,824}, {604,884}};
void word_group(Memory& m, u32 dst, u32 src, u32 count) {
    u32 words[4];
    for (u32 word = 0; word < count; ++word) words[word] = m.r32(src + 4 * word);
    for (u32 word = 0; word < count; ++word) m.w32(dst + 4 * word, words[word]);
}
void pack_fields(Memory& m, u32 dst, u32 src, u32 bytes, u32 bits) {
    const u32 perByte = 8 / bits, mask = (1u << bits) - 1;
    for (u32 byte = 0; byte < bytes; ++byte) {
        m.w8(dst + byte, 0);
        for (u32 field = 0; field < perByte; ++field) {
            const u32 value = m.r8(src + byte * perByte + field) & mask;
            m.w8(dst + byte, m.r8(dst + byte) | (value << (bits * field)));
        }
    }
}
void unpack_fields(Memory& m, u32 dst, u32 src, u32 bytes, u32 bits) {
    const u32 perByte = 8 / bits, mask = (1u << bits) - 1;
    for (u32 byte = 0; byte < bytes; ++byte)
        for (u32 field = 0; field < perByte; ++field)
            m.w8(dst + byte * perByte + field, (m.r8(src + byte) >> (bits * field)) & mask);
}
}

u32 Game::o2_pilot_snapshot_store() {
    cover(0x800E2AB4u);
    Frame frame(*this, 8);
    const u32 record = m.r32(m.r32(kPilots) + kActive), selected = m.r32(g::kScene) + kSelectedPilot;
    m.w32(record + 4, m.r32(m.r32(g::kScene) + 2408));
    for (u32 pad = 0; pad < 6; ++pad) {
        const u32 controller = record + kControllers + kControllerBytes * pad;
        const u32 scene = m.r32(g::kScene);
        for (u32 binding = 0; binding < 20; ++binding)
            m.w16(controller + 2 * binding, m.r16(scene + 370 + 92 * pad + 2 * binding));
        const u32 source = m.r32(g::kScene) + 412 + 92 * pad;
        word_group(m, controller + 40, source, 4);
        word_group(m, controller + 56, source + 16, 2);
        m.w32(controller + 64, m.r32(m.r32(g::kScene) + 436 + 92 * pad));
        m.w8(record + 1124, (m.r32(m.r32(g::kScene) + 288) & 4) ? 0u : 1u);
    }
    pack_fields(m, record + 26, selected + 24, 8, 1);
    pack_fields(m, record + 24, selected + 88, 2, 1);
    for (u32 mission = 0; mission < 60; ++mission)
        for (const auto& array : kMissionStats)
            m.w16(record + array.record + 2 * mission, m.r16(selected + array.selected + 2 * mission));
    for (u32 pair = 0; pair < 30; ++pair)
        for (const auto& array : kMissionNibbles) {
            const u32 low = m.r8(selected + array.selected + 2 * pair) & 15;
            const u32 high = m.r8(selected + array.selected + 2 * pair + 1) & 15;
            m.w8(record + array.record + pair, low | (high << 4));
        }
    pack_fields(m, record + 634, selected + 1004, 20, 2);
    pack_fields(m, record + 654, selected + 1084, 45, 2);
    const u32 audio = m.r32(kAudio);
    m.w16(record + 1120, m.r16(audio + 8)); m.w16(record + 1122, m.r16(audio + 12));
    m.w8(record + 1117, m.r8(m.r32(g::kScene) + 920));
    m.w8(record + 1118, m.r8(m.r32(g::kScene) + 921));
    m.w8(record + 1116, m.r8(m.r32(g::kScene) + 922));
    m.w16(record + 704, m.r16(selected + 1748)); m.w16(record + 706, m.r16(selected + 1752));
    m.w32(record + 1128, 0); m.w16(record + 20, m.r8(selected + 1));
    if (m.r8(m.r32(g::kScene) + 924) & 1) m.w32(record + 1128, 1);
    if (m.r32(m.r32(g::kScene) + 288) & 1) m.w32(record + 1128, m.r32(record + 1128) | 2);
    if (m.r32(m.r32(g::kScene) + 288) & 2) m.w32(record + 1128, m.r32(record + 1128) | 4);
    u32 result = 1;
    if (m.r8(m.r32(g::kScene) + 2394) == 1) {
        result = m.r32(record + 1128) | 8;
        m.w32(record + 1128, result);
    }
    for (s32 byte = 16; byte >= 0; --byte) m.w8(record + 1132 + static_cast<u32>(byte), 0);
    return result;
}

u32 Game::o2_pilot_snapshot_load(u32 selectionOut) {
    cover(0x800E30F4u);
    Frame frame(*this, 40);
    const u32 record = m.r32(m.r32(kPilots) + kActive), selected = m.r32(g::kScene) + kSelectedPilot;
    m.w8(selectionOut, m.r8(record + 20));
    libc_strcpy(m.r32(g::kScene) + 2376, m.r32(m.r32(kPilots) + kActive) + 8);
    m.w32(m.r32(g::kScene) + 2408, m.r32(m.r32(m.r32(kPilots) + kActive) + 4));
    for (u32 mission = 0; mission < 60; ++mission)
        for (const auto& array : kMissionStats)
            m.w16(selected + array.selected + 2 * mission, m.r16(record + array.record + 2 * mission));
    for (u32 pair = 0; pair < 30; ++pair)
        for (u32 half = 0; half < 2; ++half)
            for (const auto& array : kMissionNibbles)
                m.w8(selected + array.selected + 2 * pair + half, (m.r8(record + array.record + pair) >> (4 * half)) & 15);
    m.w32(selected + 1748, static_cast<u32>(m.r16s(record + 704)));
    m.w32(selected + 1752, static_cast<u32>(m.r16s(record + 706)));
    m.w8(selected + 1, m.r8(record + 20));
    for (u32 pad = 0; pad < 6; ++pad) {
        const u32 controller = record + kControllers + kControllerBytes * pad;
        const u32 scene = m.r32(g::kScene);
        for (u32 binding = 0; binding < 20; ++binding)
            m.w16(scene + 370 + 92 * pad + 2 * binding, m.r16(controller + 2 * binding));
        const u32 destination = m.r32(g::kScene) + 412 + 92 * pad;
        word_group(m, destination, controller + 40, 4);
        word_group(m, destination + 16, controller + 56, 2);
        m.w32(destination + 24, m.r32(controller + 64));
    }
    u32 scene = m.r32(g::kScene);
    m.w32(scene + 288, m.r8(record + 1124) ? (m.r32(scene + 288) & ~4u) : (m.r32(scene + 288) | 4u));
    const u32 audio = m.r32(kAudio);
    m.w32(audio + 8, static_cast<u32>(m.r16s(m.r32(m.r32(kPilots) + kActive) + 1120)));
    m.w32(audio + 12, static_cast<u32>(m.r16s(m.r32(m.r32(kPilots) + kActive) + 1122)));
    m.w8(m.r32(g::kScene) + 920, m.r8(record + 1117));
    m.w8(m.r32(g::kScene) + 921, m.r8(record + 1118));
    m.w8(m.r32(g::kScene) + 922, m.r8(record + 1116));
    req_stereo_set(m.r8(m.r32(g::kScene) + 922) & 1);
    req_volume_apply();
    unpack_fields(m, selected + 24, record + 26, 8, 1);
    unpack_fields(m, selected + 88, record + 24, 2, 1);
    unpack_fields(m, selected + 1004, record + 634, 20, 2);
    unpack_fields(m, selected + 1084, record + 654, 45, 2);
    scene = m.r32(g::kScene);
    m.w8(scene + 924, (m.r8(scene + 924) & ~1u) | (m.r32(record + 1128) & 1));
    scene = m.r32(g::kScene);
    m.w32(scene + 288, (m.r32(scene + 288) & ~1u) | ((m.r32(record + 1128) >> 1) & 1));
    scene = m.r32(g::kScene);
    m.w32(scene + 288, (m.r32(scene + 288) & ~2u) | ((m.r32(record + 1128) >> 1) & 2));
    m.w8(m.r32(g::kScene) + 2394, (m.r32(record + 1128) & 8) ? 1u : 0u);
    const u32 updatedAudio = m.r32(kAudio), result = m.r32(updatedAudio) | 2;
    m.w32(updatedAudio, result);
    return result;
}
}

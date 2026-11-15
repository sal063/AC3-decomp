#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
u32 Game::o2_sequence_labels(u32 extraArg2, u32 extraArg3) {
    cover(0x800F56D8u);
    Frame frame(*this, 48);
    constexpr u32 kSequence = 0x80112E04u, kRecordBytes = 44, kCountInRecord = 41;
    for (u32 bank = 0; bank < 2; ++bank) {
        const u32 records = bank ? 0x270 : 0x110;
        const u32 labelY = bank ? 216 : 54, countY = bank ? 223 : 59;
        for (u32 row = 0; row < 8; ++row) {
            const u32 record = records + kRecordBytes * row;
            if (!m.r8(m.r32(kSequence) + record + kCountInRecord)) break;
            o2_text_at(178, labelY + 22 * row);
            o2_text_printf(m.r32(kSequence) + record, labelY + 22 * row, extraArg2, extraArg3);
            o2_text_at(270, countY + 22 * row);
            o2_text_printf_zero(0x800CD14Cu,
                                m.r8(m.r32(kSequence) + record + kCountInRecord), extraArg2, extraArg3);
        }
    }
    return 0;
}

u32 Game::o2_sequence_clock_draw() {
    cover(0x800F5834u);
    Frame frame(*this, 24);
    constexpr u32 kSequence = 0x80112E04u, kSeconds = 2, kRunning = 0;
    const u32 sequence = m.r32(kSequence);
    const u32 ticks = m.r16(m.r32(g::kScene) + 0x3BA);
    u32 hundredths = 0;
    if (m.r8(sequence + kRunning)) {
        if (m.r16s(sequence + kSeconds) < static_cast<s32>(ticks / 30))
            hundredths = static_cast<u32>(rand() % 30 + 30);
        else
            hundredths = (ticks % 30) * 100 / 30;
    }
    o2_text_at(30, 339);
    const s32 seconds = m.r16s(m.r32(kSequence) + kSeconds);
    const s32 minutes = seconds / 60;
    return o2_text_printf_zero(0x800CD154u,
                               static_cast<u32>(static_cast<s16>(minutes % 60)),
                               static_cast<u32>(static_cast<s16>(seconds % 60)), hundredths & 255u);
}
u32 Game::o2_sequence_events() {
    cover(0x800F5018u);
    Frame frame(*this, 40);
    constexpr u32 kSequence = 0x80112E04u, kSource = 0x80112E08u;
    constexpr u32 kEventCursor = 1, kEvents = 0x390, kEventBytes = 8;
    constexpr u32 kRecordBytes = 44, kLabelId = 40, kLabelCount = 41;
    u32 sequence = m.r32(kSequence), source = m.r32(kSource);
    u32 index = m.r8(sequence + kEventCursor), total = m.r8(source + 10);
    if (index >= total) return 0;
    u32 event = source + kEvents + kEventBytes * index;
    u32 destination = sequence + 16 + 4 * index;
    if (m.r16s(sequence + 2) < m.r16s(event + 2)) return 1;
    for (;;) {
        sequence = m.r32(kSequence);
        if (m.r8(sequence + kEventCursor) >= m.r8(m.r32(kSource) + 10)) return 1;
        const u32 id = m.r8(event);
        const u32 category = id < 48 ? 1u : (id - 112u < 5 ? 2u : 0u);
        m.w8(destination + 3, 1);
        m.w8(destination, m.r8(event + 4));
        m.w8(destination + 1, m.r8(event + 5));
        m.w8(destination + 2, m.r8(event + 1));
        sequence = m.r32(kSequence);
        m.w8(sequence + kEventCursor, m.r8(sequence + kEventCursor) + 1);
        if (category) {
            const u32 bank = category == 1 ? 0x110u : 0x270u;
            const u32 countField = category == 1 ? 0x3D0u : 0x3D1u;
            sequence = m.r32(kSequence);
            const u32 count = m.r8(sequence + countField);
            u32 row = 0;
            while (row < count && m.r8(sequence + bank + kRecordBytes * row + kLabelId) != id) ++row;
            if (row < count) {
                const u32 record = sequence + bank + kRecordBytes * row;
                m.w8(record + kLabelCount, m.r8(record + kLabelCount) + 1);
            } else {
                m.w8(sequence + bank + kRecordBytes * row + kLabelId, id);
                const u32 labels = category == 1 ? 0x8010E560u : 0x8010E620u;
                const u32 labelIndex = category == 1 ? id : id - 112u;
                libc_strcpy(m.r32(kSequence) + bank + kRecordBytes * row, m.r32(labels + 4 * labelIndex));
                m.w8(m.r32(kSequence) + bank + kRecordBytes * row + kLabelCount, 1);
                sequence = m.r32(kSequence);
                m.w8(sequence + countField, m.r8(sequence + countField) + 1);
            }
            sound_play(category == 1 ? 46 : 47);
        }
        sequence = m.r32(kSequence); source = m.r32(kSource);
        index = m.r8(sequence + kEventCursor); total = m.r8(source + 10);
        if (index == total) return 1;
        event = source + kEvents + kEventBytes * index;
        destination = sequence + 16 + 4 * index;
        if (m.r16s(sequence + 2) < m.r16s(event + 2)) return 1;
    }
}

u32 Game::o2_sequence_markers(u32 packet) {
    cover(0x800F5388u);
    Frame frame(*this, 104);
    constexpr u32 kSequence = 0x80112E04u, kSource = 0x80112E08u, kMenu = 0x80112DC4u;
    const s32 sample = m.r16s(m.r32(kSequence) + 2) / 4;
    const u32 point = m.r32(kSource) + 12 + 2 * static_cast<u32>(sample);
    const s32 scrollX = m.r8(point), scrollY = m.r8(point + 1);
    const u32 line = frame.at(16), mode = frame.at(32);
    for (u32 index = 0; index < 64; ++index) {
        const u32 marker = m.r32(kSequence) + 16 + 4 * index;
        if (!m.r8(marker + 3)) break;
        const s32 x = static_cast<s32>(m.r8(marker)) - scrollX;
        const s32 y = 2 * (scrollY - static_cast<s32>(m.r8(marker + 1)));
        for (u32 byte = 0; byte < 8; ++byte) m.w8(mode + byte, 0);
        packet = o2_draw_mode_packet(mode, packet, m.r32(m.r32(kMenu) + 8));
        constexpr u8 colours[5][3] = {{255,255,255}, {0,0,255}, {0,0,0}, {255,255,0}, {255,0,0}};
        const u32 type = m.r8(marker + 2);
        if (type < 5)
            for (u32 channel = 0; channel < 3; ++channel) m.w8(line + 8 + channel, colours[type][channel]);
        for (u32 diagonal = 0; diagonal < 2; ++diagonal) {
            constexpr s32 stripeOffsets[] = {80, 78, 79};
            if (diagonal)
                for (u32 channel = 0; channel < 3; ++channel)
                    m.w8(line + 8 + channel, m.r8(line + 8 + channel) >> 1);
            for (u32 stripe = 0; stripe < 3; ++stripe) {
                if (stripe == 2)
                    for (u32 channel = 0; channel < 3; ++channel)
                        m.w8(line + 8 + channel, m.r8(line + 8 + channel) << 1);
                m.w8(line + 11, 1);
                m.w16(line, static_cast<u32>(x + stripeOffsets[stripe]));
                m.w16(line + 2, static_cast<u32>(y + (diagonal ? 200 : 184)));
                m.w16(line + 4, static_cast<u32>(x + stripeOffsets[stripe] + 8));
                m.w16(line + 6, static_cast<u32>(y + (diagonal ? 184 : 200)));
                packet = o2_line_flat(line, packet, m.r32(m.r32(kMenu) + 8));
            }
        }
        m.w8(mode + 3, 0);
        packet = o2_draw_mode_packet(mode, packet, m.r32(m.r32(kMenu) + 8));
    }
    return packet;
}

u32 Game::o2_sequence_stage_label(u32 extraArg2, u32 extraArg3) {
    cover(0x800F5A88u);
    Frame frame(*this, 24);
    o2_text_at(10, 330);
    return o2_text_printf(0x800CD160u, m.r8(m.r32(0x80112E04u)), extraArg2, extraArg3);
}
}

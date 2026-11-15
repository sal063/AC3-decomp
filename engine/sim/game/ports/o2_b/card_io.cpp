#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kPilots = 0x80112DF0u, kCardIcons = 0x80112DECu;
constexpr u32 kFile = 60, kNames = 692, kHeader = 744;
constexpr u32 kTransfer = 822, kRetries = 856, kOperation = 858;
constexpr u32 kHeaderOffset = 640, kHeaderBytes = 128, kChecksumSeed = 0x41434533u;
u32 pilots(Memory& m) { return m.r32(kPilots); }
void begin_operation(Memory& m) {
    const u32 state = pilots(m);
    if (!m.r16(state + kOperation)) {
        m.w16(state + kOperation, 1);
        m.w16(state + kRetries, 0);
    }
}
bool retry(Memory& m) {
    const u32 state = pilots(m);
    if (m.r16s(state + kRetries) >= 3) return false;
    m.w16(state + kRetries, m.r16(state + kRetries) + 1);
    return true;
}
u32 header_checksum(Memory& m) {
    const u32 header = m.r32(pilots(m) + kHeader);
    u32 checksum = kChecksumSeed;
    for (u32 offset = 4; offset < kHeaderBytes; offset += 4) checksum ^= m.r32(header + offset);
    return checksum;
}
}

u32 Game::o2_card_header_read(u32 port, u32 resumePolling) {
    cover(0x800E4EF0u);
    Frame frame(*this, 32);
    if (!o2_cards_poll_end()) return 0;
    begin_operation(m);
    if (!m.r8(pilots(m) + kTransfer)) {
        const u32 handle = hw->file_open(pilots(m) + kNames + 26 * port, 0x8001);
        m.w32(pilots(m) + kFile, handle);
        if (handle == 0xFFFFFFFFu) {
            if (retry(m)) return 0;
            o2_cards_poll_begin();
            m.w16(pilots(m) + kOperation, 0);
            return 0xFFFFFFFFu;
        }
        hw->file_lseek(handle, kHeaderOffset, 0);
        hw->file_read(m.r32(pilots(m) + kFile), m.r32(pilots(m) + kHeader), kHeaderBytes);
        m.w8(pilots(m) + kTransfer, 1);
    }
    const u32 event = o2_card_event_software(port, 0);
    if (event == 0xFFFFFFFFu) return 0;
    hw->file_close(m.r32(pilots(m) + kFile));
    m.w8(pilots(m) + kTransfer, 0);
    const u32 checksum = header_checksum(m), header = m.r32(pilots(m) + kHeader);
    if (m.r32(header) != checksum || m.r32(header + 4) != 0x00990426u) {
        o2_cards_poll_begin();
        m.w16(pilots(m) + kOperation, 0);
        return 2;
    }
    if (event != 0) {
        if (retry(m)) return 0;
        m.w16(pilots(m) + kOperation, 0);
        o2_cards_poll_begin();
        return 0xFFFFFFFFu;
    }
    m.w16(pilots(m) + kOperation, 0);
    if (resumePolling == 1) o2_cards_poll_begin();
    return 1;
}

u32 Game::o2_card_icons_write(u32 port, u32 resumePolling) {
    cover(0x800E5664u);
    Frame frame(*this, 32);
    if (!o2_cards_poll_end()) return 0;
    begin_operation(m);
    if (!m.r8(pilots(m) + kTransfer)) {
        const u32 handle = hw->file_open(pilots(m) + kNames + 26 * port, 0x8002);
        m.w32(pilots(m) + kFile, handle);
        if (handle == 0xFFFFFFFFu) {
            if (retry(m)) return 0;
            o2_cards_poll_begin();
            m.w16(pilots(m) + kOperation, 0);
            return 0xFFFFFFFFu;
        }
        hw->file_write(handle, m.r32(kCardIcons), kHeaderOffset);
        m.w8(pilots(m) + kTransfer, 1);
    }
    const u32 event = o2_card_event_software(port, 0);
    if (event == 0xFFFFFFFFu) return 0;
    hw->file_close(m.r32(pilots(m) + kFile));
    m.w8(pilots(m) + kTransfer, 0);
    if (event != 0) {
        if (retry(m)) return 0;
        m.w16(pilots(m) + kOperation, 0);
        o2_cards_poll_begin();
        return 0xFFFFFFFFu;
    }
    if (resumePolling == 1) o2_cards_poll_begin();
    return 1;
}

u32 Game::o2_card_header_write(u32 port, s32 slot, u32 resumePolling) {
    cover(0x800E57F0u);
    Frame frame(*this, 32);
    if (!o2_cards_poll_end()) return 0;
    begin_operation(m);
    if (!m.r8(pilots(m) + kTransfer)) {
        u32 header = m.r32(pilots(m) + kHeader);
        if (slot < 0) {
            m.w8(header + 8, 0);
            m.w32(m.r32(pilots(m) + kHeader) + 12, 0);
        } else {
            m.w8(header + 8, m.r8(header + 8) | (1u << (static_cast<u32>(slot) & 31)));
            header = m.r32(pilots(m) + kHeader);
            m.w32(header + 12, m.r32(header + 12) + 1);
        }
        const u32 checksum = header_checksum(m);
        m.w32(m.r32(pilots(m) + kHeader), checksum);
        const u32 handle = hw->file_open(pilots(m) + kNames + 26 * port, 0x8002);
        m.w32(pilots(m) + kFile, handle);
        if (handle == 0xFFFFFFFFu) {
            if (retry(m)) return 0;
            o2_cards_poll_begin();
            m.w16(pilots(m) + kOperation, 0);
            return 0xFFFFFFFFu;
        }
        hw->file_lseek(handle, kHeaderOffset, 0);
        hw->file_write(m.r32(pilots(m) + kFile), m.r32(pilots(m) + kHeader), kHeaderBytes);
        m.w8(pilots(m) + kTransfer, 1);
    }
    const u32 event = o2_card_event_software(port, 0);
    if (event == 0xFFFFFFFFu) return 0;
    hw->file_close(m.r32(pilots(m) + kFile));
    m.w8(pilots(m) + kTransfer, 0);
    if (event != 0) {
        if (retry(m)) return 0;
        o2_cards_poll_begin();
        m.w16(pilots(m) + kOperation, 0);
        return 0xFFFFFFFFu;
    }
    if (resumePolling == 1) o2_cards_poll_begin();
    return 1;
}
u32 Game::o2_card_slots_read(u32 port, u32 resumePolling) {
    cover(0x800E5100u);
    Frame frame(*this, 32);
    if (!o2_cards_poll_end()) return 0;
    constexpr u32 kIndex = 80, kSlots = 748, kBytes = 1152;
    const u32 state = pilots(m), index = static_cast<u32>(m.r16s(state + kIndex));
    if ((m.r8(m.r32(state + kHeader) + 8) >> (index & 31)) & 1) {
        if (!o2_cards_poll_end()) return 0;
        begin_operation(m);
        if (!m.r8(pilots(m) + kTransfer)) {
            const u32 handle = hw->file_open(pilots(m) + kNames + 26 * port, 0x8001);
            m.w32(pilots(m) + kFile, handle);
            if (handle == 0xFFFFFFFFu) {
                if (retry(m)) return 0;
                o2_cards_poll_begin();
                m.w16(pilots(m) + kOperation, 0); m.w16(pilots(m) + kIndex, 0);
                return 0xFFFFFFFFu;
            }
            const u32 slot = static_cast<u32>(m.r16s(pilots(m) + kIndex));
            hw->file_lseek(handle, 768 + kBytes * slot, 0);
            const u32 current = pilots(m), currentSlot = static_cast<u32>(m.r16s(current + kIndex));
            hw->file_read(m.r32(current + kFile), m.r32(current + kSlots + 4 * currentSlot), kBytes);
            m.w8(pilots(m) + kTransfer, 1);
        }
        const u32 event = o2_card_event_software(port, 0);
        if (event == 0xFFFFFFFFu) return 0;
        hw->file_close(m.r32(pilots(m) + kFile));
        m.w8(pilots(m) + kTransfer, 0);
        if (event != 0) {
            if (retry(m)) return 0;
            o2_cards_poll_begin();
            m.w16(pilots(m) + kOperation, 0); m.w16(pilots(m) + kIndex, 0);
            return 0xFFFFFFFFu;
        }
        const u32 record = m.r32(pilots(m) + kSlots + 4 * static_cast<u32>(m.r16s(pilots(m) + kIndex)));
        u32 checksum = kChecksumSeed;
        for (u32 offset = 4; offset < kBytes; offset += 4) checksum ^= m.r32(record + offset);
        const u32 completedRecord = m.r32(pilots(m) + kSlots + 4 * static_cast<u32>(m.r16s(pilots(m) + kIndex)));
        if (m.r32(completedRecord) != checksum) {
            o2_cards_poll_begin();
            m.w16(pilots(m) + kOperation, 0); m.w16(pilots(m) + kIndex, 0);
            return 2;
        }
    }
    const u32 current = pilots(m);
    if (m.r16s(current + kIndex) < 2) {
        m.w16(current + kIndex, m.r16(current + kIndex) + 1);
        return 0;
    }
    m.w16(current + kIndex, 0);
    if (resumePolling == 1) o2_cards_poll_begin();
    return 1;
}

u32 Game::o2_card_records_read(u32 port, u32 slot, u32 resumePolling) {
    cover(0x800E53A0u);
    Frame frame(*this, 40);
    if (!o2_cards_poll_end()) return 0;
    constexpr u32 kIndex = 80, kSlots = 748, kRecords = 764, kBytes = 384;
    const u32 state = pilots(m), pilot = m.r32(state + kSlots + 4 * slot);
    const u32 shift = static_cast<u32>(m.r16s(state + kIndex)) & 31;
    if ((m.r16s(pilot + 22) >> shift) & 1) {
        if (!o2_cards_poll_end()) return 0;
        begin_operation(m);
        if (!m.r8(pilots(m) + kTransfer)) {
            const u32 handle = hw->file_open(pilots(m) + kNames + 26 * port, 0x8001);
            m.w32(pilots(m) + kFile, handle);
            if (handle == 0xFFFFFFFFu) {
                if (retry(m)) return 0;
                m.w16(pilots(m) + kOperation, 0);
                o2_cards_poll_begin();
                m.w16(pilots(m) + kIndex, 0);
                return 0xFFFFFFFFu;
            }
            const u32 index = static_cast<u32>(m.r16s(pilots(m) + kIndex));
            hw->file_lseek(handle, 4224 + 2304 * slot + kBytes * index, 0);
            const u32 current = pilots(m), currentIndex = static_cast<u32>(m.r16s(current + kIndex));
            hw->file_read(m.r32(current + kFile), m.r32(current + kRecords + 4 * currentIndex), kBytes);
            m.w8(pilots(m) + kTransfer, 1);
        }
        const u32 event = o2_card_event_software(port, 0);
        if (event == 0xFFFFFFFFu) return 0;
        hw->file_close(m.r32(pilots(m) + kFile));
        m.w8(pilots(m) + kTransfer, 0);
        if (event != 0) {
            if (retry(m)) return 0;
            m.w16(pilots(m) + kOperation, 0);
            o2_cards_poll_begin();
            m.w16(pilots(m) + kIndex, 0);
            return 0xFFFFFFFFu;
        }
        const u32 record = m.r32(pilots(m) + kRecords + 4 * static_cast<u32>(m.r16s(pilots(m) + kIndex)));
        u32 checksum = kChecksumSeed;
        for (u32 offset = 4; offset < kBytes; offset += 4) checksum ^= m.r32(record + offset);
        const u32 completedRecord = m.r32(pilots(m) + kRecords + 4 * static_cast<u32>(m.r16s(pilots(m) + kIndex)));
        if (m.r32(completedRecord) != checksum) {
            o2_cards_poll_begin();
            m.w16(pilots(m) + kOperation, 0); m.w16(pilots(m) + kIndex, 0);
            return 2;
        }
    }
    const u32 current = pilots(m);
    if (m.r16s(current + kIndex) < 6) {
        m.w16(current + kIndex, m.r16(current + kIndex) + 1);
        return 0;
    }
    if (resumePolling == 1) o2_cards_poll_begin();
    m.w16(pilots(m) + kOperation, 0); m.w16(pilots(m) + kIndex, 0);
    return 1;
}
u32 Game::o2_card_slot_write(u32 port, u32 slot, u32 ending, u32 resumePolling) {
    cover(0x800E5A28u);
    Frame frame(*this, 40);
    if (!o2_cards_poll_end()) return 0;
    begin_operation(m);
    if (ending < 6) {
        m.w16(m.r32(pilots(m) + 760) + 20, ending);
        const u32 saved = m.r32(pilots(m) + 748 + 4 * slot);
        m.w16(m.r32(pilots(m) + 760) + 22, m.r16(saved + 22));
        const u32 active = m.r32(pilots(m) + 760);
        m.w16(active + 22, m.r16(active + 22) | (1u << ending));
    } else if (ending == 6) {
        m.w16(m.r32(pilots(m) + 760) + 20, 0);
        m.w16(m.r32(pilots(m) + 760) + 22, 0);
    }
    if (!m.r8(pilots(m) + kTransfer)) {
        const u32 active = m.r32(pilots(m) + 760);
        u32 checksum = kChecksumSeed;
        for (u32 offset = 4; offset < 1152; offset += 4) checksum ^= m.r32(active + offset);
        m.w32(active, checksum);
        const u32 handle = hw->file_open(pilots(m) + kNames + 26 * port, 0x8002);
        m.w32(pilots(m) + kFile, handle);
        if (handle == 0xFFFFFFFFu) {
            if (retry(m)) return 0;
            o2_cards_poll_begin();
            m.w16(pilots(m) + kOperation, 0);
            return 0xFFFFFFFFu;
        }
        hw->file_lseek(handle, 768 + 1152 * slot, 0);
        hw->file_write(m.r32(pilots(m) + kFile), m.r32(pilots(m) + 760), 1152);
        m.w8(pilots(m) + kTransfer, 1);
    }
    const u32 event = o2_card_event_software(port, 0);
    if (event == 0xFFFFFFFFu) return 0;
    hw->file_close(m.r32(pilots(m) + kFile));
    m.w8(pilots(m) + kTransfer, 0);
    if (event) {
        if (retry(m)) return 0;
        m.w16(pilots(m) + kOperation, 0);
        o2_cards_poll_begin();
        return 0xFFFFFFFFu;
    }
    if (resumePolling == 1) o2_cards_poll_begin();
    return 1;
}

u32 Game::o2_card_record_write(u32 port, u32 slot, u32 ending, u32 resumePolling) {
    cover(0x800E5CA4u);
    Frame frame(*this, 40);
    if (!o2_cards_poll_end()) return 0;
    begin_operation(m);
    if (!m.r8(pilots(m) + kTransfer)) {
        const u32 record = m.r32(pilots(m) + 788);
        u32 checksum = kChecksumSeed;
        for (u32 offset = 4; offset < 384; offset += 4) checksum ^= m.r32(record + offset);
        m.w32(record, checksum);
        const u32 handle = hw->file_open(pilots(m) + kNames + 26 * port, 0x8002);
        m.w32(pilots(m) + kFile, handle);
        if (handle == 0xFFFFFFFFu) {
            if (retry(m)) return 0;
            o2_cards_poll_begin();
            m.w16(pilots(m) + kOperation, 0);
            return 0xFFFFFFFFu;
        }
        hw->file_lseek(handle, 4224 + 2304 * slot + 384 * ending, 0);
        hw->file_write(m.r32(pilots(m) + kFile), m.r32(pilots(m) + 788), 384);
        m.w8(pilots(m) + kTransfer, 1);
    }
    const u32 event = o2_card_event_software(port, 0);
    if (event == 0xFFFFFFFFu) return 0;
    hw->file_close(m.r32(pilots(m) + kFile));
    m.w8(pilots(m) + kTransfer, 0);
    if (event) {
        if (retry(m)) return 0;
        o2_cards_poll_begin();
        m.w16(pilots(m) + kOperation, 0);
        return 0xFFFFFFFFu;
    }
    if (resumePolling == 1) o2_cards_poll_begin();
    return 1;
}
}

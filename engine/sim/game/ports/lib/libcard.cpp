#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDirectory = 0x800CBC98u;
constexpr u32 kBrokenSectors = 0x800CBE78u;
constexpr u32 kFrame = 0x800CBED8u;
constexpr u32 kFrameSize = 128u;
constexpr u32 kDirEntries = 15u, kDirEntrySize = 32u, kBrokenEntries = 20u;
constexpr u32 kDirFirstFrame = 1u, kBrokenFirstFrame = 16u;
constexpr u32 kEntryFree = 0xA0u;
constexpr u32 kNoLink = 0xFFFFu;

constexpr u32 kPatchVector = 0x0000DFFCu;
constexpr u32 kPatchEntryA = 0xA000DFACu, kPatchEntryB = 0xA000DF80u;
}

void Game::card_frame_checksum() {
    u32 x = 0;
    for (u32 k = 0; k < kFrameSize - 1; ++k) x ^= m.r8(kFrame + k);
    m.w8(kFrame + kFrameSize - 1, x);
}

bool Game::card_frame_write(u32 chan, u32 frame) {
    return hw->card_write(chan, frame, kFrame) == 1;
}

u32 Game::card_format(u32 fn, u32 chan) {
    cover(fn);
    Frame f(*this, 0x38);
    for (u32 i = 0; i < kDirEntries; ++i) {
        const u32 entry = kDirectory + kDirEntrySize * i;
        m.fill(kFrame, 0, kFrameSize);
        m.fill(entry, 0, kDirEntrySize);
        m.w32(entry, kEntryFree);
        m.w32(entry + 4, 0);
        m.w16(entry + 8, kNoLink);
        m.copy(kFrame, entry, 16);
        m.copy(kFrame + 16, entry + 16, 16);
        card_frame_checksum();
        if (!card_frame_write(chan, kDirFirstFrame + i)) return 0;
    }
    for (u32 i = 0; i < kBrokenEntries; ++i) {
        const u32 entry = kBrokenSectors + 4 * i;
        m.w32(entry, 0xFFFFFFFFu);
        m.fill(kFrame, 0, kFrameSize);
        m.copy(kFrame, entry, 4);
        card_frame_checksum();
        if (!card_frame_write(chan, kBrokenFirstFrame + i)) return 0;
    }
    m.fill(kFrame, 0, kFrameSize);
    m.w8(kFrame, 'M');
    m.w8(kFrame + 1, 'C');
    card_frame_checksum();
    if (!card_frame_write(chan, 0)) return 0;
    m.fill(kFrame, 0, kFrameSize);
    if (hw->card_read(chan, 0, kFrame) != 1) return 0;
    if (hw->card_load(chan) != 1) return 0;
    return 1;
}

void Game::card_tpl_control(u32 base, u32 bits) {
    cover(0x800AD5ACu);
    m.w16(base + 10, m.r16(base + 10) | bits | 0x12u);
}

u32 Game::card_tpl_wait(u32 base) {
    cover(0x800AD5D8u);
    if (!(m.r32(base + 0x1074) & 0x80u)) return 0;
    while (m.r32(base + 0x1044) & 0x80u) {
    }
    return lib_call(m.r32(kPatchVector));
}

u32 Game::card_tpl_jump() {
    cover(0x800AD61Cu);
    return lib_call(kPatchEntryA);
}

u32 Game::card_tpl_call() {
    cover(0x800AD630u);
    lib_call(kPatchEntryB);
    return 0;
}
}

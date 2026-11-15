#include "core/wrap.hpp"
#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kSpuRegs = 0x800B47C4u;
constexpr u32 kSpuDmaChcr = 0x800B47D8u;
constexpr u32 kTransferAddr = 0x800B47DCu;
constexpr u32 kSemitones = 0x800B4C34u;
constexpr u32 kFineSteps = 0x800B4C4Cu;

constexpr u32 kRegTransferAddr = 0x1A6u, kRegFifo = 0x1A8u, kRegCnt = 0x1AAu, kRegStat = 0x1AEu;
constexpr u32 kCntTransferMode = 0x30u, kCntManualWrite = 0x10u;
constexpr u32 kStatBusy = 0x400u, kStatMode = 0x7FFu;
constexpr u32 kBlock = 64u;
constexpr u32 kTimeout = 0xF00u;
}

void Game::spu_write_by_io(u32 src, u32 size) {
    cover(0x8009F458u);
    Frame f(*this, 0x28);
    u32 spu = m.r32(kSpuRegs);
    const u32 addr = m.r16(kTransferAddr);
    const u32 mode = m.r16(spu + kRegStat) & kStatMode;
    m.w16(spu + kRegTransferAddr, addr);
    while (size) {
        const u32 n = size < kBlock + 1 ? size : kBlock;
        if (static_cast<s32>(n) > 0) {
            spu = m.r32(kSpuRegs);
            for (u32 i = 0; static_cast<s32>(i) < static_cast<s32>(n); i += 2, src += 2)
                m.w16(spu + kRegFifo, m.r16(src));
        }
        spu = m.r32(kSpuRegs);
        m.w16(spu + kRegCnt, (m.r16(spu + kRegCnt) & ~kCntTransferMode & 0xFFFFu) | kCntManualWrite);
        if (m.r16(m.r32(kSpuRegs) + kRegStat) & kStatBusy) {
            for (u32 polls = 1;; ++polls) {
                if (polls > kTimeout) break;
                if (!(m.r16(m.r32(kSpuRegs) + kRegStat) & kStatBusy)) break;
            }
        }
        size -= n;
    }
    spu = m.r32(kSpuRegs);
    m.w16(spu + kRegCnt, m.r16(spu + kRegCnt) & ~kCntTransferMode & 0xFFFFu);
    if ((m.r16(spu + kRegStat) & kStatMode) != mode) {
        for (u32 polls = 1;; ++polls) {
            if (polls > kTimeout) break;
            if ((m.r16(m.r32(kSpuRegs) + kRegStat) & kStatMode) == mode) break;
        }
    }
}

void Game::spu_dma_start(u32 mode_bits) {
    const u32 chcr = m.r32(kSpuDmaChcr);
    m.w32(chcr, (m.r32(chcr) & 0xF0FFFFFFu) | mode_bits);
}

void Game::spu_dma_start_a() {
    cover(0x8009FC60u);
    spu_dma_start(0x20000000u);
}

void Game::spu_dma_start_b() {
    cover(0x8009FC88u);
    spu_dma_start(0x22000000u);
}

u32 Game::spu_note2pitch(u32 cen_hi, u32 cen_lo, u32 note, u32 fine) {
    cover(0x800A1E98u);
    const u32 tune = (fine + cen_lo) & 0xFFFFu;
    const s32 semis = static_cast<s16>(note + (tune >> 7) - cen_hi);
    const s32 octave = semis / 12;
    s32 rem = semis - octave * 12;
    s32 shift = octave - 2;
    if (static_cast<s16>(rem) < 0) {
        rem += 12;
        shift = octave - 3;
    }
    const u32 semitone = m.r16(kSemitones + 2u * static_cast<u32>(static_cast<s16>(rem)));
    const u32 step = m.r16(kFineSteps + 2u * (tune & 0x7Fu));
    u32 pitch = static_cast<u32>(static_cast<s32>(semitone * step) >> 16);
    shift = static_cast<s16>(shift);
    if (shift >= 0) {
        pitch = 0x3FFF;
    } else {
        const u32 down = static_cast<u32>(-shift);
        pitch = wrap::shr(pitch + wrap::shl(1u, down - 1), down);
    }
    return pitch & 0xFFFFu;
}

u32 Game::spu_pitch2note(u32 cen_hi, u32 cen_lo, u32 pitch, u32 t2_in, u32 t1_in) {
    cover(0x800A1F68u);
    if ((pitch & 0xFFFFu) >= 0x4000u) pitch = 0x3FFF;
    const u32 p = pitch & 0xFFFFu;
    u32 top = 0;
    for (u32 b = 0; b < 14; ++b)
        if ((p >> b) & 1u) top = b;
    const u32 norm = (p << (15 - top)) & 0xFFFFu;
    u32 semitone = t2_in;
    for (s32 k = 11; k >= 0; --k) {
        if (norm >= m.r16(kSemitones + 2u * static_cast<u32>(k))) {
            semitone = static_cast<u32>(k);
            break;
        }
    }
    const u32 rest = wrap::divu(norm << 15, m.r16(kSemitones + 2u * (semitone & 0xFFFFu))).lo & 0xFFFFu;
    u32 step = t1_in;
    for (s32 k = 0x7F; k >= 0; --k) {
        if (rest >= m.r16(kFineSteps + 2u * static_cast<u32>(k))) {
            step = static_cast<u32>(k);
            break;
        }
    }
    const u32 tune = cen_lo + step + 1;
    const u32 note = cen_hi + (top - 12) * 12 + semitone + ((tune & 0xFFFFu) >> 7);
    return ((note & 0xFFFFu) << 8) | (tune & 0x7Eu);
}
}

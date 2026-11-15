#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kVideoMode = 0x800B61B4u;

constexpr u32 kIntrInit = 0x800B50ACu;
constexpr u32 kIntrBusy = 0x800B50AEu;
constexpr u32 kIntrHandlers = 0x800B50B0u;
constexpr u32 kIntrMask = 0x800B50DCu;
constexpr u32 kIntrSavedMask = 0x800B50DEu;
constexpr u32 kIntrSavedDpcr = 0x800B50E0u;
constexpr u32 kIntrJmpBuf = 0x800B50E4u;
constexpr u32 kIntrStackTop = 0x800B60C4u;
constexpr u32 kIntrWords = 0x41Au;
constexpr u32 kIntrTable = 0x800B6134u;
constexpr u32 kIStatPtr = 0x800B6138u;
constexpr u32 kIMaskPtr = 0x800B613Cu;
constexpr u32 kDpcrPtr = 0x800B6140u;
constexpr u32 kIntrStall = 0x800B6144u;
constexpr u32 kIntrSources = 11u;
constexpr s32 kStallLimit = 0x800;

constexpr u32 kVsyncCallbacks = 0x800B6154u;
constexpr u32 kVsyncCount = 0x800B6174u;
constexpr u32 kDicrPtr = 0x800B6184u;
constexpr u32 kDmaCallbacks = 0x800B6188u;
constexpr u32 kDmaBasePtr = 0x800B61A8u;
constexpr u32 kDmaChannels = 7u;
}

u32 Game::lib_call(u32 fn, u32 a0, u32 a1, u32 a2, u32 a3) {
    u32 regs[34] = {};
    regs[4] = a0;
    regs[5] = a1;
    regs[6] = a2;
    regs[7] = a3;
    regs[28] = kGp;
    regs[29] = sp;
    u32 out = 0;
    if (port_call_lib(fn, regs, out)) {
        port_regs.mask = 0;
        return out;
    }
    return call_by_address(fn, "a function called through a library pointer", a0, a1, a2, a3);
}

u32 Game::lib_clear_words(u32 fn, u32 p, u32 n) {
    cover(fn);
    for (; n; --n, p += 4) m.w32(p, 0);
    return 0xFFFFFFFFu;
}

void Game::vsync_wait(s32 target, u32 timeout) {
    cover(0x800A2210u);
    Frame f(*this, 0x20);
    const u32 polls = f.at(0x10);
    m.w32(polls, timeout << 15);
    if (m.r32s(kVsyncCount) >= target) return;
    do {
        m.w32(polls, m.r32(polls) - 1);
        if (m.r32(polls) == 0xFFFFFFFFu) break;
    } while (m.r32s(kVsyncCount) < target);
}

u32 Game::intr_install(const u32* regs) {
    cover(0x800A243Cu);
    Frame f(*this, 0x18);
    if (m.r16(kIntrInit)) return 0;
    const u32 istat = m.r32(kIStatPtr), imask = m.r32(kIMaskPtr);
    m.w16(imask, 0);
    m.w16(istat, m.r16(imask));
    m.w32(m.r32(kDpcrPtr), 0x33333333u);
    lib_clear_words(0x800A2944u, kIntrInit, kIntrWords);
    const u32 saved[8] = {kIntrInit, regs[17], regs[18], regs[19], regs[20], regs[21], regs[22], regs[23]};
    if (libc_setjmp(kIntrJmpBuf, 0x800A24A4u, sp, regs[30], saved, regs[28])) intr_dispatch();
    m.w32(kIntrJmpBuf + 4, kIntrStackTop);
    m.w16(kIntrInit, 1);
    m.w32(m.r32(kIntrTable) + 20, 0);
    m.w32(m.r32(kIntrTable) + 4, 0);
    hw->exit_critical_section();
    return kIntrInit;
}

void Game::intr_dispatch() {
    cover(0x800A2514u);
    Frame f(*this, 0x28);
    if (!m.r16(kIntrInit)) {
        (void)m.r16(m.r32(kIStatPtr));
    }
    u32 istat = m.r32(kIStatPtr);
    u32 mask = m.r16(kIntrMask);
    m.w16(kIntrBusy, 1);
    u32 imask = m.r32(kIMaskPtr);
    u32 pending = mask & m.r16(istat) & m.r16(imask);
    while (pending) {
        for (u32 n = 0; pending && n < kIntrSources; ++n, pending >>= 1) {
            if (!(pending & 1u)) continue;
            m.w16(m.r32(kIStatPtr), ~(1u << n));
            if (const u32 handler = m.r32(kIntrHandlers + 4 * n)) lib_call(handler);
        }
        istat = m.r32(kIStatPtr);
        mask = m.r16(kIntrMask);
        imask = m.r32(kIMaskPtr);
        pending = mask & m.r16(istat) & m.r16(imask);
    }
    istat = m.r32(kIStatPtr);
    imask = m.r32(kIMaskPtr);
    if (m.r16(istat) & m.r16(imask)) {
        const s32 stalls = m.r32s(kIntrStall);
        m.w32(kIntrStall, static_cast<u32>(stalls) + 1);
        if (stalls > kStallLimit) {
            (void)m.r16(istat);
            (void)m.r16(imask);
            const u32 ack = m.r32(kIStatPtr);
            m.w32(kIntrStall, 0);
            m.w16(ack, 0);
        }
    } else {
        m.w32(kIntrStall, 0);
    }
    m.w16(kIntrBusy, 0);
}

u32 Game::intr_callback_set(u32 n, u32 fn) {
    cover(0x800A26E4u);
    Frame f(*this, 0x28);
    const u32 slot = kIntrHandlers + 4 * n;
    const u32 old = m.r32(slot);
    if (fn == old || !m.r16(kIntrInit)) return old;
    const u32 imask = m.r32(kIMaskPtr);
    u32 enabled = m.r16(imask);
    m.w16(imask, 0);
    const u32 bit = 1u << (n & 31u);
    if (fn) {
        m.w32(slot, fn);
        enabled |= bit;
        m.w16(kIntrMask, m.r16(kIntrMask) | bit);
    } else {
        m.w32(slot, 0);
        enabled &= ~bit;
        m.w16(kIntrMask, m.r16(kIntrMask) & ~bit);
    }
    m.w16(m.r32(kIMaskPtr), enabled);
    return old;
}

u32 Game::intr_stop() {
    cover(0x800A282Cu);
    Frame f(*this, 0x18);
    if (!m.r16(kIntrInit)) return 0;
    hw->enter_critical_section();
    const u32 imask = m.r32(kIMaskPtr);
    u32 dpcr = m.r32(kDpcrPtr);
    m.w16(kIntrSavedMask, m.r16(imask));
    const u32 channels = m.r32(dpcr);
    const u32 istat = m.r32(kIStatPtr);
    m.w32(kIntrSavedDpcr, channels);
    m.w16(imask, 0);
    m.w16(istat, m.r16(imask));
    dpcr = m.r32(kDpcrPtr);
    m.w32(dpcr, m.r32(dpcr) & 0x77777777u);
    m.w16(kIntrInit, 0);
    return kIntrInit;
}

u32 Game::intr_restart() {
    cover(0x800A28CCu);
    Frame f(*this, 0x18);
    if (m.r16(kIntrInit)) return 0;
    const u32 imask = m.r32(kIMaskPtr);
    const u32 enabled = m.r16(kIntrSavedMask);
    m.w16(kIntrInit, 1);
    m.w16(imask, enabled);
    m.w32(m.r32(kDpcrPtr), m.r32(kIntrSavedDpcr));
    hw->exit_critical_section();
    return kIntrInit;
}

void Game::vsync_intr() {
    cover(0x800A29C0u);
    Frame f(*this, 0x20);
    m.w32(kVsyncCount, m.r32(kVsyncCount) + 1);
    for (u32 k = 0; k < 8; ++k)
        if (const u32 callback = m.r32(kVsyncCallbacks + 4 * k)) lib_call(callback);
}

u32 Game::vsync_callback_set(u32 n, u32 fn) {
    cover(0x800A2A2Cu);
    const u32 slot = kVsyncCallbacks + 4 * n;
    const u32 old = m.r32(slot);
    if (fn != old) m.w32(slot, fn);
    return old;
}

void Game::dma_intr() {
    cover(0x800A2AD4u);
    Frame f(*this, 0x30);
    auto flags = [&] { return (m.r32(m.r32(kDicrPtr)) >> 24) & 0x7Fu; };
    for (u32 pending = flags(); pending; pending = flags()) {
        for (u32 ch = 0; pending && ch < kDmaChannels; ++ch, pending >>= 1) {
            if (!(pending & 1u)) continue;
            const u32 dicr = m.r32(kDicrPtr);
            m.w32(dicr, m.r32(dicr) & ((1u << (ch + 24)) | 0xFFFFFFu));
            if (const u32 callback = m.r32(kDmaCallbacks + 4 * ch)) lib_call(callback);
        }
    }
    const u32 dicr = m.r32(kDicrPtr);
    if ((m.r32(dicr) & 0xFF000000u) == 0x80000000u || (m.r32(dicr) & 0x8000u)) {
        (void)m.r32(dicr);
        for (u32 ch = 0; ch < kDmaChannels; ++ch)
            (void)m.r32(m.r32(kDmaBasePtr) + 16 * ch);
    }
}

u32 Game::dma_callback_set(u32 ch, u32 fn) {
    cover(0x800A2C54u);
    const u32 slot = kDmaCallbacks + 4 * ch;
    const u32 old = m.r32(slot);
    if (fn == old) return old;
    const u32 dicr = m.r32(kDicrPtr);
    const u32 enable = 1u << ((ch + 16) & 31u);
    if (fn) {
        m.w32(slot, fn);
        m.w32(dicr, (m.r32(dicr) & 0xFFFFFFu) | enable | 0x800000u);
    } else {
        m.w32(slot, 0);
        m.w32(dicr, ((m.r32(dicr) & 0xFFFFFFu) | 0x800000u) & ~enable);
    }
    return old;
}

u32 Game::set_video_mode(u32 mode) {
    cover(0x800A2D28u);
    const u32 old = m.r32(kVideoMode);
    m.w32(kVideoMode, mode);
    return old;
}

u32 Game::get_video_mode() {
    cover(0x800A2D3Cu);
    return m.r32(kVideoMode);
}
}

#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kPadPorts = 0x800C7CB8u;
constexpr u32 kPortSize = 240u;

constexpr u32 kPortModes = 0, kPortActs = 4, kPortCombs = 8;
constexpr u32 kPortSubPorts = 12;
constexpr u32 kPortRequest = 20;
constexpr u32 kPortResult = 24;
constexpr u32 kPortAlign = 32;
constexpr u32 kPortParam = 36;
constexpr u32 kPortActData = 40;
constexpr u32 kPortSendData = 44;
constexpr u32 kPortRecv = 48;
constexpr u32 kPortActLen = 52;
constexpr u32 kPortRecvLen = 53;
constexpr u32 kPortSendLen = 54;
constexpr u32 kPortCommand = 55;
constexpr u32 kPortLastCommand = 56;
constexpr u32 kPortRx = 60, kPortTx = 64;
constexpr u32 kPortRxCount = 68, kPortTxCount = 69;
constexpr u32 kPortState = 70;
constexpr u32 kPortStep = 71, kPortSubStep = 72;
constexpr u32 kPortReset = 73;
constexpr u32 kPortFails = 74;
constexpr u32 kPortConfig = 80;
constexpr u32 kPortMode = 81, kPortLock = 82, kPortSameMode = 83;
constexpr u32 kPortActSends = 87;
constexpr u32 kPortIds = 93;
constexpr u32 kPortModeCount = 227, kPortCurMode = 228;
constexpr u32 kPortStable = 230;
constexpr u32 kPortType = 232;
constexpr u32 kPortActCount = 233, kPortCombCount = 234, kPortRetry = 235;
constexpr u32 kPortCheck = 238;
constexpr u32 kPortTableEnd = 227;

constexpr u32 kEndFn = 0x800B4684u;
constexpr u32 kResetFn = 0x800B4688u;
constexpr u32 kTxByteFn = 0x800B468Cu;
constexpr u32 kActPrepareFn = 0x800B4690u;
constexpr u32 kPortLookupFn = 0x800B4698u;
constexpr u32 kRequestFn = 0x800B469Cu;
constexpr u32 kBusyFn = 0x800B46A0u;
constexpr u32 kResultFn = 0x800B46A4u;
constexpr u32 kDoneFn = 0x800B46A8u;
constexpr u32 kHookA = 0x800B46B0u, kHookB = 0x800B46B4u, kHooksOn = 0x800B46C0u;
constexpr u32 kPortArray = 0x800B46B8u;
constexpr u32 kCurPort = 0x800B46C4u;
constexpr u32 kXferState = 0x800B46C8u;
constexpr u32 kActBudget = 0x800B46CCu;
constexpr u32 kMultitap = 0x800B46D0u;
constexpr u32 kFirstPort = 0x800B46D4u, kLastPort = 0x800B46D8u;
constexpr u32 kTapSlots = 0x800B46DCu;
constexpr u32 kIStatPtr = 0x800B46E4u;
constexpr u32 kSioPtr = 0x800B46E8u;
constexpr u32 kBytesLeft = 0x800B46FCu;
constexpr u32 kSio2Ptr = 0x800B4700u;
constexpr u32 kRequested = 0x800B4704u;
constexpr u32 kTapPoll = 0x800B4708u;
constexpr u32 kXferFns = 0x800B470Cu;
constexpr u32 kSio3Ptr = 0x800B472Cu;
constexpr u32 kCombCursor = 0x800C7C18u;
constexpr u32 kWaitStart = 0x800C7E98u;
constexpr u32 kWaitTicks = 0x800C7E9Cu;

constexpr u32 kSioData = 0, kSioStat = 4, kSioCtrl = 10, kSioBaud = 14;
constexpr u32 kSioTxReady = 1, kSioRxReady = 2;
constexpr u32 kRc2Count = 0x1F801120u, kRc2Mode = 0x1F801124u, kRc2Target = 0x1F801128u;
constexpr u32 kIrqPad = 0x80u;

constexpr u32 kClrIntSio0 = 0;
constexpr u32 kInitSioMode = 0;
constexpr u32 kRecvAtLoadInfo = 1;
}

u32 Game::pad_wait_set(u32 ticks) {
    cover(0x8009E8F0u);
    const u32 count = m.r16(kRc2Count);
    m.w32(kWaitTicks, ticks);
    m.w32(kWaitStart, count);
    return count;
}

u32 Game::pad_wait_over() {
    cover(0x8009E910u);
    u32 count = m.r16(kRc2Count);
    if (count < m.r32(kWaitStart)) count += m.r16(kRc2Target) ? m.r16(kRc2Target) : 0x10000u;
    u32 elapsed;
    if (m.r16(kRc2Mode) & 0x200u) elapsed = count - m.r32(kWaitStart);
    else elapsed = (count - m.r32(kWaitStart)) >> 3;
    return elapsed < m.r32(kWaitTicks) ? 0u : 1u;
}

s32 Game::pad_sio_next(u32 port, u32 send) {
    cover(0x8009C194u);
    Frame f(*this, 0x28);
    if (static_cast<s32>(send) < 0) {
        u32 sio = m.r32(kSioPtr);
        const u32 tx = m.r32(port + kPortTx);
        const u32 got = m.r8(sio + kSioData);
        m.w8(port + kPortRxCount, 0xFF);
        m.w8(port + kPortTxCount, 1);
        m.w8(tx, ~send);
        sio = m.r32(kSioPtr);
        while (!(m.r16(sio + kSioStat) & kSioTxReady)) {
        }
        while (!pad_wait_over()) {
        }
        m.w8(m.r32(kSioPtr) + kSioData, ~send);
        return static_cast<s32>(got);
    }
    u32 baud = 0x88;
    if ((m.r8(m.r32(port + kPortRx)) >> 4) == 8 && m.r8(port + kPortRxCount) >= 9) baud = 0x22;
    const u32 count = m.r16(kRc2Count);
    u32 sio = m.r32(kSioPtr);
    const u32 stat = m.r16(sio + kSioStat);
    m.w32(kWaitTicks, 0x1AE);
    m.w32(kWaitStart, count);
    if (!(stat & kSioRxReady))
        while (!(m.r16(sio + kSioStat) & kSioRxReady)) {
        }
    sio = m.r32(kSioPtr);
    const u32 istat = m.r32(kIStatPtr);
    const u32 got = m.r8(sio + kSioData);
    m.w16(sio + kSioBaud, baud);
    if (!(m.r32(istat) & kIrqPad)) {
        do {
            if (pad_wait_over()) return -20;
        } while (!(m.r32(m.r32(kIStatPtr)) & kIrqPad));
    }
    m.w8(m.r32(kSioPtr) + kSioData, send);
    if (baud == 0x22) {
        const u32 ack = m.r32(kIStatPtr);
        sio = m.r32(kSioPtr);
        m.w32(ack, ~kIrqPad);
        m.w16(sio + kSioCtrl, m.r16(sio + kSioCtrl) | 0x10u);
    }
    const u32 sent = m.r8(port + kPortTxCount), at = m.r8(port + kPortRxCount);
    const u32 rx = m.r32(port + kPortRx);
    m.w8(port + kPortTxCount, sent + 1);
    m.w8(rx + at, got);
    m.w8(port + kPortRxCount, m.r8(port + kPortRxCount) + 1);
    return static_cast<s32>(got);
}

s32 Game::pad_sio_first(u32 port, u32 send) {
    cover(0x8009C3A4u);
    Frame f(*this, 0x28);
    u32 baud = 0x88;
    if ((m.r8(m.r32(port + kPortRx)) >> 4) == 8 && m.r8(port + kPortRxCount) >= 9) baud = 0x22;
    u32 sio = m.r32(kSioPtr);
    while (!(m.r16(sio + kSioStat) & kSioRxReady)) {
    }
    pad_wait_set(0x190);
    sio = m.r32(kSioPtr);
    const u32 got = m.r8(sio + kSioData);
    m.w16(sio + kSioBaud, m.r8(port + kPortRxCount) == 0 && (got >> 4) == 8 ? 0x22u : baud);
    const u32 istat = m.r32(kIStatPtr);
    if (!(m.r32(istat) & kIrqPad)) {
        const u32 start = m.r32(kWaitStart), ticks = m.r32(kWaitTicks);
        do {
            u32 count = m.r16(kRc2Count);
            if (count < start) count += m.r16(kRc2Target) ? m.r16(kRc2Target) : 0x10000u;
            const u32 elapsed = (m.r16(kRc2Mode) & 0x200u) ? count - start : (count - start) >> 3;
            if (!(elapsed < ticks)) return -2;
        } while (!(m.r32(istat) & kIrqPad));
    }
    if (m.r8(port + kPortType) != 8 && m.r32(kXferState) == 2) {
        pad_wait_set(0x3C);
        while (!pad_wait_over()) {
        }
    }
    m.w8(m.r32(kSioPtr) + kSioData, send);
    if (m.r32(kXferState) == 3 && got == 0x80) {
        const u32 ack = m.r32(kIStatPtr);
        sio = m.r32(kSioPtr);
        m.w32(ack, ~kIrqPad);
        m.w16(sio + kSioCtrl, m.r16(sio + kSioCtrl) | 0x10u);
    }
    const u32 sent = m.r8(port + kPortTxCount), at = m.r8(port + kPortRxCount);
    m.w8(port + kPortTxCount, sent + 1);
    if (at != 0xFF) m.w8(m.r32(port + kPortRx) + m.r8(port + kPortRxCount), got);
    m.w8(port + kPortRxCount, m.r8(port + kPortRxCount) + 1);
    return static_cast<s32>(got);
}

s32 Game::pad_xfer_start(u32 port) {
    cover(0x8009C6E0u);
    Frame f(*this, 0x18);
    const u32 requested = lib_call(m.r32(kRequestFn), port);
    const u32 rx = m.r32(port + kPortRx);
    m.w32(kRequested, requested);
    m.w8(rx, 0);
    return pad_sio_next(port, 0xFFFFFFFEu);
}

s32 Game::pad_xfer_command(u32 port) {
    cover(0x8009C728u);
    Frame f(*this, 0x18);
    if (m.r32(kCurPort) == m.r32(kFirstPort) && m.r32(kHooksOn)) {
        lib_call(m.r32(kHookB), port);
        lib_call(m.r32(kHookA), port);
    }
    if (m.r32(kRequested)) {
        lib_call(m.r32(kRequestFn), m.r32(port + kPortSubPorts));
        lib_call(m.r32(kRequestFn), m.r32(port + kPortSubPorts) + kPortSize);
    }
    const u32 command = m.r8(port + kPortCommand) ? m.r8(port + kPortCommand) : 0x42u;
    return pad_sio_first(port, command);
}

s32 Game::pad_xfer_id(u32 port) {
    cover(0x8009C800u);
    Frame f(*this, 0x18);
    if (m.r32(kRequested)) {
        lib_call(m.r32(kRequestFn), m.r32(port + kPortSubPorts) + 2 * kPortSize);
        lib_call(m.r32(kRequestFn), m.r32(port + kPortSubPorts) + 3 * kPortSize);
    }
    const u32 send = m.r8(port + kPortCommand) ? 0u : m.r32(kMultitap);
    const s32 got = pad_sio_first(port, send);
    if (got < 0) return got;
    const u32 bytes = (static_cast<u32>(got) & 0xFu) << 1;
    m.w32(kBytesLeft, bytes);
    if (!bytes) m.w32(kBytesLeft, 0x20);
    return 0;
}

s32 Game::pad_xfer_ready(u32 port) {
    cover(0x8009C8ACu);
    Frame f(*this, 0x18);
    u32 tap = 0;
    if (m.r32(kMultitap) && (m.r8(m.r32(port + kPortRx)) >> 4) == 8) tap = m.r8(port + kPortCommand) == 0 ? 1u : 0u;
    m.w32(kTapPoll, tap);
    if (!tap) lib_call(m.r32(kActPrepareFn), port);
    const u32 send = lib_call(m.r32(kTxByteFn), port, m.r32(kTapPoll));
    const s32 got = pad_sio_first(port, send & 0xFFu);
    if (got == 0x5A || got == 0) return got;
    return got >= 0 ? -4 : got;
}

s32 Game::pad_xfer_data(u32 port) {
    cover(0x8009C970u);
    Frame f(*this, 0x30);
    if (m.r32(kTapPoll)) lib_call(m.r32(kActPrepareFn), port);
    const u32 tap = m.r32(kTapPoll);
    if (tap) {
        for (s32 k = -1; k < 4; ++k) {
            const s32 left = m.r32s(kBytesLeft) - 1;
            m.w32(kBytesLeft, static_cast<u32>(left));
            if (left <= 0) break;
            if (k >= 0) lib_call(m.r32(kActPrepareFn), m.r32(port + kPortSubPorts) + kPortSize * static_cast<u32>(k));
            const u32 send = lib_call(m.r32(kTxByteFn), port, 1);
            const s32 got = pad_sio_first(port, send & 0xFFu);
            if (got < 0) return got;
            pad_wait_set(0x3C);
            if (!kClrIntSio0) return -3;
        }
    }
    u32 sub = 0;
    if (m.r32s(kBytesLeft) >= 2) {
        const u32 other = m.r32(kCurPort) == 0 ? 1u : 0u;
        const u32 slot = kTapSlots + 4 * other;
        do {
            const s32 n = m.r32s(slot);
            if (n < 0) break;
            if (n > 0) {
                sub = m.r32(m.r32(kPortArray) + kPortSize * other + kPortSubPorts) + kPortSize * static_cast<u32>(n) -
                      kPortSize;
                lib_call(m.r32(kResultFn), sub);
            }
            const s32 state = m.r32s(slot);
            if (state == 3) {
                lib_call(m.r32(kResultFn), sub - kPortSize);
                m.w32(slot, 1);
            } else if (state == 4) {
                m.w32(slot, 3);
            } else if (state == 0 || state == 1) {
                sub = m.r32(kPortArray) + kPortSize * other;
                lib_call(m.r32(kResultFn), sub);
                lib_call(m.r32(kDoneFn), sub);
                m.w32(slot, 0xFFFFFFFFu);
            }
            const u32 send = lib_call(m.r32(kTxByteFn), port, tap);
            const s32 got = pad_sio_next(port, send & 0xFFu);
            if (got < 0) return got;
            pad_wait_set(0x3C);
            if (!kClrIntSio0) return -3;
            m.w32(kBytesLeft, m.r32(kBytesLeft) - 1);
        } while (m.r32s(kBytesLeft) >= 2);
    }
    for (;;) {
        const s32 left = m.r32s(kBytesLeft) - 1;
        m.w32(kBytesLeft, static_cast<u32>(left));
        if (left <= 0) break;
        const u32 send = lib_call(m.r32(kTxByteFn), port, tap);
        const s32 got = pad_sio_next(port, send & 0xFFu);
        if (got < 0) return got;
        if (m.r16(m.r32(kSio2Ptr) + kSioBaud) != 0x22) {
            pad_wait_set(0x3C);
            if (!kClrIntSio0) return -3;
        }
    }
    const u32 at = m.r8(port + kPortRxCount);
    m.w8(port + kPortRxCount, at + 1);
    const u32 sio = m.r32(kSio2Ptr);
    m.w8(m.r32(port + kPortRx) + at, m.r8(sio + kSioData));
    lib_call(m.r32(kEndFn), 0);
    return 0;
}

void Game::pad_xfer_step(u32 port) {
    cover(0x8009C0A4u);
    Frame f(*this, 0x18);
    const u32 state = m.r32(kXferState);
    const u32 handler = m.r32(kXferFns + 4 * state);
    m.w32(kXferState, state + 1);
    const s32 result = static_cast<s32>(lib_call(handler, port));
    if (result < 0) {
        lib_call(m.r32(kEndFn), static_cast<u32>(result));
        return;
    }
    s32 next = m.r32s(kXferState);
    if (next != 0) {
        if (!(next == 3 && m.r8(m.r32(port + kPortRx)) == 0x80)) {
            pad_wait_set(0x3C);
            if (!kClrIntSio0) lib_call(m.r32(kEndFn), 0xFFFFFFFDu);
        }
        next = m.r32s(kXferState);
    }
    if (next >= 5) m.w32(kXferState, static_cast<u32>(next - 1));
}

void Game::pad_port_reset(u32 port) {
    cover(0x8009DF30u);
    if (!m.r8(port + kPortReset)) return;
    m.w8(port + kPortReset, 0);
    m.w8(port + kPortState, 0);
    m.w16(port + kPortStable, 0);
    m.w32(port + kPortRequest, 0);
    m.w32(port + kPortResult, 0);
    m.w8(port + kPortModeCount, 0);
    m.w8(port + kPortCurMode, 0);
    m.w16(port + kPortStable, 0);
    m.w8(port + kPortActCount, 0);
    m.w8(port + kPortCombCount, 0);
    m.w32(port + kPortModes, 0);
    m.w32(port + kPortActs, 0);
    m.w32(port + kPortCombs, 0);
    for (u32 i = 0; i < 6; ++i) m.w8(port + kPortIds + i, 0xFF);
}

u32 Game::pad_command_done(u32 port) {
    cover(0x8009E08Cu);
    const u32 command = m.r8(port + kPortCommand);
    m.w8(port + kPortCommand, 0);
    m.w8(port + kPortLastCommand, command);
    return command;
}

u32 Game::pad_xfer_end(u32 code) {
    cover(0x8009DF98u);
    Frame f(*this, 0x28);
    for (;;) {
        u32 cur = m.r32(kCurPort);
        if (code != 0xFFFFFFF7u) {
            if (code == 0) m.w32(kTapSlots + 4 * cur, 0);
            else pad_command_done(kPadPorts + kPortSize * cur);
        }
        const u32 sio = m.r32(kSio3Ptr);
        cur = m.r32(kCurPort);
        m.w32(kXferState, 0);
        m.w16(sio + kSioCtrl, 0);
        const s32 last = m.r32s(kLastPort);
        m.w32(kCurPort, cur + 1);
        code = 0xFFFF;
        if (last < static_cast<s32>(cur + 1) || kInitSioMode) return 1;
    }
}

u32 Game::pad_tx_byte(u32 port) {
    cover(0x8009E09Cu);
    const s32 n = static_cast<s32>(m.r8(port + kPortTxCount)) - 3;
    const u32 command = m.r8(port + kPortCommand);
    if (command == 0) {
        if (n < 6 && !m.r8(port + kPortActSends + static_cast<u32>(n))) return 0;
        if (!(n < static_cast<s32>(m.r8(port + kPortActLen)))) return 0;
        return m.r8(m.r32(port + kPortActData) + static_cast<u32>(n));
    }
    if (!(n < static_cast<s32>(m.r8(port + kPortSendLen)))) return command == 0x4D ? 0xFFu : 0u;
    return m.r8(m.r32(port + kPortSendData) + static_cast<u32>(n));
}

void Game::pad_act_prepare(u32 port) {
    cover(0x8009E158u);
    Frame f(*this, 0x18);
    m.fill(port + kPortActSends, 0, 6);
    if (m.r16(port + kPortStable) && m.r32(port + kPortActData)) {
        const u32 n = m.r8(port + kPortActLen) < 7 ? m.r8(port + kPortActLen) : 6u;
        if (!m.r8(port + kPortActCount)) return;
        for (u32 act = 0;; ++act) {
            const u32 entry = m.r32(port + kPortActs) + 5 * act;
            const u32 mask = m.r8(entry + 2) ? 0xFFu : 1u;
            bool wanted = false;
            const u32 data = m.r32(port + kPortActData);
            for (u32 i = 0; i < n; ++i) {
                if (m.r8(port + kPortIds + i) == act && (m.r8(data + i) & mask)) {
                    wanted = true;
                    break;
                }
            }
            if (wanted) {
                const s32 used = m.r32s(kActBudget) + m.r8(m.r32(port + kPortActs) + 5 * act + 3);
                if (used < 0x3D) {
                    m.w32(kActBudget, static_cast<u32>(used));
                    for (u32 i = 0; i < n; ++i)
                        if (m.r8(port + kPortIds + i) == act) m.w8(port + kPortActSends + i, 1);
                }
            }
            if (!(static_cast<s32>(act + 1) < static_cast<s32>(m.r8(port + kPortActCount)))) return;
        }
    }
    const u32 type = m.r8(port + kPortType);
    if ((type - 4 < 2 || type == 7) && !m.r16(port + kPortStable) && m.r8(port + kPortActLen) >= 2) {
        const u32 data = m.r32(port + kPortActData);
        if ((m.r8(data) & 0xC0u) != 0x40u) return;
        if (!(m.r8(data + 1) & 1u)) return;
        if (!(m.r32s(kActBudget) + 10 < 0x3D)) return;
        m.w8(port + kPortActSends + 1, 1);
        m.w8(port + kPortActSends, 1);
        m.w32(kActBudget, m.r32(kActBudget) + 10);
        return;
    }
    if (m.r8(port + kPortType) == 3) {
        m.w8(port + kPortActSends, 1);
        return;
    }
    if (m.r16(port + kPortStable)) return;
    for (s32 i = 5; i >= 0; --i) m.w8(port + kPortActSends + static_cast<u32>(i), 1);
}

u32 Game::pad_port_number(u32 port) {
    cover(0x8009E3D4u);
    for (u32 k = 0; k < 2; ++k)
        if (port == kPadPorts + kPortSize * k) return 0x10 * (k + 1);
    return 0xFF;
}

u32 Game::libpad_port(u32 port) {
    cover(0x8009E40Cu);
    return (port & 0xF0u) ? kPadPorts + kPortSize : kPadPorts;
}

u32 Game::libpad_busy(u32 p) {
    cover(0x8009E8B8u);
    if (!m.r16(p + kPortStable)) return 1;
    return m.r8(p + kPortState) != 0xFFu ? 1u : 0u;
}

u32 Game::pad_direct_request(u32 port) {
    cover(0x8009E464u);
    Frame f(*this, 0x18);
    if (m.r8(m.r32(port + kPortRx)) == 0xF3) {
        if (!m.r8(port + kPortType)) return 0;
        if (m.r8(port + kPortState) == 0xFF) return 0;
        if (m.r8(port + kPortReset) == 2) lib_call(m.r32(kResetFn), port);
    }
    const u32 state = m.r8(port + kPortState);
    if (state == 1 || state == 0 || state == 0xFE || state == 0xFF) return 0;
    if (const u32 request = m.r32(port + kPortRequest)) lib_call(request, port);
    return 0;
}

void Game::pad_direct_result(u32 port) {
    cover(0x8009E574u);
    Frame f(*this, 0x18);
    const u32 id = m.r8(m.r32(port + kPortRx));
    if (!(id & 0xF0u)) {
        m.w8(m.r32(port + kPortRecv), 0xFF);
        m.w8(m.r32(port + kPortRecv) + 1, 0);
        m.w8(port + kPortType, 0);
        m.w8(port + kPortRecvLen, 0);
        lib_call(m.r32(kResetFn), port);
        return;
    }
    const u32 type = id >> 4, old_type = m.r8(port + kPortType);
    m.w8(port + kPortType, type);
    if (type == 0xF) {
        m.w8(port + kPortType, old_type);
    } else {
        m.w8(m.r32(port + kPortRecv), 0);
        m.w8(m.r32(port + kPortRecv) + 1, m.r8(m.r32(port + kPortRx)));
        m.w8(port + kPortRecvLen, m.r8(port + kPortRxCount));
        for (u32 i = 2; static_cast<s32>(i) < static_cast<s32>(m.r8(port + kPortRxCount)); ++i)
            m.w8(m.r32(port + kPortRecv) + i, m.r8(m.r32(port + kPortRx) + i));
    }
    bool reset;
    if (m.r8(m.r32(port + kPortRx) + 1) != 0) reset = m.r8(port + kPortType) != old_type;
    else if (m.r8(port + kPortState) == 1 && !m.r32(port + kPortRequest)) reset = m.r8(port + kPortType) != old_type;
    else if (!m.r8(port + kPortConfig)) reset = true;
    else reset = m.r8(port + kPortType) != old_type;
    if (reset) lib_call(m.r32(kResetFn), port);

    u32 state = m.r8(port + kPortState);
    m.w8(port + kPortFails, 0);
    if (state == 0xFF) return;
    if (state != 0 && !m.r8(port + kPortCommand)) return;
    if (((state - 2) & 0xFFu) < 0xFC && m.r8(m.r32(port + kPortRx)) != 0xF3) {
        lib_call(m.r32(kResetFn), port);
        return;
    }
    state = m.r8(port + kPortState);
    if (state == 1) {
        const u32 s = m.r8(port + kPortState);
        m.w8(port + kPortStep, 0);
        m.w8(port + kPortState, s + 1);
    } else if (state == 0) {
        const u32 s = m.r8(port + kPortState);
        m.w8(port + kPortReset, 1);
        m.w8(port + kPortState, s + 1);
    } else if (state == 0xFE) {
        m.w8(port + kPortState, 0xFF);
    } else {
        const u32 result = m.r32(port + kPortResult);
        const u32 step = result ? lib_call(result, port) : kRecvAtLoadInfo;
        m.w8(port + kPortState, m.r8(port + kPortState) + step);
    }
}

u32 Game::pad_command(u32 fn, u32 port, u32 param) {
    cover(fn);
    u32 command;
    switch (fn) {
    case 0x8009DD44u: command = 0x45; break;
    case 0x8009DDB8u: command = 0x4B; break;
    case 0x8009DD58u: command = 0x4C; break;
    case 0x8009DD78u: command = 0x46; break;
    default: command = 0x47; break;
    }
    m.w8(port + kPortCommand, command);
    if (fn == 0x8009DD44u || fn == 0x8009DDB8u) {
        m.w32(port + kPortSendData, 0);
        m.w8(port + kPortSendLen, 0);
        return command;
    }
    m.w32(port + kPortSendData, port + kPortParam);
    m.w8(port + kPortParam, param);
    m.w8(port + kPortSendLen, 1);
    return 1;
}

void Game::pad_info_request(u32 port) {
    cover(0x8009D5ECu);
    Frame f(*this, 0x18);
    switch (m.r8(port + kPortState)) {
    case 2: pad_command(0x8009DD58u, port, m.r8(port + kPortStep)); break;
    case 3: pad_command(0x8009DD78u, port, m.r8(port + kPortStep)); break;
    case 4:
        if (!m.r8(port + kPortSubStep)) pad_command(0x8009DD98u, port, m.r8(port + kPortStep));
        else pad_command(0x8009DDB8u, port, 0);
        break;
    default: break;
    }
}

u32 Game::pad_info_result(u32 port) {
    cover(0x8009D694u);
    switch (m.r8(port + kPortState)) {
    case 2: {
        const u32 rx = m.r32(port + kPortRx);
        if (m.r8(rx + 2) || m.r8(rx + 3)) return 0;
        m.w16(m.r32(port + kPortModes) + 2 * m.r8(port + kPortStep), m.r8(rx + 5) + (static_cast<u32>(m.r8(rx + 4)) << 8));
        const u32 id = m.r16(m.r32(port + kPortModes) + 2 * m.r8(port + kPortStep));
        if (m.r16(port + kPortCheck) != id) {
            m.w16(port + kPortCheck, id);
            return 0;
        }
        const u32 next = m.r8(port + kPortStep) + 1u, count = m.r8(port + kPortModeCount);
        m.w16(port + kPortCheck, 0);
        m.w8(port + kPortRetry, 0);
        m.w8(port + kPortStep, next);
        if ((next & 0xFFu) < count) return 0;
        m.w8(port + kPortStep, 0);
        return 1;
    }
    case 3: {
        const u32 rx = m.r32(port + kPortRx);
        if (m.r8(rx + 2) || m.r8(rx + 3)) return 0;
        const u32 entry = m.r32(port + kPortActs) + 5 * m.r8(port + kPortStep);
        const bool same = m.r8(entry) == m.r8(rx + 4) && m.r8(entry + 1) == (m.r8(rx + 5) & 0x7Fu) &&
                          m.r8(entry + 2) == m.r8(rx + 6) && m.r8(entry + 3) == m.r8(rx + 7) &&
                          m.r8(entry + 4) == (m.r8(rx + 5) >> 7);
        m.w16(port + kPortCheck, same ? 0u : 0xFFFFu);
        m.w8(entry, m.r8(m.r32(port + kPortRx) + 4));
        m.w8(entry + 1, m.r8(m.r32(port + kPortRx) + 5) & 0x7Fu);
        m.w8(entry + 2, m.r8(m.r32(port + kPortRx) + 6));
        m.w8(entry + 3, m.r8(m.r32(port + kPortRx) + 7));
        m.w8(entry + 4, m.r8(m.r32(port + kPortRx) + 5) >> 7);
        if (m.r16(port + kPortCheck)) return 0;
        const u32 next = m.r8(port + kPortStep) + 1u, count = m.r8(port + kPortActCount);
        m.w8(port + kPortRetry, 0);
        m.w8(port + kPortStep, next);
        if ((next & 0xFFu) < count) return 0;
        m.w8(port + kPortStep, 0);
        m.w8(port + kPortSubStep, 0);
        return 1;
    }
    case 4: {
        const u32 rx = m.r32(port + kPortRx);
        if (m.r8(rx + 2)) {
            m.w8(port + kPortSubStep, 0);
            return 0;
        }
        const u32 list = m.r32(port + kPortCombs) + 8 * m.r8(port + kPortStep);
        u32 src;
        s32 bytes;
        if (!m.r8(port + kPortSubStep)) {
            const u32 length = m.r8(rx + 4);
            m.w8(port + kPortSubStep, length);
            m.w8(list, length);
            src = m.r32(port + kPortRx) + 5;
            u32 data;
            if (!m.r8(port + kPortStep)) data = m.r32(port + kPortCombs) + (static_cast<u32>(m.r8(port + kPortCombCount)) << 3);
            else data = m.r32(list - 4) + ((m.r8(list - 8) + 3u) & 0x1FCu);
            m.w32(list + 4, data);
            m.w32(kCombCursor, data);
            bytes = 3;
        } else {
            src = rx + 3;
            bytes = 5;
        }
        bool ended = false;
        for (; bytes > 0; --bytes) {
            if (!m.r8(port + kPortSubStep)) {
                ended = true;
                break;
            }
            const u32 cursor = m.r32(kCombCursor);
            if (!(cursor < port + kPortTableEnd)) {
                m.w8(port + kPortStep, 0);
                m.w8(port + kPortSubStep, 0);
                return 0;
            }
            if (m.r8(cursor) != m.r8(src)) m.w16(port + kPortCheck, 0xFFFF);
            const u32 at = m.r32(kCombCursor);
            m.w8(at, m.r8(src));
            ++src;
            m.w32(kCombCursor, at + 1);
            m.w8(port + kPortSubStep, m.r8(port + kPortSubStep) - 1u);
        }
        if (!ended && m.r8(port + kPortSubStep)) return 0;
        if (m.r16(port + kPortCheck)) {
            m.w16(port + kPortCheck, 0);
            m.w8(port + kPortSubStep, 0);
            return 0;
        }
        const u32 next = m.r8(port + kPortStep) + 1u, count = m.r8(port + kPortCombCount);
        m.w8(port + kPortStep, next);
        if ((next & 0xFFu) < count) {
            m.w8(port + kPortSubStep, 0);
            m.w8(port + kPortRetry, 0);
            return 0;
        }
        m.w8(port + kPortReset, 6);
        m.w8(port + kPortState, 0xFE);
        m.w8(port + kPortRetry, 0);
        return 0;
    }
    default: return 1;
    }
}

u32 Game::pad_align_queue(u32 p, u32 data) {
    cover(0x8009DA8Cu);
    Frame f(*this, 0x20);
    if (lib_call(m.r32(kBusyFn), p)) return 0;
    m.w8(p + kPortState, 1);
    m.w32(p + kPortRequest, 0x8009DAF4u);
    m.w32(p + kPortAlign, data);
    m.w32(p + kPortResult, 0x8009DB10u);
    return 1;
}

u32 Game::pad_align_request(u32 p) {
    cover(0x8009DAF4u);
    const u32 data = m.r32(p + kPortAlign);
    m.w8(p + kPortCommand, 0x4D);
    m.w8(p + kPortSendLen, 6);
    m.w32(p + kPortSendData, data);
    return 6;
}

u32 Game::pad_align_result(u32 p) {
    cover(0x8009DB10u);
    if (m.r8(p + kPortActCount)) {
        for (u32 act = 0;; ++act) {
            u32 data = m.r32(p + kPortAlign);
            s32 given = 0;
            for (u32 i = 0; i < 6; ++i)
                if (m.r8(data + i) == act) ++given;
            s32 size = m.r8(m.r32(p + kPortActs) + 5 * act + 2);
            data = m.r32(p + kPortAlign);
            if (!size) size = 1;
            for (u32 i = 0; i < 6; ++i) {
                if (m.r8(data + i) != act) continue;
                if (given < size) {
                    m.w8(p + kPortIds + i, 0xFF);
                    --given;
                } else {
                    m.w8(p + kPortIds + i, act);
                }
            }
            if (!(static_cast<s32>(act + 1) < static_cast<s32>(m.r8(p + kPortActCount)))) break;
        }
    }
    m.w8(p + kPortState, 0xFE);
    return 0;
}

u32 Game::pad_mode_queue(u32 p, u32 mode, u32 lock) {
    cover(0x8009DBD8u);
    Frame f(*this, 0x28);
    if (lib_call(m.r32(kBusyFn), p)) return 0;
    const u32 current = m.r8(p + kPortCurMode);
    m.w8(p + kPortState, 1);
    m.w32(p + kPortRequest, 0x8009DC70u);
    m.w32(p + kPortResult, 0x8009DCC4u);
    m.w8(p + kPortMode, mode);
    m.w8(p + kPortLock, lock);
    m.w8(p + kPortSameMode, (mode & 0xFFu) == current ? 1u : 0u);
    return 1;
}

u32 Game::pad_mode_request(u32 p) {
    cover(0x8009DC70u);
    const u32 step = m.r8(p + kPortState);
    if (step == 2) {
        m.w8(p + kPortCommand, 0x44);
        m.w32(p + kPortSendData, p + kPortMode);
        m.w8(p + kPortSendLen, 2);
        return p + kPortMode;
    }
    if (step == 3) {
        m.w8(p + kPortCommand, 0x4D);
        m.w32(p + kPortSendData, p + kPortIds);
        m.w8(p + kPortSendLen, 6);
        return 6;
    }
    return 0x4D;
}

u32 Game::pad_mode_result(u32 p) {
    cover(0x8009DCC4u);
    Frame f(*this, 0x18);
    if (m.r8(p + kPortSameMode)) {
        if (m.r8(p + kPortState) == 2) return 1;
        m.w8(p + kPortState, 0xFE);
        return 0;
    }
    lib_call(m.r32(kResetFn), p);
    return 0;
}

void Game::pad_act_set(u32 p, u32 data, u32 len) {
    cover(0x8009D1D0u);
    m.w32(p + kPortActData, data);
    m.w8(p + kPortActLen, len);
}

u32 Game::libpad_set_act_align(u32 port, u32 data) {
    cover(0x8009D0FCu);
    Frame f(*this, 0x18);
    return pad_align_queue(lib_call(m.r32(kPortLookupFn), port, data), data);
}

u32 Game::libpad_set_main_mode(u32 port, u32 mode, u32 lock) {
    cover(0x8009D134u);
    Frame f(*this, 0x20);
    return pad_mode_queue(lib_call(m.r32(kPortLookupFn), port, mode, lock), mode & 0xFFu, lock & 0xFFu);
}

void Game::libpad_set_act(u32 port, u32 data, u32 len) {
    cover(0x8009D17Cu);
    Frame f(*this, 0x20);
    pad_act_set(lib_call(m.r32(kPortLookupFn), port, data, len), data, len);
}
}

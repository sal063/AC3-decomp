#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kPadPorts = 0x800C7CB8u;
constexpr u32 kPortSize = 240u;
constexpr u32 kCmdBufA = 0x800C7C28u;
constexpr u32 kCmdBufB = 0x800C7C70u;
constexpr u32 kCmdBufSize = 35u;

constexpr u32 kHandlers = 0x800B4684u;
constexpr u32 kDirectCmds = 0x800B469Cu;
constexpr u32 kIdleHandler = 0x800B46A8u;
constexpr u32 kPortArray = 0x800B46B8u;
constexpr u32 kReady = 0x800B46BCu;
constexpr u32 kMultitap = 0x800B46D0u;
constexpr u32 kVsyncParam = 0x800C7BF8u;

constexpr u32 kPortZero = 12, kPortSelf = 16, kPortRecv = 48, kPortCmdA = 60, kPortCmdB = 64;
constexpr u32 kPortIds = 93, kPortIdCount = 6;

constexpr u32 kHandlerFns[6] = {0x8009DF98u, 0x8009DF30u, 0x8009E09Cu, 0x8009E158u, 0x8009E3D4u,
                                0x8009E40Cu};
constexpr u32 kIdleFn = 0x8009E08Cu;
constexpr u32 kDirectFns[3] = {0x8009E464u, 0x8009E8B8u, 0x8009E574u};
constexpr u32 kVsyncFns[2] = {0x8009BABCu, 0x8009BA54u};
constexpr u32 kBzeroSize = 2 * kPortSize;
}

u32 Game::pad_set_vsync_param() {
    cover(0x8009BA28u);
    m.w32(kVsyncParam + 4, kVsyncFns[0]);
    m.w32(kVsyncParam + 8, kVsyncFns[1]);
    m.w32(kVsyncParam, 0);
    m.w32(kVsyncParam + 12, 0);
    return kVsyncParam + 4;
}

u32 Game::pad_direct_handlers() {
    cover(0x8009E430u);
    for (u32 k = 0; k < 3; ++k) m.w32(kDirectCmds + 4 * k, kDirectFns[k]);
    return kDirectFns[2];
}

u32 Game::pad_init_direct(u32 buf0, u32 buf1) {
    cover(0x8009DDD0u);
    Frame f(*this, 0x20);
    m.w32(kReady, 0);
    m.w32(kMultitap, 0);
    pad_direct_handlers();
    for (u32 k = 0; k < 6; ++k) m.w32(kHandlers + 4 * k, kHandlerFns[k]);
    m.w32(kPortArray, kPadPorts);
    m.w32(kIdleHandler, kIdleFn);
    m.fill(kPadPorts, 0, kBzeroSize);
    m.w32(kPadPorts + kPortRecv, buf0);
    m.w32(kPadPorts + kPortSize + kPortRecv, buf1);
    for (u32 k = 0; k < 2; ++k) {
        const u32 port = kPadPorts + kPortSize * k;
        m.w32(port + kPortZero, 0);
        m.w32(port + kPortSelf, port);
        m.w8(m.r32(port + kPortRecv), 0xFF);
        m.w8(m.r32(port + kPortRecv) + 1, 0);
        m.w32(port + kPortCmdA, kCmdBufA + kCmdBufSize * k);
        m.w32(port + kPortCmdB, kCmdBufB + kCmdBufSize * k);
        for (u32 i = 0; i < kPortIdCount; ++i) m.w8(port + kPortIds + i, 0xFF);
    }
    pad_set_vsync_param();
    m.w32(kReady, 1);
    return 1;
}

u32 Game::pad_chk_vsync_api() {
    cover(0x8009CD10u);
    Frame f(*this, 0x18);
    return hw->pad_chk_vsync();
}

u32 Game::pad_start_com_api() {
    cover(0x8009CD30u);
    Frame f(*this, 0x18);
    return hw->pad_start_com();
}

u32 Game::pad_stop_com_api() {
    cover(0x8009CD50u);
    Frame f(*this, 0x18);
    return 0;
}
}

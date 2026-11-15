#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kPadInitFlag = 0x800BCB34u;
constexpr u32 kSioRegs = 0x800BCB38u;
constexpr u32 kIntrRegs = 0x800BCB3Cu;
constexpr u32 kPadHook = 0x800CBFB8u;
constexpr u32 kPadHandlerFn = 0x800AFAFCu, kPadVerifierFn = 0x800AFB64u;
constexpr u32 kSioCtrl = 0xAu;

constexpr u32 kDeviceTable = 0x150u;
constexpr u32 kDeviceSize = 80u;
constexpr u32 kDeviceFirstFile = 0x34u;
constexpr u32 kPatchedFirstFile = 0x800CBF88u;
constexpr u32 kPatchedDevice = 0x800CBF90u;
}

u32 Game::api_remove96_entry() {
    cover(0x800AF2F8u);
    return 0;
}

u32 Game::api_firstfile_patched(u32 path, u32 entry, u32 a2) {
    cover(0x800AF794u);
    Frame f(*this, 0x30);
    if (!m.r32(path)) m.w32(path, 1);
    const u32 bytes = m.r32(kDeviceTable + 4);
    const u32 table = m.r32(kDeviceTable);
    const u32 replacement = m.r32(kPatchedFirstFile);
    const u32 end = table + (bytes / kDeviceSize) * kDeviceSize;
    for (u32 dev = table; dev < end; dev += kDeviceSize) {
        const u32 name = m.r32(dev);
        if (name && libc_strcmp(name, kPatchedDevice) == 0) {
            m.w32(dev + kDeviceFirstFile, replacement);
            break;
        }
    }
    return lib_call(m.r32(kPatchedFirstFile), path, entry, a2);
}

void Game::api_set_init_pad_flag(u32 v) {
    cover(0x800AF8A8u);
    m.w32(kPadInitFlag, v);
}

u32 Game::api_read_init_pad_flag() {
    cover(0x800AF8B4u);
    return m.r32(kPadInitFlag);
}

u32 Game::api_pad_hook_install() {
    cover(0x800AFA4Cu);
    Frame f(*this, 0x18);
    hw->enter_critical_section();
    m.w32(kPadHook + 4, kPadHandlerFn);
    m.w32(kPadHook + 8, kPadVerifierFn);
    m.w32(kPadHook, 0);
    m.w32(kPadHook + 12, 0);
    hw->exit_critical_section();
    return 1;
}

u32 Game::api_pad_hook_remove() {
    cover(0x800AFAC4u);
    Frame f(*this, 0x18);
    hw->enter_critical_section();
    hw->exit_critical_section();
    return 1;
}

u32 Game::api_pad_init(u32 fn) {
    cover(fn);
    Frame f(*this, 0x28);
    hw->enter_critical_section();
    hw->exit_critical_section();
    api_pad_hook_install();
    m.w32(kPadInitFlag, 1);
    return 1;
}

u32 Game::api_start_pad() {
    cover(0x800AF9E4u);
    Frame f(*this, 0x18);
    return 1;
}

u32 Game::api_stop_pad() {
    cover(0x800AFA14u);
    Frame f(*this, 0x18);
    const u32 v = api_pad_hook_remove();
    m.w32(kPadInitFlag, 0);
    return v;
}

u32 Game::api_pad_intr() {
    cover(0x800AFAFCu);
    Frame f(*this, 0x10);
    m.w16(m.r32(kSioRegs) + kSioCtrl, 0);
    const u32 count = f.at(0);
    m.w32(count, 10);
    do {
        m.w32(count, m.r32(count) - 1);
    } while (m.r32(count) != 0xFFFFFFFFu);
    return 0;
}

u32 Game::api_pad_intr_pending() {
    cover(0x800AFB64u);
    const u32 regs = m.r32(kIntrRegs);
    if (!(m.r32(regs + 4) & 1u)) return 0;
    return (m.r32(regs) & 1u) ? 1u : 0u;
}

u32 Game::api_stub_minus1(u32 p) {
    cover(0x800AFCF8u);
    (void)m.r32(p);
    return 0xFFFFFFFFu;
}
}

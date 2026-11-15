#include "sim/game/game.hpp"

#include <cstring>

namespace ac3::sim {
namespace {
constexpr u32 kBssStart = 0x800BCE40u;
constexpr u32 kBssEnd = 0x800CBFE8u;
constexpr u32 kStackTopWord = 0x800257DCu;
constexpr u32 kCrt0Return = 0x800257D4u;
}

bool Game::load_exe(const std::vector<u8>& exe) {
    if (exe.size() < 0x800 || std::memcmp(exe.data(), "PS-X EXE", 8) != 0) return false;
    u32 t_addr = 0, t_size = 0;
    std::memcpy(&t_addr, exe.data() + 0x18, 4);
    std::memcpy(&t_size, exe.data() + 0x1C, 4);
    if (exe.size() < 0x800u + t_size) return false;
    u8* dst = m.ptr(t_addr, t_size);
    if (!dst) return false;
    std::memcpy(dst, exe.data() + 0x800, t_size);
    return true;
}

u32 Game::boot() {
    cover(0x8002572Cu);
    for (u32 a = kBssStart; a < kBssEnd; a += 4) m.w32(a, 0);
    sp = m.r32(kStackTopWord) | 0x80000000u;
    return main_start(kCrt0Return);
}

u32 Game::main_start(u32 ra) {
    cover(0x800261A8u);
    sp -= 0x18;
    m.w32(sp + 0x10, ra);
    flow_init();
    hw->vsync(0);
    hw->set_disp_mask(1);
    return frame_start();
}

u32 Game::boot_phase() {
    cover(0x80026044u);
    return 0x60000004u;
}
}

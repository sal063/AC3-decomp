#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kScripts8 = 24, kScripts64 = 504, kScripts2000 = 584, kScriptsEnd = 552;
}

void Game::phase_leave_64() {
    cover(0x80031EDCu);
    Frame f(*this, 24);
    view_reset();
}

void Game::phase_enter_128() {
    cover(0x80031EFCu);
    Frame f(*this, 24);
    cd_cancel_streams();
}

void Game::phase_script_8(u32 k) {
    cover(0x80031FA8u);
    Frame f(*this, 24);
    msn_vm_exec(m.r32(m.r32(g::kMissionData) + 4 * k + kScripts8));
}

void Game::phase_script_64(u32 k) {
    cover(0x80031FD8u);
    Frame f(*this, 24);
    msn_vm_exec(m.r32(m.r32(g::kMissionData) + 4 * k + kScripts64));
}

void Game::phase_script_2000(u32 k) {
    cover(0x80032010u);
    Frame f(*this, 24);
    msn_vm_exec(m.r32(m.r32(g::kMissionData) + 4 * k + kScripts2000));
}

void Game::phase_script_end(u32 k) {
    cover(0x80032040u);
    Frame f(*this, 24);
    msn_vm_exec(m.r32(m.r32(g::kMissionData) + 4 * k + kScriptsEnd));
}
}

#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
bool Game::port_call(u32 fn, const u32* r, u32& out) {
    struct RestoreHeading {
        u32& heading;
        u32 saved;
        ~RestoreHeading() { heading = saved; }
    } restore{inherited_restart_heading, inherited_restart_heading};
    inherited_restart_heading = r[19];
    if (port_call_exe_a(fn, r, out)) return true;
    if (port_call_exe_b(fn, r, out)) return true;
    if (port_call_exe_c(fn, r, out)) return true;
    if (port_call_exe_d(fn, r, out)) return true;
    if (port_call_exe_e(fn, r, out)) return true;
    if (port_call_lib(fn, r, out)) return true;
    if (port_call_o2_a(fn, r, out)) return true;
    if (port_call_o2_b(fn, r, out)) return true;
    if (port_call_o2_c(fn, r, out)) return true;
    if (port_call_o134(fn, r, out)) return true;
    if (port_call_o4(fn, r, out)) return true;
    return false;
}

bool Game::port_phase(u32 fn, bool run) {
    if (port_phase_exe_a(fn, run)) return true;
    if (port_phase_exe_b(fn, run)) return true;
    if (port_phase_exe_c(fn, run)) return true;
    if (port_phase_exe_d(fn, run)) return true;
    if (port_phase_exe_e(fn, run)) return true;
    if (port_phase_lib(fn, run)) return true;
    if (port_phase_o2_a(fn, run)) return true;
    if (port_phase_o2_b(fn, run)) return true;
    if (port_phase_o2_c(fn, run)) return true;
    if (port_phase_o134(fn, run)) return true;
    if (port_phase_o4(fn, run)) return true;
    return false;
}
}

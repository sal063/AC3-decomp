#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
bool Game::port_call_o4(u32 fn, const u32* r, u32& out) {
    if (m.r32(m.r32(g::kScene) + 56) != 4) return false;
    const u32 a0 = r[4], a1 = r[5], a2 = r[6], a3 = r[7];
    auto arg = [&](u32 k) { return m.r32(r[29] + 0x10 + 4 * (k - 4)); };
    auto s = [](u32 v) { return static_cast<s32>(v); };
    switch (fn) {
    case 0x800CD734u: out = o4_music_init(); return true;
    case 0x800CD780u: out = o4_music_update(); return true;
    case 0x800CDEFCu: out = o4_text_clear(); return true;
    case 0x800CDF38u: out = o4_text_scroll(); return true;
    case 0x800CDF88u: out = o4_text_newline(); return true;
    case 0x800CDFDCu: out = o4_text_return(); return true;
    case 0x800CDFECu: out = o4_cursor_back(); return true;
    case 0x800CE040u: out = o4_text_backspace(); return true;
    case 0x800CE070u: out = o4_text_locate(s(a0), s(a1)); return true;
    case 0x800CE0C0u: out = o4_text_control(a0); return true;
    case 0x800CE154u: out = o4_put_char(a0); return true;
    case 0x800CE218u: out = o4_print(a0, r[2]); return true;
    case 0x800CE2E4u: out = o4_purge_line(a0); return true;
    case 0x800CE498u: out = o4_name_char(a0); return true;
    case 0x800CE554u: out = o4_draw_text(a0, a1, s(a2), s(a3), arg(4)); return true;
    case 0x800CE808u: out = o4_script_step(a0, a1); return true;
    case 0x800CE9B4u: o4_picture_keep(); out = 0; return true;
    case 0x800CE9F4u: out = o4_opening(a0, a1); return true;
    case 0x800CED74u: out = o4_phase0(); return true;
    case 0x800CEEFCu: out = o4_phase1(); return true;
    default: return false;
    }
}

bool Game::port_phase_o4(u32 fn, bool run) {
    if (m.r32(m.r32(g::kScene) + 56) != 4) return false;
    switch (fn) {
    case 0x800CED74u:
        if (run) phase_result = o4_phase0();
        return true;
    case 0x800CEEFCu:
        if (run) phase_result = o4_phase1();
        return true;
    default: return false;
    }
}
}

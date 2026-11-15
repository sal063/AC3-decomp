#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kScene = 0x800BCE60u;
constexpr u32 kWorld = 0x800BCE54u;
constexpr u32 kPlayerWeapons = 0x800BD098u;
}

void Game::tick_targeting() {
    const u32 st = m.r32(kPlayerWeapons);
    m.w32(sp + 0x10, m.r32(st + 0x1C));
    m.w32(sp + 0x14, m.r32(st + 0x08));
    m.w32(sp + 0x18, m.r32(st + 0x0C));
    m.w32(sp + 0x1C, m.r32(st + 0x10));
    targeting_update(sp + 0x10);
}

void Game::tick_record() {
    const u32 subject = m.r32(sp + 0x24);
    if (m.r32(m.r32(kPlayerWeapons) + 0x18) != subject && (m.r32(m.r32(kWorld)) & 2u))
        record_view_subject(static_cast<s32>(subject));
    m.w32(m.r32(m.r32(kWorld) + 0x28) + 0xBC, m.r32(sp + 0x20));
    m.w32(m.r32(kPlayerWeapons) + 0x18, m.r32(sp + 0x24));
    if (m.r32(m.r32(kWorld)) & 2u) record_tick();
}

bool Game::part_ported(int part) {
    switch (part) {
    case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10: case 11: case 12: case 13:
    case 14: case 15: case 17: case 18: case 19: return true;
    default: return false;
    }
}

bool Game::part_ported(int part, u32 target) {
    if (!part_ported(part)) return false;
    switch (part) {
    case 13: case 15: return phase_ported(target);
    case 17: return tick_ported(target);
    default: return true;
    }
}

bool Game::run_part(int part) {
    if (!part_ported(part)) return false;
    switch (part) {
    case 1:
        m.w32(sp + 0x28, reg_s0);
        m.w32(sp + 0x2C, reg_ra);
        sim_step1();
        break;
    case 2: sim_step2(); break;
    case 3: sim_step3(); break;
    case 4: sim_step4(); break;
    case 5: sim_step5(); break;
    case 6: sim_step6(); break;
    case 7: sim_step7(); break;
    case 8: sim_step8(); break;
    case 9: tick_targeting(); break;
    case 10: tick_record(); break;
    case 11: flight_frame_view(); break;
    case 12: draw_passes(); break;
    case 13: phase_leave(part_target); break;
    case 14:
        main_loop(phase_result);
        break;
    case 15: phase_enter(part_target); break;
    case 17: run_tick(part_target); break;
    case 18: sp += 24; break;
    case 19: break;
    default: return false;
    }
    return true;
}

bool Game::phase_ported(u32 fn) {
    switch (fn) {
    case 0x80026044u:
    case 0x8009B6B8u:
    case 0x8009B7CCu:
    case 0x80032754u:
    case 0x800357C0u:
    case 0x800358F8u:
    case 0x80035A38u:
    case 0x80035F18u:
    case 0x80035F98u:
    case 0x80033EF0u:
    case 0x80034040u:
    case 0x800342ECu:
    case 0x80057CD0u:
    case 0x80057E34u:
    case 0x80057EBCu:
    case 0x800581C4u: return true;
    default: return false;
    }
}

bool Game::tick_ported(u32 fn) {
    switch (fn) {
    case 0x80032F3Cu:
    case 0x800330CCu:
    case 0x8003329Cu: return true;
    default: return false;
    }
}

bool Game::overlay_phase_ported(u32 overlay, u32 fn) {
    switch (overlay) {
    case 1:
        switch (fn) {
        case 0x800CCE98u: case 0x800CD07Cu: case 0x800CC5D8u: case 0x800CC764u: return true;
        default: return false;
        }
    case 2:
        switch (fn) {
        case 0x800F60F0u: case 0x800F6848u: case 0x800FBC48u: case 0x800FBD74u: case 0x800FBF70u:
        case 0x800FD634u: case 0x800FD698u: case 0x800CDCD4u: case 0x800CDF40u: return true;
        default: return false;
        }
    case 4:
        return fn == 0x800CED74u || fn == 0x800CEEFCu;
    default: return false;
    }
}

bool Game::runs_phase(u32 fn) {
    return phase_ported(fn) || overlay_phase_ported(m.r32(m.r32(kScene) + 56), fn) || port_phase(fn, false);
}

bool Game::run_phase(u32 fn) {
    if (port_phase(fn, false)) {
        part_target = fn;
        port_phase(fn, true);
        return true;
    }
    if (!runs_phase(fn)) {
        u32 out = 0;
        if (!call_address(fn, 0, 0, 0, 0, out)) return false;
        part_target = fn;
        phase_result = out;
        return true;
    }
    part_target = fn;
    if (!phase_ported(fn)) {
        const u32 ovl = m.r32(m.r32(kScene) + 56);
        if (ovl == 1) phase_result = run_title_phase(fn);
        else if (ovl == 2) phase_result = run_o2_phase(fn);
        return true;
    }
    if (is_loader(fn)) run_loader(fn);
    else if (fn == 0x80026044u) phase_result = boot_phase();
    else if (fn == 0x80032754u) phase_result = mission_init();
    else if (fn == 0x800357C0u) phase_result = loading_frame();
    else if (fn == 0x800358F8u) phase_result = request_mission();
    else if (fn == 0x80035A38u) phase_result = unpack_mission();
    else if (fn == 0x80035F18u) phase_result = request_briefing();
    else if (fn == 0x80035F98u) phase_result = unpack_briefing();
    else if (fn == 0x80057CD0u || fn == 0x80057E34u || fn == 0x80057EBCu || fn == 0x800581C4u)
        phase_result = run_gmv_phase(fn);
    else phase_result = flight_phase_frame(fn);
    return true;
}

void Game::sim_tick_p7() {
    Frame frame(*this, 0x30);
    if (m.r32(m.r32(kScene)) & 0x20000000u) {
        m.w32(sp + 0x28, reg_s0);
        m.w32(sp + 0x2C, reg_ra);
        return;
    }
    for (int k = 1; k <= 10; ++k) run_part(k);
}
}

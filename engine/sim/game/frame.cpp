#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kLibgpuTail = 0x800BBA24u;
constexpr u32 kLibgpuTailNext = 0x800BBA10u;
constexpr u32 kOtTerm = 0x00FFFFFFu;

struct FlightPhase {
    u32 fn, tick, ret;
};
constexpr FlightPhase kFlightPhases[] = {
    {0x80033EF0u, 0x80032F3Cu, 0x80033F14u},
    {0x80034000u, 0x8003307Cu, 0x80034028u},
    {0x80034040u, 0x800330CCu, 0x80034068u},
    {0x80034160u, 0x80033104u, 0x80034188u},
    {0x800342ACu, 0x800331E0u, 0x800342D4u},
    {0x800342ECu, 0x8003329Cu, 0x8003432Cu},
};

const FlightPhase* flight_phase(u32 fn) {
    for (const FlightPhase& p : kFlightPhases)
        if (p.fn == fn) return &p;
    return nullptr;
}
}

bool Game::is_flight_phase(u32 fn) { return flight_phase(fn) != nullptr; }

bool Game::is_flight_tick(u32 fn) {
    for (const FlightPhase& p : kFlightPhases)
        if (p.tick == fn) return true;
    return false;
}

void Game::clear_otag_r(u32 ot, u32 n) {
    for (u32 i = 0; i < n; ++i) m.w32(ot + 4 * i, i ? ((ot + 4 * (i - 1)) & 0x00FFFFFFu) : kOtTerm);
    if (!n) return;
    m.w32(kLibgpuTail, (4u << 24) | (kLibgpuTailNext & 0x00FFFFFFu));
    m.w32(kLibgpuTailNext, (4u << 24) | kOtTerm);
    m.w32(ot, kLibgpuTail & 0x00FFFFFFu);
}

void Game::flight_frame_head(u32 ret) {
    const u32 world = m.r32(g::kWorld), scene = m.r32(g::kScene);
    sp -= 32;
    m.w32(sp + 16, 0);
    m.w32(sp + 28, ret);
    m.w32(sp + 24, 0);
    m.w32(sp + 20, 0);
    const u32 sel = world + 4 * m.r32(scene + 4);
    const u32 ot = m.r32(sel + 24);
    m.w32(world + 16, ot);
    m.w32(world + 20, m.r32(sel + 32));
    clear_otag_r(ot, 4096);
    const u32 w = m.r32(g::kWorld), link = m.r32(scene + 8);
    const u32 first = m.r32(w + 16);
    m.w32(first, (m.r32(first) & 0xFF000000u) | (m.r32(link) & 0x00FFFFFFu));
    m.w32(link, (m.r32(link) & 0xFF000000u) | ((m.r32(w + 16) + 16380) & 0x00FFFFFFu));
    m.w16(w + 652, 0);
    m.w16(w + 654, 0);
    m.w16(w + 656, 0);
    m.w16(w + 658, 0);
}

void Game::phase_enter(u32 fn) {
    const FlightPhase* p = flight_phase(fn);
    const u32 world = m.r32(g::kWorld);
    sp -= 24;
    m.w32(sp + 16, 0x800263A4u);
    if (fn == 0x80033EF0u) {
        m.w32(world, m.r32(world) | 2u);
    } else if (fn == 0x800342ECu) {
        const u32 sc = m.r32(g::kScene);
        m.w32(sc, m.r32(sc) | 0x2000000u);
        m.w32(world, m.r32(world) & ~2u);
        continue_menu_input();
    } else {
        m.w32(world, m.r32(world) & ~2u);
    }
    flight_frame_head(p->ret);
    reg_s0 = p->tick;
    reg_ra = 0x80033A8Cu;
    reg_s4 = 0;
}

void Game::run_tick(u32 fn) {
    switch (fn) {
    case 0x80032F3Cu: sim_tick_p7(); break;
    case 0x800330CCu: sim_tick_p9(); break;
    case 0x800331E0u: sim_tick_p13(); break;
    case 0x8003307Cu: sim_tick_p8(); break;
    case 0x80033104u: sim_tick_p10(); break;
    case 0x8003329Cu: break;
    default: call_by_address(fn, "a flight phase's tick"); break;
    }
}

void Game::flight_frame_view() {
    view_update(m.r32(m.r32(g::kWorld) + 40));
    const u32 w = m.r32(g::kWorld);
    view_matrices(m.r32(w + 40), w + 44, w + 76);
}

void Game::phase_leave(u32 fn) {
    sp += 32;
    switch (fn) {
    case 0x80033EF0u: normal_flight_tail(); break;
    case 0x80034040u: replay_viewer_tail(); break;
    case 0x800342ECu: {
        const u32 w = m.r32(g::kWorld);
        m.w32(w + 20, continue_menu_draw(m.r32(m.r32(g::kScene) + 12), m.r32(w + 20)));
        phase_result = world_frame();
        break;
    }
    case 0x80034000u:
    case 0x800342ACu: phase_result = world_frame(); break;
    case 0x80034160u: phase10_tail(); break;
    default:
        unported(fn, "a flight phase's tail");
        phase_result = world_frame();
        break;
    }
}

void Game::normal_flight_tail() {
    const u32 sc = m.r32(g::kScene);
    const u32 flags = m.r32(sc);
    if (flags & 0x20000000u) {
        pause_menu();
        const u32 w = m.r32(g::kWorld);
        const u32 p = pause_draw(m.r32(m.r32(g::kScene) + 12), m.r32(w + 20));
        m.w32(w + 20, p);
        if (m.r32(m.r32(g::kScene)) & 0x8000000u) cd_pause(0);
    } else if (!(flags & 0x10000000u)) {
        bool pause = false;
        const u32 b = m.r16(sc + 342);
        if (b & 1u) {
            m.w16(sc + 342, b & 0xFFFEu);
            pause = true;
        }
        const u32 sc2 = m.r32(g::kScene);
        if (m.r32(sc2 + 300) & 0x40u) pause = true;
        if (pause) {
            m.w32(sc2, m.r32(sc2) | 0x4000000u);
            cd_pause(1);
        }
    }
    phase_result = world_frame();
}

u32 Game::flight_phase_frame(u32 fn) {
    phase_enter(fn);
    run_tick(flight_phase(fn)->tick);
    flight_frame_view();
    smooth_capture(fn);
    draw_passes();
    phase_leave(fn);
    sp += 24;
    return phase_result;
}
}

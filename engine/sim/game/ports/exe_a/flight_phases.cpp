#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kCamState = 0x800BD098u;
constexpr u32 kRetryText = 0x800235F4u;
}

u32 Game::flow_scene_machine() {
    cover(0x80026350u);
    Frame f(*this, 24);
    const u32 sc = m.r32(g::kScene);
    const bool loading = m.r32(sc + 52) != m.r32(sc + 56);
    const u32 fn = scene_phase_fn();
    if (!run_phase(fn)) unported(fn, "a phase function");
    const u32 r = phase_result;
    if (loading && (r & 0x2000000u)) {
        const u32 s = m.r32(g::kScene);
        m.w32(s + 48, 0);
        m.w32(s + 56, m.r32(s + 52));
    }
    flow_phase_return(r);
    return 0;
}

void Game::sim_tick_p8() {
    cover(0x8003307Cu);
    Frame f(*this, 24);
    scene_tick();
    if (m.r32(m.r32(g::kScene)) & 0x20000000u) return;
    bullets_replay();
    sim_step7();
}

void Game::sim_tick_p10() {
    cover(0x80033104u);
    Frame f(*this, 0x30);
    m.w32(sp + 0x2C, reg_ra);
    m.w32(sp + 0x28, reg_s0);
    mission_end_dispatch(0, 0);
    if (m.r32(m.r32(g::kScene)) & 0x20000000u) return;
    if (m.r32(m.r32(g::kWorld) + 4) & 0x1000u) return;
    const u32 view = m.r32(m.r32(g::kWorld) + 40);
    sim_step1();
    sim_step2();
    sim_step3();
    sim_step4();
    sim_step5();
    sim_step6();
    sim_step7();
    sim_step8();
    m.w32(sp + 0x10, m.r32(m.r32(kCamState) + 0x1C));
    targeting_update(sp + 0x10);
    m.w32(view + 0xBC, m.r32(sp + 0x20));
    m.w32(m.r32(kCamState) + 0x18, m.r32(sp + 0x24));
}

void Game::phase11_frame() {
    cover(0x800332A4u);
    Frame f(*this, 24);
    mission_movie_tick();
}

void Game::phase12_frame() {
    cover(0x800332C4u);
    Frame f(*this, 24);
    movie_mission_draw();
}

u32 Game::phase11() {
    cover(0x8003423Cu);
    Frame f(*this, 24);
    const u32 w = m.r32(g::kWorld);
    m.w32(w, m.r32(w) & ~2u);
    phase11_frame();
    return world_frame();
}
u32 Game::phase12() {
    cover(0x80034274u);
    Frame f(*this, 24);
    const u32 w = m.r32(g::kWorld);
    m.w32(w, m.r32(w) & ~2u);
    phase12_frame();
    return world_frame();
}

void Game::retry_prompt_input() {
    cover(0x8003514Cu);
    const u32 w = m.r32(g::kWorld);
    const u32 fl = m.r32(w);
    m.w32(w, fl | 0x40u);
    if (m.r32(m.r32(g::kScene) + 300) & 0x40u) m.w32(w, fl | 0x60u);
}

u32 Game::retry_prompt_draw(u32 ot, u32 p) {
    cover(0x800353E8u);
    Frame f(*this, 0x20);
    s32 level = m.r16s(g::kSinCos + ((m.r32(m.r32(g::kWorld) + 12) << 9) & 0x7E00u)) >> 2;
    if (level < 0) level = -level;
    const u32 color = static_cast<u32>(level >> 6) << 8;
    m.w32(f.at(0x10), color);
    m.w32(f.at(0x14), kRetryText);
    return text_line(ot, p, 40, 212, color, kRetryText);
}

void Game::phase10_tail() {
    if (m.r32(m.r32(g::kWorld) + 4) & 0x1000u) {
        retry_prompt_input();
        const u32 w = m.r32(g::kWorld);
        const u32 p = retry_prompt_draw(m.r32(m.r32(g::kScene) + 12), m.r32(w + 20));
        const u32 w2 = m.r32(g::kWorld), sc = m.r32(g::kScene);
        m.w32(w2 + 20, p);
        m.w32(sc, m.r32(sc) | 0x2000000u);
        if (m.r32(w2) & 0x20u) {
            m.w32(w2, m.r32(w2) & ~0x20u);
            voices_off();
            rumble_clear();
            request_exit(1024, m.r32(m.r32(g::kWorld) + 8));
            player_activate_parameter();
        }
    }
    phase_result = world_frame();
}
}

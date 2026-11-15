#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kPlayerWeapons = 0x800BD098u;
constexpr u32 kStartKind = 0x800BCE14u;
}

void Game::player_mode_second_stage() {
    cover(0x80073A30u);
    Frame f(*this, 24);
    const u32 p = m.r32(g::kPlayer);
    m.w32(p + 16, 0);
    m.w32(m.r32(kPlayerWeapons) + 36, 0);
    const u32 kind = m.r32(kStartKind);
    switch (m.r32(p + 4)) {
    case 8: player_mode_from_carrier(kind ? 1 : 0, 1); break;
    case 256: player_mode_from_refuel(kind != 256 ? 1 : 0, 1); break;
    case 0x4000: player_mode_from_launch(0, 1); break;
    case 0x8000: player_mode_from_launch(1, 1); break;
    case 0x4000000: player_mode_end(1); break;
    default: break;
    }
}

void Game::player_mode_level() {
    cover(0x80076338u);
    const u32 p = m.r32(g::kPlayer);
    const u32 state = m.r32(p);
    m.w32(p + 4, 0x200000);
    m.w32(p + 8, 0);
    m.w32(p, state | 0x40u);
}

void Game::player_mode_idle() {
    cover(0x80076408u);
    const u32 p = m.r32(g::kPlayer);
    m.w32(p, 32);
    m.w32(p + 4, 0x20000000);
    m.w32(p + 8, 0);
}

void Game::player_mode_first_stage() {
    cover(0x80076F04u);
    Frame f(*this, 24);
    switch (m.r32(kStartKind)) {
    case 0: player_mode_from_carrier(0, 0); break;
    case 1: player_mode_from_carrier(1, 0); break;
    case 2: player_mode_from_refuel(0, 0); break;
    case 3: player_mode_from_refuel(1, 0); break;
    case 4: player_mode_end(0); break;
    case 5: player_mode_from_launch(0, 0); break;
    case 6: player_mode_from_launch(1, 0); break;
    default: break;
    }
}

void Game::view_projection(u32 view, u32 h) {
    cover(0x80077768u);
    Frame f(*this, 32);
    m.w16(view + 36, h);
    set_geom_screen(static_cast<s32>(h));
    depth_cue(m.r16s(view + 40), m.r16s(view + 42), static_cast<s32>(h));
}

bool Game::view_fog(u32 view, s32 near, s32 far, s32 h, u32 table) {
    cover(0x800777B0u);
    Frame f(*this, 40);
    if (far < near) return false;
    if (near < 1000) near = 1000;
    if (near >= 31768) near = 31767;
    if (far < 1000) far = 1000;
    if (far >= 31768) far = 31767;
    fog_table(near, far, table);
    m.w16(view + 40, static_cast<u32>(near));
    m.w16(view + 42, static_cast<u32>(far));
    if (!h) h = m.r16s(view + 36);
    view_projection(view, static_cast<u32>(h));
    return true;
}

void Game::view_fixed() {
    cover(0x800798ECu);
    Frame f(*this, 24);
    view_set(m.r32(m.r32(g::kWorld) + 40), 4096, m.r32(m.r32(g::kSlots)));
    view_flag(0);
}

void Game::view_follow_player_by_option() {
    cover(0x8007953Cu);
    Frame f(*this, 24);
    view_follow_player(m.r8(m.r32(g::kScene) + 927));
}
}

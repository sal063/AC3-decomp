#include "sim/game/game.hpp"

namespace ac3::sim {
void Game::o2_sequence_idle() { cover(0x800F60E8u); }

void Game::o2_primitive_idle() { cover(0x800F6CB4u); }

u32 Game::o2_loader_idle() { cover(0x800FBC40u); return 0; }

void Game::o2_aircraft_material_idle() { cover(0x80102650u); }

void Game::o2_aircraft_script_idle() { cover(0x80105E18u); }

u32 Game::o2_transition_state() {
    cover(0x800FEEE0u);
    constexpr u32 kTransition = 0x8011278Cu;
    return m.r32(m.r32(kTransition));
}

u32 Game::o2_transition_duration(u32 ticks) {
    cover(0x800FEEF8u);
    const u32 transition = m.r32(0x8011278Cu);
    m.w32(transition + 8, ticks);
    return transition;
}

u32 Game::o2_transition_reset() {
    cover(0x800FEF08u);
    const u32 transition = m.r32(0x8011278Cu);
    m.w32(transition + 4, 0);
    m.w32(transition, 0);
    return 1;
}

u32 Game::o2_hangar_effect3_begin() {
    cover(0x80100FACu);
    const u32 effects = m.r32(0x80112DA8u);
    m.w32(effects + 0x1C, 1);
    m.w32(effects + 0x28, 0);
    return 1;
}

u32 Game::o2_hangar_effect3_exit() {
    cover(0x80100FC4u);
    const u32 effects = m.r32(0x80112DA8u);
    m.w32(effects + 0x1C, 3);
    m.w32(effects + 0x28, 0);
    return 3;
}

u32 Game::o2_brief_advance() {
    cover(0x800FE564u);
    const u32 menu = m.r32(0x80112DC4u);
    const u32 next = m.r16(menu + 0x86) + 1u;
    m.w16(menu + 0x86, next);
    return next;
}

u32 Game::o2_text_ordering_table(u32 table) {
    cover(0x800F3EF4u);
    const u32 text = m.r32(0x80112E00u);
    constexpr u32 kOrderingTable = 0xA8DC, kDefaultTable = 0xA8E0;
    m.w32(text + kOrderingTable, table == 0xFFFFFFFFu ? m.r32(text + kDefaultTable) : table);
    return text + 0x8000;
}
u32 Game::o2_text_alternate_palette(u32 enabled) {
    cover(0x800F34F4u);
    const u32 text = m.r32(0x80112E00u);
    constexpr u32 kBlend = 0xA909, kSemi = 0xA908, kClutY = 0xA8FE, kClutX = 0xA8FC;
    if (enabled) {
        m.w8(text + kSemi, 1);
        m.w16(text + kClutY, 21);
        m.w8(m.r32(0x80112E00u) + kBlend, 2);
    } else {
        m.w16(text + kClutY, 19);
        m.w8(text + kSemi, 0);
    }
    const u32 current = m.r32(0x80112E00u), textureY = m.r16(current + 0xA8F6);
    const u32 page = ((m.r8(current + kBlend) & 3u) << 5) | ((textureY & 0x100u) >> 4)
                     | ((m.r16(current + 0xA8F4) & 0x3FFu) >> 6) | ((textureY & 0x200u) << 2);
    const u32 clutX = (m.r16(current + kClutX) >> 4) & 63u;
    m.w16(current + 0xA8EC, page);
    m.w16(current + 0xA8EE, (m.r16(current + kClutY) << 6) | clutX);
    return clutX;
}
}

#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kScreens = 0x80112DC0u, kSelector = 0x80112DD0u;
constexpr u32 kLoadPhase = 454, kEntryTables = 364, kFlags = 92;
u32 menu(Memory& m) { return m.r32(kMenu); }
void ready_when_idle(Memory& m, u32 stage) {
    if (m.r32(m.r32(g::kScene) + 68) != 1) m.w16(menu(m) + kLoadPhase, stage);
}
void metadata_font(Game& game) { game.o2_text_font(6); game.o2_text_rgb(128, 128, 128); }
}

void Game::o2_aircraft_request_ready() {
    cover(0x800CED08u);
    ready_when_idle(m, 3);
}

void Game::o2_selector_completion_clear() {
    cover(0x800CF6B0u);
    m.w8(m.r32(kSelector) + 4, 0);
}

u32 Game::o2_selector_idle(u32 previous) {
    cover(0x800D0828u);
    return previous;
}

u32 Game::o2_frontend_idle(u32 previous) {
    cover(0x800D1410u);
    return previous;
}

void Game::o2_frontend_request_ready() {
    cover(0x800D14A0u);
    ready_when_idle(m, 0);
}

void Game::o2_frontend_secondary_ready() {
    cover(0x800D1558u);
    ready_when_idle(m, 0);
}

u32 Game::o2_selection_bit_index(u32 mask) {
    cover(0x800D1B04u);
    for (u32 bit = 0; bit < 6; ++bit) if (mask & (1u << bit)) return bit;
    return 0xFFFFFFFFu;
}

u32 Game::o2_metadata_idle(u32 previous) {
    cover(0x800D31A8u);
    return previous;
}

void Game::o2_metadata_font_first() {
    cover(0x800D31B0u);
    Frame frame(*this, 24);
    metadata_font(*this);
}

void Game::o2_metadata_font_second() {
    cover(0x800D31E0u);
    Frame frame(*this, 24);
    metadata_font(*this);
}

void Game::o2_metadata_font_third() {
    cover(0x800D3210u);
    Frame frame(*this, 24);
    metadata_font(*this);
}

void Game::o2_metadata_palette_begin() {
    cover(0x800D32DCu);
    Frame frame(*this, 24);
    o2_text_alternate_palette(1);
}

void Game::o2_frontend_reset() {
    cover(0x800DE718u);
    const u32 state = menu(m);
    m.w8(state + 126, 0);
    const u32 current = menu(m);
    m.w32(state + 80, 0); m.w8(current + 125, 1);
}

void Game::o2_frontend_cancel() {
    cover(0x800DE73Cu);
    const u32 scene = m.r32(g::kScene);
    if ((m.r32(scene + 300) & 32) && !(m.r32(scene + 296) & 0x5F)) {
        m.w8(menu(m) + 124, 0); m.w8(menu(m) + 125, 0); m.w8(menu(m) + 126, 4);
    }
}

u32 Game::o2_menu_order_table() {
    cover(0x800DE8F0u);
    return m.r32(menu(m) + 8);
}

void Game::o2_entry_position(u32 group, u32 index, u32 x, u32 y) {
    cover(0x800DED68u);
    Frame frame(*this, 32);
    const u32 signedX = static_cast<u32>(static_cast<s16>(x)), signedY = static_cast<u32>(static_cast<s16>(y));
    m.w32(frame.at(16), signedY); m.w32(frame.at(20), 0);
    o2_entry_set(group, index, 1, signedX, signedY, 0);
}

u32 Game::o2_entry_position_read(u32 group, u32 index, u32 output) {
    cover(0x800DEDA0u);
    const u32 table = menu(m) + kEntryTables + 4 * group;
    m.w16(output, m.r16(m.r32(table) + 12 * index + 4));
    m.w16(output + 2, m.r16(m.r32(table) + 12 * index + 6));
    return 1;
}

void Game::o2_flags_clear() {
    cover(0x800DEFB4u);
    for (u32 flag = 0; flag < 64; ++flag) m.w8(m.r32(m.r32(kScreens) + kFlags) + flag, 0);
}
}

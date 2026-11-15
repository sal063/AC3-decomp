#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kCardState = 0x800DDA48u;
constexpr u32 kResource = 0x800DDA2Cu;
constexpr u32 kCardOperation = 744, kSave = 760;
}

u32 Game::o1_resource_handle() {
    cover(0x800CD968u);
    return m.r32(kResource);
}
u32 Game::o1_card_operation_clear() {
    cover(0x800D4D54u);
    const u32 operation = m.r32(m.r32(kCardState) + kCardOperation);
    m.w8(operation + 8, 0);
    return operation;
}
u32 Game::o1_card_callback_clear() {
    cover(0x800D5870u);
    const u32 cards = m.r32(kCardState);
    m.w32(cards + 40, 0);
    return cards;
}
u32 Game::o1_card_cursor() {
    cover(0x800D5A20u);
    return static_cast<u32>(m.r16s(m.r32(kCardState) + 78));
}
u32 Game::o1_save_pointer_address() {
    cover(0x800D5C74u);
    return 0x800CC504u;
}
u32 Game::o1_save_byte_45c() {
    cover(0x800D5CF4u);
    return m.r32(m.r32(kCardState) + kSave) + 0x45Cu;
}
u32 Game::o1_save_byte_45d() {
    cover(0x800D5D0Cu);
    return m.r32(m.r32(kCardState) + kSave) + 0x45Du;
}
u32 Game::o1_save_byte_45e() {
    cover(0x800D5D24u);
    return m.r32(m.r32(kCardState) + kSave) + 0x45Eu;
}
u32 Game::o1_save_byte_460() {
    cover(0x800D5D3Cu);
    return m.r32(m.r32(kCardState) + kSave) + 0x460u;
}

void Game::o1_empty_5968() { cover(0x800D5968u); }
void Game::o1_empty_8f34() { cover(0x800D8F34u); }
void Game::o1_empty_c424() { cover(0x800DC424u); }
void Game::o1_empty_c42c() { cover(0x800DC42Cu); }
void Game::o1_empty_c5e4() { cover(0x800DC5E4u); }
void Game::o1_empty_c694() { cover(0x800DC694u); }
void Game::o1_empty_c69c() { cover(0x800DC69Cu); }
void Game::o1_empty_c6a4() { cover(0x800DC6A4u); }
void Game::o1_empty_c6ac() { cover(0x800DC6ACu); }
void Game::o1_empty_c6b4() { cover(0x800DC6B4u); }
void Game::o1_empty_c6bc() { cover(0x800DC6BCu); }
}

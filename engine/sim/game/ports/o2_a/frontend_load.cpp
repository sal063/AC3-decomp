#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kLoader = 0x800BCE50u;
constexpr u32 kRecordSlot = 96, kLoadStage = 454, kLoadBuffer = 264;
}

void Game::o2_frontend_record_request(u32 record, bool retainBuffer) {
    const u32 loader = m.r32(kLoader);
    m.w32(loader + kRecordSlot, record);
    const u32 buffer = mem_alloc(records_size(loader + kRecordSlot, 1));
    if (retainBuffer) m.w32(m.r32(kMenu) + kLoadBuffer, buffer);
    m.w32(m.r32(g::kScene) + 68, 1);
    request_records(m.r32(kLoader) + kRecordSlot, 1, buffer, 0x8009B904u);
    const u32 menu = m.r32(kMenu);
    m.w16(menu + kLoadStage, m.r16(menu + kLoadStage) + 1);
}

void Game::o2_frontend_archive_first() {
    cover(0x800CE75Cu);
    Frame frame(*this, 24);
    o2_frontend_record_request(121, true);
}

void Game::o2_frontend_archive_second() {
    cover(0x800CE894u);
    Frame frame(*this, 24);
    o2_frontend_record_request(122, true);
}

void Game::o2_frontend_archive_third() {
    cover(0x800CE9E4u);
    Frame frame(*this, 24);
    o2_frontend_record_request(123, false);
}

void Game::o2_frontend_archive_fourth() {
    cover(0x800D1418u);
    Frame frame(*this, 24);
    o2_frontend_record_request(124, false);
}

void Game::o2_frontend_archive_fifth() {
    cover(0x800D14D0u);
    Frame frame(*this, 24);
    o2_frontend_record_request(125, false);
}

void Game::o2_dialog_load_base() {
    cover(0x800CEAFCu);
    Frame frame(*this, 24);
    m.w32(0x80112DDCu, mem_alloc(324));
    o2_frontend_record_request(115, false);
}

void Game::o2_dialog_load_extra() {
    cover(0x800CEC70u);
    Frame frame(*this, 24);
    m.w32(0x80112DDCu, mem_alloc(324));
    o2_frontend_record_request(125, false);
}
}

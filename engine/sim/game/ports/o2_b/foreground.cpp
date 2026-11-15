#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
void link(Memory& m, u32 packet, u32 ot) {
    m.w32(packet, (m.r32(packet) & 0xFF000000u) | (m.r32(ot) & 0xFFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (packet & 0xFFFFFFu));
}
void offset_packet(Memory& m, u32 packet, s32 x, s32 y) {
    m.w32(packet + 8, 0);
    m.w32(packet + 4, 0xE5000000u | static_cast<u32>(x) | (static_cast<u32>(y) << 11));
}
}
void Game::o2_foreground_draw() {
    cover(0x800E67ACu);
    Frame frame(*this, 40);
    view_update(m.r32(m.r32(g::kWorld) + 40));
    u32 camera = m.r32(g::kWorld);
    transpose_matrix(camera + 44, camera + 76);
    const u32 buffer = m.r32(m.r32(g::kScene) + 4);
    const u32 display = m.r32(m.r32(0x800BCE58u) + 4116 * buffer);
    camera = m.r32(g::kWorld);
    const u32 packet = m.r32(camera + 20);
    const s32 x = m.r16s(display + 8), y = m.r16s(display + 10);
    const s32 centreX = x + (m.r16s(display + 4) >> 1), centreY = y + (m.r16s(display + 6) >> 1);
    m.w32(camera + 20, packet + 12);
    m.w16(camera + 660, x); m.w16(camera + 662, y);
    m.w16(camera + 664, centreX); m.w16(camera + 666, centreY);
    offset_packet(m, packet, m.r16s(camera + 660), m.r16s(camera + 662));
    link(m, packet, m.r32(camera + 16) + 4);
    const u32 next = draw_model_pass(camera + 580, m.r32(camera + 16) + 4,
        m.r32(camera + 20), static_cast<u32>(centreX));
    camera = m.r32(g::kWorld);
    m.w32(camera + 20, next + 12);
    offset_packet(m, next, m.r16s(camera + 664), m.r16s(camera + 666));
    link(m, next, m.r32(camera + 16) + 0x3FFC);
}
}

#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kFx = 0x800BCF90u;

bool paused(Memory& m) { return (m.r32(m.r32(g::kScene)) & 0x20000000u) != 0; }
}

void Game::gun_muzzle(u32 rec) {
    cover(0x8009044Cu);
    const s32 kind = m.r16s(rec + 4);
    const s32 t = m.r32s(rec) - 2;
    const u32 view = m.r32(m.r32(g::kWorld) + 40);
    if (m.r32(view + 80) == 1) return;
    cover(0x80090210u);
    Frame f(*this, 0x40);
    if (t < 0 || static_cast<u32>(kind) >= 6u) return;
    const u32 mat = f.at(0x10);
    rot_matrix_yxz(slot(0) + 24, mat);
    set_rot_matrix(mat);
    gte.mtc2(0, 0xFF9C0000u);
    gte.mtc2(1, 0xF38u);
    gte.op(0x486012);
    const u32 w = m.r32(g::kWorld);
    m.w16(w + 120, gte.mfc2(25));
    m.w16(w + 122, gte.mfc2(26));
    m.w16(w + 124, gte.mfc2(27));
}

void Game::view_filters() {
    cover(0x8008FFACu);
    Frame f(*this, 24);
    const u32 sc = m.r32(g::kScene);
    const u32 fx = m.r32(kFx);
    u32 v = m.r16(sc + 942);
    const u32 pad = m.r16(sc + 96);
    if (v & 4u) {
        v &= ~4u;
        m.w32(fx + 1480, m.r32(fx + 1480) | 1u);
        m.w16(sc + 942, v);
    }
    if (pad & 0x300u) {
        if (pad & 0x200u) v ^= 1u;
        if (pad & 0x100u) {
            if (v & 2u) v |= 4u;
            v ^= 2u;
        }
        m.w16(sc + 942, v);
    }
    if (v & 6u) {
        m.w32(fx + 1480, m.r32(fx + 1480) | 1u);
        if (v & 2u) {
            cover(0x8008488Cu);
            cover(0x80084318u);
            for (s32 h = 0xF0; h > 0; h -= 0x30)
                for (s32 w = 0x140; w > 0; w -= 0x20) rand();
        }
    }
}

void Game::engine_flames(u32 st) {
    cover(0x8008C0A0u);
    const u32 def = m.r32(m.r32(g::kExit) + 12);
    u32 n = m.r8(def + 12);
    if (!n) return;
    if (n >= 3) n = 2;
    s32 thrust = m.r16s(st + 6);
    if (thrust <= 0) return;
    if (thrust >= 4097) thrust = 4096;
    const bool stop = paused(m);
    u32 bits = m.r32(st + 12);
    if (!stop) bits <<= 2;
    for (u32 i = 0; i < n; ++i)
        if (!stop && (rand() & 0xFFF) >= 2 * (3966 - thrust)) bits |= 1u << i;
    if (!stop) m.w32(st + 12, bits);
}
}

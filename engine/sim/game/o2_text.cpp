#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kE = 0x80112E00u;
u32 E(Memory& m) { return m.r32(kE); }
}

void Game::o2_text_at(u32 x, u32 y) {
    cover(0x800F33A8u);
    const u32 v = (x & 0xFFFFu) | (y << 16);
    m.w32(E(m) + 0xA8E4, v);
    m.w32(E(m) + 0xA8E8, v);
}

void Game::o2_text_rgb(u32 r, u32 g, u32 b) {
    cover(0x800F33D0u);
    m.w32(E(m) + 0xA8CC, r | (g << 8) | (b << 16));
}

void Game::o2_text_tpage() {
    const u32 e = E(m);
    const u32 ty = m.r16(e + 0xA8F6), tx = m.r16(e + 0xA8F4);
    m.w16(e + 0xA8EC, ((m.r8(e + 0xA909) & 3u) << 5) | ((ty & 0x100u) >> 4) | ((tx & 0x3FFu) >> 6) | ((ty & 0x200u) << 2));
}

void Game::o2_text_semi(s32 abr) {
    cover(0x800F33F8u);
    if (static_cast<s16>(abr) == -1) {
        m.w8(E(m) + 0xA908, 0);
        m.w8(E(m) + 0xA909, 0);
    } else {
        m.w8(E(m) + 0xA908, 1);
        m.w8(E(m) + 0xA909, static_cast<u32>(abr));
    }
    o2_text_tpage();
}

void Game::o2_text_frame_mode(u32 v) {
    cover(0x800F34B0u);
    m.w16(E(m) + 0xA910, v);
}

void Game::o2_text_fold(u32 on, u32 a, u32 b) {
    cover(0x800F34C8u);
    m.w8(E(m) + 0xA90B, on);
    m.w16(E(m) + 0xA90C, a);
    m.w16(E(m) + 0xA90E, b);
}

void Game::o2_text_font(u32 k) {
    cover(0x800F35B8u);
    const u32 e = E(m);
    auto base = [&](u32 w, u32 h, u32 row, u32 a903, u32 tx, u32 ty, u32 cx, u32 cy) {
        m.w8(e + 0xA900, w);
        m.w8(e + 0xA901, h);
        m.w8(e + 0xA902, row);
        m.w8(e + 0xA903, a903);
        m.w16(e + 0xA8F4, tx);
        m.w16(e + 0xA8F6, ty);
        m.w16(e + 0xA8FC, cx);
        m.w16(e + 0xA8FE, cy);
    };
    switch (k) {
    case 1:
        base(6, 6, 32, 0, 320, 320, 320, 344);
        m.w16(e + 0xA8F0, 16);
        m.w16(e + 0xA8F2, 2);
        m.w8(e + 0xA908, 0);
        m.w8(e + 0xA909, 0);
        break;
    case 2:
        base(6, 6, 32, 0, 392, 280, 384, 510);
        m.w16(e + 0xA8F2, 2);
        m.w16(e + 0xA8F0, 32);
        m.w8(e + 0xA908, 0);
        m.w8(e + 0xA909, 0);
        break;
    case 3: case 4:
        base(16, 21, 33, 1, 768, k == 3 ? 382u : 256u, 400, 45);
        m.w16(e + 0xA8F0, 16);
        m.w16(e + 0xA8F2, 1);
        m.w8(e + 0xA90A, 4);
        m.w32(e + 0xA904, k == 3 ? 0x8010BB38u : 0x8010BAD8u);
        m.w8(e + 0xA908, 1);
        m.w8(e + 0xA909, 2);
        break;
    case 5:
        base(12, 16, 33, 1, 832, 0, 416, 19);
        m.w16(e + 0xA8F0, 21);
        m.w16(e + 0xA8F2, 1);
        m.w8(e + 0xA90A, 4);
        m.w32(e + 0xA904, 0x8010BBB0u);
        m.w8(e + 0xA908, 0);
        m.w8(e + 0xA909, 0);
        break;
    case 6:
        base(12, 16, 33, 1, 832, 0, 416, 21);
        m.w16(e + 0xA8F0, 21);
        m.w16(e + 0xA8F2, 1);
        m.w8(e + 0xA90A, 4);
        m.w32(e + 0xA904, 0x8010BBB0u);
        m.w8(e + 0xA908, 1);
        m.w8(e + 0xA909, 2);
        break;
    case 7:
        base(6, 10, 48, 0, 384, 384, 400, 511);
        m.w16(e + 0xA8F0, 25);
        m.w16(e + 0xA8F2, 1);
        m.w8(e + 0xA90A, 6);
        m.w8(e + 0xA908, 0);
        m.w8(e + 0xA909, 0);
        break;
    case 8: case 9:
        base(8, 29, 48, 1, k == 8 ? 918u : 896u, 384, 432, k == 8 ? 47u : 48u);
        m.w16(e + 0xA8F0, 12);
        m.w16(e + 0xA8F2, 1);
        m.w8(e + 0xA90A, 6);
        m.w8(e + 0xA908, 0);
        m.w8(e + 0xA909, 0);
        m.w32(e + 0xA904, k == 8 ? 0x8010BB98u : 0x8010BBA4u);
        break;
    default: break;
    }
    o2_text_tpage();
    const u32 e2 = E(m);
    m.w16(e2 + 0xA8EE, (m.r16(e2 + 0xA8FE) << 6) | ((m.r16(e2 + 0xA8FC) >> 4) & 0x3Fu));
    m.w16(e2 + 0xA8F8, static_cast<u32>(static_cast<s32>(static_cast<s16>(m.r16s(e2 + 0xA8F4) % 64)) << 2));
    m.w16(e2 + 0xA8FA, static_cast<u32>(m.r16s(e2 + 0xA8F6) % 256));
}

void Game::o2_text_setup() {
    cover(0x800F3C14u);
    Frame f(*this, 0x30);
    m.w32(kE, mem_alloc(0xA914));
    for (u32 k = 0; k < 2; ++k) {
        const u32 b = mem_alloc(4);
        const u32 e = E(m);
        m.w32(e + 0xA8D4 + 4 * k, b);
        m.w8(e + 0xA908, 0);
        m.w32(e + 0x8CA4, e + 24004 + 6000 * k);
        m.w8(E(m) + 0xA909, 0);
        o2_text_font(4);
        for (u32 i = 0; i < 300; ++i) {
            const u32 p = m.r32(E(m) + 0x8CA4);
            m.w8(p + 3, 4);
            m.w8(p + 7, 100);
            m.w8(p + 4, 0x80);
            m.w8(p + 5, 0x80);
            m.w8(p + 6, 0x80);
            m.w32(E(m) + 0x8CA4, p + 20);
        }
    }
    m.w32(E(m) + 0xA8D0, 0);
    m.w8(E(m) + 0xA90B, 0);
    o2_text_at(10, 200);
    o2_text_rgb(128, 128, 128);
}

void Game::o2_text_frame(u32 ot) {
    cover(0x800F3DB4u);
    Frame f(*this, 0x20);
    const u32 k = m.r32(m.r32(g::kScene) + 4);
    const u32 e = E(m);
    m.w32(e + 0x5DC0, e + 12000 * k);
    m.w32(e + 0x8CA4, e + 6000 * k + 24004);
    m.w32(e + 0xA8C8, e + 3600 * k + 36008);
    const u32 t = m.r32(e + 0xA8D4 + 4 * k);
    m.w32(e + 0xA8DC, t);
    m.w32(e + 0xA8E0, t);
    clear_otag_r(t, 1);
    const u32 e2 = E(m);
    const u32 tt = m.r32(e2 + 0xA8DC);
    m.w32(tt, (m.r32(tt) & 0xFF000000u) | (m.r32(ot) & 0x00FFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (m.r32(e2 + 0xA8DC) & 0x00FFFFFFu));
}
}

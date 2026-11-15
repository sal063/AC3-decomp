#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDisplay = 0x800BCE48u;
constexpr u32 kPlayer = 0x800BD044u;
constexpr u32 kCtx = 0x800BD048u;
constexpr u32 kClearRect = 0x800BD038u;
constexpr u32 kSubs = 0x800BCD8Cu;
constexpr u32 kMovies = 0x800BCD94u;
constexpr u32 kPhaseKind = 0x800BCEA8u;
constexpr u32 kStartKinds = 0x800B1FC4u;
}

void Game::wingmen_route_reset() {
    cover(0x8004E060u);
    const u32 rec = kStartKinds + 4 * m.r32(kPhaseKind);
    for (u32 i = 0; i < 2; ++i) {
        const u32 s = m.r8(rec + 2 + i);
        if (!s) continue;
        const u32 ai = m.r32(g::kAiInfo) + 184 * s;
        const u32 kind = m.r8(m.r32((s << 6) + m.r32(g::kSlotRecords) + 48));
        const u32 flags = m.r32(ai);
        m.w8(ai + 29, 0);
        m.w8(ai + 30, 0);
        m.w32(ai + 56, 0);
        m.w8(ai + 61, 0xFF);
        m.w8(ai + 60, 0xFF);
        m.w32(ai, flags | 0x200000u);
        m.w8(ai + 28, kind);
    }
}

u32 Game::gmv_decode_ready(u32 poll) {
    cover(0x80056FD8u);
    Frame f(*this, 0x18);
    if (m.r8(m.r32(kCtx) + 4)) return 1;
    if (!gmv_vlc_next(m.r32(kCtx))) return 0;
    m.w8(m.r32(kCtx) + 3, 0);
    const u32 c1 = m.r32(kCtx);
    hw->dec_dct_in(m.r32(c1 + 4 * m.r8(c1) + 8), m.r8(c1 + 5) | 2u);
    const u32 c2 = m.r32(kCtx);
    hw->dec_dct_out(m.r32(c2 + 4 * m.r8(c2 + 1) + 0x10), static_cast<u32>(m.r16s(c2 + 0x11036)));
    gmv_wait_dct(m.r32(kCtx), poll);
    return 0;
}

void Game::mission_movie_begin() {
    cover(0x800573E8u);
    Frame f(*this, 0x20);
    const u32 sc = m.r32(g::kScene);
    const u32 k = m.r8(sc + 0x3A8);
    if (k == 7 || k == 16 || k == 12) {
        request_exit(0x20000, m.r32(m.r32(g::kWorld) + 8));
        return;
    }
    if (m.r8(sc + 0x39C) & m.r8(sc + 0x3A7))
        m.w32(m.r32(kSubs) + 4, m.r16s(m.r32(kMovies) + 8 * m.r8(sc + 0x3A8) + 2) == 1 ? 1u : 0u);
    else
        m.w32(m.r32(kSubs) + 4, 0);
    const u32 sc1 = m.r32(g::kScene);
    const u32 display = m.r32(kDisplay);
    const u32 world = m.r32(g::kWorld);
    m.w32(sc1, m.r32(sc1) | 0x10000000u);
    m.w32(display + 0x14, 0);
    heap_set(m.r32(world + 0x2B8));
    m.w32(kPlayer, mem_alloc(0x44));
    m.w32(m.r32(kPlayer) + 4, mem_alloc(0x400));
    const u32 b = mem_alloc(0x400);
    const u32 p = m.r32(kPlayer);
    m.w32(p + 8, b);
    m.w32(p, 0);
    m.w16(p + 0x24, 0);
    m.w16(p + 0x26, 0);
    m.w16(p + 0x28, 0);
    m.w16(p + 0x2A, 256);
    m.w16(p + 0x2C, 320);
    m.w16(p + 0x2E, 240);
    m.w16(p + 0x30, 0);
    m.w16(p + 0x32, 32);
    const s32 stream = m.r16s(m.r32(kMovies) + 8 * m.r8(m.r32(g::kScene) + 0x3A8));
    m.w32(p + 0x38, 0);
    m.w32(p + 0x3C, 0);
    m.w32(p + 0x40, 120);
    m.w32(p + 0x34, static_cast<u32>(stream));
    m.w16(kClearRect, 0);
    m.w16(kClearRect + 2, 0);
    m.w16(kClearRect + 4, 320);
    m.w16(kClearRect + 6, 240);
    hw->clear_image(kClearRect, 1, 1, 1);
    m.w16(kClearRect, 0);
    m.w16(kClearRect + 2, 256);
    hw->clear_image(kClearRect, 1, 1, 1);
    if (m.r32(m.r32(kSubs) + 4)) radio_face(7, 0, 0);
    const u32 sc2 = m.r32(g::kScene);
    const u32 k2 = m.r8(sc2 + 0x3A8);
    const bool skip = m.r16(sc2 + 0x3AC) ? (k2 == 12 || k2 == 27) : (k2 - 29u < 2u);
    request_exit(skip ? 0x10000u : 0x8000u, m.r32(m.r32(g::kWorld) + 8));
}

void Game::mission_movie_end() {
    cover(0x80057640u);
    Frame f(*this, 0x18);
    const u32 sc = m.r32(g::kScene);
    m.w32(sc, m.r32(sc) & ~0x10000000u);
    m.w32(m.r32(kDisplay) + 0x14, 2);
    if (m.r32(m.r32(kSubs) + 4)) radio_face(0xFF, 0, 0);
    heap_set(m.r32(m.r32(g::kWorld) + 0x2B8));
    display_buffers_alloc_recorded(m.r32(m.r32(g::kWorld) + 0x2B8));
    m.w16(kClearRect + 4, 320);
    m.w16(kClearRect, 0);
    m.w16(kClearRect + 2, 0);
    m.w16(kClearRect + 6, 240);
    hw->clear_image(kClearRect, 1, 1, 1);
    m.w16(kClearRect, 0);
    m.w16(kClearRect + 2, 256);
    hw->clear_image(kClearRect, 1, 1, 1);
    mission_end();
}

void Game::mission_movie_tick() {
    cover(0x80057BBCu);
    Frame f(*this, 0x18);
    const u32 sub = m.r32(m.r32(g::kWorld) + 4);
    if (sub == 0x4000u) {
        mission_movie_begin();
        return;
    }
    if (sub == 0x10000u) {
        mission_movie_end();
        return;
    }
    const u32 buffer = m.r32(m.r32(g::kScene) + 4);
    const u32 p = m.r32(kPlayer);
    const u32 ot = p + 8 * buffer + 0xC;
    m.w32(p + 0x20, ot);
    m.w32(p + 0x1C, m.r32(p + 4 * buffer + 4));
    clear_otag_r(ot, 2);
    if (gmv_play()) {
        request_exit(0x10000, m.r32(m.r32(g::kWorld) + 8));
        return;
    }
    if (m.r32(m.r32(kSubs) + 4)) {
        const u32 pl = m.r32(kPlayer);
        const u32 np = face_draw(m.r32(pl + 0x20), m.r32(pl + 0x1C));
        m.w32(m.r32(kPlayer) + 0x1C, np);
    }
    hw->draw_otag(m.r32(m.r32(kPlayer) + 0x20) + 4);
}
}

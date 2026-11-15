#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDisplay = 0x800BCE48u;
constexpr u32 kCtx = 0x800BD048u, kPlayer = 0x800BD044u, kPhaseCount = 0x800BD040u;
constexpr u32 kClearRect = 0x800BD038u;
constexpr u32 kPage = 0x800BCD90u;
constexpr u32 kMovieWait = 0x800BCE80u;
constexpr u32 kSubs = 0x800BCD8Cu;
constexpr u32 kMovies = 0x800BCD94u;
constexpr u32 kSaveRect = 0x800BCD98u;
constexpr u32 kStrm = 0x800BCBACu, kFlag = 0x800BCBB0u, kRingState = 0x800BCBB4u, kReq = 0x800BCBBCu;
constexpr u32 kKcd = 0x800BCBA8u;
constexpr u32 kFace = 0x800BCE88u;
}

u32 Game::kcd_ring_get(u32 data_out, u32 sector_out) {
    cover(0x8002B9ECu);
    const u32 s = m.r32(kStrm);
    const u32 r0 = m.r32(kRingState);
    if (m.r32(s + 0x20) != 64 || m.r32s(r0 + 0x34) <= 0) {
        if (m.r32(s) & 0x8000u) {
            const u32 f = m.r32(kFlag);
            m.w32(f, m.r32(f) & ~(1u << (m.r8(s + 0x1D) & 31)));
        }
        return 1;
    }
    u32 t1 = m.r32(r0 + 0x3C);
    u32 v1 = m.r8(t1);
    if (v1 == 4) {
        m.w8(t1, 0);
        const u32 r = m.r32(kRingState);
        const u32 first = m.r32(r + 0x10);
        m.w32(r + 0x3C, first);
        t1 = first;
        m.w32(r + 0x38, 0);
        m.w32(r + 0x40, m.r32(r + 0x14));
        v1 = m.r8(t1);
    }
    if (v1 != 2) return 1;
    const u32 s2 = m.r32(kStrm);
    if (m.r32(s2) & 0x8000u) {
        const u32 ch = m.r8(s2 + 0x1D);
        const u32 f = m.r32(kFlag);
        m.w32(f, m.r32(f) | (1u << (ch & 31)));
        for (u32 i = 0; i < 4; ++i) m.w32(f + 16 * ch + 4 + 4 * i, m.r32(t1 + 16 + 4 * i));
    }
    const u32 r = m.r32(kRingState);
    m.w32(sector_out, m.r32(r + 0x3C));
    m.w32(data_out, m.r32(r + 0x40));
    const u32 at = m.r32s(r + 0x38) + m.r8(t1 + 2);
    m.w32(r + 0x38, at);
    if (static_cast<s32>(at) >= m.r32s(r + 0xC)) {
        m.w32(r + 0x38, 0);
        m.w32(r + 0x3C, m.r32(r + 0x10));
        m.w32(r + 0x40, m.r32(r + 0x14));
    } else {
        const u32 step = 4 * m.r32(r), step2 = 4 * m.r32(r + 4);
        for (u32 i = 0; i < m.r8(t1 + 2); ++i) {
            m.w32(r + 0x3C, m.r32(r + 0x3C) + step);
            m.w32(r + 0x40, m.r32(r + 0x40) + step2);
        }
    }
    m.w32(r + 0x34, m.r32(r + 0x34) - 1);
    return 0;
}

u32 Game::kcd_ring_free(u32 rec) {
    cover(0x8002BBE0u);
    Frame f(*this, 0x28);
    const u32 n = m.r8(rec + 2);
    u32 p = rec;
    for (u32 i = 0; i < n; ++i) {
        if (m.r8(p) != 2) {
            m.w32(f.at(0x10), m.r8(p));
            m.w32(f.at(0x14), m.r8(p + 1));
            m.w32(f.at(0x18), m.r8(p + 2));
            hw->cd_control_b(9, 0, m.r32(kKcd) + 0x28);
            unported(0x800A239Cu, "StopCallback (a KCd ring error)");
            return 1;
        }
        m.w8(p, 0);
        p += 4 * m.r32(m.r32(kRingState));
    }
    return 0;
}

u32 Game::kcd_request_video(u32 id, u32 ring, u32 size, u32 flags, u32 done) {
    cover(0x8002C928u);
    const u32 q = m.r32(kReq);
    const u32 n = m.r32(q + 4);
    m.w32(q + 4, n + 1);
    const u32 e = q + 20 * (n & 0xF) + 8;
    m.w8(e, 4);
    m.w16(e + 2, id);
    m.w32(e + 4, ring);
    m.w32(e + 8, size);
    m.w32(e + 0xC, flags);
    m.w8(e + 1, 0);
    m.w32(e + 0x10, done);
    return 4;
}

u32 Game::gmv_first_page() {
    cover(0x80056B1Cu);
    return m.r8(m.r32(kCtx) + 2) == 0 ? 1u : 0u;
}

void Game::gmv_decoder_init() {
    cover(0x80056B30u);
    Frame f(*this, 0x18);
    hw->dec_dct_reset(0);
    dct_vlc_size(0);
    hw->dec_dct_out_callback(0x80057170u);
    dct_vlc_build(m.r32(kCtx) + 0x30);
    m.w8(m.r32(kCtx) + 4, 0);
    m.w8(m.r32(kCtx) + 3, 1);
}

void Game::gmv_setup(u32 d) {
    cover(0x80056B88u);
    const u32 c = m.r32(kCtx);
    m.w32(c + 0x11038, m.r32(d + 4));
    m.w32(c + 0x1103C, m.r32(d + 8));
    m.w32(c + 0x11040, m.r32(d + 0xC));
    m.w32(c + 8, m.r32(d + 0x10));
    m.w32(c + 0xC, m.r32(d + 0x14));
    m.w32(c + 0x10, m.r32(d + 0x18));
    m.w32(c + 0x14, m.r32(d + 0x1C));
    m.w8(c + 5, m.r8(d));
    const u32 c2 = m.r32(kCtx);
    m.w16(c2 + 0x18, m.r16(d + 0x20));
    m.w16(c2 + 0x1A, m.r16(d + 0x22));
    m.w16(c2 + 0x20, m.r16(d + 0x24));
    m.w16(c2 + 0x1C, 0);
    m.w16(c2 + 0x1E, 0);
    m.w16(c2 + 0x24, 0);
    m.w16(c2 + 0x26, 0);
    m.w16(c2 + 0x22, m.r16(d + 0x26));
    m.w16(c2 + 0x28, m.r16(d + 0x20));
    m.w16(c2 + 0x2A, m.r16(d + 0x22));
    m.w16(c2 + 0x2C, m.r32(d) ? 24u : 16u);
    const u32 c3 = m.r32(kCtx);
    m.w8(c3 + 1, 0);
    m.w8(c3, 0);
    m.w16(c3 + 0x2E, 0);
    m.w16(c3 + 0x11032, 0);
    m.w16(c3 + 0x11030, 0);
    m.w16(c3 + 0x11036, 0);
    m.w8(c3 + 2, 0);
    m.w8(m.r32(kCtx) + 3, 0);
}

u32 Game::gmv_open(u32 stream, u32 bits24, u32 base, u32 ring, u32 vlc, u32 strip, u32 x0, u32 y0, u32 x1,
                   u32 y1) {
    cover(0x80056CACu);
    Frame f(*this, 0x40);
    m.w32(kCtx, base);
    const u32 d = f.at(0x10);
    const u32 ring4 = ((ring + 3) >> 2) << 2;
    const u32 vlc4 = ((vlc + 3) >> 2) << 2, strip4 = ((strip + 3) >> 2) << 2;
    u32 p = base + 0x11044;
    m.w32(d, bits24);
    m.w32(d + 4, stream);
    m.w32(d + 8, ring4);
    m.w32(d + 0xC, p);
    p += ring4;
    m.w32(d + 0x10, p);
    p += vlc4;
    m.w32(d + 0x14, p);
    p += vlc4;
    m.w32(d + 0x18, p);
    p += strip4;
    m.w32(d + 0x1C, p);
    p += strip4;
    m.w16(d + 0x20, x0);
    m.w16(d + 0x22, y0);
    m.w16(d + 0x24, x1);
    m.w16(d + 0x26, y1);
    gmv_setup(d);
    gmv_decoder_init();
    cd_reset(0, 0);
    return p;
}

u32 Game::gmv_request() {
    cover(0x80056D7Cu);
    Frame f(*this, 0x20);
    const u32 c = m.r32(kCtx);
    u32 flags = (m.r32(m.r32(kSubs) + 4) != 0 ? 1u : 0u) << 14;
    if (m.r8(c + 5)) flags |= 0x800u;
    m.w32(f.at(0x10), 0x80056C9Cu);
    return kcd_request_video(m.r32(c + 0x11038), m.r32(c + 0x11040), m.r32(c + 0x1103C), flags, 0x80056C9Cu);
}

u32 Game::gmv_vlc_next(u32 c) {
    cover(0x800572C8u);
    Frame f(*this, 0x20);
    const u32 data = f.at(0x10), sector = f.at(0x14);
    if (kcd_ring_get(data, sector)) return 0;
    const u32 sec = m.r32(sector);
    if (!m.r16(sec + 6)) m.w8(c + 4, 1);
    const u32 hi = c + 0x10000;
    m.w16(hi + 0x1034, m.r16(sec + 6));
    const s32 h = m.r16s(sec + 0xA);
    if (m.r16s(hi + 0x1032) != h) {
        m.w16(hi + 0x1032, static_cast<u32>(h));
        m.w16(hi + 0x1036, static_cast<u32>((h * m.r16s(c + 0x2C)) >> 1));
        m.w16(c + 0x2E, static_cast<u32>(h));
        m.w16(c + 0x26, static_cast<u32>(h));
        m.w16(c + 0x1E, static_cast<u32>(h));
    }
    s32 w = m.r16s(m.r32(sector) + 8);
    if (m.r16s(hi + 0x1030) != w) {
        m.w16(hi + 0x1030, static_cast<u32>(w));
        if (m.r8(c + 5)) w = (3 * w) >> 1;
        m.w16(c + 0x24, static_cast<u32>(w));
        m.w16(c + 0x1C, static_cast<u32>(w));
    }
    const u32 b = m.r8(c) ^ 1u;
    m.w8(c, b);
    dct_vlc2(m.r32(data), m.r32(c + 4 * b + 8), c + 0x30);
    kcd_ring_free(m.r32(sector));
    return 1;
}

u32 Game::gmv_first_vlc(u32 once) {
    cover(0x80056DDCu);
    Frame f(*this, 0x28);
    if (once) return gmv_vlc_next(m.r32(kCtx)) ? 1u : 0u;
    m.w32(f.at(0x10), 120);
    for (;;) {
        if (gmv_vlc_next(m.r32(kCtx))) return 1;
        kcd_pump();
        hw->vsync(0);
        const s32 t = m.r32s(f.at(0x10)) - 1;
        m.w32(f.at(0x10), static_cast<u32>(t));
        if (t < 0) break;
    }
    m.w8(m.r32(kCtx) + 4, 1);
    return 1;
}

u32 Game::gmv_close() {
    cover(0x80056E8Cu);
    Frame f(*this, 0x18);
    cd_reset(0, 0);
    hw->dec_dct_out_callback(0);
    return 1;
}

u32 Game::gmv_wait_dct(u32 c, u32 poll) {
    cover(0x80057084u);
    Frame f(*this, 0x20);
    m.w32(f.at(0x10), 0x40000);
    if (poll) return m.r8(c + 3);
    while (!m.r8(c + 3)) {
        const u32 stop = m.r8(m.r32(kCtx) + 4);
        m.w32(f.at(0x14), stop);
        const s32 t = m.r32s(f.at(0x10)) - 1;
        m.w32(f.at(0x10), static_cast<u32>(t));
        if (t > 0 && !stop) continue;
        const u32 page = m.r8(c + 2) == 0 ? 1u : 0u;
        m.w8(c + 2, page);
        m.w8(c + 3, 1);
        const u32 y = m.r16(c + 8 * page + 0x18);
        m.w16(c + 0x28, y);
        m.w16(c + 0x2A, m.r16(c + 8 * m.r8(c + 2) + 0x1A));
    }
    return 1;
}

u32 Game::gmv_decode(u32 poll) {
    cover(0x80056EB8u);
    Frame f(*this, 0x28);
    const u32 c = m.r32(kCtx);
    if (m.r8(c + 4)) return 1;
    m.w8(c + 3, 0);
    const u32 c1 = m.r32(kCtx);
    hw->dec_dct_in(m.r32(c1 + 4 * m.r8(c1) + 8), m.r8(c1 + 5) | 2u);
    const u32 c2 = m.r32(kCtx);
    hw->dec_dct_out(m.r32(c2 + 4 * m.r8(c2 + 1) + 0x10), static_cast<u32>(m.r16s(c2 + 0x11036)));
    m.w32(f.at(0x10), 0x40000);
    for (;;) {
        if (gmv_vlc_next(m.r32(kCtx))) break;
        if (m.r8(m.r32(kCtx) + 4) == 1) break;
        const s32 t = m.r32s(f.at(0x10)) - 1;
        m.w32(f.at(0x10), static_cast<u32>(t));
        if (t > 0) continue;
        break;
    }
    if (m.r32s(f.at(0x10)) <= 0) m.w8(m.r32(kCtx) + 4, 1);
    gmv_wait_dct(m.r32(kCtx), poll);
    return 0;
}

void Game::gmv_strip_done() {
    cover(0x80057170u);
    Frame f(*this, 0x18);
    if (m.r8(m.r32(kCtx) + 5) && m.r32s(kMovieWait) > 0) {
        kcd_stream_reader();
        m.w32(kMovieWait, 0);
    }
    const u32 c = m.r32(kCtx);
    hw->load_image(c + 0x28, m.r32(c + 4 * m.r8(c + 1) + 0x10));
    const u32 c1 = m.r32(kCtx);
    m.w8(c1 + 1, m.r8(c1 + 1) ^ 1u);
    const u32 c2 = m.r32(kCtx);
    const u32 x = (m.r16(c2 + 0x28) + m.r16(c2 + 0x2C)) & 0xFFFFu;
    const u32 pg = m.r8(c2 + 2);
    m.w16(c2 + 0x28, x);
    if (static_cast<s16>(x) < m.r16s(c2 + 8 * pg + 0x18) + m.r16s(c2 + 8 * pg + 0x1C)) {
        hw->dec_dct_out(m.r32(c2 + 4 * m.r8(c2 + 1) + 0x10), static_cast<u32>(m.r16s(c2 + 0x11036)));
        return;
    }
    m.w8(c2 + 3, 1);
    const u32 c3 = m.r32(kCtx);
    m.w8(c3 + 2, m.r8(c3 + 2) ^ 1u);
    const u32 c4 = m.r32(kCtx);
    const u32 r = c4 + 8 * m.r8(c4 + 2);
    m.w16(c4 + 0x28, m.r16(r + 0x18));
    m.w16(c4 + 0x2A, m.r16(c4 + 8 * m.r8(c4 + 2) + 0x1A));
}

void Game::gmv_show(u32 page) {
    const u32 d = m.r32(kDisplay);
    m.w32(d, m.r32(d) | 0x30u);
    const u32 p = m.r32(kPlayer);
    m.w32(kPage, page);
    set_def_disp_env(d + 0x18, m.r16s(p + 4 * page + 0x24), m.r16s(p + 4 * page + 0x26), m.r16s(p + 0x2C),
                     m.r16s(p + 0x2E));
    m.w8(m.r32(kDisplay) + 0x28, 0);
    m.w8(m.r32(kDisplay) + 0x29, m.r8(m.r32(kPlayer) + 0x38));
    const u32 d2 = m.r32(kDisplay), p2 = m.r32(kPlayer);
    m.w16(d2 + 0x20, m.r16(d2 + 8));
    m.w16(d2 + 0x22, m.r16(d2 + 0xC));
    const u32 other = page ^ 1u;
    const s32 w = m.r32(p2 + 0x38) ? (m.r16s(p2 + 0x2C) * 3) >> 1 : m.r16s(p2 + 0x2C);
    set_def_draw_env(d2 + 0x2C, m.r16s(p2 + 4 * other + 0x24), m.r16s(p2 + 4 * other + 0x26), w,
                     m.r16s(p2 + 0x2E));
    const u32 d3 = m.r32(kDisplay);
    m.w8(d3 + 0x44, 1);
    m.w8(d3 + 0x45, 0);
    m.w8(d3 + 0x46, 0);
    m.w8(d3 + 0x47, 0);
}

u32 Game::gmv_play() {
    cover(0x80057720u);
    Frame f(*this, 0x30);
    const u32 p = m.r32(kPlayer);
    switch (m.r32(p)) {
    case 0: {
        const u32 base = mem_peek(0);
        const u32 pl = m.r32(kPlayer);
        const u32 strip = m.r32(pl + 0x38) ? 0x2100u : 0x1600u;
        const u32 x0 = static_cast<u32>(m.r16s(pl + 0x24) + m.r16s(pl + 0x30));
        const u32 y0 = static_cast<u32>(m.r16s(pl + 0x26) + m.r16s(pl + 0x32));
        const u32 x1 = static_cast<u32>(m.r16s(pl + 0x28) + m.r16s(pl + 0x30));
        const u32 y1 = static_cast<u32>(m.r16s(pl + 0x2A) + m.r16s(pl + 0x32));
        m.w32(f.at(0x10), 0x1B804);
        m.w32(f.at(0x14), strip);
        m.w32(f.at(0x18), x0);
        m.w32(f.at(0x1C), y0);
        m.w32(f.at(0x20), x1);
        m.w32(f.at(0x24), y1);
        gmv_open(m.r32(pl + 0x34), m.r32(pl + 0x38), base, 0x12300, 0x1B804, strip, x0, y0, x1, y1);
        gmv_request();
        m.w32(m.r32(kPlayer), 1);
        return 0;
    }
    case 1: {
        if (m.r32(m.r32(g::kScene) + 4)) return 0;
        if (gmv_first_vlc(1)) {
            m.w32(m.r32(kPlayer), 2);
            gmv_show(gmv_first_page());
            return 0;
        }
        const u32 pl = m.r32(kPlayer);
        const s32 t = m.r32s(pl + 0x40) - 1;
        m.w32(pl + 0x40, static_cast<u32>(t));
        if (t <= 0) m.w8(m.r32(kCtx) + 4, 1);
        if (!m.r8(m.r32(kCtx) + 4)) return 0;
        if (m.r32(m.r32(kSubs) + 4)) radio_face(0xFF, 0, 0);
        gmv_close();
        m.w32(m.r32(kPlayer), 3);
        return 0;
    }
    case 2: {
        if (m.r32(m.r32(g::kScene) + 0x12C) & 0x40u) m.w8(m.r32(kCtx) + 4, 1);
        if (!gmv_decode(0)) {
            gmv_show(gmv_first_page());
            return 0;
        }
        gmv_close();
        const u32 page = gmv_first_page();
        const u32 pl = m.r32(kPlayer);
        m.w16(kClearRect, m.r16(pl + 4 * page + 0x24));
        m.w16(kClearRect + 2, m.r16(pl + 4 * page + 0x26));
        m.w16(kClearRect + 6, m.r16(pl + 0x2E));
        m.w32(kPage, page);
        if (m.r32(pl + 0x38)) m.w16(kClearRect + 4, static_cast<u32>((m.r16s(pl + 0x2C) * 3) >> 1));
        else m.w16(kClearRect + 4, m.r16(pl + 0x2C));
        hw->clear_image(kClearRect, 1, 1, 1);
        if (m.r32(m.r32(kSubs) + 4)) radio_face(0xFF, 0, 0);
        m.w32(m.r32(kPlayer), 3);
        return 0;
    }
    case 3:
        m.w32(m.r32(kPlayer), 4);
        return 0;
    case 4: return 1;
    default: return 0;
    }
}

u32 Game::gmv_phase0() {
    cover(0x80057CD0u);
    Frame f(*this, 0x18);
    m.w32(kPhaseCount, 0);
    cd_reset(0, 0);
    m.w8(m.r32(kDisplay) + 0x10, 0);
    m.w8(m.r32(kDisplay) + 0x11, 0);
    m.w8(m.r32(kDisplay) + 0x12, 0);
    const u32 d = m.r32(kDisplay);
    m.w32(d, m.r32(d) | 8u);
    u32 sc = m.r32(g::kScene);
    if (!m.r8(sc + 0x3A8)) {
        m.w8(sc + 0x3A4, m.r8(sc + 0x3A4) + 1);
        sc = m.r32(g::kScene);
        if ((m.r8(sc + 0x3A4) & 3u) != 1) {
            m.w8(sc + 0x3A5, (m.r32(sc + 0x128) & 0x300u) == 0x300u ? 2u : 1u);
            return 0x60000002u;
        }
    }
    sc = m.r32(g::kScene);
    const u32 on = m.r8(sc + 0x39C) & m.r8(sc + 0x3A7);
    const u32 subs = m.r32(kSubs);
    m.w32(subs + 4, on);
    const u32 rec = static_cast<u32>(m.r16s(m.r32(kMovies) + 8 * m.r8(sc + 0x3A8) + 4));
    if (on && rec) {
        const u32 buf = mem_alloc(ld_record_size(rec));
        const u32 s = m.r32(kSubs);
        m.w32(s, buf);
        m.w32(s + 8, 1);
        ld_request_record(rec, buf, 0x80057CC4u);
        return 0x4000000u;
    }
    m.w32(m.r32(kSubs), 0);
    m.w32(m.r32(kSubs) + 4, 0);
    return 0x4000000u;
}

u32 Game::gmv_phase1() {
    cover(0x80057E34u);
    Frame f(*this, 0x18);
    const u32 s = m.r32(kSubs);
    if (!m.r32(s + 4) || !m.r32(s)) return 0x4000000u;
    if (m.r32(s + 8)) return 0;
    const u32 dir = ld_relocate_dir(m.r32(s));
    const u32 list = ld_relocate_dir(m.r32(dir));
    ld_upload_tims(list, m.r32s(m.r32(dir)));
    return 0x4000000u;
}

u32 Game::gmv_phase2() {
    cover(0x80057EBCu);
    Frame f(*this, 0x28);
    const u32 k = m.r8(m.r32(g::kScene) + 0x3A8);
    if (k == 7 || k == 16 || k == 12) return 0x60000006u;
    if (k == 29) return 0x60000008u;
    if (k == 30) return 0x60000007u;
    u32 n = m.r32(kPhaseCount);
    if (!n) {
        const u32 d = m.r32(kDisplay);
        m.w8(d + 0x10, 0);
        m.w32(d + 4, 0);
        m.w32(d + 0x14, 0);
        m.w8(d + 0x11, 0);
        m.w8(m.r32(kDisplay) + 0x12, 0);
        const u32 d2 = m.r32(kDisplay);
        m.w32(d2, m.r32(d2) | 9u);
        const u32 sc = m.r32(g::kScene);
        m.w32(sc, m.r32(sc) | 0x10000004u);
        n = m.r32(kPhaseCount);
    }
    ++n;
    m.w32(kPhaseCount, n);
    if (static_cast<s32>(n) < 3) return 0;
    if (!m.r32(m.r32(g::kScene) + 4)) return 0;
    m.w32(kPlayer, mem_alloc(0x44));
    m.w32(m.r32(kPlayer) + 4, mem_alloc(0x400));
    const u32 b = mem_alloc(0x400);
    const u32 p = m.r32(kPlayer);
    m.w32(p + 8, b);
    m.w16(p + 0x2C, 320);
    m.w32(p, 0);
    m.w16(p + 0x24, 0);
    m.w16(p + 0x26, 0);
    m.w16(p + 0x28, 0);
    m.w16(p + 0x2A, 256);
    m.w16(p + 0x2E, 240);
    m.w16(p + 0x30, 0);
    m.w16(p + 0x32, 32);
    m.w32(kMovieWait, 0);
    m.w32(p + 0x38, 1);
    m.w32(p + 0x40, 120);
    m.w32(p + 0x34, static_cast<u32>(m.r16s(m.r32(kMovies) + 8 * m.r8(m.r32(g::kScene) + 0x3A8))));
    const u32 rect = f.at(0x10);
    m.w32(rect, m.r32(kSaveRect));
    m.w32(rect + 4, m.r32(kSaveRect + 4));
    const u32 save = mem_alloc(0x26C00);
    m.w32(m.r32(kPlayer) + 0x3C, save);
    hw->store_image(rect, save);
    m.w16(kClearRect, 0);
    m.w16(kClearRect + 2, 0);
    m.w16(kClearRect + 4, 480);
    m.w16(kClearRect + 6, 240);
    hw->clear_image(kClearRect, 1, 1, 1);
    m.w16(kClearRect, 0);
    m.w16(kClearRect + 2, 256);
    hw->clear_image(kClearRect, 1, 1, 1);
    if (m.r32(m.r32(kSubs) + 4)) radio_face(9, 0, 0);
    return 0x4000000u;
}

void Game::gmv_end() {
    cover(0x80058134u);
    Frame f(*this, 0x20);
    cd_reset(0, 0);
    hw->dec_dct_out_callback(0);
    const u32 rect = f.at(0x10);
    m.w32(rect, m.r32(kSaveRect));
    m.w32(rect + 4, m.r32(kSaveRect + 4));
    hw->vsync(0);
    hw->load_image(rect, m.r32(m.r32(kPlayer) + 0x3C));
    const u32 sc = m.r32(g::kScene);
    m.w32(sc, m.r32(sc) & ~4u);
}

u32 Game::gmv_phase3() {
    cover(0x800581C4u);
    Frame f(*this, 0x18);
    const u32 k = m.r32(m.r32(g::kScene) + 4);
    const u32 p = m.r32(kPlayer);
    const u32 ot = p + 8 * k + 0xC;
    m.w32(p + 0x20, ot);
    m.w32(p + 0x1C, m.r32(p + 4 * k + 4));
    clear_otag_r(ot, 2);
    if (gmv_play()) {
        gmv_end();
        const u32 sc = m.r32(g::kScene);
        if (!(m.r32(sc + 0x96C) & 0x10u)) {
            switch (m.r8(sc + 0x3A8)) {
            case 9: case 21: case 23: case 25: case 28: return 0x60000007u;
            default: break;
            }
        }
        return m.r32(m.r32(g::kScene) + 0x3A0) | 0x60000000u;
    }
    if (m.r32(m.r32(kSubs) + 4)) {
        const u32 pl = m.r32(kPlayer);
        const u32 np = face_draw(m.r32(pl + 0x20), m.r32(pl + 0x1C));
        m.w32(m.r32(kPlayer) + 0x1C, np);
    }
    hw->draw_otag(m.r32(m.r32(kPlayer) + 0x20) + 4);
    return 0;
}

u32 Game::run_gmv_phase(u32 fn) {
    switch (fn) {
    case 0x80057CD0u: return gmv_phase0();
    case 0x80057E34u: return gmv_phase1();
    case 0x80057EBCu: return gmv_phase2();
    case 0x800581C4u: return gmv_phase3();
    default: unported(fn, "a phase of group 5"); return 0;
    }
}

u32 Game::face_draw(u32 ot, u32 p) {
    cover(0x8002D3F4u);
    Frame f(*this, 0x28);
    const u32 word = f.at(0x10);
    if (stream_face_word(word)) {
        const u32 v = m.r32(word);
        if (v != 0xFFFFFFFFu && m.r32(0x800BCE90u) != v) {
            m.w8(0x800BCE94u, v >> 25);
            m.w8(0x800BCE95u, (v >> 18) & 0x7Fu);
            m.w8(0x800BCE96u, (v >> 11) & 0x7Fu);
            m.w8(0x800BCE8Eu, (v >> 5) & 0x3Fu);
            m.w8(0x800BCE8Fu, (v >> 2) & 7u);
        }
        m.w32(0x800BCE90u, v);
    }
    face_animate();
    const u32 st = m.r32(m.r32(kFace));
    if (st < 2 || st == 7 || st == 10 || st == 9) {
        if (m.r8(m.r32(g::kScene) + 0x39C) != 1) return p;
    }
    if (m.r8(0x800BCE8Cu) != 1) return p;
    const u32 st2 = m.r32(m.r32(kFace));
    if (st2 == 9) return face_copy(ot, p);
    if (st2 == 10) return face_quads(ot, p);
    return face_sprites(ot, p);
}

u32 Game::face_copy(u32 ot, u32 p) {
    cover(0x8002D330u);
    const u32 fc = m.r32(kFace);
    if (!m.r8(fc + 4)) return p;
    const u32 y = m.r32(kPage) == 1 ? 0x1D6u : 0xD6u;
    m.w32(p, 0x05000000u);
    m.w32(p + 4, 0x01000000u);
    m.w32(p + 8, 0x80000000u);
    m.w32(p + 0x14, 0xA0180u);
    m.w32(p + 0x10, (y << 16) | 0x36u);
    const u32 line = m.r8(fc + 4) - 1;
    m.w32(p + 0xC, ((5 * line) << 17) | 0x240u);
    ot_link(ot, p);
    return p + 0x18;
}

u32 Game::face_sprites(u32 ot, u32 p) {
    cover(0x8002CE70u);
    Frame f(*this, 0x18);
    const u32 st = m.r32(m.r32(kFace));
    const u32 t = 0x800B03ACu + 20 * st;
    if (!st && m.r32(m.r32(g::kScene) + 0x2C) == 2 && (m.r32(m.r32(g::kWorld)) & 1u)) return p;
    u32 drawn = 0;
    for (u32 i = 0; i < 3; ++i) {
        const u32 fr = m.r8(m.r32(kFace) + i + 4);
        if (!fr) continue;
        const s32 n = static_cast<s32>(fr) - 1, per = static_cast<s32>(m.r8(t + 2));
        const s32 q = n / per, rem = n % per;
        const u32 v = static_cast<u32>(rem) * m.r8(t + 1);
        ++drawn;
        m.w8(p + 3, 4);
        m.w8(p + 7, m.r8(t + 6) ? 0x66u : 0x64u);
        m.w8(p + 4, m.r8(t + 3));
        m.w8(p + 5, m.r8(t + 4));
        m.w8(p + 6, m.r8(t + 5));
        m.w16(p + 8, m.r8(t + 7));
        m.w8(p + 0xC, 0);
        m.w16(p + 0xA, m.r16(t + 8 + 2 * i));
        m.w16(p + 0x10, m.r8(t));
        m.w16(p + 0x12, m.r8(t + 1));
        if (m.r32(m.r32(kFace)) == 6 && i == 1) m.w16(p + 0x12, 0x8A);
        m.w16(m.r32(kFace) + 0x1E, 0x4A16);
        m.w16(p + 0xE, ((static_cast<u32>(q) & 7u) << 6) + 0x4A16);
        m.w8(p + 0xD, v);
        ot_link(ot, p);
        const u32 tp = p + 0x14;
        m.w8(tp + 3, 1);
        m.w32(tp + 4, ((m.r16(m.r32(kFace) + 0x1C) + static_cast<u32>(q >> 1)) & 0x9FFu) | 0xE1000200u);
        ot_link(ot, tp);
        p += 0x1C;
    }
    if ((m.r8(t + 0xF) & 0x10u) && drawn > 0) {
        const u32 label = m.r8(0x800BCE8Eu);
        if (label) p = face_label(ot, p, label - 1);
    }
    return p;
}

u32 Game::face_quads(u32 ot, u32 p) {
    cover(0x8002D10Cu);
    const u32 t = 0x800B03ACu + 20 * m.r32(m.r32(kFace));
    for (u32 i = 0; i < 3; ++i) {
        const u32 fr = m.r8(m.r32(kFace) + i + 4);
        if (!fr) continue;
        const s32 n = static_cast<s32>(fr) - 1, per = static_cast<s32>(m.r8(t + 2));
        const s32 q = n / per, rem = n % per;
        const u32 v = static_cast<u32>(rem) * m.r8(t + 1);
        const u32 x = m.r8(t + 7), y = m.r16(t + 8 + 2 * i);
        const u32 w = m.r8(t), h = m.r8(t + 1);
        m.w8(p + 3, 9);
        m.w8(p + 7, m.r8(t + 6) ? 0x2Eu : 0x2Cu);
        m.w8(p + 4, m.r8(t + 3));
        m.w8(p + 5, m.r8(t + 4));
        m.w8(p + 6, m.r8(t + 5));
        m.w16(p + 8, x);
        m.w16(p + 0xA, y);
        m.w16(p + 0x10, x + w);
        m.w16(p + 0x12, y);
        m.w16(p + 0x18, x);
        m.w16(p + 0x1A, y - 1 + 2 * h);
        m.w16(p + 0x20, x + w);
        m.w8(p + 0xC, 0);
        m.w8(p + 0xD, v);
        m.w16(p + 0x22, y - 1 + 2 * h);
        m.w8(p + 0x15, v);
        m.w8(p + 0x1C, 0);
        m.w8(p + 0x14, w);
        m.w8(p + 0x1D, h + v);
        m.w8(p + 0x24, w);
        m.w8(p + 0x25, h + v);
        m.w16(p + 0xE, ((static_cast<u32>(q) & 7u) << 6) + 0x4A16);
        m.w16(p + 0x16, m.r16(m.r32(kFace) + 0x1C) + static_cast<u32>(q >> 1));
        ot_link(ot, p);
        p += 0x28;
    }
    return p;
}

u32 Game::face_label(u32 ot, u32 p, u32 k) {
    cover(0x8002D560u);
    const u32 st = m.r32(m.r32(kFace));
    const u32 t = 0x800B03ACu + 20 * st;
    m.w8(p + 3, 4);
    m.w8(p + 7, m.r8(t + 6) ? 0x66u : 0x64u);
    m.w8(p + 4, m.r8(t + 3));
    m.w8(p + 5, m.r8(t + 4));
    m.w8(p + 6, m.r8(t + 5));
    if (!m.r32(m.r32(kFace))) {
        m.w16(p + 8, 0x84);
        m.w16(p + 0xA, 0x38);
    } else {
        m.w16(p + 8, m.r8(t + 7) - 4);
        m.w16(p + 0xA, m.r16(t + 8) - 6);
    }
    const u32 l = 0x800B0488u + 4 * k;
    m.w8(p + 0xC, m.r8(l));
    m.w8(p + 0xD, m.r8(l + 1));
    m.w16(p + 0x10, m.r8(l + 2));
    m.w16(p + 0xE, 0x7FD8);
    m.w16(p + 0x12, m.r8(l + 3));
    ot_link(ot, p);
    const u32 tp = p + 0x14;
    m.w8(tp + 3, 1);
    m.w32(tp + 4, 0xE1000236u);
    ot_link(ot, tp);
    return p + 0x1C;
}
}

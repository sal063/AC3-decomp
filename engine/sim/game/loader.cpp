#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kLoaderTable = 0x800B4674u;
constexpr u32 kDisplay = 0x800BCE48u;
constexpr u32 kLoadMode = 0x800BCE30u;
constexpr u32 kLoadId = 0x800BCE34u;
constexpr u32 kClearRect = 0x800BCE38u;
constexpr u32 kLoading = 0x800BD0C8u;
constexpr u32 kReq = 0x800BCBBCu;
constexpr u32 kStrm = 0x800BCBACu;
constexpr u32 kLoadDone = 0x8009B6ACu;
}

bool Game::is_loader(u32 fn) { return fn == 0x8009B6B8u || fn == 0x8009B7CCu; }

u32 Game::scene_loader_fn() {
    const u32 sc = m.r32(g::kScene);
    if (m.r32(sc + 52) == m.r32(sc + 56)) return 0;
    return m.r32(kLoaderTable + 4u * m.r32(sc + 48));
}

void Game::run_loader(u32 fn) {
    const u32 sc = m.r32(g::kScene);
    if (fn == 0x8009B6B8u) phase_result = overlay_load_begin(m.r32(sc + 52), m.r32(sc + 60));
    else phase_result = overlay_load_poll();
}

u32 Game::ld_record_size(u32 id) {
    cover(0x8002C5E0u);
    Frame f(*this, 0x18);
    const u32 e = m.r32(kcd_entry(id));
    if (id & 0x800u) return (e & 0x7FFFFu) << ((e >> 24) & 7u);
    return 4 * (e & 0x7FFFFu);
}

u32 Game::ld_request_record(u32 id, u32 dest, u32 cb) {
    cover(0x8002C8B8u);
    Frame thunk(*this, 0x18);
    return ld_request(id, dest, cb);
}

u32 Game::ld_request(u32 id, u32 dest, u32 cb) {
    cover(0x8002C7ACu);
    Frame f(*this, 0x18);
    const u32 q = m.r32(kReq);
    const u32 n = m.r32(q + 4);
    m.w32(q + 0x17C, dest);
    m.w32(q + 4, n + 1);
    const u32 e = q + 8 + 20 * (n & 0xFu);
    m.w8(e, 2);
    m.w16(e + 2, id);
    m.w32(e + 4, dest);
    m.w32(e + 16, cb);
    m.w8(e + 1, 0);
    const u32 end = dest + static_cast<u32>((static_cast<s32>(ld_record_size(id)) >> 2) << 2);
    m.w32(m.r32(kReq) + 0x180, end);
    return end;
}

u32 Game::stream_position() {
    cover(0x8002B5FCu);
    return m.r32(m.r32(kStrm) + 0x18);
}

s32 Game::load_progress(u32 out) {
    cover(0x8002CA2Cu);
    Frame f(*this, 0x20);
    const u32 q = m.r32(kReq);
    const u32 lo = m.r32(q + 0x17C), hi = m.r32(q + 0x180);
    const u32 at = stream_position();
    if (out) m.w32(out, at);
    const u32 total = (hi - lo) >> 2, done = (at - lo) >> 2;
    s32 v = total ? static_cast<s32>((done << 12) / total) : 0;
    if (v < 0) v = 0;
    if (v >= 4097) v = 4096;
    return v;
}

u32 Game::vram_copy_packet(u32 ot, u32 p, u32 sx, u32 sy, u32 dx, u32 dy, u32 w, u32 h) {
    cover(0x800363B0u);
    m.w32(p, 0x05000000u);
    m.w32(p + 4, 0x01000000u);
    m.w32(p + 12, (sx & 0xFFFFu) | (sy << 16));
    m.w32(p + 8, 0x80000000u);
    m.w32(p + 16, (dx & 0xFFFFu) | (dy << 16));
    m.w32(p + 20, (w & 0xFFFFu) | (h << 16));
    m.w32(p, (m.r32(p) & 0xFF000000u) | (m.r32(ot) & 0x00FFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (p & 0x00FFFFFFu));
    return p + 24;
}

u32 Game::overlay_load_begin(u32 id, u32 dest) {
    cover(0x8009B6B8u);
    Frame f(*this, 0x30);
    const u32 d = m.r32(kDisplay);
    m.w32(d + 20, 0);
    m.w32(d, m.r32(d) | 5u);
    const u32 mode = m.r32(d + 4);
    if (mode == 0) {
        const u32 sc = m.r32(g::kScene);
        const u32 from = m.r32(sc + 4) ? 256u : 0u, to = m.r32(sc + 4) ? 0u : 256u;
        m.w32(f.at(0x10), 0);
        m.w32(f.at(0x14), to);
        m.w32(f.at(0x18), 0x140);
        m.w32(f.at(0x1C), 0xF0);
        const u32 p = vram_copy_packet(m.r32(sc + 16), m.r32(sc + 20), 0, from, 0, to, 0x140, 0xF0);
        m.w32(m.r32(g::kScene) + 20, p);
        m.w32(kLoadMode, 1);
    } else {
        m.w32(kLoadMode, mode == 1 ? 2u : 0u);
    }
    m.w32(kLoadId, id);
    m.w32(kLoading, 1);
    ld_request_record(id, dest, kLoadDone);
    return 0x4000000u;
}

u32 Game::overlay_load_poll() {
    cover(0x8009B7CCu);
    Frame f(*this, 0x18);
    if (cd_current() == m.r32(kLoadId)) {
        const s32 q = load_progress(0);
        const u32 mode = m.r32(kLoadMode);
        if (mode == 1 || mode == 2) {
            const u32 rect = g::kScratch;
            m.w16(rect + 4, 0x140);
            m.w16(rect, 0);
            m.w16(rect + 2, 0);
            m.w16(rect + 6, mode == 1 ? 0xF0u : 0x1E0u);
            const u32 sc = m.r32(g::kScene);
            m.w32(sc + 20, flash_tile(m.r32(sc + 12), m.r32(sc + 20), rect, -q));
        }
    }
    if (m.r32(kLoading)) return 0;
    if (!m.r32(m.r32(kDisplay) + 4)) {
        hw->vsync(0);
        hw->clear_image(kClearRect, 0, 0, 0);
    }
    const u32 d = m.r32(kDisplay);
    m.w8(d + 16, 0);
    m.w8(d + 17, 0);
    m.w8(d + 18, 0);
    m.w32(d, m.r32(d) | 8u);
    return 0x2000000u;
}
}

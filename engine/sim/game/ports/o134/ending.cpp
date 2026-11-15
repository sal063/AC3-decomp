#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kWork = 0x800BD050u, kPlayer = 0x800BD054u;
constexpr u32 kDisplay = 0x800BCE48u, kMovies = 0x800BCD94u;
constexpr u32 kPressed = 0x12C;
u32 scene(Memory& m) { return m.r32(g::kScene); }
u32 work(Memory& m) { return m.r32(kWork); }
u32 player(Memory& m) { return m.r32(kPlayer); }
u32 cue_record(u32 p, u32 bank, u32 cue) { return p + 70 * bank + 14 * cue; }
void link_fresh(Memory& m, u32 ot, u32 p, u32 words) {
    m.w32(p, m.r32(ot) | (words << 24));
    m.w32(ot, (p << 8) >> 8);
}
void link_mode(Memory& m, u32 ot, u32 p, u32 mode) {
    m.w8(p + 3, 1);
    m.w32(p + 4, mode);
    m.w32(p, (m.r32(p) & 0xFF000000u) | (m.r32(ot) & 0xFFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (p & 0xFFFFFFu));
}
u32 pair(u32 high, u32 low) { return (high << 16) | static_cast<u32>(static_cast<s32>(static_cast<s16>(low))); }
u32 grey(u32 c) { return (c << 16) | (c << 8) | c; }
}

u32 Game::o3_block_reset(u32 b) {
    cover(0x8011B328u);
    m.w16(b + 4, 0x10);
    m.w16(b + 6, 0x18);
    m.w8(b + 0xC, 0x80);
    m.w8(b + 0xD, 0x80);
    m.w8(b + 0xE, 0x80);
    m.w16(b, 0);
    m.w16(b + 2, 0);
    m.w8(b + 0xF, 0x60);
    m.w32(b + 0x10, 0);
    m.w32(b + 0x2D0, 0);
    m.w32(b + 0x2D4, 0);
    m.w8(b + 0x2D8, 0);
    m.w32(b + 0x2DC, 0);
    m.w32(b + 0x2E0, 0);
    for (u32 i = 0; i < 100; ++i) {
        m.w16(b + 0x1A4 + 2 * i, 0);
        m.w8(b + 0x26C + i, 0);
    }
    m.w32(b + 0x2E4, 0);
    return 0;
}

u32 Game::o3_credits_setup(u32 t) {
    cover(0x8011B39Cu);
    Frame frame(*this, 0x12D0);
    const u32 table = frame.at(0);
    for (u32 k = 0; k < 0x128A; k += 2) m.w16(table + k, m.r16(0x80119FCCu + k));
    for (u32 k = 0; k < 0x1A; k += 2) m.w16(frame.at(0x1290) + k, m.r16(0x8011B258u + k));
    for (u32 k = 0; k < 0x1A; k += 2) m.w16(frame.at(0x12B0) + k, m.r16(0x8011B274u + k));
    m.w16(t + 0x1532, 0);
    m.w16(t + 0x1534, 0);
    static constexpr u32 kPages[13] = {0x27, 0x2A, 0x2B, 0x2C, 0x3B, 0x37, 0x3A, 0x2D, 0x2E, 0x3C, 0x2F, 0x3D, 0x3E};
    for (u32 i = 0; i < 339; ++i) {
        const u32 e = table + 14 * i;
        m.w32(t + 4 * i, static_cast<u32>(m.r16s(e)));
        m.w32(t + 4 * i + 0x54C, static_cast<u32>(m.r16s(e + 2) + 0x2A8));
        m.w16(t + 2 * i + 0xA98, m.r16(e + 4));
        m.w16(t + 2 * i + 0xD3E, m.r16(e + 6));
        m.w8(t + i + 0xFE4, m.r8(e + 0xA));
        m.w8(t + i + 0x1137, m.r8(e + 0xC));
        m.w16(t + 0x1530, (m.r16(0x8011E8F8u) << 6) | ((static_cast<u32>(m.r32s(0x8011E8F4u) >> 4)) & 0x3Fu));
        const u32 page = static_cast<u32>(m.r16s(e + 8));
        if (page < 13) m.w16(t + 2 * i + 0x128A, kPages[page]);
    }
    m.w32(t + 0x1538, 0);
    return 0;
}

u32 Game::o3_pictures_upload() {
    cover(0x8011B690u);
    Frame frame(*this, 0x18);
    static constexpr u32 kSize[18] = {0x8011E85Cu, 0x8011E86Cu, 0x8011E87Cu, 0x8011E88Cu, 0x8011E89Cu, 0x8011E8ACu,
                                      0x8011E8BCu, 0x8011E8D4u, 0x8011E8E4u, 0x8011E8FCu, 0x8011E90Cu, 0x8011E91Cu,
                                      0x8011E92Cu, 0x8011E93Cu, 0x8011E94Cu, 0x8011E95Cu, 0x8011E96Cu, 0x8011E97Cu};
    static constexpr u32 kData[18] = {0x800CBFECu, 0x800CFBECu, 0x800D37ECu, 0x800D73ECu, 0x800DAFECu, 0x800E27ECu,
                                      0x800EA00Cu, 0x800F180Cu, 0x800F902Cu, 0x8010082Cu, 0x8010802Cu, 0x8010F82Cu,
                                      0x8011702Cu, 0x80117A4Cu, 0x801181CCu, 0x801189CCu, 0x801191CCu, 0x801195CCu};
    constexpr u32 kRects = 0x8011E99Cu;
    for (u32 i = 0; i < 18; ++i) {
        const u32 s = kSize[i], r = kRects + 8 * i;
        m.w16(r, m.r16(s + 8));
        m.w16(r + 2, m.r16(s + 0xC));
        m.w16(r + 4, m.r16(s));
        m.w16(r + 6, m.r16(s + 4));
        hw->load_image(r, kData[i]);
    }
    const u32 r = kRects + 0x90;
    m.w16(r + 4, 0x10);
    m.w16(r + 6, 1);
    m.w16(r, m.r16(0x8011E8F4u));
    m.w16(r + 2, m.r16(0x8011E8F8u));
    hw->load_image(r, 0x800F900Cu);
    return 0;
}

u32 Game::o3_phase0() {
    cover(0x8011BB7Cu);
    Frame frame(*this, 0x40);
    if (!m.r32(scene(m) + 4)) return 0;
    const u32 d = m.r32(kDisplay);
    m.w8(d + 0x10, 0);
    m.w32(d + 4, 1);
    m.w32(d + 0x14, 0);
    m.w8(m.r32(kDisplay) + 0x11, 0);
    m.w8(m.r32(kDisplay) + 0x12, 0);
    m.w32(m.r32(kDisplay), m.r32(m.r32(kDisplay)) | 9u);
    m.w32(scene(m), m.r32(scene(m)) | 0x10000000u);
    m.w32(kWork, mem_alloc(0x1894));
    m.w32(work(m) + 4, mem_alloc(0x2000));
    m.w32(work(m) + 8, mem_alloc(0x2000));
    {
        const u32 w = work(m);
        m.w16(w + 0x54, 0x140);
        m.w32(w, 0);
        m.w16(w + 0x56, 0xF0);
        m.w32(w + 0x5C, 0);
        m.w32(w + 0x58, static_cast<u32>(m.r16s(m.r32(kMovies) + 0xF0)));
    }
    const u32 rect = frame.at(0x10), screen = frame.at(0x18);
    m.w32(rect, m.r32(0x8011B2C8u));
    m.w32(rect + 4, m.r32(0x8011B2CCu));
    m.w32(work(m) + 0x60, mem_alloc(0x28000));
    hw->store_image(rect, m.r32(work(m) + 0x60));
    m.w16(rect, 0x1E0);
    m.w16(rect + 2, 0x100);
    m.w16(rect + 4, 0x60);
    m.w16(rect + 6, 0x100);
    m.w32(work(m) + 0x64, mem_alloc(0xC000));
    hw->store_image(rect, m.r32(work(m) + 0x64));
    m.w16(screen, 0);
    m.w16(screen + 2, 0);
    m.w16(screen + 4, 0x1E0);
    m.w16(screen + 6, 0xF0);
    hw->clear_image(screen, 0, 0, 0);
    m.w16(screen, 0);
    m.w16(screen + 2, 0x100);
    hw->clear_image(screen, 0, 0, 0);
    o3_pictures_upload();
    {
        const u32 w = work(m);
        m.w32(w + 0x68, 1);
        m.w32(w + 0x1890, m.r32(scene(m) + 0x24));
        o3_block_reset(w + 0x6C);
    }
    o3_credits_setup(work(m) + 0x354);
    return 0x4000000;
}

u32 Game::o3_player_open(u32 id, u32 mode, u32 base, u32 size) {
    cover(0x8011C05Cu);
    Frame frame(*this, 0x38);
    m.w32(kPlayer, base);
    const u32 ring = base + 0xD8, words = (size + 3) >> 2 << 2;
    const u32 d = frame.at(0x10);
    m.w32(d + 0xC, ring);
    m.w32(d + 4, id);
    m.w32(d + 8, words);
    m.w32(d, mode);
    o3_player_reset(d);
    cd_reset(0, 0);
    return ring + words;
}

u32 Game::o3_player_reset(u32 d) {
    cover(0x8011C100u);
    const u32 p = player(m);
    m.w32(p + 0xCC, m.r32(d + 4));
    m.w32(p + 0xD0, m.r32(d + 8));
    m.w32(p + 0xD4, m.r32(d + 0xC));
    m.w8(p + 0xA1, m.r8(d));
    for (const u32 o : {0x9Eu, 0x9Fu, 0xA0u, 0xAu, 0x1u}) m.w8(player(m) + o, 0);
    m.w32(player(m) + 0xC, 0);
    m.w32(player(m) + 4, 0);
    m.w8(player(m), 0);
    m.w8(player(m) + 0x10, 0);
    m.w16(player(m) + 8, 0);
    m.w8(player(m) + 0x10, 0);
    for (u32 bank = 0; bank < 2; ++bank)
        for (u32 cue = 0; cue < 5; ++cue) {
            const u32 c = cue_record(player(m), bank, cue);
            m.w8(c + 0x12, 0);
            m.w16(c + 0x14, 0);
            m.w16(c + 0x16, 0);
            for (u32 k = 0; k < 4; ++k) m.w16(c + 0x18 + 2 * k, 0xFFFF);
        }
    for (u32 bank = 0; bank < 2; ++bank)
        for (u32 k = 0; k < 20; ++k) m.w8(player(m) + 0xA2 + 20 * bank + k, 0);
    return 0;
}

u32 Game::o3_player_request() {
    cover(0x8011C260u);
    Frame frame(*this, 0x20);
    const u32 p = player(m);
    m.w32(frame.at(0x10), 0x8011C2A0u);
    return kcd_request_video(m.r32(p + 0xCC), m.r32(p + 0xD4), m.r32(p + 0xD0), 0x2000, 0x8011C2A0u);
}

u32 Game::o3_player_done() {
    cover(0x8011C2A0u);
    m.w8(player(m) + 0xA0, 1);
    return 1;
}

u32 Game::o3_player_stop() {
    cover(0x8011C2B4u);
    Frame frame(*this, 0x18);
    cd_reset(0, 0);
    return 1;
}

u32 Game::o3_player_first() {
    cover(0x8011C2D8u);
    return m.r8(player(m) + 0x9E) < 1 ? 1u : 0u;
}

u32 Game::o3_player_wait(u32 once) {
    cover(0x8011C2F0u);
    Frame frame(*this, 0x28);
    if (once) return o3_sector(player(m)) ? 1u : 0u;
    const u32 count = frame.at(0x10);
    m.w32(count, 0x78);
    do {
        if (o3_sector(player(m))) return 1;
        kcd_pump();
        hw->vsync(0);
        m.w32(count, m.r32(count) - 1);
    } while (m.r32s(count) >= 0);
    m.w8(player(m) + 0xA0, 1);
    return 1;
}

u32 Game::o3_sector(u32 p) {
    cover(0x8011C39Cu);
    Frame frame(*this, 0x28);
    const u32 data_at = frame.at(0x10), sector_at = frame.at(0x14);
    if (kcd_ring_get(data_at, sector_at)) return 0;
    const auto sector = [&] { return m.r32(sector_at); };
    if (!m.r16(sector() + 6)) m.w8(p + 0xA0, 1);
    const u32 flags = m.r8(sector() + 0xC), last = m.r8(sector() + 0xD), strip = m.r8(sector() + 0xF);
    const u32 base_cue = m.r8(sector() + 0x13) & 0xFu;
    if (!strip) {
        for (u32 k = 0, at = m.r32(data_at) + 0x14; k < 256; ++k, at += 2)
            if (m.r16s(at)) m.w16(at, m.r16(at) | 0x8000u);
    } else {
        const u32 data = m.r32(data_at);
        for (s32 i = 0; i < 5158; ++i) {
            const s32 skip = static_cast<s16>(i / 573);
            m.w32(data + 4 * static_cast<u32>(i), m.r32(data + 4 * static_cast<u32>(i + skip)));
        }
    }
    const auto record = [&](u32 cue) { return cue_record(p, m.r8(p + 0x9E), cue); };
    if (strip == base_cue + 7) {
        m.w8(p + 0x9E, m.r8(p + 0x9E) < 1 ? 1 : 0);
        if (last == 1) m.w8(p + 0x9E, 0);
        for (u32 cue = 0; cue < 5; ++cue) {
            m.w8(record(cue) + 0x12, 0);
            m.w16(record(cue) + 0x14, 0xFFFF);
            m.w16(record(cue) + 0x16, 0xFFFF);
            for (u32 k = 0; k < 4; ++k) m.w16(record(cue) + 0x18 + 2 * k, 0);
        }
    }
    if (strip - 2u < 16u) {
        const u32 c = strip - 2, at = p + 20 * m.r8(p + 0x9E) + 0xB1 - c;
        m.w8(at, m.r8(sector() + 0x13) >> 4);
        if (c < 4) m.w8(at + 4, m.r8(sector() + 0x13) >> 4);
    }
    {
        const u32 s = sector(), cue = m.r8(s + 0x10);
        if ((cue & 0xFu) == 1) {
            const u32 k = cue >> 4;
            m.w8(record(k) + 0x12, m.r8(s + 0x11) & 0xFu);
            m.w16(record(k) + 0x14, m.r16(s + 0x14));
            m.w16(record(k) + 0x16, m.r16(s + 0x16));
            for (u32 j = 0; j < 4; ++j) m.w16(record(k) + 0x18 + 2 * j, m.r16(s + 0x18 + 2 * j));
        }
    }
    if (!(flags & 0xFu)) {
        constexpr u32 kRects = 0x8011EA4Cu, kSources = 0x8011EA9Cu;
        const u32 rect = kRects + 8 * strip;
        if (strip) {
            const s32 x0 = m.r8(p + 0x9E) ? 0x200 : 0x140, n = static_cast<s32>(strip) - 1, row = n >> 2;
            m.w16(rect, static_cast<u32>(x0 - 40 * (n - 4 * row - 3)));
            m.w16(rect + 2, static_cast<u32>(-240 * (row - 1)));
            m.w16(rect + 4, 0x28);
            m.w16(rect + 6, 0xF0);
        } else {
            m.w16(rect, 0);
            m.w16(rect + 4, 0x100);
            m.w16(rect + 6, 1);
            m.w16(rect + 2, m.r8(p + 0x9E) ? 0x1FFu : 0x1FEu);
        }
        m.w32(kSources + 4 * strip, m.r32(data_at) + 0x14);
        hw->load_image(rect, m.r32(data_at) + 0x14);
    }
    if ((flags >> 4) == 1) m.w8(p, last == 1 ? 2 : 1);
    kcd_ring_free(m.r32(sector_at));
    return 1;
}

u32 Game::o3_movie_tiles(u32 p, u32 pk, u32 ot) {
    cover(0x8011D59Cu);
    Frame frame(*this, 8);
    const u32 effect = m.r8(p + 0xA);
    s32 level = 0x80;
    if (effect == 4 || effect == 5) {
        const u32 c = cue_record(p, m.r8(p + 0x9F), m.r8(p + 0x10));
        const s32 length = m.r16s(c + 0x16) - m.r16s(c + 0x14), t = static_cast<s32>(m.r32(p + 0xC) << 7);
        s32 v = effect == 4 ? static_cast<s32>(wrap::div(wrap::neg(t), length).lo) + 0x80
                            : static_cast<s32>(wrap::div(t, length).lo);
        v = static_cast<s16>(v);
        level = v > 128 ? 128 : v < 0 ? 0 : v;
    }
    const u32 colour = grey(static_cast<u32>(level));
    struct Tile { u32 w, h, x, y, page0, page1; };
    static constexpr Tile kTiles[4] = {{0x100, 0x100, 0, 0, 0x85, 0x88}, {0x40, 0x100, 0x100, 0, 0x87, 0x8A},
                                       {0x100, 0xE0, 0, 0x100, 0x95, 0x98}, {0x40, 0xE0, 0x100, 0x100, 0x97, 0x9A}};
    for (u32 t = 0; t < 4; ++t, pk += 24) {
        const Tile& tile = kTiles[t];
        const bool second = m.r8(p + 0x9F) != 0;
        m.w32(pk + 4, second ? tile.page1 : tile.page0);
        m.w32(pk + 0xC, pair(tile.y, tile.x));
        m.w32(pk + 0x10, (second ? 0x7FC0u : 0x7F80u) << 16);
        m.w32(pk + 8, colour);
        m.w8(pk + 0xB, 0x64);
        m.w32(pk + 0x14, pair(tile.h, tile.w));
        m.w8(pk + 7, 0xE1);
        m.w8(pk + 6, 0);
        const u32 entry = ot + (m.r16s(p + 8) == 0x17 ? 8u : 0x10u);
        m.w32(pk, m.r32(entry) | 0x5000000u);
        m.w32(entry, pk & 0xFFFFFFu);
    }
    return pk;
}

u32 Game::o3_cue(u32 p, u32 pk, u32 ot) {
    cover(0x8011D474u);
    Frame frame(*this, 0x18);
    {
        const u32 c = cue_record(p, m.r8(p + 0x9F), m.r8(p + 0x10));
        if (m.r32(p + 4) == static_cast<u32>(m.r16s(c + 0x16) + m.r16s(c + 0x1E))) {
            m.w8(p + 0xA, 0);
            m.w32(p + 0xC, 0);
            m.w8(p + 0x10, m.r8(p + 0x10) + 1);
        }
    }
    {
        const u32 c = cue_record(p, m.r8(p + 0x9F), m.r8(p + 0x10));
        if (m.r32(p + 4) == static_cast<u32>(m.r16s(c + 0x14))) {
            m.w32(p + 0xC, 0);
            m.w8(p + 0xA, m.r8(c + 0x12));
        }
    }
    const u32 effect = m.r8(p + 0xA);
    if (effect >= 6) return pk;
    pk = o3_movie_tiles(p, pk, ot);
    if (effect) m.w32(p + 0xC, m.r32(p + 0xC) + 1);
    return pk;
}

u32 Game::o3_trail(u32 b, u32 ot, u32 pk) {
    cover(0x8011D8F4u);
    Frame frame(*this, 0x20);
    const u32 local = frame.at(0);
    m.w32(local, 0x30FC);
    m.w32(local + 4, 0x3AC0);
    m.w16(local + 8, 0x136);
    m.w16(local + 0xA, 0xFFE0);
    m.w16(local + 0x10, 0xB4);
    m.w16(local + 0x12, 0x50);
    const auto segment = [&] { return m.r32(b + 0x10); };
    const auto clock = [&] { return m.r32s(b + 0x2E4); };
    const auto start = [&] { return m.r32s(local + 8 * segment()); };
    const auto end = [&] { return m.r32s(local + 8 * segment() + 4); };
    const auto add_point = [&](u32 brightness) {
        const u32 x = m.r16(b), n = m.r32(b + 0x2D0) + 1;
        m.w32(b + 0x2D0, n);
        m.w16(b + 0x14 + 2 * n, x);
        m.w16(b + 0xDC + 2 * m.r32(b + 0x2D0), m.r16(b + 2));
        m.w8(b + 0x26C + m.r32(b + 0x2D0), brightness);
        m.w16(b + 0x1A4 + 2 * m.r32(b + 0x2D0), 1);
    };
    if (!(clock() < start()) && clock() < end()) {
        const u32 k = segment(), xs = local + 8 + 4 * k, ys = local + 0x10 + 4 * k;
        const u32 time = local + 8 * k;
        const s32 span = static_cast<s16>(m.r16(time + 4) - m.r16(time));
        const s32 dx = static_cast<s16>(m.r16(xs + 2) - m.r16(xs)), dy = static_cast<s16>(m.r16(ys + 2) - m.r16(ys));
        const s32 elapsed = static_cast<s16>(m.r16(b + 0x2E4) - m.r16(time));
        if (elapsed == 0x41A) {
            m.w8(b + 0x2D8, 1);
            m.w32(b + 0x2DC, m.r32(b + 0x2E4));
        }
        const s32 ox = static_cast<s32>(wrap::div(wrap::mul(dx, elapsed), span).lo);
        const s32 oy = static_cast<s32>(wrap::div(wrap::mul(dy, elapsed), span).lo);
        m.w16(b, m.r16(local + 8 + 4 * segment()) + static_cast<u32>(ox));
        m.w16(b + 2, m.r16(local + 0x10 + 4 * segment()) + static_cast<u32>(oy));
        m.w32(pk + 4, m.r32(b + 0xC));
        m.w32(pk + 8, (static_cast<u32>(m.r16s(b + 2)) << 16) | m.r16(b));
        m.w32(pk + 0xC, (static_cast<u32>(m.r16s(b + 6)) << 16) | static_cast<u32>(m.r16s(b + 4)));
        pk += 0x10;
        if (elapsed % 100 == 0 && elapsed != 0) add_point(0x40);
    }
    if (clock() == start()) {
        m.w32(b + 0x2D0, 0);
        m.w32(b + 0x2D4, 1);
        m.w8(b + 0x2D8, 0);
        m.w32(b + 0x2DC, 0);
        m.w16(b + 0x14, m.r16(local + 8 + 4 * segment()));
        m.w16(b + 0xDC, m.r16(local + 0x10 + 4 * segment()));
        m.w8(b + 0x26C, 0x40);
        m.w16(b + 0x1A4, 1);
    }
    if (clock() == end()) {
        m.w32(b + 0x10, segment() + 1);
        if (!m.r8(b + 0x2D8)) {
            m.w8(b + 0x2D8, 1);
            m.w32(b + 0x2DC, m.r32(b + 0x2E4));
        }
        add_point(0x40);
    }
    if (m.r8(b + 0x2D8) == 1) {
        const s32 age = clock() - m.r32s(b + 0x2DC);
        s32 a = age;
        for (s32 i = 0; i < m.r32s(b + 0x2D4); ++i, a -= 100) {
            s32 v = 0x40 - wrap::mul(a, 61) / 200;
            if (static_cast<s16>(v) < 3) v = 3;
            m.w8(b + 0x26C + static_cast<u32>(i), static_cast<u32>(v));
            if (a >= 100) {
                s32 w = wrap::mul(a - 100, 2) / 100 + 1;
                if (static_cast<s16>(w) >= 4) w = 3;
                m.w16(b + 0x1A4 + 2 * static_cast<u32>(i), static_cast<u32>(w));
            }
        }
        if (age % 100 == 0 && age != 0 && !(m.r32s(b + 0x2D0) < m.r32s(b + 0x2D4)))
            m.w32(b + 0x2D4, m.r32(b + 0x2D4) + 1);
    }
    const auto half = [&](u32 at) { const s32 v = m.r16s(at); return static_cast<u32>((v + (v < 0 ? 1 : 0)) >> 1); };
    const auto xy = [&](u32 x, u32 y) { return (y << 16) | (x & 0xFFFFu); };
    const auto quad = [&](u32 q, u32 c0, u32 c1) {
        m.w32(q + 4, c0 | 0x3A000000u);
        m.w32(q + 0xC, c1);
        m.w32(q + 0x14, grey(3));
        m.w32(q + 0x1C, grey(3));
    };
    const auto link_quad = [&](u32 q) {
        m.w32(q, m.r32(ot + 0xC) | 0x8000000u);
        m.w32(ot + 0xC, (q << 8) >> 8);
    };
    u32 q = pk;
    if (!(clock() < start()) && !(end() < clock())) {
        for (u32 side = 0; side < 2; ++side, q += 0x24) {
            const u32 n = m.r32(b + 0x2D0), hx = half(b + 4), hy = half(b + 6);
            quad(q, grey(0x40), grey(m.r8(b + 0x26C + n)));
            const u32 cx = m.r16(b) + hx, cy = m.r16(b + 2) + hy;
            const u32 px = m.r16(b + 0x14 + 2 * n) + hx, py = m.r16(b + 0xDC + 2 * n) + hy;
            m.w32(q + 8, xy(cx, cy));
            m.w32(q + 0x10, xy(px, py));
            const u32 w = m.r16(b + 0x1A4 + 2 * m.r32(b + 0x2D0));
            if (side == 0) {
                m.w32(q + 0x18, xy(cx + 1, cy - 1));
                m.w32(q + 0x20, xy(w + px, py - w));
            } else {
                m.w32(q + 0x18, xy(cx - 1, cy + 1));
                m.w32(q + 0x20, xy(px - w, w + py));
            }
            link_quad(q);
        }
    }
    if (m.r32s(b + 0x2D0) > 0)
        for (s32 i = 0; i < m.r32s(b + 0x2D0); ++i)
            for (u32 side = 0; side < 2; ++side, q += 0x24) {
                const u32 a = b + 2 * static_cast<u32>(i), c = a + 2;
                quad(q, grey(m.r8(b + 0x26C + static_cast<u32>(i) + 1)), grey(m.r8(b + 0x26C + static_cast<u32>(i))));
                const u32 hx = half(b + 4), hy = half(b + 6);
                const u32 x1 = m.r16(c + 0x14) + hx, y1 = m.r16(c + 0xDC) + hy, w1 = m.r16(c + 0x1A4);
                const u32 x0 = m.r16(a + 0x14) + hx, y0 = m.r16(a + 0xDC) + hy, w0 = m.r16(a + 0x1A4);
                m.w32(q + 8, xy(x1, y1));
                m.w32(q + 0x10, xy(x0, y0));
                if (side == 0) {
                    m.w32(q + 0x18, xy(w1 + x1, y1 - w1));
                    m.w32(q + 0x20, xy(w0 + x0, y0 - w0));
                } else {
                    m.w32(q + 0x18, xy(x1 - w1, w1 + y1));
                    m.w32(q + 0x20, xy(x0 - w0, w0 + y0));
                }
                link_quad(q);
            }
    if (m.r32s(b + 0x2D0) < static_cast<s32>(m.r8(b + 0x2D8))) {
        m.w8(b + 0x2D8, 0);
        m.w32(b + 0x2DC, 0);
    }
    link_mode(m, ot + 0xC, q, 0xE1000020u);
    return q + 8;
}

u32 Game::o3_credits(u32 t, u32 ot, u32 pk) {
    cover(0x8011E474u);
    Frame frame(*this, 0x10);
    m.w32(0x8011EAC4u, 0x202);
    const u32 step = m.r8(0x8011EAC4u);
    for (s32 i = m.r16s(t + 0x1532); i < 338; ++i)
        if (!wrap::div(m.r32s(t + 0x1538), 2).hi)
            m.w32(t + 4 * static_cast<u32>(i) + 0x54C, m.r32(t + 4 * static_cast<u32>(i) + 0x54C) - step);
    for (s32 i = m.r16s(t + 0x1532); i < 338 && m.r32s(t + 4 * static_cast<u32>(i) + 0x54C) < 501; ++i)
        m.w16(t + 0x1534, static_cast<u32>(i));
    {
        const s32 last = m.r16s(t + 0x1534);
        if (last > 0)
            for (s32 i = 0; m.r32s(t + 4 * static_cast<u32>(i) + 0x54C) < -49;) {
                m.w16(t + 0x1532, static_cast<u32>(i + 1));
                if (!(++i < last)) break;
            }
    }
    u32 q = pk;
    for (s32 i = m.r16s(t + 0x1532); !(m.r16s(t + 0x1534) < i); ++i, q += 0x1C) {
        const u32 k = static_cast<u32>(i);
        m.w32(q + 4, 0x66808080u);
        m.w32(q + 8, pair(m.r16(t + 4 * k + 0x54C), m.r16(t + 4 * k)));
        m.w32(q + 0xC, (m.r16(t + 0x1530) << 16) | (m.r8(t + k + 0x1137) << 8) | m.r8(t + k + 0xFE4));
        m.w32(q + 0x10, pair(m.r16(t + 2 * k + 0xD3E), m.r16(t + 2 * k + 0xA98)));
        link_fresh(m, ot, q, 4);
        link_mode(m, ot, q + 0x14, (m.r16(t + 2 * k + 0x128A) & 0x9FFu) | 0xE1000200u);
    }
    link_mode(m, ot, q, 0xE1000220u);
    return q + 8;
}

u32 Game::o3_frame() {
    cover(0x8011CB5Cu);
    Frame frame(*this, 0x28);
    {
        const u32 p = player(m), state = m.r8(p);
        if (state) {
            if (state == 1) {
                m.w8(p + 0x9F, m.r8(p + 0x9F) ^ 1);
                m.w16(player(m) + 8, m.r16(player(m) + 8) + 1);
            } else {
                m.w8(p + 0x9F, 0);
                m.w16(player(m) + 8, 0);
                m.w8(player(m) + 1, 1);
                m.w32(work(m) + 0x1890, m.r32(scene(m) + 0x24));
            }
            m.w8(player(m), 0);
            m.w32(player(m) + 4, 0);
            m.w8(player(m) + 0x10, 0);
            m.w8(player(m) + 0xA, 0);
        }
    }
    const auto sprite = [&](u32 q, u32 i, u32 colour, bool fixed_y) {
        const u32 w = work(m), t = w + 0x354;
        m.w32(q + 4, colour);
        m.w32(q + 8, fixed_y ? (static_cast<u32>(static_cast<s32>(static_cast<s16>(m.r16(t + 4 * i)))) | 0x1900000u)
                             : pair(m.r16(t + 4 * i + 0x54C), m.r16(t + 4 * i)));
        m.w32(q + 0xC, 0x78140000u | (m.r8(t + i + 0x1137) << 8) | m.r8(t + i + 0xFE4));
        m.w32(q + 0x10, pair(m.r16(t + 2 * i + 0xD3E), m.r16(t + 2 * i + 0xA98)));
        link_fresh(m, m.r32(w + 0x40), q, 4);
        link_mode(m, m.r32(work(m) + 0x40), q + 0x14, (m.r16(t + 2 * i + 0x128A) & 0x9FFu) | 0xE1000200u);
    };
    const auto banner = [&](u32 q, u32 colour) {
        m.w32(q + 0x10, 0x140100);
        m.w32(q + 8, 0xC80020);
        m.w32(q + 0xC, 0x78141400u);
        m.w32(q + 4, colour);
        link_fresh(m, m.r32(work(m) + 0x40), q, 4);
        link_mode(m, m.r32(work(m) + 0x40), q + 0x14, 0xE100002Fu);
        m.w32(work(m) + 0x3C, q + 0x1C);
    };
    const u32 p = player(m);
    if (m.r8(p + 1) == 1) {
        {
            const u32 w = work(m);
            m.w32(w + 0x350, m.r32(scene(m) + 0x24) - m.r32(w + 0x1890));
            m.w32(w + 0x188C, m.r32(scene(m) + 0x24) - m.r32(w + 0x1890));
            m.w32(w + 0x3C, o3_cue(p, m.r32(w + 0x3C), m.r32(w + 0x40)));
        }
        const u32 q = o3_trail(work(m) + 0x6C, m.r32(work(m) + 0x40), m.r32(work(m) + 0x3C));
        m.w32(work(m) + 0x3C, q);
        if (!(m.r32s(work(m) + 0xDE0) < 190)) {
            m.w32(work(m) + 0x3C, o3_credits(work(m) + 0x354, m.r32(work(m) + 0x40), q));
        } else if (m.r16s(player(m) + 8) < 23) {
            sprite(q, 336, 0x66808080u, false);
            sprite(q + 0x1C, 337, 0x66808080u, false);
            m.w32(work(m) + 0x3C, q + 0x38);
        } else if (m.r32s(player(m) + 4) < 128) {
            const u32 c = grey((0u - 128u - m.r8(player(m) + 4)) & 0xFFu) | 0x66000000u;
            sprite(q, 336, c, false);
            sprite(q + 0x1C, 337, c, false);
            m.w32(work(m) + 0x3C, q + 0x38);
        } else {
            const u32 n = m.r32(player(m) + 4);
            u32 c = 0;
            if (n - 200u < 0x40u) c = (n - 200u) << 1;
            else if (n - 264u < 0x2C8u) c = 0x80;
            else if (n - 976u < 0x40u) c = 0u - 128u - ((n - 976u) << 1);
            sprite(q, 338, grey(c & 0xFFu) | 0x66000000u, true);
            m.w32(work(m) + 0x3C, q + 0x1C);
        }
        if (m.r16s(player(m) + 8) == 0 && m.r32s(player(m) + 4) < 128)
            banner(m.r32(work(m) + 0x3C), grey((0u - 128u - m.r8(player(m) + 4)) & 0xFFu) | 0x66000000u);
    } else {
        const u32 n = m.r32(p + 4);
        u32 colour;
        if (n - 180u < 0x80u) colour = grey((m.r8(p + 4) + 0x4Cu) & 0xFFu) | 0x66000000u;
        else colour = m.r32s(p + 4) < 308 ? 0x66000000u : 0x66808080u;
        banner(m.r32(work(m) + 0x3C), colour);
    }
    o3_sector(player(m));
    const u32 q = player(m);
    m.w32(q + 4, m.r32(q + 4) + 1);
    return (m.r8(q + 0xA0) ^ 1u) < 1 ? 1u : 0u;
}

u32 Game::o3_step() {
    cover(0x8011BD94u);
    Frame frame(*this, 0x38);
    switch (m.r32(work(m))) {
    case 0: {
        const u32 base = mem_peek(0);
        const u32 w = work(m);
        for (u32 k = 0; k < 8; ++k) m.w32(frame.at(0x10 + 4 * k), static_cast<u32>(m.r16s(w + 0x44 + 2 * k)));
        o3_player_open(m.r32(w + 0x58), m.r32(w + 0x5C), base, 0xD740);
        o3_player_request();
        m.w32(work(m), 1);
        return 0;
    }
    case 1:
        if (!m.r32(scene(m) + 4)) {
            m.w32(scene(m) + 4, 0);
            if (o3_player_wait(1)) m.w32(work(m), 2);
            else if (m.r8(player(m) + 0xA0) == 1) m.w32(work(m), 3);
        }
        m.w32(player(m) + 4, m.r32(player(m) + 4) + 1);
        return 0;
    case 2:
        if (m.r32(scene(m) + kPressed) & 0x40) m.w8(player(m) + 0xA0, 1);
        if (o3_frame()) m.w32(work(m), 3);
        return 0;
    case 3: {
        o3_player_stop();
        o3_player_first();
        const u32 r = 0x8011EA3Cu;
        m.w16(r, 0);
        m.w16(r + 4, 0x140);
        m.w16(r + 2, 0);
        m.w16(r + 6, 0xF0);
        hw->clear_image(r, 0, 0, 0);
        if (!m.r32(scene(m) + 4)) m.w32(work(m), 4);
        return 0;
    }
    case 4: {
        o3_player_first();
        const u32 r = 0x8011EA44u;
        m.w16(r, 0);
        m.w16(r + 4, 0x140);
        m.w16(r + 2, 0);
        m.w16(r + 6, 0xF0);
        hw->clear_image(r, 0, 0, 0);
        m.w32(work(m), 5);
        return 0;
    }
    case 5: {
        m.w32(scene(m), m.r32(scene(m)) & 0xEFFFFFFFu);
        const u32 d = m.r32(kDisplay), flags = m.r32(d);
        m.w32(d + 4, 0);
        m.w32(d + 0x14, 2);
        m.w32(d, flags | 5u);
        return 1;
    }
    default: return 0;
    }
}

u32 Game::o3_end() {
    cover(0x8011E7FCu);
    Frame frame(*this, 0x18);
    cd_reset(0, 0);
    hw->load_image(0x8011E98Cu, m.r32(work(m) + 0x60));
    hw->load_image(0x8011E994u, m.r32(work(m) + 0x64));
    return 1;
}

u32 Game::o3_phase1() {
    cover(0x8011E744u);
    Frame frame(*this, 0x20);
    const u32 b = m.r32(scene(m) + 4), w = work(m);
    const u32 ot = w + 24 * b + 0xC;
    m.w32(w + 0x40, ot);
    m.w32(w + 0x3C, m.r32(w + 4 * b + 4));
    clear_otag_r(ot, 6);
    if (!o3_step()) {
        hw->draw_otag(m.r32(work(m) + 0x40) + 0x14);
        return 0;
    }
    o3_end();
    return m.r32(scene(m) + 0x3A0) | 0x60000000u;
}
}

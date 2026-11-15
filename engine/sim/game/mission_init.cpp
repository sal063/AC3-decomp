#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDisplay = 0x800BCE48u;
constexpr u32 kMsnFile = 0x800BCF50u;
constexpr u32 kMg = 0x800BCE98u;
constexpr u32 kRadarBlips = 0x800C67A0u;
constexpr u32 kPlayerBlocks = 0x800BD088u;
constexpr u32 kCamState = 0x800BD098u;
constexpr u32 kHeapTop = 0x801FB700u;
}

u32 Game::rot_matrix_yxz_gte(u32 ang, u32 out) {
    cover(0x800A7718u);
    constexpr u32 kTable = 0x800B7104u;
    auto sc = [&](s32 a, s32& s, s32& c) {
        const u32 e = m.r32(kTable + 4 * (static_cast<u32>(a < 0 ? -a : a) & 0xFFFu));
        const s32 v = static_cast<s16>(e);
        s = a < 0 ? -v : v;
        c = static_cast<s32>(e) >> 16;
    };
    s32 sx, cx, sy, cy, sz, cz;
    sc(m.r16s(ang), sx, cx);
    sc(m.r16s(ang + 2), sy, cy);
    sc(m.r16s(ang + 4), sz, cz);
    auto mul12 = [](s32 a, s32 b) { return wrap::mul(a, b) >> 12; };
    m.w16(out + 0xA, static_cast<u32>(-sx));
    m.w16(out + 4, static_cast<u32>(mul12(sy, cx)));
    m.w16(out + 0x10, static_cast<u32>(mul12(cy, cx)));
    m.w16(out + 6, static_cast<u32>(mul12(sz, cx)));
    m.w16(out + 8, static_cast<u32>(mul12(cz, cx)));
    const s32 sysx = mul12(sy, sx), cysx = mul12(cy, sx);
    m.w16(out + 0, static_cast<u32>(mul12(cy, cz) + mul12(sysx, sz)));
    m.w16(out + 2, static_cast<u32>(-mul12(cy, sz) + mul12(sysx, cz)));
    m.w16(out + 0xE, static_cast<u32>(mul12(sy, sz) + mul12(cysx, cz)));
    m.w16(out + 0xC, static_cast<u32>(-mul12(sy, cz) + mul12(cysx, sz)));
    return out;
}

u32 Game::apply_matrix_sv(u32 mat, u32 v, u32 out) {
    cover(0x800A6E78u);
    for (u32 k = 0; k < 5; ++k) gte.ctc2(k, m.r32(mat + 4 * k));
    gte.mtc2(0, m.r32(v));
    gte.mtc2(1, m.r32(v + 4));
    gte.op(0x486012);
    m.w16(out, gte.mfc2(9));
    m.w16(out + 2, gte.mfc2(10));
    m.w16(out + 4, gte.mfc2(11));
    return out;
}

void Game::set_geom_screen(s32 h) {
    cover(0x800A7138u);
    gte.ctc2(26, static_cast<u32>(h));
}

u32 Game::heap_set(u32 at) {
    cover(0x80027538u);
    Frame f(*this, 0x18);
    const u32 sc = m.r32(g::kScene);
    const u32 base = m.r32(sc + 24);
    m.w32(sc + 28, at);
    if (at < base) return 0;
    return at;
}

u32 Game::heap_free() {
    cover(0x80027584u);
    return kHeapTop - m.r32(m.r32(g::kScene) + 28);
}

u32 Game::mission_init() {
    cover(0x80032754u);
    Frame f(*this, 0x88);
    if (!m.r16(m.r32(g::kScene) + 940)) loading_frame();
    {
        const u32 d = m.r32(kDisplay);
        m.w32(d + 20, 2);
        m.w32(d + 4, 0);
        m.w32(d, m.r32(d) | 5u);
        const u32 sc = m.r32(g::kScene), x = m.r32(g::kExit);
        m.w32(sc, m.r32(sc) | 0x10000000u);
        m.w32(x + 112, m.r32(sc + 944));
        m.w32(x + 116, m.r32(sc + 948));
    }
    heap_set(m.r32(m.r32(g::kExit) + 92));
    const u32 w = mem_alloc(700);
    m.w32(g::kWorld, w);
    m.w32(w, 0);
    m.w32(w + 4, 0);
    m.w32(w + 8, 0);
    m.w32(w + 12, 0);
    m.w16(w + 668, 0);
    m.w16(w + 670, 0);
    for (u32 k = 0; k < 3; ++k) {
        m.w32(w + 672 + 8 * k, 0x80000001u);
        m.w32(w + 676 + 8 * k, 0x7FFFFFFFu);
    }
    {
        const u32 x = m.r32(g::kExit);
        for (u32 k = 0; k < 68; k += 4) m.w32(w + 580 + k, m.r32(x + 24 + k));
        m.w32(m.r32(w + 592) + 12, m.r32(x + 8));
    }
    const u32 slots = mem_alloc(256);
    m.w32(g::kSlots, slots);
    m.w32(g::kGroundSlots, slots + 128);
    for (u32 i = 0; i < 2; ++i) m.w32(m.r32(g::kSlots) + 4 * i, mem_alloc(88));
    for (u32 i = 2; i < 32; ++i) m.w32(m.r32(g::kSlots) + 4 * i, mem_alloc(92));
    for (u32 i = 0; i < 32; ++i) m.w32(m.r32(g::kGroundSlots) + 4 * i, mem_alloc(148));
    for (u32 i = 0; i < 64; ++i) {
        const u32 e = slot(i);
        m.w32(e, 0);
        m.w8(e + 4, 0);
        m.w8(e + 5, 0);
        m.w16(e + 6, 0);
        m.w32(e + 8, 0);
        m.w32(e + 12, 0);
        m.w32(e + 16, 0);
        m.w16(e + 24, 0);
        m.w16(e + 26, 0);
        m.w16(e + 28, 0);
    }
    for (u32 i = 0; i < 64; ++i) {
        const u32 e = slot(i);
        for (u32 k : {32u, 36u, 40u, 56u, 60u, 64u, 80u, 84u}) m.w32(e + k, 0);
        for (u32 k : {48u, 50u, 52u, 72u, 74u, 76u, 78u}) m.w16(e + k, 0);
    }
    m.w8(slot(0) + 5, m.r8(slot(0) + 5) | 1u);
    {
        const u32 wo = m.r32(g::kWorld);
        const u32 md = m.r32(wo + 580);
        m.w32(wo + 40, wo + 184);
        const u32 angles = m.r32(m.r32(g::kExit) + 12) + 4;
        m.w32(f.at(0x10), angles);
        view_init(m.r32(wo + 40), 296, static_cast<u32>(m.r16s(md + 12)), static_cast<u32>(m.r16s(md + 14)), angles);
    }
    {
        const u32 wo = m.r32(g::kWorld);
        for (u32 k : {72u, 68u, 64u, 104u, 100u, 96u}) m.w32(wo + k, 0);
    }
    set_geom_offset(0, 0);
    {
        const u32 v = m.r32(m.r32(g::kWorld) + 40);
        set_geom_screen(m.r16s(v + 36));
        depth_cue(m.r16s(v + 40), m.r16s(v + 42), m.r16s(v + 36));
    }
    {
        const u32 t = mem_alloc(36);
        const u32 wo = m.r32(g::kWorld);
        m.w32(wo + 648, t);
        const u32 md = m.r32(wo + 580);
        fog_table(m.r16s(md + 12), m.r16s(md + 14), t);
    }
    {
        const u32 s3 = m.r32(g::kWorld);
        const u32 z = f.at(0x18), ang = f.at(0x20), mat = f.at(0x28);
        m.w16(z, 0);
        m.w16(z + 2, 0);
        m.w16(z + 4, 0x1000);
        const u32 md = m.r32(s3 + 580);
        m.w16(s3 + 0x8C, m.r16(md + 0x40));
        m.w16(s3 + 0x92, m.r16(md + 0x42));
        m.w16(s3 + 0x98, m.r16(md + 0x44));
        m.w16(s3 + 0x8E, m.r16(md + 0x50));
        m.w16(s3 + 0x94, m.r16(md + 0x52));
        m.w16(s3 + 0x90, 0);
        m.w16(s3 + 0x96, 0);
        m.w16(s3 + 0x9C, 0);
        m.w16(s3 + 0x9A, m.r16(md + 0x54));
        for (u32 k = 0; k < 18; k += 2) m.w16(s3 + 0x6C + k, 0);
        m.w32(s3 + 0xAC, static_cast<u32>(m.r16s(md + 0x46)));
        m.w32(s3 + 0xB0, static_cast<u32>(m.r16s(md + 0x48)));
        m.w32(s3 + 0xB4, static_cast<u32>(m.r16s(md + 0x4A)));
        m.w16(ang, m.r16(md + 0x3C));
        m.w16(ang + 4, 0);
        m.w16(ang + 2, m.r16(md + 0x3E));
        rot_matrix_yxz_gte(ang, mat);
        apply_matrix_sv(mat, z, s3 + 0x6C);
        m.w16(ang, m.r16(md + 0x4C));
        m.w16(ang + 4, 0);
        m.w16(ang + 2, m.r16(md + 0x4E));
        rot_matrix_yxz_gte(ang, mat);
        apply_matrix_sv(mat, z, s3 + 0x72);
    }
    hud_marks_templates();
    world_setup();
    {
        const u32 tbl = f.at(0x48);
        for (u32 k = 0; k < 32; k += 4) m.w32(tbl + k, m.r32(0x800234F8u + k));
        m.w32(tbl + 32, m.r32(0x80023518u));
        m.w8(tbl + 36, m.r8(0x8002351Cu));
        m.w8(tbl + 37, m.r8(0x8002351Du));
        const u32 p = slot(0);
        m.w8(p + 4, 0);
        m.w16(p + 72, m.r8(tbl + static_cast<u32>(m.r16s(m.r32(g::kScene) + 910))));
    }
    hud_setup();
    trails_alloc();
    effects_setup();
    weapons_alloc();
    player_setup(slot(0));
    player_start();
    for (u32 i = 2; i < 64; ++i) {
        const u32 e = slot(i);
        const u32 fl = m.r32(e);
        if (!(fl & 0x4000u)) continue;
        const u32 k = m.r8(e + 4);
        const u32 sc = m.r32(g::kScene), wo = m.r32(g::kWorld);
        s32 v;
        if ((fl & 0x20u) && !(m.r8(e + 5) & 2u)) {
            if (static_cast<s32>(k) >= m.r32s(sc + 0x1044)) continue;
            v = m.r16s(m.r32(m.r32(wo + 600) + 4 * k) + 2);
        } else {
            if (static_cast<s32>(4 * k) >= m.r32s(sc + 0x103C)) continue;
            v = static_cast<s16>(m.r16(m.r32(m.r32(wo + 592) + 16 * k) + 2)) >> 2;
            if (v < 15) v = 15;
        }
        if (m.r32(e) & 0x20u) {
            if (m.r16s(e + 0x4A) <= 0) m.w16(e + 0x4A, static_cast<u32>(v) - m.r16(e + 0x4A));
            if (m.r16s(e + 0x4C) <= 0) m.w16(e + 0x4C, static_cast<u32>(v) - m.r16(e + 0x4C));
        } else {
            m.w16(e + 0x4A, m.r16(e + 0x4A) + static_cast<u32>(v));
            m.w16(e + 0x4C, m.r16(e + 0x4C) + static_cast<u32>(v));
        }
    }
    for (u32 i = 0; i < 2; ++i) {
        m.w16(slot(i) + 0x4A, 0x36);
        m.w16(slot(i) + 0x4C, 0x36);
    }
    view_flag(1);
    mission_view_limits(static_cast<u32>(m.r16s(m.r32(g::kScene) + 908)));
    const u32 free_at = mem_peek(0);
    m.w32(m.r32(g::kWorld) + 696, free_at);
    if (m.r8(m.r32(g::kScene) + 933)) {
        display_buffers_alloc(free_at);
        const u32 x = m.r32(g::kExit);
        m.w32(x + 112, 0x4000000u);
        m.w32(x + 116, 0);
    } else {
        display_buffers_alloc_recorded(free_at);
    }
    {
        const u32 x = m.r32(g::kExit), wo = m.r32(g::kWorld);
        m.w32(wo + 4, m.r32(x + 112));
        m.w32(wo + 8, m.r32(x + 116));
    }
    return phase_change(0, 0);
}

void Game::display_buffers_alloc(u32 at) {
    cover(0x800324ACu);
    Frame f(*this, 0x20);
    heap_set(at);
    for (u32 i = 0; i < 2; ++i) {
        m.w32(m.r32(g::kWorld) + 24 + 4 * i, mem_alloc(0x4000));
        m.w32(m.r32(g::kWorld) + 32 + 4 * i, mem_alloc(0x1B800));
    }
}

void Game::display_buffers_alloc_recorded(u32 at) {
    cover(0x800323F8u);
    Frame f(*this, 0x20);
    heap_set(at);
    for (u32 i = 0; i < 2; ++i) {
        m.w32(m.r32(g::kWorld) + 24 + 4 * i, mem_alloc(0x4000));
        m.w32(m.r32(g::kWorld) + 32 + 4 * i, mem_alloc(0x1B800));
    }
    for (u32 i = 0; i < 64; ++i) {
        const u32 e = slot(i);
        const u32 fl = m.r32(e);
        if (fl & 0x4000u) m.w32(e, fl | 0x2000u);
    }
    recorder_alloc_heap();
}

void Game::recorder_alloc_heap() {
    cover(0x8007DB30u);
    Frame f(*this, 0x18);
    const u32 r = mem_alloc(0x2054);
    m.w32(0x800BD0B8u, r);
    m.w32(r, 0);
    m.w32(r + 0x1018, 0);
    m.w32(r + 0x184C, 0xFFFFFFFFu);
    const u32 size = heap_free() - 0x8000u;
    m.w32(m.r32(0x800BD0B8u) + 0x2050, size);
    const u32 ring = mem_alloc(size);
    const u32 q = m.r32(0x800BD0B8u);
    m.w32(q + 4, ring);
    m.w32(q + 8, ring + ((m.r32(q + 0x2050) >> 2) << 2) - 0x40);
    m.w32(q + 16, m.r32(q + 4));
    m.w32(q + 12, m.r32(q + 4));
    for (u32 j = 0; j < 64; ++j) m.w32(q + 0x18 + 32 * j, 0);
}

void Game::mission_view_limits(u32 mission) {
    cover(0x80032518u);
    const u32 w = m.r32(g::kWorld);
    const u32 v = m.r32(w + 40);
    auto world_box = [&](u32 a, u32 b, u32 c, u32 d, u32 e, u32 f2) {
        m.w32(w + 0x2A0, a);
        m.w32(w + 0x2A4, b);
        m.w32(w + 0x2A8, c);
        m.w32(w + 0x2AC, d);
        m.w32(w + 0x2B0, e);
        m.w32(w + 0x2B4, f2);
    };
    auto view_box = [&](u32 a, u32 b, u32 c, u32 d, u32 e, u32 f2) {
        m.w32(v + 0x7C, a);
        m.w32(v + 0x80, b);
        m.w32(v + 0x84, c);
        m.w32(v + 0x8C, d);
        m.w32(v + 0x90, e);
        m.w32(v + 0x94, f2);
    };
    constexpr u32 kLo = 0x80000001u, kHi = 0x7FFFFFFFu;
    switch (mission) {
    case 6:
        world_box(kLo, kHi, kLo, 0, kLo, kHi);
        m.w32(w, m.r32(w) | 0x18u);
        view_box(kLo, 0xFF6E0000u, kLo, kHi, 0xFFEF8000u, kHi);
        break;
    case 32: case 47: case 53:
        world_box(kLo, kHi, 0xFF48A000u, 0x01770000u, kLo, kHi);
        m.w32(w, m.r32(w) | 0x10u);
        view_box(0xDE96E000u, 0xFF5D4000u, 0xEA906000u, 0x056F0000u, 0xFFEF8000u, 0x11764000u);
        break;
    case 55:
        world_box(kLo, kHi, 0xFF48A000u, 0x01770000u, kLo, kHi);
        m.w32(w, m.r32(w) | 0x10u);
        view_box(kLo, 0xFF5D4000u, kLo, kHi, 0xFFEF8000u, kHi);
        break;
    case 42: case 58:
        world_box(kLo, kHi, kLo, kHi, kLo, kHi);
        view_box(kLo, kLo, kLo, kHi, kHi, kHi);
        m.w32(v, m.r32(v) | 0x1000u);
        m.w32(w, m.r32(w) | 0x300u);
        break;
    default:
        view_box(kLo, kLo, kLo, kHi, 0, kHi);
        world_box(kLo, kHi, kLo, kHi, kLo, kHi);
        break;
    }
}

void Game::hud_marks_templates() {
    cover(0x8004C6ECu);
    Frame f(*this, 0x20);
    m.w32(0x800BCFECu, mem_alloc(0x120));
    for (u32 k = 0; k < 2; ++k) {
        const u32 t = mem_alloc(0x788);
        m.w32(0x800BCFE8u, t);
        m.w32(m.r32(0x800BCFECu) + 4 * k, t);
        for (u32 i = 0; i < 32; ++i) pkt(t + 0x280 + 40 * i, 9, 0x2C);
        for (u32 i = 0; i < 32; ++i) pkt(t + 20 * i, 4, 0x64);
        m.w8(t + 0x783, 1);
        m.w32(t + 0x784, 0xE1000656u);
    }
    const u32 b = m.r32(0x800BCFECu);
    for (u32 i = 0; i < 32; ++i) m.w8(b + 8 + 8 * i, 0);
    m.w32(b + 0x10C, 0);
    m.w32(b + 0x108, 0);
    m.w32(b + 0x110, 0);
    m.w32(b + 0x114, 0);
    m.w32(b + 0x118, 0);
    m.w32(b + 0x11C, 0xFF);
}

void Game::hud_setup() {
    cover(0x8003A008u);
    Frame f(*this, 0x18);
    m.w32(0x800C6D60u, 0);
    m.w8(0x800C6D6Eu, 0xFF);
    m.w8(0x800C6D72u, 0xFF);
    m.w8(0x800C6D73u, 0xFF);
    m.w8(0x800C6D6Fu, 0);
    m.w16(0x800C6D66u, 0);
    m.w32(g::kFarDist, 3200);
    target_box_templates();
    gauge_blocks();
    hud_gauge_templates();
    hud_radar_templates();
    hud_warning_templates();
    hud_ladder_templates();
}

void Game::trails_alloc() {
    cover(0x8007DA98u);
    Frame f(*this, 0x18);
    m.w32(g::kTrails, mem_alloc(0x1C248));
    trails_reset();
}

void Game::weapons_alloc() {
    cover(0x8006C588u);
    Frame f(*this, 0x18);
    m.w32(g::kWeapons, mem_alloc(0x3564));
    weapons_reset();
}

void Game::effects_setup() {
    cover(0x80084A6Cu);
    Frame f(*this, 0x18);
    const u32 fx = mem_alloc(0x3B54);
    m.w32(0x800BCF90u, fx);
    noise_tables(fx);
    sun_directions(m.r32(0x800BCF90u) + 0x5C8);
    weather_colours(m.r32(0x800BCF90u) + 0x3948);
    effects_reset();
    const u32 sc = m.r32(g::kScene);
    if (!(m.r32(sc + 0x96C) & 0x10u)) return;
    const s32 mission = m.r16s(sc + 0x38C);
    const u32 fl = m.r16(sc + 0x5E);
    if (mission == 13 || mission == 42 || mission == 56) return;
    if ((fl & 0x1008u) == 0x1008u && mission != 32 && mission != 47 && mission != 53 && mission != 55) {
        m.w8(sc + 0x3AA, 7);
        m.w8(sc + 0x3AB, 7);
    } else if ((fl & 0x4008u) == 0x4008u) {
        m.w8(sc + 0x3AA, 10);
        m.w8(sc + 0x3AB, 10);
    } else if ((fl & 0x8008u) == 0x8008u) {
        m.w8(sc + 0x3AA, 5);
        m.w8(sc + 0x3AB, 5);
    } else if ((fl & 0x2008u) == 0x2008u) {
        m.w8(sc + 0x3AB, 9);
    }
}

void Game::noise_tables(u32 p) {
    cover(0x80084B84u);
    Frame f(*this, 0x30);
    for (u32 i = 0; i < 0x210; ++i) {
        s32 v = rand();
        if (rand() & 1) v = -v;
        m.w16(p + 2 * i, static_cast<u32>(v));
    }
    auto sum4 = [&]() {
        const s32 a = rand() & 0x7FF, b = rand() & 0x7FF, c = rand() & 0x7FF, d = rand() & 0x7FF;
        return (a + b + c + d) >> 2;
    };
    for (u32 i = 0; i < 0x6A; ++i) {
        const s32 x = sum4(), y = sum4(), z = sum4();
        const u32 w = (static_cast<u32>(x - 0x400) & 0x7FFu) | ((static_cast<u32>(y - 0x400) & 0x7FEu) << 10) |
                      (static_cast<u32>(z - 0x400) << 21);
        m.w32(p + 0x420 + 4 * i, w);
    }
}

void Game::sun_directions(u32 p) {
    cover(0x8008B178u);
    Frame f(*this, 0x20);
    const u32 v = g::kScratch + 0x1C, mat = g::kScratch + 0x24;
    m.w32(v, m.r32(m.r32(m.r32(g::kWorld) + 580) + 0x3C));
    m.w16(v + 4, 0);
    rot_matrix_yxz_gte(v, mat);
    for (u32 k = 0; k < 5; ++k) gte.ctc2(k, m.r32(mat + 4 * k));
    auto turn = [&](u32 xy, u32 z, u32 out) {
        gte.mtc2(0, xy);
        gte.mtc2(1, z);
        gte.op(0x486012);
        m.w16(out, gte.mfc2(9));
        m.w16(out + 2, gte.mfc2(10));
        m.w16(out + 4, gte.mfc2(11));
    };
    m.w32(p, 0);
    turn(0, 0x4000, v);
    m.w32(p + 8, m.r32(v));
    m.w32(p + 12, m.r32(v + 4));
    for (u32 k = 0; k < 4; ++k) {
        const s32 x = ((static_cast<s32>(k & 1u) << 1) - 1) * 442;
        const s32 y = (static_cast<s32>(k & 2u) - 1) * 442;
        m.w16(v, static_cast<u32>(x));
        m.w16(v + 2, static_cast<u32>(y));
        m.w16(v + 4, 0x4000);
        turn(m.r32(v), m.r32(v + 4), p + 16 + 8 * k);
    }
    turn(0, 0x1000, v);
    m.w32(p + 48, m.r32(v));
    m.w32(p + 52, m.r32(v + 4));
}

void Game::weather_colours(u32 p) {
    cover(0x800900A4u);
    const u32 src = m.r32(m.r32(g::kExit) + 12);
    for (u32 k = 0; k < 20; k += 4) m.w32(p + 8 + k, m.r32(src + 0x7C + k));
}

void Game::player_blocks_alloc() {
    cover(0x8006CCE0u);
    Frame f(*this, 0x18);
    const u32 a = mem_alloc(8);
    m.w32(kPlayerBlocks, a);
    for (u32 k = 0; k < 8; k += 2) m.w16(a + k, 0);
    const u32 b = mem_alloc(16);
    m.w32(0x800BD090u, b);
    for (u32 k = 0; k < 12; k += 2) m.w16(b + k, 0);
    m.w32(b + 12, 0);
    m.w32(0x800BD08Cu, mem_alloc(16));
    m.w32(0x800BD094u, mem_alloc(112));
    m.w32(kCamState, mem_alloc(60));
    m.w32(0x800BD0A0u, mem_alloc(56));
}

u32 Game::player_def(s32 model) {
    cover(0x8006CD6Cu);
    u32 k;
    switch (model) {
    case 1: case 12: k = 3; break;
    case 2: case 13: k = 4; break;
    case 3: k = 7; break;
    case 4: k = 8; break;
    case 5: k = 9; break;
    case 6: k = 10; break;
    case 7: k = 11; break;
    case 8: case 28: k = 19; break;
    case 9: k = 12; break;
    case 10: k = 1; break;
    case 11: k = 2; break;
    case 14: case 18: k = 5; break;
    case 15: case 19: k = 6; break;
    case 16: k = 13; break;
    case 17: case 29: k = 18; break;
    case 20: k = 15; break;
    case 21: k = 16; break;
    case 22: case 30: k = 20; break;
    case 23: k = 14; break;
    case 24: k = 17; break;
    case 25: k = 22; break;
    case 26: k = 21; break;
    case 27: k = 23; break;
    default: k = 0; break;
    }
    return m.r32(0x800B3AC8u + 4 * k);
}

void Game::player_setup(u32 e) {
    cover(0x8006CE70u);
    Frame f(*this, 0x20);
    player_blocks_alloc();
    const s32 model = m.r16s(e + 72);
    const u32 p = slot(0);
    const u32 def = player_def(model);
    m.w32(p + 32, 0);
    m.w32(p + 36, 0);
    m.w32(p + 40, 0x20000);
    m.w16(p + 48, 0);
    m.w16(p + 50, 0);
    m.w16(p + 52, 0);
    rotate_by_angles(p + 32, p + 24, p + 56);
    const u32 c = m.r32(g::kPlayer);
    m.w32(p + 84, 0x80000000u);
    m.w32(c, 0);
    m.w32(c + 4, 2);
    for (u32 k : {8u, 12u, 16u, 28u, 32u, 36u, 44u}) m.w32(c + k, 0);
    for (u32 k : {20u, 22u, 24u, 48u, 50u, 52u, 56u, 58u, 60u, 64u, 66u, 68u}) m.w16(c + k, 0);
    m.w16(c + 76, 0x1000);
    m.w16(c + 84, 0);
    m.w16(c + 86, 0);
    m.w32(c + 72, m.r32(p + 40));
    for (u32 i = 0; i < 12; ++i) m.w8(m.r32(g::kPlayer) + 88 + i, 0);
    for (u32 i = 0; i < 12; ++i) m.w8(m.r32(g::kPlayer) + 100 + i, 0);
    const u32 cs = m.r32(kCamState);
    m.w32(cs, def);
    const u32 ammo = m.r32(def + 132);
    m.w32(cs + 20, ammo);
    m.w16(p + 78, ammo);
    if (m.r32s(m.r32(g::kScene) + 916) >= 2) {
        m.w32(cs + 20, 30);
        m.w16(p + 78, 30);
    }
    const u32 cs2 = m.r32(kCamState);
    m.w32(cs2 + 8, m.r32(def + 116));
    m.w32(cs2 + 12, static_cast<u32>(m.r16s(def + 128)));
    m.w32(cs2 + 24, 0xFFFFFFFFu);
    m.w32(cs2 + 16, static_cast<u32>(m.r16s(def + 130)));
    m.w32(cs2 + 28, m.r32(def + 120));
    const s32 lim = m.r16s(def + 124);
    m.w32(cs2 + 4, static_cast<u32>(lim));
    const s32 cap = m.r16s(0x800B3B28u + 2 * m.r8(m.r32(g::kScene) + 938));
    if (cap < lim) m.w32(cs2 + 4, static_cast<u32>(cap));
    const u32 cs3 = m.r32(kCamState);
    m.w32(cs3 + 32, 0);
    m.w32(cs3 + 36, 0);
    m.w32(cs3 + 40, 0);
    m.w32(cs3 + 44, 0x7FFFFFFFu);
    m.w16(cs3 + 48, 0);
    m.w16(cs3 + 50, 0);
    m.w16(cs3 + 52, 0);
    m.w32(cs3 + 56, 0);
    alert_levels_reset();
}

void Game::player_start() {
    cover(0x8004D794u);
    Frame f(*this, 0x30);
    const u32 src = m.r32(m.r32(g::kSlotRecords) + 12);
    const u32 a = f.at(0x10);
    for (u32 k = 0; k < 24; k += 4) m.w32(a + k, m.r32(src + 4 + k));
    player_place(a);
}

void Game::world_setup() {
    cover(0x80038B2Cu);
    Frame f(*this, 0x18);
    const u32 file = m.r32(m.r32(g::kWorld) + 608);
    m.w32(kMsnFile, file);
    if (m.r32(file) == 3) {
        const s32 k = rand() % 3;
        m.w32(kMsnFile, file + m.r32(m.r32(kMsnFile) + 4 * static_cast<u32>(k) + 4));
    }
    units_parse();
    scripts_parse();
    mission_state_init();
    kcd_voice_reset();
    radio_setup();
    message_reset();
    m.w8(0x800BD05Cu, 0);
    cover(0x8005B648u);
    flight_log_init();
    air_units_init();
    ground_units_init_all();
    pairs_reset();
    view_attach_player(m.r32(m.r32(g::kWorld) + 40));
    lock_target_reset();
    lock_reset();
}

void Game::units_parse() {
    cover(0x8004D1ACu);
    Frame f(*this, 0x18);
    const u32 file = m.r32(kMsnFile);
    const u32 t = file + m.r32(file + 4);
    m.w8(kMg + 0x46, m.r8(t));
    m.w8(kMg + 0x59, m.r8(t + 4));
    m.w8(kMg + 0x50, m.r8(t + 8));
    m.w8(kMg + 0x58, m.r8(t + 12));
    static constexpr u32 kPtrs[][2] = {
        {0x54, 0x10}, {0x74, 0x14}, {0x68, 0x18}, {0x94, 0x1C}, {0xB0, 0x20}, {0xAC, 0x24},
        {0xA0, 0x28}, {0xA8, 0x2C}, {0x6C, 0x30}, {0x90, 0x34}, {0x98, 0x38}, {0x88, 0x3C},
    };
    for (const auto& [dst, src] : kPtrs) m.w32(kMg + dst, t + 4 * m.r32(t + src));
    {
        const u32 list = t + 4 * m.r32(t + 0x40);
        const u32 base = list + 0x40;
        for (u32 i = 0; i < 16; ++i) {
            const u32 v = m.r32(list + 4 * i);
            m.w32(0x800C7110u + 4 * i, v == 0xFFFFFFFFu ? 0u : base + 32 * v);
        }
    }
    {
        const u32 list = t + 4 * m.r32(t + 0x44);
        for (u32 i = 0; i < 4; ++i) {
            const u32 v = m.r32(list + 4 * i);
            m.w32(0x800C6E90u + 4 * i, v == 0xFFFFFFFFu ? 0u : list + v);
        }
    }
    m.w32(kMg + 0x5C, mem_alloc(12 * m.r8(kMg + 0x46)));
    for (u32 i = 0; i < m.r8(kMg + 0x46); ++i) {
        const u32 d = m.r32(kMg + 0x5C) + 12 * i, s = m.r32(kMg + 0x54) + 12 * i;
        m.w32(d, 1);
        m.w16(d + 4, m.r16(s + 2));
        m.w8(d + 6, m.r8(s + 4));
        m.w8(m.r32(kMg + 0x5C) + 12 * i + 7, 0);
        m.w32(m.r32(kMg + 0x5C) + 12 * i + 8, 0);
    }
    const u32 flags = mem_alloc(4 * m.r8(kMg + 0x59));
    m.w32(kMg + 0x80, flags);
    {
        const u32 n = m.r8(kMg + 0x59);
        u32 link = m.r32(kMg + 0x74);
        for (u32 i = 0; i < n; ++i, link += 20) {
            const u32 at = flags + 4 * i;
            if (m.r16(link) == 4) m.w32(at, 0);
            else m.w32(at, m.r32(at) | 1u);
        }
    }
    m.w32(kMg + 0x8C, file + m.r32(file + 8));
    aircraft_records();
    ground_records();
    for (u32 i = 0; i < 64; ++i) {
        m.w32(kRadarBlips + 4 * i, 0);
        m.w32(g::kRelation + 4 * i, 0);
    }
    const u32 patches = m.r32(m.r32(g::kWorld) + 612);
    m.w32(kMg + 0x84, patches);
    m.w32(kMg + 0x48, patches + 32);
    terrain_patches_all();
}

void Game::aircraft_records() {
    cover(0x8004D4D0u);
    Frame f(*this, 0x18);
    m.w32(kMg + 0x60, mem_alloc(static_cast<u32>(m.r8(kMg + 0x50)) << 6));
    m.w32(kMg + 0x28, mem_alloc(0x1700));
    for (u32 i = 0; i < m.r8(kMg + 0x50); ++i) {
        const u32 r = m.r32(kMg + 0x60) + 64 * i;
        const u32 e = m.r32(m.r32(g::kSlots) + 4 * i);
        const u32 d = m.r32(kMg + 0x68) + 44 * i;
        m.w32(r + 8, m.r32(kMg + 0x28) + 184 * i);
        m.w32(r + 12, d);
        m.w32(r + 4, e);
        m.w32(r + 16, m.r32(kMg + 0x94) + 16 * m.r8(d + 32));
        m.w32(r + 20, m.r32(kMg + 0x8C) + 40 * m.r8(d + 3));
        m.w32(r + 24, m.r32(kMg + 0xAC) + 24 * m.r8(d + 33));
        const u32 a = m.r8(d + 34);
        m.w32(r + 28, a == 0xFF ? 0u : m.r32(kMg + 0xA0) + 12 * a);
        const u32 b = m.r8(m.r32(kMg + 0x68) + 44 * i + 35);
        m.w32(m.r32(kMg + 0x60) + 64 * i + 32, b == 0xFF ? 0u : m.r32(kMg + 0xA8) + 8 * b);
        const u32 r2 = m.r32(kMg + 0x60) + 64 * i;
        const u32 d2 = m.r32(kMg + 0x68) + 44 * i;
        m.w32(r2 + 36, 0);
        m.w32(r2 + 40, m.r32(d2 + 36));
        const u32 v11 = m.r32(d2 + 40);
        const u32 k = m.r8(r2 + 40);
        m.w32(r2 + 60, 0);
        m.w32(r2 + 56, 0);
        m.w32(r2 + 52, 0);
        m.w32(r2 + 44, v11);
        m.w32(r2 + 48, m.r32(kMg + 0xB0) + 32 * k);
    }
}

void Game::ground_records() {
    cover(0x8004D680u);
    Frame f(*this, 0x18);
    const u32 n = m.r8(kMg + 0x58);
    const u32 base = mem_alloc(40 * n);
    m.w32(kMg + 0x64, base);
    u32 d = m.r32(kMg + 0x6C);
    for (u32 i = 0; i < m.r8(kMg + 0x58); ++i, d += 36) {
        const u32 r = base + 40 * i;
        m.w32(r + 8, d);
        m.w32(r + 4, m.r32(m.r32(g::kGroundSlots) + 4 * i));
        m.w32(r + 12, m.r32(kMg + 0x94) + 16 * m.r8(d + 32));
        const u32 a = m.r8(d + 33);
        m.w32(r + 16, a == 0xFF ? 0u : m.r32(kMg + 0x90) + 24 * a);
        m.w32(r + 20, m.r32(kMg + 0x98) + 48 * m.r8(d + 34));
        const u32 b = m.r8(d + 35);
        m.w32(r + 24, b != 0xFF ? m.r32(kMg + 0x88) + 16 * b : 0u);
        m.w32(r + 36, 0);
        m.w32(r + 32, 0);
        m.w32(r + 28, 0);
    }
}

void Game::terrain_patches_all() {
    cover(0x80048B48u);
    Frame f(*this, 0x20);
    for (u32 i = 0; i < 32; ++i) {
        u32 p = m.r32(kMg + 0x48) + 8 * m.r8(m.r32(kMg + 0x84) + i);
        while (m.r16(p)) {
            tile_set(m.r8(p + 2), m.r8(p + 3), m.r16s(p + 4));
            p += 8;
        }
    }
}

void Game::scripts_parse() {
    cover(0x80053E08u);
    Frame f(*this, 0x18);
    const u32 file = m.r32(kMsnFile);
    const u32 s = file + m.r32(file + 12);
    m.w32(kMg + 0x20, s);
    const u32 t = mem_alloc(600);
    m.w32(kMg + 0x14, t);
    m.w32(t, s + static_cast<u32>(m.r16s(s + 4)));
    m.w32(t + 4, s + static_cast<u32>(m.r16s(s + 6)));
    for (u32 i = 0; i < 4; ++i) {
        const u32 h = s + 2 * i;
        m.w32(t + 8 + 4 * i, s + static_cast<u32>(m.r16s(h + 8)));
        m.w32(t + 24 + 4 * i, s + static_cast<u32>(m.r16s(h + 16)));
        m.w32(t + 40 + 4 * i, s + static_cast<u32>(m.r16s(h + 24)));
        m.w32(t + 56 + 4 * i, s + static_cast<u32>(m.r16s(h + 32)));
        m.w32(t + 328 + 4 * i, s + static_cast<u32>(m.r16s(h + 168)));
        for (u32 j = 0; j < 16; ++j) {
            m.w32(t + 72 + 64 * i + 4 * j, s + static_cast<u32>(m.r16s(s + 32 * i + 2 * j + 40)));
            m.w16(t + 344 + 32 * i + 2 * j, m.r16(s + 32 * i + 2 * j + 176));
        }
    }
    for (u32 i = 0; i < 8; ++i) {
        m.w32(t + 472 + 4 * i, s + static_cast<u32>(m.r16s(s + 304 + 2 * i)));
        m.w32(t + 504 + 4 * i, s + static_cast<u32>(m.r16s(s + 320 + 2 * i)));
    }
    for (u32 i = 0; i < 4; ++i) {
        m.w32(t + 536 + 4 * i, s + static_cast<u32>(m.r16s(s + 336 + 2 * i)));
        m.w32(t + 552 + 4 * i, s + static_cast<u32>(m.r16s(s + 344 + 2 * i)));
    }
    for (u32 i = 0; i < 4; ++i) {
        m.w32(t + 568 + 4 * i, s + static_cast<u32>(m.r16s(s + 352 + 2 * i)));
        m.w32(t + 584 + 4 * i, s + static_cast<u32>(m.r16s(s + 360 + 2 * i)));
    }
    const u32 air = m.r32(kMg + 0x60), gnd = m.r32(kMg + 0x64);
    for (s32 i = 0; i < m.r32s(m.r32(kMg + 0x20) + 368); ++i) {
        const u32 h = m.r32(kMg + 0x20) + 372 + 4 * static_cast<u32>(i);
        const u32 who = m.r8(h), kind = m.r8(h + 1);
        const u32 at = s + static_cast<u32>(m.r16s(h + 2));
        u32 off;
        if (kind == 1) off = 0;
        else if (kind == 2) off = 4;
        else if (kind == 4) off = 8;
        else continue;
        if (who >= 32) m.w32(gnd + 40 * (who - 32) + 28 + off, at);
        else m.w32(air + 64 * who + 52 + off, at);
    }
}

void Game::mission_state_init() {
    cover(0x80053BACu);
    Frame f(*this, 0x18);
    const u32 file = m.r32(kMsnFile);
    m.w32(kMg + 0x18, file + m.r32(file + 16));
    const u32 s = mem_alloc(64);
    m.w32(kMg + 0x1C, s);
    m.w8(s + 10, 0);
    m.w32(s, 0);
    m.w32(s + 4, 0);
    m.w16(s + 8, 0);
    m.w8(s + 11, 0);
    m.w8(s + 28, 0);
    m.w32(s + 12, 0);
    m.w32(s + 16, 0);
    m.w32(s + 20, 0);
    m.w8(s + 29, 0);
    m.w16(s + 24, 0);
    m.w16(s + 26, 0);
    for (u32 k = 30; k <= 42; ++k) m.w8(s + k, 0);
    m.w8(s + 61, 0);
    m.w8(s + 43, 0xFF);
    m.w8(s + 44, demo_stream(static_cast<u32>(m.r16s(m.r32(g::kScene) + 908))));
    for (u32 k = 45; k <= 49; ++k) m.w8(s + k, 0);
    m.w8(s + 52, 0);
    m.w16(s + 50, 0);
    for (u32 k = 53; k <= 55; ++k) m.w8(s + k, 0);
    m.w8(s + 60, 0);
    m.w16(s + 56, 0);
    m.w16(s + 58, 0);
    m.w8(s + 62, 0);
    m.w8(s + 63, 0);
    msn_vm_exec(m.r32(m.r32(kMg + 0x14)));
}

void Game::kcd_voice_reset() {
    cover(0x8002CBCCu);
    m.w8(0x800BCE96u, 0);
    m.w8(0x800BCE95u, 0);
    m.w8(0x800BCE94u, 0);
    m.w8(0x800BCE8Fu, 0);
    m.w8(0x800BCE8Eu, 0);
    m.w32(0x800BCE88u, 0x800C57E0u);
    for (u32 i = 0; i < 3; ++i) {
        const u32 b = m.r32(0x800BCE88u);
        m.w8(b + i + 4, 0);
        m.w8(b + i + 7, 0);
        m.w16(b + 2 * i + 10, 0);
    }
    const u32 b = m.r32(0x800BCE88u);
    m.w16(b + 30, 0x4A16);
    m.w16(b + 28, 58);
}

void Game::radio_setup() {
    cover(0x80056198u);
    Frame f(*this, 0x18);
    const u32 file = m.r32(kMsnFile);
    const u32 r = file + m.r32(file + 20);
    const u32 n = m.r32(r);
    m.w32(0x800BD034u, r + 4);
    m.w32(0x800BD024u, n);
    m.w32(0x800BD02Cu, r + 4 * (n + 1));
    const u32 st = mem_alloc(24);
    m.w32(0x800BD020u, st);
    m.w32(st, 0xFF);
    for (u32 i = 0; i < 4; ++i) {
        m.w8(m.r32(0x800BD020u) + i + 20, 0xFF);
        m.w16(m.r32(0x800BD020u) + 2 * i + 12, 0);
        m.w16(m.r32(0x800BD020u) + 2 * i + 4, 0);
    }
    radio_script_alloc();
    m.w32(0x800BD028u, 0);
}

void Game::flight_log_init() {
    cover(0x8004856Cu);
    const u32 sc = m.r32(g::kScene);
    const u32 a = sc + 0x3B8;
    m.w8(a, 0);
    m.w8(a + 1, 0);
    m.w16(a + 2, 0);
    m.w8(a + 4, m.r8(sc + 0x38C));
    m.w8(a + 11, 4);
    m.w8(a + 10, 0);
    m.w8(a + 5, m.r8(m.r32(g::kScene) + 0x38C));
    m.w16(a + 8, 0);
    m.w8(m.r32(g::kScene) + 0x956, 0);
    const u32 pos = slot(0) + 8;
    auto cell = [&](s32 v) {
        s32 c = ((v >> 12) + 0x40000) >> 11;
        if (c < 0) c = 0;
        if (c >= 256) c = 0xFF;
        return static_cast<u32>(c);
    };
    m.w8(a + 12 + 2 * static_cast<u32>(m.r16s(a + 8)), cell(m.r32s(pos)));
    m.w8(a + 13 + 2 * static_cast<u32>(m.r16s(a + 8)), cell(m.r32s(pos + 8)));
}

void Game::relation_reset(u32 i) {
    cover(0x8004D800u);
    const u32 r = m.r32(g::kRelation + 4 * i);
    m.w32(r, 0);
    m.w32(r + 4, 0x100000);
    m.w32(r + 16, 0);
    m.w32(r + 12, 0);
    m.w32(r + 8, 0);
    m.w16(r + 24, 0);
    m.w16(r + 26, 0);
    m.w16(r + 28, 0);
    m.w16(r + 30, 0);
    m.w32(r + 32, 0);
    m.w32(r + 36, 0);
}

void Game::air_units_init() {
    cover(0x80038A6Cu);
    Frame f(*this, 0x18);
    air_units_setup();
    const u32 p = slot(0);
    m.w8(p + 4, 0);
    m.w32(p, 0x1C240);
    m.w32(p + 8, 0);
    m.w32(p + 12, 0xFFB50000u);
    m.w32(p + 16, 0);
    m.w16(p + 24, 0);
    m.w16(p + 26, 0);
    m.w16(p + 28, 0);
    m.w32(slot(1), 1);
}

void Game::air_units_setup() {
    cover(0x8004D84Cu);
    Frame f(*this, 0x38);
    for (u32 i = 2; i < 32; ++i) {
        m.w32(slot(i), i);
        m.w32(g::kSlotFlags + 4 * i, 0);
    }
    for (u32 i = 2; i < m.r8(kMg + 0x50); ++i) {
        const u32 e = slot(i);
        const u32 rec = m.r32(kMg + 0x60) + 64 * i;
        const u32 def = m.r32(rec + 12), hp = m.r32(rec + 16), ai_def = m.r32(rec + 48);
        m.w32(e, m.r32(e) | 0x4000u);
        m.w32(kRadarBlips + 4 * i, mem_alloc(12));
        m.w32(g::kRelation + 4 * i, mem_alloc(40));
        relation_reset(i);
        const u32 side = m.r8(def);
        if (side == 0) m.w32(e, m.r32(e) | 0x40u);
        else if (side == 1) m.w32(e, m.r32(e) | 0x80u);
        else if (side == 2) m.w32(e, m.r32(e) | 0x100u);
        m.w8(e + 4, m.r8(def + 2));
        m.w16(e + 0x4E, m.r16(hp));
        m.w16(e + 0x48, m.r8(def + 1));
        for (u32 k = 0; k < 24; k += 4) m.w32(e + 8 + k, m.r32(def + 4 + k));
        m.w32(m.r32(g::kRelation + 4 * i) + 4, static_cast<u32>(distance(e + 8, slot(0) + 8)));
        m.w32(e + 0x20, 0);
        m.w32(e + 0x24, 0);
        const u32 a0 = m.r32(m.r32(m.r32(kMg + 0x60) + 64 * i + 0x14) + 4);
        m.w16(e + 0x30, 0);
        m.w16(e + 0x32, 0);
        m.w16(e + 0x34, 0);
        m.w32(e + 0x38, 0);
        m.w32(e + 0x3C, 0);
        m.w32(e + 0x40, 0);
        m.w32(e + 0x54, 0x80200000u);
        m.w32(e + 0x28, a0);
        const u32 ai = m.r32(g::kAiInfo) + 184 * i;
        m.w32(ai, 0);
        m.w8(ai + 0x1D, 0);
        m.w8(ai + 0x1E, 0);
        m.w32(ai + 0x38, 0);
        m.w8(ai + 0x1C, m.r8(ai_def));
        m.w8(ai + 0x1F, m.r8(ai_def + 1));
        for (u32 k = 0; k < 16; k += 4) m.w32(ai + 0x20 + k, m.r32(ai_def + 8 + k));
        m.w32(ai + 0x30, m.r32(ai_def + 0x18));
        m.w32(ai + 0x34, m.r32(ai_def + 0x1C));
        m.w8(ai + 0x3C, 0xFF);
        m.w8(ai + 0x3D, 0xFF);
        m.w8(ai + 0x3E, 0);
        m.w16(ai + 0x40, 0);
        m.w8(ai + 0x62, 0xFF);
        m.w8(ai + 0x63, 0xFF);
        m.w8(ai + 0x64, 0);
        m.w8(ai + 0x3F, 0xFF);
        m.w8(ai + 0x60, 0xFF);
        m.w8(ai + 0x61, 0);
        for (u32 k : {0x66u, 0xB6u, 4u, 6u, 8u, 0x80u, 0x82u, 0x84u}) m.w16(ai + k, 0);
        for (u32 k : {0xCu, 0x10u, 0x14u}) m.w32(ai + k, 0);
    }
}

void Game::ground_units_setup() {
    cover(0x8004E3A8u);
    Frame g2(*this, 0x38);
    for (u32 i = 0; i < 32; ++i) {
        m.w32(m.r32(m.r32(g::kGroundSlots) + 4 * i), 32 + i);
        m.w32(g::kSlotFlags + 0x80 + 4 * i, 0);
    }
    for (u32 i = 0; i < m.r8(kMg + 0x58); ++i) {
        const u32 e = m.r32(m.r32(g::kGroundSlots) + 4 * i);
        const u32 rec = m.r32(kMg + 0x64) + 40 * i;
        const u32 def = m.r32(rec + 8), hp = m.r32(rec + 12);
        m.w32(e, m.r32(e) | 0x4000u);
        m.w32(kRadarBlips + 0x80 + 4 * i, mem_alloc(12));
        m.w32(g::kRelation + 0x80 + 4 * i, mem_alloc(40));
        relation_reset(32 + i);
        const u32 side = m.r8(def);
        if (side == 0) m.w32(e, m.r32(e) | 0x40u);
        else if (side == 1) m.w32(e, m.r32(e) | 0x80u);
        else if (side == 2) m.w32(e, m.r32(e) | 0x100u);
        m.w32(e + 0x54, 0);
        m.w8(e + 5, 0);
        for (u32 k = 0; k < 24; k += 4) m.w32(e + 8 + k, m.r32(def + 4 + k));
        const u32 t = m.r8(def + 2);
        m.w8(e + 4, t == 0xFF ? 0u : t);
        if (m.r8(def + 0x1F) == 1) m.w8(e + 5, m.r8(e + 5) | 2u);
        m.w16(e + 6, 0);
        m.w16(e + 0x48, m.r8(def + 1));
        const u32 h = m.r16(hp);
        for (u32 k : {0x20u, 0x24u, 0x28u, 0x38u, 0x3Cu, 0x40u}) m.w32(e + k, 0);
        m.w16(e + 0x30, 0);
        m.w16(e + 0x32, 0);
        m.w16(e + 0x34, 0);
        m.w32(e + 0x54, 0x80000000u);
        m.w16(e + 0x4E, h);
        m.w32(m.r32(g::kRelation + 0x80 + 4 * i) + 4, static_cast<u32>(distance(e + 8, slot(0) + 8)));
        m.w8(e + 0x58, 0);
        m.w8(e + 0x59, 0);
        m.w8(e + 0x5A, 0);
        m.w8(e + 0x5B, 0xFF);
        m.w32(e + 0x5C, 0);
        m.w32(e + 0x60, 0);
        m.w32(e + 0x64, 0);
        m.w32(e + 0x68, 1);
    }
}

void Game::pairs_reset() {
    cover(0x80045FF0u);
    for (u32 i = 0; i < 64; ++i) {
        const u32 p = g::kPairs + 4 * i;
        m.w8(p, 0);
        m.w8(p + 3, 0xFF);
        m.w8(p + 2, 0xFF);
        m.w8(p + 1, 0xFF);
    }
    m.w8(g::kPairs, 0xFF);
}

void Game::view_attach_player(u32 v) {
    cover(0x80038AF4u);
    m.w32(v, m.r32(v) | 0x100u);
    m.w32(v + 56, 0);
    m.w32(v + 60, 0);
    m.w32(v + 64, 0);
    m.w16(v + 72, 0);
    m.w16(v + 74, 0);
    m.w16(v + 76, 0);
    m.w32(v + 52, slot(0) + 8);
}

void Game::lock_target_reset() {
    cover(0x80048BDCu);
    m.w16(0x800C7194u, 1);
    m.w32(0x800C7190u, 0);
    m.w8(0x800C7199u, 0xFF);
    m.w16(0x800C7196u, 0xFFFF);
    m.w8(0x800C7198u, 0xFF);
    m.w8(0x800C719Au, 0);
}
}

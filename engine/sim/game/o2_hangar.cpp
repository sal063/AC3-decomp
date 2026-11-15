#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kH = 0x80112DA8u;
}

void Game::o2_hangar_camera(u32 h) {
    cover(0x800FF390u);
    Frame f(*this, 0x28);
    m.w32(h + 0x78, 200);
    m.w16(h + 0x70, 0x100);
    m.w16(h + 0x108, 0x200);
    m.w16(h + 0x6C, 0);
    m.w32(h + 0x7C, 0);
    m.w16(h + 0x72, 0);
    m.w16(h + 0x10A, 0);
    m.w32(h + 0xC, 0);
    const u32 rect = f.at(0x10);
    m.w32(rect, m.r32(0x800CD76Cu));
    m.w32(rect + 4, m.r32(0x800CD770u));
    m.w16(h + 0xB0, 0xF);
    m.w16(h + 0xB2, 0x347F);
    m.w16(h + 0xB6, 0xC0);
    m.w16(h + 0xB4, 0);
    m.w16(h + 0xB8, 0x1F);
    m.w16(h + 0xBA, 0x1F);
    hw->store_image(rect, h + 0xBC);
    for (u32 k = 0; k < 2; ++k) m.w32(h + 4 * k, mem_alloc(0x400));
    for (u32 i = 0; i < 64; ++i) {
        m.w32(m.r32(h + 0x80) + 8 * i, 0);
        m.w32(m.r32(h + 0x80) + 8 * i + 4, 0);
        m.w32(m.r32(h + 0x84) + 16 * i, 0);
        m.w32(m.r32(h + 0x84) + 16 * i + 4, 0);
        const s32 a = static_cast<s32>(8 * 0xC35 * i);
        m.w32(m.r32(h + 0x84) + 16 * i + 8, static_cast<u32>(0x5DA8 - (a >> 6)));
    }
}

void Game::o2_hangar_stars(u32 h) {
    cover(0x801009ACu);
    Frame f(*this, 0x28);
    for (u32 i = 0; i < 48; ++i) {
        m.w16(m.r32(h + 0xDC) + 2 * i, static_cast<u32>(rand() % 64));
        const s32 c = rcos(rand() & 0xFFF);
        const s32 rc = rand() % 1536;
        m.w32(m.r32(h + 0xE0) + 16 * i, static_cast<u32>(c * rc / 4096));
        const s32 s = rsin(rand() & 0xFFF);
        const s32 rs = rand() % 1536;
        m.w32(m.r32(h + 0xE0) + 16 * i + 4, static_cast<u32>(s * rs / 4096));
        m.w32(m.r32(h + 0xE0) + 16 * i + 8, static_cast<u32>(rand() % 1562));
        const u32 p = m.r32(h + 0xE0) + 16 * i;
        const s32 x = m.r32s(p);
        m.w32(p, static_cast<u32>(x >= 0 ? x + 384 : x - 384));
        const s32 y = m.r32s(p + 4);
        m.w32(p + 4, static_cast<u32>(y >= 0 ? y + 384 : y - 384));
    }
}

void Game::o2_hangar_streak(u32 k, u32 h) {
    cover(0x80100FDCu);
    Frame f(*this, 0x30);
    const s32 z = rand() & 0x3FFF;
    const s32 xe = 160 * z / 250, ye = 240 * z / 250;
    const s32 edge = rand() & 3;
    const u32 pos = m.r32(m.r32(h + 0xF4) + 4 * k), rot = m.r32(m.r32(h + 0xF0) + 4 * k);
    auto set_pos = [&](s32 x, s32 y) {
        const u32 p = m.r32(m.r32(h + 0xF4) + 4 * k);
        m.w32(p, static_cast<u32>(x));
        m.w32(m.r32(m.r32(h + 0xF4) + 4 * k) + 4, static_cast<u32>(y));
        m.w32(m.r32(m.r32(h + 0xF4) + 4 * k) + 8, static_cast<u32>(z));
    };
    (void)pos;
    (void)rot;
    u32 a0, a1;
    if (edge == 1) {
        const s32 x = -static_cast<s32>(static_cast<s16>(xe));
        const s32 r = rand();
        set_pos(x, (r % 480 - 240) * z / 250);
        a0 = rand() & 0x1FF;
        m.w16(m.r32(m.r32(h + 0xF0) + 4 * k), a0);
        a1 = rand() & 0x5FF;
    } else if (edge == 2 || edge == 3) {
        const s32 r = rand();
        const s32 x = (r % 320 - 160) * z / 250;
        const s32 y = edge == 2 ? static_cast<s16>(ye) : -static_cast<s32>(static_cast<s16>(ye));
        set_pos(x, y);
        a0 = rand() & (edge == 2 ? 0xDFFu : 0x5FFu);
        m.w16(m.r32(m.r32(h + 0xF0) + 4 * k), a0);
        a1 = rand() & 0xFFF;
    } else {
        const s32 x = static_cast<s16>(xe);
        const s32 r = rand();
        set_pos(x, (r % 480 - 240) * z / 250);
        a0 = rand() & 0x1FF;
        m.w16(m.r32(m.r32(h + 0xF0) + 4 * k), a0);
        a1 = rand() & 0xDFF;
    }
    m.w16(m.r32(m.r32(h + 0xF0) + 4 * k) + 2, a1);
    m.w16(m.r32(m.r32(h + 0xF0) + 4 * k) + 4, 0);
    for (u32 i = 1; i < 15; ++i) {
        const u32 r = m.r32(m.r32(h + 0xF0) + 4 * k) + 8 * i;
        m.w16(r, m.r16(r - 8));
        m.w16(r + 2, m.r16(r - 6));
        m.w16(r + 4, m.r16(r - 4));
        const u32 p = m.r32(m.r32(h + 0xF4) + 4 * k) + 16 * i;
        m.w32(p, m.r32(p - 16));
        m.w32(p + 4, m.r32(p - 12));
        m.w32(p + 8, m.r32(p - 8));
    }
}

void Game::o2_hangar_streaks(u32 h) {
    cover(0x80101570u);
    Frame f(*this, 0x28);
    m.w32(h + 0xF0, mem_alloc(12));
    m.w32(h + 0xF4, mem_alloc(12));
    m.w32(h + 0xF8, mem_alloc(40));
    for (u32 k = 0; k < 3; ++k) {
        m.w32(m.r32(h + 0xF0) + 4 * k, mem_alloc(120));
        const u32 b = mem_alloc(240);
        m.w32(m.r32(h + 0xF4) + 4 * k, b);
        o2_hangar_streak(k, h);
    }
    m.w32(m.r32(h + 0xF8), 0);
    m.w32(m.r32(h + 0xF8) + 4, 0);
    s32 a = -512;
    for (u32 i = 1; i < 5; ++i, a += 1024) {
        const s32 c = rcos(a), s = rsin(a);
        m.w32(m.r32(h + 0xF8) + 8 * i, (static_cast<u32>(c / 32) & 0xFFFFu) | (static_cast<u32>(s / 32) << 16));
        m.w32(m.r32(h + 0xF8) + 8 * i + 4, 0);
    }
    m.w32(h + 0xFC, 0);
    m.w32(h + 0x100, 512);
}

void Game::o2_hangar_alloc() {
    cover(0x80101EE4u);
    Frame f(*this, 0x20);
    const u32 h = mem_alloc(0x110);
    m.w32(kH, h);
    m.w32(h + 0x80, mem_alloc(0x200));
    m.w32(h + 0x84, mem_alloc(0x400));
    m.w32(h + 0xDC, mem_alloc(0x60));
    m.w32(h + 0xE0, mem_alloc(0x300));
    m.w32(h + 0x104, mem_alloc(0x80));
    for (u32 i = 0; i < 16; ++i) {
        m.w16(m.r32(h + 0x104) + 8 * i, static_cast<u32>(rcos(static_cast<s32>(i << 8)) / 2));
        m.w16(m.r32(h + 0x104) + 8 * i + 2, static_cast<u32>(rsin(static_cast<s32>(i << 8)) / 2));
        m.w16(m.r32(h + 0x104) + 8 * i + 4, 0);
    }
    m.w32(h + 0xC, 0);
    m.w32(h + 0x14, 0);
    m.w32(h + 0x18, 0);
    m.w32(h + 0x1C, 0);
    o2_hangar_camera(h);
    o2_hangar_stars(h);
    o2_hangar_streaks(h);
}

void Game::o2_hangar_frame(u32 ot) {
    cover(0x800FF100u);
    Frame f(*this, 0x18);
    const u32 h = m.r32(kH);
    const u32 t = m.r32(h + 4 * m.r32(m.r32(g::kScene) + 4));
    m.w32(h + 8, t);
    clear_otag_r(t, 256);
    const u32 t0 = m.r32(m.r32(kH) + 8);
    m.w32(t0, (m.r32(t0) & 0xFF000000u) | (m.r32(ot) & 0x00FFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | ((t0 + 1020) & 0x00FFFFFFu));
}

u32 Game::o2_hangar_fly() {
    cover(0x800FF2BCu);
    Frame f(*this, 0x18);
    sound_play(98);
    const u32 h = m.r32(kH);
    m.w32(h + 0x14, 1);
    m.w32(h + 0x20, 0);
    return 1;
}

u32 Game::o2_hangar_effect2(u32 state) {
    cover(state == 1 ? 0x8010097Cu : 0x80100994u);
    const u32 h = m.r32(kH);
    m.w32(h + 0x18, state);
    m.w32(h + 0x24, 0);
    return state;
}

u32 Game::o2_hangar_flight(u32 h, u32 s, u32 p) {
    cover(0x8010084Cu);
    Frame f(*this, 0x20);
    o2_hangar_tunnel_camera(h, s);
    p = o2_hangar_tunnel_draw(h, s, p);
    p = o2_hangar_lights_draw(h, s, p);
    const u32 st = m.r32(h + 0x14);
    if (st == 1) {
        if (m.r32s(h + 0x20) / 8 < 12) return p;
        m.w32(h + 0x20, 0);
        m.w32(h + 0x14, 2);
        for (u32 k = 0; k < 5; ++k) m.w32(m.r32(0x80112DC4u) + 0x130 + 4 * k, sound_play(93 + k));
        return p;
    }
    if (st == 3 && m.r32s(h + 0x20) / 8 >= 12) {
        m.w32(h + 0x20, 0);
        m.w32(h + 0x14, 0);
    }
    return p;
}

u32 Game::o2_hangar_draw(u32 p) {
    cover(0x80101FECu);
    Frame f(*this, 0x20);
    const u32 h = m.r32(kH);
    gte.ctc2(24, 0xA00000);
    gte.ctc2(25, 0xF00000);
    gte.ctc2(26, 250);
    gte.ctc2(27, static_cast<u32>(-11184));
    gte.ctc2(28, 0x1555000);
    const u32 e1 = m.r32(h + 0x14), e2 = m.r32(h + 0x18);
    if (e1) p = o2_hangar_flight(h, g::kScratch, p);
    if (e2) p = o2_hangar_stars_draw(h, g::kScratch, p);
    if (!e1 && !e2 && m.r32(h + 0x1C)) p = o2_hangar_trails_tick(h, g::kScratch, p);
    m.w32(h + 0xC, m.r32(h + 0xC) + 1);
    return p;
}
}

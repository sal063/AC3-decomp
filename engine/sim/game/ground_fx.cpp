#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kFx = 0x800BCF90u;
s32 u8of(s32 v) { return v & 0xFF; }
s32 mix(s32 r) { return (r >> 8) | (r << 7); }
}

u32 Game::fx_flashes() { return pool_take(m.r32(kFx) + 1536, 3, 7); }
u32 Game::fx_debris() { return pool_take(m.r32(kFx) + 4248, 10, 7); }
u32 Game::fx_light() { return pool_take(m.r32(kFx) + 13248, 1, 6); }
u32 Game::fx_timed() { return pool_take(m.r32(kFx) + 14020, 1, 5); }

void Game::fx_timed_add(u32 head, s32 x, s32 y, s32 z, u32 fn) {
    const u32 p = fx_timed();
    if (!p) return;
    m.w32(p, head);
    m.w32(p + 4, static_cast<u32>(x));
    m.w32(p + 8, static_cast<u32>(y));
    m.w32(p + 12, static_cast<u32>(z));
    m.w32(p + 16, fn);
}

void Game::fx_light_at(s32 x, s32 y, s32 z, u32 look) {
    cover(0x80087168u);
    Frame f(*this, 32);
    const u32 p = fx_light();
    if (!p) return;
    const s32 r = u8of(rand());
    m.w16(p + 2, (look & 0xFFu) + static_cast<u32>(r & 3));
    m.w16(p, 1);
    m.w32(p + 4, static_cast<u32>(x));
    m.w32(p + 8, static_cast<u32>(y));
    m.w32(p + 12, static_cast<u32>(z));
    m.w16(p + 20, 1);
    m.w16(p + 22, (look >> 16) + static_cast<u32>(r));
    m.w32(p + 16, 1024);
}

void Game::fx_8008721C(s32 x, s32 y, s32 z) {
    cover(0x8008721Cu);
    Frame f(*this, 48);
    s32 vy = -100;
    if (const u32 p = fx_flashes()) {
        const s32 r = rand();
        m.w32(p, 2686975);
        m.w16(p + 22, 134);
        m.w16(p + 16, static_cast<u32>(((r >> 2) & 0x7F) - 64));
        m.w16(p + 18, static_cast<u32>(-240));
        m.w16(p + 20, static_cast<u32>(((r >> 5) & 0x7F) - 64));
        m.w16(p + 24, 528);
        m.w32(p + 4, static_cast<u32>(x));
        m.w32(p + 8, static_cast<u32>(y));
        m.w32(p + 12, static_cast<u32>(z));
        m.w16(p + 26, static_cast<u32>((r & 0x7F) + 436));
    }
    for (s32 k = 0; k < 5; ++k) {
        const u32 q = fx_flashes();
        if (!q) break;
        const s32 r = rand();
        m.w16(q, 0xFFFF);
        m.w16(q + 2, static_cast<u32>(k + (r & 1) + 18));
        m.w32(q + 4, static_cast<u32>(x + ((u8of(r) - 128) << 11)));
        m.w32(q + 8, static_cast<u32>(y + ((u8of(r >> 3) - 128) << 11)));
        m.w32(q + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 11)));
        const s32 s = rand();
        m.w16(q + 18, static_cast<u32>(vy));
        vy -= 80;
        m.w16(q + 16, static_cast<u32>(u8of(s) - 128));
        m.w16(q + 20, static_cast<u32>(u8of(s >> 7) - 128));
        m.w16(q + 22, 133);
        m.w16(q + 24, 256);
        m.w16(q + 26, static_cast<u32>((k << 6) + ((s >> 4) & 0x7F) + 116));
    }
}

void Game::fx_800873E0(s32 x, s32 y, s32 z) {
    cover(0x800873E0u);
    Frame f(*this, 40);
    for (s32 k = 0; k < 16; ++k) {
        const u32 p = fx_debris();
        if (!p) break;
        const s32 r = rand();
        const s32 lo = r & 0xF;
        const s32 a = u8of(r) - 128, c = u8of(r >> 6) - 128, b = u8of(r >> 3);
        s32 vy = 4 * (b - 224);
        m.w32(p + 8, static_cast<u32>(y + (a << 8)));
        m.w16(p + 16, static_cast<u32>(4 * a));
        m.w32(p + 4, static_cast<u32>(x + (c << 8)));
        m.w16(p + 20, static_cast<u32>(4 * c));
        m.w32(p + 12, static_cast<u32>(z + ((b - 128) << 8)));
        u32 look;
        if (k >= 8) {
            m.w16(p + 26, static_cast<u32>(lo + 25));
            look = 405;
            m.w16(p, 0);
            m.w16(p + 2, static_cast<u32>(lo + 30));
            m.w16(p + 24, 0);
        } else {
            vy -= 384;
            m.w16(p + 2, static_cast<u32>(lo + 24));
            m.w16(p + 24, 5636);
            look = 139;
            m.w16(p, 0);
            m.w16(p + 26, static_cast<u32>(4 * lo + 600));
        }
        m.w16(p + 22, look);
        m.w16(p + 18, static_cast<u32>(vy));
    }
}

void Game::fx_80088D60(s32 x, s32 y, s32 z) {
    cover(0x80088D60u);
    Frame f(*this, 40);
    for (s32 k = 0; k < 18; ++k) {
        const u32 p = fx_debris();
        if (!p) break;
        const s32 r = rand();
        const s32 a = u8of(r) - 128, c = u8of(r >> 6) - 128, b = u8of(r >> 3) - 128;
        s32 vy = 4 * b - 384;
        m.w32(p + 8, static_cast<u32>(y + (a << 8)));
        m.w16(p + 16, static_cast<u32>(4 * a));
        m.w32(p + 4, static_cast<u32>(x + (c << 8)));
        m.w16(p + 20, static_cast<u32>(4 * c));
        m.w32(p + 12, static_cast<u32>(z + (b << 8)));
        u32 look;
        if (k < 12) {
            vy -= 32 * k;
            m.w16(p, 8);
            m.w16(p + 2, static_cast<u32>((r & 0xF) + k + 44));
            m.w16(p + 24, 6148);
            m.w16(p + 26, static_cast<u32>(u8of(r) + 600));
            look = 141;
        } else {
            m.w16(p + 26, static_cast<u32>((r & 0xF) + 25));
            look = 405;
            m.w16(p, 0);
            m.w16(p + 2, static_cast<u32>((r & 0xF) + 36));
            m.w16(p + 24, 0);
        }
        m.w16(p + 22, look);
        m.w16(p + 18, static_cast<u32>(vy));
    }
}

void Game::fx_80087AD0(s32 x, s32 y, s32 z) {
    cover(0x80087AD0u);
    Frame f(*this, 40);
    for (s32 k = 0; k < 20; ++k) {
        const u32 p = fx_debris();
        if (!p) break;
        const s32 r = rand();
        const s32 a = u8of(r) - 128, c = u8of(r >> 6) - 128, b = u8of(r >> 3) - 128;
        s32 vy = 4 * b - 384;
        m.w32(p + 8, static_cast<u32>(y + (a << 8)));
        m.w16(p + 16, static_cast<u32>(4 * a));
        m.w32(p + 4, static_cast<u32>(x + (c << 8)));
        m.w16(p + 20, static_cast<u32>(4 * c));
        m.w32(p + 12, static_cast<u32>(z + (b << 8)));
        u32 look;
        if (k < 15) {
            vy -= (k << 6) + 400;
            m.w16(p + 2, static_cast<u32>((r & 0xF) + k + 24));
            m.w16(p + 24, 6148);
            m.w16(p + 26, static_cast<u32>(u8of(r) + 700));
            look = 141;
            m.w16(p, 0);
        } else {
            m.w16(p + 26, static_cast<u32>((r & 0xF) + 25));
            look = 405;
            m.w16(p, 0);
            m.w16(p + 2, static_cast<u32>((r & 0xF) + 30));
            m.w16(p + 24, 0);
        }
        m.w16(p + 22, look);
        m.w16(p + 18, static_cast<u32>(vy));
    }
}

void Game::fx_80087520(s32 x, s32 y, s32 z) {
    cover(0x80087520u);
    Frame f(*this, 32);
    fx_8008721C(x, y, z);
    fx_800873E0(x, y, z);
    fx_light_at(x, y, z, 85196816);
}

void Game::fx_80087584(s32 x, s32 y, s32 z) {
    cover(0x80087584u);
    Frame f(*this, 40);
    fx_8008721C(x, y, z);
    fx_800873E0(x, y, z);
    const u32 p = fx_light();
    if (!p) return;
    const s32 r = u8of(rand());
    m.w16(p, 0);
    m.w16(p + 2, static_cast<u32>((r & 3) + 12));
    m.w32(p + 4, static_cast<u32>(x));
    m.w32(p + 8, static_cast<u32>(y));
    m.w32(p + 12, static_cast<u32>(z));
    m.w16(p + 20, 0);
    m.w16(p + 22, static_cast<u32>(r + 1200));
}

void Game::fx_80089504(s32 x, s32 y, s32 z) {
    cover(0x80089504u);
    Frame f(*this, 48);
    for (s32 k = 0; k < 6; ++k) {
        const u32 p = fx_flashes();
        if (!p) break;
        const s32 r = rand();
        const s32 v13 = (r << 8) | (r >> 7);
        m.w32(p + 4, static_cast<u32>(x + ((u8of(r) - 128) << 11)));
        m.w16(p, static_cast<u32>(~k));
        m.w16(p + 16, static_cast<u32>(2 * (u8of(v13) - 128)));
        m.w16(p + 18, static_cast<u32>(2 * (u8of(v13 >> 3) - 128)));
        m.w16(p + 20, static_cast<u32>(2 * (u8of(v13 >> 6) - 128)));
        m.w16(p + 22, 133);
        m.w16(p + 24, 256);
        m.w16(p + 2, static_cast<u32>((r & 7) + 24));
        m.w32(p + 8, static_cast<u32>(y + ((u8of(r >> 3) - 128) << 11)));
        m.w32(p + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 11)));
        m.w16(p + 26, static_cast<u32>(u8of(v13) + 300));
    }
    if (const u32 q = fx_light()) {
        const s32 r = u8of(rand());
        m.w16(q, 0);
        m.w16(q + 2, static_cast<u32>((r & 3) + 20));
        m.w32(q + 4, static_cast<u32>(x));
        m.w32(q + 8, static_cast<u32>(y));
        m.w32(q + 12, static_cast<u32>(z));
        m.w16(q + 20, 0);
        m.w16(q + 22, static_cast<u32>(r + 1600));
    }
    s32 life = 580;
    for (s32 k = 0; k < 24; ++k, life += 32) {
        const u32 p = fx_debris();
        if (!p) break;
        m.w16(p, 0);
        m.w16(p + 2, static_cast<u32>(k + 50));
        const s32 r = rand();
        const s32 r2 = mix(r);
        m.w16(p + 16, static_cast<u32>(u8of(r2) - 128));
        m.w16(p + 18, static_cast<u32>(u8of(r2 >> 3) - 128));
        m.w16(p + 20, static_cast<u32>(u8of(r2 >> 6) - 128));
        m.w32(p + 4, static_cast<u32>(x + ((u8of(r) - 128) << 11)));
        m.w32(p + 8, static_cast<u32>(y + ((u8of(r >> 3) - 128) << 11)));
        m.w32(p + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 11)));
        if (k & 1) {
            m.w16(p + 22, 135);
            m.w16(p + 24, 6148);
            m.w16(p + 26, static_cast<u32>(life));
        } else {
            m.w16(p + 22, 384);
            m.w16(p + 24, 0);
            m.w16(p + 26, static_cast<u32>(k + 30));
        }
    }
}

void Game::fx_80089488(s32 x, s32 y, s32 z) {
    cover(0x80089488u);
    Frame f(*this, 32);
    fx_80087520(x, y, z);
    fx_timed_add(6160383, x, y, z, 0x80089378u);
}

void Game::fx_800892FC(s32 x, s32 y, s32 z) {
    cover(0x800892FCu);
    Frame f(*this, 32);
    fx_80087520(x, y, z);
    fx_timed_add(6160383, x, y, z, 0x800891ECu);
}

void Game::fx_replay_marker(u32 head, s32 x, s32 y, s32 z, u32 fn, bool halves) {
    if (!(m.r32(m.r32(g::kWorld)) & 2u)) return;
    const u32 p = fx_timed();
    if (!p) return;
    if (halves) {
        m.w16(p + 2, head >> 16);
        m.w16(p, head & 0xFFFFu);
    } else {
        m.w32(p, head);
    }
    m.w32(p + 4, static_cast<u32>(x));
    m.w32(p + 8, static_cast<u32>(y));
    m.w32(p + 12, static_cast<u32>(z));
    m.w32(p + 16, fn);
}

void Game::fx_800890D8(s32 x, s32 y, s32 z) {
    cover(0x800890D8u);
    Frame f(*this, 32);
    fx_8008721C(x, y, z);
    fx_800873E0(x, y, z);
    fx_light_at(x, y, z, 117964824);
    if (const u32 p = fx_timed()) {
        m.w32(p + 8, static_cast<u32>(y + 2007040));
        m.w32(p, 9961471);
        m.w32(p + 4, static_cast<u32>(x));
        m.w32(p + 12, static_cast<u32>(z - 2457600));
        m.w32(p + 16, 0x80088A84u);
    }
    fx_replay_marker(3600u << 16, x, y, z, 0x80089C40u, true);
}

void Game::fx_80088FBC(s32 x, s32 y, s32 z) {
    cover(0x80088FBCu);
    Frame f(*this, 32);
    fx_8008721C(x, y, z);
    fx_800873E0(x, y, z);
    fx_light_at(x, y, z, 117964824);
    if (const u32 p = fx_timed()) {
        m.w32(p + 4, static_cast<u32>(x - 2457600));
        m.w32(p + 12, static_cast<u32>(z + 1638400));
        m.w32(p, 9306111);
        m.w32(p + 8, static_cast<u32>(y + 4096000));
        m.w32(p + 16, 0x80088A84u);
    }
    fx_replay_marker(3600u << 16, x, y, z, 0x80089C40u, true);
}

void Game::fx_80088968(s32 x, s32 y, s32 z) {
    cover(0x80088968u);
    Frame f(*this, 56);
    const s32 yy = y + 819200;
    s32 zz = z;
    for (s32 k = 0; k < 2; ++k) {
        fx_80087584(x, yy + 819200, zz);
        fx_8008721C(x - 7372800, yy, zz);
        fx_8008721C(x + 7372800, yy, zz);
        zz -= 6144000;
    }
    fx_timed_add(6422527, x, y, z, 0x80088748u);
}

void Game::fx_80088B48(s32 x, s32 y, s32 z) {
    cover(0x80088B48u);
    Frame f(*this, 48);
    s32 top = y, vy = -100, k = 0, yy = y;
    for (;;) {
        const u32 p = fx_flashes();
        if (!p) return;
        const s32 r = rand();
        m.w16(p + 18, static_cast<u32>(vy));
        vy -= 220;
        m.w32(p + 8, static_cast<u32>(yy));
        yy -= 0x100000;
        const s32 a = 2 * k, b = k << 8;
        ++k;
        m.w16(p, 0xFFFF);
        m.w16(p + 2, static_cast<u32>(a + (r & 7) + 40));
        m.w16(p + 16, static_cast<u32>(u8of(r) - 128));
        m.w16(p + 20, static_cast<u32>(u8of(r >> 7) - 128));
        m.w16(p + 22, 135);
        m.w16(p + 24, 528);
        m.w32(p + 4, static_cast<u32>(x));
        m.w32(p + 12, static_cast<u32>(z));
        m.w16(p + 26, static_cast<u32>(b + u8of(r) + 500));
        if (k >= 2) break;
    }
    s32 v2 = -100;
    for (s32 n = 0; n < 5; ++n) {
        const u32 q = fx_flashes();
        if (!q) return;
        const s32 r = rand();
        const s32 r2 = mix(r);
        m.w16(q + 18, static_cast<u32>(v2));
        v2 = v2 - 110 - ((r >> 8) & 7);
        const s32 qy = top + ((u8of(r >> 3) - 128) << 12);
        top -= 368640;
        m.w16(q, 0xFFFF);
        m.w16(q + 16, static_cast<u32>(u8of(r2) - 128));
        m.w16(q + 20, static_cast<u32>(u8of(r2 >> 7) - 128));
        m.w16(q + 22, 133);
        m.w16(q + 24, 256);
        m.w16(q + 2, static_cast<u32>(n + (r & 3) + 24));
        m.w32(q + 4, static_cast<u32>(x + ((u8of(r) - 128) << 12)));
        m.w32(q + 8, static_cast<u32>(qy));
        m.w32(q + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 12)));
        m.w16(q + 26, static_cast<u32>((n << 7) + ((r2 >> 4) & 0x7F) + 240));
    }
}

void Game::fx_80088EA8(s32 x, s32 y, s32 z) {
    cover(0x80088EA8u);
    Frame f(*this, 32);
    fx_80088B48(x, y, z);
    fx_80088D60(x, y, z);
    fx_light_at(x, y, z, 117964824);
    if (const u32 p = fx_timed()) {
        m.w32(p + 4, static_cast<u32>(x + 3686400));
        m.w32(p, 7995391);
        m.w32(p + 8, static_cast<u32>(y + 4096000));
        m.w32(p + 12, static_cast<u32>(z));
        m.w32(p + 16, 0x80088A84u);
    }
    fx_replay_marker(58982401, x, y, z, 0x80089C40u, false);
}

void Game::fx_80089880(s32 x, s32 y, s32 z) {
    cover(0x80089880u);
    Frame f(*this, 32);
    fx_80087520(x, y, z);
    if (const u32 p = fx_timed()) {
        m.w32(p + 4, static_cast<u32>(x + 1024000));
        m.w32(p + 12, static_cast<u32>(z - 1638400));
        m.w32(p, 5898239);
        m.w32(p + 8, static_cast<u32>(y + 2252800));
        m.w32(p + 16, 0x80088A84u);
    }
    fx_replay_marker(58982401, x, y, z, 0x80089C40u, false);
}

void Game::fx_8008978C(s32 x, s32 y, s32 z) {
    cover(0x8008978Cu);
    Frame f(*this, 32);
    fx_80087520(x, y, z);
    if (const u32 p = fx_timed()) {
        m.w32(p + 4, static_cast<u32>(x + 819200));
        m.w32(p + 12, static_cast<u32>(z - 1638400));
        m.w32(p, 6029311);
        m.w32(p + 8, static_cast<u32>(y + 737280));
        m.w32(p + 16, 0x80088A84u);
    }
    fx_replay_marker(19660801, x, y, z, 0x80089C40u, false);
}

void Game::fx_800885DC(s32 x, s32 y, s32 z) {
    cover(0x800885DCu);
    Frame f(*this, 48);
    fx_80087584(x + 122880, y + 1556480, z);
    for (s32 k = 0; k < 8; ++k) {
        const s32 r = rand();
        const u32 p = fx_debris();
        if (!p) break;
        m.w16(p + 2, static_cast<u32>((r & 0xF) + 40));
        m.w16(p, 0);
        m.w32(p + 4, static_cast<u32>((k & 1) ? x + 0x400000 : x - 0x800000));
        m.w16(p + 16, static_cast<u32>(u8of(r) - 128));
        m.w16(p + 18, static_cast<u32>(u8of(r >> 3) - 176));
        m.w16(p + 20, static_cast<u32>(u8of(r >> 6) - 128));
        m.w16(p + 22, 136);
        m.w16(p + 24, 6148);
        m.w32(p + 8, static_cast<u32>(y));
        m.w32(p + 12, static_cast<u32>(z));
        m.w16(p + 26, static_cast<u32>(u8of(r) + 300));
    }
    fx_timed_add(6029311, x, y, z, 0x80088454u);
}

void Game::fx_800878B4(s32 x, s32 y, s32 z) {
    cover(0x800878B4u);
    Frame f(*this, 48);
    s32 top = y, vy = 0, k = 0, yy = y;
    for (;;) {
        const u32 p = fx_flashes();
        if (!p) return;
        const s32 r = rand();
        m.w16(p + 18, static_cast<u32>(vy));
        vy -= 220;
        m.w32(p + 8, static_cast<u32>(yy));
        yy -= 0x100000;
        const s32 a = 2 * k, b = k << 8;
        ++k;
        m.w16(p, 0xFFFF);
        m.w16(p + 2, static_cast<u32>(a + (r & 7) + 40));
        m.w16(p + 16, static_cast<u32>(u8of(r) - 128));
        m.w16(p + 20, static_cast<u32>(u8of(r >> 7) - 128));
        m.w16(p + 22, 135);
        m.w16(p + 24, 528);
        m.w32(p + 4, static_cast<u32>(x));
        m.w32(p + 12, static_cast<u32>(z));
        m.w16(p + 26, static_cast<u32>(b + u8of(r) + 500));
        if (k >= 3) break;
    }
    s32 v2 = -140;
    for (s32 n = 0; n < 8; ++n) {
        const u32 q = fx_flashes();
        if (!q) return;
        const s32 r = rand();
        const s32 r2 = mix(r);
        const s32 base = v2 - 128;
        v2 -= 110;
        const s32 qy = top + ((u8of(r >> 3) - 128) << 13);
        top -= 327680;
        m.w16(q, 0xFFFF);
        m.w16(q + 16, static_cast<u32>(u8of(r2) - 128));
        m.w16(q + 20, static_cast<u32>(u8of(r2 >> 6) - 128));
        m.w16(q + 22, 133);
        m.w16(q + 24, 256);
        m.w16(q + 2, static_cast<u32>(n + (r & 3) + 22));
        m.w32(q + 4, static_cast<u32>(x + ((u8of(r) - 128) << 13)));
        m.w32(q + 8, static_cast<u32>(qy));
        m.w32(q + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 13)));
        m.w16(q + 18, static_cast<u32>(base + u8of(r2 >> 3)));
        m.w16(q + 26, static_cast<u32>((n << 7) + ((r2 >> 4) & 0x7F) + 260));
    }
}

void Game::fx_80087C18(s32 x, s32 y, s32 z) {
    cover(0x80087C18u);
    Frame f(*this, 32);
    fx_800878B4(x, y, z);
    fx_80087AD0(x, y, z);
    fx_light_at(x, y, z, 117964824);
}

void Game::fx_80089B24(s32 x, s32 y, s32 z) {
    cover(0x80089B24u);
    Frame f(*this, 32);
    fx_80087C18(x, y, z);
    fx_replay_marker(353894400, x, y, z, 0x80089CD8u, false);
}

void Game::fx_80089A94(s32 x, s32 y, s32 z) {
    cover(0x80089A94u);
    Frame f(*this, 32);
    fx_80087520(x, y, z);
    fx_replay_marker(589824000, x, y, z, 0x80089C40u, false);
}

void Game::fx_80089A04(s32 x, s32 y, s32 z) {
    cover(0x80089A04u);
    Frame f(*this, 32);
    fx_80087520(x, y, z);
    fx_replay_marker(58982400, x, y, z, 0x80089CD8u, false);
}

void Game::fx_80089974(s32 x, s32 y, s32 z) {
    cover(0x80089974u);
    Frame f(*this, 32);
    fx_800877FC(x, y, z);
    fx_replay_marker(58982400, x, y, z, 0x80089C40u, false);
}

void Game::fx_8008762C(s32 x, s32 y, s32 z) {
    cover(0x8008762Cu);
    Frame f(*this, 40);
    for (s32 k = 0; k < 8; ++k) {
        const u32 p = fx_debris();
        if (!p) break;
        const s32 r = rand();
        const s32 lo = r & 0xFF;
        const s32 a = u8of(r) - 128;
        m.w16(p + 16, static_cast<u32>(a));
        const s32 c = u8of(r >> 6) - 128;
        const s32 b = u8of(r >> 3);
        m.w16(p + 20, static_cast<u32>(c));
        const s32 vy = b - 272;
        m.w32(p + 8, static_cast<u32>(y + (a << 8)));
        m.w32(p + 4, static_cast<u32>(x + (c << 8)));
        m.w32(p + 12, static_cast<u32>(z + ((b - 128) << 8)));
        u32 look;
        if (k >= 4) {
            m.w16(p + 26, static_cast<u32>((lo & 0xF) + 25));
            look = 400;
            m.w16(p, 0);
            m.w16(p + 2, static_cast<u32>((lo & 0xF) + 30));
            m.w16(p + 24, 0);
        } else {
            m.w16(p + 2, static_cast<u32>((lo & 0xF) + 26));
            m.w16(p + 24, 6148);
            m.w16(p + 26, static_cast<u32>((lo & 0x7F) + 280));
            look = 141;
            m.w16(p, 0);
        }
        m.w16(p + 22, look);
        m.w16(p + 18, static_cast<u32>(vy));
    }
}

void Game::fx_8008775C(s32 x, s32 y, s32 z) {
    cover(0x8008775Cu);
    Frame f(*this, 32);
    const u32 p = fx_flashes();
    if (!p) return;
    const s32 r = u8of(rand());
    m.w16(p, 0xFFFF);
    m.w16(p + 2, static_cast<u32>((r & 7) + 20));
    m.w32(p + 4, static_cast<u32>(x));
    m.w32(p + 8, static_cast<u32>(y));
    m.w32(p + 12, static_cast<u32>(z));
    m.w16(p + 22, 0);
    m.w16(p + 24, 256);
    m.w16(p + 26, static_cast<u32>((r & 0x7F) + 220));
}

void Game::fx_800877FC(s32 x, s32 y, s32 z) {
    cover(0x800877FCu);
    Frame f(*this, 40);
    fx_8008775C(x, y, z);
    fx_8008762C(x, y, z);
    const u32 p = fx_light();
    if (!p) return;
    const s32 r = rand();
    m.w16(p + 2, static_cast<u32>((r & 3) + 10));
    m.w16(p + 20, 1);
    m.w16(p, 0);
    m.w32(p + 4, static_cast<u32>(x));
    m.w32(p + 8, static_cast<u32>(y));
    m.w32(p + 12, static_cast<u32>(z));
    m.w16(p + 22, static_cast<u32>((r & 0x7F) + 600));
    m.w16(p + 16, static_cast<u32>(r));
    m.w16(p + 18, static_cast<u32>(rand()));
}

void Game::fx_80087EE0(s32 x, s32 y, s32 z) {
    cover(0x80087EE0u);
    Frame f(*this, 48);
    s32 vy = -300;
    for (s32 k = 0; k < 16; ++k) {
        const u32 p = fx_debris();
        if (!p) break;
        const s32 r = rand();
        const s32 r2 = mix(r);
        const s32 v11 = vy + 4 * (u8of(r2 >> 3) - 128);
        vy -= 100;
        m.w16(p, 10);
        m.w16(p + 16, static_cast<u32>(4 * (u8of(r2) - 128)));
        m.w16(p + 20, static_cast<u32>(4 * (u8of(r2 >> 6) - 128)));
        m.w16(p + 22, 150);
        m.w16(p + 24, 7172);
        m.w16(p + 2, static_cast<u32>(k + 74));
        m.w32(p + 4, static_cast<u32>(x + ((u8of(r) - 128) << 12)));
        m.w32(p + 8, static_cast<u32>(y + ((u8of(r >> 3) - 128) << 12)));
        m.w32(p + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 12)));
        m.w16(p + 18, static_cast<u32>(v11));
        m.w16(p + 26, static_cast<u32>(32 * k + u8of(r2 >> 1) + 1200));
    }
    for (s32 k = 0; k < 8; ++k) {
        const u32 p = fx_debris();
        if (!p) break;
        const s32 r = rand();
        const s32 r2 = mix(r);
        m.w16(p + 2, static_cast<u32>(k + 50));
        m.w16(p + 16, static_cast<u32>(2 * (u8of(r2) - 128)));
        m.w16(p + 18, static_cast<u32>(2 * (u8of(r2 >> 3) - 128) - 256));
        m.w16(p + 20, static_cast<u32>(2 * (u8of(r2 >> 6) - 128)));
        m.w16(p + 22, 149);
        m.w16(p, 0);
        m.w32(p + 4, static_cast<u32>(x + ((u8of(r) - 128) << 12)));
        m.w32(p + 8, static_cast<u32>(y + ((u8of(r >> 3) - 128) << 12)));
        m.w32(p + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 12)));
        m.w16(p + 24, 0);
        m.w16(p + 26, static_cast<u32>(2 * k + (r2 & 0x1F) + 40));
    }
}

void Game::fx_80087C7C(s32 x, s32 y, s32 z) {
    cover(0x80087C7Cu);
    Frame f(*this, 48);
    s32 vy = -384, k = 0, yy = y;
    for (;;) {
        const u32 p = fx_flashes();
        if (!p) return;
        const s32 r = rand();
        m.w16(p, 0xFFFF);
        m.w16(p + 2, static_cast<u32>(k + 42));
        m.w32(p + 4, static_cast<u32>(x + ((u8of(r) - 128) << 13)));
        m.w32(p + 8, static_cast<u32>(yy + ((u8of(r >> 3) - 128) << 13)));
        m.w32(p + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 13)));
        const s32 s = rand();
        const s32 base = vy - 128;
        vy -= 100;
        yy -= 0x100000;
        ++k;
        m.w16(p + 16, static_cast<u32>(u8of(s) - 128));
        m.w16(p + 20, static_cast<u32>(u8of(s >> 6) - 128));
        m.w16(p + 22, 134);
        m.w16(p + 24, 528);
        m.w16(p + 18, static_cast<u32>(base + u8of(s >> 3)));
        m.w16(p + 26, static_cast<u32>(u8of(s) + 900));
        if (k >= 3) break;
    }
    s32 v2 = -340, cap = y + 2048000;
    for (s32 n = 0; n < 9; ++n) {
        const u32 q = fx_flashes();
        if (!q) return;
        const s32 r = rand();
        m.w16(q, 0xFFFF);
        m.w32(q + 4, static_cast<u32>(x + ((u8of(r) - 128) << 14)));
        m.w32(q + 8, static_cast<u32>(cap + ((u8of(r >> 3) - 128) << 14)));
        m.w16(q + 2, static_cast<u32>(2 * n + (r & 7) + 24));
        m.w32(q + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 14)));
        const s32 s = rand();
        m.w16(q + 18, static_cast<u32>(v2));
        v2 = v2 - 98 - (s & 7);
        cap -= 327680;
        m.w16(q + 16, static_cast<u32>(2 * (u8of(s) - 128)));
        m.w16(q + 20, static_cast<u32>(2 * (u8of(s >> 7) - 128)));
        m.w16(q + 22, 133);
        m.w16(q + 24, 256);
        m.w16(q + 26, static_cast<u32>((n << 7) + ((s >> 2) & 0x7F) + 496));
    }
}

void Game::fx_800881BC(s32 x, s32 y, s32 z) {
    cover(0x800881BCu);
    Frame f(*this, 32);
    fx_80087C7C(x, y, z);
    fx_80087EE0(x, y, z);
    fx_light_at(x, y, z, 196608030);
    if (const u32 p = fx_timed()) {
        m.w32(p, 1638399);
        m.w32(p + 16, 0x8008812Cu);
    }
}

void Game::fx_80083554(u32 pos, u32 puff) {
    cover(0x80083554u);
    Frame f(*this, 48);
    const s32 x = m.r32s(pos), z = m.r32s(pos + 8);
    s32 y = m.r32s(pos + 4) - 4300800;
    if (const u32 p = fx_debris()) {
        const s32 r = rand();
        m.w16(p, 10);
        m.w16(p + 2, static_cast<u32>((r & 0xF) + 40));
        m.w16(p + 16, static_cast<u32>(r & 0x3F));
        m.w16(p + 18, static_cast<u32>(-352));
        m.w16(p + 20, static_cast<u32>((r >> 6) & 0x3F));
        m.w16(p + 22, 133);
        m.w32(p + 4, static_cast<u32>(x));
        m.w32(p + 8, static_cast<u32>(y));
        m.w32(p + 12, static_cast<u32>(z));
        m.w16(p + 24, 6148);
        m.w16(p + 26, static_cast<u32>((r & 0x7F) + 200));
    }
    if (!puff) return;
    s32 vy = -320, life = 200;
    for (s32 k = 0; k < 5; ++k) {
        const u32 q = fx_flashes();
        if (!q) return;
        const s32 r = rand();
        const s32 r2 = mix(r);
        const s32 qy = y + ((u8of(r >> 3) - 128) << 8);
        y -= 327680;
        const s32 base = vy - 128;
        vy = vy - 96 - ((r >> 8) & 0xF);
        const s32 l = life + ((r >> 8) & 0x3F);
        life += 70 + ((r >> 8) & 7);
        m.w16(q, 0xFFFF);
        m.w16(q + 16, static_cast<u32>(u8of(r2) - 128));
        m.w16(q + 20, static_cast<u32>(u8of(r2 >> 6) - 128));
        m.w16(q + 22, 133);
        m.w16(q + 24, 256);
        m.w16(q + 2, static_cast<u32>(4 * k + (r & 3) + 16));
        m.w32(q + 4, static_cast<u32>(x + ((u8of(r) - 128) << 8)));
        m.w32(q + 8, static_cast<u32>(qy));
        m.w32(q + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 8)));
        m.w16(q + 18, static_cast<u32>(base + u8of(r2 >> 3)));
        m.w16(q + 26, static_cast<u32>(l));
    }
}

void Game::fx_80083764(u32 pos) {
    cover(0x80083764u);
    Frame f(*this, 48);
    s32 life = 220;
    const s32 x = m.r32s(pos), z = m.r32s(pos + 8), y = m.r32s(pos + 4) - 983040;
    for (s32 k = 0; k < 2; ++k) {
        const u32 p = fx_debris();
        if (!p) break;
        const s32 r = u8of(rand());
        const s32 l = life + (r & 0x7F);
        life += 80;
        m.w16(p + 2, static_cast<u32>(k + (r & 3) + 30));
        m.w16(p + 22, 397);
        m.w16(p + 24, 7680);
        m.w16(p, 0);
        m.w32(p + 4, static_cast<u32>(x));
        m.w32(p + 8, static_cast<u32>(y));
        m.w32(p + 12, static_cast<u32>(z));
        m.w16(p + 16, 0);
        m.w16(p + 18, 0);
        m.w16(p + 20, 0);
        m.w16(p + 26, static_cast<u32>(l));
    }
}

void Game::fx_smoke(u32 pos, u32 puff, bool large) {
    cover(large ? 0x80083844u : 0x80083A40u);
    Frame f(*this, 40);
    const s32 x = m.r32s(pos), y = m.r32s(pos + 4), z = m.r32s(pos + 8);
    if (const u32 p = fx_debris()) {
        const s32 r = rand();
        const s32 r2 = mix(r);
        m.w16(p, 10);
        m.w16(p + 16, static_cast<u32>((u8of(r2) + 20) & 0x7F));
        m.w16(p + 20, static_cast<u32>((u8of(r2 >> 6) + 50) & 0x7F));
        m.w16(p + 18, static_cast<u32>(-400));
        m.w16(p + 22, 135);
        m.w16(p + 24, large ? 6660u : 6404u);
        m.w16(p + 2, static_cast<u32>(large ? (r & 0xF) + 40 : (r & 7) + 30));
        m.w32(p + 4, static_cast<u32>(x + ((u8of(r) - 128) << 10)));
        m.w32(p + 8, static_cast<u32>(y + ((u8of(r >> 3) - 128) << 10)));
        m.w32(p + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 10)));
        m.w16(p + 26, static_cast<u32>(((r >> 8) & 0x7F) + (large ? 500 : 400)));
    }
    if (!puff) return;
    const u32 q = fx_flashes();
    if (!q) return;
    const s32 r = rand();
    const u32 sh = large ? 11 : 10;
    m.w16(q, large ? 10u : 8u);
    m.w16(q + 2, static_cast<u32>(large ? (r & 0xF) + 100 : (r & 7) + 68));
    m.w32(q + 4, static_cast<u32>(x + ((u8of(r) - 128) << sh)));
    m.w32(q + 8, static_cast<u32>(y + ((u8of(r >> 3) - 128) << sh)));
    m.w32(q + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << sh)));
    const s32 s = rand();
    m.w16(q + 16, static_cast<u32>((s & 0x7F) + 20));
    m.w16(q + 20, static_cast<u32>(((s >> 7) & 0x7F) + 50));
    m.w16(q + 18, static_cast<u32>(-400 - (s & (large ? 0xF : 0x1F))));
    m.w16(q + 22, 128);
    m.w16(q + 24, 528);
    m.w16(q + 26, static_cast<u32>(((s >> 4) & 0x7F) + (large ? 500 : 400)));
}

void Game::call_effect_fn(u32 fn, s32 x, s32 y, s32 z) {
    switch (fn) {
    case 0x80087520u: fx_80087520(x, y, z); break;
    case 0x800877FCu: fx_800877FC(x, y, z); break;
    case 0x80087C18u: fx_80087C18(x, y, z); break;
    case 0x800881BCu: fx_800881BC(x, y, z); break;
    case 0x80089974u: fx_80089974(x, y, z); break;
    case 0x80089A04u: fx_80089A04(x, y, z); break;
    case 0x80089A94u: fx_80089A94(x, y, z); break;
    case 0x80089B24u: fx_80089B24(x, y, z); break;
    case 0x800885DCu: fx_800885DC(x, y, z); break;
    case 0x8008978Cu: fx_8008978C(x, y, z); break;
    case 0x80089880u: fx_80089880(x, y, z); break;
    case 0x80088EA8u: fx_80088EA8(x, y, z); break;
    case 0x80088968u: fx_80088968(x, y, z); break;
    case 0x80088FBCu: fx_80088FBC(x, y, z); break;
    case 0x800890D8u: fx_800890D8(x, y, z); break;
    case 0x800892FCu: fx_800892FC(x, y, z); break;
    case 0x80089488u: fx_80089488(x, y, z); break;
    case 0x80089504u: fx_80089504(x, y, z); break;
    case 0x80087584u: fx_80087584(x, y, z); break;
    default: call_by_address(fn, "destruction effect", static_cast<u32>(x), static_cast<u32>(y), static_cast<u32>(z)); break;
    }
}

void Game::call_scenery_fn(u32 fn, u32 pos, u32 variant) {
    switch (fn) {
    case 0x80083554u: fx_80083554(pos, variant); break;
    case 0x80083764u: fx_80083764(pos); break;
    case 0x80083844u: fx_smoke(pos, variant, true); break;
    case 0x80083A40u: fx_smoke(pos, variant, false); break;
    default: call_by_address(fn, "scenery effect", pos, variant); break;
    }
}

void Game::fx_80088254(s32 x, s32 y, s32 z) {
    cover(0x80088254u);
    Frame f(*this, 40);
    if (const u32 p = fx_flashes()) {
        const s32 r = rand();
        m.w16(p, static_cast<u32>(~(r & 3)));
        m.w16(p + 2, static_cast<u32>((r & 0x1F) + 32));
        m.w32(p + 4, static_cast<u32>(x + ((u8of(r) - 128) << 12)));
        m.w32(p + 8, static_cast<u32>(y + ((u8of(r >> 3) - 128) << 12)));
        m.w32(p + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 12)));
        const s32 s = rand();
        m.w16(p + 16, static_cast<u32>((s & 0x7F) - 64));
        m.w16(p + 18, static_cast<u32>(((s >> 3) & 0x7F) - 320));
        m.w16(p + 20, static_cast<u32>(((s >> 6) & 0x7F) - 64));
        m.w16(p + 22, 132);
        m.w16(p + 24, 528);
        m.w16(p + 26, static_cast<u32>(u8of(s) + 600));
    }
    for (s32 k = 0; k < 3; ++k) {
        const u32 q = fx_debris();
        if (!q) break;
        const s32 r = rand();
        const s32 r2 = mix(r);
        m.w16(q, 8);
        m.w16(q + 16, static_cast<u32>(((r >> 8) & 0x7F) - 64));
        m.w16(q + 18, static_cast<u32>(((r2 >> 3) & 0x7F) - 560));
        m.w16(q + 20, static_cast<u32>(((r2 >> 6) & 0x7F) - 64));
        m.w16(q + 22, 141);
        m.w16(q + 24, 6148);
        m.w16(q + 26, static_cast<u32>(u8of(r2) + 700));
        m.w16(q + 2, static_cast<u32>((r & 0x1F) + 48));
        m.w32(q + 4, static_cast<u32>(x + ((u8of(r) - 128) << 12)));
        m.w32(q + 8, static_cast<u32>(y + ((u8of(r >> 3) - 128) << 12)));
        m.w32(q + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 12)));
    }
}

u32 Game::flash_red(u32 ot, u32 packet, s32 level) {
    cover(0x80093C90u);
    if (!level) return packet;
    gte.ctc2(21, 0x30);
    gte.ctc2(22, 0);
    gte.ctc2(23, 0);
    gte.mtc2(6, 0x6203135Fu);
    gte.mtc2(8, static_cast<u32>(0x1000 - level));
    gte.op(0x780010);
    m.w32(packet, m.r32(ot) | 0x3000000u);
    m.w32(ot, packet & 0xFFFFFFu);
    m.w32(packet + 8, 0xFF88FF60u);
    m.w32(packet + 12, 0xF00140u);
    m.w32(packet + 4, gte.mfc2(22));
    return packet + 0x10;
}

void Game::cb_8008812C(s32 age, u32 e) {
    cover(0x8008812Cu);
    Frame f(*this, 24);
    const s32 level = static_cast<s32>(wrap::div(2560 * (24 - age), 24).lo);
    if (level >= 256) {
        const u32 w = m.r32(g::kWorld);
        m.w32(w + 20, flash_red(m.r32(w + 16) + 8, m.r32(w + 20), level));
        m.w32(m.r32(kFx) + 1480, 1);
    } else {
        m.w16(e, m.r16(e + 2));
    }
}

void Game::cb_wreck_smoke(s32 age, u32 e, u32 mode) {
    cover(mode == 2 ? 0x80089CD8u : 0x80089C40u);
    Frame f(*this, 32);
    if (age & 7) return;
    const u32 variant = static_cast<u32>((age >> 3) & 1);
    if (m.r32(m.r32(g::kWorld)) & 2u) {
        if (distance(e + 4, slot(0) + 8) <= 0x7FFF) scenery_effect_timed(e + 4, variant, mode);
    } else {
        scenery_effect(e + 4, variant, mode);
    }
}

void Game::cb_80088454(s32 age, u32 e) {
    cover(0x80088454u);
    Frame f(*this, 48);
    if (age < 38 && !(age & 3)) {
        const s32 r = rand();
        if (const u32 p = fx_debris()) {
            m.w16(p, 0);
            m.w16(p + 2, static_cast<u32>((r & 0x1F) + 30));
            m.w32(p + 4, m.r32(e + 4) - 0x800000u);
            m.w32(p + 8, m.r32(e + 8));
            const u32 z = m.r32(e + 12);
            m.w16(p + 16, static_cast<u32>(u8of(r) - 128));
            m.w16(p + 18, static_cast<u32>(u8of(r >> 3) - 144));
            m.w16(p + 20, static_cast<u32>(u8of(r >> 6) - 128));
            m.w16(p + 22, 136);
            m.w16(p + 24, 5892);
            m.w16(p + 26, static_cast<u32>(u8of(r) + 200));
            m.w32(p + 12, z);
        }
    }
    if (age != 90) return;
    const s32 z = m.r32s(e + 12);
    s32 x = m.r32s(e + 4) - 0x200000, y = m.r32s(e + 8) + 6184960;
    for (s32 k = 0; k < 5; ++k) {
        fx_80088254(x, y, z);
        x -= 1310720;
        y -= 491520;
    }
    if (m.r32(m.r32(g::kWorld)) & 2u) sound_at_repeat(24, e + 4, 1);
}

void Game::cb_80088748(s32 age, u32 e) {
    cover(0x80088748u);
    Frame f(*this, 56);
    if (age == 80 && (m.r32(m.r32(g::kWorld)) & 2u)) sound_at_repeat(24, e + 4, 1);
    if (static_cast<u32>(age - 80) < 16) {
        const s32 step = 460800 * (age - 80);
        s32 z = m.r32s(e + 12), x = m.r32s(e + 4) - step;
        const s32 y = m.r32s(e + 8) + 4096000;
        for (s32 k = 0; k < 4; ++k) {
            const u32 p = fx_flashes();
            if (!p) break;
            const s32 r = rand();
            m.w16(p, 0xFFFF);
            m.w16(p + 2, static_cast<u32>((r & 0xF) + 20));
            m.w32(p + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 11)));
            m.w16(p + 24, 304);
            m.w32(p + 4, static_cast<u32>(x + ((u8of(r) - 128) << 11)));
            m.w32(p + 8, static_cast<u32>(y));
            m.w16(p + 22, 0);
            m.w16(p + 26, static_cast<u32>(u8of(r) + 320));
            if (k == 1) x += 2 * step;
            z += (k & 1) ? 6144000 : -6144000;
        }
        return;
    }
    if (!(age < 80)) return;
    const s32 r = static_cast<s16>(rand());
    s32 z = m.r32s(e + 12);
    if (r & 1) z -= 6144000;
    const s32 x = m.r32s(e + 4) + (((r & 0xFFF) - 2048) << 11);
    const s32 y = m.r32s(e + 8) + 49152 * age;
    if (r & 2) fx_8008775C(x, y, z);
    else if (age & 3) fx_8008762C(x, y, z);
    else fx_800873E0(x, y + 327680, z);
}

void Game::cb_80088A84(s32 age, u32 e) {
    cover(0x80088A84u);
    Frame f(*this, 48);
    if (age != m.r16s(e + 2) - 1) return;
    const s32 y = m.r32s(e + 8), z = m.r32s(e + 12);
    s32 x = m.r32s(e + 4) - 2048000;
    for (s32 k = 0; k < 4; ++k) {
        fx_80088254(x, y, z);
        x += 1024000;
    }
    if (m.r32(m.r32(g::kWorld)) & 2u) sound_at_repeat(24, e + 4, 1);
}

void Game::cb_burning(s32 age, u32 e, bool along_x) {
    cover(along_x ? 0x80089378u : 0x800891ECu);
    Frame f(*this, 32);
    if (age < 40 && !(age & 3)) {
        const s32 r = u8of(rand());
        if (age < 20 || (r & 1)) {
            if (along_x) fx_8008762C(m.r32s(e + 4) + ((r & 1) << 22), m.r32s(e + 8), m.r32s(e + 12) + ((r - 128) << 11));
            else fx_8008762C(m.r32s(e + 4) + ((r - 128) << 11), m.r32s(e + 8), m.r32s(e + 12) + ((r & 1) << 22));
        }
    }
    u32 k = static_cast<u32>(age - 82);
    if (age == 82) {
        if (m.r32(m.r32(g::kWorld)) & 2u) sound_at_repeat(24, e + 4, 1);
        k = 0;
    }
    if (!(k < 11 && (age & 1))) return;
    const s32 off = static_cast<s32>(wrap::div(static_cast<s32>(k << 22), 10).lo);
    if (along_x) fx_80088254(m.r32s(e + 4) + off, m.r32s(e + 8) + 2949120, m.r32s(e + 12));
    else fx_80088254(m.r32s(e + 4), m.r32s(e + 8) + 2949120, m.r32s(e + 12) + off);
}

bool Game::call_timed_fx(u32 fn, s32 age, u32 e) {
    switch (fn) {
    case 0x8008812Cu: cb_8008812C(age, e); return true;
    case 0x80089CD8u: cb_wreck_smoke(age, e, 2); return true;
    case 0x80089C40u: cb_wreck_smoke(age, e, 3); return true;
    case 0x80088454u: cb_80088454(age, e); return true;
    case 0x80088748u: cb_80088748(age, e); return true;
    case 0x80088A84u: cb_80088A84(age, e); return true;
    case 0x800891ECu: cb_burning(age, e, false); return true;
    case 0x80089378u: cb_burning(age, e, true); return true;
    default: return false;
    }
}
}

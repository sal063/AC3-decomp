#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kFx = 0x800BCF90u;
constexpr u32 kAtanLib = 0x800BB104u;
u32 flashes(Memory& m) { return m.r32(kFx) + 0x600; }
u32 lights(Memory& m) { return m.r32(kFx) + 0x33C0; }
u32 debris(Memory& m) { return m.r32(kFx) + 0x1098; }
s32 u8of(s32 v) { return v & 0xFF; }
}

s32 Game::ratan2_lib(s32 y, s32 x) {
    cover(0x800A7BB8u);
    const bool neg_x = x < 0, neg_y = y < 0;
    if (neg_x) x = -x;
    if (neg_y) y = -y;
    if (x == 0 && y == 0) return 0;
    s32 v;
    if (y < x) {
        const s32 q = (y & 0x7FE00000) ? static_cast<s32>(wrap::div(y, x >> 10).lo)
                                       : static_cast<s32>(wrap::div(y << 10, x).lo);
        v = m.r16s(kAtanLib + 2u * static_cast<u32>(q));
    } else {
        const s32 q = (x & 0x7FE00000) ? static_cast<s32>(wrap::div(x, y >> 10).lo)
                                       : static_cast<s32>(wrap::div(x << 10, y).lo);
        v = 0x400 - m.r16s(kAtanLib + 2u * static_cast<u32>(q));
    }
    if (neg_x) v = 0x800 - v;
    if (neg_y) v = -v;
    return v;
}

u32 Game::surface_angles(u32 normal) {
    cover(0x80089E48u);
    Frame f(*this, 32);
    const s32 x = m.r16s(normal), z = m.r16s(normal + 4), y = m.r16s(normal + 2);
    s32 heading = 0, pitch;
    if (x == 0 && z == 0) {
        pitch = y <= 0 ? 0x400 : -0x400;
    } else {
        gte.mtc2(9, static_cast<u32>(x));
        gte.mtc2(10, static_cast<u32>(z));
        heading = ratan2_lib(x, z);
        gte.op(0xA00428);
        const s32 flat = isqrt(static_cast<s32>(gte.mfc2(25) + gte.mfc2(26)));
        pitch = ratan2_lib(-y, flat);
    }
    return (static_cast<u32>(heading) << 16) | (static_cast<u32>(pitch) & 0xFFFFu);
}

void Game::hit_sparks(u32 pos, u32 kind) {
    cover(0x8007FE28u);
    Frame f(*this, 48);
    if (kind >= 6) return;
    const s32 x = m.r32s(pos), y = m.r32s(pos + 4), z = m.r32s(pos + 8);
    if (const u32 p = pool_take(flashes(m), 3, 7)) {
        s32 size = 50;
        m.w32(p, 0x8FFFFu);
        m.w32(p + 4, static_cast<u32>(x));
        m.w32(p + 8, static_cast<u32>(y));
        m.w32(p + 12, static_cast<u32>(z));
        m.w16(p + 22, 0);
        if (kind == 2) size = 100;
        m.w16(p + 24, 256);
        m.w16(p + 26, static_cast<u32>(size + (rand() & 0x1F)));
    }
    s32 n = kind == 2 ? 5 : 2;
    do {
        const u32 q = pool_take(debris(m), 10, 7);
        if (!q) break;
        m.w16(q, 0);
        m.w16(q + 2, static_cast<u32>(n + 29));
        const s32 r = rand();
        const s32 r2 = (r >> 8) | (r << 7);
        m.w16(q + 16, static_cast<u32>(((r >> 8) & 0x7F) - 64));
        m.w16(q + 18, static_cast<u32>(((r2 >> 3) & 0x7F) - 80));
        m.w16(q + 20, static_cast<u32>(((r2 >> 6) & 0x7F) - 64));
        m.w32(q + 4, static_cast<u32>(x + (((r & 0x7F) - 64) << 10)));
        m.w32(q + 8, static_cast<u32>(y + ((((r >> 3) & 0x7F) - 64) << 10)));
        m.w32(q + 12, static_cast<u32>(z + ((((r >> 6) & 0x7F) - 64) << 10)));
        u32 look;
        if (n == 1) {
            m.w16(q + 24, 5636);
            m.w16(q + 26, static_cast<u32>(((r >> 8) & 0x3F) + 110));
            look = 145;
            m.w16(q + 22, 0);
        } else {
            m.w16(q + 26, static_cast<u32>(n + ((r >> 8) & 3) + 11));
            look = 392;
            m.w16(q + 24, 0);
        }
        --n;
        m.w16(q + 22, look);
    } while (n > 0);
    if (kind < 3) return;
    const u32 q = pool_take(debris(m), 10, 7);
    if (!q) return;
    s32 life;
    if (kind == 3) {
        m.w32(q, 0xE0001u);
        m.w16(q + 24, 2049);
        life = (rand() & 0x1F) + 144;
    } else {
        m.w32(q, 0x120001u);
        m.w16(q + 24, kind == 4 ? 3072u : 3075u);
        life = (rand() & 0x1F) + 184;
    }
    m.w16(q + 26, static_cast<u32>(life));
    m.w32(q + 4, static_cast<u32>(x));
    m.w32(q + 8, static_cast<u32>(y));
    m.w32(q + 12, static_cast<u32>(z));
    const s32 r = rand();
    m.w16(q + 16, static_cast<u32>((r & 0x3F) - 32));
    m.w16(q + 18, static_cast<u32>(((r >> 3) & 0x3F) - 132));
    m.w16(q + 20, static_cast<u32>(((r >> 6) & 0x3F) - 32));
    m.w16(q + 22, 144);
}

void Game::hit_sparks_recorded(u32 pos, u32 kind) {
    cover(0x800800E0u);
    Frame f(*this, 32);
    const u32 r = record_effect(0, pos);
    m.w32(r + 8, (m.r32(r + 8) & 0xFFFFF00Fu) | ((kind & 0xFFu) << 4));
    hit_sparks(pos, kind);
}

void Game::ground_hit(u32 pos, u32 surface, u32 kind) {
    cover(0x80080140u);
    Frame f(*this, 48);
    if (kind >= 6) return;
    const s32 x = m.r32s(pos), y = m.r32s(pos + 4), z = m.r32s(pos + 8);
    if (kind < 3) {
        const u32 p = pool_take(flashes(m), 3, 7);
        m.w32(p + 4, static_cast<u32>(x));
        m.w32(p + 8, static_cast<u32>(y));
        m.w32(p + 12, static_cast<u32>(z));
        auto flash = [&](u32 head, u32 look) {
            m.w32(p, head);
            m.w16(p + 22, 0);
            m.w32(p + 24, look);
        };
        if (kind == 2) {
            if (surface == 1) {
                flash(0xEFFFFu, 0x640120u);
                return;
            }
            m.w32(p, 0xBFFFFu);
            m.w16(p + 22, 0);
            m.w16(p + 24, 256);
            m.w16(p + 26, static_cast<u32>((rand() & 0x7F) + 120));
            for (s32 k = 0; k < 4; ++k) {
                const u32 q = pool_take(debris(m), 10, 7);
                if (!q) break;
                m.w16(q, 0);
                m.w16(q + 2, static_cast<u32>(k + 16));
                const s32 r = rand();
                const s32 r2 = (r >> 8) | (r << 7);
                m.w16(q + 16, static_cast<u32>(4 * (u8of(r2) - 128)));
                m.w16(q + 18, static_cast<u32>(((r2 >> 1) & 0x3FC) - 640));
                m.w16(q + 20, static_cast<u32>(4 * (u8of(r2 >> 6) - 128)));
                m.w16(q + 22, 394);
                m.w32(q + 4, static_cast<u32>(x + ((u8of(r) - 128) << 8)));
                m.w32(q + 8, static_cast<u32>(y + ((u8of(r >> 3) - 128) << 8) - 163840));
                m.w32(q + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 8)));
                m.w16(q + 24, 0);
                m.w16(q + 26, static_cast<u32>((k & 3) + 14));
            }
            return;
        }
        switch (surface) {
        case 1: flash(0xCFFFFu, 0x500120u); break;
        case 2: flash(0xAFFFFu, 0x500121u); break;
        case 3: flash(0xAFFFFu, 0x500122u); break;
        case 4: {
            m.w32(p, 0xCFFFFu);
            const s32 r = rand();
            m.w16(p + 16, static_cast<u32>((r & 0x3F) - 32));
            m.w16(p + 20, static_cast<u32>(((r >> 6) & 0x3F) - 32));
            m.w16(p + 18, static_cast<u32>(((r >> 3) & 0x3F) - 32));
            m.w16(p + 22, 136);
            m.w32(p + 24, 0x640123u);
            break;
        }
        case 5: flash(0xAFFFFu, 0x500124u); break;
        default: flash(0xAFFFFu, 0x500000u); break;
        }
        return;
    }
    const u32 q = pool_take(debris(m), 10, 7);
    if (!q) return;
    m.w16(q + 22, 144);
    m.w32(q + 4, static_cast<u32>(x));
    m.w32(q + 8, static_cast<u32>(y));
    m.w32(q + 12, static_cast<u32>(z));
    m.w16(q + 20, 0);
    if (kind == 3) {
        m.w32(q, 0x100000u);
        m.w32(q + 16, 0xFF800000u);
        m.w16(q + 24, 1537);
        m.w16(q + 26, static_cast<u32>((rand() & 0x3F) + 208));
        return;
    }
    m.w32(q, 0x140000u);
    m.w32(q + 16, 0xFF600000u);
    m.w16(q + 24, kind == 4 ? 2048u : 2051u);
    m.w16(q + 26, static_cast<u32>((rand() & 0x3F) + 268));
}

void Game::ground_hit_recorded(u32 pos, u32 surface, u32 kind) {
    cover(0x800804B8u);
    Frame f(*this, 32);
    const u32 s = surface & 0xFu;
    const u32 r = record_effect(1, pos);
    m.w32(r + 8, (m.r32(r + 8) & 0xFFFFF000u) | ((kind & 0xFFu) << 4) | s);
    ground_hit(pos, s, kind);
}

void Game::air_burst(u32 pos, u32 vel) {
    cover(0x80080534u);
    Frame f(*this, 56);
    const s32 x = m.r32s(pos), y = m.r32s(pos + 4), z = m.r32s(pos + 8);
    if (const u32 p = pool_take(flashes(m), 3, 7)) {
        m.w32(p, 0xEFFFFu);
        m.w32(p + 4, static_cast<u32>(x));
        m.w32(p + 8, static_cast<u32>(y));
        m.w32(p + 12, static_cast<u32>(z));
        m.w16(p + 22, 0);
        m.w32(p + 24, 0x1000100u);
    }
    if (const u32 q = pool_take(lights(m), 1, 6)) {
        const s32 r = rand();
        m.w16(q, 0);
        m.w16(q + 2, static_cast<u32>((r & 3) + 9));
        m.w32(q + 4, static_cast<u32>(x));
        m.w32(q + 8, static_cast<u32>(y));
        m.w32(q + 12, static_cast<u32>(z));
        m.w16(q + 20, 0);
        m.w16(q + 22, static_cast<u32>((r & 0x3F) + 400));
    }
    const s32 dx = m.r32s(vel) >> 9, dy = m.r32s(vel + 4) >> 9, dz = (m.r32s(vel + 8) >> 9) - 128;
    for (s32 k = 0; k < 8; ++k) {
        const u32 q = pool_take(debris(m), 10, 7);
        if (!q) break;
        m.w16(q, 0);
        m.w16(q + 2, static_cast<u32>(k + 32));
        const s32 r = rand();
        const s32 r2 = (r >> 8) | (r << 7);
        m.w32(q + 4, static_cast<u32>(x + ((u8of(r) - 128) << 8)));
        m.w16(q + 16, static_cast<u32>(dx - 128 + u8of(r2)));
        m.w16(q + 18, static_cast<u32>(dy + u8of(r2 >> 3) - 256));
        m.w16(q + 20, static_cast<u32>(dz + u8of(r2 >> 6)));
        m.w32(q + 8, static_cast<u32>(y + ((u8of(r >> 3) - 128) << 8)));
        m.w32(q + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 8)));
        if (k >= 2) {
            m.w16(q + 22, 392);
            m.w16(q + 24, 0);
            m.w16(q + 26, static_cast<u32>(k + 20));
        } else {
            m.w16(q + 22, 147);
            m.w16(q + 24, 5380);
            m.w16(q + 26, static_cast<u32>(((r >> 8) & 0x1F) + 240));
        }
    }
}

void Game::explosion_air(u32 pos, u32 vel, u32 b2) {
    cover(0x8008074Cu);
    Frame f(*this, 32);
    m.w8(record_motion(4, 0, pos, vel) + 2, b2);
    air_burst(pos, vel);
}

void Game::fireball(u32 pos, u32 vel, u32 angles) {
    cover(0x800807A4u);
    Frame f(*this, 56);
    const s32 x = m.r32s(pos), y = m.r32s(pos + 4), z = m.r32s(pos + 8);
    if (const u32 p = pool_take(flashes(m), 3, 7)) {
        m.w32(p, 0xEFFFFu);
        m.w32(p + 4, static_cast<u32>(x));
        m.w32(p + 8, static_cast<u32>(y));
        m.w32(p + 12, static_cast<u32>(z));
        m.w16(p + 22, 0);
        m.w32(p + 24, 0x1000100u);
    }
    if (const u32 q = pool_take(lights(m), 1, 6)) {
        const s32 r = rand();
        m.w16(q + 2, static_cast<u32>((r & 3) + 9));
        m.w16(q, 0);
        m.w32(q + 4, static_cast<u32>(x));
        m.w32(q + 8, static_cast<u32>(y));
        m.w32(q + 12, static_cast<u32>(z));
        m.w32(q + 16, angles);
        m.w16(q + 20, 1);
        m.w16(q + 22, static_cast<u32>((r & 0x3F) + 400));
    }
    const s32 dx = (0 - m.r32s(vel)) >> 11, dy = (0 - m.r32s(vel + 4)) >> 11, dz = (0 - m.r32s(vel + 8)) >> 11;
    for (s32 k = 0; k < 8; ++k) {
        const u32 q = pool_take(debris(m), 10, 7);
        if (!q) break;
        m.w16(q, 0);
        m.w16(q + 2, static_cast<u32>(k + 24));
        const s32 r = rand();
        const s32 r2 = (r >> 8) | (r << 7);
        m.w16(q + 16, static_cast<u32>(dx + 2 * (u8of(r2) - 128)));
        m.w16(q + 18, static_cast<u32>(dy + ((r2 >> 2) & 0x1FE) - 304));
        m.w16(q + 20, static_cast<u32>(dz + 2 * (u8of(r2 >> 6) - 128)));
        m.w32(q + 4, static_cast<u32>(x + ((u8of(r) - 128) << 9)));
        m.w32(q + 8, static_cast<u32>(y + ((u8of(r >> 3) - 128) << 9)));
        m.w32(q + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 9)));
        if (k >= 2) {
            m.w16(q + 22, 392);
            m.w16(q + 24, 0);
            m.w16(q + 26, static_cast<u32>(k + 20));
        } else {
            m.w16(q + 22, 139);
            m.w16(q + 24, 5380);
            m.w16(q + 26, static_cast<u32>(((r >> 8) & 0x1F) + 240));
            m.w16(q + 18, m.r16(q + 18) - 128u);
        }
    }
}

void Game::explosion_big(u32 pos, u32 vel, u32 normal, u32 b2) {
    cover(0x800809F0u);
    Frame f(*this, 40);
    const u32 a = surface_angles(normal);
    const u32 r = record_motion(5, 0, pos, vel);
    const u32 w = m.r32(r + 16);
    m.w8(r + 2, b2);
    m.w32(r + 16, (w & 0xFF000000u) | (a & 0xFFFu) | (((a >> 16) & 0xFFFu) << 12));
    fireball(pos, vel, a);
}

void Game::blast_small(s32 x, s32 y, s32 z) {
    cover(0x80080A94u);
    Frame f(*this, 48);
    if (const u32 q = pool_take(lights(m), 1, 6)) {
        m.w32(q, 0x29000Au);
        m.w32(q + 4, static_cast<u32>(x));
        m.w32(q + 8, static_cast<u32>(y));
        m.w32(q + 12, static_cast<u32>(z));
        m.w32(q + 20, 0x3200002u);
    }
    s32 life = 800;
    for (s32 k = 0; k < 20; ++k) {
        const u32 p = pool_take(debris(m), 10, 7);
        if (!p) break;
        m.w16(p, 0);
        m.w16(p + 2, static_cast<u32>((k & 3) + 60));
        const s32 r = rand();
        const s32 r2 = (r >> 8) | (r << 7);
        m.w16(p + 26, static_cast<u32>(life));
        life += 2;
        m.w16(p + 16, static_cast<u32>(2 * (u8of(r2) - 128)));
        m.w16(p + 18, static_cast<u32>(((r2 >> 2) & 0x1FE) - 384));
        m.w16(p + 20, static_cast<u32>(2 * (u8of(r2 >> 6) - 128)));
        m.w16(p + 22, 398);
        m.w16(p + 24, 6658);
        m.w32(p + 4, static_cast<u32>(x + ((u8of(r) - 128) << 8)));
        m.w32(p + 8, static_cast<u32>(y + ((u8of(r >> 3) - 128) << 8)));
        m.w32(p + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 8)));
    }
}

void Game::blast_large(s32 x, s32 y, s32 z) {
    cover(0x80080C08u);
    Frame f(*this, 48);
    if (const u32 p = pool_take(flashes(m), 3, 7)) {
        const s32 r = rand();
        s32 order = -1;
        m.w16(p, 0xFFFFu);
        m.w16(p + 2, static_cast<u32>((r & 7) + 40));
        m.w16(p + 22, 149);
        m.w16(p + 18, static_cast<u32>(-512 - (r & 0x3F)));
        m.w16(p + 24, 528);
        m.w32(p + 4, static_cast<u32>(x));
        m.w32(p + 8, static_cast<u32>(y));
        m.w32(p + 12, static_cast<u32>(z));
        m.w16(p + 16, 0);
        m.w16(p + 20, 0);
        m.w16(p + 26, static_cast<u32>((r & 0x7F) + 720));
        for (s32 k = 0; k < 7; ++k) {
            const u32 q = pool_take(flashes(m), 3, 7);
            if (!q) break;
            const s32 s = rand();
            m.w16(q, static_cast<u32>(order));
            order -= 2;
            m.w16(q + 2, static_cast<u32>((s & 3) + 12));
            m.w32(q + 4, static_cast<u32>(x + ((u8of(s) - 128) << 14)));
            m.w32(q + 12, static_cast<u32>(z + ((u8of(s >> 6) - 128) << 14)));
            m.w16(q + 24, 256);
            m.w16(q + 26, static_cast<u32>(((s >> 7) & 0x7F) + 500));
            m.w32(q + 8, static_cast<u32>(y));
            m.w16(q + 22, 0);
        }
    }
    if (const u32 q = pool_take(lights(m), 1, 6)) {
        const s32 r = rand();
        m.w16(q + 2, static_cast<u32>((r & 3) + 24));
        m.w16(q + 20, 1);
        m.w16(q + 22, static_cast<u32>((r & 0x7F) + 1200));
        m.w16(q, 0);
        m.w32(q + 4, static_cast<u32>(x));
        m.w32(q + 8, static_cast<u32>(y));
        m.w32(q + 12, static_cast<u32>(z));
        m.w32(q + 16, 1024);
    }
    const s32 top = y - 327680;
    for (s32 k = 0; k < 10; ++k) {
        const u32 p = pool_take(debris(m), 10, 7);
        if (!p) break;
        const s32 r = rand();
        m.w16(p + 2, static_cast<u32>((r & 0xF) + 30));
        m.w32(p + 4, static_cast<u32>(x + ((u8of(r) - 128) << 13)));
        m.w16(p, 0);
        m.w32(p + 8, static_cast<u32>(top));
        m.w32(p + 12, static_cast<u32>(z + ((u8of(r >> 4) - 128) << 13)));
        const s32 s = rand();
        m.w16(p + 16, static_cast<u32>(2 * (u8of(s) - 128)));
        m.w16(p + 18, static_cast<u32>(((s >> 2) & 0x1FE) - 576));
        m.w16(p + 20, static_cast<u32>(2 * (u8of(s >> 6) - 128)));
        m.w16(p + 22, 141);
        m.w16(p + 24, 7172);
        m.w16(p + 26, static_cast<u32>(((s >> 8) & 0x7F) + 800));
    }
}

void Game::blast_effect(u32 pos, u32 size) {
    cover(0x80080F10u);
    Frame f(*this, 32);
    const u32 r = record_effect(5, pos);
    m.w32(r + 8, (m.r32(r + 8) & 0xFFFFF00Fu) | ((size & 0xFFu) << 4));
    blast_effect_fx(pos, size);
}

void Game::blast_effect_fx(u32 pos, u32 size) {
    cover(0x80080EB4u);
    Frame g(*this, 32);
    m.w32(g.at(0x10), 0x80080A94u);
    m.w32(g.at(0x14), 0x80080C08u);
    if (size & 1u) blast_large(m.r32s(pos), m.r32s(pos + 4), m.r32s(pos + 8));
    else blast_small(m.r32s(pos), m.r32s(pos + 4), m.r32s(pos + 8));
}
}

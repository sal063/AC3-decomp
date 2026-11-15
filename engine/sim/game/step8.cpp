#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kFx = 0x800BCF90u;
constexpr u32 kRecorder = 0x800BD0B8u;
constexpr u32 kSea = 0x800BCEB0u;
constexpr u32 kCamState = 0x800BD098u;
constexpr u32 kPlayerFlight = 0x800BD090u;
constexpr u32 kPuffTable = 0x800247ACu;
}

u32 Game::pool_take(u32 masks, s32 groups, s32 words) {
    cover(0x8007FDB4u);
    if (groups <= 0) return 0;
    u32 e = masks + 4 * static_cast<u32>(groups);
    const u32 stride = 4 * static_cast<u32>(words), group_bytes = static_cast<u32>(words) << 7;
    u32 at = masks;
    for (s32 k = 1;; ++k) {
        const u32 mask = m.r32(at);
        if (mask != 0xFFFFFFFFu) {
            u32 bit = 1, v = mask | 1u;
            if (mask & 1u) {
                do {
                    bit <<= 1;
                    e += stride;
                } while (mask & bit);
                v = mask | bit;
            }
            m.w32(at, v);
            return e;
        }
        at += 4;
        e += group_bytes;
        if (k >= groups) return 0;
    }
}

u32 Game::recorder_fold_move(u32 rec, u32 base) {
    cover(0x8007DD58u);
    const u32 h = m.r32(rec);
    if ((h >> 7) & 3u) return (h >> 7) & 3u;
    const u32 s = base + ((h >> 4) & 0x7E0u);
    m.w32(s + 8, static_cast<u32>((m.r32s(rec + 4) >> 12) << 12));
    m.w32(s + 12, static_cast<u32>((m.r32s(rec + 8) >> 12) << 12));
    m.w32(s + 16, static_cast<u32>((m.r32s(rec + 12) >> 12) << 12));
    m.w16(s + 24, static_cast<u32>(static_cast<s16>(m.r16(rec + 4) << 4) >> 2));
    m.w16(s + 26, static_cast<u32>(static_cast<s16>(m.r16(rec + 8) << 4) >> 2));
    const u32 roll = static_cast<u32>(static_cast<s16>(m.r16(rec + 12) << 4) >> 2);
    m.w16(s + 28, roll);
    return roll;
}

u32 Game::recorder_fold_state(u32 rec, u32 base) {
    cover(0x8007DC3Cu);
    const u32 h = m.r32(rec);
    const u32 s = base + ((h >> 5) & 0x7E0u);
    switch ((h >> 7) & 7u) {
    case 0: m.w32(m.r32(kRecorder) + 20, 1); return 1;
    case 1: {
        const u32 flags = (h & 0x10000u) ? (m.r32(s) | 0x14000u) : (m.r32(s) & 0xFFFEBFFFu);
        m.w32(s, flags);
        return flags;
    }
    case 2: {
        m.w8(s + 4, m.r8(rec + 2));
        const u32 state = m.r8(rec + 3);
        m.w8(s + 5, state);
        return state;
    }
    case 3: m.w16(s + 6, (h >> 16) & 0x7FFFu); return (h >> 5) & 0x7E0u;
    case 4: {
        s32 v = static_cast<s32>(m.r8(rec + 1) >> 2);
        if (!v) v = -1;
        const u32 r = m.r32(kRecorder);
        m.w32(r + 6208, static_cast<u32>(v));
        m.w32(r + 6192, 0);
        m.w32(r + 6196, 0);
        m.w32(r + 6200, 0);
        return r;
    }
    default: return 0x80020000u;
    }
}

u32 Game::recorder_fold(u32 rec) {
    cover(0x8007DDF4u);
    Frame f(*this, 24);
    const u32 kind = (m.r32(rec) >> 4) & 7u;
    const u32 base = m.r32(kRecorder) + 24;
    if (kind == 0) recorder_fold_state(rec, base);
    else if (kind == 1) recorder_fold_move(rec, base);
    return m.r32(rec) & 0xFu;
}

void Game::recorder_drop(s32 n) {
    cover(0x8007DE64u);
    Frame f(*this, 40);
    const u32 r = m.r32(kRecorder);
    u32 p = m.r32(r + 16);
    const u32 end = m.r32(r + 8);
    if (n > 0) {
        s32 done = 0;
        bool more;
        do {
            if (m.r32(p) == 1) m.w32(m.r32(kRecorder), m.r32(m.r32(kRecorder)) - 1);
            const u32 len = recorder_fold(p);
            p += 4 * len;
            done += static_cast<s32>(len);
            more = done < n;
            if (static_cast<s32>(p) >= static_cast<s32>(end)) {
                p = m.r32(m.r32(kRecorder) + 4);
                more = done < n;
            }
        } while (more);
    }
    m.w32(m.r32(kRecorder) + 16, p);
}

u32 Game::recorder_alloc(s32 n) {
    cover(0x8007DF28u);
    Frame f(*this, 32);
    const u32 r = m.r32(kRecorder);
    const u32 at = m.r32(r + 12);
    u32 next = at + 4 * static_cast<u32>(n);
    const u32 tail = m.r32(r + 16);
    if (static_cast<s32>(next) >= m.r32s(r + 8)) {
        next = m.r32(r + 4);
        if (!(static_cast<s32>(tail) < static_cast<s32>(at) && static_cast<s32>(next) < static_cast<s32>(tail)))
            recorder_drop(n);
    } else if (static_cast<s32>(tail) >= static_cast<s32>(at) && static_cast<s32>(next) >= static_cast<s32>(tail) &&
               m.r32(m.r32(kRecorder))) {
        recorder_drop(n);
    }
    m.w32(m.r32(kRecorder) + 12, next);
    return at;
}

u32 Game::record_effect(u32 kind, u32 pos) {
    cover(0x8007ECE8u);
    Frame f(*this, 32);
    const u32 r = recorder_alloc(3);
    u32 h = m.r32(r);
    m.w8(r + 4, kind);
    const u32 w1 = m.r32(r + 4), w2 = m.r32(r + 8);
    h = (((h & 0xFFFFFFF0u) | 3u) & 0xFFFFFF8Fu) | 0x30u;
    h &= 0xFFFFFE7Fu;
    m.w32(r, h);
    m.w32(r, (h & 0xFFFu) | static_cast<u32>((m.r32s(pos) >> 12) << 12));
    m.w32(r + 4, (w1 & 0xFFFu) | static_cast<u32>((m.r32s(pos + 4) >> 12) << 12));
    m.w32(r + 8, (w2 & 0xFFFu) | static_cast<u32>((m.r32s(pos + 8) >> 12) << 12));
    return r;
}

u32 Game::record_motion(u32 words, u32 tag, u32 pos, u32 vel) {
    cover(words == 4 ? 0x8007ED9Cu : 0x8007EEACu);
    Frame f(*this, 32);
    const u32 r = recorder_alloc(static_cast<s32>(words));
    u32 h = m.r32(r);
    u32 a = m.r32(r + 4) & 0xFFF00000u, b = m.r32(r + 8) & 0xFFF00000u, c = m.r32(r + 12) & 0xFFF00000u;
    h = (((h & 0xFFFFFFF0u) | words) & 0xFFFFFF8Fu) | 0x30u;
    h &= 0xFFFFFE7Fu;
    m.w32(r, h);
    m.w8(r + 3, tag);
    a |= m.r32(pos) >> 12;
    b |= m.r32(pos + 4) >> 12;
    c |= m.r32(pos + 8) >> 12;
    m.w32(r + 4, a);
    m.w32(r + 12, c);
    m.w32(r + 8, b);
    m.w32(r + 4, (a & 0xFFFFFu) | static_cast<u32>((m.r32s(vel) >> 12) << 20));
    m.w32(r + 8, (b & 0xFFFFFu) | static_cast<u32>((m.r32s(vel + 4) >> 12) << 20));
    m.w32(r + 12, (m.r32(r + 12) & 0xFFFFFu) | static_cast<u32>((m.r32s(vel + 8) >> 12) << 20));
    return r;
}

void Game::puff_spawn(u32 pos, u32 vel, u32 kind) {
    cover(0x800827E0u);
    Frame f(*this, 0x88);
    const u32 table = f.at(0x10);
    for (u32 k = 0; k < 88; k += 4) m.w32(table + k, m.r32(kPuffTable + k));
    if (kind >= 11) return;
    const u32 p = pool_take(m.r32(kFx) + 0x1098, 10, 7);
    if (!p) return;
    const s32 vx = m.r32s(vel), vy = m.r32s(vel + 4), vz = m.r32s(vel + 8);
    m.w32(p + 4, static_cast<u32>(m.r32s(pos) - vx - (vx >> 2)));
    m.w32(p + 8, static_cast<u32>(m.r32s(pos + 4) - vy - (vy >> 2)));
    m.w16(p + 0x10, static_cast<u32>(vx >> 8));
    m.w16(p + 0x12, static_cast<u32>(vy >> 8));
    m.w16(p + 0x14, static_cast<u32>(vz >> 8));
    m.w32(p + 12, static_cast<u32>(m.r32s(pos + 8) - vz - (vz >> 2)));
    const u32 look = m.r32(table + 8 * kind), look2 = m.r32(table + 8 * kind + 4);
    const u32 r = static_cast<u32>(rand());
    m.w32(p, static_cast<u32>((static_cast<s32>(look) >> 24) << 16));
    m.w16(p + 0x16, look2 >> 16);
    m.w16(p + 0x18, look2);
    m.w16(p + 0x1A, look + (r & ((look << 8) >> 24)));
}

void Game::puff_spawn_recorded(u32 pos, u32 vel, u32 kind) {
    cover(0x800829BCu);
    Frame f(*this, 32);
    if (kind == 1 && m.r16(m.r32(g::kScene) + 908) != 42) kind = 10;
    m.w8(record_motion(4, 6, pos, vel) + 2, kind);
    puff_spawn(pos, vel, kind);
}

void Game::spray_spawn(u32 pos, s32 strength) {
    cover(0x80082D80u);
    Frame f(*this, 40);
    const s32 s = strength + (rand() & 0x3F) - 0x20;
    if (s <= 0) return;
    const u32 p = pool_take(m.r32(kFx) + 0x600, 3, 7);
    if (!p) return;
    gte.mtc2(30, static_cast<u32>(s));
    m.w32(p, (static_cast<u32>((s >> 4) + 4) << 16) | 1u);
    m.w32(p + 0x18, (static_cast<u32>(s) << 16) | 0x130u);
    m.w16(p + 0x16, 0);
    const u32 lz = gte.mfc2(31);
    const s32 spread = static_cast<s32>(wrap::shl(1u, 0x1Fu - lz)) - 1;
    const s32 half = static_cast<s32>(wrap::shl(1u, 0x1Eu - lz));
    const s32 rx = (rand() & spread) - half;
    m.w32(p + 4, static_cast<u32>(m.r32s(pos) + (rx << 10)));
    const s32 rz = (rand() & spread) - half;
    m.w32(p + 12, static_cast<u32>(m.r32s(pos + 8) + (rz << 10)));
    m.w32(p + 8, m.r32(pos + 4));
}

void Game::water_wake(u32 pos, u32 vel, s32 sea_y) {
    cover(0x80082E80u);
    Frame f(*this, 56);
    if (m.r32(m.r32(g::kWorld)) & 0x100u) return;
    const s32 h = ((m.r32s(pos + 4) - sea_y) >> 12) + 1024;
    if (h < 0) return;
    if ((m.r32(m.r32(kCamState) + 56) & 0xFu) != 1) return;
    const s32 speed = 16 * length3(m.r32s(vel) >> 4, m.r32s(vel + 4) >> 4, m.r32s(vel + 8) >> 4);
    if (speed <= 35007) return;
    const s32 low = h - (h >> 2);
    s32 k = low + ((speed - 175042) >> 9);
    if (k < 0) return;
    if (k < 1024) k = (4096 - rcos(k)) >> 4;
    if (k < 80) return;
    if (k >= 256) k = 255;
    const u32 at = f.at(0x10);
    m.w32(at + 4, static_cast<u32>(sea_y));
    m.w32(at, static_cast<u32>(m.r32s(pos) - 2 * m.r32s(vel)));
    m.w32(at + 8, static_cast<u32>(m.r32s(pos + 8) - 2 * m.r32s(vel + 8)));
    const u32 r = record_effect(3, at);
    m.w32(r + 8, (m.r32(r + 8) & 0xFFFFF00Fu) | (16u * (static_cast<u32>(k) & 0xFFu)));
    spray_spawn(at, k);
}

void Game::player_exhaust(s32 mission, s32 variant) {
    cover(0x80084F6Cu);
    Frame f(*this, 48);
    s32 min_speed, min_throttle;
    u32 kind;
    if (mission == 58) {
        min_throttle = 1920;
        min_speed = 30;
        kind = 5;
    } else {
        if (static_cast<u32>(variant - 25) >= 2) return;
        min_throttle = 3072;
        min_speed = 60;
        kind = static_cast<u32>(variant - 19);
    }
    if (m.r16s(m.r32(kPlayerFlight) + 6) < min_throttle) return;
    const u32 p = slot(0);
    const u32 vel = p + 56;
    const s32 vx = m.r32s(vel), vy = m.r32s(p + 60), vz = m.r32s(p + 64);
    if (length3(vx >> 12, vy >> 12, vz >> 12) < min_speed) return;
    const s32 x = m.r32s(p + 8), y = m.r32s(p + 12), z = m.r32s(p + 16);
    m.w32(g::kScratch, static_cast<u32>(x));
    m.w32(g::kScratch + 4, static_cast<u32>(y));
    m.w32(g::kScratch + 8, static_cast<u32>(z));
    if (static_cast<u32>(variant - 25) >= 3) {
        m.w32(g::kScratch, static_cast<u32>(x - (vx - (vx >> 2))));
        m.w32(g::kScratch + 4, static_cast<u32>(y - (vy - (vy >> 2))));
        m.w32(g::kScratch + 8, static_cast<u32>(z - (vz - (vz >> 2))));
    }
    puff_spawn_recorded(g::kScratch, vel, kind);
}

void Game::sea_spray(u32 pos, u32 vel) {
    cover(0x80084E6Cu);
    Frame f(*this, 40);
    const s32 y = m.r32s(pos + 4), vy = m.r32s(vel + 4);
    const s32 k = ((y - m.r32s(m.r32(kSea) + 4)) >> 12) + (vy >> 10);
    if (k < 0) return;
    if (k < (rand() & 0xFFF)) return;
    const s32 x = m.r32s(pos), z = m.r32s(pos + 8), vx = m.r32s(vel), vz = m.r32s(vel + 8);
    m.w32(g::kScratch + 4, static_cast<u32>(y + vy));
    m.w32(g::kScratch + 0x14, static_cast<u32>(vy));
    m.w32(g::kScratch, static_cast<u32>(x + 2 * vx));
    m.w32(g::kScratch + 8, static_cast<u32>(z + 2 * vz));
    m.w32(g::kScratch + 0x10, static_cast<u32>(vx));
    m.w32(g::kScratch + 0x18, static_cast<u32>(vz));
    if (m.r32(m.r32(g::kWorld)) & 2u) puff_spawn_recorded(g::kScratch, g::kScratch + 0x10, 8);
    else puff_spawn(g::kScratch, g::kScratch + 0x10, 8);
}

void Game::particles_update(u32 masks, s32 groups) {
    cover(0x80084D04u);
    const u32 base = masks + 4 * static_cast<u32>(groups);
    for (s32 gi = 0; gi < groups; ++gi) {
        const u32 mask_at = masks + 4 * static_cast<u32>(gi);
        u32 bits = m.r32(mask_at);
        u32 p = base + 4 * 224 * static_cast<u32>(gi);
        for (u32 k = 0; bits; ++k, bits >>= 1, p += 28) {
            if (!(bits & 1u)) continue;
            const s16 age = static_cast<s16>(m.r16(p) + 1u);
            m.w16(p, static_cast<u32>(age));
            if (!(age < m.r16s(p + 2))) {
                m.w32(mask_at, m.r32(mask_at) & ~wrap::shl(1u, k));
                continue;
            }
            const s16 fl = m.r16s(p + 22);
            if (!(fl & 0xFF)) continue;
            s32 vx = m.r16s(p + 16) << 8, vy = m.r16s(p + 18) << 8, vz = m.r16s(p + 20) << 8;
            const s32 z = m.r32s(p + 12);
            m.w32(p + 4, static_cast<u32>(m.r32s(p + 4) + vx));
            const s32 y = m.r32s(p + 8);
            m.w32(p + 12, static_cast<u32>(z + vz));
            m.w32(p + 8, static_cast<u32>(y + vy));
            if (!(fl & 0x1F)) continue;
            if (fl & 7) {
                const u32 sh = static_cast<u32>(fl & 7);
                vx -= vx >> sh;
                vy -= vy >> sh;
                vz -= vz >> sh;
            }
            const s32 grav = fl & 0x18;
            if (grav == 8) vy += 2218;
            else if (grav == 16) vy += 6656;
            else if (grav) vy += 13312;
            m.w16(p + 16, static_cast<u32>(vx >> 8));
            m.w16(p + 18, static_cast<u32>(vy >> 8));
            m.w16(p + 20, static_cast<u32>(vz >> 8));
        }
    }
}

void Game::timed_effects_update(u32 pool) {
    cover(0x80089ED8u);
    u32 bits = m.r32(pool);
    u32 e = pool + 4;
    for (u32 i = 0; bits; e += 24, bits >>= 1, ++i) {
        if (!(bits & 1u)) continue;
        const s16 age = static_cast<s16>(m.r16(e) + 1u);
        m.w16(e, static_cast<u32>(age));
        if (age >= m.r16s(e + 2)) m.w32(pool, m.r32(pool) & ~wrap::shl(1u, i));
    }
}

void Game::callback_effects_update(u32 pool) {
    cover(0x80089D70u);
    Frame f(*this, 48);
    u32 bits = m.r32(pool);
    if (!bits) return;
    u32 e = pool + 4;
    for (u32 i = 0; bits; bits >>= 1, ++i, e += 20) {
        if (!(bits & 1u)) continue;
        const s16 age = static_cast<s16>(m.r16(e) + 1u);
        m.w16(e, static_cast<u32>(age));
        if (age < m.r16s(e + 2)) {
            const u32 fn = m.r32(e + 16);
            if (fn) call_effect(fn, age, e);
        } else {
            m.w32(pool, m.r32(pool) & ~wrap::shl(1u, i));
        }
    }
}

void Game::call_effect(u32 fn, s32 age, u32 effect) {
    if (call_timed_fx(fn, age, effect)) return;
    call_by_address(fn, "effect callback", static_cast<u32>(age), effect);
}

u32 Game::spark_take() {
    cover(0x80082FF0u);
    const u32 fx = m.r32(kFx);
    u32 e = fx + 0x3968;
    const u32 mask = m.r32(fx + 0x3964);
    if (static_cast<s32>(mask) < 0) return 0;
    const u32 both = mask & (mask >> 16);
    u32 bit = 1, v = mask | 1u;
    if (both & 1u) {
        do {
            bit <<= 1;
            e += 0x1C;
        } while (both & bit);
        v = mask | bit;
    }
    m.w32(fx + 0x3964, v | (bit << 16));
    return e;
}

void Game::spark_add(u32 from, u32 to, u32 size, u32 kind) {
    cover(0x80083054u);
    Frame f(*this, 40);
    const u32 e = spark_take();
    if (!e) return;
    m.w16(e, size);
    m.w16(e + 2, kind);
    for (u32 k = 0; k < 12; k += 4) m.w32(e + 4 + k, m.r32(from + k));
    for (u32 k = 0; k < 12; k += 4) m.w32(e + 16 + k, m.r32(to + k));
}

void Game::sparks_update(u32 mask_at) {
    cover(0x80091654u);
    u32 lo = m.r16(mask_at);
    if (!lo) return;
    u32 hi = m.r16(mask_at + 2);
    u32 bit = 1;
    for (u32 v = lo; v; v >>= 1, bit <<= 1) {
        if (!(v & 1u)) continue;
        if (hi & bit) hi &= ~bit;
        else lo &= ~bit;
    }
    m.w16(mask_at, lo);
    m.w16(mask_at + 2, hi);
}

void Game::record_ray(u32 from, u32 to, u32 size, u32 kind) {
    cover(0x800830DCu);
    Frame f(*this, 48);
    s32 dx = m.r32s(to) - m.r32s(from), dy = m.r32s(to + 4) - m.r32s(from + 4), dz = m.r32s(to + 8) - m.r32s(from + 8);
    u32 shift = 0;
    if (static_cast<u32>(dx + 0x800000) > 0x1000000u || dy > 0x800000 || dy < -0x800000 || dz > 0x800000 || dz < -0x800000) {
        dx >>= 4;
        dy >>= 4;
        dz >>= 4;
        shift = 4;
    }
    const u32 v = f.at(0x10);
    m.w32(v + 8, static_cast<u32>(dz));
    m.w32(v + 4, static_cast<u32>(dy));
    m.w32(v, static_cast<u32>(dx));
    const u32 r = record_motion(5, 1, from, v);
    const u32 w = m.r32(r + 16);
    m.w8(r + 2, kind);
    m.w32(r + 16, (w & 0xFF000000u) | shift | ((size & 0xFFFu) << 12));
}

void Game::gun_ray(u32 pos, u32 angles, u32 size, u32 view_mode) {
    cover(0x800831D0u);
    Frame f(*this, 96);
    const u32 a = f.at(0x10), b = f.at(0x20), hit = f.at(0x30);
    rot_matrix_yxz(angles, g::kScratch);
    load_scratch_rotation();
    gte.mtc2(0, 0xFFF80000u);
    gte.mtc2(1, 0x37);
    gte.op(0x486012);
    s32 px = m.r32s(pos), py = m.r32s(pos + 4), pz = m.r32s(pos + 8);
    const s32 ax = static_cast<s32>(gte.mfc2(25)), ay = static_cast<s32>(gte.mfc2(26)), az = static_cast<s32>(gte.mfc2(27));
    gte.mtc2(0, 0);
    gte.mtc2(1, 0x1AE0);
    gte.op(0x486012);
    m.w32(a, static_cast<u32>((ax << 12) + px));
    m.w32(a + 4, static_cast<u32>((ay << 12) + py));
    m.w32(a + 8, static_cast<u32>((az << 12) + pz));
    m.w32(b, static_cast<u32>((static_cast<s32>(gte.mfc2(25)) << 12) + px));
    m.w32(b + 4, static_cast<u32>((static_cast<s32>(gte.mfc2(26)) << 12) + py));
    m.w32(b + 8, static_cast<u32>((static_cast<s32>(gte.mfc2(27)) << 12) + pz));
    if (view_mode == 1) {
        gte.mtc2(0, 0x300000u);
        gte.mtc2(1, 0x82);
        gte.op(0x486012);
        px += static_cast<s32>(gte.mfc2(25)) << 12;
        py += static_cast<s32>(gte.mfc2(26)) << 12;
        pz += static_cast<s32>(gte.mfc2(27)) << 12;
    }
    u32 end = hit;
    if (0x7FFFFFFE < terrain_raycast(a, b, hit, g::kScratch)) end = b;
    if (m.r32(m.r32(g::kWorld)) & 2u) record_ray(a, end, size, view_mode);
    if (view_mode == 1) {
        m.w32(a, static_cast<u32>(px));
        m.w32(a + 4, static_cast<u32>(py));
        m.w32(a + 8, static_cast<u32>(pz));
    }
    spark_add(a, end, size, view_mode);
}

void Game::gun_update(u32 st) {
    cover(0x80090148u);
    Frame f(*this, 24);
    const s32 n = m.r32s(st);
    if (n <= 0) return;
    const u32 w = m.r32(st + 4);
    m.w32(st, n < static_cast<s32>(w) >> 16 ? static_cast<u32>(n + 1) : 0u);
    if ((m.r32(m.r32(g::kWorld)) & 2u) && (w & 0xFFFFu) == 5) {
        s32 size = (n - 1) << 6;
        if (!(m.r32(m.r32(g::kScene) + 296) & 0x800u)) {
            size += 256;
            if (size >= 1025) size = 1024;
        }
        const u32 p = slot(0);
        gun_ray(p + 8, p + 24, static_cast<u32>(size), m.r32(m.r32(m.r32(g::kWorld) + 40) + 80) == 1 ? 1u : 0u);
    }
}

u32 Game::flash_tile(u32 ot, u32 packet, u32 rect, s32 level) {
    cover(0x80093B2Cu);
    if (level == 0) return packet;
    u32 mode = 1;
    if (level < 0) {
        level = -level;
        mode = 2;
    }
    gte.ctc2(21, 0);
    gte.ctc2(22, 0);
    gte.ctc2(23, 0);
    gte.mtc2(6, 0x62FFFFFFu);
    gte.mtc2(8, static_cast<u32>(0x1000 - level));
    m.w32(packet, m.r32(ot) | 0x04000000u);
    m.w32(ot, (packet << 8) >> 8);
    gte.op(0x780010);
    m.w32(packet + 4, 0xE1000200u | ((mode & 3u) << 5));
    m.w32(packet + 12, m.r32(rect));
    m.w32(packet + 16, m.r32(rect + 4));
    m.w32(packet + 8, gte.mfc2(22));
    return packet + 0x14;
}

u32 Game::noise_sprites(u32 packet, u32 clut_word, u32 xy, u32 wh) {
    cover(0x80084318u);
    Frame f(*this, 64);
    gte.ctc2(21, 0x30);
    gte.ctc2(22, 0x30);
    gte.ctc2(23, 0x30);
    const s32 x0 = static_cast<s16>(xy);
    m.w32(f.at(0x10), static_cast<u32>(x0));
    gte.op(0x780010);
    s32 y = static_cast<s32>(xy) >> 16;
    const s32 w0 = static_cast<s16>(wh);
    s32 h = static_cast<s32>(wh) >> 16;
    m.w32(f.at(0x14), static_cast<u32>(w0));
    const u32 color = gte.mfc2(22);
    for (; h > 0; h -= 0x30, y += 0x30) {
        s32 w = m.r32s(f.at(0x14));
        s32 x = m.r32s(f.at(0x10));
        for (; w > 0; w -= 0x20, x += 0x20) {
            m.w32(packet, ((packet + 0x14) & 0xFFFFFFu) | 0x04000000u);
            m.w32(packet + 4, color);
            m.w16(packet + 8, static_cast<u32>(x));
            m.w16(packet + 10, static_cast<u32>(y));
            u32 uv = static_cast<u32>(rand());
            uv &= clut_word == 0x4696 ? 0x7C7Eu : 0x7E7Fu;
            m.w16(packet + 12, uv);
            m.w16(packet + 14, clut_word);
            m.w16(packet + 16, static_cast<u32>(w < 0x20 ? w : 0x20));
            m.w16(packet + 18, static_cast<u32>(h < 0x30 ? h : 0x30));
            packet += 0x14;
        }
    }
    return packet;
}

u32 Game::noise_close(u32 ot, u32 packet, u32 end) {
    m.w32(end + 4, 0xE1000220u);
    m.w32(end, 0x03000000u);
    m.w32(end + 8, 0xE2000000u);
    m.w32(end + 12, 0);
    m.w32(end, (m.r32(end) & 0xFF000000u) | (m.r32(ot) & 0x00FFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (packet & 0x00FFFFFFu));
    return end + 0x10;
}

u32 Game::noise_screen(u32 ot, u32 packet, s32 level, u32 rgb) {
    if (level == 0) return packet;
    u32 mode = 1;
    if (level < 0) {
        mode = 2;
        level = -level;
    }
    m.w32(packet, (((packet + 0x10) << 8) >> 8) | 0x03000000u);
    m.w32(packet + 4, (mode << 5) | 0xE1000218u);
    m.w32(packet + 8, 0xE20C4318u);
    m.w32(packet + 12, 0);
    gte.mtc2(6, rgb);
    gte.mtc2(8, static_cast<u32>(0x1000 - level));
    return noise_close(ot, packet, noise_sprites(packet + 0x10, 0x46D6, 0xFF88FF60u, 0xF00140u));
}

u32 Game::noise_overlay(u32 ot, u32 packet, s32 level) {
    cover(0x80084474u);
    Frame f(*this, 32);
    return noise_screen(ot, packet, level, 0x66838383u);
}

u32 Game::noise_overlay_green(u32 ot, u32 packet, s32 level) {
    cover(0x8008488Cu);
    Frame f(*this, 32);
    return noise_screen(ot, packet, level, 0x66008300u);
}

u32 Game::noise_rect(u32 ot, u32 packet, u32 rect, u32 level) {
    cover(0x800846CCu);
    Frame f(*this, 32);
    if (m.r16s(rect + 4) <= 0 || m.r16s(rect + 6) <= 0) return packet;
    m.w32(packet, (((packet + 0x10) << 8) >> 8) | 0x03000000u);
    m.w32(packet + 4, 0xE1000218u);
    m.w32(packet + 8, 0xE20C6318u);
    m.w32(packet + 12, 0);
    gte.mtc2(6, 0x64838383u);
    gte.mtc2(8, level);
    return noise_close(ot, packet, noise_sprites(packet + 0x10, 0x4696, m.r32(rect), m.r32(rect + 4)));
}

u32 Game::ot_skip_empty(u32 ot, s32 n) {
    cover(0x80084978u);
    u32 expect = ot & 0xFFFFFFu, run = 0;
    s32 k = 1;
    for (u32 at = ot + 4; k < n; ++k, at += 4, expect += 4) {
        if (m.r32(at) == expect) {
            run += 4;
            continue;
        }
        if (run >= 5) m.w32(at - 4, expect - run);
        run = 0;
    }
    const u32 end = static_cast<u32>(k) << 2;
    if (!run) return end;
    m.w32(ot + end - 4, expect - run);
    return ot + end;
}

void Game::screen_flash_update(u32 st) {
    cover(0x800928F4u);
    Frame f(*this, 32);
    if (!m.r32(st)) return;
    const s32 t = m.r16s(st + 6);
    if (!(t < 0x31)) {
        m.w32(st, 0);
        return;
    }
    m.w16(st + 6, static_cast<u32>(t + 1));
    if (static_cast<u32>(t - 4) < 0xD) {
        const u32 size = t < 8 ? static_cast<u32>(8 - t) << 8 : static_cast<u32>(t - 8) << 6;
        spark_add(st + 0x20, st + 0x14, size & 0xFFFFu, 2);
    }
    if (static_cast<u32>(t - 4) < 5) {
        const s32 level = static_cast<s32>(wrap::sar(0x400, static_cast<u32>(t - 4))) + 0x100;
        m.w16(g::kScratch, static_cast<u32>(-160));
        m.w16(g::kScratch + 2, static_cast<u32>(-120));
        m.w16(g::kScratch + 4, 0x140);
        m.w16(g::kScratch + 6, 0xF0);
        const u32 w = m.r32(g::kWorld);
        const u32 p = flash_tile(m.r32(w + 16) + 8, m.r32(w + 20), g::kScratch, level);
        m.w32(m.r32(g::kWorld) + 20, p);
    }
    if (t == 9) {
        const u32 e = pool_take(m.r32(kFx) + 0x33C0, 1, 6);
        if (e) {
            m.w16(e, 0);
            m.w16(e + 2, static_cast<u32>((rand() & 7) + 0x3C));
            m.w32(e + 4, m.r32(st + 8));
            m.w32(e + 8, m.r32(st + 12));
            const u32 z = m.r32(st + 16);
            m.w16(e + 0x10, 0x400);
            m.w16(e + 0x12, 0);
            m.w16(e + 0x14, 1);
            m.w32(e + 12, z);
            m.w16(e + 0x16, static_cast<u32>((rand() & 0xFF) + 0x7D0));
        }
    }
    if (t < 16) return;
    const s32 k = (0x30 - t) << 5;
    const s32 r = rand();
    const u32 w = m.r32(g::kWorld);
    const u32 p = noise_overlay(m.r32(w + 16) + 8, m.r32(w + 20), k + (r & 0x1FF));
    m.w32(m.r32(g::kWorld) + 20, p);
}

void Game::sim_step8() {
    cover(0x800850D8u);
    Frame frame(*this, 24);
    const u32 world = m.r32(g::kWorld);
    if (m.r32(world) & 2u) {
        const u32 p = slot(0);
        water_wake(p + 8, p + 56, -m.r32s(m.r32(kSea) + 4));
        if (m.r16s(slot(0) + 78) > 0)
            player_exhaust(m.r16s(m.r32(g::kScene) + 908), m.r16s(m.r32(g::kScene) + 910));
    }
    if (m.r16(m.r32(g::kScene) + 908) == 42 && m.r16s(slot(0) + 78) > 0) sea_spray(slot(0) + 8, slot(0) + 56);
    callback_effects_update(m.r32(kFx) + 14020);
    particles_update(m.r32(kFx) + 1536, 3);
    particles_update(m.r32(kFx) + 4248, 10);
    timed_effects_update(m.r32(kFx) + 13248);
    gun_update(m.r32(kFx) + 14664);
    sparks_update(m.r32(kFx) + 14692);
    screen_flash_update(m.r32(kFx) + 15144);
}
}

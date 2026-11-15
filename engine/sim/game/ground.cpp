#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kLockVoice = 0x800BCFF0u;
constexpr u32 kLockSource = 0x800BCFF1u;
constexpr u32 kLockDist = 0x800BCFF4u;
constexpr u32 kLockBuf = 0x800C77E0u;
constexpr u32 kZeroVel = 0x800B20A8u;
constexpr u32 kZeroVel2 = 0x800B20B8u;
constexpr u32 kPitchTable = 0x800B0500u;
constexpr u32 kFlagsBD004 = 0x800BD004u;
constexpr u32 kAlertFlag = 0x800C6ECCu;

u32 unit(Memory& m, u32 i) { return m.r32(m.r32(g::kGroundSlots) + 4 * i); }
u32 record(Memory& m, u32 i) { return m.r32(g::kGroundRecords) + 40 * i; }
u32 sflag(u32 k) { return g::kSlotFlags + 4 * k; }
s32 absv(s32 v) { return v < 0 ? -v : v; }
}

u32 Game::voice_release(u32 v) {
    cover(0x8002DDA8u);
    if (v >= 24) return 255;
    m.w8(g::kVoices + 20 * v + 1, 2);
    return v;
}

u32 Game::voice_level(u32 v, s32 vol, u32 b) {
    cover(0x8002DF40u);
    if (v >= 24) return 255;
    const s32 x = vol << 7;
    const s32 q = static_cast<s32>((static_cast<s64>(x) * 0x51EB851F) >> 32 >> 5) - (x >> 31);
    const u32 at = g::kVoices + 20 * v;
    m.w16(at + 0x12, b);
    m.w8(at + 0x10, static_cast<u32>(q));
    return static_cast<u32>(q);
}

s32 Game::pitch_add(s32 a, s32 b) {
    cover(0x8002E64Cu);
    const s32 v = b + (((a & 0xFF00) >> 1) | (a & 0x7F));
    return static_cast<s16>(((v & 0x7F80) << 1) | (v & 0x7F));
}

u32 Game::sound_height(u32 v, u32 buf) {
    cover(0x8002EC48u);
    Frame f(*this, 32);
    if (v == 255) return 255;
    const u32 vc = g::kVoices + 20 * v;
    m.w8(vc + 1, m.r8(vc + 1) | 0x10u);
    s32 k = static_cast<s32>(wrap::div(static_cast<s32>(static_cast<u32>(m.r32s(buf + 4) + 0x470) << 9), 2272).lo);
    if (k < 0) k = 0;
    if (k >= 512) k = 511;
    m.w16(vc + 12, static_cast<u32>(pitch_add(m.r16s(vc + 14), m.r16s(kPitchTable + 2u * static_cast<u32>(k)))));
    return v;
}

void Game::vec_to_angles(u32 v, u32 out) {
    cover(0x80031BB4u);
    Frame f(*this, 40);
    const s32 x = m.r32s(v), z = m.r32s(v + 8), y = m.r32s(v + 4);
    const s32 flat_m = absv(x) + absv(z);
    s32 flat;
    if (flat_m > 720895) {
        flat = 0x100000;
    } else {
        s32 xx = x, zz = z, sh = 0;
        if (flat_m > 45056) {
            xx = x >> 4;
            zz = z >> 4;
            sh = 4;
        }
        flat = static_cast<s32>(wrap::shl(static_cast<u32>(length3(xx, 0, zz)), static_cast<u32>(sh)));
    }
    s32 pitch = ratan2(-y, flat);
    s32 yaw = ratan2(x, z);
    if (pitch >= 4097) {
        pitch = 0x2000 - pitch;
        yaw += 0x2000;
    }
    if (pitch < -4096) {
        pitch = -8192 - pitch;
        yaw += 0x2000;
    }
    if (yaw >= 0x2000) yaw -= 0x4000;
    if (yaw < -8192) yaw += 0x4000;
    m.w16(out, static_cast<u32>(pitch));
    m.w16(out + 2, static_cast<u32>(yaw));
    m.w16(out + 4, 0);
}

s32 Game::dir_normalize(u32 a, u32 b, u32 out) {
    cover(0x80052D70u);
    Frame f(*this, 64);
    const s32 dx = m.r32s(b) - m.r32s(a);
    s32 ax = dx < 0 ? m.r32s(a) - m.r32s(b) : dx;
    const s32 dy = m.r32s(b + 4) - m.r32s(a + 4);
    s32 ay = absv(dy);
    const s32 dz = m.r32s(b + 8) - m.r32s(a + 8);
    s32 az = absv(dz);
    m.w32(f.at(0x10), static_cast<u32>(dx));
    m.w32(f.at(0x14), static_cast<u32>(dy));
    m.w32(f.at(0x18), static_cast<u32>(dz));
    if (ax < 4096 && ay < 4096 && az < 4096) {
        m.w32(out, 0);
        m.w32(out + 4, 0);
        m.w32(out + 8, 0);
        return 0;
    }
    s32 sh = 0;
    while (ax >= 0x4000 || ay >= 0x4000 || az >= 0x4000) {
        ++sh;
        ax >>= 1;
        az >>= 1;
        ay >>= 1;
    }
    const s32 len = square_root0(ax * ax + ay * ay + az * az);
    m.w32(out, static_cast<u32>(wrap::div((dx >> sh) << 14, len).lo));
    m.w32(out + 4, static_cast<u32>(wrap::div((dy >> sh) << 14, len).lo));
    m.w32(out + 8, static_cast<u32>(wrap::div((dz >> sh) << 14, len).lo));
    return len << sh;
}

u32 Game::bullet_request(u32 owner, u32 mount, u32 vel, u32 gun) {
    cover(0x8006B034u);
    const u32 w = m.r32(g::kWeapons);
    if (m.r32s(w + 0x154C) >= 8) return 0;
    u32 masks = w + 0x33D0;
    u32 mask = m.r32(masks);
    u32 base = 0;
    if (mask == 0xFFFFFFFFu) {
        for (u32 k = 32;; k += 32) {
            masks += 4;
            if (k >= 128) return 0;
            mask = m.r32(masks);
            if (mask != 0xFFFFFFFFu) {
                base = k;
                break;
            }
        }
    }
    u32 bit = 0;
    if (mask & 1u) {
        bit = 1;
        while (mask & wrap::shl(1u, bit)) ++bit;
    }
    m.w32(masks, m.r32(masks) | wrap::shl(1u, bit));
    const u32 n = m.r32(w + 0x154C);
    m.w32(w + 0x154C, n + 1);
    const u32 req = w + 16 * n + 0x1550;
    m.w16(req, bit + base);
    m.w16(req + 2, gun);
    m.w32(req + 4, owner);
    m.w32(req + 8, mount);
    m.w32(req + 12, vel);
    return 1;
}

u32 Game::launch_rocket_19(u32 owner, u32 pos, u32 vel, u32 target) {
    cover(0x800689E8u);
    Frame f(*this, 24);
    const u32 o = weapon_alloc(owner, pos, vel, static_cast<s32>(target));
    if (!o) return 0;
    m.w16(o + 26, 0x2000);
    m.w8(o + 52, 4);
    m.w8(o + 5, 2);
    m.w16(o + 24, 300);
    m.w32(o + 156, 400);
    const u32 fl = m.r32(o);
    m.w8(o + 6, 0);
    m.w8(o + 7, 0);
    m.w32(o + 160, 192);
    m.w32(o, fl | 0x10u);
    return o;
}

u32 Game::launch_rocket_18(u32 owner, u32 pos, u32 vel, u32 target) {
    cover(0x80068BA8u);
    Frame f(*this, 24);
    const u32 o = weapon_alloc(owner, pos, vel, static_cast<s32>(target));
    if (!o) return 0;
    m.w16(o + 26, 4096);
    m.w8(o + 5, 3);
    m.w32(o + 132, 5120);
    m.w32(o + 136, 5120);
    m.w8(o + 6, 0);
    m.w8(o + 7, 0);
    m.w32(o + 128, 1228800);
    m.w16(o + 24, 90);
    return o;
}

u32 Game::launch_sam_13(u32 owner, u32 pos, u32 vel, u32 target) {
    cover(0x80068C80u);
    Frame f(*this, 24);
    const u32 o = weapon_alloc(owner, pos, vel, static_cast<s32>(target));
    if (!o) return 0;
    m.w16(o + 26, 128);
    m.w8(o + 52, 3);
    m.w8(o + 6, 2);
    m.w8(o + 5, 1);
    m.w16(o + 24, 150);
    m.w16(o + 124, 18);
    m.w16(o + 126, 7);
    const u32 b53 = m.r8(o + 53);
    m.w32(o + 128, 253952);
    m.w8(o + 7, 0);
    m.w32(o + 132, 23040);
    m.w32(o + 136, 23040);
    m.w8(o + 53, b53 | 0x18u);
    return o;
}

u32 Game::launch_sam_12(u32 owner, u32 pos, u32 vel, u32 target) {
    cover(0x80068C0Cu);
    Frame f(*this, 24);
    const u32 o = weapon_alloc(owner, pos, vel, static_cast<s32>(target));
    if (!o) return 0;
    m.w16(o + 24, 83);
    m.w16(o + 124, 20);
    m.w16(o + 126, 1);
    m.w32(o + 128, 655360);
    const u32 b53 = m.r8(o + 53);
    m.w16(o + 48, 0);
    m.w8(o + 6, 0);
    m.w8(o + 7, 0);
    m.w8(o + 5, 0);
    m.w32(o + 132, 23040);
    m.w32(o + 136, 23040);
    m.w8(o + 53, b53 | 0x18u);
    return o;
}

u32 Game::ground_pick_target(u32 a) {
    cover(0x80046124u);
    Frame f(*this, 56);
    const u32 e = slot(a);
    if (a != (m.r32(m.r32(g::kWorld) + 12) & 0x3Fu)) return 255;
    s32 best_d = 0x100000;
    const u32 team = m.r32(e) & 0x1C0u;
    u32 best = 255;
    if (m.r32(e) & 0x80u) {
        bool check = false;
        if (m.r32(m.r32(g::kScene) + 916)) {
            if (m.r8(g::kPairs + 2) == 255) check = true;
            else check = m.r8(g::kPairs + 3) == 255;
        } else {
            check = m.r8(g::kPairs + 2) == 255;
        }
        if (check && m.r8(g::kPairs + 1) != a) best = 0;
    }
    if (best == 255) {
        const u32 pa = g::kPairs + 4 * a;
        for (u32 k = 2; k < 64; ++k) {
            if (a == k) continue;
            const u32 t = slot(k);
            if (!(m.r32(t) & 0x4000u)) continue;
            const u32 sf = m.r32(sflag(k));
            if (!(sf & 1u)) continue;
            const u32 tt = m.r32(t) & 0x1C0u;
            const u32 pk = g::kPairs + 4 * k;
            if (sf & 0x800000u) continue;
            if (tt == team || tt == 256) continue;
            if (!(m.r8(pk + 2) == 255 || m.r8(pk + 3) == 255)) continue;
            if (m.r8(pa + 2) == k || m.r8(pa + 3) == k) continue;
            if (team == 64 && (sf & 0x100u) && m.r16(m.r32(g::kScene) + 908) != 30) continue;
            const s32 dx = absv((m.r32s(e + 8) - m.r32s(t + 8)) >> 12);
            if (!(best_d >= dx)) continue;
            const s32 dz = absv((m.r32s(e + 16) - m.r32s(t + 16)) >> 12);
            if (!(best_d >= dz)) continue;
            const s32 d = distance(e + 8, t + 8);
            if (d < best_d) {
                best = k;
                best_d = d;
            }
        }
    }
    return pair_add(a, best);
}

static bool aim_within(Memory& m, u32 e, u32 wd) {
    s32 dp = m.r16s(e + 124) - m.r16s(e + 24);
    if (dp >= 4097) dp = 0x2000 - dp;
    else if (dp < -4096) dp = -8192 - dp;
    if (dp < m.r16s(wd + 8) || m.r16s(wd + 10) < dp) return false;
    s32 dy = (m.r16s(e + 126) - m.r16s(e + 26)) & 0x3FFF;
    if (dy < m.r16s(wd + 12)) dy += 0x4000;
    return !(m.r16s(wd + 14) < dy);
}

s32 Game::ground_gun_fire(u32 i, s32 d) {
    cover(0x80053214u);
    Frame f(*this, 56);
    const u32 e = unit(m, i);
    const u32 wd = m.r32(record(m, i) + 16);
    const u32 t = slot(m.r8(e + 91));
    for (u32 k = 0; k < 16; k += 4) m.w32(e + 108 + k, m.r32(e + 8 + k));
    for (u32 k = 0; k < 16; k += 4) m.w32(e + 132 + k, m.r32(t + 8 + k));
    const s32 n = static_cast<s32>(wrap::div(d, 144).lo);
    m.w32(e + 132, m.r32(e + 132) + static_cast<u32>(m.r32s(t + 56) * n));
    m.w32(e + 136, m.r32(e + 136) + static_cast<u32>(m.r32s(t + 60) * n));
    const s32 lz = m.r32s(t + 64) * n;
    m.w32(e + 136, m.r32(e + 136) - static_cast<u32>((n * 669 * n) >> 1));
    m.w32(e + 140, m.r32(e + 140) + static_cast<u32>(lz));
    const u32 v = f.at(0x10);
    m.w32(v, static_cast<u32>((m.r32s(e + 132) - m.r32s(e + 8)) >> 12));
    m.w32(v + 4, static_cast<u32>((m.r32s(e + 136) - m.r32s(e + 12)) >> 12));
    m.w32(v + 8, static_cast<u32>((m.r32s(e + 140) - m.r32s(e + 16)) >> 12));
    vec_to_angles(v, e + 124);
    s32 spread;
    if (m.r32(m.r32(g::kScene) + 916) || !(m.r32(e) & 0x80u)) {
        spread = 0;
        if (m.r8(wd + 1)) spread = m.r8(wd + 1) == 1 ? 200 : 600;
    } else {
        spread = 600;
    }
    if (2 * spread) {
        const s32 r1 = rand();
        m.w16(e + 124, m.r16(e + 124) + static_cast<u32>(r1 % (2 * spread) - spread));
        const s32 r2 = rand();
        m.w16(e + 126, m.r16(e + 126) + static_cast<u32>(r2 % (2 * spread) - spread));
    }
    if (!aim_within(m, e, wd)) return 1;
    if (m.r8(e + 90)) {
        if (m.r32s(e + 100) < m.r16s(wd + 20)) {
            m.w32(e + 100, m.r32(e + 100) + 1);
            return 0;
        }
        m.w8(e + 90, 0);
    } else {
        if (m.r32s(e + 100) % m.r16s(wd + 16) == 0)
            bullet_request(m.r32(e), e + 108, kZeroVel, static_cast<u32>(m.r8(wd) - 1) & 0xFFFFu);
        if (m.r32s(e + 100) < m.r16s(wd + 18)) {
            m.w32(e + 100, m.r32(e + 100) + 1);
            return 0;
        }
        m.w8(e + 90, 1);
    }
    m.w32(e + 100, 1);
    return 0;
}

s32 Game::ground_missile_fire(u32 i) {
    cover(0x800535D4u);
    Frame f(*this, 64);
    const u32 e = unit(m, i);
    const u32 ti = m.r8(e + 91);
    const u32 wd = m.r32(record(m, i) + 16);
    const u32 t = slot(ti);
    const u32 w = m.r32(g::kWeapons);
    if (ti >= 32) {
        if (m.r32(w + 4) & wrap::shl(1u, ti - 32)) return 1;
    } else if (m.r32(w) & wrap::shl(1u, ti)) {
        return 1;
    }
    const u32 firing = m.r8(e + 90);
    const u32 timer = m.r32(e + 100);
    m.w8(e + 90, 0);
    m.w32(e + 100, 0);
    for (u32 k = 0; k < 16; k += 4) m.w32(e + 108 + k, m.r32(e + 8 + k));
    for (u32 k = 0; k < 16; k += 4) m.w32(e + 132 + k, m.r32(t + 8 + k));
    const u32 v = f.at(0x18);
    m.w32(v, static_cast<u32>((m.r32s(e + 132) - m.r32s(e + 8)) >> 12));
    m.w32(v + 4, static_cast<u32>((m.r32s(e + 136) - m.r32s(e + 12)) >> 12));
    m.w32(v + 8, static_cast<u32>((m.r32s(e + 140) - m.r32s(e + 16)) >> 12));
    vec_to_angles(v, e + 124);
    if (!aim_within(m, e, wd)) return 1;
    m.w8(e + 90, firing);
    m.w32(e + 100, timer);
    u32 flags = 0;
    if (m.r8(wd) == 17) {
        flags = 256;
        if (m.r32(m.r32(g::kScene) + 916) || !(m.r32(e) & 0x80u)) {
            if (m.r8(wd + 1) == 0) flags = 0x20100u;
            else if (m.r8(wd + 1) == 1) flags = 0x10100u;
        }
    }
    if (m.r8(e + 90)) {
        if (m.r32s(e + 100) < m.r16s(wd + 20)) {
            m.w32(e + 100, m.r32(e + 100) + 1);
            return 0;
        }
        m.w8(e + 90, 0);
    } else {
        s32 iv = m.r16s(wd + 18);
        if (m.r32(e) & 0x80u) {
            const s32 k = m.r32s(m.r32(g::kScene) + 916);
            if (!k) iv *= 2;
            if (k >= 2) iv >>= 1;
        }
        if (m.r32s(e + 100) == iv >> 1) m.w32(kAlertFlag, m.r32(kAlertFlag) | 1u);
        if (m.r32s(e + 100) < iv) {
            m.w32(e + 100, m.r32(e + 100) + 1);
            return 0;
        }
        const u32 owner = m.r32(e), tf = m.r32(t);
        switch (m.r8(wd)) {
        case 0x11: case 0x14: spawn_weapon(owner, e + 108, kZeroVel2, static_cast<s32>(tf), flags); break;
        case 0x12: launch_sam_12(owner, e + 108, kZeroVel2, tf); break;
        case 0x13: launch_sam_13(owner, e + 108, kZeroVel2, tf); break;
        case 0x18: launch_rocket_18(owner, e + 108, kZeroVel2, tf); break;
        case 0x19: launch_rocket_19(owner, e + 108, kZeroVel2, tf); break;
        default: break;
        }
        m.w8(e + 90, 1);
    }
    m.w32(e + 100, 1);
    return 0;
}

s32 Game::ground_weapons(u32 i) {
    cover(0x80052FA8u);
    Frame f(*this, 40);
    const u32 wd = m.r32(record(m, i) + 16);
    const u32 e = unit(m, i);
    if (!m.r8(wd)) return 0;
    if (m.r16(m.r32(g::kScene) + 908) == 12 && m.r16(e + 72) == 54) {
        const u32 x = m.r32(e + 8);
        if (x + 452984832u <= 0x2000000u || x + 335544320u <= 0x4000000u || x + 117440512u <= 0x2000000u) return 0;
        if (x - 0x800000u <= 0x3000000u) return 0;
    }
    if (m.r8(e + 91) == 255) {
        const u32 t = ground_pick_target(i + 32);
        m.w8(e + 91, t);
        if ((t & 0xFFu) == 255) {
            if (m.r32(e) & 0x80u) m.w8(e + 91, 0);
            if (m.r8(e + 91) == 255) return 0;
        }
    }
    u32 d;
    if (m.r8(e + 91)) {
        const u32 ti = m.r8(e + 91);
        const u32 t = slot(ti);
        if (!(m.r32(sflag(ti)) & 1u)) m.w8(e + 91, 255);
        d = static_cast<u32>(absv((m.r32s(e + 8) - m.r32s(t + 8)) >> 12) + absv((m.r32s(e + 12) - m.r32s(t + 12)) >> 12) +
                             absv((m.r32s(e + 16) - m.r32s(t + 16)) >> 12));
    } else {
        d = m.r32(m.r32(g::kRelation + 4 * (i + 32)) + 4);
    }
    if (m.r32(wd + 4) < d) {
        m.w8(e + 91, pair_drop_target(i + 32));
        return 0;
    }
    const s32 r = (m.r8(wd) & 0x10u) ? ground_missile_fire(i) : ground_gun_fire(i, static_cast<s32>(d));
    if (r) {
        m.w8(e + 91, pair_drop_target(i + 32));
        return r;
    }
    return 0;
}

void Game::ground_route(u32 i) {
    cover(0x800522B0u);
    Frame f(*this, 176);
    const u32 e = unit(m, i);
    const u32 rd = m.r32(record(m, i) + 20);
    if (m.r32(sflag(32 + i)) & 0x400000u) return;
    const u32 ms = m.r32(g::kMission);
    auto set_vel = [&](u32 dir) {
        const s32 spd = m.r32s(rd + 8);
        m.w32(e + 56, static_cast<u32>((spd * m.r32s(dir)) >> 14));
        m.w32(e + 60, static_cast<u32>((m.r32s(rd + 8) * m.r32s(dir + 4)) >> 14));
        m.w32(e + 64, static_cast<u32>((m.r32s(rd + 8) * m.r32s(dir + 8)) >> 14));
    };
    auto scene908 = [&] { return m.r16(m.r32(g::kScene) + 908); };
    switch (m.r8(rd)) {
    case 0:
        m.w32(e + 56, 0);
        m.w32(e + 60, 0);
        m.w32(e + 64, 0);
        return;
    case 1: {
        const u32 a = m.r16(rd + 2) & 0x3FFFu;
        const s32 x = m.r32s(rd + 8) * m.r16s(g::kSinCos + 2 * a);
        m.w32(e + 60, 0);
        m.w32(e + 56, static_cast<u32>(x >> 14));
        const s32 z = m.r16s(g::kSinCos + 0x2000 + 2 * a) * m.r32s(rd + 8);
        m.w32(e + 64, static_cast<u32>(z >> 14));
        return;
    }
    case 2:
        m.w32(e + 56, 0);
        m.w32(e + 64, 0);
        m.w32(e + 60, m.r32(rd + 4));
        return;
    case 3: {
        const u32 st = m.r8(e + 88);
        if (st && st != 3) m.w32(e + 60, m.r32(e + 60) + 256);
        else m.w32(e + 60, m.r32(e + 60) - 256);
        const u32 c = m.r32(e + 92) + 1;
        m.w32(e + 92, c);
        if (c == 64) {
            const u32 s2 = (m.r8(e + 88) + 1) & 0xFFu;
            m.w8(e + 88, s2);
            if (static_cast<s8>(s2) == 4) m.w8(e + 88, 0);
            m.w32(e + 92, 0);
        }
        m.w32(e + 56, 0);
        m.w32(e + 64, 0);
        return;
    }
    case 4: {
        const u32 a = f.at(0x10), b = f.at(0x20), dir = f.at(0x30);
        const s32 st = m.r8s(e + 88);
        auto own = [&] { for (u32 k = 0; k < 16; k += 4) m.w32(a + k, m.r32(e + 8 + k)); };
        auto pt = [&](u32 to, u32 from) { for (u32 k = 0; k < 12; k += 4) m.w32(to + k, m.r32(from + k)); };
        if (st == 0) {
            own();
            pt(b, rd + 12);
        } else if (st == 1) {
            own();
            pt(b, rd + 24);
        } else {
            if (st == 2 || scene908() != 43) own();
            else pt(a, rd + 24);
            pt(b, rd + 36);
        }
        const s32 d = dir_normalize(a, b, dir);
        if (m.r8s(e + 88) < 3 && d < m.r32s(rd + 8)) m.w8(e + 88, m.r8(e + 88) + 1u);
        if (scene908() == 43 && m.r8(e + 88) == 3 && d == 0) {
            if ((m.r32(e) & 0x40u) && !(m.r8(kFlagsBD004) & 0x40u)) {
                msn_vm_exec(m.r32(m.r32(g::kMissionData) + 328));
                m.w32(kFlagsBD004, m.r32(kFlagsBD004) | 0x40u);
            } else if ((m.r32(e) & 0x80u) && !(m.r8(kFlagsBD004) & 0x80u)) {
                msn_vm_exec(m.r32(m.r32(g::kMissionData) + 332));
                m.w32(kFlagsBD004, m.r32(kFlagsBD004) | 0x80u);
            }
        }
        set_vel(dir);
        return;
    }
    case 5: {
        const u32 b = f.at(0x40), dir = f.at(0x50);
        const u32 st = m.r8(e + 88);
        if (st == 0) for (u32 k = 0; k < 12; k += 4) m.w32(b + k, m.r32(rd + 12 + k));
        else if (st == 1) for (u32 k = 0; k < 12; k += 4) m.w32(b + k, m.r32(rd + 24 + k));
        const s32 d = dir_normalize(e + 8, b, dir);
        if (d < m.r32s(rd + 8)) m.w8(e + 88, m.r8(e + 88) ? 0u : 1u);
        set_vel(dir);
        return;
    }
    case 6: {
        const s32 st = m.r8s(e + 88);
        m.w8(e + 5, m.r8(e + 5) | 4u);
        if (st == 0) {
            m.w32(e + 56, 0);
            m.w32(e + 64, 0);
            m.w32(e + 60, m.r32(rd + 4));
            if (m.r32s(e + 12) - m.r32s(rd + 16) >= -1048576) m.w16(e + 6, 12288);
            const s32 top = m.r32s(rd + 16);
            if (m.r32s(e + 12) >= top) {
                m.w16(e + 6, 4096);
                m.w32(e + 12, static_cast<u32>(top));
                m.w8(e + 88, 1);
                m.w32(e + 96, 0);
            }
            return;
        }
        if (st == 1) {
            const s32 c = m.r32s(e + 96);
            if (c < 32) {
                m.w16(e + 6, 0x2000);
            } else if (c < 64) {
                m.w16(e + 6, 12288);
            } else {
                m.w8(e + 88, 2);
                m.w32(e + 104, 512);
            }
            m.w32(e + 96, m.r32(e + 96) + 1);
            return;
        }
        if (st != 2) return;
        if (m.r32s(rd + 16) < m.r32s(e + 12)) m.w32(e + 12, m.r32(rd + 16));
        const u32 b = f.at(0x60), dir = f.at(0x70);
        for (u32 k = 0; k < 12; k += 4) m.w32(b + k, m.r32(rd + 12 + k));
        const s32 d = dir_normalize(e + 8, b, dir);
        if (!(m.r8(ms) & 0x40u) && !d) {
            msn_vm_exec(m.r32(m.r32(g::kMissionData) + 328));
            m.w32(ms, m.r32(ms) | 0x40u);
        }
        m.w32(e + 56, static_cast<u32>((m.r32s(rd + 8) * m.r32s(dir)) >> 14));
        m.w32(e + 60, static_cast<u32>((m.r32s(rd + 8) * m.r32s(dir + 4)) >> 14));
        const s32 roll = m.r16s(e + 6);
        m.w32(e + 64, static_cast<u32>((m.r32s(rd + 8) * m.r32s(dir + 8)) >> 14));
        if (roll == 0x4000) m.w32(e + 104, static_cast<u32>(-512));
        else if (roll == 0x2000) m.w32(e + 104, 512);
        m.w16(e + 6, m.r16(e + 6) + m.r16(e + 104));
        return;
    }
    case 7: {
        const u32 p = slot(0);
        const s32 x = m.r32s(p + 8) >> 12, y = m.r32s(p + 12) >> 12, z = m.r32s(p + 16) >> 12;
        m.w32(f.at(0x80), static_cast<u32>(x));
        m.w32(f.at(0x84), static_cast<u32>(y));
        m.w32(f.at(0x88), static_cast<u32>(z));
        if (x < -20479 || x >= -18432 || y < -1023 || z >= -6144 || z < -22527) return;
        break;
    }
    case 8:
        if (!(m.r32s(e + 16) - m.r32s(slot(0) + 16) < m.r32s(rd + 12))) return;
        break;
    case 9:
        ground_route_zones(i);
        return;
    default:
        return;
    }
    if (!(m.r8(e + 5) & 4u)) m.w32(sflag(32 + i), m.r32(sflag(32 + i)) | 0x20000u);
}

void Game::ground_route_zones(u32 i) {
    constexpr u32 kZoneTimers = 0x800BCFFCu;
    constexpr u32 kZone = 0x800BD008u;
    constexpr u32 kFade = 0x800BCFF8u;
    constexpr u32 kZones = 0x800B2078u;
    const u32 e = unit(m, i);
    const u32 p = slot(0);
    if (i == 0) {
        s32 best = 0x7FFFFFFF;
        for (u32 k = 0; k < 3; ++k) {
            const s32 d = distance(kZones + 16 * k, p + 8);
            if (d < best) {
                best = d;
                m.w16(kZone, k);
            }
            if (m.r16s(kZoneTimers + 2 * k) > 0 && (m.r32(m.r32(g::kWorld) + 12) & 1u))
                m.w16(kZoneTimers + 2 * k, m.r16(kZoneTimers + 2 * k) - 1u);
        }
        const u32 t = kZoneTimers + 2u * static_cast<u32>(m.r16s(kZone));
        if (m.r16s(t) > 0 && best < 0x4000) m.w16(t, m.r16(t) - 16u);
        if (m.r16s(t) < 0) m.w16(t, 0);
        const u32 w = m.r32(g::kWorld);
        if (m.r32(w + 4) == 4) {
            const u32 ms = m.r32(g::kMission);
            if (best >= 12288) {
                const u32 flags = m.r32(ms);
                m.w32(kFade, m.r32(kFade) - 64);
                m.w32(ms, flags & 0xFFFFFFF7u);
                if (m.r32s(kFade) < 0) m.w32(kFade, 0);
                if (m.r32s(kFade) > 0) m.w32(w + 20, fade_overlay(m.r32(w + 16), m.r32(w + 20), m.r32s(kFade)));
            } else {
                m.w32(ms, m.r32(ms) | 8u);
                m.w32(w + 20, fade_overlay(m.r32(w + 16), m.r32(w + 20), m.r32s(kFade)));
                if (m.r32s(kFade) < 1024) m.w32(kFade, m.r32(kFade) + 16);
                if (!(m.r32(w + 12) & 0x1Fu)) zone_penalty();
            }
        }
    }
    m.w8(e + 5, m.r8(e + 5) | 4u);
    const s32 timer = m.r16s(kZoneTimers + 2u * static_cast<u32>(m.r16s(kZone)));
    m.w8(e + 4, m.r8(m.r32(record(m, i) + 8) + 2) + static_cast<u32>(2 * (3 - (timer >> 12))));
    m.w16(e + 6, static_cast<u32>(timer & 0xFFF));
    const u32 z = kZones + 16u * static_cast<u32>(m.r16s(kZone));
    m.w32(e + 8, m.r32(z) + ((i & 1u) << 24) - 0x800000u);
    m.w32(e + 12, m.r32(z + 4));
    m.w32(e + 16, m.r32(z + 8) - ((i >> 1) << 24) + 0x800000u);
}

void Game::ground_animate(u32 i) {
    cover(0x80051BA0u);
    Frame f(*this, 40);
    const u32 ad = m.r32(record(m, i) + 24);
    const u32 e = unit(m, i);
    const u32 k = i + 32;
    if (!ad && !m.r8(2)) return;
    const s32 key = m.r8s(e + 89);
    const s32 t = m.r32s(e + 96);
    m.w8(e + 5, m.r8(e + 5) | 4u);
    const s32 len = m.r16s(ad + 2u * static_cast<u32>(key) + 4);
    const s32 frac = static_cast<s32>(wrap::div(t << 12, len).lo);
    const s32 nt = m.r32s(e + 96) + m.r32s(e + 104);
    m.w32(e + 96, static_cast<u32>(nt));
    m.w16(e + 6, static_cast<u32>(frac + (key << 12)));
    const s32 cur_len = m.r16s(ad + 2u * static_cast<u32>(m.r8s(e + 89)) + 4);
    if (cur_len >= nt) {
        if (nt < 0) {
            const s32 nk = m.r8s(e + 89) - 1;
            m.w8(e + 89, static_cast<u32>(nk));
            if (static_cast<s8>(nk) >= 0) m.w32(e + 96, static_cast<u32>(m.r16s(ad + 2u * static_cast<u32>(static_cast<s8>(nk)) + 4)));
        }
    } else {
        m.w32(e + 96, static_cast<u32>(nt - cur_len));
        m.w8(e + 89, m.r8(e + 89) + 1u);
    }
    const s32 keys = m.r8(ad + 3);
    if (m.r8s(e + 89) == keys - 1) {
        const u32 end = m.r8(ad + 2);
        if (end == 1) {
            m.w32(sflag(k), m.r32(sflag(k)) & ~0x20000u);
        } else if (end == 2) {
            m.w32(sflag(k), m.r32(sflag(k)) & ~0x20000u);
            ground_set_wreck(i);
        } else if (end == 3) {
            m.w8(e + 89, 0);
            m.w32(e + 96, 0);
        } else {
            const s32 back = keys - 2;
            m.w8(e + 89, static_cast<u32>(back));
            m.w32(e + 104, 0xFFFFFFFFu);
            m.w32(e + 96, static_cast<u32>(m.r16s(ad + 2u * static_cast<u32>(static_cast<s8>(back)) + 4)));
        }
    }
    if (m.r8s(e + 89) < 0) {
        u32 end = m.r8(ad + 2);
        if (end == 4) {
            m.w32(sflag(k), m.r32(sflag(k)) & ~0x20000u);
            end = m.r8(ad + 2);
        }
        if (end == 5) {
            m.w32(sflag(k), m.r32(sflag(k)) & ~0x20000u);
            ground_set_wreck(i);
        } else {
            m.w8(e + 89, 0);
            m.w32(e + 96, 0);
            m.w32(e + 104, 1);
        }
    }
}

void Game::ground_set_wreck(u32 i) {
    cover(0x80051F88u);
    const u32 e = unit(m, i);
    m.w32(e, m.r32(e) & 0xFFFE7BFFu);
    m.w32(sflag(i + 32), (m.r32(sflag(i + 32)) & 0xFF000000u) | 4u);
    m.w8(g::kWreckTimer + i + 32, 150);
}

void Game::ground_check_sink(u32 i) {
    cover(0x80052060u);
    Frame f(*this, 24);
    const u32 kind = m.r8(m.r32(record(m, i) + 20));
    const u32 e = unit(m, i);
    if (kind == 6 && !m.r8(e + 88)) ground_set_wreck(i);
}

u32 Game::record_tile(u32 x, u32 z, u32 tile, u32 old) {
    cover(0x8007EB30u);
    Frame f(*this, 24);
    return record_tile_change(x, z, tile, old);
}

u32 Game::record_tile_change(u32 x, u32 z, u32 tile, u32 old) {
    cover(0x8007E530u);
    Frame f(*this, 48);
    const u32 r = recorder_alloc(2);
    const u32 h = m.r32(r);
    m.w8(r + 4, x);
    m.w8(r + 5, z);
    m.w32(r, (h & 0x3F80u) | 0x42u | ((old & 0x1FFu) << 14) | (tile << 23));
    return r;
}

u32 Game::tile_set(u32 x, u32 z, s32 tile) {
    cover(0x8005E460u);
    Frame f(*this, 24);
    const u32 w = m.r32(g::kWorld);
    const u32 p = (z << 9) + 2 * x + m.r32(w + 584);
    const u32 old = m.r16(p);
    if (tile) {
        m.w16(p, static_cast<u32>(tile) | (old & 0xFE00u));
        if (m.r32(w) & 2u) record_tile(x & 0xFFu, z & 0xFFu, static_cast<u32>(tile), static_cast<u32>(static_cast<s16>(old)) & 0x1FFu);
    }
    return old & 0x1FFu;
}

void Game::terrain_patch(u32 k) {
    cover(0x80048AD4u);
    Frame f(*this, 32);
    u32 p = m.r32(0x800BCEE0u) + 8 * m.r8(m.r32(0x800BCF1Cu) + k);
    while (m.r16(p)) {
        tile_set(m.r8(p + 2), m.r8(p + 3), m.r16s(p + 6));
        p += 8;
    }
}

void Game::destroy_effect(u32 pos, u32 k) {
    cover(0x80089BB4u);
    Frame f(*this, 112);
    for (u32 n = 0; n < 21; ++n) m.w32(f.at(0x10 + 4 * n), m.r32(0x80024874u + 4 * n));
    if (k >= 21) return;
    call_effect_fn(m.r32(0x80024874u + 4 * k), m.r32s(pos), m.r32s(pos + 4), m.r32s(pos + 8));
}

void Game::destroy_effect_recorded(u32 pos, u32 k) {
    cover(0x8008130Cu);
    Frame f(*this, 32);
    const u32 r = record_effect(4, pos);
    m.w32(r + 8, (m.r32(r + 8) & 0xFFFFF00Fu) | ((k & 0xFFu) << 4));
    destroy_effect(pos, k);
}

void Game::ground_destroy_effect(u32 i) {
    cover(0x80052000u);
    Frame f(*this, 24);
    const u32 ad = m.r32(record(m, i) + 24);
    if (!ad) return;
    const u32 k = m.r8(ad);
    if (k) destroy_effect_recorded(unit(m, i) + 8, (k & 0x7Fu) - 1);
}

void Game::ground_destroyed(u32 i) {
    cover(0x80051E34u);
    Frame f(*this, 32);
    const u32 e = unit(m, i);
    m.w32(sflag(i + 32), m.r32(sflag(i + 32)) & ~0x20000u);
    const u32 b5 = m.r8(e + 5);
    const u32 hits = m.r32(e + 84);
    m.w16(e + 6, 0);
    m.w8(e + 5, b5 & 0xFBu);
    m.w32(e + 84, hits & 0x7FFFFFFFu);
    const u32 ad = m.r32(record(m, i) + 24);
    if (!ad) return;
    const u32 wreck = m.r8(ad + 14);
    if (wreck == 254) {
        ground_set_wreck(i);
    } else if (wreck != 255) {
        m.w32(e, m.r32(e) | 0x10000u);
        m.w8(e + 4, m.r8(ad + 14));
    }
    if (m.r8(ad + 15) != 255) terrain_patch(m.r8(ad + 15));
    if (m.r8(ad + 1) == 3 && m.r8(ad + 3)) m.w32(sflag(i + 32), m.r32(sflag(i + 32)) | 0x20000u);
    ground_destroy_effect(i);
}

void Game::ground_check_destroyed(u32 i) {
    cover(0x800520C8u);
    Frame f(*this, 32);
    const u32 k = i + 32;
    const u32 e = unit(m, i);
    if (!(m.r32(sflag(k)) & 1u)) return;
    if (m.r16s(e + 78) > 0) return;
    m.w32(e, m.r32(e) & ~0x200u);
    const u32 ms = m.r32(g::kMission);
    if (m.r32(sflag(k)) & 0x100u) m.w8(ms + 32, m.r8(ms + 32) + 1u);
    if (m.r32(sflag(k)) & 0x200u) m.w8(ms + 33, m.r8(ms + 33) + 1u);
    if ((m.r32(sflag(k)) & 0x800u) && !(m.r32(e + 80) & 0x3Fu) && !(m.r32(e) & 0x40u)) {
        const u32 sc = m.r32(g::kScene);
        if (m.r8(sc + 925) & 1u) kill_camera(m.r32(e));
        if (m.r16(sc + 908) == 22 && m.r16(e + 72) == 70) message_show(46);
        else if (m.r32(e) & 0x80u) message_show(7);
    }
    mission_log_kill(k);
    m.w32(sflag(k), (m.r32(sflag(k)) & 0xFF000000u) | 2u);
    ground_destroyed(i);
    const u32 script = m.r32(record(m, i) + 28);
    if (script) msn_vm_exec(script);
}

void Game::ground_unit_update(u32 i) {
    cover(0x80051A54u);
    Frame f(*this, 48);
    const u32 k = i + 32;
    const u32 rec = record(m, i);
    const u32 ad = m.r32(rec + 24);
    if (m.r32(sflag(k)) & 0x20000u) ground_animate(i);
    if (!(m.r32(sflag(k)) & 1u)) {
        ground_check_sink(i);
        return;
    }
    if (ad && m.r8(ad + 1) == 1 && m.r8(ad + 3)) m.w32(sflag(k), m.r32(sflag(k)) | 0x20000u);
    ground_check_destroyed(i);
    if (!(m.r32(sflag(k)) & 1u)) return;
    ground_route(i);
    if (m.r32(m.r32(g::kWorld) + 4) == 4 && m.r32(rec + 16)) ground_weapons(i);
}

void Game::lock_warning() {
    cover(0x80051840u);
    Frame f(*this, 24);
    if (m.r8(kLockVoice) != 255 && !(m.r8(m.r32(g::kWorld) + 4) & 4u)) {
        voice_release(m.r8(kLockVoice));
        m.w8(kLockVoice, 255);
    }
    m.w8(kLockSource, 255);
    m.w32(kLockDist, 0x100000);
    for (u32 i = 0; i < 32; ++i) {
        const u32 e = unit(m, i);
        if ((m.r32(e) & 0xC000u) != 0xC000u) continue;
        const u32 t = m.r16(e + 72);
        if (!(t - 35u < 2u || static_cast<s16>(t) == 43)) continue;
        const u32 rel = m.r32(g::kRelation + 4 * (i + 32));
        if (!rel) continue;
        if (m.r32s(rel + 4) < m.r32s(kLockDist)) {
            m.w8(kLockSource, i + 32);
            m.w32(kLockDist, m.r32(rel + 4));
        }
    }
    const bool near_src = m.r8(kLockSource) != 255 && m.r32s(kLockDist) < 0x2000;
    if (m.r8(kLockVoice) == 255 || near_src) {
        if (m.r8(kLockSource) == 255) return;
        if (m.r8(kLockVoice) == 255) {
            if (m.r32s(kLockDist) < 0x2000) m.w8(kLockVoice, sound_play_free(92));
            if (m.r8(kLockVoice) == 255) return;
        }
        const u32 v = m.r8(kLockVoice);
        sound_listener(kLockBuf, m.r32(m.r32(g::kWorld) + 40) + 4, slot(m.r8(kLockSource)) + 8);
        voice_reset_mix(v);
        sound_attenuate(m.r8(kLockVoice), kLockBuf, 2);
        sound_pan(m.r8(kLockVoice), kLockBuf);
        sound_height(m.r8(kLockVoice), kLockBuf);
    } else {
        voice_release(m.r8(kLockVoice));
        m.w8(kLockVoice, 255);
    }
}

void Game::ground_flag_idle() {
    cover(0x80051498u);
    for (u32 i = 0; i < 32; ++i) {
        const u32 e = unit(m, i);
        const u32 fl = m.r32(e) & 0xFFFF7FFFu;
        m.w32(e, fl);
        if (!(fl & 0x4000u)) continue;
        const u32 sf = m.r32(sflag(i + 32));
        if (!(sf & 1u)) {
            if (sf & 2u) m.w32(e, fl | 0x8000u);
            continue;
        }
        if ((sf & 0x10u) || (m.r32(m.r32(g::kRelation + 4 * (i + 32))) & 0x80u)) {
            m.w32(e, fl | 0x8000u);
            continue;
        }
        if (m.r16s(slot(i + 32) + 78) <= 0) m.w32(e, fl | 0x8000u);
    }
}

void Game::ground_units_update() {
    cover(0x80051414u);
    Frame f(*this, 24);
    for (u32 i = 0; i < 32; ++i)
        if ((m.r32(unit(m, i)) & 0xC000u) == 0xC000u) ground_unit_update(i);
    if (m.r16(m.r32(g::kScene) + 908) != 41) lock_warning();
}

void Game::scenery_effect(u32 pos, u32 variant, u32 mode) {
    cover(0x80083C3Cu);
    Frame f(*this, 40);
    for (u32 n = 0; n < 4; ++n) m.w32(f.at(0x10 + 4 * n), m.r32(0x80024804u + 4 * n));
    if (mode >= 4) return;
    call_scenery_fn(m.r32(0x80024804u + 4 * mode), pos, variant);
}

void Game::scenery_effect_timed(u32 pos, u32 tick, u32 mode) {
    cover(0x80083CA4u);
    Frame f(*this, 32);
    u32 variant = tick;
    if (mode == 0) {
        if (tick & 0xFu) return;
        variant = 0;
        if (((tick >> 4) & 7u) == 0) variant = (rand() & 7) != 0 ? 1u : 0u;
    } else if (mode == 1 && (tick & 3u)) {
        return;
    }
    const u32 r = record_effect(6, pos);
    const u32 w2 = (m.r32(r + 8) & 0xFFFFF00Fu) | ((variant & 0xFFu) << 4);
    m.w32(r + 4, (m.r32(r + 4) & 0xFFFFF0FFu) | ((mode & 0xFu) << 8));
    m.w32(r + 8, w2);
    scenery_effect(pos, variant, mode);
}

void Game::ground_move() {
    cover(0x8005157Cu);
    Frame f(*this, 40);
    for (u32 i = 0; i < 32; ++i) {
        const u32 e = unit(m, i);
        if ((m.r32(e) & 0xC000u) != 0xC000u || (m.r32(sflag(32 + i)) & 0x400000u)) continue;
        if (m.r32(e + 84) & 0x80000000u) {
            const u32 vy = m.r32(e + 60), vz = m.r32(e + 64);
            m.w32(e + 8, m.r32(e + 8) + m.r32(e + 56));
            const u32 z = m.r32(e + 16) + vz;
            m.w32(e + 12, m.r32(e + 12) + vy);
            m.w32(e + 16, z);
        }
        m.w32(e + 84, m.r32(e + 84) & 0xFF000000u);
    }
    const s32 mission = m.r16s(m.r32(g::kScene) + 908);
    auto smoke = [&](s32 type, u32 mode) {
        for (u32 i = 0; i < 32; ++i) {
            const u32 e = unit(m, i);
            if (!(m.r32(e) & 0x4000u) || !(m.r32(sflag(32 + i)) & 1u)) continue;
            if (!(m.r32s(m.r32(g::kRelation + 4 * (32 + i)) + 4) < 12289)) continue;
            if (m.r16s(e + 72) == type) scenery_effect_timed(e + 8, m.r32(m.r32(g::kScene) + 40) + 8 * i, mode);
        }
    };
    if (mission == 8) smoke(65, 0);
    if (mission == 48) smoke(62, 1);
}

void Game::sim_step4() {
    cover(0x800517F4u);
    Frame frame(*this, 24);
    ground_flag_idle();
    ground_units_update();
    ground_move();
}
}

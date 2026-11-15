#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kHitNormal = 0x800BD0C0u;
constexpr u32 kHitFlags = 0x800BD0C4u;
constexpr u32 kSideNormals = 0x800245C8u;
u32 weapons_base(Memory& m) { return m.r32(g::kWeapons); }
}

u32 Game::record_weapon(u32 kind, u32 slot_i, u32 a3, u32 a4, u32 a5) {
    static constexpr u32 kWrappers[4] = {0x8007E8C4u, 0x8007E900u, 0x8007E930u, 0x8007E968u};
    if (kind < 4) cover(kWrappers[kind]);
    Frame wrapper(*this, 32);
    m.w32(wrapper.at(0x10), a5);
    return record_weapon_kind(kind, slot_i, a3, a4, a5);
}

u32 Game::record_weapon_kind(u32 kind, u32 slot_i, u32 a3, u32 a4, u32 a5) {
    cover(0x8007E2B0u);
    Frame f(*this, 88);
    auto pack = [&](u32 obj, u32 at) {
        const u32 y = ((static_cast<u32>(m.r16(obj + 18)) >> 2) & 0xFFFu) | static_cast<u32>((m.r32s(obj + 4) >> 12) << 12);
        const u32 az = static_cast<u32>(m.r16(obj + 20));
        const s32 z = m.r32s(obj + 8);
        m.w32(at, ((static_cast<u32>(m.r16(obj + 16)) >> 2) & 0xFFFu) | static_cast<u32>((m.r32s(obj) >> 12) << 12));
        m.w32(at + 4, y);
        m.w32(at + 8, ((az >> 2) & 0xFFFu) | static_cast<u32>((z >> 12) << 12));
    };
    u32 out = static_cast<s32>(kind) < 2 ? 1u : 3u;
    switch (kind) {
    case 0: {
        const u32 r = recorder_alloc(6);
        out = r;
        m.w32(r + 20, a5);
        m.w32(r, (m.r32(r) & 0xFFFF0000u) | 0x26u | ((slot_i & 0x7Fu) << 9));
        const s32 vy = m.r32s(a4 + 4), vz = m.r32s(a4 + 8) >> 6;
        m.w16(r + 2, static_cast<u32>(m.r32s(a4) >> 6));
        m.w16(r + 6, static_cast<u32>(vz));
        m.w16(r + 4, static_cast<u32>(vy >> 6));
        pack(a3, r + 8);
        break;
    }
    case 1: {
        const u32 r = recorder_alloc(1);
        out = r;
        const u32 v = (m.r32(r) & 0xFFFFFE00u) | 0xA1u;
        m.w32(r, (v & 0xFFFF01FFu) | ((slot_i & 0x7Fu) << 9));
        break;
    }
    case 2: {
        const u32 r = recorder_alloc(4);
        out = r;
        m.w32(r, (m.r32(r) & 0xFFFF0000u) | 0x124u | ((slot_i & 0x7Fu) << 9));
        m.w16(r + 2, a3);
        pack(a4, r + 4);
        break;
    }
    case 3: {
        const u32 r = recorder_alloc(1);
        out = r;
        const u32 v = (m.r32(r) & 0xFFFFFE00u) | 0x1A1u;
        m.w32(r, (v & 0xFFFF01FFu) | ((slot_i & 0x7Fu) << 9));
        break;
    }
    default: break;
    }
    return out;
}

void Game::damage_queue_add(u32 owner, u32 target_flags, u32 kind, s32 delay) {
    cover(0x80068454u);
    const u32 w = weapons_base(m);
    const u32 mask = m.r32(w + 13280);
    if (mask == 0xFFFFFFFFu) return;
    u32 k = 0;
    if (mask & 1u) {
        k = 1;
        while (mask & wrap::shl(1u, k)) ++k;
    }
    const u32 e = w + 12 * k + 13284;
    m.w32(w + 13280, mask | wrap::shl(1u, k));
    m.w16(e, static_cast<u32>(delay));
    m.w16(e + 2, kind);
    m.w32(e + 4, target_flags);
    m.w32(e + 8, owner);
}

void Game::damage_queue_update() {
    cover(0x800684D4u);
    const u32 w = weapons_base(m);
    u32 mask = m.r32(w + 13280);
    if (!mask) return;
    for (u32 i = 0; i < 32; ++i) {
        if (!(mask & wrap::shl(1u, i))) continue;
        const u32 e = w + 13284 + 12 * i;
        const s16 d = static_cast<s16>(m.r16(e) - 1u);
        m.w16(e, static_cast<u32>(d));
        if (d >= 0) continue;
        const u32 t = slot(m.r32(e + 4) & 0x3Fu);
        mask &= ~wrap::shl(1u, i);
        const u32 tf = m.r32(t);
        if ((tf & 0x4200u) == 0x4200u && !(tf & m.r32(e + 8) & 0x1C0u)) {
            m.w32(t + 84, m.r32(t + 84) | m.r16(e + 2));
            m.w32(t + 80, m.r32(e + 8));
        }
    }
    m.w32(weapons_base(m) + 13280, mask);
}

void Game::blast_damage(u32 obj, s32 radius) {
    cover(0x8006A978u);
    Frame f(*this, 56);
    const s32 reach = static_cast<s32>(3u * static_cast<u32>(radius));
    const s32 x = m.r32s(obj + 28), y = m.r32s(obj + 32), z = m.r32s(obj + 36);
    const u32 team = m.r32(obj + 108) & 0x1C0u;
    for (u32 i = 0; i < 64; ++i) {
        const u32 e = slot(i);
        const u32 ef = m.r32(e);
        if ((ef & 0x4200u) != 0x4200u || (ef & team)) continue;
        s32 dx = static_cast<s32>(m.r32(e + 8) - static_cast<u32>(x)) >> 12;
        s32 dy = static_cast<s32>(m.r32(e + 12) - static_cast<u32>(y)) >> 12;
        s32 dz = static_cast<s32>(m.r32(e + 16) - static_cast<u32>(z)) >> 12;
        dx = dx < 0 ? -dx : dx;
        dy = dy < 0 ? -dy : dy;
        dz = dz < 0 ? -dz : dz;
        if (!(reach >= dx + dy + dz + m.r16s(e + 74))) continue;
        const s32 d = distance(e + 8, obj + 28);
        if (radius >= d) {
            const s32 divisor = m.r32s(obj + 160);
            if (!divisor) unported(0x8006AA8Cu, "blast delay division by zero");
            if (divisor == -1 && d == (-2147483647 - 1))
                unported(0x8006AAA4u, "blast delay division overflow");
            damage_queue_add(m.r32(obj + 108), ef, m.r16(obj + 26), static_cast<s32>(wrap::div(d, m.r32s(obj + 160)).lo));
        }
    }
}

void Game::direct_damage(u32 obj) {
    cover(0x8006AAFCu);
    Frame f(*this, 24);
    if (m.r32(obj) & 1u) return;
    const u32 tf = m.r32(slot(m.r32(obj + 112) & 0x3Fu));
    if ((tf & 0x4200u) != 0x4200u) return;
    const u32 owner = m.r32(obj + 108);
    if (tf & 0xFFFFu & owner & 0x1C0u) return;
    const s32 blastSize = m.r32s(obj + 156), divisor = m.r32s(obj + 160);
    if (!divisor) unported(0x8006AB7Cu, "direct delay division by zero");
    if (divisor == -1 && blastSize == (-2147483647 - 1))
        unported(0x8006AB94u, "direct delay division overflow");
    damage_queue_add(owner, tf, m.r16(obj + 26),
                     static_cast<s32>(wrap::div(m.r32s(obj + 156), m.r32s(obj + 160)).lo));
}

void Game::weapon_thrust(u32 w, u32 accel_at) {
    if (m.r32s(w + 80) < m.r32s(w + 128)) m.w32(w + 96, m.r32(w + 96) + m.r32(accel_at));
    m.w32(w + 80, m.r32(w + 96) + m.r32(w + 100));
}

void Game::weapon_level_roll(u32 w) {
    if (!(m.r32(w) & 8u)) return;
    const s32 roll = m.r16s(w + 48);
    const s32 lim = 4 * m.r16s(w + 124);
    if (lim >= (roll < 0 ? -roll : roll)) {
        m.w16(w + 92, 0u - m.r16(w + 48));
        m.w32(w, m.r32(w) & ~8u);
    } else {
        m.w16(w + 92, static_cast<u32>(roll >= 0 ? -lim : lim));
    }
    m.w32(w, m.r32(w) | 0x2000u);
}

void Game::wpn_dropped_then_thrust(u32 w) {
    cover(0x8006903Cu);
    const s16 ph = m.r16s(w + 8);
    if (ph == 1 || ph == 0) {
        if (ph == 0) m.w16(w + 8, 1);
        if (m.r16s(w + 126) < m.r16s(w + 10)) {
            if (m.r32(w) & 1u) {
                m.w16(w + 8, m.r16(w + 8) + 1u);
            } else {
                m.w8(w + 4, 1);
                m.w16(w + 8, 0);
                m.w16(w + 10, 0xFFFF);
            }
        }
        const u32 thrust = m.r32(w + 96) + m.r32(w + 132);
        m.w32(w + 96, thrust);
        m.w32(w + 80, thrust + m.r32(w + 100));
        m.w32(w, m.r32(w) | 0x4000u);
    }
    weapon_level_roll(w);
}

void Game::wpn_dropped_falling(u32 w) {
    cover(0x80069158u);
    const s32 ph = m.r16s(w + 8);
    if (ph == 1 || (ph < 2 && m.r16(w + 8) == 0)) {
        if (ph != 1) m.w16(w + 8, m.r16(w + 8) + 1u);
        if (m.r16s(w + 126) < m.r16s(w + 10)) {
            if (m.r32(w) & 1u) {
                m.w16(w + 8, m.r16(w + 8) + 1u);
            } else {
                m.w8(w + 4, 1);
                m.w16(w + 8, 0);
                m.w16(w + 10, 0xFFFF);
            }
            m.w32(w + 76, 0);
        }
        if (m.r16s(w + 10) >= 7) weapon_thrust(w, w + 132);
        else m.w32(w + 76, m.r32(w + 76) + 669u);
        m.w32(w, m.r32(w) | 0x4000u);
    }
    weapon_level_roll(w);
}

void Game::wpn_bomb(u32 w) {
    cover(0x80069D18u);
    if (m.r16(w + 8)) {
        if (m.r16(w + 8) != 1) return;
    } else {
        m.w16(w + 8, m.r16(w + 8) + 1u);
    }
    const bool steep = m.r16s(w + 44) < -2047;
    m.w32(w + 60, m.r32(w + 60) + 669u);
    if (!steep) m.w16(w + 44, m.r16(w + 44) - 32u);
}

void Game::wpn_rocket(u32 w) {
    cover(0x80069F48u);
    if (m.r16(w + 8)) {
        if (m.r16(w + 8) != 1) return;
    } else {
        m.w16(w + 8, m.r16(w + 8) + 1u);
    }
    weapon_thrust(w, w + 132);
    m.w32(w, m.r32(w) | 0x4000u);
}

void Game::wpn_inert(u32 w) {
    cover(0x80069D6Cu);
    if (!m.r16(w + 8)) m.w16(w + 8, 1);
}

void Game::wpn_homing(u32 w, bool may_skip_lead) {
    Frame f(*this, 40);
    const u32 lead = f.at(0x10);
    const s32 speed = m.r32s(w + 80);
    if (speed == -1 && (m.r32(w + 104) << 12) == 0x80000000u)
        unported(may_skip_lead ? 0x8006951Cu : 0x80069AECu, "missile lead division overflow");
    s32 t = 1;
    if (speed) t = static_cast<s32>(wrap::div(static_cast<s32>(static_cast<u32>(m.r32(w + 104)) << 12), speed).lo);
    if (may_skip_lead && (m.r32(w) & 0x100u)) t = 0;
    const u32 tv = m.r32(w + 120), tp = m.r32(w + 116);
    const u32 arrival = static_cast<u32>(t);
    const u32 leadY = m.r32(tv + 4) * arrival, leadZ = m.r32(tv + 8) * arrival;
    m.w32(lead, m.r32(tp) + m.r32(tv) * arrival);
    m.w32(lead + 4, m.r32(tp + 4) + leadY);
    m.w32(lead + 8, m.r32(tp + 8) + leadZ);
    const s32 d = steer_to(w + 28, lead, w + 88);
    const s32 lim = m.r16s(w + 124);
    m.w32(w + 104, static_cast<u32>(d));
    m.w16(w + 92, 0);
    auto clamp = [&](u32 at) {
        const s32 v = m.r16s(at);
        m.w16(at, static_cast<u32>(v < -lim ? -lim : lim < v ? lim : v));
    };
    clamp(w + 88);
    clamp(w + 90);
    if (m.r32s(w + 80) < m.r32s(w + 128)) m.w32(w + 96, m.r32(w + 96) + m.r32(w + 136));
    m.w32(w + 80, m.r32(w + 96) + m.r32(w + 100));
    m.w32(w, m.r32(w) | 0x5000u);
    weapon_level_roll(w);
}

void Game::wpn_cruise(u32 w) {
    weapon_cruise(w);
}

u32 Game::submunition_spawn(u32 owner, u32 pos, u32 vel, u32 target) {
    cover(0x80068E28u);
    Frame f(*this, 32);
    const u32 o = weapon_alloc(owner, pos, vel, static_cast<s32>(target));
    if (!o) return 0;
    const u32 b53 = m.r8(o + 53);
    m.w16(o + 26, 256);
    m.w8(o + 52, 2);
    m.w16(o + 6, 4);
    m.w8(o + 5, 0);
    m.w8(o + 53, b53 | 0x18u);
    m.w16(o + 126, static_cast<u32>(rand() & 3));
    m.w16(o + 48, m.r16(o + 48) - 512u + static_cast<u32>(rand() & 0x3FF));
    if (target & 0x20u) {
        m.w16(o + 24, 150);
        m.w16(o + 124, 24);
        m.w32(o + 132, 4096);
        m.w32(o + 136, 4096);
        m.w32(o + 156, 640);
        const u32 fl = m.r32(o);
        m.w32(o + 128, 245760);
        m.w32(o + 160, 192);
        m.w32(o, fl | 0x10u);
    } else {
        m.w16(o + 24, 240);
        m.w16(o + 124, 48);
        m.w32(o + 132, 512);
        m.w32(o + 128, 450560);
        m.w32(o + 136, 3072);
    }
    return o;
}

void Game::wpn_cluster(u32 w) {
    weapon_split(w);
}

void Game::wpn_explode(u32 w) {
    weapon_explode(w);
}

void Game::wpn_blast(u32 w, u32 size, u32 ground_sound) {
    weapon_blast(w, size, ground_sound);
}

void Game::wpn_explode_on_target(u32 w) {
    weapon_explode_target(w);
}

void Game::call_weapon_handler(u32 fn, u32 w) {
    switch (fn) {
    case 0x8006903Cu: wpn_dropped_then_thrust(w); break;
    case 0x80069158u: wpn_dropped_falling(w); break;
    case 0x80069D18u: wpn_bomb(w); break;
    case 0x80069F48u: wpn_rocket(w); break;
    case 0x80069D6Cu: wpn_inert(w); break;
    case 0x800694CCu: cover(0x800694CCu); wpn_homing(w, true); break;
    case 0x800692C4u: cover(0x800692C4u); wpn_homing(w, true); break;
    case 0x800696D4u: wpn_cruise(w); break;
    case 0x80069900u: wpn_cluster(w); break;
    case 0x80069A9Cu: cover(0x80069A9Cu); wpn_homing(w, false); break;
    case 0x80069C8Cu: wpn_explode(w); break;
    case 0x80069D88u: cover(0x80069D88u); wpn_blast(w, 0, 37); break;
    case 0x80069E10u: wpn_explode_on_target(w); break;
    case 0x80069EC0u: cover(0x80069EC0u); wpn_blast(w, 1, 29); break;
    case 0x80069FC0u: break;
    default: call_by_address(fn, "weapon behaviour", w); break;
    }
}

void Game::weapons_behave() {
    cover(0x80069FC8u);
    Frame f(*this, 56);
    const u32 mask = m.r32(weapons_base(m) + 5448);
    if (!mask) return;
    for (u32 i = 0; i < 32; ++i) {
        if (!(mask & wrap::shl(1u, i))) continue;
        const u32 w = weapons_base(m) + 200 + 164 * i;
        const u32 kind = m.r8(w + 4);
        u32 fn;
        if (kind == 1) fn = m.r32(0x800B2C94u + 4 * m.r8(w + 6));
        else if (kind == 2) fn = m.r32(0x800B2CA8u + 4 * m.r8(w + 7));
        else if (kind == 0) fn = m.r32(0x800B2C80u + 4 * m.r8(w + 5));
        else fn = 0x80069FC0u;
        call_weapon_handler(fn, w);
        m.w16(w + 10, m.r16(w + 10) + 1u);
    }
}

void Game::weapons_move() {
    cover(0x8006ABB8u);
    const u32 mask = m.r32(weapons_base(m) + 5448);
    if (!mask) return;
    for (u32 i = 0; i < 32; ++i) {
        if (!(mask & wrap::shl(1u, i))) continue;
        const u32 p = weapons_base(m) + 236 + 164 * i;
        const u32 vy = m.r32(p + 24), vz = m.r32(p + 28);
        m.w32(p - 8, m.r32(p - 8) + m.r32(p + 20));
        const u32 z = m.r32(p) + vz;
        m.w32(p - 4, m.r32(p - 4) + vy);
        m.w32(p, z);
    }
}

void Game::weapons_expire() {
    cover(0x8006AC34u);
    Frame f(*this, 56);
    const u32 w0 = weapons_base(m);
    u32 mask = m.r32(w0 + 5448);
    if (!mask) return;
    for (u32 i = 0; i < 32; ++i) {
        const u32 bit = wrap::shl(1u, i);
        const u32 w = weapons_base(m) + 200 + 164 * i;
        if (!(mask & bit)) continue;
        u32 fl = m.r32(w);
        if (fl & 0x2000u) {
            fl &= ~0x2000u;
            m.w16(w + 48, m.r16(w + 48) + m.r16(w + 92));
        }
        if (fl & 0x1000u) {
            attitude_integrate(w + 44, w + 88);
            fl &= ~0x1000u;
        }
        if (fl & 0x4000u) {
            rotate_by_angles(w + 72, w + 44, w + 56);
            fl &= ~0x4000u;
        }
        if (fl & 0x40u) puff_spawn_recorded(w + 28, w + 56, 0);
        if (fl & 0x80u) puff_spawn_recorded(w + 28, w + 56, 1);
        m.w32(w, fl);
        const s16 life = static_cast<s16>(m.r16(w + 24) - 1u);
        m.w16(w + 24, static_cast<u32>(life));
        if (life >= 0) {
            if (m.r32(w + 108) & 0x2000u) record_weapon(2, i, static_cast<u32>(m.r16s(w + 52)), w + 28, 0);
            continue;
        }
        if (m.r32(w) & 0x10u) blast_damage(w, m.r32s(w + 156));
        if (m.r32(w) & 0x20u) direct_damage(w);
        if (m.r32(w + 108) & 0x2000u) record_weapon(3, i, 0, 0, 0);
        const u32 owner_count = weapons_base(m) + (m.r32(w + 108) & 0x3Fu) + 72;
        m.w8(owner_count, m.r8(owner_count) - 1u);
        const u32 target = m.r32(w + 112);
        mask &= ~bit;
        if (target == 0xFFFFFFFFu) continue;
        const u32 locks = weapons_base(m) + (target & 0x3Fu) + 136;
        const u32 n = (m.r8(locks) - 1u) & 0xFFu;
        m.w8(locks, n);
        if (n) continue;
        const u32 tb = wrap::shl(1u, target & 0x1Fu);
        if (target & 0x20u) m.w32(weapons_base(m) + 4, m.r32(weapons_base(m) + 4) & ~tb);
        else m.w32(weapons_base(m), m.r32(weapons_base(m)) & ~tb);
        if (!(m.r8(w) & 2u)) {
            const u32 te = slot(target & 0x3Fu);
            const u32 hits = m.r32(te + 84);
            if (!(hits & 0x7FC0u)) {
                m.w32(te + 84, hits | 0x8000u);
                m.w32(te + 80, m.r32(w + 108));
            }
        }
    }
    m.w32(weapons_base(m) + 5448, mask);
}

static u32 hit_candidates(Game& g, Memory& m, u32 radii) {
    u32 n = 0, at = g::kScratch;
    for (u32 i = 0; i < 64; ++i) {
        const u32 e = g.slot(i);
        const u32 ef = m.r32(e);
        if ((ef & 0x4200u) != 0x4200u) continue;
        ++n;
        const s32 r = m.r16s(e + 0x4A);
        m.w32(at, static_cast<u32>(m.r32s(e + 8) >> 12));
        m.w32(at + 4, static_cast<u32>(m.r32s(e + 12) >> 12));
        m.w32(at + 8, static_cast<u32>(m.r32s(e + 16) >> 12));
        m.w32(at + 12, ef);
        at += 16;
        m.w32(radii, static_cast<u32>(3 * r));
        m.w32(radii + 4, static_cast<u32>(r * r));
        radii += 8;
    }
    return n;
}

static bool sphere_hit(Memory& m, u32 k, u32 radii, s32 x, s32 y, s32 z, s32 reach, s32 reach_sq9, u32 squareScratch) {
    const u32 at = g::kScratch + 16 * k;
    const s32 dx = m.r32s(at) - x, dz = m.r32s(at + 8) - z, dy = m.r32s(at + 4) - y;
    const s32 man = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) + (dz < 0 ? -dz : dz);
    if (wrap::add(m.r32s(radii + 8 * k), reach) < man) return false;
    const s32 ySquared = wrap::mul(dy, dy);
    m.w32(squareScratch, static_cast<u32>(ySquared));
    const s32 distanceSquared = wrap::add(wrap::add(wrap::mul(dx, dx), ySquared), wrap::mul(dz, dz));
    return wrap::add(m.r32s(radii + 8 * k + 4), reach_sq9) >= distanceSquared;
}

void Game::weapons_collide() {
    cover(0x8006A11Cu);
    Frame f(*this, 0x2A8);
    const u32 radii = f.at(0x18), models = f.at(0x218), end_pt = f.at(0x258), hit = f.at(0x268);
    const u32 mask = m.r32(weapons_base(m) + 0x1548);
    m.w32(f.at(0x278), mask);
    u32 n_models = 0;
    if (!mask) return;
    const u32 n = hit_candidates(*this, m, radii);
    u32 s4 = reg_s4;
    for (u32 i = 0; i < 32; ++i) {
        if (!(mask & wrap::shl(1u, i))) continue;
        const u32 w = weapons_base(m) + 200 + 164 * i;
        const s32 vx = m.r32s(w + 56), vy = m.r32s(w + 60), vz = m.r32s(w + 64);
        auto magnitude = [](s32 value) { return value < 0 ? wrap::neg(value) : value; };
        const s32 reach = wrap::add(wrap::add(magnitude(vx), magnitude(vy)), magnitude(vz)) >> 13;
        const s32 ny = wrap::add(m.r32s(w + 32), vy) >> 12;
        const s32 nx = wrap::add(m.r32s(w + 28), vx) >> 12;
        const s32 nz = wrap::add(m.r32s(w + 36), vz) >> 12;
        const s32 reach9 = wrap::mul(reach, reach) / 9;
        if (!n) continue;
        s4 = ~(m.r32(w + 108) & 0x1C0u) & 0x1C0u;
        for (u32 k = 0; k < n; ++k) {
            const u32 ef = m.r32(g::kScratch + 16 * k + 12);
            if (!(ef & s4)) continue;
            if (!sphere_hit(m, k, radii, nx, ny, nz, reach, reach9, f.at(0x27C))) continue;
            const u32 e = slot(ef & 0x3Fu);
            if (ef & 0x1800u) {
                m.w32(models + 8 * n_models, w);
                m.w32(models + 8 * n_models + 4, e);
                if (n_models < 8) ++n_models;
                continue;
            }
            m.w32(e + 84, m.r32(e + 84) | m.r16(w + 26));
            m.w32(e + 80, m.r32(w + 108));
            const u32 ox = m.r32(w + 56), oy = m.r32(w + 60), oz = m.r32(w + 64);
            m.w32(w + 28, static_cast<u32>(nx) << 12);
            m.w32(w + 32, static_cast<u32>(ny) << 12);
            m.w32(w + 36, static_cast<u32>(nz) << 12);
            m.w32(w + 56, 0);
            m.w32(w + 60, 0);
            m.w32(w + 64, 0);
            m.w32(w + 140, ox);
            m.w32(w + 144, oy);
            m.w32(w + 148, oz);
            m.w32(w, m.r32(w) | 2u);
            m.w16(w + 24, 0);
            m.w8(w + 4, 2);
            m.w16(w + 8, 0);
            m.w16(w + 10, 0);
        }
    }
    for (u32 k = 0; k < n_models; ++k) {
        const u32 w = m.r32(models + 8 * k), e = m.r32(models + 8 * k + 4);
        s32 r;
        if ((m.r32(e) & 0x1020u) == 0x1020u && !(m.r8(e + 5) & 2u)) {
            m.w32(f.at(0x10), g::kScratch);
            r = model_raycast(w + 0x1C, w + 0x38, e, end_pt, g::kScratch, s4);
        } else {
            m.w32(end_pt, m.r32(w + 0x1C));
            m.w32(end_pt + 4, m.r32(w + 0x20));
            m.w32(end_pt + 8, m.r32(w + 0x24));
            r = 0;
        }
        if (0x7FFFFFFE < r) continue;
        m.w32(e + 0x54, m.r32(e + 0x54) | m.r16(w + 0x1A));
        m.w32(e + 0x50, m.r32(w + 0x6C));
        const u32 vx = m.r32(w + 0x38), vy = m.r32(w + 0x3C), vz = m.r32(w + 0x40);
        m.w32(w + 0x1C, m.r32(end_pt));
        m.w32(w + 0x20, m.r32(end_pt + 4));
        const u32 z = m.r32(end_pt + 8);
        const u32 fl = m.r32(w);
        m.w32(w + 0x8C, vx);
        m.w32(w + 0x90, vy);
        m.w32(w + 0x94, vz);
        m.w32(w + 0x38, 0);
        m.w32(w + 0x3C, 0);
        m.w32(w + 0x40, 0);
        m.w16(w + 0x18, 0);
        m.w16(w + 8, 0);
        m.w16(w + 0xA, 0);
        m.w32(w, fl | 2u);
        m.w8(w + 4, 2);
        m.w32(w + 0x24, z);
    }
    const u32 world = m.r32(g::kWorld);
    const u32 b = world + 0x2A0, grid = m.r32(world + 0x244 + 4);
    for (u32 i = 0; i < 32; ++i) {
        if (!(mask & wrap::shl(1u, i))) continue;
        const u32 w = weapons_base(m) + 200 + 164 * i;
        const s32 x = m.r32s(w + 28), y = m.r32s(w + 32), z = m.r32s(w + 36);
        s32 tx = wrap::add(m.r32s(w + 56), x), ty = wrap::add(m.r32s(w + 60), y), tz = wrap::add(m.r32s(w + 64), z);
        u32 side = 0, surface = 0;
        if (tx < m.r32s(b)) { side = 1; surface = 4; tx = m.r32s(b); }
        if (m.r32s(b + 4) < tx) { side = 2; surface = 4; tx = m.r32s(b + 4); }
        if (ty < m.r32s(b + 8)) { side = 3; surface = 3; ty = m.r32s(b + 8); }
        if (m.r32s(b + 12) < ty) { side = 4; surface = 1; ty = m.r32s(b + 12); }
        if (tz < m.r32s(b + 16)) { side = 5; surface = 4; tz = m.r32s(b + 16); }
        if (m.r32s(b + 20) < tz) { side = 6; surface = 4; tz = m.r32s(b + 20); }
        if (side) {
            m.w32(w, m.r32(w) | 4u);
            const u32 nrm = kSideNormals + 8 * side;
            m.w16(w + 24, 0);
            m.w32(w + 28, static_cast<u32>(tx));
            m.w32(w + 32, static_cast<u32>(ty));
            m.w32(w + 36, static_cast<u32>(tz));
            const u32 vz = m.r32(w + 64);
            m.w16(w + 16, m.r16(nrm));
            m.w16(w + 18, m.r16(nrm + 2));
            const u32 nz = m.r16(nrm + 4);
            const u32 vx = m.r32(w + 56), vy = m.r32(w + 60);
            m.w32(kHitFlags, surface);
            m.w32(w + 12, surface);
            m.w32(w + 148, vz);
            m.w32(w + 56, 0);
            m.w32(w + 60, 0);
            m.w32(w + 64, 0);
            m.w16(w + 8, 0);
            m.w16(w + 10, 0);
            m.w32(w + 140, vx);
            m.w32(w + 144, vy);
            m.w8(w + 4, 2);
            m.w16(w + 20, nz);
            continue;
        }
        auto cell = [](s32 v) {
            s32 c = (v >> 23) + 0x80;
            if (c < 0) c &= 7;
            if (c >= 0x100) c = (c & 7) + 0xF8;
            return c;
        };
        const s32 c0 = cell(x) + (cell(z) << 8), c1 = cell(tx) + (cell(tz) << 8);
        m.w32(end_pt, static_cast<u32>(tx));
        m.w32(end_pt + 4, static_cast<u32>(ty));
        m.w32(end_pt + 8, static_cast<u32>(tz));
        s32 r;
        if (c0 == c1) {
            const u32 tile = m.r16(grid + 2u * static_cast<u32>(c0)) & 0x1FFu;
            if (!tile) continue;
            const s32 top = m.r16s(m.r32(m.r32(world + 0x244 + 8) + 4 * tile) + 0x16);
            if ((y >> 12) < top && (ty >> 12) < top) continue;
            r = ray_cell(w + 28, end_pt, hit, g::kScratch);
        } else {
            r = terrain_raycast(w + 28, end_pt, hit, g::kScratch);
        }
        if (0x7FFFFFFE < r) continue;
        const u32 nrm = m.r32(kHitNormal);
        m.w32(w, m.r32(w) | 4u);
        m.w16(w + 24, 0);
        const u32 vx = m.r32(w + 56), vz = m.r32(w + 64);
        m.w32(w + 28, m.r32(hit));
        m.w32(w + 32, m.r32(hit + 4));
        const u32 hz = m.r32(hit + 8);
        const u32 vy = m.r32(w + 60);
        m.w32(w + 140, vx);
        m.w32(w + 148, vz);
        m.w32(w + 56, 0);
        m.w32(w + 60, 0);
        m.w32(w + 64, 0);
        m.w32(w + 144, vy);
        m.w32(w + 36, hz);
        m.w16(w + 16, m.r16(nrm));
        m.w16(w + 18, m.r16(nrm + 2));
        const u32 fl = m.r32(kHitFlags);
        const u32 nz = m.r16(nrm + 4);
        m.w16(w + 8, 0);
        m.w16(w + 10, 0);
        m.w32(w + 12, fl);
        m.w8(w + 4, 2);
        m.w16(w + 20, nz);
    }
}

void Game::weapons_update() {
    cover(0x8006AF20u);
    Frame f(*this, 24);
    weapons_collide();
    weapons_move();
    weapons_behave();
    weapons_expire();
}

void Game::sim_step6() {
    cover(0x8006C5B4u);
    Frame frame(*this, 24);
    bullets_spawn();
    bullets_update();
    weapons_update();
    damage_queue_update();
}
}

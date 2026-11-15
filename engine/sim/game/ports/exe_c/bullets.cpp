#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kHitFlags = 0x800BD0C4u;
constexpr u32 kBullets = 0x15D0u;
constexpr u32 kBulletMasks = 0x33D0u;
u32 weapons_base(Memory& m) { return m.r32(g::kWeapons); }

struct Gun { u32 flags, life; u16 speed, tracer; };
constexpr Gun kGuns[6] = {
    {0x20410u, 0x25u, 0x1580u, 0x800u}, {0x20820u, 0x18u, 0x19C0u, 0x990u},
    {0x41040u, 0x3Eu, 0x1020u, 0x4C0u}, {0x82080u, 0x23u, 0x1120u, 0u},
    {0x94103u, 0x10u, 0x35C0u, 0u},     {0x98202u, 0x10u, 0x2B00u, 0x800u},
};

u32 effect_kind(u32 flags) {
    if (flags & 0x20u) return 1;
    if (flags & 0x40u) return 2;
    if (flags & 0x80u) return 3;
    if (flags & 0x100u) return 4;
    if (flags & 0x200u) return 5;
    return 0;
}
}

void Game::bullets_spawn() {
    cover(0x8006B110u);
    Frame f(*this, 0xA0);
    const u32 ra = f.at(0x10), rb = f.at(0x20), rot = f.at(0x30), va = f.at(0x50), vb = f.at(0x58), rec = f.at(0x60);
    for (s32 k = 0; k < m.r32s(weapons_base(m) + 0x154C); ++k) {
        const u32 w = weapons_base(m);
        const u32 req = w + 0x1550 + 16u * static_cast<u32>(k);
        const u32 mount = m.r32(req + 8);
        const u32 b = w + kBullets + 60u * static_cast<u32>(m.r16s(req));
        m.w32(b + 0x38, m.r32(req + 4));
        m.w16(va, m.r16(mount + 0x10));
        m.w16(va + 2, m.r16(mount + 0x12));
        m.w16(va + 4, m.r16(mount + 0x14));
        rot_matrix_yxz(va, rot);
        for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(rot + 4 * r));
        const u32 gun = static_cast<u32>(m.r16s(req + 2));
        if (gun < 6) {
            const Gun& g = kGuns[gun];
            m.w32(b, g.flags);
            m.w32(b + 4, g.life);
            m.w16(va, 0);
            m.w16(va + 2, 0);
            m.w16(va + 4, g.speed);
            m.w16(vb, 0);
            m.w16(vb + 2, 0);
            m.w16(vb + 4, g.tracer);
        }
        auto turn = [&](u32 v, u32 out) {
            gte.mtc2(0, m.r32(v));
            gte.mtc2(1, m.r32(v + 4));
            gte.op(0x486012);
            m.w32(out, gte.mfc2(25));
            m.w32(out + 4, gte.mfc2(26));
            m.w32(out + 8, gte.mfc2(27));
        };
        turn(va, ra);
        turn(vb, rb);
        s32 sx = 0, sy = 0, sz = 0;
        if (!(m.r32(b) & 0x10000u)) {
            sx = (0x4000 - rand()) >> 1;
            sy = (0x4000 - rand()) >> 1;
            sz = (0x4000 - rand()) >> 1;
        }
        if (!(m.r32(b) & 2u)) sy -= 0x3332;
        m.w32(b + 0x28, (m.r32(ra) << 7) + m.r32(m.r32(req + 12)) + static_cast<u32>(sx));
        m.w32(b + 0x2C, (m.r32(ra + 4) << 7) + m.r32(m.r32(req + 12) + 4) + static_cast<u32>(sy));
        const u32 vz = (m.r32(ra + 8) << 7) + m.r32(m.r32(req + 12) + 8) + static_cast<u32>(sz);
        m.w32(b + 0x30, vz);
        const u32 x = m.r32(mount) - m.r32(b + 0x28);
        const u32 y = m.r32(mount + 4) - m.r32(b + 0x2C);
        const u32 z = m.r32(mount + 8) - vz;
        m.w32(b + 8, x);
        m.w32(b + 0xC, y);
        m.w32(b + 0x10, z);
        m.w32(b + 0x18, x + (m.r32(rb) << 8));
        m.w32(b + 0x1C, y + (m.r32(rb + 4) << 8));
        m.w32(b + 0x20, z + (m.r32(rb + 8) << 8));
        if (m.r32(b + 0x38) & 0x2000u) {
            m.w32(rec, static_cast<u32>(x));
            m.w32(rec + 4, m.r32(b + 0xC));
            m.w32(rec + 8, m.r32(b + 0x10));
            m.w16(rec + 0x10, m.r16(mount + 0x10));
            m.w16(rec + 0x12, m.r16(mount + 0x12));
            m.w16(rec + 0x14, m.r16(mount + 0x14));
            record_weapon(0, static_cast<u32>(m.r16s(req)), rec, b + 0x28, static_cast<u32>(m.r16s(req + 2)));
        }
    }
    m.w32(weapons_base(m) + 0x154C, 0);
}

static void surface_sound(u32 flags, u32 surface, u32& id, u32& shift) {
    id = 0x1E;
    shift = 2;
    if (flags & 0x20000u) {
        if ((surface & 0xFu) == 1) id = 0x23;
    } else if (flags & 0x40000u) {
        id = (surface & 0xFu) == 1 ? 0x23u : 0x28u;
    } else if (flags & 0x80000u) {
        id = 0x29;
        shift = 1;
    }
}

void Game::bullets_update() {
    cover(0x8006B504u);
    Frame f(*this, 0x2D0);
    const u32 radii = f.at(0x18), expired = f.at(0x218), models = f.at(0x220);
    const u32 hit = f.at(0x260), at = f.at(0x270);
    u32 n = 0;
    for (u32 i = 0, e_at = g::kScratch; i < 64; ++i) {
        const u32 e = slot(i);
        const u32 ef = m.r32(e);
        if ((ef & 0x4200u) != 0x4200u) continue;
        ++n;
        const s32 r = m.r16s(e + 0x4A);
        m.w32(e_at, static_cast<u32>(m.r32s(e + 8) >> 12));
        m.w32(e_at + 4, static_cast<u32>(m.r32s(e + 12) >> 12));
        m.w32(e_at + 8, static_cast<u32>(m.r32s(e + 16) >> 12));
        m.w32(e_at + 12, ef);
        e_at += 16;
        m.w32(radii + 8 * (n - 1), static_cast<u32>(3 * r));
        m.w32(radii + 8 * (n - 1) + 4, static_cast<u32>(r * r));
    }
    const u32 s4 = g::kScratch + 16 * n;
    u32 n_models = 0;

    const u32 masks = weapons_base(m) + kBulletMasks;
    for (u32 grp = 0; grp < 128; grp += 32) {
        const u32 mask = m.r32(masks + grp / 8);
        if (!mask) continue;
        for (u32 i = 0; i < 32; ++i) {
            if (!(mask & wrap::shl(1u, i))) continue;
            const u32 b = weapons_base(m) + kBullets + 60 * (grp + i);
            const s32 vx = m.r32s(b + 0x28), vy = m.r32s(b + 0x2C), vz = m.r32s(b + 0x30);
            auto magnitude = [](s32 value) { return value < 0 ? wrap::neg(value) : value; };
            const s32 reach = wrap::add(wrap::add(magnitude(vx), magnitude(vy)), magnitude(vz)) >> 13;
            const s32 ny = wrap::add(m.r32s(b + 0xC), vy) >> 12;
            const s32 nx = wrap::add(m.r32s(b + 8), vx) >> 12;
            const s32 nz = wrap::add(m.r32s(b + 0x10), vz) >> 12;
            const s32 reach9 = wrap::mul(reach, reach) / 9;
            if (n == 0) continue;
            const u32 team = ~(m.r32(b + 0x38) & 0x1C0u) & 0x1C0u;
            for (u32 k = 0; k < n; ++k) {
                const u32 ea = g::kScratch + 16 * k;
                const u32 ef = m.r32(ea + 12);
                if (!(ef & team)) continue;
                const s32 dx = m.r32s(ea) - nx, dz = m.r32s(ea + 8) - nz, dy = m.r32s(ea + 4) - ny;
                const s32 man = (dx < 0 ? -dx : dx) + (dz < 0 ? -dz : dz) + (dy < 0 ? -dy : dy);
                if (wrap::add(m.r32s(radii + 8 * k), reach) < man) continue;
                const s32 distanceSquared = wrap::add(wrap::add(wrap::mul(dx, dx), wrap::mul(dy, dy)), wrap::mul(dz, dz));
                if (wrap::add(m.r32s(radii + 8 * k + 4), reach9) < distanceSquared) continue;
                const u32 e = slot(ef & 0x3Fu);
                if (ef & 0x1800u) {
                    m.w32(models + 8 * n_models, b);
                    m.w32(models + 8 * n_models + 4, e);
                    if (n_models < 8) ++n_models;
                    continue;
                }
                const u32 fl = m.r32(b);
                static constexpr u32 kDamage[6][2] = {{0x400u, 1}, {0x800u, 2}, {0x1000u, 4}, {0x2000u, 8}, {0x4000u, 0x10}, {0x8000u, 0x20}};
                for (const auto& [bit, dmg] : kDamage) {
                    if (fl & bit) {
                        m.w32(e + 0x54, m.r32(e + 0x54) | dmg);
                        break;
                    }
                }
                m.w32(e + 0x50, m.r32(b + 0x38));
                m.w32(b + 8, static_cast<u32>(nx) << 12);
                m.w32(b + 0xC, static_cast<u32>(ny) << 12);
                m.w32(b + 0x10, static_cast<u32>(nz) << 12);
                m.w32(b + 4, 0);
                u32 id = 0x1E, shift = 2;
                if (k == 0) {
                    id = (m.r32(b) & 0xC0000u) ? 0x2Au : 0x1Fu;
                } else if (m.r32(b) & 0x40000u) {
                    id = 0x28;
                } else if (m.r32(b) & 0x80000u) {
                    id = 0x29;
                    shift = 1;
                }
                sound_at_repeat(id, b + 8, shift);
                hit_sparks_recorded(b + 8, effect_kind(m.r32(b)));
            }
        }
    }

    for (u32 k = 0; k < n_models; ++k) {
        const u32 e = m.r32(models + 8 * k + 4), b = m.r32(models + 8 * k);
        s32 r;
        if ((m.r32(e) & 0x1020u) == 0x1020u && !(m.r8(e + 5) & 2u)) {
            m.w32(f.at(0x10), g::kScratch);
            r = model_raycast(b + 8, b + 0x28, e, hit, g::kScratch, s4);
        } else {
            m.w32(hit, m.r32(b + 8));
            m.w32(hit + 4, m.r32(b + 0xC));
            m.w32(hit + 8, m.r32(b + 0x10));
            r = 0;
        }
        if (0x7FFFFFFE < r) continue;
        const u32 fl = m.r32(b);
        if (fl & 0x400u) m.w32(e + 0x54, m.r32(e + 0x54) | 1u);
        else if (fl & 0x800u) m.w32(e + 0x54, m.r32(e + 0x54) | 2u);
        else if (fl & 0x1000u) m.w32(e + 0x54, m.r32(e + 0x54) | 4u);
        m.w32(e + 0x50, m.r32(b + 0x38));
        m.w32(b + 4, 0);
        u32 id, shift;
        surface_sound(m.r32(b), m.r32(kHitFlags), id, shift);
        sound_at_repeat(id, hit, shift);
        ground_hit_recorded(hit, m.r32(kHitFlags), effect_kind(m.r32(b)));
    }

    const u32 world = m.r32(g::kWorld);
    const u32 bb = world + 0x2A0, env = world + 0x244;
    const u32 masks2 = weapons_base(m) + kBulletMasks;
    const u32 grid = m.r32(env + 4);
    auto cell = [](s32 v) {
        s32 c = (v >> 23) + 0x80;
        if (c < 0) c &= 7;
        if (c >= 0x100) c = (c & 7) + 0xF8;
        return c;
    };
    for (u32 grp = 0; grp < 128; grp += 32) {
        const u32 mask = m.r32(masks2 + grp / 8);
        if (!mask) continue;
        for (u32 i = 0; i < 32; ++i) {
            if (!(mask & wrap::shl(1u, i))) continue;
            const u32 b = weapons_base(m) + kBullets + 60 * (grp + i);
            const s32 x = m.r32s(b + 8), y = m.r32s(b + 0xC), z = m.r32s(b + 0x10);
            s32 tx = wrap::add(m.r32s(b + 0x28), x), ty = wrap::add(m.r32s(b + 0x2C), y), tz = wrap::add(m.r32s(b + 0x30), z);
            u32 side = 0;
            if (tx < m.r32s(bb)) { side = 4; tx = m.r32s(bb); }
            if (m.r32s(bb + 4) < tx) { side = 4; tx = m.r32s(bb + 4); }
            if (ty < m.r32s(bb + 8)) { side = 3; ty = m.r32s(bb + 8); }
            if (m.r32s(bb + 12) < ty) { side = 1; ty = m.r32s(bb + 12); }
            if (tz < m.r32s(bb + 16)) { side = 4; tz = m.r32s(bb + 16); }
            if (m.r32s(bb + 20) < tz) { side = 4; tz = m.r32s(bb + 20); }
            if (side) {
                m.w32(b + 4, 0);
                m.w32(at, static_cast<u32>(tx));
                m.w32(at + 4, static_cast<u32>(ty));
                m.w32(at + 8, static_cast<u32>(tz));
                m.w32(kHitFlags, side);
                const u32 fl = m.r32(b);
                u32 id = 0x1E, shift = 2;
                if (fl & 0x20000u) {
                    if ((side & 0xFu) == 1) id = 0x23;
                } else if (fl & 0x40000u) {
                    id = 0x28;
                } else if (fl & 0x80000u) {
                    id = 0x29;
                    shift = 1;
                }
                sound_at_repeat(id, at, shift);
                ground_hit_recorded(at, m.r32(kHitFlags), effect_kind(m.r32(b)));
                continue;
            }
            const s32 c0 = cell(x) + (cell(z) << 8), c1 = cell(tx) + (cell(tz) << 8);
            m.w32(hit, static_cast<u32>(tx));
            m.w32(hit + 4, static_cast<u32>(ty));
            m.w32(hit + 8, static_cast<u32>(tz));
            s32 r;
            if (c0 == c1) {
                const u32 tile = m.r16(grid + 2u * static_cast<u32>(c0)) & 0x1FFu;
                if (!tile) continue;
                const s32 top = m.r16s(m.r32(m.r32(env + 8) + 4 * tile) + 0x16);
                if ((y >> 12) < top && (ty >> 12) < top) continue;
                r = ray_cell(b + 8, hit, at, g::kScratch);
            } else {
                r = terrain_raycast(b + 8, hit, at, g::kScratch);
            }
            if (0x7FFFFFFE < r) continue;
            m.w32(b + 4, 0);
            u32 id, shift;
            surface_sound(m.r32(b), m.r32(kHitFlags), id, shift);
            sound_at_repeat(id, at, shift);
            ground_hit_recorded(at, m.r32(kHitFlags), effect_kind(m.r32(b)));
        }
    }

    gte.mtc2(8, 0xFA0);
    u32 n_expired = 0;
    const u32 masks3 = weapons_base(m) + kBulletMasks;
    for (u32 grp = 0; grp < 128; grp += 32) {
        const u32 mask_at = masks3 + grp / 8;
        u32 mask = m.r32(mask_at);
        if (!mask) continue;
        for (u32 i = 0; i < 32; ++i) {
            const u32 bit = wrap::shl(1u, i);
            if (!(mask & bit)) continue;
            const u32 idx = grp + i;
            const u32 b = weapons_base(m) + kBullets + 60 * idx;
            const s32 life = wrap::sub(m.r32s(b + 4), 1);
            m.w32(b + 4, static_cast<u32>(life));
            if (life < 0) {
                mask &= ~bit;
                m.w8(expired + n_expired, idx);
                ++n_expired;
                continue;
            }
            s32 vx = m.r32s(b + 0x28), vy = m.r32s(b + 0x2C), vz = m.r32s(b + 0x30);
            m.w32(b + 8, m.r32(b + 8) + static_cast<u32>(vx));
            m.w32(b + 0x10, m.r32(b + 0x10) + static_cast<u32>(vz));
            m.w32(b + 0xC, m.r32(b + 0xC) + static_cast<u32>(vy));
            m.w32(b + 0x1C, m.r32(b + 0x1C) + static_cast<u32>(vy));
            m.w32(b + 0x18, m.r32(b + 0x18) + static_cast<u32>(vx));
            m.w32(b + 0x20, m.r32(b + 0x20) + static_cast<u32>(vz));
            const u32 nf = ~m.r32(b);
            if (nf & 2u) vy = wrap::add(vy, 0x444);
            vx >>= 6;
            vy >>= 6;
            vz >>= 6;
            if (nf & 1u) {
                gte.mtc2(9, static_cast<u32>(vx));
                gte.mtc2(10, static_cast<u32>(vy));
                gte.mtc2(11, static_cast<u32>(vz));
                gte.op(0x198003D);
                vx = static_cast<s32>(gte.mfc2(25));
                vy = static_cast<s32>(gte.mfc2(26));
                vz = static_cast<s32>(gte.mfc2(27));
            }
            m.w32(b + 0x28, static_cast<u32>(vx) << 6);
            m.w32(b + 0x2C, static_cast<u32>(vy) << 6);
            m.w32(b + 0x30, static_cast<u32>(vz) << 6);
        }
        m.w32(mask_at, mask);
    }
    for (u32 k = 0; k < n_expired; ++k) {
        const u32 idx = m.r8(expired + k);
        if (m.r32(weapons_base(m) + 60 * idx + 0x1608) & 0x2000u) record_weapon(1, idx, 0, 0, 0);
    }
}
}

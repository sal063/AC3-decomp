#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kAlertFlag = 0x800C6ECCu;
constexpr u32 kArea = 0x800BCEB0u;
constexpr u32 kRoutes = 0x800BCF48u;
constexpr u32 kFormationCount = 0x800BCEDEu;
constexpr u32 kFormationDefs = 0x800BCEECu;
constexpr u32 kFormationState = 0x800BCEF4u;
constexpr u32 kFormationSlots = 0x800B1880u;
constexpr u32 kFormationOffsets = 0x800B17F0u;
constexpr u32 kPaths = 0x800C7110u;
constexpr u32 kTracks = 0x800C6E90u;
constexpr u32 kHudMessage = 0x800BCFD8u;
constexpr u32 kSpawnX = 0x800B2048u;
constexpr u32 kSpawnZ = 0x800B2060u;
constexpr u32 kBreakTurn = 0x800B2028u;
constexpr u32 kBreakShift = 0x800B2038u;

u32 ai_rec(Memory& m, u32 i) { return m.r32(g::kAiInfo) + 184 * i; }
u32 srec(Memory& m, u32 i) { return m.r32(g::kSlotRecords) + 64 * i; }
u32 sflag(u32 i) { return g::kSlotFlags + 4 * i; }
void spill(Memory& m, u32 at, u32 value) { m.w32(at, value); }
bool flying(Memory& m, u32 e) { return (~m.r32(e) & 0xC000u) == 0; }
s32 mission(Memory& m) { return m.r16s(m.r32(g::kScene) + 908); }
u32 tick(Memory& m) { return m.r32(m.r32(g::kWorld) + 12); }
s32 absv(s32 v) { return v < 0 ? -v : v; }
s32 sin_at(Memory& m, s32 i) { return m.r16s(g::kSinCos + 2 * static_cast<u32>(i & 0x3FFF)); }
s32 cos_at(Memory& m, s32 i) { return m.r16s(g::kSinCos + 0x2000 + 2 * static_cast<u32>(i & 0x3FFF)); }
bool sea_mission(s32 mi) { return mi == 32 || mi == 47 || mi == 53; }
void set_bits(Memory& m, u32 a, u32 bits) { m.w32(a, m.r32(a) | bits); }
void clear_bits(Memory& m, u32 a, u32 bits) { m.w32(a, m.r32(a) & ~bits); }
void copy_words(Memory& m, u32 to, u32 from, u32 n) {
    for (u32 k = 0; k < n; ++k) m.w32(to + 4 * k, m.r32(from + 4 * k));
}
}

void Game::sim_step3() {
    cover(0x80038A3Cu);
    Frame f(*this, 24);
    ai_flags_update();
    ai_update();
    ai_move();
}

void Game::ai_flags_update() {
    cover(0x80038100u);
    for (u32 i = 2; i < 32; ++i) {
        const u32 e = slot(i);
        const u32 w = m.r32(e) & ~0x8000u;
        m.w32(e, w);
        if (~w & 0x4000u) continue;
        const u32 sf = sflag(i);
        const u32 v = m.r32(sf) & ~0x40000u;
        m.w32(sf, v);
        if (m.r16s(e + 78) >= 10000 && (v & 0x800000u) && (m.r32(e) & 0x80u)) m.w32(sf, v | 0x40000u);
        if (m.r32(sf) & 0x400000u) continue;
        if (!(m.r32(sf) & 1u)) {
            if (m.r32(sf) & 2u) set_bits(m, e, 0x8000u);
            continue;
        }
        if ((m.r32(sf) & 0x10u) || (m.r32(m.r32(g::kRelation + 4 * i)) & 0x80u) || m.r16s(slot(i) + 78) <= 0 ||
            !m.r8(ai_rec(m, i) + 96))
            set_bits(m, e, 0x8000u);
        const u32 p = g::kPairs + 4 * i;
        const u32 a = m.r8(p + 2);
        if (a != 255 && (m.r32(slot(a)) & 0x40u)) set_bits(m, e, 0x8000u);
        const u32 b = m.r8(p + 3);
        if (b != 255 && (m.r32(slot(b)) & 0x40u)) set_bits(m, e, 0x8000u);
    }
}

void Game::ai_update() {
    cover(0x8003809Cu);
    Frame f(*this, 24);
    m.w32(kAlertFlag, m.r32(kAlertFlag) & ~1u);
    ai_deaths();
    ai_routes();
    ai_goals();
    ai_behaviours();
    ai_states();
    ai_engine_sounds();
    radio_message_update();
}

void Game::ai_move() {
    cover(0x800383ACu);
    Frame f(*this, 40);
    for (u32 i = 2; i < 32; ++i) {
        const u32 e = slot(i);
        if (!flying(m, e)) continue;
        if (m.r32(e + 84) & 0x800000u) attitude_integrate(e + 24, e + 48);
        if (m.r32(e + 84) & 0x400000u) rotate_by_angles(e + 32, e + 48, e + 56);
        if (m.r32(e + 84) & 0x200000u) rotate_by_angles(e + 32, e + 24, e + 56);
        u32 q = m.r32(e + 84);
        if (static_cast<s32>(q) < 0) {
            m.w32(e + 8, m.r32(e + 8) + m.r32(e + 56));
            m.w32(e + 12, m.r32(e + 12) + m.r32(e + 60));
            m.w32(e + 16, m.r32(e + 16) + m.r32(e + 64));
            q = m.r32(e + 84);
        }
        m.w32(e + 84, q & 0xFF000000u);
    }
    if (m.r32(m.r32(g::kWorld) + 4) == 4) ai_weapons();
    const s32 mi = mission(m);
    if (sea_mission(mi)) {
        for (u32 i = 2; i < 32; ++i) {
            const u32 e = slot(i);
            if (!flying(m, e)) continue;
            if (m.r32s(e + 12) < -11878400) m.w32(e + 12, static_cast<u32>(-11878400));
            if (m.r32s(e + 12) > -409600) m.w32(e + 12, static_cast<u32>(-409600));
        }
    }
    if (mi == 6) {
        for (u32 i = 2; i < 32; ++i) {
            const u32 e = slot(i);
            if (flying(m, e)) water_wake(e + 8, e + 56, -m.r32s(m.r32(kArea) + 4));
        }
    }
    const u32 tk = m.r8(m.r32(g::kWorld) + 12);
    if ((mi == 17 || mi == 31 || mi == 38 || mi == 46) && m.r8(m.r32(g::kMission) + 60) >= 3) {
        for (u32 i = 2; i < 32; ++i) {
            const u32 e = slot(i);
            if (flying(m, e) && m.r16(e + 72) == 45 && (m.r32(sflag(i)) & 0x200000u)) {
                smoke(e + 8, e + 56, tk + i, 3);
                break;
            }
        }
    }
    if (static_cast<u32>(mi - 31) < 3 || mi == 38 || mi == 46 || mi == 47 || mi == 48 || mi == 53) {
        for (u32 i = 2; i < 32; ++i) {
            const u32 e = slot(i);
            if (!flying(m, e)) continue;
            s32 kind = m.r16s(e + 72);
            if (kind == 26) {
                const s32 hp = m.r16s(e + 78);
                if (((mi == 33 || mi == 48 || mi == 53) && hp < 201 && hp > 0) ||
                    (mi == 38 && m.r32(m.r32(g::kWorld) + 8) == 1)) {
                    smoke(e + 8, e + 56, tk + i, 2);
                    m.w8(e + 5, m.r8(e + 5) & ~8u);
                } else if (!(m.r8(m.r32(g::kMission)) & 2u) && !(mi == 38 && m.r32s(m.r32(g::kWorld) + 8) > 0) &&
                           !(mi == 48 && m.r32s(m.r32(g::kWorld) + 8) >= 2) && (m.r8(e + 5) & 8u)) {
                    puff_spawn_recorded(e + 8, e + 56, 2);
                    m.w8(e + 5, m.r8(e + 5) & ~8u);
                }
                kind = m.r16s(e + 72);
            }
            if (kind == 27 && (mi == 33 || mi == 46 || mi == 53)) {
                const s32 hp = m.r16s(e + 78);
                if (hp < 201 && hp > 0) {
                    smoke(e + 8, e + 56, tk + i, 2);
                    m.w8(e + 5, m.r8(e + 5) & ~8u);
                }
            }
        }
    }
    if (mi == 34 || mi == 54) {
        for (u32 i = 2; i < 32; ++i) {
            const u32 e = slot(i);
            if (flying(m, e) && m.r16(e + 72) == 25 && (m.r8(e + 5) & 8u)) {
                puff_spawn_recorded(e + 8, e + 56, 3);
                m.w8(e + 5, m.r8(e + 5) & ~8u);
            }
        }
    }
    if (mi == 27) {
        for (u32 i = 2; i < 32; ++i) {
            const u32 e = slot(i);
            const u32 sf = m.r32(sflag(i));
            if (flying(m, e) && (sf & 1u) && m.r16(e + 72) == 39 && (sf & 0x200000u)) spark_shower(e + 8, e + 56);
        }
    }
    if (mi == 58) {
        for (u32 i = 2; i < 32; ++i) {
            const u32 e = slot(i);
            if (flying(m, e)) puff_spawn_recorded(e + 8, e + 56, 4);
        }
    }
}

void Game::ai_deaths() {
    cover(0x80046600u);
    Frame f(*this, 40);
    for (u32 i = 2; i < 32; ++i) {
        const u32 e = slot(i);
        const u32 sf = sflag(i);
        if (!(m.r32(sf) & 1u) || m.r16s(e + 78) > 0) continue;
        const u32 msn = m.r32(g::kMission);
        if (m.r32(sf) & 0x100u) m.w8(msn + 32, m.r8(msn + 32) + 1);
        if (m.r32(sf) & 0x200u) m.w8(msn + 33, m.r8(msn + 33) + 1);
        if (m.r32(sf) & 0x80000u) {
            spill(m, f.at(0) - 80 + 0x38, reg_s4);
            msn_vm_exec(m.r32(m.r32(g::kMissionData) + 340));
            m.w16(e + 78, m.r16(m.r32(srec(m, i) + 16)));
            const u32 v = m.r32(sf) & ~0x80000u;
            m.w32(sf, v);
            const s32 mi = mission(m);
            if (m.r16(e + 72) == 27 && (mi == 33 || mi == 58)) {
                m.w32(sf, v | 0x800u);
                mission_log_kill(i);
            }
            continue;
        }
        if ((m.r32(sf) & 0x800u) && !(m.r32(e + 80) & 0x3Fu) && !(m.r32(e) & 0x40u) && m.r16(e + 72) != 44) {
            if (m.r8(m.r32(g::kScene) + 925) & 1u) kill_camera(m.r32(e));
            if (m.r32(e) & 0x80u) message_show((tick(m) & 7u) ? 7 : 6);
        }
        mission_log_kill(i);
        ai_slot_dead(i);
        if (const u32 script = m.r32(srec(m, i) + 52)) {
            spill(m, f.at(0) - 80 + 0x38, reg_s4);
            msn_vm_exec(script);
        }
    }
}

void Game::ai_slot_dead(u32 i) {
    cover(0x800382D4u);
    m.w32(sflag(i), (m.r32(sflag(i)) & 0xFFEF8FFCu) | 2u);
}

void Game::ai_routes() {
    cover(0x80046854u);
    Frame f(*this, 40);
    for (u32 i = 2; i < 32; ++i) {
        const u32 a = ai_rec(m, i);
        if (flying(m, slot(i)) && m.r8(a + 28) == 255 && !route_next(i)) {
            m.w8(a + 61, 255);
            m.w8(a + 60, 255);
            m.w8(a + 99, 255);
            m.w8(a + 98, 255);
        }
    }
    if (m.r32(m.r32(g::kWorld) + 4) & 0xEu) formations_update();
    link_flags_update();
}

u32 Game::route_next(u32 i) {
    cover(0x80046928u);
    const u32 r = srec(m, i);
    const s32 n = m.r32s(r + 36);
    if (n >= 7) return 1;
    const u32 k = static_cast<u32>(n + 1);
    const u32 b = m.r8(r + k + 40);
    if (b == 255) return 1;
    m.w32(r + 36, k);
    const u32 ent = m.r32(kRoutes) + 32 * b;
    m.w32(r + 48, ent);
    const u32 a = ai_rec(m, i);
    const u32 kind = m.r8(ent);
    m.w8(a + 29, 0);
    m.w8(a + 30, 0);
    m.w8(a + 28, kind);
    m.w8(a + 31, m.r8(ent + 1));
    copy_words(m, a + 32, ent + 8, 4);
    m.w32(a + 48, m.r32(ent + 24));
    m.w32(a + 52, m.r32(ent + 28));
    return 0;
}

void Game::formations_update() {
    cover(0x8004BA0Cu);
    Frame f(*this, 32);
    for (u32 k = 0; k < m.r8(kFormationCount); ++k)
        if (m.r32(m.r32(kFormationState) + 12 * k) & 1u) formation_update(k);
}

void Game::formation_update(u32 k) {
    cover(0x8004BA7Cu);
    Frame f(*this, 48);
    const u32 def = m.r32(kFormationDefs) + 12 * k;
    const u32 st = m.r32(kFormationState) + 12 * k;
    const u32 members = f.at(0x10);
    u32 n = 0;
    for (u32 j = 0; j < m.r16(def + 2); ++j) {
        const u32 s = m.r8(def + 4 + j);
        if (!(m.r32(sflag(s)) & 1u)) continue;
        if (n) {
            set_bits(m, sflag(s), 0x100000u);
        } else {
            m.w8(st + 6, s);
            clear_bits(m, sflag(m.r8(def + 4 + j)), 0x100000u);
        }
        m.w8(members + n, m.r8(def + 4 + j));
        ++n;
        if (m.r8(st + 7) != 1 && (m.r32(m.r32(g::kWeapons)) & (1u << (m.r8(def + 4 + j) & 31u)))) {
            m.w8(st + 7, 1);
            m.w32(st + 8, 0);
        }
    }
    m.w16(st + 4, n);
    if ((n & 0xFFFFu) < 2) formation_break(k);
    const u32 rel = m.r32(g::kRelation + 4 * m.r8(st + 6));
    if (!rel) return;
    if (m.r8(st + 7) != 1 && (m.r32(rel) & 0x20u)) {
        m.w8(st + 7, 1);
        m.w32(st + 8, 0);
    }
    for (u32 j = 1; j < m.r16(st + 4); ++j) {
        const u32 a = ai_rec(m, m.r8(members + j));
        if (m.r8(st + 7) == 1) {
            if (!m.r32(st + 8)) {
                const s32 off = m.r32s(a + 68);
                m.w8(a + 99, 12);
                m.w8(a + 61, 23);
                m.w8(a + 97, 0);
                m.w32(a + 120, 4096);
                m.w16(a + 124, static_cast<u32>(off >> 7));
            }
            if (m.r32(a) & 0x400000u) {
                formation_break(k);
                m.w8(a + 61, 255);
                m.w8(a + 99, 255);
            }
        } else {
            m.w8(a + 61, 3);
            m.w8(a + 63, m.r8(st + 6));
            formation_slot(m.r16(st + 4), j, a + 68);
        }
    }
    m.w32(st + 8, m.r32(st + 8) + 1);
}

void Game::formation_slot(u32 n, u32 j, u32 out) {
    cover(0x8004BD18u);
    const u32 src = kFormationOffsets + 16 * m.r8(kFormationSlots + 8 * n - 16 + j);
    copy_words(m, out, src, 4);
}

void Game::formation_break(u32 k) {
    cover(0x8004BDA8u);
    const u32 def = m.r32(kFormationDefs) + 12 * k;
    clear_bits(m, m.r32(kFormationState) + 12 * k, 1u);
    for (u32 j = 0; j < m.r16(def + 2); ++j) clear_bits(m, sflag(m.r8(def + 4 + j)), 0x100000u);
}

void Game::link_flags_update() {
    cover(0x8004BE34u);
    const u32 n = m.r8(g::kLinkCount);
    for (u32 k = 0; k < n; ++k) {
        if (!(m.r32(m.r32(g::kLinkFlags) + 4 * k) & 1u)) continue;
        const u32 link = m.r32(g::kLinks) + 20 * k;
        set_bits(m, sflag(m.r8(link + 2)), 0x200000u);
        set_bits(m, sflag(m.r8(link + 3)), 0x400000u);
    }
}

void Game::ai_goals() {
    cover(0x800469ECu);
    Frame f(*this, 56);
    for (u32 i = 2; i < 32; ++i) {
        const u32 e = slot(i);
        const u32 a = ai_rec(m, i);
        const u32 sf = sflag(i);
        if (!flying(m, e)) continue;
        const u32 v = m.r32(sf);
        if (v & 2u) {
            m.w32(sf, v & ~0x100000u);
            m.w8(a + 61, 12);
            if (m.r8(a + 28) == 6) m.w8(a + 28, 0);
            continue;
        }
        switch (m.r8(a + 28)) {
        case 5: m.w32(sf, v & ~0x100000u); ai_follow_path(i); continue;
        case 6: m.w32(sf, v & ~0x100000u); ai_follow_track(i); continue;
        case 7: ai_special(i); continue;
        case 8: ai_wait_end(i); continue;
        default: break;
        }
        if (v & 0x100000u) continue;
        ai_check_home(i);
        if (m.r8(a + 60) == 11) continue;
        if (ai_out_of_area(i)) {
            m.w8(a + 61, 11);
            const u32 t = pair_drop_target(i);
            m.w8(a + 96, t);
            m.w8(a + 63, m.r8(a + 31));
            copy_words(m, a + 68, a + 32, 4);
            continue;
        }
        if (m.r8(a + 60) == 10) continue;
        if (ai_near_ceiling(i) || ai_near_floor(i)) {
            m.w8(a + 61, 10);
            const u32 t = pair_drop_target(i);
            m.w8(a + 96, t);
            m.w8(a + 63, m.r8(a + 31));
            continue;
        }
        const u32 b = m.r8(a + 60);
        if (b == 7) continue;
        if (m.r32(m.r32(g::kWeapons)) & (1u << i)) {
            m.w8(a + 61, 7);
            m.w8(a + 96, pair_drop_target(i));
            continue;
        }
        if (b == 5) continue;
        if ((m.r32(e) & 0x1C0u) != 0x40u && (m.r32(m.r32(g::kRelation + 4 * i)) & 0x2000000u) &&
            m.r32(m.r32(g::kScene) + 916) != 3) {
            m.w8(a + 61, 5);
            m.w8(a + 96, pair_drop_target(i));
            continue;
        }
        switch (m.r8(a + 28)) {
        case 2: case 4: ai_goal_escort(i); break;
        case 9: case 10: ai_goal_chase(i); break;
        default: ai_goal_default(i); break;
        }
        if (m.r8(a + 96) == 255 || static_cast<u32>(m.r8(a + 28) - 9) < 2) continue;
        if (m.r16s(m.r32(srec(m, i) + 24) + 20) < m.r16s(a + 182)) {
            m.w8(a + 96, pair_drop_target(i));
            continue;
        }
        const u32 t = m.r8(a + 96);
        if (!t || (m.r32(sflag(t)) & 1u)) {
            if (m.r8(g::kPairs + 4 * i + 1) == 255)
                m.w8(a + 96, 255);
            else
                set_bits(m, sflag(i), 0x10000u);
        } else {
            m.w8(a + 96, 255);
        }
    }
}

void Game::ai_check_home(u32 i) {
    cover(0x8004781Cu);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    const u32 kind = m.r8(a + 28);
    set_bits(m, a, 0x3000000u);
    const u32 c = kind == 2 || kind == 4 || kind == 9 || kind == 10 ? slot(m.r8(a + 31)) + 8 : a + 32;
    const s32 cx = m.r32s(c), cy = m.r32s(c + 4), cz = m.r32s(c + 8);
    const s32 x = m.r32s(e + 8), y = m.r32s(e + 12), z = m.r32s(e + 16);
    const s32 r = m.r32s(a + 48);
    if (cx + r >= x && x >= cx - r && cz + r >= z && z >= cz - r) clear_bits(m, a, 0x1000000u);
    const u32 k2 = m.r8(a + 28);
    const s32 r2 = m.r32s(a + 52);
    if (cx + r2 < x || x < cx - r2 || cz + r2 < z || z < cz - r2) return;
    if ((k2 == 2 || k2 == 4 || k2 == 9 || k2 == 10) && (cy + 0x800000 < y || y < cy - 0x800000)) return;
    clear_bits(m, a, 0x2000000u);
}

u32 Game::ai_out_of_area(u32 i) {
    cover(0x800476F0u);
    const u32 e = slot(i);
    const u32 area = m.r32(kArea);
    const s32 x0 = m.r32s(area + 16), x1 = m.r32s(area + 20), z0 = m.r32s(area + 24), z1 = m.r32s(area + 28);
    const s32 x = m.r32s(e + 8), z = m.r32s(e + 16);
    if (x < x0 || x1 < x || z < z0 || z1 < z) return 2;
    const s32 nx = x + (m.r32s(e + 56) << 8), nz = z + (m.r32s(e + 64) << 8);
    if (nx < x0 || x1 < nx || nz < z0 || z1 < nz) return 1;
    const s32 ny = m.r32s(e + 12) + (m.r32s(e + 60) << 8);
    if (m.r16(m.r32(g::kScene) + 908) == 42 && (ny > 61440000 || ny < -245760000)) return 1;
    if (m.r16(m.r32(g::kScene) + 908) == 58) return ny > -3145728 ? 1 : 0;
    return 0;
}

u32 Game::ai_near_ceiling(u32 i) {
    cover(0x800475B0u);
    const u32 e = slot(i);
    const u32 r = srec(m, i);
    const s32 y = m.r32s(e + 12);
    const s32 top = m.r32s(m.r32(r + 24) + 4);
    const u32 fp = m.r32(r + 20);
    if (y < top) return 2;
    const s32 vy = m.r32s(e + 60);
    if (vy >= 0) return 0;
    const s32 t = wrap::div(top - y, vy).lo;
    if (t < 0) return 2;
    const s32 roll = absv(wrap::div(m.r16s(e + 28), m.r16s(fp + 24) >> 1).lo);
    s32 pitch = wrap::div(-m.r16s(e + 24), m.r16s(fp + 28) >> 1).lo;
    if (pitch < 0) pitch = 0;
    return roll + 2 + pitch >= t ? 1 : 0;
}

u32 Game::ai_near_floor(u32 i) {
    cover(0x80047470u);
    const u32 e = slot(i);
    const u32 r = srec(m, i);
    const s32 y = m.r32s(e + 12);
    const s32 bottom = m.r32s(m.r32(r + 24) + 8);
    const u32 fp = m.r32(r + 20);
    if (bottom < y) return 2;
    const s32 vy = m.r32s(e + 60);
    if (vy <= 0) return 0;
    const s32 t = wrap::div(bottom - y, vy).lo;
    if (t < 0) return 2;
    const s32 roll = absv(wrap::div(m.r16s(e + 28), m.r16s(fp + 24) >> 1).lo);
    s32 pitch = wrap::div(-m.r16s(e + 24), m.r16s(fp + 26) >> 1).lo;
    if (pitch < 0) pitch = 0;
    return roll + 2 + pitch >= t ? 1 : 0;
}

void Game::ai_goal_default(u32 i) {
    cover(0x80050720u);
    Frame f(*this, 32);
    spill(m, f.at(0x18), 0x80046CDCu);
    const u32 a = ai_rec(m, i);
    const u32 range = m.r32(srec(m, i) + 28);
    if (m.r8(a + 96) != 255) {
        const u32 b = m.r8(a + 60);
        if (!b) return;
        if (b == 1) {
            if (!(m.r32(a) & 0x1000000u)) {
                if (m.r32(a + 160) < m.r32(range + 8)) m.w8(a + 61, 4);
                return;
            }
        } else if (b == 4 && !(m.r32(a) & 0x1000000u)) {
            if (m.r32(range + 8) < m.r32(a + 160)) m.w8(a + 61, 1);
            return;
        }
        m.w8(a + 96, pair_drop_target(i));
        m.w8(a + 61, 0);
        m.w8(a + 63, 255);
        copy_words(m, a + 68, a + 32, 4);
        return;
    }
    m.w8(a + 61, 0);
    m.w8(a + 63, 255);
    copy_words(m, a + 68, a + 32, 4);
    if (m.r32(a) & 0x2000000u) return;
    const u32 t = ground_pick_target(i) & 0xFFu;
    m.w8(a + 96, t);
    if (t == 255) return;
    m.w8(a + 61, 1);
    m.w32(a + 76, 0);
    m.w32(a + 72, 0);
    m.w32(a + 68, 0);
    m.w8(a + 63, m.r8(a + 96));
}

void Game::ai_goal_chase(u32 i) {
    cover(0x80050BB4u);
    const u32 a = ai_rec(m, i);
    const u32 t = m.r8(a + 31);
    if (m.r16s(slot(t) + 78) <= 0) {
        m.w8(a + 28, 255);
        return;
    }
    m.w8(a + 63, t);
    m.w8(a + 96, m.r8(a + 31));
    set_bits(m, sflag(i), 0x10000u);
    auto weave = [&] {
        m.w8(a + 61, 1);
        m.w32(a + 68, 0);
        m.w32(a + 72, 0);
        m.w8(a + 63, m.r8(a + 31));
        u32 z;
        if (m.r8(a + 29)) {
            m.w8(a + 29, 0);
            z = (2 * tick(m) - 20000) << 12;
        } else {
            m.w8(a + 29, 1);
            z = tick(m) << 13;
        }
        m.w32(a + 76, z);
    };
    const u32 n = m.r32(a + 56);
    if (!n) {
        weave();
        m.w32(a + 56, m.r32(a + 56) + 1);
        return;
    }
    if (static_cast<s32>(n) == m.r16s(m.r32(srec(m, i) + 24) + 20)) {
        m.w8(a + 61, 0);
        copy_words(m, a + 68, slot(0) + 8, 4);
        m.w32(a + 56, m.r32(a + 56) + 1);
        return;
    }
    const u32 b = m.r8(a + 60);
    const u32 range = m.r32(srec(m, i) + 28);
    if (b == 1) {
        if (m.r32(a + 160) < m.r32(range + 8)) m.w8(a + 61, 4);
        m.w32(a + 56, m.r32(a + 56) + 1);
        return;
    }
    if (b == 4) {
        if (m.r32(range + 8) < m.r32(a + 160)) m.w8(a + 61, 1);
        m.w32(a + 56, m.r32(a + 56) + 1);
        return;
    }
    if (b) {
        weave();
        m.w32(a + 56, 0);
    }
}

void Game::ai_goal_escort(u32 i) {
    cover(0x800508D0u);
    Frame f(*this, 40);
    spill(m, f.at(0x20), 0x80046CBCu);
    const u32 a = ai_rec(m, i);
    const u32 t = m.r8(a + 31);
    if (m.r16s(slot(t) + 78) <= 0) {
        m.w8(a + 28, 255);
        return;
    }
    if (m.r8(a + 28) == 4 && m.r8(a + 60) == 2 && (m.r32(a) & 0x2000000u) && m.r8(a + 63) == t) return;
    if (!(m.r32(a) & 0x1000000u)) {
        if (m.r8(a + 96) == 255) {
            if (m.r8(a + 28) == 2) m.w8(a + 96, pair_add(i, m.r8(a + 31)));
            bool have = m.r8(a + 96) != 255;
            if (!have) {
                m.w8(a + 96, ground_pick_target(i));
                have = m.r8(a + 96) != 255;
            }
            if (have) {
                m.w8(a + 61, 1);
                m.w32(a + 68, 0);
                m.w32(a + 72, 0);
                m.w32(a + 76, 0);
                m.w8(a + 63, m.r8(a + 96));
                return;
            }
            escort_position(i);
            return;
        }
        const u32 b = m.r8(a + 60);
        const u32 range = m.r32(srec(m, i) + 28);
        if (b == 1) {
            if (m.r32(a + 160) < m.r32(range + 8)) m.w8(a + 61, 4);
            return;
        }
        if (b == 4) {
            if (m.r32(range + 8) < m.r32(a + 160)) m.w8(a + 61, 1);
            return;
        }
        if (b == 2) return;
    }
    m.w8(a + 96, pair_drop_target(i));
    escort_position(i);
}

void Game::escort_position(u32 i) {
    cover(0x80050AC0u);
    Frame f(*this, 32);
    const u32 a = ai_rec(m, i);
    m.w8(a + 63, m.r8(a + 31));
    if (m.r8(a + 61) == 2) return;
    const u32 kind = m.r8(a + 28);
    m.w8(a + 61, 2);
    if (kind == 2) {
        m.w32(a + 68, 0);
        m.w32(a + 72, static_cast<u32>(((rand() & 0x1FF) - 256) << 7));
        m.w32(a + 76, 0x1000000);
    } else if (kind == 4) {
        const s32 k = static_cast<s32>((i & 0xFu) << 10);
        m.w32(a + 68, static_cast<u32>(sin_at(m, k) << 7));
        const s32 r = static_cast<s16>(rand());
        const s32 c = cos_at(m, k);
        m.w32(a + 72, static_cast<u32>(((r & 0x1FF) - 256) << 7));
        m.w32(a + 76, static_cast<u32>(c << 7));
    }
}

void Game::ai_follow_path(u32 i) {
    cover(0x80050DE8u);
    Frame f(*this, 24);
    const u32 a = ai_rec(m, i);
    spill(m, f.at(0x10), a);
    const u32 path = m.r32(kPaths + 4 * m.r8(m.r32(srec(m, i) + 48) + 2));
    if (!m.r8(a + 29)) {
        m.w32(a + 56, 0);
        m.w8(a + 29, 1);
        path_point(a, path + 32 * m.r32(a + 56));
        return;
    }
    if (m.r16(slot(i) + 72) == 44 && m.r16s(slot(0) + 78) < 0) {
        m.w8(a + 99, 50);
        m.w8(a + 97, 9);
        return;
    }
    const u32 pt = path + 32 * m.r32(a + 56);
    if (!(m.r32(a) & 0x400000u)) {
        const u32 c = m.r8(pt + 3);
        if (c == 1) {
            if (m.r32s(a + 136) < m.r32s(pt + 4)) set_bits(m, a, 0x400000u);
        } else if (c == 2 && m.r16s(a + 102) >= m.r32s(pt + 4)) {
            set_bits(m, a, 0x400000u);
        }
    }
    if (m.r32(a) & 0x400000u) {
        const u32 n = m.r32(a + 56) + 1;
        m.w32(a + 56, n);
        path_point(a, path + 32 * n);
        if (m.r8(a + 99) == 255) {
            m.w8(a + 28, 255);
            return;
        }
        clear_bits(m, a, 0x400000u);
    }
    m.w8(a + 61, 13);
}

void Game::path_point(u32 a, u32 pt) {
    cover(0x80050FA8u);
    m.w8(a + 99, m.r8(pt + 1));
    copy_words(m, a + 104, pt + 8, 4);
    copy_words(m, a + 120, pt + 24, 2);
    m.w8(a + 97, static_cast<u32>(m.r8(a + 99) - 50) < 2 ? 9 : 0);
}

void Game::ai_follow_track(u32 i) {
    cover(0x80051010u);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    if (m.r16(m.r32(g::kScene) + 908) == 3) m.w8(e + 5, m.r8(e + 5) | 8u);
    const s32 n = m.r32s(a + 56);
    const u32 base = m.r32(kTracks + 4 * m.r8(m.r32(srec(m, i) + 48) + 2));
    const s32 fr = n % 8;
    const u32 p0 = base + static_cast<u32>(20 * (n / 8));
    const u32 p1 = p0 + 20;
    if (m.r32(p1) == 0xFFFFFFFFu) {
        m.w8(a + 28, 255);
        return;
    }
    Frame f(*this, 16);
    copy_words(m, f.at(0), e + 8, 4);
    for (u32 k = 0; k < 12; k += 4) {
        const s32 v0 = m.r32s(p0 + k), v1 = m.r32s(p1 + k);
        m.w32(e + 56 + k, static_cast<u32>((v1 - v0) * fr / 8 + v0 - m.r32s(e + 8 + k)));
    }
    s32 dp = m.r16s(p1 + 12) - m.r16s(p0 + 12);
    s32 dy = m.r16s(p1 + 14) - m.r16s(p0 + 14);
    s32 dr = m.r16s(p1 + 16) - m.r16s(p0 + 16);
    if (static_cast<u32>((dy & 0x3FFF) - 7169) < 0x7FFu) {
        dy += dy <= 0 ? 0x2000 : -0x2000;
        dp = 0x2000 - m.r16s(p0 + 12) - m.r16s(p1 + 12);
        dr = m.r16s(p1 + 16) + m.r16s(p0 + 16) - 0x2000;
    }
    auto wrap_half = [](s32 v) {
        if (v >= 0x2000) v -= 0x4000;
        if (v < -8192) v += 0x4000;
        return v;
    };
    dp = wrap_half(dp);
    dy = wrap_half(dy);
    dr = wrap_half(dr);
    m.w16(e + 24, static_cast<u32>(m.r16(p0 + 12) + dp * fr / 8));
    m.w16(e + 26, static_cast<u32>(m.r16(p0 + 14) + dy * fr / 8));
    m.w16(e + 28, static_cast<u32>(m.r16(p0 + 16) + dr * fr / 8));
    m.w8(a + 61, 13);
    m.w8(a + 99, 8);
    m.w8(a + 97, 9);
    m.w32(a + 56, static_cast<u32>(n + 1));
}

void Game::ai_special(u32 i) {
    cover(0x80051304u);
    const u32 a = ai_rec(m, i);
    const u32 k = m.r32(m.r32(srec(m, i) + 20));
    const s32 mi = mission(m);
    switch (k) {
    case 7: m.w8(a + 61, 14); return;
    case 8: m.w8(a + 61, 15); return;
    case 6:
        if (mi == 28 || mi == 36) {
            m.w8(a + 61, mi == 28 ? 16 : 17);
            m.w8(a + 63, m.r8(a + 31));
        }
        return;
    default:
        if (mi == 6 || mi == 55) {
            m.w8(a + 61, 18);
            m.w8(a + 63, m.r8(a + 31));
        }
        return;
    }
}

void Game::ai_wait_end(u32 i) {
    cover(0x800513D8u);
    const u32 a = ai_rec(m, i);
    if (m.r8(a + 61) == 255) m.w8(a + 28, 255);
}

void Game::ai_behaviours() {
    cover(0x80046DFCu);
    Frame f(*this, 40);
    for (u32 i = 2; i < 32; ++i) {
        const u32 a = ai_rec(m, i);
        if (!flying(m, slot(i)) || m.r8(a + 28) == 6) continue;
        const u32 next = m.r8(a + 61);
        if (m.r8(a + 60) != next) {
            m.w8(a + 60, next);
            m.w8(a + 62, 0);
        }
        switch (m.r8(a + 60)) {
        case 0: beh_patrol(i); break;
        case 1: beh_pursue(i); break;
        case 2: beh_station(i); break;
        case 3: beh_formation(i); break;
        case 4: beh_attack(i); break;
        case 5: beh_attack_player(i); break;
        case 7: beh_evade(i); break;
        case 10: beh_level_out(i); break;
        case 11: beh_return(i); break;
        case 12: beh_leave(i); break;
        case 14: beh_dive(i); break;
        case 15: beh_drop(i); break;
        case 16: beh_arrive(i); break;
        case 17: beh_close_in(i); break;
        case 18: beh_ghost(i); break;
        case 19: beh_hold(i); break;
        case 20: beh_timed_exit(i); break;
        case 21: beh_escort_count(i); break;
        case 22: beh_turn_away(i); break;
        default: break;
        }
        if (m.r8(a + 61) == 255) {
            const u32 v = m.r32(a);
            m.w8(a + 60, 255);
            m.w32(a, v | 0x200000u);
        }
    }
}

void Game::goal_clamp(u32 i, u32 p) {
    cover(0x800505DCu);
    Frame f(*this, 24);
    goal_clamp_y(i, p);
    const u32 area = m.r32(kArea);
    s32 lo = m.r32s(area + 16) + 0x4000000;
    if (m.r32s(p) < lo) m.w32(p, static_cast<u32>(lo));
    s32 hi = m.r32s(area + 20) - 0x4000000;
    if (hi < m.r32s(p)) m.w32(p, static_cast<u32>(hi));
    lo = m.r32s(area + 24) + 0x4000000;
    if (m.r32s(p + 8) < lo) m.w32(p + 8, static_cast<u32>(lo));
    hi = m.r32s(area + 28) - 0x4000000;
    if (hi < m.r32s(p + 8)) m.w32(p + 8, static_cast<u32>(hi));
}

void Game::goal_clamp_y(u32 i, u32 p) {
    cover(0x80050690u);
    const u32 sk = m.r32(srec(m, i) + 24);
    const s32 bottom = m.r32s(sk + 8);
    if (bottom < m.r32s(p + 4)) m.w32(p + 4, static_cast<u32>(bottom));
    const s32 top = m.r32s(sk + 4);
    if (m.r32s(p + 4) < top) m.w32(p + 4, static_cast<u32>(top));
}

void Game::beh_patrol(u32 i) {
    cover(0x8004F4D8u);
    Frame f(*this, 24);
    const u32 a = ai_rec(m, i);
    if (!m.r8(a + 62)) {
        m.w8(a + 62, 1);
        copy_words(m, a + 104, a + 68, 4);
        goal_clamp(i, a + 104);
        m.w8(a + 97, 0);
    }
    m.w8(a + 99, (m.r32(a) & 0x2000000u) ? 5 : 0);
}

void Game::beh_pursue(u32 i) {
    cover(0x8004ED9Cu);
    Frame f(*this, 24);
    const u32 a = ai_rec(m, i);
    const u32 t = slot(m.r8(a + 63));
    if (!m.r8(a + 62)) m.w8(a + 62, 1);
    for (u32 k = 0; k < 12; k += 4) m.w32(a + 104 + k, m.r32(t + 8 + k) + m.r32(t + 56 + k));
    copy_words(m, a + 120, t + 24, 2);
    goal_clamp(i, a + 104);
    m.w8(a + 99, 4);
    m.w8(a + 97, 4);
}

void Game::beh_station(u32 i) {
    cover(0x8004EE68u);
    Frame f(*this, 40);
    spill(m, f.at(0x10), i);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    const u32 t = slot(m.r8(a + 63));
    if (!m.r8(a + 62)) m.w8(a + 62, 1);
    from_local(t + 8, a + 68, a + 104);
    goal_clamp(i, a + 104);
    if (m.r32s(a + 136) >= 0x2000) {
        m.w8(a + 99, 4);
    } else {
        set_bits(m, sflag(i), 0x20u);
        m.w8(a + 99, 0);
    }
    u32 mode = 7;
    if (!(m.r8(e) & 0x40u) && !m.r8(a + 63)) mode = 4;
    m.w8(a + 97, mode);
}

void Game::beh_formation(u32 i) {
    cover(0x8004EF80u);
    Frame f(*this, 32);
    const u32 a = ai_rec(m, i);
    const u32 t = slot(m.r8(a + 63));
    if (!m.r8(a + 62)) m.w8(a + 62, 1);
    from_local(t + 8, a + 68, a + 104);
    goal_clamp(i, a + 104);
    m.w8(a + 99, m.r32s(a + 136) >= 0x2000 ? 4 : 7);
    m.w8(a + 97, 5);
}

void Game::beh_attack(u32 i) {
    cover(0x8004F034u);
    Frame f(*this, 24);
    const u32 a = ai_rec(m, i);
    const u32 t = slot(m.r8(a + 63));
    if (!m.r8(a + 62)) m.w8(a + 62, 1);
    copy_words(m, a + 104, t + 8, 4);
    goal_clamp(i, a + 104);
    m.w8(a + 99, 4);
    m.w8(a + 97, 6);
}

void Game::beh_attack_player(u32 i) {
    cover(0x8004F0C8u);
    Frame f(*this, 24);
    const u32 a = ai_rec(m, i);
    const u32 r = srec(m, i);
    const u32 e = slot(i);
    const u32 pl = slot(0);
    const u32 sk = m.r32(r + 24);
    const u32 fp = m.r32(r + 20);
    const u32 ph = m.r8(a + 62);
    const u32 diff = m.r32(m.r32(g::kScene) + 916);
    if (!ph) {
        m.w8(a + 62, 1);
        const u32 t = m.r16(sk + 14);
        m.w16(a + 64, t);
        if (static_cast<s32>(diff) >= 2) m.w16(a + 64, static_cast<u32>(static_cast<s16>(t) >> 1));
        m.w32(a + 108, m.r32(pl + 12));
        return;
    }
    if (ph == 1) {
        m.w8(a + 63, 0);
        const s32 tb = m.r8(m.r32(g::kWorld) + 12);
        m.w32(a + 104, m.r32(pl + 8) + (m.r32(pl + 56) << 12));
        m.w32(a + 112, m.r32(pl + 16) + (m.r32(pl + 64) << 12));
        if (static_cast<s32>(m.r32(m.r32(g::kScene) + 916)) >= 2) {
            const s32 k = (tb << 6) & 0x3FFF;
            const s32 reach = m.r8(sk + 1);
            m.w32(a + 104, m.r32(a + 104) + static_cast<u32>((sin_at(m, k) * reach) << 12));
            m.w32(a + 112, m.r32(a + 112) + static_cast<u32>((cos_at(m, k) * reach) << 12));
        }
        goal_clamp(i, a + 104);
        m.w8(a + 99, 4);
        m.w8(a + 97, 3);
        const u32 t = m.r16(a + 64) - 1u;
        m.w16(a + 64, t);
        if (!(t & 0xFFFFu)) m.w8(a + 62, 2);
        return;
    }
    if (ph == 3) {
        if (m.r32(a) & 0x400000u) m.w8(a + 61, 255);
        return;
    }
    if (ph != 2) return;
    m.w8(a + 62, 3);
    const u32 kind = m.r32(fp);
    if (kind == 3) return;
    u32 st = 12;
    const u32 tk = tick(m);
    const s32 mi = mission(m);
    if (kind == 1) {
        s32 turn = m.r16s(fp + 20);
        if (!turn) turn = 4096;
        if (!(tk & 1u)) turn = -turn;
        m.w16(a + 120, 2048);
        m.w16(a + 122, 0);
        m.w16(a + 124, static_cast<u32>(turn));
    } else if (sea_mission(mi)) {
        m.w16(a + 120, 0x2000);
        m.w16(a + 122, 0);
        m.w16(a + 124, static_cast<u32>(((tk & 1u) ? -4096 : 4096) - m.r16(e + 28)));
    } else if (m.r8(sk) < 2 || (tk & 7u)) {
        if (m.r8(sk) && (tk & 7u) == 1) {
            static constexpr u32 kTricks[4] = {10, 11, 13, 14};
            st = kTricks[m.r32(e + 8) & 3u];
        } else {
            m.w32(a + 120, 0x2000);
            m.w16(a + 124, static_cast<u32>((rand() & 0x3FFF) - 0x2000));
        }
    } else {
        st = 15;
        switch (m.r32(e + 8) & 3u) {
        case 0: m.w16(a + 120, 4096); m.w16(a + 122, 0); m.w16(a + 124, 0); break;
        case 1: m.w16(a + 120, 0x2000); m.w16(a + 122, 0); m.w16(a + 124, 0); break;
        case 2: m.w32(a + 120, 4096); m.w16(a + 124, 4096); break;
        default:
            m.w16(a + 122, 0);
            m.w16(a + 120, (m.r16(e + 12) & 0x1FFFu) + 0x2000);
            m.w16(a + 124, m.r16(e + 16) & 0x1FFFu);
            break;
        }
    }
    m.w8(a + 99, st);
    m.w8(a + 97, 0);
}

void Game::beh_evade(u32 i) {
    cover(0x8004F570u);
    Frame f(*this, 48);
    spill(m, f.at(0x18), 184 * i);
    const u32 r = srec(m, i);
    const u32 sk = m.r32(r + 24);
    const u32 fp = m.r32(r + 20);
    const u32 a = ai_rec(m, i);
    const u32 wobj = m.r32(g::kWeapons) + 164 * m.r8(m.r32(g::kWeapons) + i + 8);
    const u32 wp = wobj + 200;
    const u32 e = slot(i);
    if (!m.r8(a + 62)) {
        const u32 k = m.r32(fp);
        if (k && k != 5 && k != 2 && k != 1) m.w8(a + 61, 255);
        m.w8(a + 62, 1);
        m.w16(a + 64, m.r16(sk + 18) + m.r16(sk + 16) - (m.r8(sk + 1) - 22u));
        if (~m.r32(sflag(i)) & 0x800000u) {
            const u32 w = m.r32(wobj + 308);
            if ((w & 0x3Fu) && (w & 0x40u)) m.w8(a + 62, 16);
        }
        if (m.r8(a + 28) == 10 && (tick(m) & 3u)) m.w8(a + 62, 7);
    }
    const u32 t = m.r16(a + 64) - 1u;
    m.w16(a + 64, t);
    if (static_cast<s16>(t) <= 0) m.w8(a + 61, 255);
    if (m.r8(a + 62) == 16) {
        m.w8(e + 5, m.r8(e + 5) | 8u);
        m.w8(a + 99, 0);
        m.w8(a + 97, 0);
        return;
    }
    if (m.r16s(sk + 18) < m.r16s(a + 64)) return;
    m.w8(e + 5, m.r8(e + 5) | 8u);
    const u32 ph = m.r8(a + 62);
    if (ph == 7) {
        const u32 pl = slot(0);
        copy_words(m, a + 104, pl + 8, 4);
        copy_words(m, a + 120, pl + 24, 2);
        m.w8(a + 99, 4);
        m.w8(a + 97, 6);
        return;
    }
    if (ph == 3) {
        switch (m.r32(m.r32(g::kScene) + 916)) {
        case 3: m.w8(a + 62, 1); break;
        case 2: m.w8(a + 99, 0); if (m.r16s(a + 102) == 60) m.w8(a + 62, 1); break;
        case 1: m.w8(a + 99, 0); if (m.r16s(a + 102) == 90) m.w8(a + 62, 1); break;
        case 0: m.w8(a + 99, 0); break;
        default: break;
        }
    }
    if (m.r8(a + 62) == 2 && m.r8(a + 99) == 255) {
        m.w8(a + 99, 50);
        m.w8(a + 97, m.r8(a + 28) == 10 ? 1 : 0);
        m.w8(a + 62, 3);
    }
    if (m.r8(a + 62) != 1 || m.r16s(sk + 18) < m.r16s(a + 64)) return;
    m.w8(a + 62, 2);
    const u32 dd = m.r32(m.r32(g::kScene) + 916) & 3u;
    const s32 rnd = static_cast<s16>((rand() & 0x3FFF) - 0x2000);
    const u32 k = m.r32(fp);
    s32 turn = rnd;
    if (k == 1) {
        s32 v = m.r16s(fp + 20);
        if (!v) v = 4096;
        if (!(tick(m) & 1u)) v = -v;
        m.w16(a + 124, static_cast<u32>(v));
        m.w16(a + 120, 2048);
        m.w8(a + 99, 12);
        m.w16(a + 122, 0);
        m.w8(a + 97, m.r8(a + 28) == 10 ? k : 0);
        return;
    }
    const s32 mi = mission(m);
    if (sea_mission(mi)) {
        turn = ((tick(m) & 1u) ? -4096 : 4096) - m.r16(e + 28);
    } else if (dd == 3) {
        const s32 d = m.r32s(wp + 104);
        if (d < 1024) turn = rnd >> 3;
        else if (d < 2048) turn = rnd >> 2;
        else if (d < 4096) turn = rnd >> 1;
    } else if (!((dd || m.r8(sk) == 3) && m.r8(sk))) {
        const s32 roll = m.r16s(e + 28);
        if (absv(roll) >= 4097) {
            m.w16(a + 120, 0);
            m.w16(a + 122, 0);
            m.w8(a + 99, 12);
            m.w16(a + 124, static_cast<u32>(-roll));
            m.w8(a + 97, m.r8(a + 28) == 10 ? 1 : 0);
            return;
        }
        turn = (roll <= 0 ? 4096 : -4096) - roll;
    }
    const u32 reach = m.r8(sk + 1);
    const u32 shift = m.r32(kBreakShift + 4 * dd);
    const u32 base = m.r32(kBreakTurn + 4 * dd);
    m.w8(a + 99, 12);
    m.w16(a + 120, base + (reach << (shift & 31u)));
    m.w16(a + 122, 0);
    m.w16(a + 124, static_cast<u32>(turn));
    m.w8(a + 97, m.r8(a + 28) == 10 ? 1 : 0);
}

void Game::beh_level_out(u32 i) {
    cover(0x8004FB28u);
    const u32 a = ai_rec(m, i);
    if (m.r8(a + 62)) {
        if (m.r32(a) & 0x400000u) m.w8(a + 61, 255);
    } else {
        m.w8(a + 62, 1);
        m.w8(a + 99, 30);
    }
    m.w8(a + 97, m.r8(a + 100) ? 0 : 2);
}

void Game::beh_return(u32 i) {
    cover(0x8004FBA4u);
    const u32 a = ai_rec(m, i);
    if (m.r8(a + 62)) {
        if (m.r32(a) & 0x400000u) m.w8(a + 61, 255);
    } else {
        m.w8(a + 62, 1);
        m.w8(a + 99, 31);
    }
}

void Game::beh_leave(u32 i) {
    cover(0x8004FC04u);
    Frame f(*this, 24);
    const u32 a = ai_rec(m, i);
    if (!m.r8(a + 62)) {
        const u32 k = m.r32(m.r32(srec(m, i) + 20));
        s32 st;
        if (k == 1 || k == 3) {
            st = 43;
        } else {
            const s32 kind = m.r16s(slot(i) + 72);
            if (kind == 26) st = 42;
            else if (kind == 44) st = 40;
            else st = rand() % 3 + 40;
        }
        m.w8(a + 99, static_cast<u32>(st));
        m.w8(a + 62, 1);
    }
    m.w8(a + 97, 9);
}

void Game::beh_dive(u32 i) {
    cover(0x8004FCF0u);
    const u32 a = ai_rec(m, i);
    if (!m.r8(a + 62)) {
        m.w8(a + 62, 1);
        m.w8(a + 99, 25);
    }
    m.w8(a + 97, 9);
}

void Game::beh_drop(u32 i) {
    cover(0x8004FD38u);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    if (!m.r8(a + 62)) {
        m.w8(a + 62, 1);
        m.w32(e + 12, m.r32(e + 12) - 0x1000000u);
        m.w8(a + 99, 26);
    }
    m.w8(a + 97, 9);
}

void Game::beh_arrive(u32 i) {
    cover(0x8004FD9Cu);
    Frame f(*this, 32);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    if (!m.r8(a + 62)) {
        s32 base = 0;
        const s32 s = last_wave_slot();
        if (s != -1 && m.r32s(slot(static_cast<u32>(s)) + 8) < 0) base = 3;
        const u32 k = static_cast<u32>(base + rand() % 3);
        m.w32(e + 8, m.r32(kSpawnX + 4 * k));
        const u32 z = m.r32(kSpawnZ + 4 * k);
        m.w16(e + 24, 4096);
        m.w16(e + 26, 0x2000);
        m.w16(e + 28, 0);
        m.w32(e + 16, z);
        m.w8(a + 62, 1);
    }
    u32 ph = m.r8(a + 62);
    if (ph == 1) {
        m.w8(a + 97, 9);
        m.w8(a + 99, 26);
        if (m.r32s(e + 12) < -20971520) m.w8(a + 62, 2);
        ph = m.r8(a + 62);
    }
    if (ph == 2) {
        m.w8(a + 97, 0);
        m.w8(a + 99, 0);
        if (m.r32s(e + 16) < -536870912) {
            m.w8(a + 62, 3);
            m.w32(a + 108, static_cast<u32>(-419430400));
            m.w32(a + 112, static_cast<u32>(-956301312));
            m.w32(a + 104, m.r32(e + 8));
        }
    }
    if (m.r8(a + 62) == 3) {
        m.w8(a + 97, 0);
        m.w8(a + 99, 5);
        if (m.r32s(e + 12) < -419430400) {
            m.w8(a + 62, 4);
            msn_vm_exec(m.r32(m.r32(g::kMissionData) + 328));
        }
    }
}

s32 Game::last_wave_slot() {
    cover(0x80048A80u);
    const u32 sc = m.r32(g::kScene);
    s32 n = static_cast<s32>(m.r8(sc + 962)) - 1;
    if (n < 0) return -1;
    for (u32 at = static_cast<u32>(8 * n + 1864);; at -= 8) {
        --n;
        if (!m.r8(sc + at + 1)) return m.r8(sc + at + 6);
        if (n < 0) return -1;
    }
}

void Game::beh_close_in(u32 i) {
    cover(0x8004FF88u);
    Frame f(*this, 24);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    const u32 t = slot(m.r8(a + 63));
    u32 ph = m.r8(a + 62);
    if (!ph) {
        m.w8(a + 62, 1);
        m.w8(a + 99, 4);
        m.w8(a + 97, 0);
        ph = m.r8(a + 62);
    }
    if (ph != 1) return;
    copy_words(m, a + 104, t + 8, 4);
    if (m.r32s(a + 136) >= 8193) {
        const s32 s = m.r16s(g::kSinCos + ((tick(m) << 8) & 0x7F00u));
        const s32 yaw = m.r16(e + 26);
        const s32 dz = cos_at(m, yaw) * s;
        m.w32(e + 8, m.r32(e + 8) + static_cast<u32>((sin_at(m, yaw) * s) >> 14));
        m.w32(e + 16, m.r32(e + 16) + static_cast<u32>(dz >> 14));
    }
    if (m.r32s(a + 136) < 512) msn_vm_exec(m.r32(m.r32(g::kMissionData) + 328));
}

void Game::beh_ghost(u32 i) {
    cover(0x800500ECu);
    const u32 a = ai_rec(m, i);
    if (!m.r8(a + 62)) {
        m.w8(a + 99, 27);
        m.w8(a + 97, 9);
        m.w8(a + 62, 1);
    }
}

void Game::beh_hold(u32 i) {
    cover(0x80050134u);
    const u32 a = ai_rec(m, i);
    if (!m.r8(a + 62)) {
        m.w16(a + 128, 0);
        m.w16(a + 130, 0);
        m.w16(a + 132, 0);
        m.w8(a + 62, 1);
    }
    m.w8(a + 97, 8);
    m.w8(a + 99, 0);
}

void Game::beh_timed_exit(u32 i) {
    cover(0x80050184u);
    const u32 a = ai_rec(m, i);
    const u32 ph = m.r8(a + 62);
    if (ph == 1) {
        if (!m.r16(a + 64)) m.w8(a + 62, 2);
    } else if (ph == 2) {
        m.w8(a + 99, 52);
        m.w32(a + 108, 0);
        m.w32(a + 104, 0);
        m.w32(a + 112, mission(m) == 31 ? 0x200000u : 0x800000u);
        m.w8(a + 62, 3);
    } else if (ph) {
        if (m.r32(a) & 0x400000u) {
            m.w8(a + 61, 255);
            m.w32(srec(m, i) + 36, 0xFFFFFFFFu);
        }
    } else {
        const s32 mi = mission(m);
        m.w8(a + 99, 50);
        u32 t;
        switch (mi) {
        case 7: t = 150; break;
        case 31: t = 360; break;
        case 41: t = i == 2 ? 240 : 420; break;
        default: t = mi == 50 ? 210 : 40; break;
        }
        m.w16(a + 64, t);
        m.w8(a + 62, 1);
    }
    m.w8(a + 97, 9);
    if (m.r16s(a + 64) > 0) m.w16(a + 64, m.r16(a + 64) - 1u);
}

void Game::beh_escort_count(u32 i) {
    cover(0x800502D4u);
    Frame f(*this, 24);
    const u32 a = ai_rec(m, i);
    const u32 ph = m.r8(a + 62);
    const u32 w4 = m.r32(g::kWorld) + 4;
    if (ph) {
        if (ph == 1) {
            m.w8(a + 99, 12);
            m.w16(a + 124, 2048);
            m.w16(a + 120, 1024);
            m.w16(a + 122, 0);
            m.w8(a + 62, 2);
        } else if (ph == 2) {
            if (m.r32(a) & 0x400000u) m.w8(a + 62, 3);
        } else if (ph == 3) {
            m.w8(a + 99, 0);
        }
    } else {
        m.w8(a + 99, 0);
        if (m.r32(w4) & 0x800u) m.w16(a + 64, m.r16(a + 64) + 1u);
        if (m.r16s(a + 64) >= 151) m.w8(a + 62, 1);
    }
    const s32 mi = mission(m);
    if (m.r32(w4) & 0x800u) {
        const u32 rel = m.r32(g::kRelation + 4 * i);
        const u32 msn = m.r32(g::kMission);
        if (m.r32s(rel + 4) < 4097 || (m.r32(rel) & 2u)) {
            if (!(m.r16(msn + 8) & 0xFu)) hud_message(mi == 7 ? 7 : mi == 44 ? 8 : 6);
            m.w8(m.r32(g::kMission) + 10, 1);
            const u32 msn2 = m.r32(g::kMission);
            m.w16(msn2 + 8, m.r16(msn2 + 8) + 1u);
        } else {
            m.w8(msn + 10, 0);
            const u32 msn2 = m.r32(g::kMission);
            m.w16(msn2 + 8, m.r16(msn2 + 8) - 1u);
        }
    }
    if ((m.r32(w4) & 0x2000u) && m.r8(m.r32(g::kMission) + 47) == 1 && !(tick(m) & 0xFu))
        hud_message(mi == 7 ? 15 : mi == 44 ? 16 : 14);
    m.w8(a + 97, 8);
}

void Game::hud_message(u32 k) {
    cover(0x8004291Cu);
    m.w8(kHudMessage, 1);
    m.w8(kHudMessage + 1, k + 12);
    m.w16(kHudMessage + 2, 0);
}

void Game::beh_turn_away(u32 i) {
    cover(0x80050534u);
    const u32 a = ai_rec(m, i);
    const u32 ph = m.r8(a + 62);
    if (!ph) {
        m.w8(a + 99, 0);
        m.w8(a + 62, 1);
    } else if (ph == 1) {
        m.w8(a + 99, 12);
        m.w16(a + 124, static_cast<u32>(-2048));
        m.w16(a + 120, 2048);
        m.w16(a + 122, 0);
        m.w8(a + 62, 2);
    } else if (ph == 2) {
        if (m.r32(a) & 0x400000u) m.w8(a + 62, 3);
    } else if (ph == 3) {
        m.w8(a + 99, 0);
    }
    m.w8(a + 97, 8);
}

void Game::ai_weapons() {
    cover(0x80047A54u);
    Frame f(*this, 32);
    for (u32 i = 2; i < 32; ++i) {
        const u32 sf = sflag(i);
        if (~m.r32(sf) & 0x10000u) continue;
        m.w32(sf, m.r32(sf) & ~0x10000u);
        if (m.r8(ai_rec(m, i) + 96) == 255) return;
        if (ai_gun(i) == 1) ai_missile(i);
    }
}

u32 Game::ai_gun(u32 i) {
    cover(0x80047B00u);
    Frame f(*this, 40);
    u32 hits = 0;
    const u32 a = ai_rec(m, i);
    const u32 r = srec(m, i);
    const u32 gun = m.r32(r + 32);
    const u32 e = slot(i);
    if (!gun) return 1;
    if (m.r32(a + 160) >= m.r32(gun + 4)) {
        m.w16(a + 92, 0);
        return 1;
    }
    const u32 level = m.r8(m.r32(r + 24));
    const u32 off = m.r16(a + 180);
    if (level == 1) {
        if (off - 257u < 0xEFFu) hits = 1;
    } else if (level == 2 || level == 3) {
        if (m.r16s(a + 180) < 4096) hits = 1;
    } else if (level == 0) {
        if (off - 2049u < 0x17FFu) hits = 1;
    }
    const u32 cnt = m.r32(a + 92);
    switch (m.r16(gun)) {
    case 1: case 4: if ((cnt & 0x41u) == 64) ++hits; break;
    case 2: if ((cnt & 0x43u) == 64) ++hits; break;
    case 3: if ((cnt & 0x27u) == 32) ++hits; break;
    case 5:
        if ((cnt & 0x50u) == 0x50u) {
            if (m.r16(a + 92) & 1u) ++hits;
            gun_ray(e + 8, e + 24, (m.r16(a + 92) & 0xFu) << 6, 0);
            if (!(m.r16(a + 92) & 0xFu)) sound_at(13, e + 8, 0);
        }
        break;
    case 6: if ((cnt & 0x13u) == 16) ++hits; break;
    default: break;
    }
    if (hits == 2) bullet_request(m.r32(e), e + 8, e + 56, m.r16(gun) - 1u);
    m.w16(a + 92, m.r16(a + 92) + 1u);
    return 0;
}

void Game::ai_missile(u32 i) {
    cover(0x80047D68u);
    Frame f(*this, 48);
    const u32 a = ai_rec(m, i);
    const u32 r = srec(m, i);
    const u32 e = slot(i);
    const u32 t = slot(m.r8(a + 96));
    const u32 ms = m.r32(r + 28);
    const u32 sk = m.r32(r + 24);
    if (!ms || !m.r16(ms)) return;
    if (mission(m) == 23 && !m.r32(m.r32(g::kWorld) + 8)) {
        if (m.r32s(e + 12) > -62914560) return;
        if (absv(m.r32s(e + 8)) <= 155647999) return;
        if (absv(m.r32s(e + 16)) <= 155647999) return;
    }
    const u32 w = m.r32(g::kWeapons);
    const u32 tg = m.r8(a + 96);
    if (tg >= 32) {
        if (m.r32(w + 4) & (1u << ((tg - 32) & 31u))) {
            m.w16(a + 94, 0);
            return;
        }
    } else if (m.r32(w) & (1u << tg)) {
        m.w16(a + 94, 0);
        return;
    }
    if (!(m.r16s(a + 180) < m.r32s(ms + 4) && m.r32(ms + 8) >= m.r32(a + 160))) {
        m.w16(a + 94, 0);
        return;
    }
    m.w16(a + 94, m.r16(a + 94) + 1u);
    s32 interval = m.r16s(sk + 22);
    if (m.r32(e) & 0x80u) {
        const s32 d = m.r32s(m.r32(g::kScene) + 916);
        if (!d) interval *= 2;
        if (d >= 2) {
            interval >>= 1;
            if (interval < 60) interval = 60;
        }
    }
    if (m.r16s(a + 94) >= interval || (m.r8(a + 96) && (~m.r16(sflag(m.r8(a + 96))) & 0x400u))) {
        u32 k = m.r16(ms);
        if (k == 3 && (m.r32(sflag(m.r8(a + 96))) & 0x400u)) k = 1;
        const u32 owner = m.r32(e), target = m.r32(t);
        switch (k) {
        case 1: case 4: {
            u32 lv = 0;
            if (m.r32(m.r32(g::kScene) + 916) || !(m.r32(e) & 0x80u)) lv = m.r8(m.r32(srec(m, i) + 24));
            u32 flags;
            if (lv == 1) flags = 258;
            else if (lv == 2) flags = 65794;
            else if (lv == 3) flags = ((m.r32(e) & 0x40u) || m.r32(m.r32(g::kScene) + 916) == 3) ? 131586 : 66050;
            else if (lv == 0) flags = 134217730;
            else flags = 66050;
            spawn_weapon(owner, e + 8, e + 56, static_cast<s32>(target), flags);
            break;
        }
        case 2: launch_sam_12(owner, e + 8, e + 56, target); break;
        case 3: launch_sam_13(owner, e + 8, e + 56, target); break;
        case 8: launch_rocket_18(owner, e + 8, e + 56, target); break;
        case 9: launch_rocket_19(owner, e + 8, e + 56, target); break;
        default: break;
        }
        m.w16(a + 94, 0);
        if (m.r32(sflag(i)) & 0x800000u) {
            if ((m.r32(slot(i)) & 0x40u) && m.r8(a + 96) < 32) radio_call(3, 0, m.r8(m.r32(srec(m, i) + 16) + 3));
            if (m.r32(slot(i)) & 0x80u) radio_call(7, 0, m.r8(m.r32(srec(m, i) + 16) + 3));
        }
        if (m.r8(a + 96)) return;
    } else if (m.r8(a + 96)) {
        return;
    }
    s32 n = m.r16s(a + 94);
    const s32 quarter = interval >> 2;
    if ((interval >> 1) < n) {
        if (n < m.r16s(sk + 22)) m.w32(kAlertFlag, m.r32(kAlertFlag) | 1u);
        n = m.r16s(a + 94);
    }
    if (n != quarter) return;
    if ((m.r32(sflag(i)) & 0x800000u) && (m.r32(e) & 0x80u))
        radio_call(6, 0, m.r8(m.r32(m.r32(g::kSlotRecords) + 64 * i + 16) + 3));
    else
        radio_call(0, 0, 0);
}

void Game::spark_shower(u32 pos, u32 vel) {
    cover(0x80082D38u);
    Frame f(*this, 32);
    record_motion(4, 8, pos, vel);
    spark_shower_fx(pos, vel);
}

void Game::spark_shower_fx(u32 pos, u32 vel) {
    cover(0x80082C1Cu);
    Frame g2(*this, 40);
    const u32 p = fx_flashes();
    if (!p) return;
    const s32 vx = m.r32s(vel), vy = m.r32s(vel + 4), vz = m.r32s(vel + 8);
    m.w32(p + 4, static_cast<u32>(m.r32s(pos) - vx));
    m.w32(p + 8, static_cast<u32>(m.r32s(pos + 4) - vy));
    m.w16(p + 22, 135);
    m.w32(p + 12, static_cast<u32>(m.r32s(pos + 8) - vz));
    const s32 r = rand();
    m.w16(p, static_cast<u32>((r & 1) + 4));
    m.w16(p + 2, static_cast<u32>((r & 3) + 40));
    m.w16(p + 16, static_cast<u32>(((0 - vx) >> 6) - 64 + (r & 0x7F)));
    m.w16(p + 18, static_cast<u32>(((0 - vy) >> 6) - 64 + ((r >> 3) & 0x7F)));
    m.w16(p + 20, static_cast<u32>(((0 - vz) >> 6) - 64 + ((r >> 6) & 0x7F)));
    m.w16(p + 24, 273);
    m.w16(p + 26, static_cast<u32>((r & 0x3F) + 240));
}

void Game::smoke(u32 pos, u32 vel, u32 c, u32 kind) {
    cover(0x80081C30u);
    Frame f(*this, 32);
    u32 k = kind;
    if (k < 2 && (m.r32(m.r32(g::kWorld)) & 0x100u)) {
        k = 1;
        c = 0;
    }
    switch (k) {
    case 1: if (c & 1u) return; break;
    case 0: if (c & 3u) return; break;
    case 2: if ((c & 3u) || (rand() & 1)) return; break;
    case 3: if ((c & 1u) || !(rand() & 3)) return; break;
    default: break;
    }
    m.w8(record_motion(4, 2, pos, vel) + 2, k);
    smoke_fx(pos, vel, k);
}

void Game::smoke_fx(u32 pos, u32 vel, u32 k) {
    cover(0x80081BC8u);
    Frame g2(*this, 40);
    static constexpr u32 kFns[4] = {0x8008136Cu, 0x8008158Cu, 0x80081760u, 0x80081994u};
    for (u32 j = 0; j < 4; ++j) m.w32(g2.at(0x10 + 4 * j), kFns[j]);
    switch (k) {
    case 0: smoke_fire(pos, vel); break;
    case 1: smoke_flash(pos, vel); break;
    case 2: smoke_black(pos, vel); break;
    case 3: smoke_trail_light(pos, vel); break;
    default: break;
    }
}

namespace {
s32 u8of(s32 v) { return v & 0xFF; }
s32 mix(s32 r) { return (r >> 8) | (r << 7); }
}

void Game::smoke_fire(u32 pos, u32 vel) {
    cover(0x8008136Cu);
    Frame f(*this, 56);
    const s32 vx = m.r32s(vel), vy = m.r32s(vel + 4), vz = m.r32s(vel + 8);
    const s32 x = m.r32s(pos) + ((rand() & 0x7F) << 11);
    const s32 y = m.r32s(pos + 4) + ((rand() & 0x7F) << 11);
    const s32 z = m.r32s(pos + 8) + ((rand() & 0x7F) << 11);
    const s32 r = rand();
    if (const u32 p = fx_flashes()) {
        m.w32(p, 2621440);
        m.w16(p + 24, 528);
        m.w32(p + 4, static_cast<u32>(x));
        m.w32(p + 8, static_cast<u32>(y));
        m.w32(p + 12, static_cast<u32>(z));
        m.w16(p + 22, 0);
        m.w16(p + 26, static_cast<u32>((r & 0x3F) + 128));
    }
    if (r & 1) {
        if (const u32 p = fx_flashes()) {
            m.w32(p, 851967);
            m.w32(p + 4, static_cast<u32>(x - vx));
            m.w32(p + 8, static_cast<u32>(y - vy));
            m.w32(p + 12, static_cast<u32>(z - vz));
            m.w16(p + 24, 256);
            m.w16(p + 22, 0);
            m.w16(p + 26, static_cast<u32>(((r >> 3) & 0x3F) + 88));
        }
    }
    if (!(r & 3)) return;
    const u32 d = fx_debris();
    if (!d) return;
    m.w32(d + 4, static_cast<u32>(x));
    m.w32(d + 8, static_cast<u32>(y));
    m.w32(d + 12, static_cast<u32>(z));
    const s32 s = rand();
    m.w16(d + 16, static_cast<u32>((vx >> 8) - 128 + u8of(s)));
    m.w16(d + 18, static_cast<u32>((vy >> 8) - 128 + u8of(s >> 3)));
    m.w16(d + 20, static_cast<u32>((vz >> 8) - 128 + u8of(s >> 6)));
    if (s & 1) {
        m.w32(d, 1179648);
        m.w16(d + 22, 139);
        m.w16(d + 24, 5380);
        m.w16(d + 26, static_cast<u32>((s & 0x1F) + 100));
    } else {
        m.w32(d, 1966080);
        m.w16(d + 22, 396);
        m.w16(d + 24, 0);
        m.w16(d + 26, static_cast<u32>((s & 7) + 20));
    }
}

void Game::smoke_flash(u32 pos, u32 vel) {
    cover(0x8008158Cu);
    Frame f(*this, 56);
    const s32 vx = m.r32s(vel), vy = m.r32s(vel + 4), vz = m.r32s(vel + 8);
    const s32 x = m.r32s(pos) + vx, z = m.r32s(pos + 8) + vz, y = m.r32s(pos + 4) + vy;
    const s32 r = rand();
    if (const u32 p = fx_flashes()) {
        m.w16(p, 0xFFFF);
        m.w16(p + 2, static_cast<u32>((r & 7) + 9));
        m.w32(p + 4, static_cast<u32>(x + ((u8of(r) - 128) << 9)));
        m.w32(p + 8, static_cast<u32>(y + ((u8of(r >> 3) - 128) << 9)));
        m.w32(p + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 9)));
        m.w16(p + 24, 256);
        m.w16(p + 22, 0);
        m.w16(p + 26, static_cast<u32>((r & 0x3F) + 90));
    }
    if (!(r & 1)) return;
    const u32 d = fx_debris();
    if (!d) return;
    m.w16(d, 0);
    m.w16(d + 2, static_cast<u32>((r & 7) + 16));
    m.w32(d + 4, static_cast<u32>(x));
    m.w32(d + 8, static_cast<u32>(y));
    m.w32(d + 12, static_cast<u32>(z));
    const s32 s = rand();
    m.w16(d + 16, static_cast<u32>((vx >> 8) - 64 + (s & 0x7F)));
    m.w16(d + 18, static_cast<u32>((vy >> 8) - 64 + ((s >> 3) & 0x7F)));
    m.w16(d + 20, static_cast<u32>((vz >> 8) - 64 + ((s >> 6) & 0x7F)));
    if (s & 1) {
        m.w16(d + 22, 141);
        m.w16(d + 24, 5380);
        m.w16(d + 26, static_cast<u32>((s & 0x3F) + 150));
    } else {
        m.w16(d + 22, 396);
        m.w16(d + 24, 0);
        m.w16(d + 26, static_cast<u32>((s & 7) + 20));
    }
}

void Game::smoke_black(u32 pos, u32 vel) {
    cover(0x80081760u);
    Frame f(*this, 48);
    const s32 vx = m.r32s(vel), vy = m.r32s(vel + 4), vz = m.r32s(vel + 8);
    const s32 x = m.r32s(pos) + vx, y = m.r32s(pos + 4) + vy, z = m.r32s(pos + 8) + vz;
    const s32 dx = (0 - vx) >> 11, dy = (0 - vy) >> 11, dz = (0 - vz) >> 11;
    if (const u32 p = fx_flashes()) {
        const s32 r = rand();
        const s32 q = mix(r);
        m.w32(p + 4, static_cast<u32>(x + ((u8of(r) - 128) << 9)));
        m.w16(p + 16, static_cast<u32>(dx - 128 + u8of(q)));
        m.w16(p + 18, static_cast<u32>(dy - 256));
        m.w16(p + 22, 132);
        m.w16(p + 24, 528);
        m.w16(p, 0);
        m.w16(p + 2, static_cast<u32>((r & 0x1F) + 30));
        m.w32(p + 8, static_cast<u32>(y + ((u8of(r >> 3) - 128) << 9)));
        m.w32(p + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 9)));
        m.w16(p + 20, static_cast<u32>(dz - 128 + u8of(q >> 7)));
        m.w16(p + 26, static_cast<u32>(((r >> 8) & 0x7F) + 120));
    }
    const u32 d = fx_debris();
    if (!d) return;
    const s32 r = rand();
    const s32 q = mix(r);
    m.w32(d + 4, static_cast<u32>(x + ((u8of(r) - 128) << 9)));
    m.w16(d + 16, static_cast<u32>(dx - 64 + ((r >> 8) & 0x7F)));
    m.w16(d + 18, static_cast<u32>(dy - 256));
    m.w16(d + 22, 131);
    m.w16(d + 24, 5636);
    m.w16(d, 0);
    m.w16(d + 2, static_cast<u32>((r & 0xF) + 18));
    m.w32(d + 8, static_cast<u32>(y + ((u8of(r >> 3) - 128) << 9)));
    m.w32(d + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 9)));
    m.w16(d + 20, static_cast<u32>(dz - 64 + ((q >> 7) & 0x7F)));
    m.w16(d + 26, static_cast<u32>(((r >> 8) & 0x3F) + 100));
}

void Game::smoke_trail_light(u32 pos, u32 vel) {
    cover(0x80081994u);
    Frame f(*this, 56);
    const s32 vx = m.r32s(vel), vy = m.r32s(vel + 4), vz = m.r32s(vel + 8);
    const s32 x = m.r32s(pos) - vx, y = m.r32s(pos + 4) - vy, z = m.r32s(pos + 8) - vz;
    const s32 dx = (0 - vx) >> 7, dy = (0 - vy) >> 7, dz = (0 - vz) >> 7;
    s32 r = rand();
    const u32 d = fx_debris();
    const s32 life = r & 0xF;
    if (d) {
        const s32 b0 = u8of(r), b3 = r >> 3, b6 = r >> 6;
        r = mix(r);
        m.w32(d + 4, static_cast<u32>(x + ((b0 - 128) << 8)));
        m.w32(d + 8, static_cast<u32>(y + ((u8of(b3) - 128) << 8)));
        m.w16(d + 16, static_cast<u32>(dx - 128 + u8of(r)));
        m.w16(d + 18, static_cast<u32>(dy - 192));
        m.w16(d + 22, 134);
        m.w16(d, 0);
        m.w16(d + 2, static_cast<u32>(life + 30));
        m.w32(d + 12, static_cast<u32>(z + ((u8of(b6) - 128) << 8)));
        m.w16(d + 20, static_cast<u32>(dz - 128 + u8of(r >> 7)));
        m.w16(d + 24, 6148);
        m.w16(d + 26, static_cast<u32>(u8of(r) + 200));
    }
    if (!(r & 1)) return;
    const u32 p = fx_flashes();
    if (!p) return;
    const s32 s = rand();
    m.w16(p, 0xFFFF);
    m.w32(p + 8, static_cast<u32>(y + ((u8of(s >> 3) - 128) << 8)));
    m.w16(p + 16, static_cast<u32>(dx - 64 + ((s >> 8) & 0x7F)));
    m.w32(p + 4, static_cast<u32>(x + ((u8of(s) - 128) << 8)));
    m.w16(p + 18, static_cast<u32>(dy - 192));
    m.w16(p + 22, 135);
    m.w16(p + 2, static_cast<u32>((s & 0x1F) + 50));
    m.w32(p + 12, static_cast<u32>(z + ((u8of(s >> 6) - 128) << 8)));
    m.w16(p + 20, static_cast<u32>(dz - 64 + ((mix(s) >> 7) & 0x7F)));
    m.w16(p + 24, 528);
    m.w16(p + 26, static_cast<u32>(((s >> 8) & 0x7F) + 200));
}

void Game::explosion_fall(u32 pos, u32 vel, u32 small) {
    cover(0x80081298u);
    Frame f(*this, 32);
    if (m.r32(m.r32(g::kWorld)) & 0x100u) small = 1;
    m.w8(record_motion(4, 1, pos, vel) + 2, small);
    explosion_fall_fx(pos, vel, small);
}

void Game::explosion_fall_fx(u32 pos, u32 vel, u32 small) {
    cover(0x80080F70u);
    Frame g2(*this, 56);
    const s32 x = m.r32s(pos), y = m.r32s(pos + 4), z = m.r32s(pos + 8);
    if (small == 1) {
        fx_80089504(x, y, z);
        return;
    }
    if (const u32 p = fx_flashes()) {
        const s32 r = rand();
        m.w32(p, 1900543);
        m.w32(p + 4, static_cast<u32>(x));
        m.w32(p + 8, static_cast<u32>(y));
        m.w32(p + 12, static_cast<u32>(z));
        m.w16(p + 22, 0);
        m.w16(p + 24, 528);
        m.w16(p + 26, static_cast<u32>((r & 0x3F) + 588));
    }
    for (s32 n = 0; n < 4; ++n) {
        const u32 q = fx_flashes();
        if (!q) break;
        const s32 r = rand();
        m.w16(q, static_cast<u32>(-(r & 1) - 2 * n));
        m.w16(q + 2, static_cast<u32>(((r >> 1) & 3) + 15));
        m.w32(q + 4, static_cast<u32>(x + ((u8of(r) - 128) << 12)));
        m.w32(q + 8, static_cast<u32>(y + ((u8of(r >> 3) - 128) << 12)));
        m.w32(q + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 12)));
        m.w16(q + 24, 256);
        m.w16(q + 26, static_cast<u32>(((r >> 7) & 0x7F) + 300 - 32 * n));
        m.w16(q + 22, 0);
    }
    if (const u32 l = fx_light()) {
        const s32 r = u8of(rand());
        m.w16(l, 0);
        m.w16(l + 2, static_cast<u32>((r & 3) + 16));
        m.w32(l + 4, static_cast<u32>(x));
        m.w32(l + 8, static_cast<u32>(y));
        m.w32(l + 12, static_cast<u32>(z));
        m.w16(l + 20, 0);
        m.w16(l + 22, static_cast<u32>(r + 1600));
    }
    const s32 dx = m.r32s(vel) >> 9, dy = m.r32s(vel + 4) >> 9, dz = (m.r32s(vel + 8) >> 9) - 128;
    for (s32 n = 0; n < 16; ++n) {
        const u32 d = fx_debris();
        if (!d) break;
        m.w16(d, 0);
        m.w16(d + 2, static_cast<u32>(n + 28));
        const s32 r = rand();
        const s32 q = mix(r);
        m.w32(d + 4, static_cast<u32>(x + ((u8of(r) - 128) << 9)));
        m.w16(d + 16, static_cast<u32>(dx - 128 + u8of(q)));
        m.w16(d + 18, static_cast<u32>(dy + u8of(q >> 3) - 192));
        m.w16(d + 20, static_cast<u32>(dz + u8of(q >> 6)));
        m.w32(d + 8, static_cast<u32>(y + ((u8of(r >> 3) - 128) << 9) - 163840));
        m.w32(d + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 9)));
        if (n >= 3) {
            m.w16(d + 22, 392);
            m.w16(d + 24, 0);
            m.w16(d + 26, static_cast<u32>(n + 24));
        } else {
            m.w16(d + 22, 140);
            m.w16(d + 24, 5380);
            const u32 v = m.r16(d + 18);
            m.w16(d + 26, static_cast<u32>(32 * n + 280));
            m.w16(d + 18, v - 128u);
        }
    }
}
}

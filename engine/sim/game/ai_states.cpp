#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kGhostTrack = 0x800C71A0u;
constexpr u32 kGhostLive = 0x800C77A0u;

u32 ai_rec(Memory& m, u32 i) { return m.r32(g::kAiInfo) + 184 * i; }
u32 srec(Memory& m, u32 i) { return m.r32(g::kSlotRecords) + 64 * i; }
u32 limits(Memory& m, u32 i) { return m.r32(srec(m, i) + 20); }
u32 sflag(u32 i) { return g::kSlotFlags + 4 * i; }
bool flying(Memory& m, u32 e) { return (~m.r32(e) & 0xC000u) == 0; }
s32 mission(Memory& m) { return m.r16s(m.r32(g::kScene) + 908); }
bool sea_mission(s32 mi) { return mi == 32 || mi == 47 || mi == 53; }
s32 absv(s32 v) { return v < 0 ? -v : v; }
s32 sin_at(Memory& m, s32 i) { return m.r16s(g::kSinCos + 2 * static_cast<u32>(i & 0x3FFF)); }
s32 cos_at(Memory& m, s32 i) { return m.r16s(g::kSinCos + 0x2000 + 2 * static_cast<u32>(i & 0x3FFF)); }
void set_bits(Memory& m, u32 a, u32 bits) { m.w32(a, m.r32(a) | bits); }
void spill(Memory& m, u32 at, u32 value) { m.w32(at, value); }
void zero_turns(Memory& m, u32 a) {
    m.w16(a + 128, 0);
    m.w16(a + 130, 0);
    m.w16(a + 132, 0);
}
s32 angle_diff(s32 d) {
    u32 v = static_cast<u32>(d) & 0x3FFFu;
    if (v >= 0x2000u) v |= 0xC000u;
    return static_cast<s16>(v);
}
}

void Game::ai_states() {
    cover(0x8004701Cu);
    Frame f(*this, 40);
    for (u32 i = 2; i < 32; ++i) {
        const u32 e = slot(i);
        const u32 a = ai_rec(m, i);
        if (!flying(m, e) || m.r8(a + 28) == 6) continue;
        u32 st = m.r8(a + 98);
        if (st != m.r8(a + 99)) {
            const u32 flags = m.r32(a);
            const u32 next = m.r8(a + 99);
            m.w8(a + 100, 0);
            m.w16(a + 102, 0);
            m.w32(a, flags & ~0x400000u);
            m.w8(a + 98, next);
            m.w16(e + 48, 0);
            m.w16(e + 50, 0);
            m.w16(e + 52, 0);
            st = m.r8(a + 98);
        }
        const u32 r = srec(m, i);
        switch (st) {
        case 0: st_level(i); break;
        case 4: {
            if (m.r32(m.r32(r + 20)) == 1) {
                st_approach(i);
                break;
            }
            const s32 mi = mission(m);
            if ((m.r8(a + 60) != 3 && !m.r8(m.r32(r + 24))) || sea_mission(mi))
                st_fly_to(i);
            else
                st_track(i);
            break;
        }
        case 5: {
            const u32 k = m.r32(m.r32(r + 20));
            if (k == 1) st_approach(i);
            else if (k == 3) st_climb_to(i);
            else st_turn_toward(i);
            break;
        }
        case 6: st_approach(i); break;
        case 7: st_formation(i); break;
        case 8: st_scripted(i); break;
        case 9: st_attack_turn(i); break;
        case 10: st_loop(i); break;
        case 11: st_half_loop(i); break;
        case 12: st_break(i); break;
        case 13: st_roll_away(i); break;
        case 14: st_roll_drop(i); break;
        case 15: case 16: case 17: st_split(i); break;
        case 25: st_circle(i); break;
        case 26: st_fall(i); break;
        case 27: st_ghost(i); break;
        case 30: st_level_band(i); break;
        case 31: st_turn_back(i); break;
        case 40: st_explode(i); break;
        case 41: st_crash_smoke(i); break;
        case 42: st_crash_spin(i); break;
        case 43: st_crash_dive(i); break;
        case 50: st_park(i); break;
        case 51: st_taxi(i); break;
        case 52: st_takeoff(i); break;
        default: break;
        }
        const u32 next = m.r8(a + 99);
        m.w16(a + 102, m.r16(a + 102) + 1u);
        if (next == 255) {
            const u32 v = m.r8(a + 99);
            set_bits(m, a, 0x400000u);
            m.w8(a + 98, v);
            m.w16(e + 48, 0);
            m.w16(e + 50, 0);
            m.w16(e + 52, 0);
        }
    }
    for (u32 i = 2; i < 32; ++i)
        if (flying(m, slot(i)) && m.r8(ai_rec(m, i) + 28) != 6) ai_steer(i);
    for (u32 i = 2; i < 32; ++i)
        if (flying(m, slot(i)) && m.r8(ai_rec(m, i) + 28) != 6) ai_speed(i);
}

namespace {
void end_or_restart(Memory& m, u32 a, u32 ph, u32 last) {
    if (ph == last)
        m.w8(a + 99, 255);
    else
        m.w8(a + 100, 0);
}
}

void Game::st_level(u32 i) {
    cover(0x80049E28u);
    Frame f(*this, 32);
    const u32 a = ai_rec(m, i);
    const u32 ph = m.r8(a + 100);
    if (ph == 1) return;
    if (ph >= 2) {
        end_or_restart(m, a, ph, 2);
        return;
    }
    if (!turn_level(i)) m.w8(a + 100, 1);
}

void Game::st_track(u32 i) {
    cover(0x8004960Cu);
    Frame f(*this, 32);
    set_bits(m, sflag(i), 0x20u);
    const u32 a = ai_rec(m, i);
    turn_toward(i, a + 140, m.r16(a + 156), f.at(0x10));
}

void Game::st_fly_to(u32 i) {
    cover(0x8004966Cu);
    Frame f(*this, 72);
    const u32 e = slot(i);
    set_bits(m, sflag(i), 0x20u);
    const u32 a = ai_rec(m, i);
    switch (m.r8(a + 100)) {
    case 0: {
        const s32 gx = m.r32s(a + 104) >> 12, gz = m.r32s(a + 112) >> 12;
        m.w32(f.at(0x10), static_cast<u32>(gx));
        m.w32(f.at(0x18), static_cast<u32>(gz));
        const s32 d = absv(m.r32s(e + 12) - ratan2(gx, gz));
        m.w8(a + 100, d < 1025 ? 2 : 1);
        break;
    }
    case 1: {
        const u32 goal = f.at(0x10), local = f.at(0x20), out = f.at(0x30);
        m.w32(goal, m.r32(a + 104));
        m.w32(goal + 4, m.r32(e + 12));
        m.w32(goal + 8, m.r32(a + 112));
        to_local(e + 8, goal, local);
        const s32 len = static_cast<s16>(off_axis_angle(local));
        turn_toward(i, local, static_cast<u32>(len), out);
        if (absv(m.r16s(out)) < 16 && absv(m.r16s(out + 2)) < 16) m.w8(a + 100, 2);
        break;
    }
    case 2:
        if (!turn_level(i)) m.w8(a + 100, 3);
        break;
    case 3:
        climb_toward(i, a + 104);
        if (absv(m.r32s(e + 12) - m.r32s(a + 108)) <= 0x7FFFF) m.w8(a + 100, 4);
        break;
    case 4: m.w8(a + 99, 255); break;
    default: m.w8(a + 100, 0); break;
    }
}

void Game::st_turn_toward(u32 i) {
    cover(0x80049A1Cu);
    Frame f(*this, 32);
    set_bits(m, sflag(i), 0x20u);
    const u32 a = ai_rec(m, i);
    const u32 ph = m.r8(a + 100);
    if (ph == 1) {
        turn_toward(i, a + 140, m.r16(a + 156), f.at(0x10));
        return;
    }
    if (ph >= 2) {
        if (ph == 2) {
            if (!turn_level(i)) m.w8(a + 100, 3);
        } else {
            end_or_restart(m, a, ph, 3);
        }
        return;
    }
    m.w8(a + 100, 1);
}

void Game::st_attack_turn(u32 i) {
    cover(0x80049F38u);
    Frame f(*this, 32);
    set_bits(m, sflag(i), 0x20u);
    const u32 a = ai_rec(m, i);
    const u32 ph = m.r8(a + 100);
    m.w8(a + 97, 1);
    if (ph == 1) {
        turn_toward(i, a + 140, m.r16(a + 156), f.at(0x10));
        return;
    }
    if (ph >= 2) {
        if (ph == 2) {
            if (!turn_level(i)) m.w8(a + 100, 3);
        } else {
            end_or_restart(m, a, ph, 3);
        }
        return;
    }
    m.w8(a + 100, 1);
}

void Game::st_climb_to(u32 i) {
    cover(0x80049C4Cu);
    Frame f(*this, 32);
    const u32 e = slot(i);
    const u32 fp = limits(m, i);
    set_bits(m, sflag(i), 0x20u);
    const u32 a = ai_rec(m, i);
    const u32 ph = m.r8(a + 100);
    if (ph == 1) {
        const s32 h = ratan2((m.r32s(a + 104) - m.r32s(e + 8)) >> 12, (m.r32s(a + 112) - m.r32s(e + 16)) >> 12);
        const u32 yaw = m.r16(e + 26);
        const s32 d = angle_diff(h - static_cast<s32>(yaw));
        const s32 rate = m.r16s(fp + 30);
        if (absv(d) < rate) m.w16(e + 26, yaw + static_cast<u32>(d));
        else if (d > 0) m.w16(e + 26, yaw + static_cast<u32>(rate));
        else if (d < 0) m.w16(e + 26, yaw - static_cast<u32>(rate));
        const s32 y = m.r32s(e + 12);
        const s32 dy = m.r32s(a + 108) - y;
        if (dy < 20481) m.w32(e + 12, dy >= -20480 ? m.r32(a + 108) : static_cast<u32>(y - 20480));
        else m.w32(e + 12, static_cast<u32>(y + 20480));
        return;
    }
    if (ph >= 2) {
        if (ph == 2) {
            if (!turn_level(i)) m.w8(a + 100, 0);
        } else if (ph == 3) {
            if (!turn_level(i)) m.w8(a + 99, 255);
            m.w8(a + 100, 0);
        } else {
            m.w8(a + 100, 0);
        }
        return;
    }
    m.w16(e + 52, 0);
    m.w16(e + 50, 0);
    m.w16(e + 48, 0);
    m.w16(a + 132, 0);
    m.w16(a + 130, 0);
    m.w16(a + 128, 0);
    m.w8(a + 100, 1);
}

void Game::st_approach(u32 i) {
    cover(0x80049AF8u);
    Frame f(*this, 32);
    const u32 e = slot(i);
    set_bits(m, sflag(i), 0x20u);
    const u32 a = ai_rec(m, i);
    switch (m.r8(a + 100)) {
    case 0: if (!turn_level(i)) m.w8(a + 100, 1); break;
    case 1: if (!heading_toward(i, a + 104)) m.w8(a + 100, 2); break;
    case 2: if (!turn_level(i)) m.w8(a + 100, 3); break;
    case 3:
        climb_toward(i, a + 104);
        if (absv(m.r32s(e + 12) - m.r32s(a + 108)) <= 0x3FFFF) m.w8(a + 100, 0);
        break;
    case 4: if (!turn_level(i)) m.w8(a + 99, 255); break;
    default: m.w8(a + 100, 0); break;
    }
}

void Game::st_formation(u32 i) {
    cover(0x8004986Cu);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    const u32 t = slot(m.r8(a + 63));
    set_bits(m, sflag(i), 0x20u);
    const u32 ph = m.r8(a + 100);
    if (ph == 1) {
        for (u32 k : {24u, 28u, 26u}) {
            s32 d = static_cast<s32>((m.r16(t + k) - m.r16(e + k)) & 0x3FFFu);
            if (d >= 0x2001) d -= 0x4000;
            m.w16(e + k, m.r16(e + k) + static_cast<u32>(d / 4));
        }
        for (u32 k = 0; k < 12; k += 4) {
            const s32 p = m.r32s(e + 8 + k);
            m.w32(e + 8 + k, static_cast<u32>(p + ((m.r32s(a + 104 + k) - p) >> 8)));
        }
        return;
    }
    if (ph >= 2) end_or_restart(m, a, ph, 2);
    else m.w8(a + 100, 1);
}

void Game::st_scripted(u32 i) {
    cover(0x80049EC4u);
    const u32 a = ai_rec(m, i);
    const u32 ph = m.r8(a + 100);
    if (ph == 1) return;
    if (ph >= 2) end_or_restart(m, a, ph, 2);
    else m.w8(a + 100, 1);
}

void Game::st_loop(u32 i) {
    cover(0x8004A2D8u);
    Frame f(*this, 32);
    const u32 a = ai_rec(m, i);
    const u32 ph = m.r8(a + 100);
    if (ph == 1) {
        if (!m.r16(a + 128)) m.w8(a + 100, 2);
        return;
    }
    if (ph >= 2) {
        end_or_restart(m, a, ph, 2);
        return;
    }
    if (!turn_level(i)) {
        m.w8(a + 100, 1);
        m.w16(a + 128, 0x4000);
    }
}

void Game::st_half_loop(u32 i) {
    cover(0x8004A394u);
    Frame f(*this, 32);
    const u32 a = ai_rec(m, i);
    const u32 ph = m.r8(a + 100);
    if (ph == 1) {
        if (!m.r16(a + 128)) m.w8(a + 100, 2);
        return;
    }
    if (ph >= 2) {
        if (ph == 2) {
            if (!turn_level(i)) m.w8(a + 100, 3);
        } else {
            end_or_restart(m, a, ph, 3);
        }
        return;
    }
    if (!turn_level(i)) {
        m.w8(a + 100, 1);
        m.w16(a + 128, 0x2000);
    }
}

void Game::st_break(u32 i) {
    cover(0x8004A678u);
    const u32 a = ai_rec(m, i);
    const bool novice = m.r8(m.r32(srec(m, i) + 24)) < 2;
    const u32 ph = m.r8(a + 100);
    if (novice || sea_mission(mission(m))) {
        if (ph == 2 || ph == 0) {
            if (ph == 0) {
                m.w16(a + 130, 0);
                m.w16(a + 128, 0);
                m.w8(a + 100, 2);
                m.w16(a + 132, m.r16(a + 124));
            }
            if (!m.r16(a + 132)) {
                m.w8(a + 100, 3);
                m.w16(a + 128, m.r16(a + 120));
            }
            return;
        }
        if (ph == 1) {
            m.w8(a + 100, 0);
            return;
        }
        if (ph != 3) {
            if (ph == 4) m.w8(a + 99, 255);
            else m.w8(a + 100, 0);
            return;
        }
        if (!m.r16(a + 128)) m.w8(a + 100, 4);
        return;
    }
    if (ph != 1) {
        if (ph >= 2) {
            if (ph == 4) m.w8(a + 99, 255);
            else m.w8(a + 100, 0);
            return;
        }
        m.w32(a + 128, m.r32(a + 120));
        m.w32(a + 132, m.r32(a + 124));
        m.w8(a + 100, 1);
    }
    if (m.r16(a + 128)) return;
    if (!m.r16(a + 132)) m.w8(a + 100, 4);
}

namespace {
u32 roll_to_centre(Memory& m, u32 e) {
    const s32 z = m.r32s(e + 16);
    if (z > 0) return static_cast<u32>(-16384 - m.r16(e + 28));
    if (z < 0) return static_cast<u32>(0x4000 - m.r16(e + 28));
    return (m.r32(e + 8) & 1u) ? static_cast<u32>(-16384) : 0x4000u;
}
}

void Game::st_roll_away(u32 i) {
    cover(0x8004A474u);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    const u32 ph = m.r8(a + 100);
    if (ph >= 2) {
        end_or_restart(m, a, ph, 2);
        return;
    }
    if (ph == 0) {
        m.w16(a + 132, roll_to_centre(m, e));
        m.w8(a + 100, 1);
    }
    if (!m.r16(a + 132)) m.w8(a + 100, 2);
}

void Game::st_roll_drop(u32 i) {
    cover(0x8004A56Cu);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    const u32 ph = m.r8(a + 100);
    if (ph >= 2) {
        end_or_restart(m, a, ph, 2);
        return;
    }
    if (ph == 0) {
        m.w16(a + 132, roll_to_centre(m, e));
        m.w8(a + 100, 1);
    }
    m.w32(e + 36, static_cast<u32>(-86016));
    set_bits(m, e + 84, 0x200000u);
    if (!m.r16(a + 132)) {
        m.w32(e + 36, 0);
        m.w8(a + 100, 2);
    }
}

void Game::st_split(u32 i) {
    cover(0x8004A7E8u);
    Frame f(*this, 32);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    const u32 fp = limits(m, i);
    switch (m.r8(a + 100)) {
    case 0:
        m.w8(a + 97, 0);
        if (!turn_level(i)) {
            m.w8(a + 97, 9);
            m.w32(e + 40, 0);
            m.w32(e + 36, 0);
            m.w32(e + 32, 0);
            m.w8(a + 100, 1);
            m.w16(a + 132, m.r16(a + 124));
        }
        break;
    case 1:
        if (!m.r16(a + 132)) m.w8(a + 100, 2);
        break;
    case 2: {
        const s32 vx = m.r32s(e + 56);
        if (absv(vx) > 40960) m.w32(e + 56, static_cast<u32>(vx >> 1));
        const s32 vz = m.r32s(e + 64);
        if (absv(vz) > 40960) m.w32(e + 64, static_cast<u32>(vz >> 1));
        s32 step = 2 * m.r16s(fp + 26);
        if (m.r16s(a + 120) < step) step = m.r16s(a + 120);
        m.w16(e + 24, m.r16(e + 24) + static_cast<u32>(step));
        const u32 left = m.r16(a + 120) - static_cast<u32>(step);
        m.w16(a + 120, left);
        if (!(left & 0xFFFFu)) m.w8(a + 100, 3);
        break;
    }
    case 3: {
        m.w8(a + 97, 10);
        const s32 p = m.r16s(e + 24);
        if (p <= 0 || absv(m.r16s(e + 28)) >= 4096) {
            if (m.r16s(e + 24) >= -4095) m.w16(e + 24, m.r16(e + 24) - m.r16(fp + 26));
        } else if (m.r16s(fp + 26) < p) {
            m.w16(e + 24, m.r16(e + 24) - m.r16(fp + 26));
        }
        if (m.r32s(e + 40) >= m.r32s(fp + 12)) {
            if (!m.r16(a + 122) && turn_level(i)) return;
            m.w8(a + 100, 4);
        }
        break;
    }
    case 4: m.w8(a + 99, 255); break;
    default: m.w8(a + 100, 0); break;
    }
}

void Game::st_circle(u32 i) {
    cover(0x8004AA3Cu);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    s32 period = 7200, sink = 0x4000;
    if (m.r16(m.r32(g::kScene) + 908) == 26) {
        period = 2700;
        sink = 20480;
    }
    const u32 ph = m.r8(a + 100);
    m.w8(a + 97, 9);
    if (ph == 1) {
        const s32 k = wrap::div(m.r16s(a + 102) << 15, period).lo & 0x3FFF;
        const s32 z = -212025344 - (cos_at(m, k) << 11);
        m.w32(e + 8, static_cast<u32>(-37879808 - (sin_at(m, k) << 11)));
        m.w32(e + 16, static_cast<u32>(z));
        const s32 t = m.r16s(a + 102);
        if (t != period) {
            m.w32(e + 12, static_cast<u32>(-10903552 - sink * (period - t)));
        } else {
            m.w32(e + 12, static_cast<u32>(-10903552));
            m.w8(a + 100, 2);
        }
        m.w16(e + 26, static_cast<u32>(k - 4096));
        const s32 left = period - m.r16s(a + 102);
        if (left < m.r16s(e + 28)) m.w16(e + 28, static_cast<u32>(left));
        return;
    }
    if (ph >= 2) {
        if (ph == 2) {
            const s32 y = m.r32s(e + 12);
            const s32 run = 304 - (m.r16s(a + 102) - period);
            m.w32(e + 8, m.r32(e + 8) - static_cast<u32>(run << 9));
            if (y > -4612096) m.w32(e + 12, static_cast<u32>(-4612096));
            else if (y < -4612096) m.w32(e + 12, static_cast<u32>(y + 0x8000));
            m.w32(e + 16, static_cast<u32>(-245579776));
            if (!run) m.w8(a + 100, 3);
        } else if (ph != 3) {
            m.w8(a + 100, 0);
        }
        return;
    }
    m.w32(e + 8, static_cast<u32>(-37879808));
    m.w32(e + 16, static_cast<u32>(-245579776));
    m.w16(e + 24, 0);
    m.w16(e + 26, 0);
    m.w16(e + 28, 1024);
    for (u32 k : {56u, 60u, 64u, 32u, 36u, 40u}) m.w32(e + k, 0);
    m.w32(e + 12, static_cast<u32>(-10903552 - sink * period));
    m.w8(a + 100, 1);
}

void Game::st_fall(u32 i) {
    cover(0x8004AC90u);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    const u32 fp = limits(m, i);
    const u32 ph = m.r8(a + 100);
    m.w8(a + 97, 9);
    if (ph == 1) {
        const s32 vy = m.r32s(e + 60);
        if (-m.r32s(fp + 4) < vy) m.w32(e + 60, static_cast<u32>(vy - m.r32s(fp + 16)));
        if (m.r32s(e + 12) < -419430400) m.w8(a + 100, 2);
        return;
    }
    if (ph >= 2) {
        if (ph == 2) m.w32(e + 60, 0);
        else end_or_restart(m, a, ph, 3);
        return;
    }
    for (u32 k : {56u, 60u, 64u, 32u, 36u, 40u}) m.w32(e + k, 0);
    m.w8(a + 100, 1);
}

void Game::st_ghost(u32 i) {
    cover(0x8004AD98u);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    const u32 pl = slot(0);
    const u32 ph = m.r8(a + 100);
    m.w8(a + 97, 9);
    if (ph == 1) {
        const u32 k = m.r32(m.r32(g::kScene) + 36) & 0x3Fu;
        const u32 rec = kGhostTrack + 24 * k;
        for (u32 w = 0; w < 24; w += 4) m.w32(e + 8 + w, m.r32(rec + w));
        if (!m.r8(kGhostLive + k)) m.w16(e + 78, 0);
        for (u32 w = 0; w < 24; w += 4) m.w32(rec + w, m.r32(pl + 8 + w));
        m.w8(kGhostLive + k, m.r16s(pl + 78) > 0 ? 1 : 0);
        for (u32 w = 0; w < 16; w += 4) m.w32(e + 56 + w, m.r32(pl + 56 + w));
        return;
    }
    if (ph >= 2) {
        end_or_restart(m, a, ph, 2);
        return;
    }
    for (u32 k = 0; k < 64; ++k) {
        const u32 rec = kGhostTrack + 24 * k;
        const u32 back = 64 - k;
        m.w32(rec, m.r32(pl + 8) - m.r32(pl + 56) * back);
        m.w32(rec + 4, m.r32(pl + 12) - m.r32(pl + 60) * back);
        m.w32(rec + 8, m.r32(pl + 16) - m.r32(pl + 64) * back);
        m.w32(rec + 16, m.r32(pl + 24));
        m.w32(rec + 20, m.r32(pl + 28));
        m.w8(kGhostLive + k, 1);
    }
    m.w8(a + 100, 1);
}

void Game::st_level_band(u32 i) {
    cover(0x8004AFACu);
    Frame f(*this, 56);
    spill(m, f.at(0x20), i);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    const u32 sk = m.r32(srec(m, i) + 24);
    const u32 ph = m.r8(a + 100);
    const s32 bottom = m.r32s(sk + 8), top = m.r32s(sk + 4);
    if (ph == 1) {
        const s32 y = m.r32s(e + 12);
        const u32 goal = f.at(0x10);
        if (bottom < y || y < top) {
            m.w32(goal + 8, 0);
            m.w32(goal, 0);
            m.w32(goal + 4, static_cast<u32>(bottom < y ? top : bottom));
            climb_toward(i, goal);
            return;
        }
        m.w8(a + 100, 2);
        return;
    }
    if (ph >= 2) {
        end_or_restart(m, a, ph, 2);
        return;
    }
    if (level_off(i)) return;
    const s32 y = m.r32s(e + 12);
    m.w8(a + 100, (bottom < y || y < top) ? 1 : 2);
}

void Game::st_turn_back(u32 i) {
    cover(0x8004B108u);
    Frame f(*this, 48);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    const u32 ph = m.r8(a + 100);
    if (ph == 1) {
        if (!m.r16(a + 132)) {
            m.w8(a + 100, 2);
            m.w16(a + 128, m.r16(a + 120));
        }
        m.w8(a + 97, 2);
        return;
    }
    if (ph == 0) {
        s32 x = m.r32s(e + 8) >> 12, z = m.r32s(e + 16) >> 12;
        if (sea_mission(mission(m))) {
            x += 57344;
            z += 0x2000;
        }
        m.w32(f.at(0x10), static_cast<u32>(x));
        m.w32(f.at(0x18), static_cast<u32>(z));
        const s32 d = angle_diff(ratan2(x, z) - m.r16(e + 26) - 0x2000);
        m.w16(a + 120, static_cast<u32>(d));
        m.w16(a + 130, 0);
        m.w16(a + 128, 0);
        if (d > 0) m.w16(a + 132, static_cast<u32>(4096 - m.r16(e + 28)));
        else if (d < 0) m.w16(a + 132, static_cast<u32>(-4096 - m.r16(e + 28)));
        else m.w16(a + 132, 0);
        const u32 r = m.r16(a + 132) & 0x3FFFu;
        m.w16(a + 132, r);
        if (r >= 0x2000u) m.w16(a + 132, r - 0x4000u);
        m.w8(a + 100, 1);
        m.w8(a + 97, 2);
        return;
    }
    if (ph == 2) {
        m.w8(a + 97, 2);
        if (!m.r16(a + 128)) {
            m.w8(a + 100, 3);
            m.w8(a + 97, 0);
        }
    } else if (ph == 3) {
        m.w8(a + 97, 0);
        m.w8(a + 99, 255);
    } else {
        m.w8(a + 100, 0);
    }
}

void Game::st_explode(u32 i) {
    cover(0x8004B308u);
    Frame f(*this, 24);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    const u32 ph = m.r8(a + 100);
    if (ph == 1) {
        m.w8(a + 100, 2);
    } else if (ph >= 2) {
        if (ph == 2) entity_destroy(i);
        else m.w8(a + 100, 0);
    } else {
        m.w8(a + 100, 1);
        explosion_fall(e + 8, e + 56, m.r16s(e + 72) == 44 ? 1 : 0);
    }
}

void Game::st_crash_smoke(u32 i) {
    cover(0x8004B3C8u);
    Frame f(*this, 40);
    spill(m, f.at(0x10), i);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    const u32 ph = m.r8(a + 100);
    if (ph == 1) {
        smoke(e + 8, e + 56, m.r16(a + 102), 1);
        if (m.r16s(a + 102) >= static_cast<s32>((m.r32(e + 8) & 0x1Fu) + 16)) {
            explosion_fall(e + 8, e + 56, 0);
            sound_at(27, e + 8, 0);
            m.w8(a + 100, 2);
        }
    } else if (ph >= 2) {
        if (ph == 2) entity_destroy(i);
        else m.w8(a + 100, 0);
    } else {
        m.w8(a + 100, 1);
    }
}

void Game::st_crash_spin(u32 i) {
    cover(0x8004B4D8u);
    Frame f(*this, 32);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    const u32 ph = m.r8(a + 100);
    if (ph == 1) {
        m.w32(e + 60, m.r32(e + 60) + 1024);
        set_bits(m, e + 84, 0x800000u);
        smoke(e + 8, e + 56, m.r16(a + 102), 0);
        const s32 floor = sea_mission(mission(m)) ? 0 : -8388608;
        if (floor < m.r32s(e + 12)) {
            explosion_fall(e + 8, e + 56, 0);
            sound_at(27, e + 8, 0);
            m.w8(a + 100, 2);
        }
        return;
    }
    if (ph >= 2) {
        if (ph == 2) entity_destroy(i);
        else m.w8(a + 100, 0);
        return;
    }
    for (u32 k : {132u, 130u, 128u, 8u, 6u, 4u}) m.w16(a + k, 0);
    const u32 q = m.r32(e + 84);
    m.w32(e + 40, 0);
    m.w32(e + 36, 0);
    m.w32(e + 32, 0);
    m.w16(e + 48, 0);
    m.w16(e + 50, 0);
    m.w16(e + 52, 0);
    m.w32(e + 84, q | 0x800000u);
    const s32 r = static_cast<s16>(rand());
    switch (m.r32(m.r32(g::kWorld) + 12) & 3u) {
    case 0: m.w16(e + 52, static_cast<u32>((r & 0x3FF) - 512)); break;
    case 1: m.w16(e + 50, static_cast<u32>((r & 0xFF) - 128)); break;
    case 2: m.w16(e + 48, static_cast<u32>((r & 0xFF) - 128)); break;
    default: m.w16(e + 48, static_cast<u32>(-16)); break;
    }
    m.w8(a + 100, 1);
}

void Game::st_crash_dive(u32 i) {
    cover(0x8004B6E4u);
    Frame f(*this, 40);
    spill(m, f.at(0x10), i);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    const u32 ph = m.r8(a + 100);
    if (ph != 1) {
        if (ph >= 2) {
            if (ph == 2) entity_destroy(i);
            else m.w8(a + 100, 0);
            return;
        }
        for (u32 k : {132u, 130u, 128u, 8u, 6u, 4u}) m.w16(a + k, 0);
        m.w32(e + 40, 0);
        m.w32(e + 36, 0);
        m.w32(e + 32, 0);
        m.w8(a + 100, 1);
        return;
    }
    const s32 roll = m.r16s(e + 28);
    const u32 roll_u = m.r16(e + 28);
    if (roll >= 0 && roll_u < 0x400u) {
        m.w16(e + 28, roll_u + 4);
    } else if (roll < 0 && roll >= -1023) {
        m.w16(e + 28, roll_u - 4);
    } else {
        const s32 pitch = m.r16s(e + 24);
        if (pitch >= -2047 && pitch < 0x800) m.w16(e + 24, m.r16(e + 24) - 4u);
    }
    const u32 vy = m.r32(e + 60) + 669;
    set_bits(m, e + 84, 0x800000u);
    m.w32(e + 60, vy);
    smoke(e + 8, e + 56, m.r16(a + 102), 0);
    if (m.r32s(e + 12) > -8388608) {
        explosion_fall(e + 8, e + 56, 0);
        sound_at(27, e + 8, 0);
        m.w8(a + 100, 2);
    }
}

void Game::st_park(u32 i) {
    cover(0x8004A018u);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    const u32 ph = m.r8(a + 100);
    if (ph == 1) return;
    if (ph >= 2) {
        end_or_restart(m, a, ph, 2);
        return;
    }
    m.w8(a + 100, 1);
    for (u32 k : {56u, 60u, 64u, 32u, 36u, 40u}) m.w32(e + k, 0);
    zero_turns(m, a);
}

void Game::st_taxi(u32 i) {
    cover(0x8004A0C0u);
    Frame f(*this, 32);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    const u32 ph = m.r8(a + 100);
    if (ph == 1) {
        const s32 h = ratan2((m.r32s(a + 104) - m.r32s(e + 8)) >> 12, (m.r32s(a + 112) - m.r32s(e + 16)) >> 12);
        const u32 yaw = m.r16(e + 26);
        const s32 d = angle_diff(h - static_cast<s32>(yaw));
        u32 ny;
        if (d >= 65) ny = yaw + 64;
        else if (d >= -64) ny = yaw + static_cast<u32>(d);
        else ny = yaw - 64;
        m.w16(e + 26, ny);
        if (!d) m.w8(a + 100, 2);
        return;
    }
    if (ph >= 2) {
        if (ph == 2) {
            const u32 yaw = m.r16(e + 26) & 0x3FFFu;
            const s32 x = m.r32s(e + 8) + ((5 * sin_at(m, static_cast<s32>(yaw))) >> 2);
            const s32 z0 = m.r32s(e + 16);
            const s32 dz = (5 * cos_at(m, static_cast<s32>(yaw))) >> 2;
            m.w32(e + 8, static_cast<u32>(x));
            const s32 z = z0 + dz;
            m.w32(e + 16, static_cast<u32>(z));
            const s32 gx = m.r32s(a + 104);
            if (absv((gx - x) >> 12) < 5 && absv((m.r32s(a + 112) - z) >> 12) < 5) {
                m.w32(e + 8, static_cast<u32>(gx));
                m.w32(e + 16, m.r32(a + 112));
                m.w8(a + 100, 3);
            }
        } else {
            end_or_restart(m, a, ph, 3);
        }
        return;
    }
    m.w8(a + 100, 1);
    for (u32 k : {56u, 60u, 64u, 32u, 36u, 40u}) m.w32(e + k, 0);
    zero_turns(m, a);
}

void Game::st_takeoff(u32 i) {
    cover(0x8004B88Cu);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    const u32 fp = limits(m, i);
    const u32 ph = m.r8(a + 100);
    if (ph == 1) {
        const u32 v = m.r32(e + 40) + m.r32(a + 20);
        m.w32(e + 40, v);
        if (static_cast<s32>(v) >= m.r32s(fp + 12)) {
            m.w8(a + 100, 2);
            m.w16(a + 128, 1024);
        }
    } else if (ph >= 2) {
        if (ph == 2) {
            if (m.r32s(e + 12) < m.r32s(m.r32(srec(m, i) + 24) + 8)) m.w8(a + 100, 3);
        } else if (ph == 3) {
            m.w8(a + 99, 255);
        } else {
            m.w8(a + 100, 0);
        }
    } else {
        for (u32 k : {64u, 60u, 56u, 40u, 36u, 32u}) m.w32(e + k, 0);
        const s32 top = m.r32s(fp + 12);
        const s32 n = wrap::div(2 * m.r32s(a + 112), top).lo;
        const s32 accel = wrap::div(top, n).lo;
        m.w8(a + 100, 1);
        m.w32(a + 20, static_cast<u32>(accel));
    }
    set_bits(m, e + 84, 0x200000u);
}

u32 Game::turn_level(u32 i) {
    cover(0x80044034u);
    const u32 e = slot(i);
    const u32 a = ai_rec(m, i);
    const u32 roll = (0u - m.r16(e + 28)) & 0xFFFFu;
    m.w16(a + 132, roll);
    if (roll) return 1;
    const u32 pitch = m.r16(e + 24);
    m.w16(a + 128, 0u - pitch);
    return pitch ? 1 : 0;
}

u32 Game::level_off(u32 i) {
    cover(0x8004409Cu);
    const u32 e = slot(i);
    const u32 fp = limits(m, i);
    const u32 a = ai_rec(m, i);
    zero_turns(m, a);
    s32 d = -m.r16s(e + 28);
    if (m.r16(e + 28)) {
        const s32 rate = m.r16s(fp + 24);
        if (rate < d) d = rate;
        if (d < -rate) d = -rate;
        m.w16(e + 28, m.r16(e + 28) + static_cast<u32>(d));
        return 1;
    }
    d = -m.r16s(e + 24);
    if (!m.r16(e + 24)) return 0;
    if (m.r16s(fp + 26) < d) d = m.r16s(fp + 26);
    if (d < m.r16s(fp + 28)) d = m.r16s(fp + 28);
    m.w16(e + 24, m.r16(e + 24) + static_cast<u32>(d));
    return 1;
}

u32 Game::turn_toward(u32 i, u32 vec, u32 len, u32 out) {
    cover(0x80044188u);
    Frame f(*this, 40);
    const s32 n = static_cast<s16>(len);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    if (n < 128) return 0;
    m.w16(out, static_cast<u32>(ratan2(-m.r32s(vec + 4), m.r32s(vec + 8))));
    m.w16(out + 2, static_cast<u32>(ratan2(m.r32s(vec), m.r32s(vec + 8))));
    if (n < 0x300) {
        m.w16(out + 4, 0u - m.r16(e + 28));
    } else if (n >= 0x1FFF) {
        m.w16(out + 4, 0);
    } else {
        const s32 r = ratan2(m.r32s(vec), -m.r32s(vec + 4));
        m.w16(out + 4, static_cast<u32>(r));
        const s32 p = m.r16s(out);
        if (p < 0 && p >= -1023) {
            const s32 y = m.r16s(out + 2);
            if (y < 0x200 && y >= -511) m.w16(out + 4, static_cast<u32>(r + 0x2000));
        }
        m.w16(out + 4, m.r16(out + 4) & 0x3FFFu);
        const bool low = m.r16s(out + 4) < 0x2000;
        m.w16(out, m.r16(out) & 0x3FFFu);
        if (!low) m.w16(out + 4, m.r16(out + 4) - 0x4000u);
        if (m.r16s(out) >= 0x2000) m.w16(out, m.r16(out) - 0x4000u);
    }
    m.w16(a + 132, m.r16(out + 4));
    if (((m.r16(out + 4) + 0x7Fu) & 0xFFFFu) < 0xFFu) {
        m.w16(a + 128, m.r16(out));
        if (((m.r16(out) + 0x7Fu) & 0xFFFFu) < 0xFFu) m.w16(a + 130, m.r16(out + 2));
    }
    return 1;
}

u32 Game::climb_toward(u32 i, u32 goal) {
    cover(0x80044360u);
    Frame f(*this, 32);
    const u32 e = slot(i);
    const u32 a = ai_rec(m, i);
    const u32 roll = (0u - m.r16(e + 28)) & 0xFFFFu;
    m.w16(a + 132, roll);
    if (roll) return 1;
    s32 p = ratan2(-((m.r32s(goal + 4) - m.r32s(e + 12)) >> 12), 0x800);
    s32 lim = m.r16s(limits(m, i) + 22);
    if (!lim) lim = 0xE38;
    if (lim < p) p = lim;
    if (p < -lim) p = -lim;
    if (static_cast<u32>(p - 1) < 7u) p = 8;
    if (p < 0 && p >= -7) p = -8;
    m.w16(a + 128, static_cast<u32>(p) - m.r16(e + 24));
    return 1;
}

u32 Game::heading_toward(u32 i, u32 goal) {
    cover(0x80044474u);
    Frame f(*this, 40);
    const u32 fp = limits(m, i);
    u32 bank = m.r16(fp + 20);
    const u32 e = slot(i);
    const u32 a = ai_rec(m, i);
    if (m.r16s(fp + 20) >= 4097) bank = 4096;
    if (!bank) bank = 4096;
    const s32 h = ratan2((m.r32s(goal) - m.r32s(e + 8)) >> 12, (m.r32s(goal + 8) - m.r32s(e + 16)) >> 12);
    const u32 raw = static_cast<u32>(h - m.r16(e + 26)) & 0x3FFFu;
    if (!raw) return 0;
    u32 d = raw >= 0x2000u ? (raw | 0xC000u) : raw;
    const s32 ds = static_cast<s16>(d);
    const s32 bs = static_cast<s16>(bank);
    if (ds < -bs) d = 0u - bank;
    else if (bs < ds) d = bank;
    m.w16(a + 132, d - m.r16(e + 28));
    u32 turn = static_cast<u32>((m.r16s(fp + 26) * m.r16s(e + 28)) >> 12);
    const s32 ts = static_cast<s16>(turn);
    if (absv(ts) < m.r16s(fp + 30)) {
        turn = m.r16(fp + 30);
        if (ts < 0) turn = 0u - m.r16(fp + 30);
    }
    if (absv(static_cast<s16>(d)) < absv(static_cast<s16>(turn))) turn = d;
    m.w16(e + 26, m.r16(e + 26) + turn);
    return 1;
}

void Game::ai_steer(u32 i) {
    cover(0x80044A6Cu);
    Frame f(*this, 32);
    const u32 e = slot(i);
    const u32 a = ai_rec(m, i);
    const u32 rel = m.r32(g::kRelation + 4 * i);
    if (!rel || m.r32s(rel + 4) >= 0x4000) {
        m.w8(e + 5, m.r8(e + 5) & ~8u);
    } else {
        if (m.r16s(a + 128) >= 2048) m.w8(e + 5, m.r8(e + 5) | 8u);
        if (absv(m.r16s(a + 132)) >= 4096) m.w8(e + 5, m.r8(e + 5) | 8u);
        if (!m.r16(a + 128) && !m.r16(a + 132)) m.w8(e + 5, m.r8(e + 5) & ~8u);
    }
    const s32 p = turn_pitch(i, m.r16s(a + 128));
    if (p) {
        m.w16(e + 48, static_cast<u32>(p));
        set_bits(m, e + 84, 0x800000u);
        m.w16(a + 128, m.r16(a + 128) - static_cast<u32>(p));
    }
    const s32 r = turn_roll(i, m.r16s(a + 132));
    if (r) {
        m.w16(e + 28, m.r16(e + 28) + static_cast<u32>(r));
        m.w16(a + 132, m.r16(a + 132) - static_cast<u32>(r));
    }
    const s32 y = turn_yaw(i, m.r16s(a + 130));
    if (y) {
        m.w16(e + 50, static_cast<u32>(y));
        set_bits(m, e + 84, 0x800000u);
        m.w16(a + 130, m.r16(a + 130) - static_cast<u32>(y));
    }
    angles_wrap(e + 24);
}

void Game::angles_wrap(u32 p) {
    cover(0x8004463Cu);
    if (m.r16s(p) >= 4097) {
        m.w16(p, 0x2000u - m.r16(p));
        const u32 r = m.r16(p + 4) + 0x2000u;
        m.w16(p + 2, m.r16(p + 2) + 0x2000u);
        m.w16(p + 4, r);
    }
    if (m.r16s(p) < -4096) {
        m.w16(p, static_cast<u32>(-8192 - m.r16s(p)));
        const u32 r = m.r16(p + 4) + 0x2000u;
        m.w16(p + 2, m.r16(p + 2) + 0x2000u);
        m.w16(p + 4, r);
    }
    for (u32 k : {2u, 4u}) {
        if (m.r16s(p + k) >= 0x2000) m.w16(p + k, m.r16(p + k) - 0x4000u);
        if (m.r16s(p + k) < -8192) m.w16(p + k, m.r16(p + k) + 0x4000u);
    }
}

s32 Game::turn_pitch(u32 i, s32 v) {
    cover(0x80044884u);
    const u32 a = ai_rec(m, i);
    const u32 r = srec(m, i);
    const u32 fp = m.r32(r + 20);
    const u32 reaction = m.r8(m.r32(r + 24) + 1);
    const u32 slow = m.r32(m.r32(g::kWorld) + 12) & 3u;
    const u32 cur = m.r16(a + 4);
    const s32 c = static_cast<s16>(cur);
    if (v >= 0) {
        bool store = true;
        s32 nv = c >> 1;
        if (c >= -1) {
            nv = v >> 1;
            if (2 * c < v) {
                if (v >= m.r16s(fp + 26)) v = m.r16s(fp + 26);
                if (reaction < slow) store = false;
                nv = static_cast<s32>(cur) + 1 + (v >> 4);
            }
        }
        if (store) m.w16(a + 4, static_cast<u32>(nv));
        if (m.r16s(fp + 26) < m.r16s(a + 4)) m.w16(a + 4, m.r16(fp + 26));
    } else {
        bool store = true;
        s32 nv;
        if (c <= 0) {
            nv = v >> 1;
            if (v < 2 * c) {
                if (m.r16s(fp + 28) >= v) v = m.r16s(fp + 28);
                if (reaction < slow) store = false;
                nv = static_cast<s32>(cur) + (v >> 4);
            }
        } else {
            nv = c >> 1;
        }
        if (store) m.w16(a + 4, static_cast<u32>(nv));
        if (m.r16s(a + 4) < m.r16s(fp + 28)) m.w16(a + 4, m.r16(fp + 28));
    }
    return m.r16s(a + 4);
}

s32 Game::turn_roll(u32 i, s32 v) {
    cover(0x8004470Cu);
    const u32 r = srec(m, i);
    const s32 q = m.r8(m.r32(r + 24) + 1) + 1;
    const u32 a = ai_rec(m, i);
    const s32 base = m.r16s(m.r32(r + 20) + 24);
    const s32 lim_raw = (3 * base + ((base * q) >> 3)) >> 2;
    const s32 lim = static_cast<s16>(lim_raw);
    const u32 cur = m.r16(a + 8);
    const s32 c = static_cast<s16>(cur);
    if (v < 0) {
        s32 nv;
        if (c <= 0) {
            if (v < 2 * c) {
                const s32 t = -lim;
                nv = static_cast<s32>(cur) + (((t < v) ? v : t) * q >> 8);
            } else {
                nv = v >> 1;
            }
        } else {
            nv = c >> 1;
        }
        m.w16(a + 8, static_cast<u32>(nv));
        if (m.r16s(a + 8) < -lim) m.w16(a + 8, static_cast<u32>(-lim_raw));
    } else {
        s32 nv = c >> 1;
        if (c >= -1) {
            if (2 * c < v) nv = static_cast<s32>(cur) + 1 + (((v < lim) ? v : lim) * q >> 8);
            else nv = v >> 1;
        }
        m.w16(a + 8, static_cast<u32>(nv));
        if (lim < m.r16s(a + 8)) m.w16(a + 8, static_cast<u32>(lim_raw));
    }
    return m.r16s(a + 8);
}

s32 Game::turn_yaw(u32 i, s32 v) {
    cover(0x800449DCu);
    const u32 a = ai_rec(m, i);
    const u32 fp = limits(m, i);
    const s32 lim = m.r16s(fp + 30);
    if (!v) m.w16(a + 6, 0);
    else if (v <= 0 && -lim >= v) m.w16(a + 6, static_cast<u32>(-lim));
    else if (v > 0 && v >= lim) m.w16(a + 6, static_cast<u32>(lim));
    else m.w16(a + 6, static_cast<u32>(v));
    return m.r16s(a + 6);
}

u32 Game::ai_speed(u32 i) {
    cover(0x80044C28u);
    Frame f(*this, 56);
    spill(m, f.at(0x20), i);
    const u32 a = ai_rec(m, i);
    const u32 e = slot(i);
    const u32 r = srec(m, i);
    const u32 fp = m.r32(r + 20);
    if (!m.r32(g::kRelation + 4 * i)) return 1;
    s32 accel = m.r32s(fp + 16);
    s32 step = m.r8(m.r32(r + 24) + 1) + 1;
    speed_limits(m.r32s(e + 12), f.at(0x10), f.at(0x14), f.at(0x18));
    s32 lowest = m.r32s(fp + 12);
    const s32 top = m.r32s(fp + 8);
    s32 want = m.r32s(fp + 4);
    const s32 cruise_here = m.r32s(f.at(0x14));
    auto other_speed = [&] { return m.r32s(slot(m.r8(a + 63)) + 40); };
    auto offset = [&](s32 base) {
        s32 d, s;
        if (m.r32s(a + 148) >= 0) {
            d = m.r32s(a + 136);
            s = d;
        } else {
            s = m.r32s(a + 136);
            d = -s;
        }
        if (s >= 16385) d *= 8;
        return base + d;
    };
    switch (m.r8(a + 97)) {
    case 0: want = m.r32s(fp + 4); break;
    case 1: want = m.r32s(fp + 8); break;
    case 2: want = m.r32s(fp + 12); break;
    case 3: want = cruise_here; break;
    case 4:
        if (m.r8(a + 63) && m.r8(a + 96)) {
            if (!(m.r32(e) & 0x40u) || m.r32s(a + 148) < 0) want = m.r32s(fp + 4);
            else want = other_speed() + 8 * m.r32s(a + 136);
        } else if (m.r32s(a + 148) < 0) {
            want = cruise_here;
        } else {
            want = cruise_here + 8 * m.r32s(a + 136);
        }
        break;
    case 5:
        accel = 256;
        step = 8;
        want = offset(m.r8(a + 63) ? other_speed() : cruise_here);
        break;
    case 6:
        if (m.r32s(a + 148) < 0) want = m.r32s(fp + 4);
        else want = other_speed() + m.r32s(a + 136);
        break;
    case 7: want = offset(m.r8(a + 63) ? other_speed() : cruise_here); break;
    case 8:
        accel = 256;
        step = 8;
        want = cruise_here - (cruise_here >> 3);
        break;
    case 9: return 0;
    case 10:
        want = m.r32s(fp + 12);
        lowest = 0;
        accel = 256;
        step = 8;
        break;
    default: break;
    }
    if (top < want) want = top;
    else if (want < lowest) want = lowest;
    s32 d = m.r32s(a + 20);
    const s32 gap = want - m.r32s(e + 40);
    if (d < gap) {
        m.w32(a + 20, static_cast<u32>(d + step));
        d = m.r32s(a + 20);
    } else if (gap < d) {
        m.w32(a + 20, static_cast<u32>(d - step));
        d = m.r32s(a + 20);
    }
    if (accel < d) m.w32(a + 20, static_cast<u32>(accel));
    else if (d < -accel) m.w32(a + 20, static_cast<u32>(-accel));
    const s32 v = m.r32s(e + 40);
    set_bits(m, e + 84, 0x200000u);
    const s32 nv = v + m.r32s(a + 20);
    m.w32(e + 40, static_cast<u32>(nv));
    if (top < nv) m.w32(e + 40, static_cast<u32>(top));
    else if (nv < lowest) m.w32(e + 40, static_cast<u32>(lowest));
    return 0;
}
}

#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
u32 scene_clock(Memory& m) { return m.r32(m.r32(g::kScene) + 0x24); }
}

u32 Game::replay_shot(u32 fn, u32 args, u32 s, s32 len) {
    switch (fn) {
    case 0x8007A0F0u: return shot_ahead(args, s, len);
    case 0x8007A260u: return shot_flyby(args, s, len);
    case 0x8007A3B4u: return shot_crane(args, s, len);
    case 0x8007A52Cu: return shot_pass(args, s, len);
    case 0x8007A6CCu: return shot_orbit(args, s, len);
    case 0x8007A8C4u: return shot_chase(args, s, len);
    case 0x8007AAB4u: return shot_side(args, s, len);
    case 0x8007AC50u: return shot_sweep(args, s, len);
    default: return call_by_address(fn, "replay camera shot", args, s, static_cast<u32>(len));
    }
}

void Game::vector_angles(u32 v, u32 out) {
    cover(0x80031CD8u);
    Frame f(*this, 0x28);
    const s32 x = m.r32s(v) >> 12, y = m.r32s(v + 4) >> 12, z = m.r32s(v + 8) >> 12;
    const s32 sum = (x < 0 ? -x : x) + (z < 0 ? -z : z);
    s32 h;
    if (0xAFFFF < sum) {
        h = 0x100000;
    } else {
        s32 hx = x, hz = z;
        u32 sh = 0;
        if (0xB000 < sum) {
            hx = m.r32s(v) >> 16;
            hz = m.r32s(v + 8) >> 16;
            sh = 4;
        }
        h = static_cast<s32>(wrap::shl(static_cast<u32>(length3(hx, 0, hz)), sh));
    }
    s32 pitch = ratan2(wrap::neg(y), h);
    s32 yaw = ratan2(x, z);
    if (pitch >= 4097) {
        pitch = 0x2000 - pitch;
        yaw += 0x2000;
    }
    if (pitch < -0x1000) {
        pitch = -0x2000 - pitch;
        yaw += 0x2000;
    }
    if (yaw >= 0x2000) yaw -= 0x4000;
    if (yaw < -0x2000) yaw += 0x4000;
    m.w16(out, static_cast<u32>(pitch));
    m.w16(out + 2, static_cast<u32>(yaw));
    m.w16(out + 4, 0);
}

void Game::shot_lead_point(u32 args, s32 t, u32 out) {
    cover(0x8007995Cu);
    const u32 vel = m.r32(args + 8);
    gte.mtc2(8, static_cast<u32>(t));
    gte.mtc2(9, static_cast<u32>(m.r32s(vel) >> 5));
    gte.mtc2(10, static_cast<u32>(m.r32s(vel + 4) >> 5));
    gte.mtc2(11, static_cast<u32>(m.r32s(vel + 8) >> 5));
    gte.op(0x190003D);
    const u32 o = m.r32(args);
    m.w32(out + 4, gte.mfc2(26) + m.r32(o + 4));
    m.w32(out + 8, gte.mfc2(27) + m.r32(o + 8));
    m.w32(out, gte.mfc2(25) + m.r32(o));
}

void Game::shot_heading_point(u32 args, s32 d, s32 t, u32 out) {
    cover(0x800799D4u);
    Frame f(*this, 0x60);
    const u32 o = m.r32(args);
    const u32 rot = f.at(0x20), v = f.at(0x40), mac = f.at(0x10);
    rot_matrix_yxz(o + 16, rot);
    for (u32 k = 0; k < 5; ++k) gte.ctc2(k, m.r32(rot + 4 * k));
    m.w16(v, 0);
    m.w16(v + 2, 0);
    m.w16(v + 4, static_cast<u32>(d >> 5));
    gte.mtc2(0, m.r32(v));
    gte.mtc2(1, m.r32(v + 4));
    gte.op(0x486012);
    m.w32(mac, gte.mfc2(25));
    m.w32(mac + 4, gte.mfc2(26));
    m.w32(mac + 8, gte.mfc2(27));
    gte.mtc2(8, static_cast<u32>(t));
    gte.mtc2(9, m.r32(mac));
    gte.mtc2(10, m.r32(mac + 4));
    gte.mtc2(11, m.r32(mac + 8));
    gte.op(0x190003D);
    m.w32(out, gte.mfc2(25) + m.r32(o));
    m.w32(out + 4, gte.mfc2(26) + m.r32(o + 4));
    m.w32(out + 8, gte.mfc2(27) + m.r32(o + 8));
}

s32 Game::shot_speed(u32 args) {
    cover(0x80079AE8u);
    Frame f(*this, 0x18);
    const u32 vel = m.r32(args + 8);
    return static_cast<s32>(static_cast<u32>(length3(m.r32s(vel), m.r32s(vel + 4), m.r32s(vel + 8))) << 3);
}

void Game::shot_offset_look(u32 args, u32 off, u32 out) {
    cover(0x80079DACu);
    Frame f(*this, 0x60);
    const u32 d = f.at(0x38), rot = f.at(0x10), v = f.at(0x30);
    const u32 a = m.r32(args), b = m.r32(args + 4);
    m.w32(d, m.r32(b) - m.r32(a));
    m.w32(d + 4, m.r32(b + 4) - m.r32(a + 4));
    m.w32(d + 8, m.r32(b + 8) - m.r32(a + 8));
    vector_angles(d, out + 16);
    rot_matrix_yxz(out + 16, rot);
    for (u32 k = 0; k < 5; ++k) gte.ctc2(k, m.r32(rot + 4 * k));
    const u32 vo = m.r32(args + 12);
    m.w16(v, static_cast<u32>(m.r16s(vo) + (m.r32s(off) >> 6)));
    m.w16(v + 2, static_cast<u32>(m.r16s(vo + 2) + (m.r32s(off + 4) >> 6)));
    m.w16(v + 4, static_cast<u32>(m.r16s(vo + 4) + (m.r32s(off + 8) >> 6)));
    gte.mtc2(0, m.r32(v));
    gte.mtc2(1, m.r32(v + 4));
    gte.op(0x486012);
    m.w32(d, gte.mfc2(25));
    m.w32(d + 4, gte.mfc2(26));
    m.w32(d + 8, gte.mfc2(27));
    const u32 o = m.r32(args);
    m.w32(out, (m.r32(d) << 6) + m.r32(o));
    m.w32(out + 4, (m.r32(d + 4) << 6) + m.r32(o + 4));
    m.w32(out + 8, (m.r32(d + 8) << 6) + m.r32(o + 8));
}

void Game::shot_orbit_point(u32 args, s32 r, u32 ang, u32 out) {
    cover(0x80079F2Cu);
    Frame f(*this, 0x58);
    s32 z = wrap::mul(m.r16s(m.r32(args + 12) + 4), r) >> 18;
    if (0x7FFF < z) z = 0x7FFF;
    if (z < -0x8000) z = -0x8000;
    const u32 v = f.at(0x30), rot = f.at(0x10), mac = f.at(0x38);
    m.w16(v, 0);
    m.w16(v + 2, 0);
    m.w16(v + 4, static_cast<u32>(z));
    rot_matrix_yxz(ang, rot);
    for (u32 k = 0; k < 5; ++k) gte.ctc2(k, m.r32(rot + 4 * k));
    gte.mtc2(0, m.r32(v));
    gte.mtc2(1, m.r32(v + 4));
    gte.op(0x486012);
    m.w32(mac, gte.mfc2(25));
    m.w32(mac + 4, gte.mfc2(26));
    m.w32(mac + 8, gte.mfc2(27));
    const u32 o = m.r32(args);
    m.w16(out + 16, m.r16(ang));
    m.w16(out + 18, m.r16(ang + 2));
    m.w32(out, m.r32(o) + (m.r32(mac) << 12));
    m.w32(out + 4, m.r32(o + 4) + (m.r32(mac + 4) << 12));
    m.w32(out + 8, m.r32(o + 8) + (m.r32(mac + 8) << 12));
    m.w16(out + 20, m.r16(ang + 4));
}

void Game::shot_look_at(u32 args, u32 pos, u32 out) {
    cover(0x8007A060u);
    Frame f(*this, 0x30);
    const u32 d = f.at(0x10);
    const u32 o = m.r32(args);
    m.w32(d + 4, m.r32(o + 4) - m.r32(pos + 4));
    m.w32(d, m.r32(o) - m.r32(pos));
    m.w32(d + 8, m.r32(o + 8) - m.r32(pos + 8));
    vector_angles(d, out + 16);
    m.w32(out, m.r32(pos));
    m.w32(out + 4, m.r32(pos + 4));
    m.w32(out + 8, m.r32(pos + 8));
}

void Game::shot_roll(u32 args, u32 s) {
    const u32 fl = m.r32(s + 0x24);
    if (!(fl & 8u)) return;
    const u32 roll = m.r16(m.r32(args) + 0x14);
    m.w16(s + 0x18, (fl & 4u) ? roll : 0u - roll);
}

u32 Game::shot_count(u32 s, s32 len) {
    const s32 n = m.r32s(s + 0x1C) + 1;
    m.w32(s + 0x1C, static_cast<u32>(n));
    if (n < m.r32s(s + 0x20)) return 0;
    if (len > 0) return 1;
    m.w32(s, 0);
    return 0;
}

void Game::shot_offset_back(u32 args, u32 s) {
    const u32 o = m.r32(args + 12);
    m.w32(s + 4, m.r32(s + 4) - static_cast<u32>(m.r16s(o) << 6));
    m.w32(s + 8, m.r32(s + 8) - static_cast<u32>(m.r16s(o + 2) << 6));
    m.w32(s + 12, m.r32(s + 12) - static_cast<u32>(m.r16s(o + 4) << 6));
}

u32 Game::shot_ahead(u32 args, u32 s, s32 len) {
    cover(0x8007A0F0u);
    Frame f(*this, 0x20);
    const u32 st = m.r32(s);
    if (st == 1) {
        const s32 n = m.r32s(s + 0x1C) + 1;
        m.w32(s + 0x1C, static_cast<u32>(n));
        if (n < m.r32s(s + 0x20)) return 0;
        if (len > 0) return 1;
        m.w32(s, 0);
        return 0;
    }
    if (st != 0) return 0;
    m.w32(s + 0x1C, 0);
    const u32 o = m.r32(args);
    m.w16(s + 0x14, m.r16(o + 0x10));
    m.w16(s + 0x16, m.r16(o + 0x12));
    m.w16(s + 0x18, m.r16(o + 0x14));
    s32 lead;
    if (len > 0) {
        lead = len < 13 ? len >> 1 : 12;
        m.w32(s + 0x20, static_cast<u32>(len - lead));
    } else {
        lead = static_cast<s32>(scene_clock(m) & 0xFu);
        m.w32(s + 0x20, static_cast<u32>(lead + 0x32));
    }
    shot_lead_point(args, lead << 5, s + 4);
    const u32 vo = m.r32(args + 12);
    m.w32(s + 4, m.r32(s + 4) + static_cast<u32>(m.r16s(vo) << 6));
    m.w32(s + 8, m.r32(s + 8) + static_cast<u32>(m.r16s(vo + 2) << 6));
    m.w32(s, 1);
    m.w32(s + 12, m.r32(s + 12) + static_cast<u32>(m.r16s(vo + 4) << 6));
    return 0;
}

u32 Game::shot_flyby(u32 args, u32 s, s32 len) {
    cover(0x8007A260u);
    Frame f(*this, 0x28);
    const u32 st = m.r32(s);
    if (st == 0) {
        m.w32(s + 0x1C, 0);
        s32 lead;
        if (len > 0) {
            lead = len < 31 ? len >> 1 : 30;
            m.w32(s + 0x20, static_cast<u32>(len - lead));
        } else {
            lead = static_cast<s32>(scene_clock(m) & 0x1Fu) + 0x1E;
            m.w32(s + 0x20, static_cast<u32>(lead + 0x5A));
        }
        shot_lead_point(args, lead << 5, s + 4);
        const u32 vo = m.r32(args + 12);
        m.w32(s + 4, m.r32(s + 4) + static_cast<u32>(m.r16s(vo) << 5));
        m.w32(s + 8, m.r32(s + 8) + static_cast<u32>(m.r16s(vo + 2) << 5));
        m.w32(s, 1);
        m.w32(s + 12, m.r32(s + 12) + static_cast<u32>(m.r16s(vo + 4) << 5));
    } else if (st != 1) {
        return 0;
    }
    shot_look_at(args, s + 4, s + 4);
    return shot_count(s, len);
}

u32 Game::shot_crane(u32 args, u32 s, s32 len) {
    cover(0x8007A3B4u);
    const u32 st = m.r32(s);
    if (st == 0) {
        m.w32(s + 0x1C, 0);
        m.w32(s + 0x20, len > 0 ? static_cast<u32>(len) : 150u);
        m.w32(s, 1);
    } else if (st != 1) {
        return 0;
    }
    s32 q = static_cast<s32>(wrap::div(static_cast<s32>(m.r32(s + 0x1C) << 14), m.r32s(s + 0x20)).lo);
    if (q >= 4097) q = 4096;
    const u32 i = (static_cast<u32>(q) * 2 - 0x1000) & 0x3FFFu;
    const s32 k = (static_cast<s32>(static_cast<s16>(m.r16(g::kSinCos + 2 * i))) >> 3) + 0x800;
    const u32 o = m.r32(args);
    const s32 h = m.r16s(m.r32(m.r32(g::kWorld) + 0x28) + 0x28);
    s32 y = (m.r32s(o + 4) >> 12) + ((h - 1000) >> 1);
    if (y < 0) y = 0;
    if (0x7FFF < y) y = 0x7FFF;
    const s32 lift = (wrap::mul(y, k) >> 12) + 0xC8;
    m.w32(s + 4, m.r32(o));
    m.w32(s + 8, m.r32(m.r32(args) + 4) - (static_cast<u32>(lift) << 12));
    const s32 n = m.r32s(s + 0x1C) + 1;
    m.w16(s + 0x14, 0xF000);
    m.w16(s + 0x16, 0);
    m.w16(s + 0x18, 0);
    m.w32(s + 0x1C, static_cast<u32>(n));
    m.w32(s + 12, m.r32(m.r32(args) + 8));
    if (len <= 0) return 0;
    return n < m.r32s(s + 0x20) ? 0u : 1u;
}

u32 Game::shot_pass(u32 args, u32 s, s32 len) {
    cover(0x8007A52Cu);
    Frame f(*this, 0x28);
    const u32 st = m.r32(s);
    if (st == 0) {
        m.w32(s + 0x1C, 0);
        m.w32(s, 1);
        m.w32(s + 0x20, len > 0 ? static_cast<u32>(len) : 0x1C2u);
        const s32 d = shot_speed(args);
        m.w32(s + 0x28, static_cast<u32>(d));
        if (!(0x29FFF < d)) m.w32(s + 0x28, 0x2A000);
    } else if (st != 1) {
        return 0;
    }
    const s32 half = m.r32s(s + 0x20) >> 1;
    const s32 t = m.r32s(s + 0x1C) - half;
    const s32 q = static_cast<s32>(wrap::div(wrap::mul(t, 480), half).lo);
    shot_heading_point(args, m.r32s(s + 0x28), wrap::neg(q), s + 4);
    shot_offset_back(args, s);
    shot_look_at(args, s + 4, s + 4);
    return shot_count(s, len);
}

u32 Game::shot_orbit(u32 args, u32 s, s32 len) {
    cover(0x8007A6CCu);
    Frame f(*this, 0x30);
    const u32 st = m.r32(s);
    if (st == 0) {
        m.w32(s + 0x1C, 0);
        m.w32(s, 1);
        m.w32(s + 0x20, len > 0 ? static_cast<u32>(len) : 0x140u);
        m.w32(s + 0x24, m.r8(m.r32(g::kScene) + 0x24));
    } else if (st != 1) {
        return 0;
    }
    const s32 half = m.r32s(s + 0x20) >> 1;
    const s32 t = static_cast<s32>(wrap::div(static_cast<s32>(static_cast<u32>(m.r32s(s + 0x1C) - half) << 12), half).lo);
    const u32 fl = m.r32(s + 0x24);
    const u32 i = (static_cast<u32>(t < 0 ? -t : t) + 0x1000) & 0x3FFFu;
    const s32 c = m.r16s(g::kSinCos + 2 * i);
    const s32 r = wrap::mul(0x4000 - c, static_cast<s32>((fl & 3u) + 1)) + 0x1000;
    s32 pitch = 0, yaw = 0;
    if (fl & 0x80u) pitch = (fl & 0x40u) ? t : -t;
    if (fl & 0x20u) yaw = (fl & 0x10u) ? t : -t;
    const u32 ang = f.at(0x10);
    m.w16(ang, static_cast<u32>(pitch));
    m.w16(ang + 2, static_cast<u32>(yaw));
    m.w16(ang + 4, 0);
    shot_orbit_point(args, r, ang, s + 4);
    shot_roll(args, s);
    return shot_count(s, len);
}

u32 Game::shot_chase(u32 args, u32 s, s32 len) {
    cover(0x8007A8C4u);
    Frame f(*this, 0x28);
    const u32 st = m.r32(s);
    if (st == 0) {
        m.w32(s + 0x1C, 0);
        m.w32(s, 1);
        m.w32(s + 0x20, len > 0 ? static_cast<u32>(len) : 0xF0u);
        const s32 d = shot_speed(args);
        m.w32(s + 0x28, static_cast<u32>(d));
        const u32 clock = scene_clock(m);
        m.w32(s + 0x24, clock & 0xFu);
        if (clock & 1u) {
            if (m.r32s(s + 0x28) < 0x4000) m.w32(s + 0x28, 0x4000);
        } else if (!(0x30FFF < m.r32s(s + 0x28))) {
            m.w32(s + 0x28, 0x31000);
        }
    } else if (st != 1) {
        return 0;
    }
    const s32 t = (m.r32(s + 0x24) & 1u) ? 0x80 : -0x80;
    shot_heading_point(args, m.r32s(s + 0x28), t, s + 4);
    shot_offset_back(args, s);
    shot_look_at(args, s + 4, s + 4);
    shot_roll(args, s);
    const s32 n = m.r32s(s + 0x1C) + 1;
    m.w32(s + 0x1C, static_cast<u32>(n));
    if (len <= 0) return 0;
    return n < m.r32s(s + 0x20) ? 0u : 1u;
}

u32 Game::shot_side(u32 args, u32 s, s32 len) {
    cover(0x8007AAB4u);
    Frame f(*this, 0x38);
    const u32 st = m.r32(s);
    if (st == 0) {
        m.w32(s + 0x1C, 0);
        m.w32(s, 1);
        m.w32(s + 0x20, static_cast<u32>(len));
        m.w32(s + 0x28, 0);
        m.w32(s + 0x24, scene_clock(m) & 0xFu);
    } else if (st != 1) {
        return 0;
    }
    distance(m.r32(args), m.r32(args + 4));
    if (m.r32(s + 0x28)) m.w32(s + 0x28, m.r32(s + 0x28) - 1);
    const u32 o = m.r32(args);
    const s32 sr = m.r16s(g::kSinCos + 2 * (m.r16(o + 0x14) & 0x3FFFu));
    const s32 sp_ = m.r16s(g::kSinCos + 2 * (m.r16(o + 0x10) & 0x3FFFu));
    const u32 off = f.at(0x10);
    m.w32(off + 8, 0xFFFB0000u);
    m.w32(off, static_cast<u32>(sr * 10));
    m.w32(off + 4, static_cast<u32>(((sp_ * 15) >> 1) + static_cast<s32>(0xFFFEC000u)));
    shot_offset_look(args, off, s + 4);
    shot_roll(args, s);
    const s32 n = m.r32s(s + 0x1C) + 1;
    m.w32(s + 0x1C, static_cast<u32>(n));
    if (len == 0) return 0;
    return n < m.r32s(s + 0x20) ? 0u : 1u;
}

u32 Game::shot_sweep(u32 args, u32 s, s32 len) {
    cover(0x8007AC50u);
    Frame f(*this, 0x38);
    const u32 st = m.r32(s);
    if (st == 0) {
        m.w32(s + 0x1C, 0);
        m.w32(s, 1);
        m.w32(s + 0x24, scene_clock(m) & 0xFu);
        m.w32(s + 0x20, len > 0 ? static_cast<u32>(len) : 0x3Cu);
    } else if (st != 1) {
        return 0;
    }
    const s32 q = static_cast<s32>(wrap::div(static_cast<s32>(m.r32(s + 0x1C) << 12), m.r32s(s + 0x20)).lo);
    const s32 a = 0x1000 - q;
    const u32 i = (static_cast<u32>(a) * 2 - 0x1000) & 0x3FFFu;
    const s32 k = (static_cast<s32>(static_cast<s16>(m.r16(g::kSinCos + 2 * i))) >> 3) + 0x800;
    const u32 fl = m.r32(s + 0x24);
    s32 x = 0;
    if (fl & 2u) {
        const s32 w = static_cast<s32>(static_cast<u32>(k * 15) << 15);
        x = ((fl & 1u) ? w : wrap::neg(w)) >> 12;
    }
    const u32 off = f.at(0x10);
    m.w32(off + 8, 0xFFFB0000u);
    m.w32(off, static_cast<u32>(x));
    m.w32(off + 4, static_cast<u32>(static_cast<s32>(0xFFFB0000u) >> 2));
    shot_offset_look(args, off, s + 4);
    shot_roll(args, s);
    const s32 n = m.r32s(s + 0x1C);
    if (n < m.r32s(s + 0x20)) m.w32(s + 0x1C, static_cast<u32>(n + 1));
    if (len <= 0) return 0;
    return m.r32s(s + 0x1C) < m.r32s(s + 0x20) ? 0u : 1u;
}
}

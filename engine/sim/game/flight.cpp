#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kCtlStick = 0x800BD088u;
constexpr u32 kCtl = 0x800BD08Cu;
constexpr u32 kCtlSurf = 0x800BD090u;
s32 q12(s32 x) { return x / 4096; }
s32 absv(s32 v) { return v < 0 ? wrap::neg(v) : v; }
s32 clampv(s32 v, s32 lo, s32 hi) { return v < lo ? lo : hi < v ? hi : v; }
}

void Game::stick_ease(u32 stick, s32 decay, s32 t_pitch, s32 t_yaw, s32 t_roll, s32 t_thr) {
    auto ease = [&](u32 at, s32 target, s32 div) {
        const s32 v = m.r16s(at);
        if (absv(target) < absv(v)) m.w16(at, static_cast<u32>((v * 128) / div));
    };
    ease(stick, t_pitch, decay);
    ease(stick + 2, t_yaw, decay);
    ease(stick + 4, t_roll, decay);
    ease(stick + 6, t_thr, 140);
}

void Game::surfaces_chase(u32 surf, s32 a, s32 b, s32 c, s32 thr) {
    auto step = [&](u32 at, s32 target) {
        const s32 v = m.r16s(at);
        return wrap::add((wrap::sub(v, target) < 0 ? wrap::sub(target, v) : wrap::sub(v, target)) >> 2, 2);
    };
    const s32 sa = step(surf, a), sb = step(surf + 2, b), sc = step(surf + 4, c);
    auto move = [&](u32 at, s32 target, s32 st) {
        if (target < m.r16s(at)) m.w16(at, m.r16(at) - static_cast<u32>(st));
        if (m.r16s(at) < target) m.w16(at, m.r16(at) + static_cast<u32>(st));
    };
    move(surf, a, sa);
    move(surf + 2, b, sb);
    move(surf + 4, c, sc);
    auto snap = [&](u32 at, s32 target, s32 st) {
        const s32 v = m.r16s(at);
        if (wrap::sub(v, target) < 0 ? wrap::sub(target, v) < st : wrap::sub(v, target) < st) m.w16(at, static_cast<u32>(target));
    };
    snap(surf, a, sa);
    snap(surf + 2, b, sb);
    snap(surf + 4, c, sc);
    const s32 brake = thr < -2048 ? wrap::mul(-2, wrap::add(thr, 2048)) : 0;
    if (m.r16s(surf + 8) < brake) m.w16(surf + 8, m.r16(surf + 8) + 128u);
    if (brake < m.r16s(surf + 8)) m.w16(surf + 8, m.r16(surf + 8) - 128u);
}

void Game::stick_integrate(u32 stick, u32 ctl, u32 surf, s32 pitch, s32 yaw, s32 roll, s32 thr, s32 lp, s32 ly, s32 lr,
                           u32 sh_p, u32 sh_y, u32 sh_r) {
    m.w16(surf + 6, static_cast<u32>((m.r16s(stick + 6) + 4096) >> 1));
    m.w16(stick, m.r16(stick) + static_cast<u32>(pitch >> sh_p));
    m.w16(stick + 2, m.r16(stick + 2) + static_cast<u32>(yaw >> sh_y));
    m.w16(stick + 4, m.r16(stick + 4) + static_cast<u32>(roll >> sh_r));
    m.w16(stick + 6, m.r16(stick + 6) + static_cast<u32>(thr >> 5));
    auto lim = [&](u32 at, s32 l) {
        if (m.r16s(at) < wrap::neg(l)) m.w16(at, static_cast<u32>(wrap::neg(l)));
        if (l < m.r16s(at)) m.w16(at, static_cast<u32>(l));
    };
    lim(stick, lp);
    lim(stick + 2, ly);
    lim(stick + 4, lr);
    if (m.r16s(stick + 6) < -4096) m.w16(stick + 6, static_cast<u32>(-4096));
    if (m.r16s(stick + 6) >= 4097) m.w16(stick + 6, 4096);
    m.w16(ctl + 4, m.r16(stick));
    m.w16(ctl + 6, m.r16(stick + 2));
    m.w16(ctl + 8, m.r16(stick + 4));
    m.w16(ctl + 14, m.r16(stick + 6));
}

void Game::controls_level(u32 e, u32 stick, u32 ctl, u32 surf, u32 hold) {
    cover(0x80073B18u);
    s32 thr = 0, pitch = 0;
    s32 roll = -4 * m.r16s(e + 28);
    if (m.r32s(e + 12) >= -12288000 && hold) {
        if (absv(m.r16s(e + 28)) < 2048) {
            const s32 p = m.r16s(e + 24);
            if (p < 2048) pitch = 16 * (2048 - p);
            thr = 4096;
        }
    } else {
        if (absv(m.r16s(e + 28)) < 2048) pitch = -16 * m.r16s(e + 24);
        if (absv(m.r16s(e + 24)) < 182) pitch >>= 1;
    }
    controls_level_tail(e, stick, ctl, surf, pitch, roll, thr);
}

void Game::controls_hold(u32 e, u32 stick, u32 ctl, u32 surf) {
    cover(0x8007407Cu);
    s32 thr = 0, pitch = 0;
    const s32 roll = -4 * m.r16s(e + 28);
    if (absv(m.r16s(e + 28)) < 2048) {
        const s32 p = m.r16s(e + 24);
        if (p != 2048) pitch = 16 * (2048 - p);
        thr = 4096;
    }
    controls_level_tail(e, stick, ctl, surf, pitch, roll, thr);
}

void Game::controls_level_tail(u32 e, u32 stick, u32 ctl, u32 surf, s32 pitch, s32 roll, s32 thr) {
    const s32 ar = absv(m.r16s(e + 28));
    if (ar < 455) roll >>= 1;
    if (ar < 273) roll >>= 1;
    if (ar < 136) roll >>= 1;
    pitch = clampv(pitch, -4096, 4096);
    roll = clampv(roll, -4096, 4096);
    const s32 lp = absv(pitch), lr = absv(roll);
    auto ease = [&](u32 at, s32 lim, s32 div, bool any) {
        const s32 v = m.r16s(at);
        if (any ? absv(v) > 0 : lim < absv(v)) m.w16(at, static_cast<u32>((v * 128) / div));
    };
    ease(stick, lp, 200, false);
    ease(stick + 2, 0, 200, true);
    ease(stick + 4, lr, 200, false);
    ease(stick + 6, thr, 140, false);
    auto step = [&](u32 at, s32 target) {
        const s32 v = m.r16s(at);
        return wrap::add((wrap::sub(v, target) < 0 ? wrap::sub(target, v) : wrap::sub(v, target)) >> 2, 2);
    };
    const s32 sa = step(surf, roll), sb = step(surf + 2, pitch);
    const s32 v2 = m.r16s(surf + 4);
    const s32 sc = (v2 < 0 ? (-v2) >> 2 : v2 >> 2) + 2;
    if (roll < m.r16s(surf)) m.w16(surf, m.r16(surf) - static_cast<u32>(sa));
    if (m.r16s(surf) < roll) m.w16(surf, m.r16(surf) + static_cast<u32>(sa));
    if (pitch < m.r16s(surf + 2)) m.w16(surf + 2, m.r16(surf + 2) - static_cast<u32>(sb));
    if (m.r16s(surf + 2) < pitch) m.w16(surf + 2, m.r16(surf + 2) + static_cast<u32>(sb));
    if (m.r16s(surf + 4) > 0) m.w16(surf + 4, m.r16(surf + 4) - static_cast<u32>(sc));
    if (m.r16s(surf + 4) < 0) m.w16(surf + 4, m.r16(surf + 4) + static_cast<u32>(sc));
    auto snap = [&](u32 at, s32 target, s32 st) {
        const s32 v = m.r16s(at);
        if (wrap::sub(v, target) < 0 ? wrap::sub(target, v) < st : wrap::sub(v, target) < st) m.w16(at, static_cast<u32>(target));
    };
    snap(surf, roll, sa);
    snap(surf + 2, pitch, sb);
    const s32 v4 = m.r16s(surf + 4);
    if (v4 < 0 ? -v4 < sc : v4 < sc) m.w16(surf + 4, 0);
    if (m.r16s(surf + 8) < 0) m.w16(surf + 8, m.r16(surf + 8) + 128u);
    if (m.r16s(surf + 8) > 0) m.w16(surf + 8, m.r16(surf + 8) - 128u);
    m.w16(surf + 6, static_cast<u32>((m.r16s(stick + 6) + 4096) >> 1));
    m.w16(stick, m.r16(stick) + static_cast<u32>(pitch >> 3));
    m.w16(stick + 4, m.r16(stick + 4) + static_cast<u32>(roll >> 5));
    m.w16(stick + 6, m.r16(stick + 6) + static_cast<u32>(thr >> 5));
    if (m.r16s(stick) < -lp) m.w16(stick, static_cast<u32>(-lp));
    if (lp < m.r16s(stick)) m.w16(stick, static_cast<u32>(lp));
    if (m.r16s(stick + 2) < 0) m.w16(stick + 2, 0);
    if (m.r16s(stick + 2) > 0) m.w16(stick + 2, 0);
    if (m.r16s(stick + 4) < -lr) m.w16(stick + 4, static_cast<u32>(-lr));
    if (lr < m.r16s(stick + 4)) m.w16(stick + 4, static_cast<u32>(lr));
    if (m.r16s(stick + 6) < -4096) m.w16(stick + 6, static_cast<u32>(-4096));
    if (m.r16s(stick + 6) >= 4097) m.w16(stick + 6, 4096);
    m.w16(ctl + 4, m.r16(stick));
    m.w16(ctl + 6, m.r16(stick + 2));
    m.w16(ctl + 8, m.r16(stick + 4));
    m.w16(ctl + 14, m.r16(stick + 6));
}

void Game::controls_launch(u32 e, u32 st, u32 stick, u32 ctl, u32 surf) {
    cover(0x8007542Cu);
    s32 thr = 0, pitch = 0, roll = 0;
    const s32 phase = m.r32s(st + 8);
    if (phase == 1) {
        thr = -2048;
        const s32 c = m.r32s(st + 12);
        if (c < 76) {
            m.w32(st + 12, static_cast<u32>(c + 1));
        } else {
            m.w32(st + 8, 2);
            m.w32(st + 12, 0);
        }
    } else if (phase == 0) {
        m.w32(st + 8, 1);
        m.w32(st + 12, 0);
        m.w16(stick, 0);
        m.w16(stick + 2, 0);
        m.w16(stick + 4, 0);
        m.w16(stick + 6, 0);
    } else if (phase == 2) {
        thr = -2048;
        const s32 c = m.r32s(st + 12);
        pitch = 2 * (1820 - m.r16s(e + 28));
        if (c < 121) {
            m.w32(st + 12, static_cast<u32>(c + 1));
        } else {
            m.w32(st + 8, 3);
            m.w32(st + 12, 0);
        }
    } else if (phase == 3) {
        pitch = -2 * m.r16s(e + 28);
        roll = -16 * m.r16s(e + 24);
    }
    roll = clampv(roll, -4096, 4096);
    pitch = clampv(pitch, -4096, 4096);
    const s32 lr = absv(roll), lp = absv(pitch);
    auto ease = [&](u32 at, s32 lim, s32 div, bool any) {
        const s32 v = m.r16s(at);
        if (any ? absv(v) > 0 : lim < absv(v)) m.w16(at, static_cast<u32>((v * 128) / div));
    };
    ease(stick, lr, 200, false);
    ease(stick + 2, 0, 200, true);
    ease(stick + 4, lp, 200, false);
    ease(stick + 6, absv(thr), 140, false);
    auto step = [&](u32 at, s32 target) {
        const s32 v = m.r16s(at);
        return wrap::add((wrap::sub(v, target) < 0 ? wrap::sub(target, v) : wrap::sub(v, target)) >> 2, 2);
    };
    const s32 sa = step(surf, pitch), sb = step(surf + 2, roll);
    const s32 v2 = m.r16s(surf + 4);
    const s32 sc = (v2 < 0 ? (-v2) >> 2 : v2 >> 2) + 2;
    if (pitch < m.r16s(surf)) m.w16(surf, m.r16(surf) - static_cast<u32>(sa));
    if (m.r16s(surf) < pitch) m.w16(surf, m.r16(surf) + static_cast<u32>(sa));
    if (roll < m.r16s(surf + 2)) m.w16(surf + 2, m.r16(surf + 2) - static_cast<u32>(sb));
    if (m.r16s(surf + 2) < roll) m.w16(surf + 2, m.r16(surf + 2) + static_cast<u32>(sb));
    if (m.r16s(surf + 4) > 0) m.w16(surf + 4, m.r16(surf + 4) - static_cast<u32>(sc));
    if (m.r16s(surf + 4) < 0) m.w16(surf + 4, m.r16(surf + 4) + static_cast<u32>(sc));
    auto snap = [&](u32 at, s32 target, s32 st2) {
        const s32 v = m.r16s(at);
        if (v - target < 0 ? target - v < st2 : v - target < st2) m.w16(at, static_cast<u32>(target));
    };
    snap(surf, pitch, sa);
    snap(surf + 2, roll, sb);
    const s32 v4 = m.r16s(surf + 4);
    if (v4 < 0 ? -v4 < sc : v4 < sc) m.w16(surf + 4, 0);
    const s32 brake = thr < -2048 ? wrap::mul(-2, wrap::add(thr, 2048)) : 0;
    if (m.r16s(surf + 8) < brake) m.w16(surf + 8, m.r16(surf + 8) + 128u);
    if (brake < m.r16s(surf + 8)) m.w16(surf + 8, m.r16(surf + 8) - 128u);
    m.w16(surf + 6, static_cast<u32>((m.r16s(stick + 6) + 4096) >> 1));
    m.w16(stick, m.r16(stick) + static_cast<u32>(roll >> 3));
    m.w16(stick + 4, m.r16(stick + 4) + static_cast<u32>(pitch >> 5));
    m.w16(stick + 6, m.r16(stick + 6) + static_cast<u32>(thr >> 5));
    if (m.r16s(stick) < -lr) m.w16(stick, static_cast<u32>(-lr));
    if (lr < m.r16s(stick)) m.w16(stick, static_cast<u32>(lr));
    if (m.r16s(stick + 2) < 0) m.w16(stick + 2, 0);
    if (m.r16s(stick + 2) > 0) m.w16(stick + 2, 0);
    if (m.r16s(stick + 4) < -lp) m.w16(stick + 4, static_cast<u32>(-lp));
    if (lp < m.r16s(stick + 4)) m.w16(stick + 4, static_cast<u32>(lp));
    if (m.r16s(stick + 6) < -4096) m.w16(stick + 6, static_cast<u32>(-4096));
    if (m.r16s(stick + 6) >= 4097) m.w16(stick + 6, 4096);
    m.w16(ctl + 4, m.r16(stick));
    m.w16(ctl + 6, m.r16(stick + 2));
    m.w16(ctl + 8, m.r16(stick + 4));
    m.w16(ctl + 14, m.r16(stick + 6));
}

void Game::player_controls(u32 e, u32 fp) {
    cover(0x80072938u);
    Frame f(*this, 40);
    const u32 pad = m.r32(g::kScene) + 288;
    if (m.r32s(pad + 8) < 0) {
        controls_level(e, m.r32(kCtlStick), m.r32(kCtl), m.r32(kCtlSurf), 0);
        return;
    }
    if (m.r32(pad) & 4u) {
        controls_novice(pad, m.r32(kCtlStick), m.r32(kCtl), m.r32(kCtlSurf), e, fp, m.r32(g::kPlayer));
        return;
    }
    controls_standard(pad, m.r32(kCtlStick), m.r32(kCtl), m.r32(kCtlSurf));
}

void Game::scale_by_speed(s32 speed, u32 at, u32 fn) {
    cover(fn);
    const s32 k = speed > 86015 ? 4096 : speed / 21;
    const s32 c = m.r16s(at + 4) * k / 4096;
    const s32 a = m.r16s(at) * k;
    m.w16(at + 2, static_cast<u32>(m.r16s(at + 2) * k / 4096));
    m.w16(at + 4, static_cast<u32>(c));
    m.w16(at, static_cast<u32>(a / 4096));
}

void Game::flight_simple(u32 e, u32 ctl, u32 fp, u32 st) {
    cover(0x800702D8u);
    Frame f(*this, 0xC8);
    const u32 A = f.at(0x18), rates = f.at(0x10);
    memset(A, 0, 16);
    const s32 t0 = q12(m.r16s(ctl + 4) * m.r32s(fp));
    const s32 t1 = q12(m.r16s(ctl + 6) * m.r32s(fp + 8));
    const s32 t2 = q12(m.r16s(ctl + 8) * m.r32s(fp + 0xC));
    s32 rp = clampv(m.r16s(st + 0x30) + t0, -m.r32s(fp), m.r32s(fp));
    s32 ry = clampv(m.r16s(st + 0x32) + t1, -m.r32s(fp + 8), m.r32s(fp + 8));
    s32 rr = clampv(m.r16s(st + 0x34) + t2, -m.r32s(fp + 0xC), m.r32s(fp + 0xC));
    m.w16(rates, static_cast<u32>(rp));
    m.w16(rates + 2, static_cast<u32>(ry));
    m.w16(rates + 4, static_cast<u32>(rr));
    auto settle = [&](s32 r, s32 target) {
        if (absv(target) < absv(r)) {
            r = q12(r * 0xF7F);
            if (absv(r) < absv(target)) r = target;
        }
        return r;
    };
    rp = settle(rp, t0);
    ry = settle(ry, t1);
    rr = settle(rr, t2);
    m.w16(st + 0x30, static_cast<u32>(rp));
    m.w16(st + 0x32, static_cast<u32>(ry));
    m.w16(st + 0x34, static_cast<u32>(rr));
    m.w16(e + 0x30, m.r16(rates));
    m.w16(e + 0x32, m.r16(rates + 2));
    m.w16(e + 0x34, m.r16(rates + 4));
    attitude_integrate(e + 0x18, e + 0x30);
    const s32 speed = length3((0 - m.r32s(e + 0x38)) >> 6, (0 - m.r32s(e + 0x3C)) >> 6, (0 - m.r32s(e + 0x40)) >> 6) << 6;
    const u32 mat = f.at(0x30), vv = f.at(0x28), lv = f.at(0x50);
    rot_matrix_yxz_t(e + 0x18, mat);
    for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(mat + 4 * r));
    m.w16(vv, static_cast<u32>(m.r32s(e + 0x38) >> 6));
    m.w16(vv + 2, static_cast<u32>(m.r32s(e + 0x3C) >> 6));
    m.w16(vv + 4, static_cast<u32>(m.r32s(e + 0x40) >> 6));
    gte.mtc2(0, m.r32(vv));
    gte.mtc2(1, m.r32(vv + 4));
    gte.op(0x486012);
    m.w32(lv, gte.mfc2(25));
    m.w32(lv + 4, gte.mfc2(26));
    m.w32(lv + 8, gte.mfc2(27));
    const s32 fwd = m.r32s(lv + 8) << 6;
    s32 push = (m.r16s(ctl + 0xE) * m.r32s(fp + 0x48)) >> 12;
    if (fwd < 0 && push < 0) push = 0;
    if (m.r32s(fp + 0x3C) < fwd && push > 0) push = 0;
    const u32 tv = f.at(0x60), m2 = f.at(0x68), tout = f.at(0x88);
    m.w16(tv, 0);
    m.w16(tv + 2, 0);
    m.w16(tv + 4, static_cast<u32>(push));
    rot_matrix_yxz(e + 0x18, m2);
    for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(m2 + 4 * r));
    gte.mtc2(0, m.r32(tv));
    gte.mtc2(1, m.r32(tv + 4));
    gte.op(0x486012);
    m.w32(tout, gte.mfc2(25));
    m.w32(tout + 4, gte.mfc2(26));
    m.w32(tout + 8, gte.mfc2(27));
    s32 ax = m.r32s(A) + m.r32s(tout), az = m.r32s(A + 8) + m.r32s(tout + 8), ay = m.r32s(A + 4) + m.r32s(tout + 4);
    m.w32(A, static_cast<u32>(ax));
    m.w32(A + 8, static_cast<u32>(az));
    m.w32(A + 4, static_cast<u32>(ay));
    const s32 dx = (0 - m.r32s(e + 0x38)) >> 8;
    m.w32(f.at(0x98), static_cast<u32>(dx));
    const s32 dy = (0 - m.r32s(e + 0x3C)) >> 8;
    ax += dx;
    m.w32(f.at(0x9C), static_cast<u32>(dy));
    ay += dy;
    m.w32(A, static_cast<u32>(ax));
    m.w32(A + 4, static_cast<u32>(ay));
    const s32 dz = (0 - m.r32s(e + 0x40)) >> 8;
    az += dz;
    m.w32(f.at(0xA0), static_cast<u32>(dz));
    m.w32(A + 8, static_cast<u32>(az));
    m.w32(e + 0x38, m.r32(e + 0x38) + static_cast<u32>(ax));
    m.w32(e + 0x3C, m.r32(e + 0x3C) + static_cast<u32>(ay));
    m.w32(e + 0x40, m.r32(e + 0x40) + static_cast<u32>(az));
    for (u32 o = 0x38; o <= 0x40; o += 4) {
        if (0xB4000 < m.r32s(e + o)) m.w32(e + o, 0xB4000);
        if (m.r32s(e + o) < -0xB4000) m.w32(e + o, static_cast<u32>(-0xB4000));
    }
    m.w32(st + 0x48, static_cast<u32>(speed));
    vec_to_angles(e + 0x38, st + 0x40);
    player_view_angles(e, st);
    m.w16(st + 0x4C, 0x1000);
}
}

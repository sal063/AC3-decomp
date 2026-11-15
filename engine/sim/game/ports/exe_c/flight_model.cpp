#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
s32 q12(s32 x) { return x / 4096; }
s32 q13(s32 x) { return x / 8192; }
s32 scaled(s32 x, u32 shift) { return static_cast<s32>(static_cast<u32>(x) << shift); }
s32 absv(s32 x) { return x < 0 ? wrap::neg(x) : x; }
s32 clampv(s32 x, s32 lo, s32 hi) { return x < lo ? lo : x > hi ? hi : x; }
}
void Game::flight_model(u32 e, u32 ctl, u32 fp, u32 st) {
    cover(0x8006EF30u);
    auto divide = [&](s32 numerator, s32 denominator, u32 zeroTrap, u32 overflowTrap) {
        if (!denominator) unported(zeroTrap, "flight division by zero");
        if (denominator == -1 && static_cast<u32>(numerator) == 0x80000000u)
            unported(overflowTrap, "flight division overflow");
        return static_cast<s32>(wrap::div(numerator, denominator).lo);
    };
    Frame f(*this, 0x158);
    m.w32(sp + 0x158, e);
    m.w32(sp + 0x15C, ctl);
    m.w32(sp + 0x164, st);
    const u32 acceleration = f.at(0x28), inverseRotation = f.at(0x40), velocityShort = f.at(0x38), localVelocity = f.at(0x60);
    const u32 angleOfAttack = st + 0x38;
    m.w32(f.at(0x114), angleOfAttack);
    const u32 attitude = e + 0x18;
    memset(acceleration, 0, 16);
    rot_matrix_yxz_t(attitude, inverseRotation);
    for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(inverseRotation + 4 * r));
    m.w16(velocityShort, static_cast<u32>(wrap::neg(m.r32s(e + 0x38)) >> 5));
    m.w16(velocityShort + 2, static_cast<u32>(wrap::neg(m.r32s(e + 0x3C)) >> 5));
    m.w16(velocityShort + 4, static_cast<u32>(wrap::neg(m.r32s(e + 0x40)) >> 5));
    gte.mtc2(0, m.r32(velocityShort));
    gte.mtc2(1, m.r32(velocityShort + 4));
    gte.op(0x486012);
    m.w32(localVelocity, gte.mfc2(25));
    m.w32(localVelocity + 4, gte.mfc2(26));
    m.w32(localVelocity + 8, gte.mfc2(27));
    s32 speed = scaled(absv(m.r32s(localVelocity + 8)), 5);
    m.w32(f.at(0x118), static_cast<u32>(speed));
    if (m.r32s(fp + 0x3C) < speed) {
        const s32 k = divide(scaled(m.r32s(fp + 0x3C), 6), speed, 0x8006F06Cu, 0x8006F084u);
        m.w32(e + 0x38, static_cast<u32>(m.r32s(e + 0x38) * k / 64));
        m.w32(e + 0x3C, static_cast<u32>(m.r32s(e + 0x3C) * k / 64));
        m.w32(e + 0x40, static_cast<u32>(m.r32s(e + 0x40) * k / 64));
        speed = m.r32s(fp + 0x3C);
        m.w32(f.at(0x118), static_cast<u32>(speed));
    }
    const u32 stallOut = f.at(0x108), cruiseOut = f.at(0x10C), topOut = f.at(0x110);
    m.w32(f.at(0x10), cruiseOut);
    m.w32(f.at(0x120), 0);
    m.w32(f.at(0x14), topOut);
    const s32 lift = flight_envelope(m.r32s(e + 0xC), speed, fp, stallOut, cruiseOut, topOut);
    s32 liftAcceleration = -669;
    m.w32(f.at(0x11C), static_cast<u32>(lift));
    s32 stall = m.r32s(stallOut);
    if (!(0x1CFFF < stall)) stall = 0x1D000;
    s32 liftRatio = wrap::mul(2, divide(scaled(speed, 11), stall, 0x8006F184u, 0x8006F19Cu));
    s32 alpha = m.r16s(angleOfAttack);
    s32 rate;
    if (alpha < 0) {
        const s32 lim = m.r32s(fp + 0x54);
        if (alpha < lim) alpha = lim;
        rate = divide(wrap::mul(wrap::sub(m.r32s(fp + 0x5C), 4096), alpha), lim, 0x8006F220u, 0x8006F238u);
    } else {
        const s32 lim = m.r32s(fp + 0x58);
        if (lim < alpha) alpha = lim;
        rate = divide(wrap::mul(wrap::sub(m.r32s(fp + 0x60), 4096), alpha), lim, 0x8006F220u, 0x8006F238u);
    }
    const s32 angleScale = rate + 0x1000;
    liftRatio = q12(liftRatio * angleScale);
    if (liftRatio + angleScale >= 0x2321 && liftRatio >= 0x1801 && angleScale >= 0x1401) m.w32(st + 0x10, m.r32(st + 0x10) | 0x1000u);
    s32 liftAuthority;
    if (liftRatio < m.r32s(fp + 0x64)) {
        m.w32(f.at(0x120), 0x1000);
        liftAuthority = 0;
    } else {
        liftAuthority = q12((liftRatio - m.r32s(fp + 0x64)) * m.r32s(fp + 0x68));
        if (liftAuthority >= 0x1001) liftAuthority = q12((liftAuthority - 0x1000) * m.r32s(fp + 0x6C)) + 0x1000;
        if (m.r32s(fp + 0x5C) < liftAuthority) liftAuthority = m.r32s(fp + 0x5C);
        const s32 over = liftRatio - m.r32s(fp + 0x64);
        if (over < 0x100) m.w32(f.at(0x120), static_cast<u32>(0x1000 - scaled(over, 4)));
    }
    m.w32(f.at(0x124), static_cast<u32>(liftAuthority));
    if (liftAuthority >= 0x1001) m.w32(f.at(0x124), 0x1000);
    const s32 authority = m.r32s(f.at(0x124));
    s32 bankAuthority = liftRatio;
    m.w16(st + 0x4C, static_cast<u32>(authority));
    if (bankAuthority >= 0x1001) bankAuthority = 0x1000;
    bankAuthority = q12((0x1000 - absv(m.r16s(attitude))) * bankAuthority);
    const u32 ri = m.r16(attitude + 4) & 0x3FFFu;
    s32 c = m.r16s(g::kSinCos + 2 * (ri + 0x1000));
    const s32 sn = m.r16s(g::kSinCos + 2 * ri);
    if (absv(c) < 0x401) c = c >= 0 ? 0x400 : -0x400;
    const s32 tn = divide(scaled(sn, 12), c, 0x8006F408u, 0x8006F420u);
    const s32 bank = q12(bankAuthority * absv(tn));
    const s32 maxBankAuthority = 0x1785 < liftRatio ? 0x1785 : liftRatio;
    if (maxBankAuthority < bank) {
        bankAuthority = maxBankAuthority;
    } else {
        bankAuthority = length3(bank, bankAuthority, 0);
        if (maxBankAuthority < bankAuthority) bankAuthority = maxBankAuthority;
    }
    const u32 m2 = f.at(0x88), sv = f.at(0x70), out = f.at(0x78);
    rot_matrix_yxz(attitude, m2);
    for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(m2 + 4 * r));
    if (lift < 0x1000) liftAcceleration = q12(liftAcceleration * lift);
    liftAcceleration = q12(liftAcceleration * bankAuthority);
    m.w16(sv, 0);
    m.w16(sv + 2, static_cast<u32>(liftAcceleration));
    m.w16(sv + 4, 0);
    gte.mtc2(0, m.r32(sv));
    gte.mtc2(1, m.r32(sv + 4));
    gte.op(0x486012);
    m.w32(out, gte.mfc2(25));
    m.w32(out + 4, gte.mfc2(26));
    m.w32(out + 8, gte.mfc2(27));
    const s32 fall = 669 * (0x1000 - lift);
    const s32 ax = m.r32s(acceleration) + m.r32s(out), ay = m.r32s(acceleration + 4) + m.r32s(out + 4), az = m.r32s(acceleration + 8) + m.r32s(out + 8);
    m.w32(acceleration, static_cast<u32>(ax));
    m.w32(acceleration + 4, static_cast<u32>(ay));
    m.w32(acceleration + 8, static_cast<u32>(az));
    const s32 g = q12(fall) + 0x29D;
    m.w32(f.at(0xAC), static_cast<u32>(g));
    m.w32(f.at(0xA8), 0);
    m.w32(f.at(0xB0), 0);
    m.w32(acceleration, static_cast<u32>(ax));
    m.w32(acceleration + 4, static_cast<u32>(ay + g));
    m.w32(acceleration + 8, static_cast<u32>(az));
    const s32 yaw_gain = m.r32s(fp + 0x24);
    m.w32(f.at(0x128), static_cast<u32>(yaw_gain));
    s32 k_pitch = m.r32s(fp + 0x20), k_roll = m.r32s(fp + 0x28);
    const s32 lim_p = m.r32s(fp + 0x14), lim_y = m.r32s(fp + 0x18), lim_r = m.r32s(fp + 0x1C);
    const s32 stallFade = m.r32s(f.at(0x120));
    if (stallFade) {
        k_pitch += stallFade;
        if (k_pitch >= 0x1001) k_pitch = 0x1000;
        k_roll += stallFade;
        if (k_roll >= 0x1001) k_roll = 0x1000;
    }
    m.w16(sv, 0);
    m.w16(sv + 2, 0);
    m.w16(sv + 4, m.r16(attitude + 4));
    rot_matrix_yxz(sv, m2);
    for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(m2 + 4 * r));
    gte.mtc2(0, m.r32(angleOfAttack));
    gte.mtc2(1, m.r32(angleOfAttack + 4));
    gte.op(0x486012);
    m.w32(out, gte.mfc2(25));
    m.w32(out + 4, gte.mfc2(26));
    m.w32(out + 8, gte.mfc2(27));
    if (m.r32(st + 4) & 0xC108u) k_pitch >>= 2;
    s32 dp = q12(m.r32s(out) * k_pitch);
    s32 dy = q12(m.r32s(out + 4) * yaw_gain);
    s32 dr = q12(m.r32s(out + 8) * k_roll);
    auto limitTurn = [](s32 value, s32 limit) {
        if (value < wrap::neg(limit)) value = wrap::neg(limit);
        if (limit < value) value = limit;
        return value;
    };
    dp = limitTurn(dp, lim_p);
    dy = limitTurn(dy, lim_y);
    dr = limitTurn(dr, lim_r);
    if (!(m.r32(st + 0x10) & 2u)) {
        m.w16(attitude, m.r16(attitude) + static_cast<u32>(dp));
        m.w16(attitude + 4, m.r16(attitude + 4) + static_cast<u32>(dr));
        m.w16(attitude + 2, m.r16(attitude + 2) + static_cast<u32>(dy));
    }
    const s32 stalling = m.r32s(f.at(0x120));
    if (stalling && !(m.r32(st + 0x10) & 0xBu)) {
        const s32 drop = q12(-(stalling * 37));
        if (-0x1000 - drop < m.r16s(attitude)) m.w16(attitude, m.r16(attitude) + static_cast<u32>(drop));
    }
    s32 cp = m.r16s(ctl + 4);
    const s32 cy = m.r16s(ctl + 6);
    s32 cr = m.r16s(ctl + 8);
    s32 pitchGain = cp < 0 ? m.r32s(fp + 4) : m.r32s(fp);
    {
        s32 v = q13(m.r16s(angleOfAttack) * (speed >> 1));
        v = absv(v);
        if (0x3200 < v) v = 0x3200;
        const s32 red = 0xC78 - static_cast<s32>(wrap::div(v * 0xC78, 0x3200).lo);
        const s32 acp = absv(cp);
        pitchGain += q12(pitchGain * q12(red * acp));
        const s32 sum = acp + absv(cr);
        s32 wp = 0, wr = 0;
        if (sum) {
            wp = static_cast<s32>(wrap::div(scaled(acp, 12), sum).lo);
            wr = static_cast<s32>(wrap::div(scaled(absv(cr), 12), sum).lo);
        }
        cp = q12(q12(cp * wp) * liftAuthority);
        cr = q12(q12(cr * wr) * liftAuthority);
    }
    const s32 cyaw = q12(cy * (0x1000 - stalling));
    const s32 pitchTarget = q12(cp * pitchGain);
    const s32 yawTarget = q12(cyaw * m.r32s(fp + 8));
    const s32 rollTarget = q12(cr * m.r32s(fp + 0xC));
    const s32 resp = m.r32s(fp + 0x10);
    s32 rp = m.r16s(st + 0x30) + q12(pitchTarget * resp);
    s32 ry = m.r16s(st + 0x32) + q12(yawTarget * resp);
    s32 rr = m.r16s(st + 0x34) + q12(rollTarget * resp);
    rp = clampv(rp, -pitchGain, pitchGain);
    ry = clampv(ry, -m.r32s(fp + 8), m.r32s(fp + 8));
    rr = clampv(rr, -m.r32s(fp + 0xC), m.r32s(fp + 0xC));
    m.w16(f.at(0x20), static_cast<u32>(rp));
    m.w16(f.at(0x22), static_cast<u32>(ry));
    m.w16(f.at(0x24), static_cast<u32>(rr));
    s32 damp;
    if (liftAuthority >= 0x1001) {
        damp = 0x1000 - resp;
    } else {
        damp = 0x1000 - q12(resp * liftAuthority);
        if (damp >= 0xFFD) damp = 0xFFC;
    }
    auto settle = [&](s32 r, s32 target) {
        if (absv(target) < absv(r)) {
            r = q12(r * damp);
            if (absv(r) < absv(target)) r = target;
        }
        return r;
    };
    rp = settle(rp, pitchTarget);
    ry = settle(ry, yawTarget);
    rr = settle(rr, rollTarget);
    if (lift < 0x1000) {
        const s32 k = lift < 0x80 ? 0x80 : lift;
        rp = q12(rp * k);
        ry = q12(ry * k);
        rr = q12(rr * k);
    }
    const u32 mode = m.r32(st + 4);
    m.w16(st + 0x30, static_cast<u32>(rp));
    m.w16(st + 0x32, static_cast<u32>(ry));
    m.w16(st + 0x34, static_cast<u32>(rr));
    s32 accel = m.r32s(fp + 0x48);
    if (mode & 8u) {
        accel >>= 1;
        m.w32(stallOut, 0xA000);
        m.w32(cruiseOut, 0x10000);
        m.w32(topOut, 0x1A000);
    } else if (mode & 0x100u) {
        m.w32(stallOut, 0x8000);
        m.w32(cruiseOut, 0x19A00);
        m.w32(topOut, 0x37000);
    } else if (mode & 0xC000u) {
        accel >>= 1;
        m.w32(stallOut, 0x1000);
        m.w32(cruiseOut, 0xF000);
        m.w32(topOut, 0x3C000);
    }
    if (m.r32s(stallOut) < 0) m.w32(stallOut, 0);
    s32 thr = m.r16s(ctl + 0xE);
    if (thr >= 0 && authority >= 0x1000) {
        thr += absv(m.r16s(angleOfAttack)) << 2;
        thr = clampv(thr, -0x1000, 0x1000);
    } else {
        thr = m.r16s(ctl + 0xE);
    }
    m.w32(f.at(0x10), m.r32(e + 0xC));
    m.w32(f.at(0x14), static_cast<u32>(accel));
    m.w32(f.at(0x18), static_cast<u32>(accel));
    s32 push = clampv(flight_thrust(speed, m.r32s(cruiseOut), m.r32s(topOut), thr, m.r32s(e + 0xC), accel, accel), -0x2000, 0x2000);
    const s32 sp_ = m.r16s(g::kSinCos + 2 * (m.r16(attitude) & 0x3FFFu)) >> 2;
    const s32 lean = 0x1000 - sp_;
    s32 cap = m.r32s(fp + 0x4C);
    if (m.r16s(attitude) > 0) cap = q12((m.r32s(fp + 0x4C) - m.r32s(fp + 0x50)) * lean) + m.r32s(fp + 0x50);
    if (cap < push) push = cap;
    if (push < -cap) push = -cap;
    push = q12(push * authority);
    const u32 tv = f.at(0xA8), m3 = f.at(0xB8), tout = f.at(0xD8);
    m.w16(tv, 0);
    m.w16(tv + 2, 0);
    m.w16(tv + 4, static_cast<u32>(push));
    rot_matrix_yxz(attitude, m3);
    for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(m3 + 4 * r));
    gte.mtc2(0, m.r32(tv));
    gte.mtc2(1, m.r32(tv + 4));
    gte.op(0x486012);
    m.w32(tout, gte.mfc2(25));
    m.w32(tout + 4, gte.mfc2(26));
    m.w32(tout + 8, gte.mfc2(27));
    if (m.r32s(tout + 4) < 0 && lift < 0x1000) m.w32(tout + 4, static_cast<u32>(q12(m.r32s(tout + 4) * lift)));
    m.w32(acceleration, m.r32(acceleration) + m.r32(tout));
    m.w32(acceleration + 4, m.r32(acceleration + 4) + m.r32(tout + 4));
    m.w32(acceleration + 8, m.r32(acceleration + 8) + m.r32(tout + 8));
    m.w32(st + 0x48, static_cast<u32>(speed));
    const s32 drag = q12(scaled(m.r32s(fp + 0x70), 12));
    m.w32(tv, static_cast<u32>(q12(-m.r32s(e + 0x38) * drag)));
    m.w32(tv + 4, static_cast<u32>(q12(-m.r32s(e + 0x3C) * drag)));
    const u32 m4 = f.at(0xE8);
    m.w32(tv + 8, static_cast<u32>(q12(-m.r32s(e + 0x40) * drag)));
    rot_matrix_yxz(attitude, m4);
    for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(m4 + 4 * r));
    const s32 dl = length3(m.r32s(tv) >> 1, m.r32s(tv + 4) >> 1, m.r32s(tv + 8) >> 1);
    const u32 dv = f.at(0xB8), dout = f.at(0xC0);
    m.w16(dv + 4, static_cast<u32>(dl));
    m.w16(dv, 0);
    m.w16(dv + 2, 0);
    gte.mtc2(0, m.r32(dv));
    gte.mtc2(1, m.r32(dv + 4));
    gte.op(0x486012);
    m.w32(dout, gte.mfc2(25));
    m.w32(dout + 4, gte.mfc2(26));
    m.w32(dout + 8, gte.mfc2(27));
    m.w32(dout, m.r32(dout) << 1);
    m.w32(dout + 4, m.r32(dout + 4) << 1);
    m.w32(dout + 8, m.r32(dout + 8) << 1);
    if (lift < 0x1000 && m.r32s(dout + 4) < 0) m.w32(dout + 4, static_cast<u32>(q12(m.r32s(dout + 4) * lift)));
    m.w32(acceleration, m.r32(acceleration) + m.r32(dout));
    m.w32(acceleration + 4, m.r32(acceleration + 4) + m.r32(dout + 4));
    m.w32(acceleration + 8, m.r32(acceleration + 8) + m.r32(dout + 8));
    if (lift < 0x1000 && m.r32s(tv + 4) < 0) m.w32(tv + 4, static_cast<u32>(q12(m.r32s(tv + 4) * lift)));
    m.w32(acceleration, m.r32(acceleration) + m.r32(tv));
    m.w32(acceleration + 8, m.r32(acceleration + 8) + m.r32(tv + 8));
    m.w32(acceleration + 4, m.r32(acceleration + 4) + m.r32(tv + 4));
    m.w32(e + 0x38, m.r32(e + 0x38) + m.r32(acceleration));
    m.w32(e + 0x3C, m.r32(e + 0x3C) + m.r32(acceleration + 4));
    m.w32(e + 0x40, m.r32(e + 0x40) + m.r32(acceleration + 8));
    vec_to_angles(e + 0x38, st + 0x40);
    m.w16(e + 0x30, m.r16(f.at(0x20)));
    m.w16(e + 0x32, m.r16(f.at(0x22)));
    m.w16(e + 0x34, m.r16(f.at(0x24)));
    attitude_integrate(e + 0x18, e + 0x30);
    player_view_angles(e, st);
    hud_alert_speed(0x1000 - authority);
}
}

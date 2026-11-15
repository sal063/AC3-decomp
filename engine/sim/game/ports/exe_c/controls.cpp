#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
s32 absv(s32 value) { return value < 0 ? wrap::neg(value) : value; }
s32 q12(s32 value) { return value / 4096; }
s32 clampv(s32 value, s32 lo, s32 hi) { return value < lo ? lo : value > hi ? hi : value; }
static s32 pad_axis(Memory& m, u32 pad, u32 live_bit, u32 axis, u32 plus, u32 minus, s32& limit, bool minus_first) {
    s32 v = 0;
    if (m.r32(pad + 24) & live_bit) {
        v = m.r32s(pad + 28 + 4 * axis);
        limit = absv(v);
        return v;
    }
    const u32 b = m.r32(pad + 8);
    if (minus_first) {
        if (b & minus) v = -4096;
        if (b & plus) v += 4096;
    } else {
        if (b & plus) v = 4096;
        if (b & minus) v -= 4096;
    }
    return v;
}
}

void Game::controls_standard(u32 pad, u32 stick, u32 ctl, u32 surf) {
    cover(0x8006D0DCu);
    s32 lp = 4096, ly = 4096, lr = 4096;
    const s32 pitch = pad_axis(m, pad, 1, 0, 0x800000, 0x1000000, lp, false);
    const s32 roll = pad_axis(m, pad, 4, 2, 0x40000000, 0x20000000, lr, true);
    const s32 yaw = pad_axis(m, pad, 2, 1, 0x4000000, 0x2000000, ly, true);
    s32 dummy = 0;
    const s32 thr = pad_axis(m, pad, 8, 3, 0x8000000, 0x10000000, dummy, false);
    stick_ease(stick, 200, pitch, roll, yaw, thr);
    surfaces_chase(surf, yaw, pitch, roll, thr);
    stick_integrate(stick, ctl, surf, pitch, roll, yaw, thr, lp, lr, ly, 3, 2, 5);
}

void Game::controls_ground(u32 pad, u32 stick, u32 ctl, u32 surf) {
    controls_ground_result(pad, stick, ctl, surf);
}

u32 Game::controls_ground_result(u32 pad, u32 stick, u32 ctl, u32 surf) {
    cover(0x8006D678u);
    Frame f(*this, 32);
    const u32 e = slot(0);
    if (m.r32s(pad + 8) < 0) {
        const bool autopilot = m.r32(m.r32(g::kScene) + 4) != 0 || absv(m.r16s(e + 28)) >= 547;
        if (autopilot) {
            if (m.r32(m.r32(g::kPlayer) + 4) == 0x1000000) controls_level(e, stick, ctl, surf, 0);
            else controls_hold(e, stick, ctl, surf);
            return m.r16(stick + 6);
        }
    }
    s32 lp = 4096, ly = 4096, lr = 4096;
    const s32 pitch = pad_axis(m, pad, 1, 0, 0x800000, 0x1000000, lp, false);
    const s32 roll = pad_axis(m, pad, 4, 2, 0x40000000, 0x20000000, lr, true);
    const s32 yaw = pad_axis(m, pad, 2, 1, 0x4000000, 0x2000000, ly, true);
    s32 dummy = 0;
    const s32 thr = pad_axis(m, pad, 8, 3, 0x8000000, 0x10000000, dummy, false);
    stick_ease(stick, 220, pitch, roll, yaw, thr);
    surfaces_chase(surf, yaw, pitch, roll, thr);
    m.w16(surf + 6, static_cast<u32>((m.r16s(stick + 6) + 4096) >> 1));
    m.w16(stick, m.r16(stick) + static_cast<u32>(pitch >> 5));
    m.w16(stick + 2, m.r16(stick + 2) + static_cast<u32>(roll >> 5));
    m.w16(stick + 4, m.r16(stick + 4) + static_cast<u32>(yaw >> 5));
    m.w16(stick + 6, m.r16(stick + 6) + static_cast<u32>(thr >> 5));
    auto lim = [&](u32 at, s32 l) {
        if (m.r16s(at) < wrap::neg(l)) m.w16(at, static_cast<u32>(wrap::neg(l)));
        if (l < m.r16s(at)) m.w16(at, static_cast<u32>(l));
    };
    lim(stick, lp);
    lim(stick + 2, lr);
    lim(stick + 4, ly);
    if (m.r16s(stick + 6) < -4096) m.w16(stick + 6, static_cast<u32>(-4096));
    if (m.r16s(stick + 6) >= 4097) m.w16(stick + 6, 4096);
    m.w16(ctl + 14, m.r16(stick + 6));
    m.w16(ctl + 4, m.r16(stick));
    if (m.r32(m.r32(g::kScene) + 288) & 4u) {
        m.w16(ctl + 6, m.r16(stick + 4));
        const s16 r = static_cast<s16>(m.r16(stick + 2) + static_cast<u32>(m.r16s(stick + 4) >> 5));
        m.w16(ctl + 8, static_cast<u32>(r));
        if (r >= 4097) m.w16(ctl + 8, 4096);
        if (m.r16s(ctl + 8) < -4096) m.w16(ctl + 8, static_cast<u32>(-4096));
        const u32 p = slot(0);
        if (m.r16s(p + 28) >= 13) m.w16(p + 28, m.r16(p + 28) - 12u);
        const u32 nextRoll = m.r16(p + 28) + 12u;
        if (m.r16s(p + 28) < -12) m.w16(p + 28, nextRoll);
        return nextRoll;
    } else {
        m.w16(ctl + 6, m.r16(stick + 2));
        m.w16(ctl + 8, m.r16(stick + 4));
        return m.r16(stick + 4);
    }
}

void Game::controls_novice(u32 pad, u32 stick, u32 ctl, u32 surf, u32 e, u32 fp, u32 st) {
    cover(0x8006DD5Cu);
    Frame f(*this, 32);
    s32 lyaw = 4096, yawv = 0, thr = 0, pitch = 0, bank = 0;
    const u32 buttons = m.r32(pad + 8);
    const u32 live = m.r32(pad + 24);
    if (live & 2) {
        bank = m.r32s(pad + 32);
    } else {
        if (buttons & 0x2000000) bank = -4096;
        if (buttons & 0x4000000) bank += 4096;
    }
    if (live & 1) {
        pitch = m.r32s(pad + 28);
    } else {
        if (buttons & 0x800000) pitch = 4096;
        if (buttons & 0x1000000) pitch -= 4096;
    }
    const u32 ap = e + 24;
    const s32 ab = absv(bank);
    const s32 roll = m.r16s(e + 28);
    const s32 aroll = absv(roll);
    const s32 k = 4096 - absv(m.r16s(ap));
    s32 t = 0;
    if (ab < 2048) t = q12(wrap::mul(wrap::mul(wrap::neg(roll), wrap::sub(2048, ab)) / 2048, k));
    if (roll >= 2049) t = wrap::sub(t, wrap::mul(aroll - 2048, k) / 4096);
    if (roll < -2048) t = wrap::add(t, wrap::mul(aroll - 2048, k) / 4096);
    s32 rollcmd = q12(wrap::mul(t, wrap::sub(4096, absv(pitch))));
    s32 scale, grav;
    if ((roll <= 0 || bank <= 0) && (roll >= 0 || bank >= 0)) {
        scale = m.r32s(fp + 4);
        grav = static_cast<s32>(static_cast<u32>(bank) << 12);
    } else {
        s32 w = 4096;
        if (aroll >= 1024) w = 4096 - ((aroll - 1024) << 12) / 1024;
        w = clampv(w, 0, 4096);
        scale = m.r32s(fp);
        grav = wrap::mul(w, bank);
    }
    rollcmd = wrap::add(rollcmd, q12(grav));
    s32 sn = m.r16s(g::kSinCos + 2u * (static_cast<u32>(roll) & 0x3FFFu)) >> 2;
    s32 lift;
    if (sn >= 4097) {
        lift = wrap::mul(4096, scale);
    } else if (sn < -4096) {
        lift = wrap::mul(-4096, scale);
    } else {
        lift = wrap::mul(sn, scale);
    }
    const s32 turn = q12(wrap::mul(lift / 4096, m.r16s(st + 76)));
    m.w16(e + 26, m.r16(e + 26) + static_cast<u32>(turn));
    const bool both = option(&Settings::normal_controls) != 0;
    s32 w2 = 4096;
    if (aroll >= 2048 && !both) w2 = 4096 - ((aroll - 2048) << 12) / 2048;
    const s32 pv = wrap::mul(w2, pitch);
    s32 pitchcmd = aroll >= 4096 ? pv / -4096 : pv / 4096;
    s32 over = 0;
    if (aroll >= 2048 && !both) over = ((aroll - 2048) << 12) / 2048;
    const s32 cross = q12(wrap::mul(roll >= 0 ? -over : over, absv(pitch)));
    if (aroll < 4096) rollcmd = wrap::add(rollcmd, wrap::mul(cross, 4096 - absv(m.r16s(ap))) / 4096);
    const s32 v50 = q12(wrap::mul(absv(m.r16s(ap)), absv(pitch)));
    s32 v51 = m.r32s(fp + 4);
    if (aroll >= 4097) v51 = wrap::neg(v51);
    const s32 v53 = q12(wrap::mul(v51, m.r16s(st + 76)));
    m.w16(ap, m.r16(ap) + static_cast<u32>(q12(wrap::mul(v53, v50))));
    const s32 lpitch = absv(pitchcmd);
    const s32 lroll = absv(rollcmd);
    if (live & 4) {
        yawv = m.r32s(pad + 36);
        lyaw = absv(yawv);
    } else {
        if (buttons & 0x20000000) yawv = -4096;
        if (buttons & 0x40000000) yawv += 4096;
    }
    yawv = clampv(yawv, -4096, 4096);
    lyaw = clampv(lyaw, -4096, 4096);
    if (live & 8) {
        thr = m.r32s(pad + 40);
    } else {
        if (buttons & 0x8000000) thr = 4096;
        if (buttons & 0x10000000) thr -= 4096;
    }
    stick_ease(stick, 200, pitchcmd, yawv, rollcmd, thr);
    surfaces_chase(surf, rollcmd, pitchcmd, yawv, thr);
    stick_integrate(stick, ctl, surf, pitchcmd, yawv, rollcmd, thr, lpitch, lyaw, lroll, 3, 2, 5);
}
}

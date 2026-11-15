#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
void Game::rotation_from_angles(u32 ang, u32 out, const u8 layout[9]) {
    auto t = [&](u32 i) { return static_cast<s32>(m.r16s(g::kSinCos + 2u * i)); };
    const u32 ax = m.r16(ang) & 0x3FFFu, ay = m.r16(ang + 2) & 0x3FFFu, az = m.r16(ang + 4) & 0x3FFFu;
    const s32 cx = t(ax + 0x1000), cz = t(az + 0x1000), sy = t(ay), sz = t(az), sx = t(ax);
    auto set_ir = [&](s32 ir0, s32 ir1, s32 ir2, s32 ir3) {
        gte.mtc2(8, static_cast<u32>(ir0));
        gte.mtc2(9, static_cast<u32>(ir1));
        gte.mtc2(10, static_cast<u32>(ir2));
        gte.mtc2(11, static_cast<u32>(ir3));
    };
    auto mac = [&](u32 n) { return static_cast<s32>(gte.mfc2(24 + n)); };
    auto put = [&](int k, s32 v) { m.w16(out + layout[k], static_cast<u32>(v)); };

    set_ir(cx, sy, sz, cz);
    gte.op(0x190003D);
    const s32 cy = t(ay + 0x1000);
    const s32 cx_sy = mac(1), cx_sz = mac(2), cx_cz = mac(3);
    set_ir(cy, sz, cz, cx);
    put(0, cx_sy >> 16);
    gte.op(0x190003D);
    put(1, cx_sz >> 16);
    put(2, cx_cz >> 16);
    const s32 cy_sz = mac(1), cy_cz = mac(2), cy_cx = mac(3);
    gte.mtc2(8, static_cast<u32>(sy));
    gte.mtc2(9, static_cast<u32>(cz));
    gte.mtc2(10, static_cast<u32>(sz));
    put(3, cy_cx >> 16);
    gte.op(0x190003D);
    put(4, -sx >> 2);
    const s32 sy_cz = mac(1), sy_sz = mac(2);
    set_ir(sx, sy_cz >> 14, cy_sz >> 14, cy_cz >> 14);
    gte.op(0x190003D);
    const s32 a = mac(1), b = mac(2), c = mac(3);
    gte.mtc2(8, static_cast<u32>(sx));
    gte.mtc2(9, static_cast<u32>(sy_sz >> 14));
    put(5, (a - cy_sz) >> 16);
    gte.op(0x190003D);
    put(6, (b - sy_cz) >> 16);
    put(7, (sy_sz + c) >> 16);
    put(8, (cy_cz + static_cast<s32>(gte.mfc2(25))) >> 16);
}

void Game::rot_matrix_yxz(u32 ang, u32 out) {
    cover(0x80030778u);
    static constexpr u8 layout[9] = {4, 6, 8, 16, 10, 2, 12, 14, 0};
    rotation_from_angles(ang, out, layout);
}

void Game::rot_matrix_yxz_t(u32 ang, u32 out) {
    cover(0x800308F0u);
    static constexpr u8 layout[9] = {12, 2, 8, 16, 14, 6, 4, 10, 0};
    rotation_from_angles(ang, out, layout);
}

void Game::load_scratch_rotation() {
    for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(g::kScratch + 4 * r));
}

static s32 sqrt_of_sum(Memory& m, Gte& gte, u32 scale_shift) {
    const s32 sum = static_cast<s32>(gte.mfc2(25)) + static_cast<s32>(gte.mfc2(26)) + static_cast<s32>(gte.mfc2(27));
    gte.mtc2(30, static_cast<u32>(sum));
    if (sum <= 0) return 0;
    const s32 lz = static_cast<s32>(gte.mfc2(31)) & 0xFE;
    const s32 top = lz < 0x18 ? wrap::sar(sum, static_cast<u32>(0x18 - lz))
                              : static_cast<s32>(wrap::shl(static_cast<u32>(sum), static_cast<u32>(lz - 0x18)));
    const s32 t = m.r16s(g::kSqrtTable + static_cast<u32>((top - 0x40) * 2));
    const s32 up = (0x1F - lz) >> 1;
    return static_cast<s32>(wrap::shl(static_cast<u32>(static_cast<s32>(wrap::shl(static_cast<u32>(t), static_cast<u32>(up))) >> 12),
                                      scale_shift));
}

s32 Game::distance(u32 a, u32 b) {
    cover(0x80083DFCu);
    const s32 dx = m.r32s(a) - m.r32s(b), dy = m.r32s(a + 4) - m.r32s(b + 4), dz = m.r32s(a + 8) - m.r32s(b + 8);
    const s32 x = dx >> 12, y = dy >> 12, z = dz >> 12;
    gte.mtc2(9, static_cast<u32>(x));
    gte.mtc2(10, static_cast<u32>(y));
    gte.mtc2(11, static_cast<u32>(z));
    gte.op(0xA00428);
    const s32 manhattan = (x < 0 ? -x : x) + (y < 0 ? -y : y) + (z < 0 ? -z : z);
    if (0x58000 < manhattan) return 0x100000;
    u32 shift = 0;
    if (0xB000 < manhattan) {
        gte.mtc2(9, static_cast<u32>(dx >> 15));
        gte.mtc2(10, static_cast<u32>(dy >> 15));
        gte.mtc2(11, static_cast<u32>(dz >> 15));
        shift = 3;
        gte.op(0xA00428);
    }
    return sqrt_of_sum(m, gte, shift);
}

s32 Game::length3(s32 x, s32 y, s32 z) {
    cover(0x80083F40u);
    gte.mtc2(9, static_cast<u32>(x));
    gte.mtc2(10, static_cast<u32>(y));
    gte.mtc2(11, static_cast<u32>(z));
    gte.op(0xA00428);
    return sqrt_of_sum(m, gte, 0);
}

s32 Game::ratan2(s32 x, s32 y) {
    cover(0x80030D54u);
    Frame f(*this, 32);
    s32 base[8];
    for (u32 i = 0; i < 8; ++i) {
        base[i] = m.r32s(g::kAtanOctant + 4 * i);
        m.w32(f.at(4 * i), static_cast<u32>(base[i]));
    }
    if (y == 0 && x == 0) return 0;
    s32 oct = 0;
    bool neg = false;
    if (x < 0) {
        oct = 4;
        x = -x;
        neg = true;
    }
    bool steep = x < y;
    if (y < 0) {
        oct += 2;
        y = -y;
        neg = !neg;
        steep = x < y;
    }
    s32 q = 0;
    if (steep) {
        q = static_cast<s32>(wrap::div(static_cast<s32>(static_cast<u32>(x) << 11), y).lo);
    } else {
        if (y != x) {
            ++oct;
            neg = !neg;
        }
        q = static_cast<s32>(wrap::div(static_cast<s32>(static_cast<u32>(y) << 11), x).lo);
    }
    s32 a = m.r16s(g::kAtanTable + static_cast<u32>(q * 2));
    if (neg) a = -a;
    a += base[oct & 7];
    if (a >= 0x2000) a -= 0x4000;
    return a;
}

s32 Game::off_axis_angle(u32 v) {
    cover(0x80045090u);
    const s32 x = m.r32s(v), y = m.r32s(v + 4);
    const s32 manhattan = (x < 0 ? -x : x) + (y < 0 ? -y : y);
    s32 r;
    if (manhattan > 360447) {
        r = 0x100000;
    } else {
        const u32 sh = manhattan <= 45055 ? 0u : 3u;
        const s32 xs = x >> sh, ys = y >> sh;
        r = static_cast<s32>(wrap::shl(static_cast<u32>(square_root0(xs * xs + ys * ys)), sh));
    }
    const s32 a = ratan2(r, m.r32s(v + 8));
    return a < 0 ? -a : a;
}

void Game::to_local(u32 obj, u32 target, u32 out) {
    cover(0x80044F94u);
    Frame f(*this, 80);
    const u32 mat = f.at(0x10), v = f.at(0x30);
    rot_matrix_yxz_t(obj + 16, mat);
    m.w32(v, static_cast<u32>((m.r32s(target) - m.r32s(obj)) >> 12));
    m.w32(v + 4, static_cast<u32>((m.r32s(target + 4) - m.r32s(obj + 4)) >> 12));
    m.w32(v + 8, static_cast<u32>((m.r32s(target + 8) - m.r32s(obj + 8)) >> 12));
    apply_matrix_lv(mat, v, out);
}

void Game::to_frame(u32 origin, u32 mat, u32 target, u32 out) {
    cover(0x80045024u);
    Frame f(*this, 40);
    const u32 v = f.at(0x10);
    m.w32(v, static_cast<u32>((m.r32s(target) - m.r32s(origin)) >> 12));
    m.w32(v + 4, static_cast<u32>((m.r32s(target + 4) - m.r32s(origin + 4)) >> 12));
    m.w32(v + 8, static_cast<u32>((m.r32s(target + 8) - m.r32s(origin + 8)) >> 12));
    apply_matrix_lv(mat, v, out);
}

void Game::from_local(u32 obj, u32 offset, u32 out) {
    cover(0x800451ACu);
    Frame f(*this, 64);
    const u32 mat = f.at(0x10);
    rot_matrix_yxz(obj + 16, mat);
    apply_matrix_lv(mat, offset, out);
    m.w32(out, m.r32(out) + m.r32(obj));
    const u32 z = m.r32(out + 8);
    m.w32(out + 4, m.r32(out + 4) + m.r32(obj + 4));
    m.w32(out + 8, z + m.r32(obj + 8));
}

void Game::rot_matrix_yxz_14(u32 ang, u32 out) {
    cover(0x80030A68u);
    auto t = [&](u32 i) { return static_cast<s32>(m.r16s(g::kSinCos + 2u * i)); };
    const u32 ax = m.r16(ang) & 0x3FFFu, ay = m.r16(ang + 2) & 0x3FFFu, az = m.r16(ang + 4) & 0x3FFFu;
    const s32 cx = t(ax + 0x1000), cz = t(az + 0x1000), sy = t(ay), sz = t(az), sx = t(ax);
    auto set_ir = [&](s32 ir0, s32 ir1, s32 ir2, s32 ir3) {
        gte.mtc2(8, static_cast<u32>(ir0));
        gte.mtc2(9, static_cast<u32>(ir1));
        gte.mtc2(10, static_cast<u32>(ir2));
        gte.mtc2(11, static_cast<u32>(ir3));
    };
    auto mac = [&](u32 n) { return static_cast<s32>(gte.mfc2(24 + n)); };
    set_ir(cx, sy, sz, cz);
    gte.op(0x190003D);
    const s32 cy = t(ay + 0x1000);
    const s32 cx_sy = mac(1), cx_sz = mac(2), cx_cz = mac(3);
    set_ir(cy, sz, cz, cx);
    m.w16(out + 4, static_cast<u32>(cx_sy >> 14));
    gte.op(0x190003D);
    m.w16(out + 6, static_cast<u32>(cx_sz >> 14));
    m.w16(out + 8, static_cast<u32>(cx_cz >> 14));
    const s32 cy_sz = mac(1), cy_cz = mac(2), cy_cx = mac(3);
    gte.mtc2(8, static_cast<u32>(sy));
    gte.mtc2(9, static_cast<u32>(cz));
    gte.mtc2(10, static_cast<u32>(sz));
    m.w16(out + 16, static_cast<u32>(cy_cx >> 14));
    gte.op(0x190003D);
    m.w16(out + 10, static_cast<u32>(-sx));
    const s32 sy_cz = mac(1), sy_sz = mac(2);
    set_ir(sx, sy_cz >> 14, cy_sz >> 14, cy_cz >> 14);
    gte.op(0x190003D);
    const s32 a = mac(1), b = mac(2), c = mac(3);
    gte.mtc2(8, static_cast<u32>(sx));
    gte.mtc2(9, static_cast<u32>(sy_sz >> 14));
    m.w16(out + 2, static_cast<u32>((a - cy_sz) >> 14));
    gte.op(0x190003D);
    m.w16(out + 12, static_cast<u32>((b - sy_cz) >> 14));
    m.w16(out + 14, static_cast<u32>((sy_sz + c) >> 14));
    m.w16(out + 0, static_cast<u32>((cy_cz + mac(1)) >> 14));
}

void Game::rot_matrix_xzy_14(u32 ang, u32 out) {
    cover(0x80030BDCu);
    auto t = [&](u32 i) { return static_cast<s32>(m.r16s(g::kSinCos + 2u * i)); };
    const u32 ax = m.r16(ang) & 0x3FFFu, ay = m.r16(ang + 2) & 0x3FFFu, az = m.r16(ang + 4) & 0x3FFFu;
    const s32 cx = t(ax + 0x1000), cy = t(ay + 0x1000), sy = t(ay), sz = t(az), sx = t(ax);
    auto set_ir = [&](s32 ir0, s32 ir1, s32 ir2, s32 ir3) {
        gte.mtc2(8, static_cast<u32>(ir0));
        gte.mtc2(9, static_cast<u32>(ir1));
        gte.mtc2(10, static_cast<u32>(ir2));
        gte.mtc2(11, static_cast<u32>(ir3));
    };
    auto mac = [&](u32 n) { return static_cast<s32>(gte.mfc2(24 + n)); };
    set_ir(cx, sz, sy, cy);
    gte.op(0x190003D);
    const s32 cz = t(az + 0x1000);
    const s32 cx_sz = mac(1), cx_sy = mac(2), cx_cy = mac(3);
    set_ir(cz, cx, cy, sy);
    m.w16(out + 2, static_cast<u32>(-cx_sz >> 14));
    gte.op(0x190003D);
    m.w16(out + 12, static_cast<u32>(-cx_sy >> 14));
    m.w16(out + 14, static_cast<u32>(sx));
    const s32 cz_cx = mac(1), cz_cy = mac(2), cz_sy = mac(3);
    gte.mtc2(8, static_cast<u32>(sz));
    gte.mtc2(9, static_cast<u32>(cy));
    gte.mtc2(10, static_cast<u32>(sy));
    m.w16(out + 16, static_cast<u32>(cx_cy >> 14));
    gte.op(0x190003D);
    m.w16(out + 8, static_cast<u32>(cz_cx >> 14));
    const s32 sz_sy = mac(2), sz_cy = mac(1);
    set_ir(sx, cz_sy >> 14, cz_cy >> 14, sz_sy >> 14);
    gte.op(0x190003D);
    const s32 a = mac(1), b = mac(2), c = mac(3);
    gte.mtc2(8, static_cast<u32>(sx));
    gte.mtc2(9, static_cast<u32>(sz_cy >> 14));
    m.w16(out + 0, static_cast<u32>((cz_cy - c) >> 14));
    gte.op(0x190003D);
    m.w16(out + 6, static_cast<u32>((sz_cy + a) >> 14));
    m.w16(out + 10, static_cast<u32>((sz_sy - b) >> 14));
    m.w16(out + 4, static_cast<u32>((cz_sy + mac(1)) >> 14));
}

s32 Game::asin_table(s32 v) {
    cover(0x80030EB8u);
    s32 a = v < 0 ? -v : v;
    if (a >= 16385) a = 0x4000;
    const s32 r = m.r16s(0x8001B4B4u + 2u * static_cast<u32>(a));
    return v < 0 ? -r : r;
}

void Game::rotate_by_angles(u32 vec, u32 ang, u32 out) {
    cover(0x80030EF8u);
    Frame f(*this, 64);
    const u32 mat = f.at(0x10);
    rot_matrix_yxz_14(ang, mat);
    apply_matrix_lv(mat, vec, out);
    const s32 z = m.r32s(out + 8);
    m.w32(out, static_cast<u32>(m.r32s(out) >> 2));
    const s32 y = m.r32s(out + 4);
    m.w32(out + 8, static_cast<u32>(z >> 2));
    m.w32(out + 4, static_cast<u32>(y >> 2));
}

void Game::attitude_integrate(u32 ang, u32 rates) {
    cover(0x80030F64u);
    attitude_step(ang, rates, ang);
}

void Game::attitude_integrate_to(u32 ang, u32 rates, u32 out) {
    cover(0x800313A8u);
    attitude_step(ang, rates, out);
}

void Game::attitude_step(u32 ang, u32 rates, u32 out) {
    Frame f(*this, 152);
    const u32 r = f.at(0x30), mat = f.at(0x10), va = f.at(0x38), vb = f.at(0x48), a = f.at(0x58), b = f.at(0x68);
    for (u32 k = 0; k < 16; k += 4) m.w32(va + k, m.r32(0x800234D8u + k));
    for (u32 k = 0; k < 16; k += 4) m.w32(vb + k, m.r32(0x800234E8u + k));
    const u32 r0 = m.r16(rates);
    m.w16(r, r0);
    m.w16(r + 2, m.r16(rates + 2));
    m.w16(r + 4, m.r16(rates + 4));
    auto pitch = [&] {
        const s32 p = m.r16s(ang);
        return p < 0 ? -p : p;
    };
    auto boost = [&](u32 at, s32 limit) {
        const s32 v = m.r16s(at);
        if ((v < 0 ? -v : v) < limit) m.w16(at, static_cast<u32>(m.r16(at) << 1));
    };
    if (pitch() >= 0xF91) {
        const s32 v = static_cast<s16>(r0);
        if ((v < 0 ? -v : v) < 8) m.w16(r, r0 << 1);
    }
    if (pitch() >= 0xF89) boost(r + 2, 8);
    if (pitch() >= 0xFC9) boost(r, 0xC);
    if (pitch() >= 0xFC1) boost(r + 2, 0xC);
    if (pitch() >= 0xFE1) boost(r, 0x10);
    if (pitch() >= 0xFDB) boost(r + 2, 0x10);
    auto load_rt = [&] {
        for (u32 k = 0; k < 5; ++k) gte.ctc2(k, m.r32(mat + 4 * k));
    };
    rot_matrix_yxz_14(r, mat);
    load_rt();
    apply_rot_matrix_lv(va, a);
    apply_rot_matrix_lv(vb, b);
    rot_matrix_yxz_14(ang, mat);
    load_rt();
    apply_rot_matrix_lv(a, a);
    apply_rot_matrix_lv(b, b);
    const s32 ay = -m.r32s(a + 4) >> 4;
    m.w32(b, static_cast<u32>(m.r32s(b) >> 4));
    m.w32(b + 8, static_cast<u32>(m.r32s(b + 8) >> 4));
    m.w32(b + 4, static_cast<u32>(m.r32s(b + 4) >> 4));
    s32 p = asin_table(ay);
    const s32 heading = ratan2(-m.r32s(a), m.r32s(a + 8));
    s32 h = -heading;
    m.w16(r, static_cast<u32>(-p));
    m.w16(r + 2, static_cast<u32>(heading));
    m.w16(r + 4, 0);
    rot_matrix_xzy_14(r, mat);
    load_rt();
    apply_rot_matrix_lv(b, b);
    const s32 bx = m.r32s(b) >> 2, by = m.r32s(b + 4) >> 2, bz = m.r32s(b + 8) >> 2;
    m.w32(b, static_cast<u32>(bx));
    m.w32(b + 4, static_cast<u32>(by));
    m.w32(b + 8, static_cast<u32>(bz));
    s32 roll = ratan2(bx, -by);
    if (p > 0x1000) {
        p = 0x2000 - p;
        h += 0x2000;
        roll += 0x2000;
    }
    if (p < -0x1000) {
        p = -0x2000 - p;
        h += 0x2000;
        roll += 0x2000;
    }
    if (h >= 0x2000) h -= 0x4000;
    if (h < -0x2000) h += 0x4000;
    if (roll >= 0x2000) roll -= 0x4000;
    if (roll < -0x2000) roll += 0x4000;
    m.w16(out, static_cast<u32>(p));
    m.w16(out + 2, static_cast<u32>(h));
    m.w16(out + 4, static_cast<u32>(roll));
}

s32 Game::steer_to(u32 obj, u32 target, u32 turn) {
    cover(0x800319C8u);
    Frame f(*this, 104);
    const u32 ang = f.at(0x40), mat = f.at(0x10), v = f.at(0x30);
    m.w16(ang, m.r16(obj + 0x10));
    m.w16(ang + 4, 0);
    m.w16(ang + 2, m.r16(obj + 0x12));
    rot_matrix_yxz_t(ang, mat);
    for (u32 k = 0; k < 5; ++k) gte.ctc2(k, m.r32(mat + 4 * k));
    s32 dx = (m.r32s(obj) - m.r32s(target)) >> 12;
    s32 dy = (m.r32s(obj + 4) - m.r32s(target + 4)) >> 12;
    s32 dz = (m.r32s(obj + 8) - m.r32s(target + 8)) >> 12;
    s32 dist = 0x100000;
    if (!(0xAFFF < (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) + (dz < 0 ? -dz : dz))) dist = length3(dx, dy, dz);
    m.w32(v, static_cast<u32>(dx));
    m.w32(v + 4, static_cast<u32>(dy));
    m.w32(v + 8, static_cast<u32>(dz));
    apply_rot_matrix_lv(v, v);
    dx = m.r32s(v);
    dy = m.r32s(v + 4);
    dz = m.r32s(v + 8);
    const s32 flat = (dx < 0 ? -dx : dx) + (dz < 0 ? -dz : dz);
    s32 horiz = 0x100000;
    if (!(0x57FFF < flat)) {
        const u32 sh = 0xAFFF < flat ? 3u : 0u;
        horiz = static_cast<s32>(wrap::shl(static_cast<u32>(length3(wrap::sar(dx, sh), 0, wrap::sar(dz, sh))), sh));
    }
    m.w16(turn, static_cast<u32>(ratan2(dy, horiz)));
    m.w16(turn + 2, static_cast<u32>(-ratan2(dx, -dz)));
    m.w16(turn + 4, static_cast<u32>(-ratan2(dx, dy)));
    const s32 yaw = m.r16s(turn + 2);
    if ((yaw < 0 ? -yaw : yaw) > 0x1000) dist = -dist;
    return dist;
}
}

#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kHitNormal = 0x800BD0C0u;
constexpr u32 kHitFlags = 0x800BD0C4u;
constexpr u32 kDirector = 0x800BD0A8u;
constexpr u32 kShotOrder = 0x800B4104u;
constexpr u32 kDebugText = 0x800BCB98u;
constexpr u32 kHexDigits = 0x80010124u;
s32 absv(s32 v) { return v < 0 ? -v : v; }
}

void Game::transpose_matrix(u32 mat, u32 out) {
    cover(0x800A7448u);
    const u32 w0 = m.r32(mat), w1 = m.r32(mat + 4);
    m.w32(out + 4, w0);
    m.w32(out, w1);
    m.w16(out, w0);
    const u32 w2 = m.r32(mat + 8), w3 = m.r32(mat + 12);
    m.w32(out + 12, w2);
    m.w32(out + 8, w3);
    m.w16(out + 12, w1);
    m.w16(out + 8, w2);
    const u32 m22 = m.r16(mat + 16);
    m.w16(out + 4, w3);
    m.w16(out + 16, m22);
}

void Game::view_matrices(u32 view, u32 out, u32 out_t) {
    cover(0x800332E4u);
    Frame f(*this, 32);
    rot_matrix_yxz(view + 20, out);
    transpose_matrix(out, out_t);
}

void Game::view_update(u32 v) {
    cover(0x8007B438u);
    Frame f(*this, 56);
    const u32 old = f.at(0x10);
    m.w32(old, m.r32(v + 4));
    m.w32(old + 4, m.r32(v + 8));
    m.w32(old + 8, m.r32(v + 12));
    m.w16(f.at(0x20), m.r16(v + 20));
    m.w16(f.at(0x22), m.r16(v + 22));
    m.w16(f.at(0x24), m.r16(v + 24));
    if (absv(m.r16s(v + 180)) >= 49 || absv(m.r16s(v + 182)) >= 49 || absv(m.r16s(v + 184)) >= 49)
        m.w32(v, m.r32(v) | 0x200u);
    else
        m.w32(v, m.r32(v) & ~0x200u);
    const u32 world = m.r32(g::kWorld);
    if (m.r32(world) & 0x40u) {
        const u32 sc = m.r32(g::kScene);
        m.w32(world, m.r32(world) & ~0x40u);
        const u32 a = m.r32(sc + 296), c = m.r32(sc + 304);
        m.w32(sc + 296, a & 0xFFF0FFFFu);
        const u32 b = m.r32(sc + 300);
        m.w32(sc + 304, c & 0xFFF0FFFFu);
        m.w32(sc + 300, b & 0xFFF0FFFFu);
    }
    const u32 sc = m.r32(g::kScene);
    const s32 mode = m.r32s(v + 80);
    auto next_slot = [&](u32 k) {
        while (k < 64 && (~m.r16(slot(k)) & 0x4000u)) ++k;
        return k;
    };
    switch (mode) {
    case 256: cam_orbit_sway(v); break;
    case 16: cam_orbit_turn(v); break;
    case 64: cam_orbit_pull(v); break;
    case 32: cam_orbit_rise(v); break;
    case 4: cam_spin(v); break;
    case 2:
        cam_look(v);
        if (m.r32(sc + 300) & 0x4000u) {
            view_set(v, 1, slot(0));
            m.w8(m.r32(g::kScene) + 927, 0);
        }
        break;
    case 1:
        cam_chase(v);
        if (m.r32(sc + 300) & 0x4000u) {
            view_set(v, 2, slot(m.r32(v) & 0x3Fu));
            m.w8(m.r32(g::kScene) + 927, 1);
        }
        break;
    case 1024: cam_chase(v); break;
    case 512:
        replay_director(v);
        replay_look(v);
        if (!m.r8(m.r32(g::kScene) + 933)) m.w32(v, m.r32(v) | 0x800u);
        break;
    case 2048: {
        cam_target(v, m.r32(v + 192));
        const u32 s2 = m.r32(g::kScene);
        const u32 held = m.r32(s2 + 300) & 0x60F800u;
        if (m.r32(s2) & 0x20000000u) break;
        const s32 n = m.r32s(v + 208) + 1;
        m.w32(v + 208, static_cast<u32>(n));
        if (n < 181 && !held) break;
        view_set(v, m.r8(s2 + 927) ? 2 : 1, slot(0));
        view_flag(1);
        break;
    }
    case 0x20000000: {
        const u32 pad = m.r16(sc + 94);
        if (pad & 0x8000u) cam_debug_orbit(v, pad);
        else cam_look(v);
        if (!(m.r32(m.r32(g::kScene) + 300) & 0x4000u)) break;
        u32 k = (m.r32(v) & 0x3Fu) + 1;
        if (k < 64) {
            k = next_slot(k);
            if (k < 64) view_set(v, m.r32(v + 80), slot(k));
        }
        if (k == 64) {
            view_set(v, 1, slot(0));
            view_flag(1);
        }
        break;
    }
    case 0x40000000: {
        cam_debug_free(v + 4, v + 100, v + 84, m.r16(sc + 94));
        if (!(m.r32(m.r32(g::kScene) + 300) & 0x4000u)) break;
        u32 k = (m.r32(v) & 0x3Fu) + 1;
        if (k >= 64) break;
        for (u32 p = k;;) {
            ++k;
            if (!(~m.r16(slot(p)) & 0x4000u)) {
                view_set(v, 0x20000000u, slot(p));
                break;
            }
            ++p;
            if (k >= 64) break;
        }
        break;
    }
    default: break;
    }
    if (m.r32(v) & 0x100u) {
        const u32 s = m.r32(v + 52);
        m.w32(v + 4, m.r32(s));
        m.w32(v + 8, m.r32(s + 4));
        m.w32(v + 12, m.r32(s + 8));
        m.w32(v + 4, m.r32(v + 4) + m.r32(v + 56));
        m.w32(v + 8, m.r32(v + 8) + m.r32(v + 60));
        m.w32(v + 12, m.r32(v + 12) + m.r32(v + 64));
        m.w16(v + 20, m.r16(s + 16));
        m.w16(v + 22, m.r16(s + 18));
        m.w16(v + 24, m.r16(s + 20));
        m.w16(v + 20, m.r16(v + 20) + m.r16(v + 72));
        m.w16(v + 22, m.r16(v + 22) + m.r16(v + 74));
        const s32 p = m.r16s(v + 20);
        m.w16(v + 24, m.r16(v + 24) + m.r16(v + 76));
        if (p >= 4097) {
            m.w16(v + 20, static_cast<u32>(0x2000 - p));
            const u32 r = m.r16(v + 24) + 0x2000u;
            m.w16(v + 22, m.r16(v + 22) + 0x2000u);
            m.w16(v + 24, r);
        }
        if (m.r16s(v + 20) < -4096) {
            m.w16(v + 20, static_cast<u32>(-8192 - m.r16s(v + 20)));
            const u32 r = m.r16(v + 24) + 0x2000u;
            m.w16(v + 22, m.r16(v + 22) + 0x2000u);
            m.w16(v + 24, r);
        }
        for (u32 o : {22u, 24u}) {
            if (m.r16s(v + o) >= 0x2000) m.w16(v + o, m.r16(v + o) - 0x4000u);
            if (m.r16s(v + o) < -8192) m.w16(v + o, m.r16(v + o) + 0x4000u);
        }
    }
    if ((m.r32(v) & 0x800u) && (~m.r32(v) & 0x1000u)) {
        m.w32(v, m.r32(v) & ~0x800u);
        if (~m.r8(m.r32(g::kWorld)) & 0x10u)
            m.w32(v + 8, m.r32(v + 8) + static_cast<u32>(view_ground_clearance(v + 4)));
        else
            view_clamp_box(v + 124, v + 140, v + 4);
    }
    if (~m.r16(v) & 0x400u) {
        m.w32(v + 156, m.r32(v + 4) - m.r32(old));
        m.w32(v + 160, m.r32(v + 8) - m.r32(old + 4));
        m.w32(v + 164, m.r32(v + 12) - m.r32(old + 8));
        m.w16(v + 172, m.r16(v + 20) - m.r16(f.at(0x20)));
        m.w16(v + 174, m.r16(v + 22) - m.r16(f.at(0x22)));
        m.w16(v + 176, m.r16(v + 24) - m.r16(f.at(0x24)));
    } else {
        m.w32(v, m.r32(v) & ~0x400u);
        for (u32 o : {156u, 160u, 164u}) m.w32(v + o, 0);
        for (u32 o : {172u, 174u, 176u}) m.w16(v + o, 0);
    }
    if (m.r32(m.r32(g::kScene)) & 1u) debug_view_text(v);
}

void Game::view_clamp_box(u32 lo, u32 hi, u32 pos) {
    cover(0x80079CFCu);
    if (m.r32s(pos) < m.r32s(lo)) m.w32(pos, m.r32(lo));
    if (m.r32s(pos + 4) < m.r32s(lo + 4)) m.w32(pos + 4, m.r32(lo + 4));
    if (m.r32s(pos + 8) < m.r32s(lo + 8)) m.w32(pos + 8, m.r32(lo + 8));
    if (m.r32s(hi) < m.r32s(pos)) m.w32(pos, m.r32(hi));
    if (m.r32s(hi + 4) < m.r32s(pos + 4)) m.w32(pos + 4, m.r32(hi + 4));
    if (m.r32s(hi + 8) < m.r32s(pos + 8)) m.w32(pos + 8, m.r32(hi + 8));
}

s32 Game::view_ground_clearance(u32 pos) {
    cover(0x80079CB0u);
    Frame f(*this, 40);
    const s32 d = ground_probe(pos, f.at(0x10), f.at(0x18)) - 540672 - m.r32s(pos + 4);
    return d < 0 ? d : 0;
}

s32 Game::ground_probe(u32 pos, u32 normal, u32 flags) {
    cover(0x80079B1Cu);
    Frame f(*this, 80);
    const s32 px = m.r32s(pos), pz = m.r32s(pos + 8);
    s32 x = (px >> 23) + 128, z = (pz >> 23) + 128;
    if (x < 0) x &= 7;
    if (z < 0) z &= 7;
    if (x >= 256) x = (x & 7) + 248;
    const s32 zz = (z >= 256 ? (z & 7) + 248 : z) << 8;
    auto no_normal = [&] {
        if (!normal) return;
        m.w16(normal, 0);
        m.w16(normal + 2, 0xF000);
        m.w16(normal + 4, 0);
    };
    if (!(m.r16(m.r32(m.r32(g::kWorld) + 584) + 2 * static_cast<u32>(x + zz)) & 0x1FFu)) {
        no_normal();
        return 0;
    }
    const u32 a = f.at(0x10), b = f.at(0x20), out = f.at(0x30);
    m.w32(a, static_cast<u32>(px));
    m.w32(a + 4, static_cast<u32>(-12288000));
    m.w32(a + 8, m.r32(pos + 8));
    m.w32(b + 4, 0);
    m.w32(b, m.r32(pos));
    m.w32(b + 8, m.r32(pos + 8));
    if (ray_cell(a, b, out, g::kScratch) > 0x7FFFFFFE) {
        no_normal();
        if (flags) m.w32(flags, 0);
        return 0;
    }
    if (normal) {
        const u32 n = m.r32(kHitNormal);
        m.w16(normal, m.r16(n));
        m.w16(normal + 2, m.r16(n + 2));
        m.w16(normal + 4, m.r16(n + 4));
    }
    if (flags) m.w32(flags, m.r32(kHitFlags));
    return m.r32s(out + 4);
}

void Game::look_angles(u32 pos, s32 k, u32 out) {
    cover(0x800779C8u);
    Frame f(*this, 24);
    if (k > 0) {
        const u32 e = slot(static_cast<u32>(k));
        if (m.r32(e) & 0x4000u) {
            aim_angles(pos, e + 8, out);
            m.w16(out + 4, 0);
            return;
        }
    }
    m.w16(out, 0);
    m.w16(out + 2, 0);
    m.w16(out + 4, 0);
}

s32 Game::aim_angles(u32 obj, u32 target, u32 out) {
    cover(0x800317F0u);
    Frame f(*this, 96);
    const u32 mat = f.at(0x10), v = f.at(0x30);
    rot_matrix_yxz_t(obj + 16, mat);
    for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(mat + 4 * r));
    s32 dx = (m.r32s(obj) - m.r32s(target)) >> 12;
    s32 dy = (m.r32s(obj + 4) - m.r32s(target + 4)) >> 12;
    s32 dz = (m.r32s(obj + 8) - m.r32s(target + 8)) >> 12;
    s32 dist = 0x100000;
    if (!(0xAFFF < absv(dx) + absv(dy) + absv(dz))) dist = length3(dx, dy, dz);
    m.w32(v, static_cast<u32>(dx));
    m.w32(v + 4, static_cast<u32>(dy));
    m.w32(v + 8, static_cast<u32>(dz));
    apply_rot_matrix_lv(v, v);
    dx = m.r32s(v);
    dy = m.r32s(v + 4);
    dz = m.r32s(v + 8);
    const s32 h = absv(dx) + absv(dz);
    s32 flat = 0x100000;
    if (!(0x57FFF < h)) {
        const s32 sh = 0xAFFF < h ? 3 : 0;
        flat = length3(dx >> sh, 0, dz >> sh) << sh;
    }
    m.w16(out, static_cast<u32>(ratan2(dy, flat)));
    m.w16(out + 2, static_cast<u32>(-ratan2(dx, -dz)));
    const s32 roll = -ratan2(dx, dy);
    const bool behind = absv(m.r16s(out + 2)) >= 4097;
    m.w16(out + 4, static_cast<u32>(roll));
    return behind ? -dist : dist;
}

namespace {
s32 ease_step(s32 cur, s32 want, bool gap_first) {
    const s32 d = absv(cur - want);
    return gap_first ? (d + 2) >> 2 : (d >> 2) + 2;
}
}

void Game::ease_look(u32 v, s32 p, s32 y, s32 r, bool gap_first) {
    const s32 q0 = ease_step(m.r16s(v + 180), p, gap_first);
    const s32 q1 = ease_step(m.r16s(v + 182), y, gap_first);
    const s32 q2 = ease_step(m.r16s(v + 184), r, gap_first);
    const s32 want[3] = {p, y, r}, q[3] = {q0, q1, q2};
    for (u32 k = 0; k < 3; ++k) {
        const u32 at = v + 180 + 2 * k;
        if (m.r16s(at) < want[k]) m.w16(at, m.r16(at) + static_cast<u32>(q[k]));
        if (want[k] < m.r16s(at)) m.w16(at, m.r16(at) - static_cast<u32>(q[k]));
    }
    for (u32 k = 0; k < 3; ++k) {
        const u32 at = v + 180 + 2 * k;
        const s32 d = m.r16s(at) - want[k];
        if (d >= 0 ? d < q[k] : want[k] - m.r16s(at) < q[k]) m.w16(at, static_cast<u32>(want[k]));
    }
}

void Game::look_buttons(s32& p, s32& y) {
    const u32 sc = m.r32(g::kScene);
    const u32 k = m.r32(sc + 312);
    if (k & 0x10u) p -= m.r32s(sc + 332);
    if (k & 0x20u) y += m.r32s(sc + 336);
    const u32 b = m.r32(sc + 296);
    if (b & 0x100000u) y += 0x2000;
    if (b & 0x10000u) p += 0x1000;
    if (b & 0x20000u) p -= 0x1000;
    if (b & 0x40000u) y -= 0x1000;
    if (b & 0x80000u) y += 0x1000;
}

void Game::look_wrap_yaw(u32 v, s32 y) {
    if (m.r16s(v + 182) - y < -8192) m.w16(v + 182, m.r16(v + 182) + 0x4000u);
    if (m.r16s(v + 182) - y >= 8193) m.w16(v + 182, m.r16(v + 182) - 0x4000u);
}

bool Game::look_snap_held() {
    const u32 sc = m.r32(g::kScene);
    return (m.r32(sc + 300) & 0x100000u) || (m.r32(sc + 308) & 0x100000u);
}

void Game::look_relative(u32 v, u32 out) {
    if (m.r32(v) & 0x100u) {
        attitude_integrate_to(m.r32(v + 52) + 16, v + 180, out);
        const u32 s = m.r32(v + 52);
        m.w16(v + 72, m.r16(out) - m.r16(s + 16));
        m.w16(v + 74, m.r16(out + 2) - m.r16(s + 18));
        m.w16(v + 76, m.r16(out + 4) - m.r16(s + 20));
    } else {
        attitude_integrate_to(v + 20, v + 180, out);
        m.w16(v + 72, m.r16(out) - m.r16(v + 20));
        m.w16(v + 74, m.r16(out + 2) - m.r16(v + 22));
        m.w16(v + 76, m.r16(out + 4) - m.r16(v + 24));
    }
}

void Game::cam_chase(u32 v) {
    cover(0x80077A30u);
    Frame f(*this, 32);
    const u32 out = f.at(0x10);
    s32 k = -1;
    if (m.r32(m.r32(g::kScene) + 296) & 0x2000u) k = m.r32s(v + 188);
    look_angles((m.r32(v) & 0x100u) ? m.r32(v + 52) : v + 4, k, out);
    s32 p = m.r16s(out), y = m.r16s(out + 2);
    const s32 r = m.r16s(out + 4);
    look_buttons(p, y);
    look_wrap_yaw(v, y);
    if (look_snap_held()) {
        m.w16(v + 180, static_cast<u32>(p));
        m.w16(v + 182, static_cast<u32>(y));
        m.w16(v + 184, static_cast<u32>(r));
        view_mark(v);
    } else {
        ease_look(v, p, y, r, false);
    }
    look_relative(v, out);
}

void Game::look_camera(u32 v, s32 k) {
    Frame f(*this, 128);
    const u32 ang = f.at(0x10), vec = f.at(0x18), ma = f.at(0x28), mb = f.at(0x48);
    look_angles((m.r32(v) & 0x100u) ? m.r32(v + 52) : v + 4, k, ang);
    s32 p = m.r16s(ang), y = m.r16s(ang + 2);
    const s32 r = m.r16s(ang + 4);
    look_buttons(p, y);
    look_wrap_yaw(v, y);
    if (look_snap_held()) {
        m.w16(v + 180, static_cast<u32>(p));
        m.w16(v + 182, static_cast<u32>(y));
        m.w16(v + 184, static_cast<u32>(r));
        view_mark(v);
    } else {
        ease_look(v, p, y, r, true);
    }
    rot_matrix_yxz(m.r32(v + 52) + 16, ma);
    rot_matrix_yxz(v + 180, mb);
    mul_matrix(ma, mb);
    for (u32 i = 0; i < 5; ++i) gte.ctc2(i, m.r32(ma + 4 * i));
    gte.mtc2(0, m.r32(v + 44));
    gte.mtc2(1, m.r32(v + 48));
    gte.op(0x486012);
    m.w32(vec, gte.mfc2(25));
    m.w32(vec + 4, gte.mfc2(26));
    m.w32(vec + 8, gte.mfc2(27));
    m.w32(v + 56, m.r32(vec) << 6);
    m.w32(v + 60, m.r32(vec + 4) << 6);
    m.w32(v + 64, m.r32(vec + 8) << 6);
    look_relative(v, ang);
}

void Game::cam_look(u32 v) {
    cover(0x80077DD0u);
    look_camera(v, (m.r32(m.r32(g::kScene) + 296) & 0x2000u) ? m.r32s(v + 188) : -1);
}

void Game::cam_target(u32 v, u32 k) {
    cover(0x80078234u);
    look_camera(v, static_cast<s32>(k));
}

void Game::cam_spin(u32 v) {
    cover(0x8007895Cu);
    Frame f(*this, 136);
    const u32 ma = f.at(0x28), mb = f.at(0x48), vv = f.at(0x68), vec = f.at(0x18), ang = f.at(0x10);
    m.w16(v + 184, 0);
    const u32 k = (m.r32(m.r32(g::kWorld) + 12) << 7) & 0x1F80u;
    const s32 s = m.r16s(g::kSinCos + k) >> 2;
    const s32 w = 0x1000 - s;
    m.w16(v + 180, static_cast<u32>(-w));
    m.w16(v + 182, static_cast<u32>(w << 1));
    rot_matrix_yxz(m.r32(v + 52) + 16, ma);
    rot_matrix_yxz(v + 180, mb);
    mul_matrix(ma, mb);
    m.w16(vv, m.r16(v + 44));
    m.w16(vv + 2, m.r16(v + 46));
    m.w16(vv + 4, m.r16(v + 48));
    for (u32 i = 0; i < 5; ++i) gte.ctc2(i, m.r32(ma + 4 * i));
    gte.mtc2(0, m.r32(vv));
    gte.mtc2(1, m.r32(vv + 4));
    gte.op(0x486012);
    m.w32(vec, gte.mfc2(25));
    m.w32(vec + 4, gte.mfc2(26));
    m.w32(vec + 8, gte.mfc2(27));
    m.w32(v + 56, m.r32(vec) << 6);
    m.w32(v + 60, m.r32(vec + 4) << 6);
    m.w32(v + 64, m.r32(vec + 8) << 6);
    look_relative(v, ang);
}

void Game::orbit_offset(u32 v, u32 frame_sp, u32 shift) {
    const u32 e = m.r32(v + 52);
    const s32 p = m.r16s(v + 212), y = m.r16s(v + 214), r = m.r16s(v + 216);
    const s32 ep = m.r16s(e + 16), ey = m.r16s(e + 18), er = m.r16s(e + 20);
    const u32 mat = frame_sp + 0x10, vv = frame_sp + 0x30, vec = frame_sp + 0x38;
    m.w16(vv, 0);
    m.w16(vv + 2, 0);
    m.w16(vv + 4, static_cast<u32>((0 - m.r32s(v + 228)) >> shift));
    rot_matrix_yxz(v + 212, mat);
    for (u32 i = 0; i < 5; ++i) gte.ctc2(i, m.r32(mat + 4 * i));
    gte.mtc2(0, m.r32(vv));
    gte.mtc2(1, m.r32(vv + 4));
    gte.op(0x486012);
    m.w32(vec, gte.mfc2(25));
    m.w32(vec + 4, gte.mfc2(26));
    m.w32(vec + 8, gte.mfc2(27));
    m.w32(v + 56, m.r32(vec) << shift);
    m.w32(v + 60, m.r32(vec + 4) << shift);
    m.w16(v + 72, static_cast<u32>(p - ep));
    m.w16(v + 74, static_cast<u32>(y - ey));
    m.w16(v + 76, static_cast<u32>(r - er));
    m.w32(v + 64, m.r32(vec + 8) << shift);
}

namespace {
void roll_decay(Memory& m, u32 v) {
    if (m.r16s(v + 216) >= 0x11) m.w16(v + 216, m.r16(v + 216) - 0x10u);
    if (m.r16s(v + 216) < -0x10) m.w16(v + 216, m.r16(v + 216) + 0x10u);
    if (absv(m.r16s(v + 216)) < 0x10) m.w16(v + 216, 0);
}
s32 ramp(Memory& m, u32 v) {
    s32 a = m.r32s(v + 208) << 7;
    if (a >= 0x1001) a = 0x1000;
    return a;
}
void pull_back(Memory& m, u32 v, s32 limit, s32 a, s32 mul) {
    const s32 d = m.r32s(v + 228);
    if (limit < d) return;
    s32 t = a * mul;
    t = (t < 0 ? t + 0xFFF : t) >> 12;
    m.w32(v + 228, static_cast<u32>(d + (t << 12)));
}
}

void Game::cam_orbit_turn(u32 v) {
    cover(0x80078B28u);
    Frame f(*this, 96);
    if (m.r32(m.r32(g::kScene)) & 0x20000000u) return;
    const u32 n = m.r32(v + 208);
    const s32 a = ramp(m, v);
    if (!n) {
        m.w32(v + 228, static_cast<u32>((0 - m.r16s(v + 48)) << 6));
        const u32 y = m.r16(v + 22);
        m.w16(v + 214, y + 0x2000u);
        if (static_cast<s16>(y + 0x2000u) < -0x2000) m.w16(v + 214, y + 0x6000u);
        if (m.r16s(v + 214) >= 0x2001) m.w16(v + 214, m.r16(v + 214) - 0x4000u);
    }
    pull_back(m, v, 0xAEFFFF, a, 10);
    if (!(m.r16s(v + 212) < -0x7FF)) m.w16(v + 212, m.r16(v + 212) - static_cast<u32>(a >> 7));
    const u32 y = m.r16(v + 214) + static_cast<u32>(a >> 7);
    m.w16(v + 214, y);
    if (static_cast<s16>(y) >= 0x2001) m.w16(v + 214, y - 0x4000u);
    m.w32(v + 208, m.r32(v + 208) + 1);
    orbit_offset(v, f.at(0), 9);
}

void Game::cam_orbit_rise(u32 v) {
    cover(0x80078D70u);
    Frame f(*this, 96);
    if (m.r32(m.r32(g::kScene)) & 0x20000000u) return;
    const u32 n = m.r32(v + 208);
    const s32 a = ramp(m, v);
    if (!n) {
        m.w32(v + 228, static_cast<u32>(0x190000 - (m.r16s(v + 48) << 6)));
        m.w16(v + 214, m.r16(v + 22));
        m.w16(v + 216, m.r16(v + 24));
    }
    pull_back(m, v, 0xAEFFFF, a, 20);
    if (!(m.r16s(v + 212) < -0x7FF)) m.w16(v + 212, m.r16(v + 212) - static_cast<u32>(a >> 7));
    roll_decay(m, v);
    const u32 y = m.r16(v + 214) + static_cast<u32>(a >> 7);
    m.w16(v + 214, y);
    if (static_cast<s16>(y) >= 0x2001) m.w16(v + 214, y - 0x4000u);
    m.w32(v + 208, m.r32(v + 208) + 1);
    orbit_offset(v, f.at(0), 9);
}

void Game::cam_orbit_pull(u32 v) {
    cover(0x80078FDCu);
    Frame f(*this, 96);
    if (m.r32(m.r32(g::kScene)) & 0x20000000u) return;
    const u32 n = m.r32(v + 208);
    const s32 a = ramp(m, v);
    if (!n) {
        m.w32(v + 228, static_cast<u32>(0x190000 - (m.r16s(v + 48) << 6)));
        m.w16(v + 214, m.r16(v + 22));
        m.w16(v + 216, m.r16(v + 24));
    }
    pull_back(m, v, 0x3E7FFF, a, 10);
    roll_decay(m, v);
    m.w32(v + 208, m.r32(v + 208) + 1);
    orbit_offset(v, f.at(0), 9);
}

void Game::cam_orbit_sway(u32 v) {
    cover(0x800791E8u);
    Frame f(*this, 96);
    if (m.r32(m.r32(g::kScene)) & 0x20000000u) return;
    s32 t = m.r32s(v + 208) << 5;
    if (t >= 0x1001) t = 0x1000;
    s32 s = 0 - (rsin(t >> 1) << 9);
    s = ((s < 0 ? s + 0xFFF : s) >> 12) - 0xE00;
    const s32 a = 0x2000 - (rsin(t >> 2) << 1);
    m.w16(v + 212, static_cast<u32>(s));
    const u32 y = m.r16(m.r32(v + 52) + 18) + static_cast<u32>(a);
    m.w16(v + 214, y);
    if (static_cast<s16>(y) < -0x2000) m.w16(v + 214, y + 0x4000u);
    if (m.r16s(v + 214) >= 0x2001) m.w16(v + 214, m.r16(v + 214) - 0x4000u);
    m.w32(v + 228, static_cast<u32>(((0 - m.r16s(v + 48)) << 10) - (a << 9)));
    m.w32(v + 208, m.r32(v + 208) + 1);
    orbit_offset(v, f.at(0), 9);
}

void Game::cam_debug_orbit(u32 v, u32 pad) {
    cover(0x8007754Cu);
    Frame f(*this, 96);
    if ((pad & 0x40u) && m.r16s(v + 212) < 0x1000) m.w16(v + 212, m.r16(v + 212) + 0x80u);
    if ((pad & 0x10u) && !(m.r16s(v + 212) < -0xFFF)) m.w16(v + 212, m.r16(v + 212) - 0x80u);
    if (pad & 0x80u) {
        const u32 y = m.r16(v + 214);
        m.w16(v + 214, y - 0x80u);
        if (static_cast<s16>(y - 0x80u) < -0x2000) m.w16(v + 214, y + 0x3F80u);
    }
    if (pad & 0x20u) {
        const u32 y = m.r16(v + 214);
        m.w16(v + 214, y + 0x80u);
        if (static_cast<s16>(y + 0x80u) >= 0x2001) m.w16(v + 214, y - 0x3F80u);
    }
    if ((pad & 0x400u) && !(0x31FFFF < m.r32s(v + 228))) m.w32(v + 228, m.r32(v + 228) + 0xA000u);
    if ((pad & 0x800u) && m.r32s(v + 228) > 0) m.w32(v + 228, m.r32(v + 228) - 0xA000u);
    orbit_offset(v, f.at(0), 7);
}

void Game::cam_debug_free(u32 pos, u32 motion, u32 vel, u32 pad) {
    cover(0x80077004u);
    Frame f(*this, 64);
    const u32 b = f.at(0x10);
    memset(b, 0, 24);
    const s32 sh = (2 * static_cast<s32>(pad)) & 4;
    auto add16 = [&](u32 at, s32 d) { m.w16(at, m.r16(at) + static_cast<u32>(d)); };
    auto add32 = [&](u32 at, s32 d) { m.w32(at, m.r32(at) + static_cast<u32>(d)); };
    if (pad & 0x10u) add16(b, -64);
    if (pad & 0x40u) add16(b, 64);
    if (pad & 0x20u) add16(b + 4, 64);
    if (pad & 0x80u) add16(b + 4, -64);
    if (pad & 0x200u) add16(b + 2, 64);
    if (pad & 0x100u) add16(b + 2, -64);
    if ((pad & 0x14u) == 0x14u) add16(b, 60);
    if ((pad & 0x44u) == 0x44u) add16(b, -60);
    if ((pad & 0x24u) == 0x24u) add16(b + 4, -60);
    if ((pad & 0x84u) == 0x84u) add16(b + 4, 60);
    if ((pad & 0x204u) == 0x204u) add16(b + 2, -60);
    if ((pad & 0x104u) == 0x104u) add16(b + 2, 60);
    for (u32 o : {0u, 4u, 2u}) m.w16(b + o, static_cast<u32>(m.r16s(b + o) << sh));
    if (pad & 0x1000u) add32(b + 16, 0x40000);
    if (pad & 0x4000u) add32(b + 16, -0x40000);
    if (static_cast<s16>(pad) < 0) add32(b + 8, -0x40000);
    if (pad & 0x2000u) add32(b + 8, 0x40000);
    if (pad & 0x400u) add32(b + 12, -0x40000);
    if (pad & 0x800u) add32(b + 12, 0x40000);
    if ((pad & 0x1004u) == 0x1004u) add32(b + 16, -258048);
    if ((pad & 0x4004u) == 0x4004u) add32(b + 16, 258048);
    if ((pad & 0x8004u) == 0x8004u) add32(b + 8, 258048);
    if ((pad & 0x2004u) == 0x2004u) add32(b + 8, -258048);
    if ((pad & 0x404u) == 0x404u) add32(b + 12, 258048);
    if ((pad & 0x804u) == 0x804u) add32(b + 12, -258048);
    for (u32 o : {8u, 16u, 12u}) m.w32(b + o, static_cast<u32>(m.r32s(b + o) << sh));
    for (u32 k = 0; k < 4; ++k) m.w32(motion + 4 * k, m.r32(b + 8 + 4 * k));
    m.w32(motion + 16, m.r32(b));
    m.w32(motion + 20, m.r32(b + 4));
    attitude_integrate(pos + 16, motion + 16);
    rotate_by_angles(motion, pos + 16, vel);
    if ((pad & 0xC00u) == 0xC00u) {
        m.w32(pos + 16, 0xF000);
        m.w16(pos + 20, 0);
    }
    if ((pad & 6u) == 6u) {
        m.w16(pos + 16, 0);
        m.w16(pos + 18, 0);
        m.w16(pos + 20, 0);
    }
    m.w32(pos, m.r32(pos) + m.r32(vel));
    m.w32(pos + 4, m.r32(pos + 4) + m.r32(vel + 4));
    m.w32(pos + 8, m.r32(pos + 8) + m.r32(vel + 8));
}

void Game::replay_look(u32 v) {
    cover(0x8007867Cu);
    Frame f(*this, 24);
    s32 p = 0, y = 0;
    look_buttons(p, y);
    look_wrap_yaw(v, y);
    if (look_snap_held()) {
        m.w16(v + 180, static_cast<u32>(p));
        m.w16(v + 182, static_cast<u32>(y));
        m.w16(v + 184, 0);
        view_mark(v);
    } else {
        const s32 q0 = (absv(m.r16s(v + 180) - p) + 2) >> 2;
        const s32 q1 = (absv(m.r16s(v + 182) - y) + 2) >> 2;
        const s32 q2 = (absv(m.r16s(v + 184)) + 2) >> 2;
        if (m.r16s(v + 180) < p) m.w16(v + 180, m.r16(v + 180) + static_cast<u32>(q0));
        if (p < m.r16s(v + 180)) m.w16(v + 180, m.r16(v + 180) - static_cast<u32>(q0));
        if (m.r16s(v + 182) < y) m.w16(v + 182, m.r16(v + 182) + static_cast<u32>(q1));
        if (y < m.r16s(v + 182)) m.w16(v + 182, m.r16(v + 182) - static_cast<u32>(q1));
        if (m.r16s(v + 184) < 0) m.w16(v + 184, m.r16(v + 184) + static_cast<u32>(q2));
        if (m.r16s(v + 184) > 0) m.w16(v + 184, m.r16(v + 184) - static_cast<u32>(q2));
        const s32 dp = m.r16s(v + 180) - p;
        if (dp < 0 ? p - m.r16s(v + 180) < q0 : dp < q0) m.w16(v + 180, static_cast<u32>(p));
        const s32 dy = m.r16s(v + 182) - y;
        if (dy < 0 ? y - m.r16s(v + 182) < q1 : dy < q1) m.w16(v + 182, static_cast<u32>(y));
        const s32 r = m.r16s(v + 184);
        if (r < 0 ? -r < q2 : r < q2) m.w16(v + 184, 0);
    }
    attitude_integrate_to(v + 20, v + 180, v + 20);
}

void Game::replay_director(u32 v) {
    cover(0x8007AFFCu);
    Frame f(*this, 88);
    const u32 sc = m.r32(g::kScene);
    m.w32(f.at(0x10), v + 268);
    m.w32(f.at(0x18), v + 292);
    m.w32(f.at(0x14), v + 308);
    m.w32(f.at(0x1C), v + 332);
    s32 len = 450;
    s32 pick = -1;
    const bool cut = (m.r32(sc + 300) & 0x209B80u) != 0;
    m.w32(f.at(0x20), cut);
    const u32 b = m.r32(sc + 296);
    if (b & 0x209B00u) len = 0;
    m.w32(f.at(0x2C), static_cast<u32>(len));
    if (b & 0x100u) pick = 0;
    if (b & 0x200u) pick = 3;
    if (b & 0x800u) pick = 1;
    if (b & 0x1000u) pick = 2;
    if (b & 0x8000u) pick = 4;
    if (b & 0x200000u) pick = 5;
    m.w32(f.at(0x28), static_cast<u32>(pick));
    bool alt = m.r16(v + 348) && (m.r32(sc + 36) & 0x100u);
    if (m.r16(v + 348) == 2) alt = false;
    m.w32(f.at(0x24), alt);
    const u32 args = f.at(0x30);
    m.w32(args + 12, v + 44);
    if (alt) {
        m.w32(args, v + 308);
        m.w32(args + 8, v + 332);
        m.w32(args + 4, v + 268);
    } else {
        m.w32(args, v + 268);
        m.w32(args + 8, v + 292);
        m.w32(args + 4, v + 308);
    }
    const u32 state = m.r32(v + 204);
    if (state == 0) {
        view_mark(v);
        m.w32(v + 352, 0);
        for (u32 k = 0; k < 12; k += 4) m.w32(v + 244 + k, m.r32(v + 4 + k));
        m.w16(v + 260, m.r16(v + 20));
        m.w16(v + 262, m.r16(v + 22));
        m.w16(v + 264, m.r16(v + 24));
        s32 shot = pick;
        if (shot < 0) {
            const u32 n = m.r32(v + 240);
            m.w32(v + 240, n + 1);
            shot = m.r8(kShotOrder + (n & 0x1Fu));
        }
        m.w32(v + 236, static_cast<u32>(shot));
        const s32 s = m.r32s(v + 236);
        u32 table;
        if (!(~m.r8(m.r32(g::kWorld)) & 0x10u)) {
            table = m.r16(v + 348) ? 0x800BCE28u : 0x800B40ECu + 8 * static_cast<u32>(s % 3);
        } else {
            table = m.r16(v + 348) ? 0x800B40DCu + 8 * static_cast<u32>(s % 2) : 0x800B40ACu + 8 * static_cast<u32>(s % 6);
        }
        m.w32(kDirector, table);
        m.w32(v + 204, 1);
    } else if (state != 1) {
        copy_view_from_shot(v);
        return;
    }
    const u32 shot = m.r32(kDirector);
    const s32 cap = m.r16s(shot + 4);
    const u32 done = replay_shot(m.r32(shot), args, v + 352, cap < len ? cap : len);
    if (m.r16(m.r32(kDirector) + 6) == 1) {
        replay_follow(v + 356, v + 244);
        m.w32(v + 52, v + 244);
    } else {
        m.w32(v + 52, v + 356);
    }
    if (done || m.r32(f.at(0x20)) || m.r16(v + 350)) {
        m.w16(v + 350, 0);
        m.w32(f.at(0x20), 0);
        m.w32(v + 204, 0);
    }
    copy_view_from_shot(v);
}

void Game::copy_view_from_shot(u32 v) {
    const u32 s = m.r32(v + 52);
    for (u32 k = 0; k < 24; k += 4) m.w32(v + 4 + k, m.r32(s + k));
}

void Game::replay_follow(u32 a, u32 b) {
    cover(0x8007AE40u);
    s32 p = m.r16s(b + 16), y = m.r16s(b + 18), r = m.r16s(b + 20);
    const s32 ty = m.r16s(a + 18), tp = m.r16s(a + 16), tr = m.r16s(a + 20);
    if (y - ty < -8192) y += 0x4000;
    if (y - ty >= 8193) y -= 0x4000;
    if (r - tr < -8192) r += 0x4000;
    if (r - tr >= 8193) r -= 0x4000;
    const s32 qp = (absv(p - tp) + 1) >> 4, qy = (absv(y - ty) + 1) >> 3, qr = (absv(r - tr) + 1) >> 4;
    if (p < tp) p += qp;
    if (tp < p) p -= qp;
    if (y < ty) y += qy;
    if (ty < y) y -= qy;
    if (r < tr) r += qr;
    if (tr < r) r -= qr;
    if (p - tp < 0 ? tp - p < qp : p - tp < qp) p = qp;
    if (y - ty < 0 ? ty - y < qy : y - ty < qy) y = qy;
    if (r - tr < 0 ? tr - r < qr : r - tr < qr) r = qr;
    m.w16(b + 16, static_cast<u32>(p));
    m.w16(b + 18, static_cast<u32>(y));
    m.w16(b + 20, static_cast<u32>(r));
    m.w32(b, m.r32(a));
    m.w32(b + 4, m.r32(a + 4));
    m.w32(b + 8, m.r32(a + 8));
}

u32 Game::debug_text(u32 x, u32 y, u32 s) {
    cover(0x80027608u);
    u32 c = m.r8(s++);
    const u32 buf = m.r32(kDebugText);
    u32 n = m.r32(buf);
    u32 at = buf + 8 * n + 4;
    const u32 font = m.r32(buf + 4 * m.r32(buf + 0xE20) + 0xE14);
    const u32 row = y << 16;
    while (c) {
        ++n;
        if (static_cast<s32>(n) >= 451) {
            --n;
            break;
        }
        const s32 k = static_cast<s32>(c) - 32;
        const u32 col = 6 * static_cast<u32>(k & 0xF);
        const u32 line = static_cast<u32>(6 * (k >> 4) + 64) << 8;
        m.w32(at, (x & 0xFFFFu) | row);
        x += 6;
        m.w32(at + 4, col | line | font);
        at += 8;
        c = m.r8(s++);
    }
    m.w32(m.r32(kDebugText), n);
    return x;
}

u32 Game::debug_number(u32 x, u32 y, s32 value, u32 fmt) {
    cover(0x800276C8u);
    Frame f(*this, 40);
    const u32 digits = fmt & 0xFFu;
    const u32 fl = fmt & 0xF00u;
    if (digits >= 10) return 0xFFFFFFFFu;
    u32 at = f.at(0x19);
    s32 v = absv(value);
    m.w8(f.at(0x19), 0);
    m.w8(f.at(0x18), '0');
    for (u32 i = 0; i < digits; ++i) {
        --at;
        if (v) {
            m.w8(at, static_cast<u32>(v % 10 + '0'));
            v /= 10;
        } else if (i) {
            if (fl & 0x400u) {
                ++at;
                break;
            }
            m.w8(at, (fl & 0x100u) ? '0' : ' ');
        }
    }
    if (fl & 0x200u) {
        --at;
        m.w8(at, value >= 0 ? '+' : '-');
    }
    return debug_text(x, y, at);
}

u32 Game::debug_hex(u32 x, u32 y, u32 value, s32 n) {
    cover(0x800277F0u);
    Frame f(*this, 56);
    const u32 table = f.at(0x10);
    for (u32 k = 0; k < 16; ++k) m.w8(table + k, m.r8(kHexDigits + k));
    if (n >= 10) return 0xFFFFFFFFu;
    u32 at = f.at(0x28);
    m.w8(at, 0);
    s32 v = static_cast<s32>(value);
    for (s32 i = 0; i < n; ++i) {
        --at;
        m.w8(at, m.r8(table + static_cast<u32>(v & 0xF)));
        v >>= 4;
    }
    return debug_text(x, y, at);
}

void Game::debug_view_text(u32 v) {
    cover(0x80077460u);
    Frame f(*this, 32);
    const u32 x = debug_text(230, 180, 0x800BCE24u) + 12;
    debug_hex(x, 180, m.r32(v), 8);
    debug_hex(x, 186, m.r32(v + 80), 8);
    debug_number(x, 192, m.r32s(v + 4) >> 12, 519);
    debug_number(x, 198, m.r32s(v + 8) >> 12, 519);
    debug_number(x, 204, m.r32s(v + 12) >> 12, 519);
    debug_number(x, 210, m.r16s(v + 20), 519);
    debug_number(x, 216, m.r16s(v + 22), 519);
    debug_number(x, 222, m.r16s(v + 24), 519);
}
}

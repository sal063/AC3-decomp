#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kHitNormal = 0x800BD0C0u;
constexpr u32 kHitFlags = 0x800BD0C4u;
}

s32 Game::isqrt(s32 a) {
    cover(0x80083D88u);
    gte.mtc2(30, static_cast<u32>(a));
    if (a <= 0) return 0;
    const s32 lz = static_cast<s32>(gte.mfc2(31)) & 0x1E;
    const s32 top = lz < 0x18 ? wrap::sar(a, static_cast<u32>(0x18 - lz))
                              : static_cast<s32>(wrap::shl(static_cast<u32>(a), static_cast<u32>(lz - 0x18)));
    const s32 t = m.r16s(g::kSqrtTable + static_cast<u32>((top - 0x40) * 2));
    return static_cast<s32>(wrap::shl(static_cast<u32>(t), static_cast<u32>((0x1F - lz) >> 1))) >> 12;
}

bool Game::ray_plane_clip(u32 start, u32 d, s32 y, u32 out) {
    cover(0x80098C68u);
    const s32 sy = m.r32s(start + 4);
    const s32 dx = m.r16s(d), dy = m.r16s(d + 2), dz = m.r16s(d + 4);
    const s32 k = wrap::sub(y, sy);
    if (dy > 0 || dy < 0) {
        if (dy > 0 ? y < sy : sy < y) return false;
        const s32 px = static_cast<s32>(wrap::div(static_cast<s32>(static_cast<u32>(dx) * static_cast<u32>(k)), dy).lo);
        m.w32(out + 4, static_cast<u32>(k));
        m.w32(out, static_cast<u32>(px));
        m.w32(out + 8, wrap::div(static_cast<s32>(static_cast<u32>(dz) * static_cast<u32>(k)), dy).lo);
    } else {
        if (y != sy) return false;
        m.w32(out, static_cast<u32>(dx));
        m.w32(out + 8, static_cast<u32>(dz));
        m.w32(out + 4, static_cast<u32>(k));
    }
    if (dx < 0 ? m.r32s(out) < dx : dx < m.r32s(out)) {
        m.w32(out, static_cast<u32>(dx));
        m.w32(out + 4, static_cast<u32>(dy));
    }
    if (dz < 0 ? m.r32s(out + 8) < dz : dz < m.r32s(out + 8)) {
        m.w32(out + 8, static_cast<u32>(dz));
        m.w32(out + 4, static_cast<u32>(dy));
    }
    return m.r32s(out + 4) != dy;
}

void Game::ray_cells(u32 start, u32 end, u32 corners, u32 list, u32 grid) {
    cover(0x80098DBCu);
    Frame f(*this, 0xE0);
    const u32 d = f.at(0x10), p0 = f.at(0x18), p1 = f.at(0x28), marks = f.at(0x38);
    if (m.r32s(start + 4) < -3000 && m.r32s(end + 4) < -3000) {
        m.w32(list + 8, 0x200000);
        return;
    }
    m.w16(d, m.r16(end) - m.r16(start));
    const u32 d_y = m.r16(end + 4) - m.r16(start + 4);
    m.w16(d + 2, d_y);
    m.w16(d + 4, m.r16(end + 8) - m.r16(start + 8));
    auto copy3 = [&](u32 to, u32 from) {
        for (u32 k = 0; k < 12; k += 4) m.w32(to + k, m.r32(from + k));
    };
    auto add3 = [&](u32 to, u32 from) {
        for (u32 k = 0; k < 12; k += 4) m.w32(to + k, m.r32(to + k) + m.r32(from + k));
    };
    bool clip_p1_from_start = false;
    if (!(m.r32s(start + 4) < -2999) && static_cast<s16>(d_y) < 0) {
        copy3(p0, start);
        clip_p1_from_start = true;
        if (!ray_plane_clip(start, d, -3000, p1)) copy3(p1, end);
        else add3(p1, start);
    }
    if (!clip_p1_from_start) {
        if (!ray_plane_clip(start, d, -3000, p0)) copy3(p0, start);
        else add3(p0, start);
        if (!ray_plane_clip(start, d, 0, p1)) copy3(p1, end);
        else add3(p1, start);
    }

    const u32 dx = m.r16(p1) - m.r16(p0);
    const s32 dxs = static_cast<s16>(dx);
    const s32 adx = dxs < 0 ? -dxs : dxs;
    const u32 dz = m.r16(p1 + 8) - m.r16(p0 + 8);
    m.w16(d, dx);
    m.w16(d + 4, dz);
    m.w16(d + 2, m.r16(p1 + 4) - m.r16(p0 + 4));
    const u32 fx = m.r32(p0) & 0x7FFu, fz = m.r32(p0 + 8) & 0x7FFu;
    const s32 cx = adx >> 11;
    s32 nx = cx + 2;
    const s32 dzs = static_cast<s16>(dz);
    s32 nz = ((dzs < 0 ? -dzs : dzs) >> 11) + 2;
    s32 x0, z0;
    if (dxs < 0) {
        x0 = static_cast<s16>(dx & 0xF800u);
        nx = cx + 1;
    } else if (fx != 0) {
        x0 = 0;
        nx = cx + 1;
    } else {
        x0 = -2048;
    }
    if (m.r16s(d + 4) < 0) {
        z0 = static_cast<s16>(m.r16(d + 4) & 0xF800u);
        nz -= 1;
    } else if (fz != 0) {
        z0 = 0;
        nz -= 1;
    } else {
        z0 = -2048;
    }
    for (u32 k = 0; k < 32; ++k) m.w32(marks + 4 * k, 0);

    gte.mtc2(12, (fx << 16) | fz);
    gte.mtc2(13, (((fx + static_cast<u32>(m.r16s(d))) & 0xFFFFu) << 16) |
                     ((fz + static_cast<u32>(m.r16s(d + 4))) & 0xFFFFu));
    auto clamp16 = [](s32 v) -> u32 {
        if (0x7FFF < v) return 0x7FFF;
        if (v < -0x8000) return 0x8000;
        return static_cast<u32>(v) & 0xFFFFu;
    };
    if (nz + 1 >= 0) {
        const bool no_x = nx + 1 < 0;
        s32 zc = z0;
        for (s32 r = 0; r <= nz + 1; ++r, zc += 0x800) {
            const u32 sz = clamp16(zc);
            if (no_x) continue;
            s32 xc = x0;
            u32 at = static_cast<u32>(r);
            for (s32 c = 0; c <= nx + 1; ++c, xc += 0x800, at += 0x20) {
                gte.mtc2(14, sz | (clamp16(xc) << 16));
                gte.op(0x1400006);
                const s32 side = static_cast<s32>(gte.mfc2(24));
                m.w8(corners + at, side > 0 ? 1u : side < 0 ? 0xFFu : 0u);
            }
        }
    }
    if (nz >= 0) {
        for (s32 r = 0; r <= nz; ++r) {
            if (nx < 0) continue;
            for (s32 c = 0; c <= nx; ++c) {
                const u32 at = corners + static_cast<u32>(r) + 32 * static_cast<u32>(c);
                const u32 a = m.r8(at), b = m.r8(at + 1), cc = m.r8(at + 32), dd = m.r8(at + 33);
                if (a == b && a == cc && a == dd && (a & b & cc & dd & 0xFFu)) continue;
                const u32 w = marks + 4 * static_cast<u32>(r);
                m.w32(w, m.r32(w) | wrap::shl(1u, static_cast<u32>(c)));
            }
        }
    }
    const s32 p0x = m.r32s(p0), p0z = m.r32s(p0 + 8);
    const s32 cell_x = (p0x >> 11) + 0x80, cell_z = (p0z >> 11) + 0x80;
    const s32 fxs = p0x & 0x7FF, fzs = p0z & 0x7FF;
    const u32 neg_y = 0u - m.r32(start + 4);
    u32 out = list;
    if (nz >= 0) {
        s32 zc = z0;
        for (s32 r = 0; r <= nz; ++r, zc += 0x800) {
            if (nx < 0) continue;
            s32 xc = x0;
            const u32 w = m.r32(marks + 4 * static_cast<u32>(r));
            for (s32 c = 0; c <= nx; ++c, xc += 0x800) {
                if (!(w & wrap::shl(1u, static_cast<u32>(c)))) continue;
                u32 gx = static_cast<u32>(cell_x + ((xc < 0 ? xc + 0x7FF : xc) >> 11));
                u32 gz = static_cast<u32>(cell_z + ((zc < 0 ? zc + 0x7FF : zc) >> 11));
                u32 ix = static_cast<s16>(gx) < 0 ? (gx & 7u) : gx;
                u32 iz = static_cast<s16>(gz) < 0 ? (gz & 7u) : gz;
                if (static_cast<s16>(ix) >= 0x100) ix = (ix & 7u) | 0xF8u;
                if (static_cast<s16>(iz) >= 0x100) iz = (iz & 7u) | 0xF8u;
                const u32 tile = m.r16(grid + static_cast<u32>(static_cast<s16>(ix) * 2) +
                                      static_cast<u32>(static_cast<s16>(iz) * 512)) & 0x1FFu;
                if (!tile) continue;
                m.w16(out, static_cast<u32>(xc - fxs) + m.r16(p0) + 0x400u - m.r16(start));
                m.w16(out + 2, neg_y);
                m.w32(out + 8, tile);
                m.w16(out + 4, static_cast<u32>(zc - fzs) + m.r16(p0 + 8) + 0x400u - m.r16(start + 8));
                out += 12;
            }
        }
    }
    m.w32(out + 8, 0x200000);
}

void Game::ray_matrix(u32 mat, u32 dir) {
    cover(0x80099428u);
    Frame f(*this, 0x40);
    const u32 tmp = f.at(0x10);
    auto identity = [&](u32 at) {
        for (u32 k = 0; k < 9; ++k) m.w16(at + 2 * k, (k % 4 == 0) ? 0x1000u : 0u);
    };
    auto multiply = [&] {
        for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(mat + 4 * r));
        for (u32 c = 0; c < 3; ++c) {
            gte.mtc2(9, m.r16(tmp + 2 * c));
            gte.mtc2(10, m.r16(tmp + 2 * c + 6));
            gte.mtc2(11, m.r16(tmp + 2 * c + 12));
            gte.op(0x49E012);
            m.w16(mat + 2 * c, gte.mfc2(9));
            m.w16(mat + 2 * c + 6, gte.mfc2(10));
            m.w16(mat + 2 * c + 12, gte.mfc2(11));
        }
    };
    identity(mat);
    const s32 flat = length3(m.r16s(dir), 0, m.r16s(dir + 4));
    const s32 y = m.r16s(dir + 2);
    identity(tmp);
    m.w16(tmp + 8, static_cast<u32>(flat));
    m.w16(tmp + 16, static_cast<u32>(flat));
    m.w16(tmp + 10, static_cast<u32>(-y));
    m.w16(tmp + 14, static_cast<u32>(y));
    multiply();
    if (flat == 0) return;
    const s32 s = static_cast<s32>(wrap::div(static_cast<s32>(static_cast<u32>(m.r16s(dir)) << 12), flat).lo);
    const s32 c = static_cast<s32>(wrap::div(static_cast<s32>(static_cast<u32>(m.r16s(dir + 4)) << 12), flat).lo);
    identity(tmp);
    m.w16(tmp + 4, static_cast<u32>(-s));
    m.w16(tmp + 12, static_cast<u32>(s));
    m.w16(tmp + 0, static_cast<u32>(c));
    m.w16(tmp + 16, static_cast<u32>(c));
    multiply();
}

void Game::tile_transform(u32 vb, u32 offset, u32 out) {
    cover(0x80099750u);
    const s32 ox = m.r16s(offset), oy = m.r16s(offset + 2), oz = m.r16s(offset + 4);
    s32 n = m.r32s(vb);
    u32 grp = vb + 4;
    auto xy = [&](u32 at) {
        return (static_cast<u32>(m.r16s(at) + ox) & 0xFFFFu) | (static_cast<u32>(m.r16s(at + 2) + oy) << 16);
    };
    auto put = [&] {
        m.w16(out, gte.mfc2(9));
        m.w16(out + 2, gte.mfc2(10));
        m.w16(out + 4, gte.mfc2(11));
        out += 8;
    };
    gte.mtc2(0, xy(grp));
    gte.mtc2(1, static_cast<u32>(m.r16s(grp + 12) + oz));
    do {
        gte.op(0x486012);
        gte.mtc2(2, xy(grp + 4));
        gte.mtc2(3, static_cast<u32>(m.r16s(grp + 14) + oz));
        put();
        gte.op(0x48E012);
        gte.mtc2(4, xy(grp + 8));
        gte.mtc2(5, static_cast<u32>(m.r16s(grp + 16) + oz));
        put();
        gte.op(0x496012);
        grp += 20;
        n -= 3;
        gte.mtc2(0, xy(grp));
        gte.mtc2(1, static_cast<u32>(m.r16s(grp + 12) + oz));
        put();
    } while (n > 0);
}

void Game::ray_hit_list(u32 list, u32 stride, RayState& st) {
    const s32 count = m.r32s(list);
    u32 e = list + 4;
    auto vtx = [&](u32 i) { return st.verts + 8 * i; };
    auto set_plane_from = [&](u32 v) {
        gte.ctc2(8, (static_cast<u32>(-m.r16s(v)) & 0xFFFFu) | (static_cast<u32>(-m.r16s(v + 2)) << 16));
        gte.ctc2(9, static_cast<u32>(-m.r16s(v + 4)));
    };
    auto normal_into_v0 = [&](u32 normal) -> s32 {
        gte.mtc2(0, m.r32(normal));
        gte.mtc2(1, m.r32(normal + 4));
        gte.op(0x486012);
        const u32 x = gte.mfc2(9), y = gte.mfc2(10);
        const s32 z = static_cast<s32>(gte.mfc2(11));
        gte.mtc2(1, static_cast<u32>(z));
        gte.mtc2(0, (x & 0xFFFFu) | (y << 16));
        return z;
    };
    auto nclip = [&] {
        gte.op(0x1400006);
        return static_cast<s32>(gte.mfc2(24));
    };
    auto consider = [&](s32 t, u32 normal, u32 flags) {
        if (t < 0 || st.max < t || !(t < st.best)) return;
        st.best = t;
        st.normal = normal;
        st.flags = flags;
    };
    for (s32 k = 0; k < count; ++k, e += stride) {
        const u32 flags = m.r32(e);
        const u32 idx = m.r32(e + 16);
        if (flags & 0x200u) {
            const u32 n0 = st.normals + m.r16(e + 4);
            const s32 nz0 = normal_into_v0(n0);
            const u32 a3 = vtx(idx & 0xFF);
            if (nz0 < 0) {
                set_plane_from(a3);
                st.s1 = vtx((idx >> 8) & 0xFF);
                gte.op(0x4A6412);
                const u32 a0 = vtx((idx >> 16) & 0xFF);
                gte.mtc2(12, m.r32(a3));
                gte.mtc2(13, m.r32(st.s1));
                const s32 q = static_cast<s32>(wrap::div(static_cast<s32>(0u - (gte.mfc2(25) << 12)), nz0).lo);
                s32 side = nclip();
                gte.mtc2(12, m.r32(a0));
                const s32 s2 = nclip();
                if (!(side < 0)) {
                    side = s2;
                    gte.mtc2(13, m.r32(a3));
                    const s32 s3 = nclip();
                    if (!(side > 0) && !(s3 < 0)) consider(q, n0, flags);
                }
            }
            const u32 n1 = st.normals + m.r16(e + 6);
            const s32 nz1 = normal_into_v0(n1);
            if (nz1 >= 0) continue;
            set_plane_from(st.s1);
            u32 bits = idx >> 8;
            st.s1 = vtx(bits & 0xFF);
            gte.op(0x4A6412);
            bits >>= 8;
            const u32 a0 = vtx(bits & 0xFF);
            const u32 a1 = vtx(bits >> 8);
            gte.mtc2(12, m.r32(st.s1));
            gte.mtc2(13, m.r32(a0));
            const s32 q = static_cast<s32>(wrap::div(static_cast<s32>(0u - (gte.mfc2(25) << 12)), nz1).lo);
            const s32 side = nclip();
            gte.mtc2(12, m.r32(a1));
            const s32 s2 = nclip();
            if (side > 0) continue;
            gte.mtc2(13, m.r32(st.s1));
            const s32 s3 = nclip();
            if (s2 < 0 || s3 > 0) continue;
            consider(q, n1, m.r32(e));
            continue;
        }
        const u32 n0 = st.normals + m.r16(e + 4);
        const s32 nz = normal_into_v0(n0);
        const u32 a3 = vtx(idx & 0xFF);
        if (nz >= 0) continue;
        set_plane_from(a3);
        st.s1 = vtx((idx >> 8) & 0xFF);
        gte.op(0x4A6412);
        const u32 a0 = vtx((idx >> 16) & 0xFF);
        const u32 a1 = vtx(idx >> 24);
        gte.mtc2(12, m.r32(a3));
        gte.mtc2(13, m.r32(st.s1));
        const s32 q = static_cast<s32>(wrap::div(static_cast<s32>(0u - (gte.mfc2(25) << 12)), nz).lo);
        if (flags & 0x100u) {
            const s32 s1v = nclip();
            gte.mtc2(12, m.r32(a0));
            const s32 s2v = nclip();
            if (s1v < 0) continue;
            gte.mtc2(13, m.r32(a3));
            const s32 s3v = nclip();
            if (s2v > 0) continue;
            if (!(s3v >= 0)) continue;
        } else {
            const s32 s1v = nclip();
            gte.mtc2(12, m.r32(a1));
            const s32 s2v = nclip();
            if (s1v < 0) continue;
            gte.mtc2(13, m.r32(a0));
            const s32 s3v = nclip();
            if (s2v > 0) continue;
            gte.mtc2(12, m.r32(a3));
            const s32 s4v = nclip();
            if (s3v < 0) continue;
            if (s4v > 0) continue;
        }
        consider(q, st.normals + m.r16(e + 4), m.r32(e));
    }
}

s32 Game::ray_frame(u32 delta, u32 dir, u32 mat, u32 m3) {
    m.w16(dir, m.r16(delta));
    m.w16(dir + 2, m.r16(delta + 4));
    m.w16(dir + 4, m.r16(delta + 8));
    const s32 len = isqrt(vector_normal_ss(dir, dir));
    ray_matrix(mat, dir);
    m.w32(mat + 0x1C, 0);
    m.w32(mat + 0x18, 0);
    m.w32(mat + 0x14, 0);
    for (u32 k = 0; k < 9; ++k) m.w16(m3 + 2 * k, (k % 4 == 0) ? 0x1000u : 0u);
    m.w32(m3 + 0x1C, 0);
    m.w32(m3 + 0x18, 0);
    m.w32(m3 + 0x14, 0);
    for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(mat + 4 * r));
    for (u32 c = 0; c < 3; ++c) {
        gte.mtc2(9, m.r16(m3 + 2 * c));
        gte.mtc2(10, m.r16(m3 + 2 * c + 6));
        gte.mtc2(11, m.r16(m3 + 2 * c + 12));
        gte.op(0x49E012);
        m.w16(m3 + 2 * c, gte.mfc2(9));
        m.w16(m3 + 2 * c + 6, gte.mfc2(10));
        m.w16(m3 + 2 * c + 12, gte.mfc2(11));
    }
    gte.ctc2(5, m.r32(mat + 0x14));
    gte.ctc2(6, m.r32(mat + 0x18));
    gte.ctc2(7, m.r32(mat + 0x1C));
    const u32 t = m3 + 0x14;
    gte.mtc2(0, m.r16(t) | (static_cast<u32>(m.r16(t + 4)) << 16));
    gte.mtc2(1, m.r32(t + 8));
    gte.op(0x480012);
    m.w32(t, gte.mfc2(25));
    m.w32(t + 4, gte.mfc2(26));
    m.w32(t + 8, gte.mfc2(27));
    for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(m3 + 4 * r));
    gte.ctc2(5, m.r32(m3 + 0x14));
    gte.ctc2(6, m.r32(m3 + 0x18));
    gte.ctc2(7, m.r32(m3 + 0x1C));
    gte.mtc2(14, 0);
    return len;
}

s32 Game::ray_result(u32 dir, u32 out, const RayState& st) {
    if (0x7FFF < st.best) {
        m.w32(out + 8, 0);
        m.w32(out + 4, 0);
        m.w32(out, 0);
        m.w32(kHitNormal, 0);
    } else {
        m.w32(out, static_cast<u32>(m.r16s(dir) * st.best));
        m.w32(out + 4, static_cast<u32>(m.r16s(dir + 2) * st.best));
        m.w32(kHitNormal, st.normal);
        m.w32(kHitFlags, st.flags);
        m.w32(out + 8, static_cast<u32>(m.r16s(dir + 4) * st.best));
    }
    return st.best;
}

s32 Game::ray_hit_tiles(u32 delta, u32 out, u32 verts, u32 table, u32 list, u32 stale) {
    cover(0x80099894u);
    Frame f(*this, 0x88);
    const u32 dir = f.at(0x10);
    m.w32(sp + 0x88 + 8, verts);
    m.w32(sp + 0x88 + 12, table);
    const s32 len = ray_frame(delta, dir, f.at(0x18), f.at(0x38));

    RayState st;
    st.verts = verts;
    st.max = len;
    st.best = 0x7FFFFFFF;
    st.s1 = stale;
    for (u32 cell = list; !(m.r32(cell + 8) & 0x200000u); cell += 12) {
        const u32 tile = m.r32(table + 4 * (m.r32(cell + 8) & 0x1FFu));
        m.w32(f.at(0x58), tile);
        tile_transform(m.r32(tile + 4), cell, verts);
        st.normals = m.r32(m.r32(g::kWorld) + 0x25C);
        ray_hit_list(m.r32(tile + 16), 48, st);
        ray_hit_list(m.r32(tile + 12), 36, st);
    }
    return ray_result(dir, out, st);
}

s32 Game::terrain_raycast(u32 a, u32 b, u32 out, u32 scratch) {
    cover(0x8009A800u);
    Frame f(*this, 0x290);
    const u32 s = f.at(0x258), e = f.at(0x268), cells = f.at(0x18);
    const u32 env = m.r32(g::kWorld) + 0x244;
    const u32 grid = m.r32(env + 4);
    for (u32 k = 0; k < 12; k += 4) m.w32(s + k, static_cast<u32>(m.r32s(a + k) >> 12));
    for (u32 k = 0; k < 12; k += 4) m.w32(e + k, static_cast<u32>(m.r32s(b + k) >> 12));
    m.w32(f.at(0x10), grid);
    ray_cells(s, e, scratch, cells, grid);
    for (u32 k = 0; k < 12; k += 4) m.w32(e + k, m.r32(e + k) - m.r32(s + k));
    m.w32(f.at(0x10), cells);
    const s32 r = ray_hit_tiles(e, out, scratch, m.r32(env + 8), cells, a);
    for (u32 k = 0; k < 12; k += 4) m.w32(out + k, m.r32(out + k) + m.r32(a + k));
    return r;
}

s32 Game::ray_cell(u32 a, u32 b, u32 out, u32 scratch) {
    cover(0x8009A5D0u);
    Frame f(*this, 0x70);
    const u32 env = m.r32(g::kWorld) + 0x244;
    const u32 grid = m.r32(env + 4);
    const s32 x = m.r32s(a), z = m.r32s(a + 8);
    m.w32(f.at(0x18), static_cast<u32>(x >> 12));
    m.w32(f.at(0x1C), static_cast<u32>(m.r32s(a + 4) >> 12));
    m.w32(f.at(0x20), static_cast<u32>(z >> 12));
    for (u32 k = 0; k < 12; k += 4) m.w32(f.at(0x28 + k), static_cast<u32>(m.r32s(a + k) >> 12));
    for (u32 k = 0; k < 12; k += 4) m.w32(f.at(0x38 + k), static_cast<u32>(m.r32s(b + k) >> 12));
    s32 cx = (x >> 23) + 0x80, cz = (z >> 23) + 0x80;
    if (cx < 0) cx &= 7;
    if (cz < 0) cz &= 7;
    if (cx >= 0x100) cx = (cx & 7) | 0xF8;
    if (cz >= 0x100) cz = (cz & 7) | 0xF8;
    const u32 tile = m.r16(grid + 2u * static_cast<u32>(cx) + 512u * static_cast<u32>(cz)) & 0x1FFu;
    if (!tile) {
        m.w32(out, m.r32(a));
        m.w32(out + 4, m.r32(a + 4));
        m.w32(out + 8, m.r32(a + 8));
        return 0x7FFFFFFF;
    }
    const u32 cell = f.at(0x48);
    m.w16(cell, static_cast<u32>(0x400 - ((x >> 12) & 0x7FF)));
    m.w16(cell + 4, static_cast<u32>(0x400 - ((z >> 12) & 0x7FF)));
    m.w32(cell + 8, tile);
    m.w16(cell + 2, static_cast<u32>(static_cast<s32>(0u - m.r32(a + 4)) >> 12));
    m.w32(cell + 0x14, 0x200000u);
    for (u32 k = 0; k < 12; k += 4) m.w32(f.at(0x18 + k), static_cast<u32>(wrap::sub(m.r32s(b + k), m.r32s(a + k)) >> 12));
    m.w32(f.at(0x10), cell);
    const s32 r = ray_hit_tiles(f.at(0x18), out, scratch, m.r32(env + 8), cell, out);
    for (u32 k = 0; k < 12; k += 4) m.w32(out + k, m.r32(out + k) + m.r32(a + k));
    return r;
}

u32 Game::morph_vertices(s32 t, u32 from, u32 to, u32 out) {
    cover(0x80066A9Cu);
    gte.mtc2(8, static_cast<u32>(t));
    s32 n = m.r32s(from);
    m.w32(out, static_cast<u32>(n));
    u32 a = from + 4, b = to + 4, o = out + 4;
    auto lerp = [&](u32 xy, s32 z, u32 txy, s32 tz, u32 ox, u32 oy, u32 oz) {
        gte.mtc2(9, static_cast<u32>(static_cast<s32>(static_cast<s16>(xy))));
        gte.mtc2(10, static_cast<u32>(static_cast<s32>(xy) >> 16));
        gte.mtc2(11, static_cast<u32>(z));
        gte.ctc2(21, static_cast<u32>(static_cast<s32>(static_cast<s16>(txy))));
        gte.ctc2(22, static_cast<u32>(static_cast<s32>(txy) >> 16));
        gte.ctc2(23, static_cast<u32>(tz));
        gte.op(0x980011);
        m.w16(ox, gte.mfc2(9));
        m.w16(oy, gte.mfc2(10));
        m.w16(oz, gte.mfc2(11));
    };
    do {
        const u32 za = m.r32(a + 12), zb = m.r32(b + 12);
        lerp(m.r32(a), static_cast<s16>(za), m.r32(b), static_cast<s16>(zb), o, o + 2, o + 12);
        lerp(m.r32(a + 4), static_cast<s32>(za) >> 16, m.r32(b + 4), static_cast<s32>(zb) >> 16, o + 4, o + 6, o + 14);
        lerp(m.r32(a + 8), m.r16s(a + 16), m.r32(b + 8), m.r16s(b + 16), o + 8, o + 10, o + 16);
        a += 20;
        b += 20;
        o += 20;
        n -= 3;
    } while (n > 0);
    return o;
}

s32 Game::model_hit_tiles(u32 delta, u32 out, u32 work, u32 model, u32 verts, u32 offset, u32 stale) {
    cover(0x8009A944u);
    Frame f(*this, 0x80);
    const u32 dir = f.at(0x10);
    m.w32(sp + 0x80 + 4, out);
    m.w32(sp + 0x80 + 12, model);
    const s32 len = ray_frame(delta, dir, f.at(0x18), f.at(0x38));
    tile_transform(verts, offset, work);
    RayState st;
    st.verts = work;
    st.normals = m.r32(m.r32(g::kWorld) + 0x25C);
    st.max = len;
    st.best = 0x7FFFFFFF;
    st.s1 = stale;
    ray_hit_list(m.r32(model + 16), 48, st);
    ray_hit_list(m.r32(model + 12), 36, st);
    return ray_result(dir, out, st);
}

s32 Game::model_raycast(u32 start, u32 delta, u32 e, u32 out, u32 scratch, u32 stale) {
    cover(0x8009B4ECu);
    Frame f(*this, 0x48);
    const u32 d = f.at(0x18), offset = f.at(0x28);
    for (u32 k = 0; k < 12; k += 4) m.w32(d + k, static_cast<u32>(m.r32s(delta + k) >> 12));
    for (u32 k = 0; k < 3; ++k)
        m.w16(offset + 2 * k, static_cast<u32>(wrap::sub(m.r32s(e + 8 + 4 * k), m.r32s(start + 4 * k)) >> 12));
    const u32 models = m.r32(m.r32(g::kWorld) + 0x258) + 4u * m.r8(e + 4);
    u32 model, verts, work = scratch;
    if (m.r8(e + 5) & 4u) {
        const u32 frame = m.r16(e + 6);
        s32 k = static_cast<s16>(frame) >> 12;
        const s32 t = static_cast<s32>(frame & 0xFFFu);
        if (k < 0) k = 0;
        if (k >= 5) k = 4;
        if (t) {
            const s32 k1 = k < 4 ? k + 1 : 4;
            model = m.r32(models + 4u * static_cast<u32>(k));
            const u32 next = m.r32(models + 4u * static_cast<u32>(k1));
            verts = scratch;
            work = morph_vertices(t, m.r32(model + 4), m.r32(next + 4), scratch);
        } else {
            model = m.r32(models + 4u * static_cast<u32>(k));
            verts = m.r32(model + 4);
        }
    } else {
        model = m.r32(models);
        verts = m.r32(model + 4);
    }
    m.w32(f.at(0x10), verts);
    m.w32(f.at(0x14), offset);
    const s32 r = model_hit_tiles(d, out, work, model, verts, offset, stale);
    for (u32 k = 0; k < 12; k += 4) m.w32(out + k, m.r32(out + k) + m.r32(start + k));
    return r;
}
}

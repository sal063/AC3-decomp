#include "core/wrap.hpp"
#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kRsqrtTable = 0x800B6F78u;
constexpr u32 kInitGeomRa = 0x800B6F54u;
constexpr u32 kSrCu2 = 0x40000000u;

constexpr u32 kRtpt = 0x280030u;
constexpr u32 kRtNoPers = 0x480012u;
constexpr u32 kRtColumn = 0x486012u;
constexpr u32 kNclip = 0x1400006u;
constexpr u32 kOuterProduct12 = 0x178000Cu;
constexpr u32 kOuterProduct0 = 0x170000Cu;
constexpr u32 kSquare12 = 0xA80428u;
constexpr u32 kSquare0 = 0xA00428u;
constexpr u32 kAverageZ3 = 0x158002Du;
constexpr u32 kAverageZ4 = 0x168002Eu;
constexpr u32 kLightColor = 0x4DA412u;
constexpr u32 kDpcl = 0x680029u;
constexpr u32 kDpct = 0xF8002Au;
constexpr u32 kIntpl = 0x980011u;

constexpr u32 kVxy0 = 0, kVz0 = 1, kVxy1 = 2, kVz1 = 3, kVxy2 = 4, kVz2 = 5, kRgbc = 6, kOtz = 7;
constexpr u32 kIr0 = 8, kIr1 = 9, kIr2 = 10, kIr3 = 11, kSxy0 = 12, kSxy1 = 13, kSxy2 = 14;
constexpr u32 kSz0 = 16, kSz1 = 17, kSz2 = 18, kSz3 = 19, kRgb0 = 20, kRgb1 = 21, kRgb2 = 22;
constexpr u32 kMac0 = 24, kMac1 = 25, kMac2 = 26, kMac3 = 27, kLzcs = 30, kLzcr = 31;
constexpr u32 kR11R12 = 0, kR22R23 = 2, kR33 = 4, kRbk = 13, kOfx = 24, kOfy = 25, kH = 26;
constexpr u32 kDqa = 27, kDqb = 28, kZsf3 = 29, kZsf4 = 30, kFlag = 31, kLr1Lr2 = 16;
}

u32 Game::init_geom(u32 ra) {
    cover(0x800A6270u);
    m.w32(kInitGeomRa, ra);
    cop0[12] |= kSrCu2;
    gte.ctc2(kZsf3, 0x155u);
    gte.ctc2(kZsf4, 0x100u);
    gte.ctc2(kH, 0x3E8u);
    gte.ctc2(kDqa, 0xFFFFEF9Eu);
    gte.ctc2(kDqb, 0x1400000u);
    gte.ctc2(kOfx, 0);
    gte.ctc2(kOfy, 0);
    return cop0[12];
}

s32 Game::inv_square_root(s32 a, u32 mant, u32 shift) {
    cover(0x800A6388u);
    gte.mtc2(kLzcs, static_cast<u32>(a));
    const s32 lz = static_cast<s32>(gte.mfc2(kLzcr));
    if (lz == 32 || lz == 0) return -1;
    const s32 even = lz & ~1;
    const s32 down = (31 - even) >> 1;
    const s32 k = even - 24;
    const s32 top = k >= 0 ? static_cast<s32>(wrap::shl(static_cast<u32>(a), static_cast<u32>(k)))
                           : wrap::sar(a, static_cast<u32>(24 - even));
    const s32 i = static_cast<s32>(wrap::shl(static_cast<u32>(top - 0x40), 1));
    const s32 t = m.r16s(kRsqrtTable + static_cast<u32>(i));
    m.w32(shift, static_cast<u32>(down));
    m.w32(mant, static_cast<u32>(t));
    return 1;
}

s32 Game::normal_to_short(s32 x, s32 y, s32 z, u32 out) {
    cover(0x800A6464u);
    const s32 sq = normalize_ss(x, y, z);
    m.w16(out, static_cast<u32>(x));
    m.w16(out + 2, static_cast<u32>(y));
    m.w16(out + 4, static_cast<u32>(z));
    return sq;
}

s32 Game::vector_normal_s(u32 v, u32 out) {
    cover(0x800A6414u);
    return normal_to_short(m.r32s(v), m.r32s(v + 4), m.r32s(v + 8), out);
}

s32 Game::vector_normal(u32 v, u32 out) {
    cover(0x800A6428u);
    s32 x = m.r32s(v), y = m.r32s(v + 4), z = m.r32s(v + 8);
    const s32 sq = normalize_ss(x, y, z);
    m.w32(out, static_cast<u32>(x));
    m.w32(out + 4, static_cast<u32>(y));
    m.w32(out + 8, static_cast<u32>(z));
    return sq;
}

s32 Game::matrix_normal(u32 mat, u32 out) {
    cover(0x800A6548u);
    const s32 a0 = m.r16s(mat), a1 = m.r16s(mat + 2), a2 = m.r16s(mat + 4);
    const s32 b0 = m.r16s(mat + 6), b1 = m.r16s(mat + 8), b2 = m.r16s(mat + 10);
    const u32 r11r12 = gte.cfc2(kR11R12), r22r23 = gte.cfc2(kR22R23), r33 = gte.cfc2(kR33);
    gte.ctc2(kR11R12, static_cast<u32>(a0));
    gte.ctc2(kR22R23, static_cast<u32>(a1));
    gte.ctc2(kR33, static_cast<u32>(a2));
    gte.mtc2(kIr3, static_cast<u32>(b2));
    gte.mtc2(kIr1, static_cast<u32>(b0));
    gte.mtc2(kIr2, static_cast<u32>(b1));
    gte.op(kOuterProduct12);
    const s32 c0 = static_cast<s32>(gte.mfc2(kMac1));
    const s32 c1 = static_cast<s32>(gte.mfc2(kMac2));
    const s32 c2 = static_cast<s32>(gte.mfc2(kMac3));
    gte.ctc2(kR11R12, static_cast<u32>(b0));
    gte.ctc2(kR22R23, static_cast<u32>(b1));
    gte.ctc2(kR33, static_cast<u32>(b2));
    gte.op(kOuterProduct12);
    gte.mtc2(kVxy0, static_cast<u32>(b0));
    gte.mtc2(kVz0, static_cast<u32>(b1));
    gte.mtc2(kVxy1, static_cast<u32>(b2));
    const s32 d0 = static_cast<s32>(gte.mfc2(kMac1));
    const s32 d1 = static_cast<s32>(gte.mfc2(kMac2));
    const s32 d2 = static_cast<s32>(gte.mfc2(kMac3));
    gte.ctc2(kR11R12, r11r12);
    gte.ctc2(kR22R23, r22r23);
    gte.ctc2(kR33, r33);
    s32 x = d0, y = d1, z = d2;
    normalize_ss(x, y, z);
    m.w16(out, static_cast<u32>(x));
    m.w16(out + 2, static_cast<u32>(y));
    m.w16(out + 4, static_cast<u32>(z));
    x = static_cast<s32>(gte.mfc2(kVxy0));
    y = static_cast<s32>(gte.mfc2(kVz0));
    z = static_cast<s32>(gte.mfc2(kVxy1));
    normalize_ss(x, y, z);
    m.w16(out + 6, static_cast<u32>(x));
    m.w16(out + 8, static_cast<u32>(y));
    m.w16(out + 10, static_cast<u32>(z));
    x = c0, y = c1, z = c2;
    const s32 sq = normalize_ss(x, y, z);
    m.w16(out + 12, static_cast<u32>(x));
    m.w16(out + 14, static_cast<u32>(y));
    m.w16(out + 16, static_cast<u32>(z));
    return sq;
}

u32 Game::mul_matrix_into(u32 m0, u32 m1, u32 out) {
    for (u32 k = 0; k < 5; ++k) gte.ctc2(k, m.r32(m0 + 4 * k));
    auto column = [&](u32 xy, u32 z) {
        gte.mtc2(kVxy0, xy);
        gte.mtc2(kVz0, z);
        gte.op(kRtColumn);
    };
    column(m.r16(m1) | (m.r32(m1 + 4) & 0xFFFF0000u), m.r32(m1 + 12));
    const u32 c0x = gte.mfc2(kIr1), c0y = gte.mfc2(kIr2), c0z = gte.mfc2(kIr3);
    column(m.r16(m1 + 2) | (m.r32(m1 + 8) << 16), static_cast<u32>(static_cast<s32>(m.r16s(m1 + 14))));
    const u32 c1x = gte.mfc2(kIr1), c1y = gte.mfc2(kIr2), c1z = gte.mfc2(kIr3);
    column(m.r16(m1 + 4) | (m.r32(m1 + 8) & 0xFFFF0000u), m.r32(m1 + 16));
    m.w32(out, (c1x << 16) | (c0x & 0xFFFFu));
    m.w32(out + 12, (c1z << 16) | (c0z & 0xFFFFu));
    m.w32(out + 4, (c0y << 16) | (gte.mfc2(kIr1) & 0xFFFFu));
    m.w32(out + 8, (gte.mfc2(kIr2) << 16) | (c1y & 0xFFFFu));
    m.w32(out + 16, gte.mfc2(kIr3));
    return out;
}

u32 Game::mul_matrix0(u32 m0, u32 m1, u32 out) {
    cover(0x800A6638u);
    return mul_matrix_into(m0, m1, out);
}

u32 Game::mul_matrix2(u32 m0, u32 m1) {
    cover(0x800A6D68u);
    return mul_matrix_into(m0, m1, m1);
}

u32 Game::scale_matrix(u32 mat, u32 scale) {
    cover(0x800A6F08u);
    const s32 s[3] = {m.r32s(scale), m.r32s(scale + 4), m.r32s(scale + 8)};
    auto scaled = [](s32 e, s32 k) { return wrap::mul(e, k) >> 12; };
    for (u32 w = 0; w < 4; ++w) {
        const u32 word = m.r32(mat + 4 * w);
        const s32 lo = scaled(static_cast<s16>(word), s[(2 * w) % 3]);
        const s32 hi = scaled(static_cast<s32>(word) >> 16, s[(2 * w + 1) % 3]);
        m.w32(mat + 4 * w, (static_cast<u32>(hi) << 16) | (static_cast<u32>(lo) & 0xFFFFu));
    }
    m.w32(mat + 16, static_cast<u32>(scaled(static_cast<s16>(m.r32(mat + 16)), s[2])));
    return mat;
}

void Game::set_color_matrix(u32 mat) {
    cover(0x800A7078u);
    for (u32 k = 0; k < 5; ++k) gte.ctc2(kLr1Lr2 + k, m.r32(mat + 4 * k));
}

void Game::read_geom_offset(u32 ofx, u32 ofy) {
    cover(0x800A70C8u);
    const s32 x = static_cast<s32>(gte.cfc2(kOfx)) >> 16;
    const s32 y = static_cast<s32>(gte.cfc2(kOfy)) >> 16;
    m.w32(ofx, static_cast<u32>(x));
    m.w32(ofy, static_cast<u32>(y));
}

u32 Game::read_geom_screen() {
    cover(0x800A70E8u);
    return gte.cfc2(kH);
}

void Game::set_back_color(u32 r, u32 g, u32 b) {
    cover(0x800A70F8u);
    gte.ctc2(kRbk, r << 4);
    gte.ctc2(kRbk + 1, g << 4);
    gte.ctc2(kRbk + 2, b << 4);
}

void Game::light_color(u32 in, u32 out) {
    cover(0x800A7148u);
    for (u32 k = 0; k < 3; ++k) gte.mtc2(kIr1 + k, m.r32(in + 4 * k));
    gte.op(kLightColor);
    for (u32 k = 0; k < 3; ++k) m.w32(out + 4 * k, gte.mfc2(kIr1 + k));
}

void Game::dpq_color_light(u32 v, u32 rgb, u32 p, u32 out) {
    cover(0x800A7170u);
    for (u32 k = 0; k < 3; ++k) gte.mtc2(kIr1 + k, m.r32(v + 4 * k));
    gte.mtc2(kRgbc, m.r32(rgb));
    gte.mtc2(kIr0, p);
    gte.op(kDpcl);
    m.w32(out, gte.mfc2(kRgb2));
}

void Game::dpq_color3(u32 c0, u32 c1, u32 c2, u32 p, u32 o0, u32 o1, u32 o2) {
    cover(0x800A7198u);
    gte.mtc2(kRgb0, m.r32(c0));
    gte.mtc2(kRgb1, m.r32(c1));
    gte.mtc2(kRgb2, m.r32(c2));
    gte.mtc2(kRgbc, m.r32(c2));
    gte.mtc2(kIr0, p);
    gte.op(kDpct);
    m.w32(o0, gte.mfc2(kRgb0));
    m.w32(o1, gte.mfc2(kRgb1));
    m.w32(o2, gte.mfc2(kRgb2));
}

void Game::intpl(u32 v, u32 p, u32 out) {
    cover(0x800A71D4u);
    for (u32 k = 0; k < 3; ++k) gte.mtc2(kIr1 + k, m.r32(v + 4 * k));
    gte.mtc2(kIr0, p);
    gte.op(kIntpl);
    m.w32(out, gte.mfc2(kRgb2));
}

u32 Game::square12(u32 v, u32 out) {
    cover(0x800A71F8u);
    for (u32 k = 0; k < 3; ++k) gte.mtc2(kIr1 + k, m.r32(v + 4 * k));
    gte.op(kSquare12);
    for (u32 k = 0; k < 3; ++k) m.w32(out + 4 * k, gte.mfc2(kMac1 + k));
    return out;
}

u32 Game::square0(u32 v, u32 out) {
    cover(0x800A7220u);
    for (u32 k = 0; k < 3; ++k) gte.mtc2(kIr1 + k, m.r32(v + 4 * k));
    gte.op(kSquare0);
    for (u32 k = 0; k < 3; ++k) m.w32(out + 4 * k, gte.mfc2(kMac1 + k));
    return out;
}

u32 Game::average_z3(u32 z0, u32 z1, u32 z2) {
    cover(0x800A7248u);
    gte.mtc2(kSz1, z0);
    gte.mtc2(kSz2, z1);
    gte.mtc2(kSz3, z2);
    gte.op(kAverageZ3);
    return gte.mfc2(kOtz);
}

u32 Game::average_z4(u32 z0, u32 z1, u32 z2, u32 z3) {
    cover(0x800A7268u);
    gte.mtc2(kSz0, z0);
    gte.mtc2(kSz1, z1);
    gte.mtc2(kSz2, z2);
    gte.mtc2(kSz3, z3);
    gte.op(kAverageZ4);
    return gte.mfc2(kOtz);
}

void Game::outer_product(u32 a, u32 b, u32 out, u32 command) {
    const u32 r11r12 = gte.cfc2(kR11R12), r22r23 = gte.cfc2(kR22R23), r33 = gte.cfc2(kR33);
    gte.ctc2(kR11R12, m.r32(a));
    gte.ctc2(kR22R23, m.r32(a + 4));
    gte.ctc2(kR33, m.r32(a + 8));
    gte.mtc2(kIr3, m.r32(b + 8));
    gte.mtc2(kIr1, m.r32(b));
    gte.mtc2(kIr2, m.r32(b + 4));
    gte.op(command);
    for (u32 k = 0; k < 3; ++k) m.w32(out + 4 * k, gte.mfc2(kMac1 + k));
    gte.ctc2(kR11R12, r11r12);
    gte.ctc2(kR22R23, r22r23);
    gte.ctc2(kR33, r33);
}

void Game::outer_product12(u32 a, u32 b, u32 out) {
    cover(0x800A728Cu);
    outer_product(a, b, out, kOuterProduct12);
}

void Game::outer_product0(u32 a, u32 b, u32 out) {
    cover(0x800A72E4u);
    outer_product(a, b, out, kOuterProduct0);
}

u32 Game::lzc(u32 a) {
    cover(0x800A733Cu);
    gte.mtc2(kLzcs, a);
    return gte.mfc2(kLzcr);
}

s32 Game::rot_trans_pers3(u32 v0, u32 v1, u32 v2, u32 sxy0, u32 sxy1, u32 sxy2, u32 p, u32 flag) {
    cover(0x800A7388u);
    gte.mtc2(kVxy0, m.r32(v0));
    gte.mtc2(kVz0, m.r32(v0 + 4));
    gte.mtc2(kVxy1, m.r32(v1));
    gte.mtc2(kVz1, m.r32(v1 + 4));
    gte.mtc2(kVxy2, m.r32(v2));
    gte.mtc2(kVz2, m.r32(v2 + 4));
    gte.op(kRtpt);
    m.w32(sxy0, gte.mfc2(kSxy0));
    m.w32(sxy1, gte.mfc2(kSxy1));
    m.w32(sxy2, gte.mfc2(kSxy2));
    m.w32(p, gte.mfc2(kIr0));
    const u32 f = gte.cfc2(kFlag);
    const s32 z = static_cast<s32>(gte.mfc2(kSz3));
    m.w32(flag, f);
    return z >> 2;
}

u32 Game::rot_trans(u32 v, u32 out, u32 flag) {
    cover(0x800A73E8u);
    gte.mtc2(kVxy0, m.r32(v));
    gte.mtc2(kVz0, m.r32(v + 4));
    gte.op(kRtNoPers);
    for (u32 k = 0; k < 3; ++k) m.w32(out + 4 * k, gte.mfc2(kMac1 + k));
    const u32 f = gte.cfc2(kFlag);
    m.w32(flag, f);
    return f;
}

u32 Game::gte_patch_template(u32 k0, u32 at, u32 v0, u32 v1, u32 ra, u32 a0) {
    cover(0x800A7DE4u);
    m.w32(k0 + 4, at);
    m.w32(k0 + 8, v0);
    m.w32(k0 + 12, v1);
    m.w32(k0 + 0x7C, ra);
    const u32 epc = cop0[14];
    m.w32(k0 + 4, at);
    m.w32(k0 + 8, v0);
    m.w32(k0 + 12, epc);
    m.w32(k0 + 0x7C, ra);
    hw->reset_graph(a0);
    return 0;
}

u32 Game::normal_clip(u32 sxy0, u32 sxy1, u32 sxy2) {
    cover(0x800A7418u);
    gte.mtc2(kSxy0, sxy0);
    gte.mtc2(kSxy2, sxy2);
    gte.mtc2(kSxy1, sxy1);
    gte.op(kNclip);
    return gte.mfc2(kMac0);
}
}

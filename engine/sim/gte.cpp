#include "sim/gte.hpp"

#include "core/log.hpp"
#include "sim/precise.hpp"

#include <cstring>

namespace ac3::sim {
namespace {
constexpr s64 kMacMax = 0x7FFFFFFFFFFLL;
constexpr s64 kMacMin = -0x80000000000LL;

enum : u32 {
    kMac1Pos = 1u << 30, kMac2Pos = 1u << 29, kMac3Pos = 1u << 28,
    kMac1Neg = 1u << 27, kMac2Neg = 1u << 26, kMac3Neg = 1u << 25,
    kIr1 = 1u << 24, kIr2 = 1u << 23, kIr3 = 1u << 22,
    kRSat = 1u << 21, kGSat = 1u << 20, kBSat = 1u << 19,
    kSz3 = 1u << 18, kDiv = 1u << 17, kMac0Pos = 1u << 16, kMac0Neg = 1u << 15,
    kSx2 = 1u << 14, kSy2 = 1u << 13, kIr0 = 1u << 12,
    kErrorMask = 0x7F87E000u,
};

s64 trunc44(s64 v) { return static_cast<s64>(static_cast<u64>(v) << 20) >> 20; }

u32 pack(s16 lo, s16 hi) { return (static_cast<u32>(static_cast<u16>(hi)) << 16) | static_cast<u16>(lo); }

struct Unr {
    u8 t[257];
    Unr() {
        for (int i = 0; i <= 256; ++i) {
            const s32 v = (0x40000 / (i + 0x100) + 1) / 2 - 0x101;
            t[i] = static_cast<u8>(v < 0 ? 0 : v);
        }
    }
};
const Unr kUnr;
}

s64 Gte::chk_mac(int n, s64 v) {
    static constexpr u32 pos[3] = {kMac1Pos, kMac2Pos, kMac3Pos};
    static constexpr u32 neg[3] = {kMac1Neg, kMac2Neg, kMac3Neg};
    if (v > kMacMax) flag_ |= pos[n - 1];
    else if (v < kMacMin) flag_ |= neg[n - 1];
    return trunc44(v);
}

s32 Gte::chk_mac0(s64 v) {
    if (v > 0x7FFFFFFFLL) flag_ |= kMac0Pos;
    else if (v < -0x80000000LL) flag_ |= kMac0Neg;
    return static_cast<s32>(v);
}

s16 Gte::chk_ir(int n, s32 v, bool lm) {
    static constexpr u32 bit[3] = {kIr1, kIr2, kIr3};
    const s32 lo = lm ? 0 : -32768;
    if (v > 32767) { flag_ |= bit[n - 1]; return 32767; }
    if (v < lo) { flag_ |= bit[n - 1]; return static_cast<s16>(lo); }
    return static_cast<s16>(v);
}

s16 Gte::chk_ir0(s32 v) {
    if (v > 0x1000) { flag_ |= kIr0; return 0x1000; }
    if (v < 0) { flag_ |= kIr0; return 0; }
    return static_cast<s16>(v);
}

u16 Gte::chk_sz3(s32 v) {
    if (v > 65535) { flag_ |= kSz3; return 65535; }
    if (v < 0) { flag_ |= kSz3; return 0; }
    return static_cast<u16>(v);
}

s16 Gte::chk_sxy(s32 v, bool y) {
    if (v > 1023) { flag_ |= y ? kSy2 : kSx2; return 1023; }
    if (v < -1024) { flag_ |= y ? kSy2 : kSx2; return -1024; }
    return static_cast<s16>(v);
}

u8 Gte::chk_col(s32 v, int i) {
    static constexpr u32 bit[3] = {kRSat, kGSat, kBSat};
    if (v > 255) { flag_ |= bit[i]; return 255; }
    if (v < 0) { flag_ |= bit[i]; return 0; }
    return static_cast<u8>(v);
}

void Gte::push_sz(s32 v) {
    sz_[0] = sz_[1];
    sz_[1] = sz_[2];
    sz_[2] = sz_[3];
    sz_[3] = chk_sz3(v);
}

void Gte::push_sxy(s32 x, s32 y) {
    std::memcpy(sxy_[0], sxy_[1], sizeof sxy_[0]);
    std::memcpy(sxy_[1], sxy_[2], sizeof sxy_[1]);
    sxy_[2][0] = chk_sxy(x, false);
    sxy_[2][1] = chk_sxy(y, true);
}

void Gte::push_rgb_from_mac() {
    const u8 r = chk_col(mac_[1] >> 4, 0), g = chk_col(mac_[2] >> 4, 1), b = chk_col(mac_[3] >> 4, 2);
    rgb_[0] = rgb_[1];
    rgb_[1] = rgb_[2];
    rgb_[2] = ((rgbc_ >> 24) << 24) | (static_cast<u32>(b) << 16) | (static_cast<u32>(g) << 8) | r;
}

u32 Gte::divide(u16 h, u16 sz3) {
    if (h >= sz3 * 2) {
        flag_ |= kDiv;
        return 0x1FFFF;
    }
    int z = 0;
    for (u32 t = sz3; t < 0x8000u && z < 16; t <<= 1) ++z;
    const u32 n = static_cast<u32>(h) << z;
    u32 d = static_cast<u32>(sz3) << z;
    const u32 u = kUnr.t[(d - 0x7FC0) >> 7] + 0x101u;
    d = static_cast<u32>(((0x2000080ULL - static_cast<u64>(d) * u) >> 8) & 0xFFFFFFFFu);
    d = static_cast<u32>(((0x0000080ULL + static_cast<u64>(d) * u) >> 8) & 0xFFFFFFFFu);
    const u64 r = ((static_cast<u64>(n) * d) + 0x8000ULL) >> 16;
    return r > 0x1FFFF ? 0x1FFFFu : static_cast<u32>(r);
}

void Gte::set_mac_ir(int n, s64 v, int shift, bool lm) {
    const s64 m = chk_mac(n, v) >> shift;
    mac_[n] = static_cast<s32>(m);
    ir_[n] = chk_ir(n, static_cast<s32>(m), lm);
}

void Gte::mat_vec(const s16 m[3][3], const s32 t[3], const s16 vec[3], int shift, bool lm) {
    for (int i = 0; i < 3; ++i) {
        s64 acc = static_cast<s64>(t[i]) * 0x1000;
        acc += static_cast<s64>(m[i][0]) * vec[0];
        acc += static_cast<s64>(m[i][1]) * vec[1];
        acc += static_cast<s64>(m[i][2]) * vec[2];
        set_mac_ir(i + 1, acc, shift, lm);
    }
}

void Gte::interpolate(s64 m1, s64 m2, s64 m3, int shift, bool lm) {
    set_mac_ir(1, (static_cast<s64>(fc_[0]) << 12) - m1, shift, false);
    set_mac_ir(2, (static_cast<s64>(fc_[1]) << 12) - m2, shift, false);
    set_mac_ir(3, (static_cast<s64>(fc_[2]) << 12) - m3, shift, false);
    set_mac_ir(1, static_cast<s64>(ir_[1]) * ir_[0] + m1, shift, lm);
    set_mac_ir(2, static_cast<s64>(ir_[2]) * ir_[0] + m2, shift, lm);
    set_mac_ir(3, static_cast<s64>(ir_[3]) * ir_[0] + m3, shift, lm);
}

void Gte::from_rgb(s64& r, s64& g, s64& b) const {
    r = static_cast<s64>(rgbc_ & 0xFFu) * ir_[1] * 16;
    g = static_cast<s64>((rgbc_ >> 8) & 0xFFu) * ir_[2] * 16;
    b = static_cast<s64>((rgbc_ >> 16) & 0xFFu) * ir_[3] * 16;
}

void Gte::rtp(int vi, int shift, bool lm, bool last) {
    const s16 v[3] = {v_[vi][0], v_[vi][1], v_[vi][2]};
    s64 z = 0, view[3];
    for (int i = 0; i < 3; ++i) {
        s64 acc = static_cast<s64>(tr_[i]) * 0x1000;
        acc += static_cast<s64>(rt_[i][0]) * v[0];
        acc += static_cast<s64>(rt_[i][1]) * v[1];
        acc += static_cast<s64>(rt_[i][2]) * v[2];
        view[i] = acc;
        if (i == 2) z = chk_mac(3, acc);
        set_mac_ir(i + 1, acc, shift, lm);
    }
    push_sz(static_cast<s32>(z >> 12));
    const u32 q = divide(h_, sz_[3]);
    s64 across = static_cast<s64>(q) * ir_[1];
    if (x_squeeze != 0x10000) across = (across * x_squeeze) >> 16;
    const s64 sx = across + ofx_;
    const s64 sy = static_cast<s64>(q) * ir_[2] + ofy_;
    chk_mac0(sx);
    chk_mac0(sy);
    push_sxy(static_cast<s32>(sx >> 16), static_cast<s32>(sy >> 16));
    if (precise) {
        record_precise(view);
        exact_fifo[0] = exact_fifo[1];
        exact_fifo[1] = exact_fifo[2];
        Exact& e = exact_fifo[2];
        e.ok = !(flag_ & kDiv) && view[2] > 0;
        if (e.ok) {
            const double vx = static_cast<double>(view[0]) / 4096.0, vy = static_cast<double>(view[1]) / 4096.0;
            const double vz = static_cast<double>(view[2]) / 4096.0, k = static_cast<double>(h_) / vz;
            e.sx = static_cast<float>(static_cast<double>(ofx_) / 65536.0 + vx * k * x_squeeze / 65536.0);
            e.sy = static_cast<float>(static_cast<double>(ofy_) / 65536.0 + vy * k);
            e.vx = static_cast<float>(vx);
            e.vy = static_cast<float>(vy);
            e.vz = static_cast<float>(vz);
        }
    }
    if (last) {
        const s64 dp = static_cast<s64>(q) * dqa_ + dqb_;
        mac_[0] = chk_mac0(dp);
        ir_[0] = chk_ir0(static_cast<s32>(dp >> 12));
    }
}

void Gte::light(int vi, int shift, bool lm, int mode) {
    static constexpr s32 zero[3] = {0, 0, 0};
    const s16 n[3] = {v_[vi][0], v_[vi][1], v_[vi][2]};
    mat_vec(llm_, zero, n, shift, lm);
    const s16 l[3] = {ir_[1], ir_[2], ir_[3]};
    mat_vec(lcm_, bk_, l, shift, lm);
    if (mode == 0) { push_rgb_from_mac(); return; }
    s64 r, g, b;
    from_rgb(r, g, b);
    if (mode == 2) interpolate(r, g, b, shift, lm);
    else { set_mac_ir(1, r, shift, lm); set_mac_ir(2, g, shift, lm); set_mac_ir(3, b, shift, lm); }
    push_rgb_from_mac();
}

void Gte::record_precise(const s64 view[3]) {
    if ((flag_ & kDiv) || view[2] <= 0) return;
    const s16 x = sxy_[2][0], y = sxy_[2][1];
    const double vx = static_cast<double>(view[0]) / 4096.0, vy = static_cast<double>(view[1]) / 4096.0;
    const double vz = static_cast<double>(view[2]) / 4096.0;
    const double k = static_cast<double>(h_) / vz;
    PreciseVertex p;
    p.sx = static_cast<float>(static_cast<double>(ofx_) / 65536.0 + vx * k * x_squeeze / 65536.0);
    p.sy = static_cast<float>(static_cast<double>(ofy_) / 65536.0 + vy * k);
    p.vx = static_cast<float>(vx);
    p.vy = static_cast<float>(vy);
    p.vz = static_cast<float>(vz);
    precise->record(static_cast<u32>(static_cast<u16>(x)) | (static_cast<u32>(static_cast<u16>(y)) << 16), p);
}

bool Gte::project_exact(const double view[3], PreciseVertex& out) const {
    if (view[2] <= 0.0) return false;
    const double k = static_cast<double>(h_) / view[2];
    out.sx = static_cast<float>(static_cast<double>(ofx_) / 65536.0 + view[0] * k * x_squeeze / 65536.0);
    out.sy = static_cast<float>(static_cast<double>(ofy_) / 65536.0 + view[1] * k);
    out.vx = static_cast<float>(view[0]);
    out.vy = static_cast<float>(view[1]);
    out.vz = static_cast<float>(view[2]);
    return true;
}

void Gte::op(u32 command) {
    const u32 fn = command & 0x3Fu;
    const int shift = (command >> 19) & 1u ? 12 : 0;
    const bool lm = (command >> 10) & 1u;
    flag_ = 0;

    switch (fn) {
    case 0x01: rtp(0, shift, lm, true); break;
    case 0x30: rtp(0, shift, lm, false); rtp(1, shift, lm, false); rtp(2, shift, lm, true); break;
    case 0x06: {
        const s64 v = static_cast<s64>(sxy_[0][0]) * (sxy_[1][1] - sxy_[2][1]) +
                      static_cast<s64>(sxy_[1][0]) * (sxy_[2][1] - sxy_[0][1]) +
                      static_cast<s64>(sxy_[2][0]) * (sxy_[0][1] - sxy_[1][1]);
        mac_[0] = chk_mac0(v);
        break;
    }
    case 0x0C: {
        const s64 d1 = rt_[0][0], d2 = rt_[1][1], d3 = rt_[2][2];
        const s64 m1 = d2 * ir_[3] - d3 * ir_[2];
        const s64 m2 = d3 * ir_[1] - d1 * ir_[3];
        const s64 m3 = d1 * ir_[2] - d2 * ir_[1];
        set_mac_ir(1, m1, shift, lm);
        set_mac_ir(2, m2, shift, lm);
        set_mac_ir(3, m3, shift, lm);
        break;
    }
    case 0x10: {
        interpolate(static_cast<s64>(rgbc_ & 0xFFu) << 16, static_cast<s64>((rgbc_ >> 8) & 0xFFu) << 16,
                    static_cast<s64>((rgbc_ >> 16) & 0xFFu) << 16, shift, lm);
        push_rgb_from_mac();
        break;
    }
    case 0x2A:
        for (int i = 0; i < 3; ++i) {
            interpolate(static_cast<s64>(rgb_[0] & 0xFFu) << 16, static_cast<s64>((rgb_[0] >> 8) & 0xFFu) << 16,
                        static_cast<s64>((rgb_[0] >> 16) & 0xFFu) << 16, shift, lm);
            push_rgb_from_mac();
        }
        break;
    case 0x11:
        interpolate(static_cast<s64>(ir_[1]) << 12, static_cast<s64>(ir_[2]) << 12, static_cast<s64>(ir_[3]) << 12,
                    shift, lm);
        push_rgb_from_mac();
        break;
    case 0x12: {
        const u32 mx = (command >> 17) & 3u, vx = (command >> 15) & 3u, cv = (command >> 13) & 3u;
        s16 vec[3];
        if (vx < 3) { vec[0] = v_[vx][0]; vec[1] = v_[vx][1]; vec[2] = v_[vx][2]; }
        else { vec[0] = ir_[1]; vec[1] = ir_[2]; vec[2] = ir_[3]; }
        s16 m[3][3];
        if (mx == 0) std::memcpy(m, rt_, sizeof m);
        else if (mx == 1) std::memcpy(m, llm_, sizeof m);
        else if (mx == 2) std::memcpy(m, lcm_, sizeof m);
        else {
            const s16 r = static_cast<s16>((rgbc_ & 0xFFu) << 4);
            m[0][0] = static_cast<s16>(-r); m[0][1] = r; m[0][2] = ir_[0];
            m[1][0] = m[1][1] = m[1][2] = rt_[0][2];
            m[2][0] = m[2][1] = m[2][2] = rt_[1][1];
            log::warn("gte: MVMVA with the garbage matrix");
        }
        static constexpr s32 zero[3] = {0, 0, 0};
        if (cv == 2) {
            for (int i = 0; i < 3; ++i) {
                const s64 acc = static_cast<s64>(fc_[i]) * 0x1000 + static_cast<s64>(m[i][0]) * vec[0];
                chk_mac(i + 1, acc);
                chk_ir(i + 1, static_cast<s32>(trunc44(acc) >> shift), false);
                set_mac_ir(i + 1, static_cast<s64>(m[i][1]) * vec[1] + static_cast<s64>(m[i][2]) * vec[2], shift, lm);
            }
            log::warn("gte: MVMVA with cv=FC");
            break;
        }
        mat_vec(m, cv == 0 ? tr_ : cv == 1 ? bk_ : zero, vec, shift, lm);
        break;
    }
    case 0x13: light(0, shift, lm, 2); break;
    case 0x16: for (int i = 0; i < 3; ++i) light(i, shift, lm, 2); break;
    case 0x1B: light(0, shift, lm, 1); break;
    case 0x3F: for (int i = 0; i < 3; ++i) light(i, shift, lm, 1); break;
    case 0x1E: light(0, shift, lm, 0); break;
    case 0x20: for (int i = 0; i < 3; ++i) light(i, shift, lm, 0); break;
    case 0x14: {
        const s16 l[3] = {ir_[1], ir_[2], ir_[3]};
        mat_vec(lcm_, bk_, l, shift, lm);
        s64 r, g, b;
        from_rgb(r, g, b);
        interpolate(r, g, b, shift, lm);
        push_rgb_from_mac();
        break;
    }
    case 0x1C: {
        const s16 l[3] = {ir_[1], ir_[2], ir_[3]};
        mat_vec(lcm_, bk_, l, shift, lm);
        s64 r, g, b;
        from_rgb(r, g, b);
        set_mac_ir(1, r, shift, lm);
        set_mac_ir(2, g, shift, lm);
        set_mac_ir(3, b, shift, lm);
        push_rgb_from_mac();
        break;
    }
    case 0x28:
        set_mac_ir(1, static_cast<s64>(ir_[1]) * ir_[1], shift, lm);
        set_mac_ir(2, static_cast<s64>(ir_[2]) * ir_[2], shift, lm);
        set_mac_ir(3, static_cast<s64>(ir_[3]) * ir_[3], shift, lm);
        break;
    case 0x29: {
        s64 r, g, b;
        from_rgb(r, g, b);
        interpolate(r, g, b, shift, lm);
        push_rgb_from_mac();
        break;
    }
    case 0x2D: {
        const s64 v = static_cast<s64>(zsf3_) * (static_cast<s32>(sz_[1]) + sz_[2] + sz_[3]);
        mac_[0] = chk_mac0(v);
        otz_ = chk_sz3(static_cast<s32>(v >> 12));
        break;
    }
    case 0x2E: {
        const s64 v = static_cast<s64>(zsf4_) * (static_cast<s32>(sz_[0]) + sz_[1] + sz_[2] + sz_[3]);
        mac_[0] = chk_mac0(v);
        otz_ = chk_sz3(static_cast<s32>(v >> 12));
        break;
    }
    case 0x3D:
        set_mac_ir(1, static_cast<s64>(ir_[0]) * ir_[1], shift, lm);
        set_mac_ir(2, static_cast<s64>(ir_[0]) * ir_[2], shift, lm);
        set_mac_ir(3, static_cast<s64>(ir_[0]) * ir_[3], shift, lm);
        push_rgb_from_mac();
        break;
    case 0x3E:
        set_mac_ir(1, (static_cast<s64>(mac_[1]) << shift) + static_cast<s64>(ir_[0]) * ir_[1], shift, lm);
        set_mac_ir(2, (static_cast<s64>(mac_[2]) << shift) + static_cast<s64>(ir_[0]) * ir_[2], shift, lm);
        set_mac_ir(3, (static_cast<s64>(mac_[3]) << shift) + static_cast<s64>(ir_[0]) * ir_[3], shift, lm);
        push_rgb_from_mac();
        break;
    default:
        log::error("gte: command {:02X} is not implemented", fn);
        break;
    }
    if (flag_ & kErrorMask) flag_ |= 0x80000000u;
}

u32 Gte::mfc2(u32 reg) const {
    switch (reg) {
    case 0: return pack(v_[0][0], v_[0][1]);
    case 1: return static_cast<u32>(static_cast<s32>(v_[0][2]));
    case 2: return pack(v_[1][0], v_[1][1]);
    case 3: return static_cast<u32>(static_cast<s32>(v_[1][2]));
    case 4: return pack(v_[2][0], v_[2][1]);
    case 5: return static_cast<u32>(static_cast<s32>(v_[2][2]));
    case 6: return rgbc_;
    case 7: return otz_;
    case 8: case 9: case 10: case 11: return static_cast<u32>(static_cast<s32>(ir_[reg - 8]));
    case 12: return pack(sxy_[0][0], sxy_[0][1]);
    case 13: return pack(sxy_[1][0], sxy_[1][1]);
    case 14: case 15: return pack(sxy_[2][0], sxy_[2][1]);
    case 16: case 17: case 18: case 19: return sz_[reg - 16];
    case 20: case 21: case 22: return rgb_[reg - 20];
    case 23: return res1_;
    case 24: case 25: case 26: case 27: return static_cast<u32>(mac_[reg - 24]);
    case 28: case 29: {
        auto c = [](s16 ir) {
            const s32 v = ir / 0x80;
            return v < 0 ? 0 : v > 0x1F ? 0x1F : v;
        };
        return static_cast<u32>((c(ir_[3]) << 10) | (c(ir_[2]) << 5) | c(ir_[1]));
    }
    case 30: return static_cast<u32>(lzcs_);
    case 31: return static_cast<u32>(lzcr_);
    default: return 0;
    }
}

void Gte::mtc2(u32 reg, u32 v) {
    const s16 lo = static_cast<s16>(v), hi = static_cast<s16>(v >> 16);
    switch (reg) {
    case 0: v_[0][0] = lo; v_[0][1] = hi; break;
    case 1: v_[0][2] = lo; break;
    case 2: v_[1][0] = lo; v_[1][1] = hi; break;
    case 3: v_[1][2] = lo; break;
    case 4: v_[2][0] = lo; v_[2][1] = hi; break;
    case 5: v_[2][2] = lo; break;
    case 6: rgbc_ = v; break;
    case 7: otz_ = static_cast<u16>(v); break;
    case 8: case 9: case 10: case 11: ir_[reg - 8] = lo; break;
    case 12: sxy_[0][0] = lo; sxy_[0][1] = hi; break;
    case 13: sxy_[1][0] = lo; sxy_[1][1] = hi; break;
    case 14: sxy_[2][0] = lo; sxy_[2][1] = hi; break;
    case 15:
        std::memcpy(sxy_[0], sxy_[1], sizeof sxy_[0]);
        std::memcpy(sxy_[1], sxy_[2], sizeof sxy_[1]);
        sxy_[2][0] = lo; sxy_[2][1] = hi;
        break;
    case 16: case 17: case 18: case 19: sz_[reg - 16] = static_cast<u16>(v); break;
    case 20: case 21: case 22: rgb_[reg - 20] = v; break;
    case 23: res1_ = v; break;
    case 24: case 25: case 26: case 27: mac_[reg - 24] = static_cast<s32>(v); break;
    case 28:
        ir_[1] = static_cast<s16>((v & 0x1Fu) * 0x80);
        ir_[2] = static_cast<s16>(((v >> 5) & 0x1Fu) * 0x80);
        ir_[3] = static_cast<s16>(((v >> 10) & 0x1Fu) * 0x80);
        break;
    case 30: {
        lzcs_ = static_cast<s32>(v);
        u32 x = (v & 0x80000000u) ? ~v : v;
        int n = 0;
        while (n < 32 && !(x & 0x80000000u)) { ++n; x <<= 1; }
        lzcr_ = n;
        break;
    }
    default: break;
    }
}

u32 Gte::cfc2(u32 reg) const {
    switch (reg) {
    case 0: return pack(rt_[0][0], rt_[0][1]);
    case 1: return pack(rt_[0][2], rt_[1][0]);
    case 2: return pack(rt_[1][1], rt_[1][2]);
    case 3: return pack(rt_[2][0], rt_[2][1]);
    case 4: return static_cast<u32>(static_cast<s32>(rt_[2][2]));
    case 5: case 6: case 7: return static_cast<u32>(tr_[reg - 5]);
    case 8: return pack(llm_[0][0], llm_[0][1]);
    case 9: return pack(llm_[0][2], llm_[1][0]);
    case 10: return pack(llm_[1][1], llm_[1][2]);
    case 11: return pack(llm_[2][0], llm_[2][1]);
    case 12: return static_cast<u32>(static_cast<s32>(llm_[2][2]));
    case 13: case 14: case 15: return static_cast<u32>(bk_[reg - 13]);
    case 16: return pack(lcm_[0][0], lcm_[0][1]);
    case 17: return pack(lcm_[0][2], lcm_[1][0]);
    case 18: return pack(lcm_[1][1], lcm_[1][2]);
    case 19: return pack(lcm_[2][0], lcm_[2][1]);
    case 20: return static_cast<u32>(static_cast<s32>(lcm_[2][2]));
    case 21: case 22: case 23: return static_cast<u32>(fc_[reg - 21]);
    case 24: return static_cast<u32>(ofx_);
    case 25: return static_cast<u32>(ofy_);
    case 26: return static_cast<u32>(static_cast<s32>(static_cast<s16>(h_)));
    case 27: return static_cast<u32>(static_cast<s32>(dqa_));
    case 28: return static_cast<u32>(dqb_);
    case 29: return static_cast<u32>(static_cast<s32>(zsf3_));
    case 30: return static_cast<u32>(static_cast<s32>(zsf4_));
    case 31: return flag_;
    default: return 0;
    }
}

void Gte::ctc2(u32 reg, u32 v) {
    const s16 lo = static_cast<s16>(v), hi = static_cast<s16>(v >> 16);
    switch (reg) {
    case 0: rt_[0][0] = lo; rt_[0][1] = hi; break;
    case 1: rt_[0][2] = lo; rt_[1][0] = hi; break;
    case 2: rt_[1][1] = lo; rt_[1][2] = hi; break;
    case 3: rt_[2][0] = lo; rt_[2][1] = hi; break;
    case 4: rt_[2][2] = lo; break;
    case 5: case 6: case 7: tr_[reg - 5] = static_cast<s32>(v); break;
    case 8: llm_[0][0] = lo; llm_[0][1] = hi; break;
    case 9: llm_[0][2] = lo; llm_[1][0] = hi; break;
    case 10: llm_[1][1] = lo; llm_[1][2] = hi; break;
    case 11: llm_[2][0] = lo; llm_[2][1] = hi; break;
    case 12: llm_[2][2] = lo; break;
    case 13: case 14: case 15: bk_[reg - 13] = static_cast<s32>(v); break;
    case 16: lcm_[0][0] = lo; lcm_[0][1] = hi; break;
    case 17: lcm_[0][2] = lo; lcm_[1][0] = hi; break;
    case 18: lcm_[1][1] = lo; lcm_[1][2] = hi; break;
    case 19: lcm_[2][0] = lo; lcm_[2][1] = hi; break;
    case 20: lcm_[2][2] = lo; break;
    case 21: case 22: case 23: fc_[reg - 21] = static_cast<s32>(v); break;
    case 24: ofx_ = static_cast<s32>(v); break;
    case 25: ofy_ = static_cast<s32>(v); break;
    case 26: h_ = static_cast<u16>(v); break;
    case 27: dqa_ = lo; break;
    case 28: dqb_ = static_cast<s32>(v); break;
    case 29: zsf3_ = lo; break;
    case 30: zsf4_ = lo; break;
    case 31:
        flag_ = v & 0x7FFFF000u;
        if (flag_ & kErrorMask) flag_ |= 0x80000000u;
        break;
    default: break;
    }
}

void Gte::save(u32 out[64]) const {
    for (u32 r = 0; r < 32; ++r) out[r] = mfc2(r);
    for (u32 r = 0; r < 32; ++r) out[32 + r] = cfc2(r);
}

void Gte::load(const u32 in[64]) {
    for (u32 r = 0; r < 32; ++r)
        if (r != 15 && r != 28 && r != 29 && r != 31) mtc2(r, in[r]);
    for (u32 r = 0; r < 32; ++r) ctc2(r, in[32 + r]);
    flag_ = in[63];
}
}

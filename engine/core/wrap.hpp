#pragma once

#include "core/types.hpp"

namespace ac3::wrap {
constexpr s32 add(s32 a, s32 b) { return static_cast<s32>(static_cast<u32>(a) + static_cast<u32>(b)); }
constexpr s32 sub(s32 a, s32 b) { return static_cast<s32>(static_cast<u32>(a) - static_cast<u32>(b)); }
constexpr s32 neg(s32 a) { return static_cast<s32>(0u - static_cast<u32>(a)); }
constexpr s32 mul(s32 a, s32 b) { return static_cast<s32>(static_cast<u32>(a) * static_cast<u32>(b)); }

constexpr u32 shl(u32 v, u32 s) { return v << (s & 31u); }
constexpr u32 shr(u32 v, u32 s) { return v >> (s & 31u); }
constexpr s32 sar(s32 v, u32 s) { return v >> (s & 31u); }

struct HiLo {
    u32 hi, lo;
    constexpr bool operator==(const HiLo&) const = default;
};

constexpr HiLo mult(s32 a, s32 b) {
    const s64 r = static_cast<s64>(a) * static_cast<s64>(b);
    const u64 u = static_cast<u64>(r);
    return {static_cast<u32>(u >> 32), static_cast<u32>(u)};
}

constexpr HiLo multu(u32 a, u32 b) {
    const u64 r = static_cast<u64>(a) * static_cast<u64>(b);
    return {static_cast<u32>(r >> 32), static_cast<u32>(r)};
}

constexpr HiLo div(s32 n, s32 d) {
    if (d == 0) return {static_cast<u32>(n), n >= 0 ? 0xFFFFFFFFu : 1u};
    if (static_cast<u32>(n) == 0x80000000u && d == -1) return {0u, 0x80000000u};
    return {static_cast<u32>(n % d), static_cast<u32>(n / d)};
}

constexpr HiLo divu(u32 n, u32 d) {
    if (d == 0) return {n, 0xFFFFFFFFu};
    return {n % d, n / d};
}
}

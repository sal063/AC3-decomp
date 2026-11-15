#pragma once

#include "core/bytes.hpp"

#include <string>

namespace ac3 {
constexpr u64 fnv1a64(const u8* p, size_t n, u64 h = 0xCBF29CE484222325ull) {
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001B3ull;
    }
    return h;
}

inline u64 fnv1a64(ByteView b) { return fnv1a64(b.data(), b.size()); }

std::string hex64(u64 v);
}

#pragma once

#include "core/types.hpp"

#include <cstring>
#include <span>
#include <vector>

namespace ac3 {
using Bytes = std::vector<u8>;
using ByteView = std::span<const u8>;

inline u16 le16(const u8* p) { u16 v; std::memcpy(&v, p, 2); return v; }
inline u32 le32(const u8* p) { u32 v; std::memcpy(&v, p, 4); return v; }
inline s16 le16s(const u8* p) { return static_cast<s16>(le16(p)); }
inline s32 le32s(const u8* p) { return static_cast<s32>(le32(p)); }

inline void put_le16(u8* p, u16 v) { std::memcpy(p, &v, 2); }
inline void put_le32(u8* p, u32 v) { std::memcpy(p, &v, 4); }

inline bool read_le32(ByteView b, size_t off, u32& out) {
    if (off > b.size() || b.size() - off < 4) return false;
    out = le32(b.data() + off);
    return true;
}
}

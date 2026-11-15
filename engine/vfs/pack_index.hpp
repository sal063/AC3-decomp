#pragma once

#include "core/bytes.hpp"

#include <optional>
#include <vector>

namespace ac3::vfs {
constexpr u32 kIndexMagic = 0x45334341u;

struct IndexEntry {
    u32 word = 0;
    u32 sector = 0;
    u32 load_size = 0;
    bool present = true;
};

struct PackIndex {
    u32 disc = 0, date = 0, unknown = 0;
    bool streams = false;
    std::vector<IndexEntry> entries;
};

std::optional<PackIndex> parse_index(ByteView data, bool streams);
}

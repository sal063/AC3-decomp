#include "vfs/pack_index.hpp"

namespace ac3::vfs {
std::optional<PackIndex> parse_index(ByteView d, bool streams) {
    if (d.size() < 20 || le32(d.data()) != kIndexMagic) return std::nullopt;
    PackIndex idx;
    idx.disc = le32(d.data() + 4);
    idx.date = le32(d.data() + 8);
    idx.unknown = le32(d.data() + 12);
    idx.streams = streams;
    const u32 count = le32(d.data() + 16);
    if (count > 4096 || 20u + 8u * count > d.size()) return std::nullopt;
    idx.entries.resize(count);
    for (u32 i = 0; i < count; ++i) {
        IndexEntry& e = idx.entries[i];
        e.word = le32(d.data() + 20 + 8 * i);
        e.sector = le32(d.data() + 24 + 8 * i);
        if (streams) {
            e.present = (e.word & 0x80000000u) != 0;
            e.load_size = (e.word & 0x7FFFFu) << ((e.word >> 24) & 7u);
        } else {
            e.present = true;
            e.load_size = 4u * (e.word & 0x7FFFFu);
        }
    }
    return idx;
}
}

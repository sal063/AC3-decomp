#include "vfs/iso9660.hpp"

#include <algorithm>
#include <cctype>

namespace ac3::vfs {
namespace {
bool pvd(DiscImage& disc, u8 out[kUserSize]) {
    return disc.read_user(16, out) && out[0] == 1 && std::memcmp(out + 1, "CD001", 5) == 0;
}

void walk(DiscImage& disc, u32 lba, u32 size, const std::string& prefix,
          std::vector<IsoEntry>& out, int depth) {
    if (depth > 16) return;
    Bytes data;
    if (!disc.read_bytes(lba, size, data)) return;
    size_t off = 0;
    while (off < data.size()) {
        const u8 len = data[off];
        if (len == 0) {
            off = (off / kUserSize + 1) * kUserSize;
            continue;
        }
        if (off + len > data.size() || len < 34) break;
        const u8* r = data.data() + off;
        const u32 e_lba = le32(r + 2), e_size = le32(r + 10);
        const bool dir = (r[25] & 0x02) != 0;
        const u8 name_len = r[32];
        if (33u + name_len > len) break;
        std::string name(reinterpret_cast<const char*>(r + 33), name_len);
        off += len;
        if (name_len == 1 && (name[0] == 0 || name[0] == 1)) continue;
        if (const size_t semi = name.find(';'); semi != std::string::npos) name.resize(semi);
        IsoEntry e{prefix + "/" + name, e_lba, e_size, dir};
        out.push_back(e);
        if (dir) walk(disc, e_lba, e_size, e.path, out, depth + 1);
    }
}

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}
}

std::vector<IsoEntry> iso_list(DiscImage& disc) {
    std::vector<IsoEntry> out;
    u8 p[kUserSize];
    if (!pvd(disc, p)) return out;
    const u8* root = p + 156;
    walk(disc, le32(root + 2), le32(root + 10), "", out, 0);
    return out;
}

std::optional<IsoEntry> iso_find(DiscImage& disc, const std::string& name) {
    const std::string want = "/" + upper(name);
    for (const IsoEntry& e : iso_list(disc))
        if (upper(e.path) == want) return e;
    return std::nullopt;
}

std::string iso_volume_id(DiscImage& disc) {
    u8 p[kUserSize];
    if (!pvd(disc, p)) return {};
    std::string id(reinterpret_cast<const char*>(p + 40), 32);
    while (!id.empty() && id.back() == ' ') id.pop_back();
    return id;
}
}

#include "assets/mission.hpp"

#include "vfs/ace_discs.hpp"
#include "vfs/container.hpp"
#include "vfs/ulz.hpp"

namespace ac3::assets {
namespace {
bool fail(std::string* e, std::string msg) {
    if (e) *e = std::move(msg);
    return false;
}

std::vector<Tim> tims_in(ByteView dir) {
    std::vector<Tim> out;
    for (const vfs::Child& c : vfs::directory_children(dir))
        if (auto t = parse_tim(dir.subspan(c.offset, c.size))) out.push_back(std::move(*t));
    return out;
}
}

std::optional<TileType> parse_tile(ByteView t) {
    if (t.size() < 24) return std::nullopt;
    TileType tile;
    tile.head = le32(t.data());
    const u32 vb = le32(t.data() + 4), flat = le32(t.data() + 12), gour = le32(t.data() + 16);
    if (vb < 4 || vb + 4 > t.size()) return std::nullopt;
    tile.base = le16s(t.data() + vb - 4);

    const s32 nv = le32s(t.data() + vb);
    if (nv < 0 || nv > 512 || vb + 4 + 20u * static_cast<u32>((nv + 2) / 3) > t.size()) return std::nullopt;
    for (s32 i = 0; i < nv; ++i) {
        const u8* g = t.data() + vb + 4 + 20 * (i / 3);
        const int k = i % 3;
        const u32 xy = le32(g + 4 * k);
        const u32 zw = k == 0 ? le32(g + 12) & 0xFFFFu : k == 1 ? le32(g + 12) >> 16 : le32(g + 16) & 0xFFFFu;
        tile.verts.push_back({static_cast<s16>(xy & 0xFFFFu), static_cast<s16>(xy >> 16), static_cast<s16>(zw)});
    }

    auto list = [&](u32 at, bool gouraud) -> bool {
        if (at == 0) return true;
        if (at + 4 > t.size()) return false;
        const s32 n = le32s(t.data() + at);
        const u32 stride = gouraud ? 48 : 36;
        if (n < 0 || n > 512 || at + 4 + stride * static_cast<u32>(n) > t.size()) return false;
        for (s32 i = 0; i < n; ++i) {
            const u8* e = t.data() + at + 4 + stride * static_cast<u32>(i);
            TilePoly p;
            p.gouraud = gouraud;
            std::memcpy(p.head, e, 16);
            for (int k = 0; k < 4; ++k) p.idx[k] = e[16 + k];
            p.u[0] = e[20]; p.v[0] = e[21]; p.clut = le16(e + 22);
            p.u[1] = e[24]; p.v[1] = e[25]; p.tpage = le16(e + 26);
            p.u[2] = e[28]; p.v[2] = e[29]; p.u[3] = e[30]; p.v[3] = e[31];
            for (int k = 0; k < 4; ++k) p.rgb[k] = le32(e + 32 + (gouraud ? 4 * k : 0)) & 0xFFFFFFu;
            tile.polys.push_back(p);
        }
        return true;
    };
    if (!list(flat, false) || !list(gour, true)) return std::nullopt;
    return tile;
}

std::optional<Mission> load_mission(vfs::AceDiscs& discs, int m, std::string* error) {
    if (m < 0 || m >= kMissionCount) return fail(error, "no mission " + std::to_string(m)), std::nullopt;
    Mission ms;
    ms.index = m;
    ms.record = kFirstMissionRecord + static_cast<u32>(m);
    Bytes rec;
    if (!discs.read_base(ms.record, rec)) return fail(error, "cannot read record " + std::to_string(ms.record)), std::nullopt;
    const auto kids = vfs::directory_children(rec);
    if (kids.size() != 17)
        return fail(error, "record " + std::to_string(ms.record) + " is not a 17-part mission"), std::nullopt;

    for (size_t i = 0; i < 17; ++i) {
        const ByteView c = ByteView(rec).subspan(kids[i].offset, kids[i].size);
        if (vfs::ulz::is_ulz(c)) {
            const vfs::ulz::Error err = vfs::ulz::decode(c, ms.child[i]);
            if (err != vfs::ulz::Error::none)
                return fail(error, "child " + std::to_string(i) + ": " + vfs::ulz::error_name(err)), std::nullopt;
        } else {
            ms.child[i].assign(c.begin(), c.end());
        }
    }

    ms.env = ms.child[0];
    if (ms.child[1].size() != 65536u * 2) return fail(error, "child 1 is not a 256x256 grid"), std::nullopt;
    ms.grid.resize(65536);
    std::memcpy(ms.grid.data(), ms.child[1].data(), ms.child[1].size());

    for (const vfs::Child& c : vfs::directory_children(ms.child[2])) {
        auto tile = parse_tile(ByteView(ms.child[2]).subspan(c.offset, c.size));
        if (!tile) return fail(error, "tile " + std::to_string(ms.tiles.size()) + " does not parse"), std::nullopt;
        ms.tiles.push_back(std::move(*tile));
    }

    for (int k = 0; k < 5; ++k) ms.texture_sets[k] = tims_in(ms.child[11 + k]);
    return ms;
}

std::vector<Tim> boot_textures(vfs::AceDiscs& discs) {
    std::vector<Tim> out;
    Bytes rec;
    if (!discs.read_base(0, rec)) return out;
    vfs::walk_tree(rec, "0", [&](const vfs::Node& n, ByteView b) {
        if (n.kind == vfs::NodeKind::tim)
            if (auto t = parse_tim(b)) out.push_back(std::move(*t));
    });
    return out;
}

void upload_mission_textures(const Mission& m, Vram& vram) {
    for (int k = 4; k >= 0; --k)
        for (const Tim& t : m.texture_sets[k]) vram.upload_tim(t);
}
}

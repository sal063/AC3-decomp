#include "vfs/container.hpp"

#include "assets/tim.hpp"
#include "vfs/ulz.hpp"

namespace ac3::vfs {
bool is_directory(ByteView b) {
    if (b.size() < 8) return false;
    const u32 count = le32(b.data());
    if (count == 0 || count > (b.size() - 4) / 4) return false;
    const size_t header = 4 + 4 * static_cast<size_t>(count);
    for (u32 i = 0; i < count; ++i) {
        const u32 off = le32(b.data() + 4 + 4 * i);
        if (off < header || off >= b.size()) return false;
    }
    return true;
}

std::vector<Child> directory_children(ByteView b) {
    std::vector<Child> out;
    if (!is_directory(b)) return out;
    const u32 count = le32(b.data());
    out.reserve(count);
    for (u32 i = 0; i < count; ++i) {
        const u32 off = le32(b.data() + 4 + 4 * i);
        u32 next = static_cast<u32>(b.size());
        for (u32 j = 0; j < count; ++j) {
            const u32 c = le32(b.data() + 4 + 4 * j);
            if (off < c && c < next) next = c;
        }
        out.push_back({off, next - off});
    }
    return out;
}

const char* kind_name(NodeKind k) {
    switch (k) {
    case NodeKind::data: return "data";
    case NodeKind::ulz: return "ulz";
    case NodeKind::directory: return "dir";
    case NodeKind::tim: return "tim";
    case NodeKind::ulz_error: return "ulz-error";
    }
    return "?";
}

void walk_tree(ByteView blob, const std::string& path,
               const std::function<void(const Node&, ByteView)>& visit, int depth) {
    Node n;
    n.path = path;
    n.size = static_cast<u32>(blob.size());
    n.depth = depth;
    if (depth > 24) {
        visit(n, blob);
        return;
    }
    if (ulz::is_ulz(blob)) {
        Bytes decoded;
        const ulz::Error err = ulz::decode(blob, decoded);
        n.decoded_size = ulz::decoded_size(blob);
        if (err != ulz::Error::none) {
            n.kind = NodeKind::ulz_error;
            visit(n, blob);
            return;
        }
        n.kind = NodeKind::ulz;
        n.children = 1;
        visit(n, blob);
        walk_tree(decoded, path + "/u", visit, depth + 1);
        return;
    }
    if (is_directory(blob)) {
        const auto kids = directory_children(blob);
        n.kind = NodeKind::directory;
        n.children = static_cast<u32>(kids.size());
        visit(n, blob);
        for (size_t i = 0; i < kids.size(); ++i)
            walk_tree(blob.subspan(kids[i].offset, kids[i].size), path + "/" + std::to_string(i),
                      visit, depth + 1);
        return;
    }
    n.kind = assets::is_tim(blob) ? NodeKind::tim : NodeKind::data;
    visit(n, blob);
}
}

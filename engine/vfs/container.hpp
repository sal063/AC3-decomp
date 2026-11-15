#pragma once

#include "core/bytes.hpp"

#include <functional>
#include <string>
#include <vector>

namespace ac3::vfs {
struct Child {
    u32 offset = 0, size = 0;
};

bool is_directory(ByteView blob);
std::vector<Child> directory_children(ByteView blob);

enum class NodeKind { data, ulz, directory, tim, ulz_error };

struct Node {
    std::string path;
    NodeKind kind = NodeKind::data;
    u32 size = 0;
    u32 decoded_size = 0;
    u32 children = 0;
    int depth = 0;
};

void walk_tree(ByteView blob, const std::string& path,
               const std::function<void(const Node&, ByteView)>& visit, int depth = 0);

const char* kind_name(NodeKind k);
}

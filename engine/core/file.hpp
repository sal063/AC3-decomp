#pragma once

#include "core/bytes.hpp"

#include <optional>
#include <string>

namespace ac3::file {
std::optional<Bytes> read_all(const std::string& path);
bool write_all(const std::string& path, ByteView data);
bool write_text(const std::string& path, const std::string& text);
bool exists(const std::string& path);
bool make_dirs(const std::string& path);
}

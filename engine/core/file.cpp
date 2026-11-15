#include "core/file.hpp"

#include <cstdio>
#include <filesystem>
#include <system_error>

namespace ac3::file {
std::optional<Bytes> read_all(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return std::nullopt;
    Bytes data;
    if (std::fseek(f, 0, SEEK_END) == 0) {
        const long n = std::ftell(f);
        if (n > 0) data.resize(static_cast<size_t>(n));
        std::fseek(f, 0, SEEK_SET);
    }
    const size_t got = data.empty() ? 0 : std::fread(data.data(), 1, data.size(), f);
    std::fclose(f);
    if (got != data.size()) return std::nullopt;
    return data;
}

bool write_all(const std::string& path, ByteView data) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const size_t put = data.empty() ? 0 : std::fwrite(data.data(), 1, data.size(), f);
    return std::fclose(f) == 0 && put == data.size();
}

bool write_text(const std::string& path, const std::string& text) {
    return write_all(path, ByteView(reinterpret_cast<const u8*>(text.data()), text.size()));
}

bool exists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

bool make_dirs(const std::string& path) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    return !ec || std::filesystem::is_directory(path, ec);
}
}

#pragma once

#include "core/bytes.hpp"

#include <string>

namespace ac3::png {
Bytes encode_rgba(const u8* rgba, u32 width, u32 height);
bool write_rgba(const std::string& path, const u8* rgba, u32 width, u32 height);

u32 crc32(const u8* p, size_t n, u32 crc = 0);

bool read_rgba(const std::string& path, Bytes& rgba, u32& width, u32& height, std::string* error = nullptr);
bool decode_rgba(ByteView png, Bytes& rgba, u32& width, u32& height, std::string* error = nullptr);
}

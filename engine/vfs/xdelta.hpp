#pragma once

#include "core/bytes.hpp"

#include <functional>
#include <vector>

namespace ac3::vfs {
struct XdeltaWindow {
    u64 src_pos = 0, src_len = 0;
    u64 target_pos = 0;
    u32 target_len = 0;
    bool has_adler = false;
    u32 adler = 0;
    const u8 *data = nullptr, *inst = nullptr, *addr = nullptr;
    size_t data_len = 0, inst_len = 0, addr_len = 0;
};

class Xdelta {
public:
    bool parse(ByteView patch);
    const std::vector<XdeltaWindow>& windows() const { return windows_; }
    u64 target_size() const { return target_size_; }
    u32 largest_window() const { return largest_; }

    using Copied = std::function<void(u64, u32, u64)>;
    static bool apply(const XdeltaWindow& w, const u8* source, u8* target, const Copied& copied = {});

private:
    std::vector<XdeltaWindow> windows_;
    Bytes sections_[3];
    u64 target_size_ = 0;
    u32 largest_ = 0;
};

u32 adler32(const u8* p, size_t n, u32 sum = 1);

bool xz_decode(ByteView in, u8* out, size_t out_len, size_t& produced);
}

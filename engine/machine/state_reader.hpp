#pragma once

#include "core/types.hpp"

#include <cstring>
#include <string>
#include <vector>

namespace ac3::machine {
struct StateReader {
    const u8* p = nullptr;
    const u8* end = nullptr;
    bool ok = true;
    std::string error;

    StateReader(const u8* data, size_t size) : p(data), end(data + size) {}

    u32 u() {
        if (end - p < 4) return fail("truncated"), 0u;
        u32 v;
        std::memcpy(&v, p, 4);
        p += 4;
        return v;
    }
    s32 s() { return static_cast<s32>(u()); }
    bool tag(const char* want) {
        if (end - p < 4) return fail("truncated"), false;
        char t[5] = {};
        std::memcpy(t, p, 4);
        p += 4;
        if (std::strncmp(t, want, 4) != 0) return fail(std::string("expected section ") + want + ", found " + t), false;
        return true;
    }
    bool block(std::vector<u8>& out) {
        const u32 n = u();
        if (static_cast<size_t>(end - p) < n) return fail("truncated block"), false;
        out.assign(p, p + n);
        p += n;
        return true;
    }
    bool block_into(void* dst, size_t size) {
        const u32 n = u();
        if (n != size) return fail("a block of unexpected size"), false;
        if (static_cast<size_t>(end - p) < n) return fail("truncated block"), false;
        std::memcpy(dst, p, n);
        p += n;
        return true;
    }
    void fail(const std::string& why) {
        if (ok) error = why;
        ok = false;
    }
};
}

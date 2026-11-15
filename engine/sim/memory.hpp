#pragma once

#include "core/types.hpp"

#include <cstring>
#include <vector>

namespace ac3::sim {
class Memory {
public:
    static constexpr u32 kRamSize = 0x200000u;
    static constexpr u32 kScratchSize = 0x400u;
    static constexpr u32 kScratchBase = 0x1F800000u;
    static constexpr u32 kExtBase = 0x200000u, kExtSize = 0x600000u;

    Memory() : ram_(kRamSize, 0), scratch_(kScratchSize, 0) {}

    void extend() {
        if (ext_.empty()) ext_.assign(kExtSize, 0);
    }

    u8* ram() { return ram_.data(); }
    const u8* ram() const { return ram_.data(); }
    u8* scratch() { return scratch_.data(); }
    const u8* scratch() const { return scratch_.data(); }

    u8* ptr(u32 addr, u32 len = 1) {
        const u32 top = addr >> 24;
        if (top == 0x80u || top == 0x00u || top == 0xA0u) {
            const u32 p = addr & 0x1FFFFFu;
            if ((addr & 0x00E00000u) == 0 && p + len <= kRamSize) return ram_.data() + p;
            const u32 e = (addr & 0x00FFFFFFu) - kExtBase;
            if (!ext_.empty() && (addr & 0x00FFFFFFu) >= kExtBase && e + len <= ext_.size()) return ext_.data() + e;
            return nullptr;
        }
        if ((addr & ~(kScratchSize - 1u)) == kScratchBase && (addr - kScratchBase) + len <= kScratchSize)
            return scratch_.data() + (addr - kScratchBase);
        return nullptr;
    }
    const u8* ptr(u32 addr, u32 len = 1) const { return const_cast<Memory*>(this)->ptr(addr, len); }

    u32 r32(u32 a) const { return load<u32>(a); }
    u16 r16(u32 a) const { return load<u16>(a); }
    u8 r8(u32 a) const { return load<u8>(a); }
    s32 r32s(u32 a) const { return static_cast<s32>(load<u32>(a)); }
    s16 r16s(u32 a) const { return static_cast<s16>(load<u16>(a)); }
    s8 r8s(u32 a) const { return static_cast<s8>(load<u8>(a)); }

    void w32(u32 a, u32 v) { store<u32>(a, v); }
    void w16(u32 a, u32 v) { store<u16>(a, static_cast<u16>(v)); }
    void w8(u32 a, u32 v) { store<u8>(a, static_cast<u8>(v)); }

    void copy(u32 dst, u32 src, u32 n) {
        for (u32 i = 0; i < n; ++i) w8(dst + i, r8(src + i));
    }
    void fill(u32 dst, u8 v, u32 n) {
        if (u8* p = ptr(dst, n)) { std::memset(p, v, n); return; }
        for (u32 i = 0; i < n; ++i) w8(dst + i, v);
    }

    u32 faults() const { return faults_; }
    u32 first_fault() const { return first_fault_; }
    void reset_faults() { faults_ = 0; first_fault_ = 0; }

    static constexpr u8 kWroteFirst = 1, kReadFirst = 2;
    void set_access_log(std::vector<u8>* log) { log_ = log; }
    void set_access_tags(std::vector<u32>* tags, const u32* tag) {
        tags_ = tags;
        tag_ = tag;
    }

private:
    template <class T>
    T load(u32 a) const {
        if (const u8* p = ptr(a, sizeof(T))) {
            if (log_) note(p, sizeof(T), kReadFirst);
            T v;
            std::memcpy(&v, p, sizeof(T));
            return v;
        }
        if (!io(a)) fault(a);
        return 0;
    }
    template <class T>
    void store(u32 a, T v) {
        if (u8* p = ptr(a, sizeof(T))) {
            if (log_) note(p, sizeof(T), kWroteFirst);
            std::memcpy(p, &v, sizeof(T));
            return;
        }
        if (!io(a)) fault(a);
    }
    static bool io(u32 a) {
        const u32 p = a & 0x1FFFFFFFu;
        return p >= 0x1F801000u && p < 0x1F803000u;
    }
    void note(const u8* p, u32 n, u8 how) const {
        if (!ext_.empty() && p >= ext_.data() && p < ext_.data() + ext_.size()) return;
        const u8* r = ram_.data();
        const std::size_t off = (p >= r && p < r + kRamSize) ? static_cast<std::size_t>(p - r)
                                                              : kRamSize + static_cast<std::size_t>(p - scratch_.data());
        for (u32 i = 0; i < n; ++i) {
            if ((*log_)[off + i]) continue;
            (*log_)[off + i] = how;
            if (how == kReadFirst && tags_ && tag_) (*tags_)[off + i] = *tag_;
        }
    }
    void fault(u32 a) const;

    std::vector<u8> ram_, scratch_, ext_;
    mutable u32 faults_ = 0, first_fault_ = 0;
    std::vector<u8>* log_ = nullptr;
    std::vector<u32>* tags_ = nullptr;
    const u32* tag_ = nullptr;
};
}

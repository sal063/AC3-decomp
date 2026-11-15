#pragma once

#include "core/types.hpp"

namespace ac3::sim {
class PsyqRand {
public:
    constexpr explicit PsyqRand(u32 seed = 0) : state_(seed) {}

    constexpr void seed(u32 s) { state_ = s; }
    constexpr u32 state() const { return state_; }

    constexpr s32 next() {
        state_ = state_ * 0x41C64E6Du + 0x3039u;
        return static_cast<s32>((state_ >> 16) & 0x7FFFu);
    }

private:
    u32 state_;
};
}

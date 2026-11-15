#pragma once

#include "core/types.hpp"

#include <functional>

namespace ac3::sim {
class Memory;
}

namespace ac3::machine {
class Mdec {
public:
    explicit Mdec(sim::Memory& m);

    std::function<void(u32 fn, u32 a0)> call_guest;

    u32 reset();
    u32 in(u32 buf, u32 mode);
    u32 out(u32 dst, u32 words);
    u32 in_callback(u32 fn);
    u32 out_callback(u32 fn);
    u32 put_env(u32 env);

private:
    bool block_from_stream(u32* addr, u32 end, s16* out);

    sim::Memory& m;
    u8 quant_[64];
    u32 coef_buf_ = 0, coef_pos_ = 0, coef_end_ = 0;
    int out_24bit_ = 0, out_mask15_ = 0;
    u32 in_callback_ = 0, out_callback_ = 0;
};
}

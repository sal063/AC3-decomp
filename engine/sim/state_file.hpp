#pragma once

#include "core/types.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace ac3::sim {
struct MachineState {
    u32 tick = 0, field = 0, target = 0, checkpoint = 0;
    std::vector<u8> ram;
    std::vector<u8> scratch;
    u32 gte[64] = {};
    u32 regs[34] = {};
    u32 changed = 0;
};

class StateFile {
public:
    ~StateFile();
    bool open(const std::string& path, std::string* error);
    bool next(MachineState& out);
    const std::string& error() const { return error_; }

private:
    std::FILE* f_ = nullptr;
    std::vector<u8> ram_;
    std::string error_;
};
}

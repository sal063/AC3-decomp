#include "sim/state_file.hpp"

#include "sim/memory.hpp"

#include <cstring>

namespace ac3::sim {
StateFile::~StateFile() {
    if (f_) std::fclose(f_);
}

bool StateFile::open(const std::string& path, std::string* error) {
    f_ = std::fopen(path.c_str(), "rb");
    char magic[4];
    u32 version = 0;
    if (!f_ || std::fread(magic, 1, 4, f_) != 4 || std::memcmp(magic, "AC3S", 4) != 0 ||
        std::fread(&version, 4, 1, f_) != 1 || version != 1) {
        if (error) *error = path + ": not an oracle state file (AC3S v1)";
        return false;
    }
    ram_.assign(Memory::kRamSize, 0);
    return true;
}

bool StateFile::next(MachineState& out) {
    u32 head[5];
    if (!f_ || std::fread(head, 4, 5, f_) != 5) return false;
    out.tick = head[0];
    out.field = head[1];
    out.target = head[2];
    out.checkpoint = head[3];
    out.changed = head[4];
    std::vector<u32> pairs(2 * static_cast<size_t>(head[4]));
    if (!pairs.empty() && std::fread(pairs.data(), 8, head[4], f_) != head[4]) {
        error_ = "truncated RAM diff";
        return false;
    }
    for (size_t i = 0; i < head[4]; ++i) {
        const u32 off = pairs[2 * i];
        if (off + 4 > Memory::kRamSize) {
            error_ = "RAM offset out of range";
            return false;
        }
        std::memcpy(ram_.data() + off, &pairs[2 * i + 1], 4);
    }
    out.ram = ram_;
    out.scratch.resize(Memory::kScratchSize);
    if (std::fread(out.scratch.data(), 1, Memory::kScratchSize, f_) != Memory::kScratchSize ||
        std::fread(out.gte, 4, 64, f_) != 64 || std::fread(out.regs, 4, 34, f_) != 34) {
        error_ = "truncated record";
        return false;
    }
    return true;
}
}

#pragma once

#include "core/types.hpp"
#include "sim/game/game.hpp"

#include <deque>
#include <string>
#include <utility>
#include <vector>

namespace ac3::viewer {
struct Decoded {
    std::string text;
    u32 size = 1;
    bool conditional = false;
    u32 yes = 0, no = 0;
};
Decoded decode(const sim::Memory& m, u32 pc);

void listing(const sim::Memory& m, u32 pc, std::vector<std::string>& out, size_t limit = 4000, int depth = 0);

std::string label(const sim::Memory& m, u32 pc);

std::vector<std::pair<u32, std::string>> entries(const sim::Memory& m);

std::string unit_name(u32 slot);

class Tracer final : public sim::ScriptObserver {
public:
    explicit Tracer(const sim::Memory& m) : m_(m) {}
    void script_begin(u32 pc) override;
    void script_op(u32 pc, u32 op) override;
    void script_end(u32 pc) override;

    u32 field = 0;
    bool frozen = false;
    struct Line {
        u32 field;
        int depth;
        bool header;
        std::string text;
    };
    const std::deque<Line>& lines() const { return lines_; }
    u64 total() const { return total_; }
    u32 runs() const { return runs_; }
    void clear() { lines_.clear(); }

private:
    void add(int depth, bool header, std::string text);
    const sim::Memory& m_;
    std::deque<Line> lines_;
    int depth_ = 0;
    u64 total_ = 0;
    u32 runs_ = 0;
};
}

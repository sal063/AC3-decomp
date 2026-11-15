#pragma once

#include "core/types.hpp"
#include "machine/machine.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace ac3::machine {
struct InputRecording {
    std::vector<u8> data;

    bool load(const std::string& path) {
        std::FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) return false;
        std::fseek(f, 0, SEEK_END);
        data.resize(static_cast<size_t>(std::ftell(f)));
        std::fseek(f, 0, SEEK_SET);
        const bool ok = std::fread(data.data(), 1, data.size(), f) == data.size();
        std::fclose(f);
        return ok && data.size() >= 16 && !std::memcmp(data.data(), "AC3I", 4);
    }

    u32 fields() const { return data.size() < 16 ? 0u : static_cast<u32>((data.size() - 16) / 24); }

    void pads(u32 vb, PadInput out[2]) const {
        const size_t off = 16 + 24 * static_cast<size_t>(vb - 1);
        if (!vb || off + 24 > data.size()) {
            for (int p = 0; p < 2; ++p) {
                out[p].buttons = 0;
                out[p].lx = out[p].ly = out[p].rx = out[p].ry = 0x80;
            }
            return;
        }
        for (int p = 0; p < 2; ++p) {
            const u8* r = data.data() + off + 4 + 10 * p;
            out[p].connected = r[0] != 0;
            out[p].type = r[1];
            out[p].buttons = static_cast<u16>(r[2] | (r[3] << 8));
            out[p].lx = r[4];
            out[p].ly = r[5];
            out[p].rx = r[6];
            out[p].ry = r[7];
        }
    }
};
}

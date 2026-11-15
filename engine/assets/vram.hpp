#pragma once

#include "assets/tim.hpp"

#include <vector>

namespace ac3::assets {
class Vram {
public:
    static constexpr int kWidth = 1024, kHeight = 512;

    Vram() : px_(static_cast<size_t>(kWidth) * kHeight, 0), written_(px_.size(), 0) {}

    void upload(int x, int y, int w, int h, const u16* data);
    void upload_tim(const Tim& t);

    u16 at(int x, int y) const { return px_[static_cast<size_t>(y & 511) * kWidth + (x & 1023)]; }
    bool written(int x, int y) const { return written_[static_cast<size_t>(y & 511) * kWidth + (x & 1023)] != 0; }
    const std::vector<u16>& pixels() const { return px_; }

    u16 texel(u16 tpage, u16 clut, u32 u, u32 v) const;

    Image to_image() const;

private:
    std::vector<u16> px_;
    std::vector<u8> written_;
};
}

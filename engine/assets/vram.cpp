#include "assets/vram.hpp"

namespace ac3::assets {
void Vram::upload(int x, int y, int w, int h, const u16* data) {
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i) {
            const size_t at = static_cast<size_t>((y + j) & 511) * kWidth + ((x + i) & 1023);
            px_[at] = data[static_cast<size_t>(j) * w + i];
            written_[at] = 1;
        }
}

void Vram::upload_tim(const Tim& t) {
    if (!t.data.empty()) upload(t.img_x, t.img_y, t.img_w, t.img_h, t.data.data());
    if (t.has_palette && !t.palette.empty()) upload(t.pal_x, t.pal_y, t.pal_w, t.pal_h, t.palette.data());
}

u16 Vram::texel(u16 tpage, u16 clut, u32 u, u32 v) const {
    const int px = (tpage & 15) * 64, py = ((tpage >> 4) & 1) * 256;
    const int depth = (tpage >> 7) & 3;
    const int cx = (clut & 63) * 16, cy = (clut >> 6) & 511;
    u &= 255;
    v &= 255;
    switch (depth) {
    case 0: {
        const u16 w = at(px + static_cast<int>(u >> 2), py + static_cast<int>(v));
        return at(cx + ((w >> ((u & 3) * 4)) & 15), cy);
    }
    case 1: {
        const u16 w = at(px + static_cast<int>(u >> 1), py + static_cast<int>(v));
        return at(cx + ((w >> ((u & 1) * 8)) & 255), cy);
    }
    default:
        return at(px + static_cast<int>(u), py + static_cast<int>(v));
    }
}

Image Vram::to_image() const {
    Image img;
    img.width = kWidth;
    img.height = kHeight;
    img.pixels.resize(px_.size());
    for (size_t i = 0; i < px_.size(); ++i) {
        Rgba8 c = rgba_from_555(px_[i]);
        c.a = 255;
        img.pixels[i] = c;
    }
    return img;
}
}

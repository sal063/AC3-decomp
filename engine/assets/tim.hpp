#pragma once

#include "core/bytes.hpp"

#include <optional>
#include <vector>

namespace ac3::assets {
struct Rgba8 {
    u8 r, g, b, a;
};

struct Image {
    u32 width = 0, height = 0;
    std::vector<Rgba8> pixels;
};

struct Tim {
    int depth = 0;
    bool has_palette = false;
    u16 pal_x = 0, pal_y = 0, pal_w = 0, pal_h = 0;
    std::vector<u16> palette;
    u16 img_x = 0, img_y = 0, img_w = 0, img_h = 0;
    std::vector<u16> data;
    u32 size = 0;

    u32 pixel_width() const;
};

bool is_tim(ByteView b);
std::optional<Tim> parse_tim(ByteView b);

Rgba8 rgba_from_555(u16 c);

Image to_image(const Tim& t, u32 palette_row = 0);
}

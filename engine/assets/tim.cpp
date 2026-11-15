#include "assets/tim.hpp"

namespace ac3::assets {
u32 Tim::pixel_width() const {
    switch (depth) {
    case 0: return img_w * 4u;
    case 1: return img_w * 2u;
    case 2: return img_w;
    case 3: return img_w * 2u / 3u;
    default: return 0;
    }
}

bool is_tim(ByteView b) {
    if (b.size() <= 12 || le32(b.data()) != 0x10) return false;
    const u32 flags = le32(b.data() + 4);
    if (flags > 15 || (flags & 7) > 4) return false;
    const u32 first = le32(b.data() + 8);
    return first >= 12 && first <= b.size() - 8;
}

std::optional<Tim> parse_tim(ByteView b) {
    if (!is_tim(b)) return std::nullopt;
    Tim t;
    const u32 flags = le32(b.data() + 4);
    t.depth = static_cast<int>(flags & 7);
    t.has_palette = (flags & 8) != 0;
    if (t.depth > 3) return std::nullopt;
    size_t pos = 8;
    auto block = [&](u16& x, u16& y, u16& w, u16& h, std::vector<u16>& out) -> bool {
        if (pos + 12 > b.size()) return false;
        const u32 bytes = le32(b.data() + pos);
        x = le16(b.data() + pos + 4);
        y = le16(b.data() + pos + 6);
        w = le16(b.data() + pos + 8);
        h = le16(b.data() + pos + 10);
        const size_t n = static_cast<size_t>(w) * h;
        if (bytes < 12 || pos + 12 + n * 2 > b.size()) return false;
        out.resize(n);
        std::memcpy(out.data(), b.data() + pos + 12, n * 2);
        pos += bytes;
        return true;
    };
    if (t.has_palette && !block(t.pal_x, t.pal_y, t.pal_w, t.pal_h, t.palette)) return std::nullopt;
    if (!block(t.img_x, t.img_y, t.img_w, t.img_h, t.data)) return std::nullopt;
    t.size = static_cast<u32>(pos);
    return t;
}

Rgba8 rgba_from_555(u16 c) {
    auto x = [](u32 v) { return static_cast<u8>((v << 3) | (v >> 2)); };
    return {x(c & 31u), x((c >> 5) & 31u), x((c >> 10) & 31u), static_cast<u8>(c == 0 ? 0 : 255)};
}

Image to_image(const Tim& t, u32 palette_row) {
    Image img;
    img.width = t.pixel_width();
    img.height = t.img_h;
    img.pixels.resize(static_cast<size_t>(img.width) * img.height);
    const u16* pal = nullptr;
    if (t.has_palette && t.pal_w && t.pal_h)
        pal = t.palette.data() + static_cast<size_t>(t.pal_w) * (palette_row % t.pal_h);
    auto from_pal = [&](u32 index) -> Rgba8 {
        if (!pal) {
            const u8 g = static_cast<u8>(index * (t.depth == 0 ? 17 : 1));
            return {g, g, g, 255};
        }
        return index < t.pal_w ? rgba_from_555(pal[index]) : Rgba8{255, 0, 255, 255};
    };
    const u8* bytes = reinterpret_cast<const u8*>(t.data.data());
    for (u32 y = 0; y < img.height; ++y) {
        const u8* row = bytes + static_cast<size_t>(y) * t.img_w * 2;
        for (u32 x = 0; x < img.width; ++x) {
            Rgba8 c;
            switch (t.depth) {
            case 0: c = from_pal((row[x / 2] >> ((x & 1) * 4)) & 15u); break;
            case 1: c = from_pal(row[x]); break;
            case 2: c = rgba_from_555(le16(row + x * 2)); break;
            default: c = {row[x * 3], row[x * 3 + 1], row[x * 3 + 2], 255}; break;
            }
            img.pixels[static_cast<size_t>(y) * img.width + x] = c;
        }
    }
    return img;
}
}

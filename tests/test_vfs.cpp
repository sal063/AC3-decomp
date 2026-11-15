#include "test.hpp"

#include "assets/tim.hpp"
#include "vfs/container.hpp"
#include "vfs/pack_index.hpp"
#include "vfs/ulz.hpp"
#include "vfs/xdelta.hpp"

#include <string>

using namespace ac3;
using namespace ac3::vfs;

namespace {
Bytes make_ulz(u32 type, u32 size, u32 window_bits, const std::vector<u32>& flags,
               const std::string& literals, const std::vector<u16>& refs) {
    Bytes b(16);
    put_le32(b.data(), 0x1A7A6C55u);
    put_le32(b.data() + 4, (type << 24) | size);
    for (u32 f : flags) {
        b.resize(b.size() + 4);
        put_le32(b.data() + b.size() - 4, f);
    }
    const u32 lit = static_cast<u32>(b.size());
    b.insert(b.end(), literals.begin(), literals.end());
    const u32 ref = static_cast<u32>(b.size());
    for (u16 r : refs) {
        b.resize(b.size() + 2);
        put_le16(b.data() + b.size() - 2, r);
    }
    while (b.size() <= 20) b.push_back(0);
    put_le32(b.data() + 8, (window_bits << 24) | lit);
    put_le32(b.data() + 12, ref);
    return b;
}

std::string str(const Bytes& b) { return std::string(b.begin(), b.end()); }
}

AC3_TEST(ulz_literals_and_overlapping_reference) {
    const Bytes in = make_ulz(2, 5, 8, {0xC0000000u}, "AB", {0x0001});
    Bytes out;
    CHECK(ulz::decode(in, out) == ulz::Error::none);
    CHECK_EQ(str(out), std::string("ABABA"));
}

AC3_TEST(ulz_long_reference_length) {
    const Bytes in = make_ulz(2, 9, 8, {0x80000000u}, "Z", {0x0500});
    Bytes out;
    CHECK(ulz::decode(in, out) == ulz::Error::none);
    CHECK_EQ(str(out), std::string(9, 'Z'));
}

AC3_TEST(ulz_type0_skips_bit0_of_each_flag_word) {
    std::string lits;
    for (int i = 0; i < 40; ++i) lits += static_cast<char>('a' + i % 26);
    const Bytes in = make_ulz(0, 40, 8, {0xFFFFFFFEu, 0xFF800000u}, lits, {});
    Bytes out;
    CHECK(ulz::decode(in, out) == ulz::Error::none);
    CHECK_EQ(str(out), lits);
}

AC3_TEST(ulz_type2_uses_all_32_bits) {
    std::string lits;
    for (int i = 0; i < 40; ++i) lits += static_cast<char>('A' + i % 26);
    const Bytes in = make_ulz(2, 40, 8, {0xFFFFFFFFu, 0xFF000000u}, lits, {});
    Bytes out;
    CHECK(ulz::decode(in, out) == ulz::Error::none);
    CHECK_EQ(str(out), lits);
}

AC3_TEST(ulz_rejects_bad_input) {
    Bytes out;
    CHECK(ulz::decode(make_ulz(2, 4, 8, {0x80000000u}, "A", {0x0005}), out) == ulz::Error::bad_reference);
    CHECK(ulz::decode(make_ulz(2, 4, 8, {0x80000000u}, "A", {0x0F00}), out) == ulz::Error::overrun);
    CHECK(ulz::decode(make_ulz(1, 1, 8, {0x80000000u}, "A", {}), out) == ulz::Error::bad_type);
    CHECK(ulz::decode(make_ulz(2, 1, 16, {0x80000000u}, "A", {}), out) == ulz::Error::bad_window);
    Bytes cut = make_ulz(2, 30, 8, {0xFFFFFFFFu}, "AB", {});
    CHECK(ulz::decode(cut, out) == ulz::Error::truncated);
    CHECK(ulz::decode(Bytes(32, 0), out) == ulz::Error::not_ulz);
}

AC3_TEST(directory_children_unsorted) {
    Bytes b(16 + 12);
    put_le32(b.data(), 3);
    put_le32(b.data() + 4, 24);
    put_le32(b.data() + 8, 16);
    put_le32(b.data() + 12, 20);
    CHECK(is_directory(b));
    const auto kids = directory_children(b);
    CHECK_EQ(kids.size(), size_t{3});
    CHECK_EQ(kids[0].offset, 24u);
    CHECK_EQ(kids[0].size, 4u);
    CHECK_EQ(kids[1].size, 4u);
    CHECK_EQ(kids[2].size, 4u);
}

AC3_TEST(directory_rejects_offsets_inside_header_or_outside) {
    Bytes b(16);
    put_le32(b.data(), 2);
    put_le32(b.data() + 4, 8);
    put_le32(b.data() + 8, 12);
    CHECK(!is_directory(b));
    put_le32(b.data() + 4, 12);
    put_le32(b.data() + 8, 16);
    CHECK(!is_directory(b));
}

AC3_TEST(tim_4bit_through_palette) {
    Bytes b(8 + 12 + 32 + 12 + 2);
    put_le32(b.data(), 0x10);
    put_le32(b.data() + 4, 8 | 0);
    put_le32(b.data() + 8, 12 + 32);
    put_le16(b.data() + 12, 0);
    put_le16(b.data() + 14, 480);
    put_le16(b.data() + 16, 16);
    put_le16(b.data() + 18, 1);
    for (int i = 0; i < 16; ++i) put_le16(b.data() + 20 + 2 * i, static_cast<u16>(i == 0 ? 0 : 0x7C00 | i));
    put_le32(b.data() + 52, 12 + 2);
    put_le16(b.data() + 56, 320);
    put_le16(b.data() + 58, 0);
    put_le16(b.data() + 60, 1);
    put_le16(b.data() + 62, 1);
    put_le16(b.data() + 64, 0x3210);
    CHECK(assets::is_tim(b));
    auto t = assets::parse_tim(b);
    CHECK(t.has_value());
    if (!t) return;
    CHECK_EQ(t->pixel_width(), 4u);
    CHECK_EQ(t->img_x, u16{320});
    const assets::Image img = assets::to_image(*t);
    CHECK_EQ(img.pixels[0].a, u8{0});
    CHECK_EQ(img.pixels[1].b, u8{255});
    CHECK_EQ(img.pixels[3].r, u8{(3 << 3) | (3 >> 2)});
}

AC3_TEST(index_stream_words) {
    Bytes b(20 + 16);
    put_le32(b.data(), kIndexMagic);
    put_le32(b.data() + 4, 1);
    put_le32(b.data() + 16, 2);
    put_le32(b.data() + 20, 0xBA200F85u);
    put_le32(b.data() + 24, 100);
    put_le32(b.data() + 28, 0x58003B6Au);
    auto idx = parse_index(b, true);
    CHECK(idx.has_value());
    if (!idx) return;
    CHECK(idx->entries[0].present);
    CHECK_EQ(idx->entries[0].load_size, 0xF85u << 2);
    CHECK(!idx->entries[1].present);
    auto base = parse_index(b, false);
    CHECK_EQ(base->entries[0].load_size, 4u * (0xBA200F85u & 0x7FFFFu));
}

AC3_TEST(xdelta_window) {
    const std::string source = "ABCDEFGHIJKLMNOP";
    const u8 patch[] = {0xD6, 0xC3, 0xC4, 0x00, 0x00,
                        0x01, 16, 0,
                        16,
                        20, 0x00, 4, 5, 2,
                        'x', 'y', 'z', 'q',
                        24, 4, 0, 5, 20,
                        0, 24};
    Xdelta x;
    CHECK(x.parse(ByteView(patch, sizeof patch)));
    CHECK_EQ(x.windows().size(), size_t{1});
    CHECK_EQ(x.target_size(), u64{20});
    if (x.windows().size() != 1) return;
    u8 target[20] = {};
    u64 copied = 0;
    CHECK(Xdelta::apply(x.windows()[0], reinterpret_cast<const u8*>(source.data()), target,
                        [&](u64, u32 size, u64) { copied += size; }));
    CHECK_EQ(std::string(reinterpret_cast<const char*>(target), 20), std::string("ABCDEFGHxyzqqqqqxyzq"));
    CHECK_EQ(copied, u64{8});
    CHECK_EQ(adler32(reinterpret_cast<const u8*>("Wikipedia"), 9), 0x11E60398u);
    CHECK(!x.parse(ByteView(patch, sizeof patch - 3)));
    CHECK(!x.parse(ByteView(patch + 1, sizeof patch - 1)));
}

#include "core/png.hpp"

#include "core/file.hpp"
#include "core/hash.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ac3 {
std::string hex64(u64 v) {
    char b[17];
    std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(v));
    return b;
}

namespace png {
namespace {
const std::array<u32, 256>& crc_table() {
    static const std::array<u32, 256> t = [] {
        std::array<u32, 256> r{};
        for (u32 n = 0; n < 256; ++n) {
            u32 c = n;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            r[n] = c;
        }
        return r;
    }();
    return t;
}

void put_be32(Bytes& out, u32 v) {
    out.push_back(static_cast<u8>(v >> 24));
    out.push_back(static_cast<u8>(v >> 16));
    out.push_back(static_cast<u8>(v >> 8));
    out.push_back(static_cast<u8>(v));
}

void chunk(Bytes& out, const char type[4], const Bytes& data) {
    put_be32(out, static_cast<u32>(data.size()));
    const size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    put_be32(out, crc32(out.data() + start, out.size() - start));
}
}

u32 crc32(const u8* p, size_t n, u32 crc) {
    crc = ~crc;
    const auto& t = crc_table();
    for (size_t i = 0; i < n; ++i) crc = t[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
    return ~crc;
}

Bytes encode_rgba(const u8* rgba, u32 width, u32 height) {
    Bytes raw;
    const size_t stride = static_cast<size_t>(width) * 4;
    raw.reserve((stride + 1) * height);
    for (u32 y = 0; y < height; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), rgba + y * stride, rgba + (y + 1) * stride);
    }

    Bytes z;
    z.push_back(0x78);
    z.push_back(0x01);
    size_t pos = 0;
    do {
        const size_t n = std::min<size_t>(65535, raw.size() - pos);
        const bool last = pos + n == raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(static_cast<u8>(n));
        z.push_back(static_cast<u8>(n >> 8));
        z.push_back(static_cast<u8>(~n));
        z.push_back(static_cast<u8>(~n >> 8));
        z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos),
                 raw.begin() + static_cast<std::ptrdiff_t>(pos + n));
        pos += n;
    } while (pos < raw.size());
    u32 a = 1, b = 0;
    for (u8 c : raw) {
        a = (a + c) % 65521u;
        b = (b + a) % 65521u;
    }
    put_be32(z, (b << 16) | a);

    Bytes out = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    Bytes ihdr;
    put_be32(ihdr, width);
    put_be32(ihdr, height);
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});
    chunk(out, "IHDR", ihdr);
    chunk(out, "IDAT", z);
    chunk(out, "IEND", {});
    return out;
}

bool write_rgba(const std::string& path, const u8* rgba, u32 width, u32 height) {
    return file::write_all(path, encode_rgba(rgba, width, height));
}

namespace {
class Inflater {
public:
    Inflater(const u8* p, size_t n, Bytes& out) : p_(p), n_(n), out_(out) {}

    bool run() {
        for (bool last = false; !last;) {
            last = bits(1) != 0;
            const u32 type = bits(2);
            if (bad_) return false;
            if (type == 0) {
                if (!stored()) return false;
            } else if (type == 1) {
                fixed_tables();
                if (!codes()) return false;
            } else if (type == 2) {
                if (!dynamic_tables() || !codes()) return false;
            } else {
                return false;
            }
        }
        return !bad_;
    }

private:
    struct Huffman {
        u16 count[16] = {};
        u16 symbol[320] = {};
    };

    u32 bits(u32 need) {
        while (have_ < need) {
            if (at_ >= n_) {
                bad_ = true;
                return 0;
            }
            buf_ |= static_cast<u32>(p_[at_++]) << have_;
            have_ += 8;
        }
        const u32 v = buf_ & ((1u << need) - 1u);
        buf_ >>= need;
        have_ -= need;
        return v;
    }

    static void build(Huffman& h, const u8* lengths, u32 n) {
        std::fill(std::begin(h.count), std::end(h.count), static_cast<u16>(0));
        for (u32 i = 0; i < n; ++i) ++h.count[lengths[i]];
        h.count[0] = 0;
        u16 offs[16] = {};
        for (u32 len = 1; len < 16; ++len) offs[len] = static_cast<u16>(offs[len - 1] + h.count[len - 1]);
        for (u32 i = 0; i < n; ++i)
            if (lengths[i]) h.symbol[offs[lengths[i]]++] = static_cast<u16>(i);
    }

    int decode(const Huffman& h) {
        int code = 0, first = 0, index = 0;
        for (int len = 1; len < 16; ++len) {
            code |= static_cast<int>(bits(1));
            if (bad_) return -1;
            const int count = h.count[len];
            if (code - count < first) return h.symbol[index + (code - first)];
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
        }
        bad_ = true;
        return -1;
    }

    bool stored() {
        buf_ = 0;
        have_ = 0;
        if (at_ + 4 > n_) return false;
        const u32 len = p_[at_] | (static_cast<u32>(p_[at_ + 1]) << 8);
        at_ += 4;
        if (at_ + len > n_) return false;
        out_.insert(out_.end(), p_ + at_, p_ + at_ + len);
        at_ += len;
        return true;
    }

    void fixed_tables() {
        u8 l[288];
        for (u32 i = 0; i < 144; ++i) l[i] = 8;
        for (u32 i = 144; i < 256; ++i) l[i] = 9;
        for (u32 i = 256; i < 280; ++i) l[i] = 7;
        for (u32 i = 280; i < 288; ++i) l[i] = 8;
        build(lit_, l, 288);
        u8 d[30];
        std::fill(std::begin(d), std::end(d), static_cast<u8>(5));
        build(dist_, d, 30);
    }

    bool dynamic_tables() {
        const u32 nlen = bits(5) + 257, ndist = bits(5) + 1, ncode = bits(4) + 4;
        if (bad_ || nlen > 286 || ndist > 30) return false;
        static constexpr u8 kOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
        u8 lengths[320] = {};
        for (u32 i = 0; i < ncode; ++i) lengths[kOrder[i]] = static_cast<u8>(bits(3));
        Huffman lencode;
        build(lencode, lengths, 19);
        std::fill(std::begin(lengths), std::end(lengths), static_cast<u8>(0));
        for (u32 i = 0; i < nlen + ndist;) {
            const int sym = decode(lencode);
            if (sym < 0) return false;
            if (sym < 16) {
                lengths[i++] = static_cast<u8>(sym);
                continue;
            }
            u32 repeat = 0;
            u8 value = 0;
            if (sym == 16) {
                if (!i) return false;
                value = lengths[i - 1];
                repeat = 3 + bits(2);
            } else if (sym == 17) {
                repeat = 3 + bits(3);
            } else {
                repeat = 11 + bits(7);
            }
            if (bad_ || i + repeat > nlen + ndist) return false;
            while (repeat--) lengths[i++] = value;
        }
        build(lit_, lengths, nlen);
        build(dist_, lengths + nlen, ndist);
        return true;
    }

    bool codes() {
        static constexpr u16 kLenBase[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                             31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
        static constexpr u8 kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                             2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
        static constexpr u16 kDistBase[30] = {1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
                                              193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
        static constexpr u8 kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                              6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
        for (;;) {
            const int sym = decode(lit_);
            if (sym < 0) return false;
            if (sym < 256) {
                out_.push_back(static_cast<u8>(sym));
            } else if (sym == 256) {
                return true;
            } else {
                const u32 li = static_cast<u32>(sym - 257);
                if (li >= 29) return false;
                const u32 len = kLenBase[li] + bits(kLenExtra[li]);
                const int ds = decode(dist_);
                if (ds < 0 || ds >= 30) return false;
                const u32 dist = kDistBase[ds] + bits(kDistExtra[ds]);
                if (bad_ || dist > out_.size()) return false;
                const size_t from = out_.size() - dist;
                for (u32 k = 0; k < len; ++k) out_.push_back(out_[from + k]);
            }
        }
    }

    const u8* p_;
    size_t n_, at_ = 0;
    u32 buf_ = 0, have_ = 0;
    bool bad_ = false;
    Bytes& out_;
    Huffman lit_, dist_;
};

u32 be32(const u8* p) {
    return (static_cast<u32>(p[0]) << 24) | (static_cast<u32>(p[1]) << 16) | (static_cast<u32>(p[2]) << 8) | p[3];
}

bool fail(std::string* error, const char* why) {
    if (error) *error = why;
    return false;
}
}

bool decode_rgba(ByteView png, Bytes& rgba, u32& width, u32& height, std::string* error) {
    static constexpr u8 kSig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (png.size() < 8 || !std::equal(kSig, kSig + 8, png.data())) return fail(error, "not a PNG");
    u32 depth = 0, type = 0;
    Bytes idat, palette, trns;
    width = height = 0;
    for (size_t at = 8; at + 12 <= png.size();) {
        const u32 len = be32(png.data() + at);
        const u8* tag = png.data() + at + 4;
        const u8* body = tag + 4;
        if (at + 12 + static_cast<size_t>(len) > png.size()) return fail(error, "truncated chunk");
        if (!std::memcmp(tag, "IHDR", 4)) {
            if (len < 13) return fail(error, "bad IHDR");
            width = be32(body);
            height = be32(body + 4);
            depth = body[8];
            type = body[9];
            if (body[10] || body[11]) return fail(error, "unknown compression or filter");
            if (body[12]) return fail(error, "interlaced PNGs are not read");
        } else if (!std::memcmp(tag, "PLTE", 4)) {
            palette.assign(body, body + len);
        } else if (!std::memcmp(tag, "tRNS", 4)) {
            trns.assign(body, body + len);
        } else if (!std::memcmp(tag, "IDAT", 4)) {
            idat.insert(idat.end(), body, body + len);
        } else if (!std::memcmp(tag, "IEND", 4)) {
            break;
        }
        at += 12 + static_cast<size_t>(len);
    }
    if (!width || !height || width > 16384 || height > 16384) return fail(error, "bad size");
    u32 channels = 0;
    switch (type) {
    case 0: channels = 1; break;
    case 2: channels = 3; break;
    case 3: channels = 1; break;
    case 4: channels = 2; break;
    case 6: channels = 4; break;
    default: return fail(error, "unknown colour type");
    }
    if (type == 3 ? (depth != 8) : (depth != 8 && depth != 16)) return fail(error, "bit depth not read here");
    if (idat.size() < 6) return fail(error, "no image data");
    const size_t bpp = channels * (depth / 8), stride = static_cast<size_t>(width) * bpp;
    Bytes raw;
    raw.reserve((stride + 1) * height);
    Inflater inf(idat.data() + 2, idat.size() - 2, raw);
    if (!inf.run() || raw.size() < (stride + 1) * height) return fail(error, "bad compressed data");
    Bytes img(stride * height);
    for (u32 y = 0; y < height; ++y) {
        const u8 filter = raw[y * (stride + 1)];
        const u8* src = &raw[y * (stride + 1) + 1];
        u8* row = &img[y * stride];
        const u8* up = y ? &img[(y - 1) * stride] : nullptr;
        for (size_t i = 0; i < stride; ++i) {
            const int a = i >= bpp ? row[i - bpp] : 0, b = up ? up[i] : 0, c = (up && i >= bpp) ? up[i - bpp] : 0;
            int v = src[i];
            switch (filter) {
            case 0: break;
            case 1: v += a; break;
            case 2: v += b; break;
            case 3: v += (a + b) >> 1; break;
            case 4: {
                const int pp = a + b - c, pa = std::abs(pp - a), pb = std::abs(pp - b), pc = std::abs(pp - c);
                v += (pa <= pb && pa <= pc) ? a : pb <= pc ? b : c;
                break;
            }
            default: return fail(error, "bad filter");
            }
            row[i] = static_cast<u8>(v);
        }
    }
    rgba.resize(static_cast<size_t>(width) * height * 4);
    const size_t step = depth / 8;
    for (size_t px = 0; px < static_cast<size_t>(width) * height; ++px) {
        const u8* s = &img[px * bpp];
        u8* o = &rgba[px * 4];
        switch (type) {
        case 0: o[0] = o[1] = o[2] = s[0]; o[3] = 255; break;
        case 2: o[0] = s[0]; o[1] = s[step]; o[2] = s[2 * step]; o[3] = 255; break;
        case 3: {
            const u32 i = s[0];
            if (3 * i + 2 >= palette.size()) return fail(error, "palette index out of range");
            o[0] = palette[3 * i]; o[1] = palette[3 * i + 1]; o[2] = palette[3 * i + 2];
            o[3] = i < trns.size() ? trns[i] : 255;
            break;
        }
        case 4: o[0] = o[1] = o[2] = s[0]; o[3] = s[step]; break;
        default: o[0] = s[0]; o[1] = s[step]; o[2] = s[2 * step]; o[3] = s[3 * step]; break;
        }
    }
    return true;
}

bool read_rgba(const std::string& path, Bytes& rgba, u32& width, u32& height, std::string* error) {
    const auto bytes = file::read_all(path);
    if (!bytes) return fail(error, "cannot read the file");
    return decode_rgba(*bytes, rgba, width, height, error);
}
}
}

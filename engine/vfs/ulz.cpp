#include "vfs/ulz.hpp"

namespace ac3::vfs::ulz {
const char* error_name(Error e) {
    switch (e) {
    case Error::none: return "none";
    case Error::not_ulz: return "not ULZ";
    case Error::bad_type: return "unknown type";
    case Error::bad_window: return "window over 15 bits";
    case Error::truncated: return "a stream runs past the end of the data";
    case Error::bad_reference: return "back-reference before the start of the output";
    case Error::overrun: return "back-reference longer than the bytes left";
    }
    return "?";
}

bool is_ulz(ByteView in) {
    return in.size() > 20 && le32(in.data()) == 0x1A7A6C55u;
}

u32 decoded_size(ByteView in) { return is_ulz(in) ? (le32(in.data() + 4) & 0xFFFFFFu) : 0; }

Error decode(ByteView in, Bytes& out) {
    out.clear();
    if (!is_ulz(in)) return Error::not_ulz;
    const u8* d = in.data();
    const size_t n = in.size();
    const u32 type = le32(d + 4) >> 24;
    const u32 size = le32(d + 4) & 0xFFFFFFu;
    const u32 window_bits = le32(d + 8) >> 24;
    size_t lit = le32(d + 8) & 0xFFFFFFu;
    size_t ref = le32(d + 12);
    size_t flags_at = 16;
    if (window_bits > 15) return Error::bad_window;
    const u32 offset_mask = (1u << window_bits) - 1u;

    u32 refill_when;
    switch (type) {
    case 0: refill_when = 1; break;
    case 2: refill_when = 0; break;
    default: return Error::bad_type;
    }

    out.resize(size);
    size_t o = 0;
    u32 flags = 0, mask = refill_when;
    while (o < size) {
        if (mask == refill_when) {
            if (flags_at + 4 > n) return Error::truncated;
            flags = le32(d + flags_at);
            flags_at += 4;
            mask = 0x80000000u;
        }
        if (flags & mask) {
            if (lit >= n) return Error::truncated;
            out[o++] = d[lit++];
        } else {
            if (ref + 2 > n) return Error::truncated;
            const u32 e = le16(d + ref);
            ref += 2;
            const u32 offset = e & offset_mask;
            const u32 length = 3 + (e >> window_bits);
            if (length > size - o) return Error::overrun;
            if (offset + 1 > o) return Error::bad_reference;
            size_t from = o - 1 - offset;
            for (u32 k = 0; k < length; ++k) out[o++] = out[from++];
        }
        mask >>= 1;
    }
    return Error::none;
}
}

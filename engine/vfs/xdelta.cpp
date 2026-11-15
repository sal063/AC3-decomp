#include "vfs/xdelta.hpp"

#include <cstring>
#include <memory>

namespace ac3::vfs {
namespace {
enum : u8 { kNoop, kAdd, kRun, kCopy };
struct Half {
    u8 type = kNoop, size = 0, mode = 0;
};
struct Code {
    Half h[2];
};

const Code* code_table() {
    static Code table[256];
    static bool ready = false;
    if (ready) return table;
    int n = 0;
    table[n++].h[0] = {kRun, 0, 0};
    table[n++].h[0] = {kAdd, 0, 0};
    for (u8 s = 1; s <= 17; s++) table[n++].h[0] = {kAdd, s, 0};
    for (u8 m = 0; m < 9; m++) {
        table[n++].h[0] = {kCopy, 0, m};
        for (u8 s = 4; s <= 18; s++) table[n++].h[0] = {kCopy, s, m};
    }
    for (u8 m = 0; m < 6; m++)
        for (u8 a = 1; a <= 4; a++)
            for (u8 c = 4; c <= 6; c++) {
                table[n].h[0] = {kAdd, a, 0};
                table[n++].h[1] = {kCopy, c, m};
            }
    for (u8 m = 6; m < 9; m++)
        for (u8 a = 1; a <= 4; a++) {
            table[n].h[0] = {kAdd, a, 0};
            table[n++].h[1] = {kCopy, 4, m};
        }
    for (u8 m = 0; m < 9; m++) {
        table[n].h[0] = {kCopy, 4, m};
        table[n++].h[1] = {kAdd, 1, 0};
    }
    ready = true;
    return table;
}

bool varint(const u8* b, size_t n, size_t& p, u64& v) {
    v = 0;
    for (int i = 0; i < 10; i++) {
        if (p >= n) return false;
        const u8 c = b[p++];
        v = (v << 7) | (c & 0x7Fu);
        if (!(c & 0x80)) return true;
    }
    return false;
}

constexpr u32 kBits = 11, kTotal = 1u << kBits, kMove = 5, kStates = 12, kPos = 16, kLit = 0x300u * 16u;

struct Range {
    const u8* in;
    size_t pos, end;
    u32 range = 0, code = 0;
    bool bad = false;

    bool init() {
        if (end - pos < 5 || in[pos] != 0) return false;
        range = 0xFFFFFFFFu;
        code = 0;
        for (int i = 1; i < 5; i++) code = (code << 8) | in[pos + i];
        pos += 5;
        bad = false;
        return code != range;
    }
    void norm() {
        if (range < (1u << 24)) {
            range <<= 8;
            if (pos < end) code = (code << 8) | in[pos++];
            else {
                code <<= 8;
                bad = true;
            }
        }
    }
    u32 bit(u16& p) {
        const u32 bound = (range >> kBits) * p;
        u32 b;
        if (code < bound) {
            range = bound;
            p = static_cast<u16>(p + ((kTotal - p) >> kMove));
            b = 0;
        } else {
            range -= bound;
            code -= bound;
            p = static_cast<u16>(p - (p >> kMove));
            b = 1;
        }
        norm();
        return b;
    }
    u32 direct(u32 n) {
        u32 v = 0;
        while (n--) {
            range >>= 1;
            const u32 b = code >= range;
            if (b) code -= range;
            v = (v << 1) | b;
            norm();
        }
        return v;
    }
    u32 tree(u16* p, u32 bits) {
        u32 m = 1;
        for (u32 i = 0; i < bits; i++) m = (m << 1) | bit(p[m]);
        return m - (1u << bits);
    }
    u32 rtree(u16* p, u32 bits) {
        u32 m = 1, v = 0;
        for (u32 i = 0; i < bits; i++) {
            const u32 b = bit(p[m]);
            m = (m << 1) | b;
            v |= b << i;
        }
        return v;
    }
};

struct Lengths {
    u16 choice, choice2;
    u16 low[kPos][8];
    u16 mid[kPos][8];
    u16 high[256];
};

struct Lzma {
    u32 lc = 0, lp = 0, pb = 0;
    u32 state = 0, rep[4] = {};
    u16 lit[kLit];
    u16 is_match[kStates][kPos];
    u16 is_rep[kStates], is_rep0[kStates], is_rep1[kStates], is_rep2[kStates];
    u16 is_rep0_long[kStates][kPos];
    u16 slot[4][64];
    u16 spec[115];
    u16 align[16];
    Lengths len, rep_len;

    static void fill(u16* p, size_t n) {
        for (size_t i = 0; i < n; i++) p[i] = static_cast<u16>(kTotal / 2);
    }
    void reset() {
        state = 0;
        rep[0] = rep[1] = rep[2] = rep[3] = 0;
        fill(lit, kLit);
        fill(&is_match[0][0], kStates * kPos);
        fill(is_rep, kStates);
        fill(is_rep0, kStates);
        fill(is_rep1, kStates);
        fill(is_rep2, kStates);
        fill(&is_rep0_long[0][0], kStates * kPos);
        fill(&slot[0][0], 4 * 64);
        fill(spec, 115);
        fill(align, 16);
        fill(&len.choice, sizeof len / sizeof(u16));
        fill(&rep_len.choice, sizeof rep_len / sizeof(u16));
    }
    static u32 length(Range& rc, Lengths& l, u32 ps) {
        if (!rc.bit(l.choice)) return rc.tree(l.low[ps], 3);
        if (!rc.bit(l.choice2)) return 8 + rc.tree(l.mid[ps], 3);
        return 16 + rc.tree(l.high, 8);
    }
    u32 distance(Range& rc, u32 n) {
        const u32 ls = n < 3 ? n : 3;
        const u32 s = rc.tree(slot[ls], 6);
        if (s < 4) return s;
        const u32 nd = (s >> 1) - 1;
        u32 d = (2u | (s & 1u)) << nd;
        if (s < 14) return d + rc.rtree(spec + d - s, nd);
        d += rc.direct(nd - 4) << 4;
        return d + rc.rtree(align, 4);
    }

    bool chunk(Range& rc, u8* out, size_t& op, size_t lim, size_t base) {
        size_t o = op;
        const u32 pbm = (1u << pb) - 1, lpm = (1u << lp) - 1;
        while (o < lim) {
            const u32 pos = static_cast<u32>(o - base);
            const u32 ps = pos & pbm;
            const u32 st = state;
            if (!rc.bit(is_match[st][ps])) {
                const u32 prev = o > base ? out[o - 1] : 0;
                u16* p = lit + 0x300u * (((pos & lpm) << lc) + (prev >> (8 - lc)));
                u32 sym = 1;
                if (st >= 7) {
                    if (static_cast<size_t>(rep[0]) + 1 > o - base) return false;
                    u32 mb = out[o - rep[0] - 1];
                    do {
                        const u32 mbit = (mb >> 7) & 1;
                        mb <<= 1;
                        const u32 b = rc.bit(p[((1 + mbit) << 8) + sym]);
                        sym = (sym << 1) | b;
                        if (mbit != b) break;
                    } while (sym < 0x100);
                }
                while (sym < 0x100) sym = (sym << 1) | rc.bit(p[sym]);
                out[o++] = static_cast<u8>(sym - 0x100);
                state = st < 4 ? 0 : st < 10 ? st - 3 : st - 6;
                continue;
            }
            u32 n;
            if (!rc.bit(is_rep[st])) {
                rep[3] = rep[2];
                rep[2] = rep[1];
                rep[1] = rep[0];
                n = length(rc, len, ps);
                state = st < 7 ? 7 : 10;
                rep[0] = distance(rc, n);
                if (rep[0] == 0xFFFFFFFFu) return false;
            } else {
                if (o == base) return false;
                if (!rc.bit(is_rep0[st])) {
                    if (!rc.bit(is_rep0_long[st][ps])) {
                        state = st < 7 ? 9 : 11;
                        if (static_cast<size_t>(rep[0]) + 1 > o - base) return false;
                        out[o] = out[o - rep[0] - 1];
                        o++;
                        continue;
                    }
                } else {
                    u32 d;
                    if (!rc.bit(is_rep1[st])) {
                        d = rep[1];
                    } else {
                        if (!rc.bit(is_rep2[st])) {
                            d = rep[2];
                        } else {
                            d = rep[3];
                            rep[3] = rep[2];
                        }
                        rep[2] = rep[1];
                    }
                    rep[1] = rep[0];
                    rep[0] = d;
                }
                n = length(rc, rep_len, ps);
                state = st < 7 ? 8 : 11;
            }
            n += 2;
            const size_t dist = static_cast<size_t>(rep[0]) + 1;
            if (dist > o - base || o + n > lim) return false;
            u8* dst = out + o;
            const u8* src = dst - dist;
            for (u32 i = 0; i < n; i++) dst[i] = src[i];
            o += n;
            if (rc.bad) return false;
        }
        op = o;
        return !rc.bad;
    }
};

int lzma2_block(const u8* in, size_t len, size_t& ip, u8* out, size_t out_len, size_t& op, Lzma& z, size_t& base,
                bool& have_props) {
    size_t i = ip, o = op;
    while (i < len) {
        const u32 c = in[i++];
        if (c == 0x00) {
            ip = i;
            op = o;
            return 2;
        }
        if (c == 0x01 || c == 0x02) {
            if (i + 2 > len) return 0;
            const size_t n = ((static_cast<size_t>(in[i]) << 8) | in[i + 1]) + 1;
            i += 2;
            if (i + n > len || o + n > out_len) return 0;
            if (c == 0x01) base = o;
            std::memcpy(out + o, in + i, n);
            i += n;
            o += n;
            continue;
        }
        if (c < 0x80 || i + 4 > len) return 0;
        const size_t un = (static_cast<size_t>(c & 0x1Fu) << 16) + (static_cast<size_t>(in[i]) << 8) + in[i + 1] + 1;
        const size_t cn = (static_cast<size_t>(in[i + 2]) << 8) + in[i + 3] + 1;
        i += 4;
        const u32 mode = (c >> 5) & 3u;
        if (mode == 3) base = o;
        if (mode >= 2) {
            if (i >= len) return 0;
            u32 pr = in[i++];
            if (pr >= 9 * 5 * 5) return 0;
            z.lc = pr % 9;
            pr /= 9;
            z.lp = pr % 5;
            z.pb = pr / 5;
            if (z.lc + z.lp > 4) return 0;
            have_props = true;
        }
        if (!have_props) return 0;
        if (mode >= 1) z.reset();
        if (i + cn > len || o + un > out_len) return 0;
        Range rc{in, i, i + cn};
        if (!rc.init()) return 0;
        if (!z.chunk(rc, out, o, o + un, base)) return 0;
        i += cn;
    }
    ip = i;
    op = o;
    return 1;
}

bool xz_varint(const u8* p, size_t n, size_t& k, u64& v) {
    v = 0;
    for (int s = 0; s < 63; s += 7) {
        if (k >= n) return false;
        const u8 b = p[k++];
        v |= static_cast<u64>(b & 0x7F) << s;
        if (!(b & 0x80)) return true;
    }
    return false;
}
}

u32 adler32(const u8* p, size_t n, u32 sum) {
    u32 a = sum & 0xFFFFu, b = sum >> 16;
    while (n) {
        size_t k = n < 5552 ? n : 5552;
        n -= k;
        while (k--) {
            a += *p++;
            b += a;
        }
        a %= 65521u;
        b %= 65521u;
    }
    return (b << 16) | a;
}

bool xz_decode(ByteView input, u8* out, size_t out_len, size_t& produced) {
    static const u8 magic[6] = {0xFD, '7', 'z', 'X', 'Z', 0x00};
    const u8* in = input.data();
    const size_t in_len = input.size();
    produced = 0;
    if (in_len < 12 || std::memcmp(in, magic, 6) != 0 || in[6] != 0 || (in[7] & 0xF0)) return false;
    const u32 check = in[7] & 0x0Fu;
    const size_t check_size = check ? static_cast<size_t>(4) << ((check - 1) / 3) : 0;
    const auto z = std::make_unique<Lzma>();
    size_t i = 12, o = 0, base = 0;
    bool have_props = false, ok = true;
    while (i < in_len) {
        if (in[i] == 0) break;
        const size_t block = i;
        const size_t hs = (static_cast<size_t>(in[i]) + 1) * 4;
        if (i + hs > in_len) {
            ok = false;
            break;
        }
        const u8* h = in + i;
        const u32 flags = h[1];
        if (flags & 0x3C) {
            ok = false;
            break;
        }
        size_t k = 2;
        u64 v, id, ps;
        if (((flags & 0x40) && !xz_varint(h, hs - 4, k, v)) || ((flags & 0x80) && !xz_varint(h, hs - 4, k, v)) ||
            (flags & 3) != 0 || !xz_varint(h, hs - 4, k, id) || !xz_varint(h, hs - 4, k, ps) || id != 0x21 || ps != 1 ||
            k + 1 > hs - 4) {
            ok = false;
            break;
        }
        i += hs;
        const int r = lzma2_block(in, in_len, i, out, out_len, o, *z, base, have_props);
        if (r == 0) {
            ok = false;
            break;
        }
        if (r == 1) break;
        while ((i - block) & 3) i++;
        i += check_size;
    }
    produced = o;
    return ok;
}

bool Xdelta::parse(ByteView patch) {
    windows_.clear();
    for (Bytes& s : sections_) s.clear();
    target_size_ = 0;
    largest_ = 0;
    const u8* b = patch.data();
    const size_t n = patch.size();
    if (n < 5 || b[0] != 0xD6 || b[1] != 0xC3 || b[2] != 0xC4 || b[3] != 0) return false;
    const u8 hi = b[4];
    size_t p = 5;
    if (hi & 1) {
        if (p >= n || b[p] != 2) return false;
        p++;
    }
    if (hi & 2) return false;
    if (hi & 4) {
        u64 len;
        if (!varint(b, n, p, len) || len > n - p) return false;
        p += static_cast<size_t>(len);
    }
    struct Raw {
        const u8* raw;
        size_t len;
        bool compressed;
    };
    std::vector<Raw> raw;
    while (p < n) {
        XdeltaWindow w;
        const u8 wi = b[p++];
        if ((wi & 0xF8) || (wi & 2)) return false;
        u64 dlen, tlen, lens[3];
        if ((wi & 1) && (!varint(b, n, p, w.src_len) || !varint(b, n, p, w.src_pos))) return false;
        if (!varint(b, n, p, dlen) || dlen > n - p) return false;
        const size_t start = p;
        if (!varint(b, n, p, tlen) || tlen > 0x7FFFFFFFu || p >= n) return false;
        const u8 di = b[p++];
        for (u64& len : lens)
            if (!varint(b, n, p, len)) return false;
        if (wi & 4) {
            if (p + 4 > n) return false;
            w.has_adler = true;
            w.adler = static_cast<u32>(b[p]) << 24 | static_cast<u32>(b[p + 1]) << 16 | static_cast<u32>(b[p + 2]) << 8 | b[p + 3];
            p += 4;
        }
        for (int k = 0; k < 3; k++) {
            if (lens[k] > n - p) return false;
            raw.push_back({b + p, static_cast<size_t>(lens[k]), ((di >> k) & 1) != 0});
            p += static_cast<size_t>(lens[k]);
        }
        if (p != start + dlen) return false;
        w.target_len = static_cast<u32>(tlen);
        w.target_pos = target_size_;
        target_size_ += tlen;
        if (w.target_len > largest_) largest_ = w.target_len;
        windows_.push_back(w);
    }
    for (int k = 0; k < 3; k++) {
        Bytes packed;
        size_t unpacked = 0;
        for (size_t i = 0; i < windows_.size(); i++) {
            const Raw& r = raw[3 * i + static_cast<size_t>(k)];
            if (!r.compressed) continue;
            size_t q = 0;
            u64 size;
            if (!varint(r.raw, r.len, q, size)) return false;
            packed.insert(packed.end(), r.raw + q, r.raw + r.len);
            unpacked += static_cast<size_t>(size);
        }
        if (!packed.empty()) {
            sections_[k].resize(unpacked ? unpacked : 1);
            size_t got = 0;
            if (!xz_decode(packed, sections_[k].data(), unpacked, got) || got != unpacked) return false;
        }
        size_t d = 0;
        for (size_t i = 0; i < windows_.size(); i++) {
            const Raw& r = raw[3 * i + static_cast<size_t>(k)];
            const u8* ptr = r.raw;
            size_t len = r.len;
            if (r.compressed) {
                size_t q = 0;
                u64 size;
                varint(r.raw, r.len, q, size);
                ptr = sections_[k].data() + d;
                len = static_cast<size_t>(size);
                d += len;
            }
            XdeltaWindow& w = windows_[i];
            if (k == 0) {
                w.data = ptr;
                w.data_len = len;
            } else if (k == 1) {
                w.inst = ptr;
                w.inst_len = len;
            } else {
                w.addr = ptr;
                w.addr_len = len;
            }
        }
    }
    return true;
}

bool Xdelta::apply(const XdeltaWindow& w, const u8* src, u8* t, const Copied& copied) {
    const Code* table = code_table();
    u64 recent[4] = {0, 0, 0, 0};
    std::vector<u64> same(3 * 256, 0);
    int slot = 0;
    size_t dp = 0, ip = 0, ap = 0;
    u64 pos = 0;
    const u64 S = w.src_len;
    while (ip < w.inst_len) {
        const Code& c = table[w.inst[ip++]];
        for (const Half& h : c.h) {
            if (h.type == kNoop) continue;
            u64 size = h.size;
            if (!size && !varint(w.inst, w.inst_len, ip, size)) return false;
            if (pos + size > w.target_len) return false;
            if (h.type == kAdd) {
                if (dp + size > w.data_len) return false;
                std::memcpy(t + pos, w.data + dp, static_cast<size_t>(size));
                dp += static_cast<size_t>(size);
            } else if (h.type == kRun) {
                if (dp >= w.data_len) return false;
                std::memset(t + pos, w.data[dp++], static_cast<size_t>(size));
            } else {
                const u64 here = S + pos;
                u64 a, v;
                const u8 m = h.mode;
                if (m == 0) {
                    if (!varint(w.addr, w.addr_len, ap, a)) return false;
                } else if (m == 1) {
                    if (!varint(w.addr, w.addr_len, ap, v) || v > here) return false;
                    a = here - v;
                } else if (m < 6) {
                    if (!varint(w.addr, w.addr_len, ap, v)) return false;
                    a = recent[m - 2] + v;
                } else {
                    if (ap >= w.addr_len) return false;
                    a = same[static_cast<size_t>(m - 6) * 256 + w.addr[ap++]];
                }
                recent[slot] = a;
                slot = (slot + 1) & 3;
                same[a % (3 * 256)] = a;
                if (a >= here) return false;
                u64 left = size, to = pos;
                if (a < S) {
                    const u64 k = S - a < left ? S - a : left;
                    std::memcpy(t + to, src + a, static_cast<size_t>(k));
                    if (copied) copied(w.target_pos + to, static_cast<u32>(k), w.src_pos + a);
                    to += k;
                    left -= k;
                    a += k;
                }
                if (left) {
                    const u64 ta = a - S;
                    u8* dst = t + to;
                    const u8* from = t + ta;
                    if (ta + left <= to) std::memcpy(dst, from, static_cast<size_t>(left));
                    else
                        for (u64 i = 0; i < left; i++) dst[i] = from[i];
                }
            }
            pos += size;
        }
    }
    return pos == w.target_len && dp == w.data_len && ap == w.addr_len;
}
}

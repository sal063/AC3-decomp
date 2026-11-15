#include "machine/mdec.hpp"

#include "sim/memory.hpp"

#include <cmath>
#include <cstring>

namespace ac3::machine {
namespace {
constexpr u8 kZigzag[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,  12, 19, 26, 33, 40, 48,
    41, 34, 27, 20, 13, 6,  7,  14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23,
    30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
};

constexpr u8 kQuant[64] = {
    2,  16, 19, 22, 26, 27, 29, 34, 16, 16, 22, 24, 27, 29, 34, 37, 19, 22, 26, 27, 29, 34,
    34, 38, 22, 22, 26, 27, 29, 34, 37, 40, 22, 26, 27, 29, 32, 35, 40, 48, 26, 27, 29, 32,
    35, 40, 48, 58, 26, 27, 29, 34, 38, 46, 56, 69, 27, 29, 35, 38, 46, 56, 69, 83,
};

constexpr u32 kEob = 0xFE00u;
constexpr u32 kMaxCoefBytes = 512 * 1024;

double g_cos[8][8];
bool g_cos_ready = false;

void idct_init() {
    for (int x = 0; x < 8; x++)
        for (int u = 0; u < 8; u++)
            g_cos[x][u] = std::cos((2.0 * x + 1.0) * u * 3.14159265358979 / 16.0) *
                          (u == 0 ? 0.353553390593274 : 0.5);
    g_cos_ready = true;
}

void idct_8x8(const s16* in, s16* out) {
    double tmp[64];
    if (!g_cos_ready) idct_init();
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            double s = 0;
            for (int u = 0; u < 8; u++) s += g_cos[x][u] * in[y * 8 + u];
            tmp[y * 8 + x] = s;
        }
    for (int x = 0; x < 8; x++)
        for (int y = 0; y < 8; y++) {
            double s = 0;
            for (int v = 0; v < 8; v++) s += g_cos[y][v] * tmp[v * 8 + x];
            const int r = static_cast<int>(s + (s < 0 ? -0.5 : 0.5));
            out[y * 8 + x] = static_cast<s16>(r < -256 ? -256 : (r > 255 ? 255 : r));
        }
}

void yuv_to_rgb(int y, int cb, int cr, u8* r, u8* g, u8* b) {
    int rr = y + ((91881 * cr) >> 16);
    int gg = y - ((22554 * cb + 46802 * cr) >> 16);
    int bb = y + ((116130 * cb) >> 16);
    rr += 128;
    gg += 128;
    bb += 128;
    *r = static_cast<u8>(rr < 0 ? 0 : (rr > 255 ? 255 : rr));
    *g = static_cast<u8>(gg < 0 ? 0 : (gg > 255 ? 255 : gg));
    *b = static_cast<u8>(bb < 0 ? 0 : (bb > 255 ? 255 : bb));
}
}

Mdec::Mdec(sim::Memory& mem) : m(mem) { std::memcpy(quant_, kQuant, sizeof quant_); }

u32 Mdec::reset() {
    coef_buf_ = 0;
    coef_pos_ = coef_end_ = 0;
    return 0;
}

bool Mdec::block_from_stream(u32* addr, u32 end, s16* out) {
    s16 coef[64];
    std::memset(coef, 0, sizeof coef);
    if (*addr + 2 > end) return false;
    const u32 head = m.r16(*addr);
    *addr += 2;
    const int q_scale = static_cast<int>((head >> 10) & 0x3F);
    int dc = static_cast<int>(head & 0x3FF);
    if (dc & 0x200) dc -= 0x400;
    coef[0] = static_cast<s16>(dc * quant_[0]);
    int idx = 0;
    while (*addr + 2 <= end) {
        const u32 w = m.r16(*addr);
        *addr += 2;
        if (w == kEob) break;
        const int run = static_cast<int>((w >> 10) & 0x3F);
        int level = static_cast<int>(w & 0x3FF);
        if (level & 0x200) level -= 0x400;
        idx += run + 1;
        if (idx > 63) break;
        coef[kZigzag[idx]] = static_cast<s16>(level * quant_[kZigzag[idx]] * q_scale / 8);
    }
    idct_8x8(coef, out);
    return true;
}

u32 Mdec::in(u32 buf, u32 mode) {
    if (!buf) return 0;
    u32 cmd = m.r32(buf);
    u32 words = cmd & 0xFFFFu;
    if (words > kMaxCoefBytes / 4) words = kMaxCoefBytes / 4;
    cmd = (mode & 1u) ? (cmd & ~0x08000000u) : (cmd | 0x08000000u);
    cmd = (mode & 2u) ? (cmd | 0x02000000u) : (cmd & ~0x02000000u);
    m.w32(buf, cmd);
    coef_buf_ = buf;
    coef_pos_ = buf + 4;
    coef_end_ = buf + 4 + words * 4;
    out_24bit_ = ((cmd >> 27) & 3u) == 2u;
    out_mask15_ = ((cmd >> 25) & 1u) != 0u;
    return 0;
}

u32 Mdec::in_callback(u32 fn) {
    const u32 prev = in_callback_;
    in_callback_ = fn;
    return prev;
}

u32 Mdec::out_callback(u32 fn) {
    const u32 prev = out_callback_;
    out_callback_ = fn;
    return prev;
}

u32 Mdec::put_env(u32 env) {
    if (env)
        for (u32 i = 0; i < 64; i++) quant_[i] = static_cast<u8>(m.r8(env + i));
    return 0;
}

u32 Mdec::out(u32 dst, u32 words) {
    if (!coef_buf_ || !dst || !words) return 0;
    const u32 bytes = words * 4;
    u32 pos = coef_pos_;
    const u32 end = coef_end_;
    const u32 mb_bytes = 16 * 16 * (out_24bit_ ? 3 : 2);
    const u32 mb_count = bytes / mb_bytes;
    s16 blk[6][64];
    u32 done = 0;
    for (u32 mb = 0; mb < mb_count; mb++) {
        for (int i = 0; i < 6; i++)
            if (!block_from_stream(&pos, end, blk[i])) {
                coef_pos_ = pos;
                return done;
            }
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                const int lb = (y >= 8 ? 2 : 0) + (x >= 8 ? 1 : 0);
                const int Y = blk[2 + lb][(y & 7) * 8 + (x & 7)];
                const int cb = blk[1][(y >> 1) * 8 + (x >> 1)];
                const int cr = blk[0][(y >> 1) * 8 + (x >> 1)];
                u8 r, g, b;
                yuv_to_rgb(Y, cb, cr, &r, &g, &b);
                const u32 px = mb * 256 + static_cast<u32>(y * 16 + x);
                if (out_24bit_) {
                    m.w8(dst + px * 3 + 0, r);
                    m.w8(dst + px * 3 + 1, g);
                    m.w8(dst + px * 3 + 2, b);
                } else {
                    m.w16(dst + px * 2, (out_mask15_ ? 0x8000u : 0u) | (static_cast<u32>(b >> 3) << 10) |
                                            (static_cast<u32>(g >> 3) << 5) | static_cast<u32>(r >> 3));
                }
            }
        done++;
    }
    coef_pos_ = pos;
    if (out_callback_ && call_guest) call_guest(out_callback_, 1);
    return done;
}
}

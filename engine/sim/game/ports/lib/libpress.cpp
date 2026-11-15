#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kVlcSize = 0x800BBC04u;
constexpr u32 kVlcState = 0x800BBC08u;
constexpr u32 kVlcEnd = 0x800BBC2Cu;
constexpr u32 kVlcPacked = 0x800BBC34u;

constexpr u32 kDma0Madr = 0x800BBBC4u, kDma0Bcr = 0x800BBBC8u, kDma0Chcr = 0x800BBBCCu;
constexpr u32 kDma1Madr = 0x800BBBD0u, kDma1Bcr = 0x800BBBD4u, kDma1Chcr = 0x800BBBD8u;
constexpr u32 kMdecData = 0x800BBBF4u;
constexpr u32 kMdecCtrl = 0x800BBBF8u;
constexpr u32 kDmaDpcr = 0x800BBBFCu;
constexpr u32 kQuantTable = 0x800BBAB4u, kScaleTable = 0x800BBB38u;
constexpr u32 kMdecReset = 0x80000000u, kMdecEnable = 0x60000000u;
constexpr u32 kMdecInBusy = 0x20000000u;
constexpr u32 kDmaBusy = 0x01000000u;
constexpr u32 kSyncPolls = 0x100000u;

constexpr u32 kAll = 0xFFFFFFu;
constexpr u32 kEndOfBlock = 0xFE00u;
constexpr u32 kEscape = 0x7C1Fu;
constexpr u32 kEndV2 = 0x1FFu, kEndV3 = 0x3FFu;

constexpr u32 kLumaDc = 0x000u;
constexpr u32 kChromaDc = 0x400u;
constexpr u32 kAc = 0x800u;
constexpr u32 kAcLong = 0x10800u;
constexpr u32 kTableHalfwords = 0x8800u;

struct Vlc {
    u32 src;
    u32 dst;
    u32 bits;
    u32 fill;
    u32 qscale;
    u32 block;
    u32 dc[3];
};
}

u32 Game::mdec_timeout(u32  ) {
    cover(0x800ACEA4u);
    Frame f(*this, 0x18);
    m.w32(m.r32(kMdecCtrl), kMdecReset);
    m.w32(m.r32(kDma0Chcr), 0);
    m.w32(m.r32(kDma1Chcr), 0);
    (void)m.r32(m.r32(kDma1Chcr));
    m.w32(m.r32(kMdecCtrl), kMdecEnable);
    return 0;
}

s32 Game::mdec_wait(u32 reg_ptr, u32 busy, u32 name) {
    Frame f(*this, 0x20);
    const u32 polls = f.at(0x10);
    m.w32(polls, kSyncPolls);
    if (!(m.r32(m.r32(reg_ptr)) & busy)) return 0;
    do {
        m.w32(polls, m.r32(polls) - 1);
        if (m.r32(polls) == 0xFFFFFFFFu) {
            mdec_timeout(name);
            return -1;
        }
    } while (m.r32(m.r32(reg_ptr)) & busy);
    return 0;
}

s32 Game::mdec_in_sync() {
    cover(0x800ACD64u);
    return mdec_wait(kMdecCtrl, kMdecInBusy, 0x80025124u);
}

s32 Game::mdec_out_sync() {
    cover(0x800ACDF8u);
    return mdec_wait(kDma1Chcr, kDmaBusy, 0x80025134u);
}

u32 Game::mdec_status() {
    cover(0x800ACE8Cu);
    return m.r32(m.r32(kMdecCtrl));
}

u32 Game::mdec_in(u32 buf, u32 words) {
    cover(0x800ACC48u);
    Frame f(*this, 0x20);
    mdec_in_sync();
    const u32 dpcr = m.r32(kDmaDpcr);
    m.w32(dpcr, m.r32(dpcr) | 0x88u);
    m.w32(m.r32(kDma0Madr), buf + 4);
    m.w32(m.r32(kDma0Bcr), ((words >> 5) << 16) | 0x20u);
    m.w32(m.r32(kMdecData), m.r32(buf));
    const u32 chcr = m.r32(kDma0Chcr);
    m.w32(chcr, 0x01000201u);
    return chcr;
}

u32 Game::mdec_out(u32 buf, u32 words) {
    cover(0x800ACCD8u);
    Frame f(*this, 0x20);
    mdec_out_sync();
    const u32 dpcr = m.r32(kDmaDpcr);
    m.w32(dpcr, m.r32(dpcr) | 0x88u);
    m.w32(m.r32(kDma1Chcr), 0);
    m.w32(m.r32(kDma1Madr), buf);
    m.w32(m.r32(kDma1Bcr), ((words >> 5) << 16) | 0x20u);
    const u32 chcr = m.r32(kDma1Chcr);
    m.w32(chcr, 0x01000200u);
    return chcr;
}

u32 Game::mdec_reset(u32 mode) {
    cover(0x800ACB58u);
    Frame f(*this, 0x18);
    if (mode == 0) {
        m.w32(m.r32(kMdecCtrl), kMdecReset);
        m.w32(m.r32(kDma0Chcr), 0);
        m.w32(m.r32(kDma1Chcr), 0);
        m.w32(m.r32(kMdecCtrl), kMdecEnable);
        mdec_in(kQuantTable, 0x20);
        return mdec_in(kScaleTable, 0x20);
    }
    if (mode == 1) {
        m.w32(m.r32(kMdecCtrl), kMdecReset);
        m.w32(m.r32(kDma0Chcr), 0);
        m.w32(m.r32(kDma1Chcr), 0);
        (void)m.r32(m.r32(kDma1Chcr));
        m.w32(m.r32(kMdecCtrl), kMdecEnable);
        return kMdecEnable;
    }
    return 0;
}

u32 Game::dct_vlc_size(u32 n) {
    cover(0x800ACF28u);
    const u32 old = m.r32(kVlcSize);
    m.w32(kVlcSize, static_cast<s32>(n - 1) <= 0 ? kAll : 2 * n);
    return old;
}

void Game::dct_vlc_build(u32 table) {
    cover(0x800AD2A8u);
    constexpr u32 kEnd = 0xF00u;
    u32 src = kVlcPacked, dst = table, back = 0;
    do {
        const u32 c = m.r8(src++);
        if (c < 0xF0u) {
            for (u32 i = 0; i <= c; ++i, ++dst) m.w8(dst, back ? m.r8(dst - back) : m.r8(src++));
        } else {
            back = 0;
            if (c != 0xF0u) back = ((c << 8) | m.r8(src++)) - 0xF0FFu;
        }
    } while (back != kEnd);
    for (u32 i = 4; i < kTableHalfwords; ++i) {
        const u32 h = table + 2 * i;
        m.w16(h, m.r16(h) ^ m.r16(h - 8));
    }
}

u32 Game::dct_vlc2(u32 bs, u32 buf, u32 table) {
    cover(0x800ACF58u);
    const u32 count = m.r32(kVlcSize);
    Vlc s{};
    u32 limit;
    bool resume = bs == 0;
    if (resume) {
        s.src = m.r32(kVlcState);
        s.dst = m.r32(kVlcState + 4);
        s.bits = m.r32(kVlcState + 8);
        s.fill = m.r32(kVlcState + 12);
        s.qscale = m.r32(kVlcState + 16);
        s.block = m.r32(kVlcState + 20);
        for (u32 k = 0; k < 3; ++k) s.dc[k] = m.r32(kVlcState + 24 + 4 * k);
        limit = s.dst + 2 * count;
    } else {
        limit = buf + 2 * count;
        const u32 header = m.r32(bs);
        s.qscale = static_cast<u32>(m.r16(bs + 4)) << 10;
        const u32 version = m.r16(bs + 6);
        const u32 hi = m.r16(bs + 8), lo = m.r16(bs + 10);
        s.block = static_cast<s32>(version - 3) >= 0 ? 1u : 0u;
        s.src = bs + 12;
        s.bits = (hi << 16) | lo;
        s.fill = 0;
        m.w32(buf, header);
        m.w32(kVlcEnd, buf + 4 + 4 * (header & 0xFFFFu));
        s.dst = buf + 4;
    }

    auto use = [&](u32 n) {
        s.fill += n;
        const bool refill = (s.fill & 0x10u) != 0;
        s.fill &= 0xFu;
        if (refill) {
            s.bits |= static_cast<u32>(m.r16(s.src)) << s.fill;
            s.src += 2;
        }
    };
    auto put = [&](u32 h) {
        m.w16(s.dst, h);
        s.dst += 2;
    };

    for (;;) {
        if (!resume) {
            const u32 dc_slot = s.dst;
            const u32 code = s.bits >> 22;
            if (s.block == 0) {
                if (code == kEndV2) break;
                s.bits <<= 10;
                use(10);
                put(s.qscale | code);
            } else {
                if (code == kEndV3) break;
                const u32 e = table + (s.block < 3 ? kChromaDc : kLumaDc) + 4 * (s.bits >> 24);
                const u32 length = m.r16(e), size = m.r16(e + 2);
                s.bits <<= length & 31u;
                u32 diff = 0;
                if (size) {
                    const u32 down = (32 - size) & 31u;
                    diff = s.bits >> down;
                    const bool negative = static_cast<s32>(s.bits) >= 0;
                    s.bits <<= size & 31u;
                    if (negative) diff -= 0xFFFFFFFFu >> down;
                    s.fill += size;
                }
                use(length);
                u32& pred = s.block > 2 ? s.dc[2] : s.dc[s.block - 1];
                pred += diff;
                put(s.qscale | ((pred << 2) & 0x3FFu));
                if (++s.block == 7) s.block = 1;
            }
            if (static_cast<s32>(dc_slot - limit) >= 0) {
                m.w32(kVlcState, s.src);
                m.w32(kVlcState + 4, s.dst);
                m.w32(kVlcState + 8, s.bits);
                m.w32(kVlcState + 12, s.fill);
                m.w32(kVlcState + 16, s.qscale);
                m.w32(kVlcState + 20, s.block);
                for (u32 k = 0; k < 3; ++k) m.w32(kVlcState + 24 + 4 * k, s.dc[k]);
                return 1;
            }
        }
        resume = false;

        for (bool block_end = false; !block_end;) {
            u32 e = table + kAc + 8 * (s.bits >> 19);
            u32 first = m.r32(e), more = 0;
            if (first) {
                more = m.r32(e + 4);
            } else {
                s.bits <<= 8;
                use(8);
                e = table + kAcLong + 4 * (s.bits >> 23);
                first = m.r32(e);
            }
            const u32 length = first & 0xFFu;
            s.bits <<= length & 31u;
            use(length);
            const u32 out[3] = {first >> 16, more & 0xFFFFu, more >> 16};
            for (u32 k = 0; k < 3; ++k) {
                if (k == 1 && !more) break;
                if (k == 2 && !out[2]) break;
                if (out[k] == kEscape) {
                    put(s.bits >> 16);
                    const u32 next = m.r16(s.src);
                    s.src += 2;
                    s.bits = (s.bits << 16) | (next << s.fill);
                    break;
                }
                put(out[k]);
                if (out[k] == kEndOfBlock) {
                    block_end = true;
                    break;
                }
            }
        }
    }

    const u32 end = m.r32(kVlcEnd);
    for (; static_cast<s32>(s.dst - end) < 0; s.dst += 2) m.w16(s.dst, kEndOfBlock);
    return 0;
}
}

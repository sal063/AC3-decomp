#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kGpuPrintf = 0x800BB960u;
constexpr u32 kQueueMode = 0x800BB965u;
constexpr u32 kGraphDebug = 0x800BB966u;
constexpr u32 kScreenW = 0x800BB968u;
constexpr u32 kScreenH = 0x800BB96Au;
constexpr u32 kDrawing = 0x800BB96Cu;
constexpr u32 kDrawCallback = 0x800BB970u;

constexpr u32 kGp0 = 0x800BBA68u, kGp1 = 0x800BBA6Cu;
constexpr u32 kDma2Madr = 0x800BBA70u, kDma2Bcr = 0x800BBA74u, kDma2Chcr = 0x800BBA78u;
constexpr u32 kDma6Madr = 0x800BBA7Cu, kDma6Bcr = 0x800BBA80u, kDma6Chcr = 0x800BBA84u;
constexpr u32 kDpcr = 0x800BBA88u;
constexpr u32 kStatReady = 0x04000000u;
constexpr u32 kStatData = 0x08000000u;
constexpr u32 kDmaBusy = 0x01000000u;

constexpr u32 kQueueHead = 0x800BBA8Cu, kQueueTail = 0x800BBA90u;
constexpr u32 kMaskAdd = 0x800BBA94u, kMaskRun = 0x800BBA98u, kMaskReset = 0x800BBA9Cu;
constexpr u32 kQueue = 0x800CA458u;
constexpr u32 kEntrySize = 96u, kEntryFn = 0, kEntryArg0 = 4, kEntryArg1 = 8, kEntryData = 12;
constexpr u32 kQueueWrap = 0x3Fu;
constexpr u32 kRunQueueFn = 0x800AA1DCu;

constexpr u32 kDeadline = 0x800BBAA0u;
constexpr u32 kPolls = 0x800BBAA4u;
constexpr s32 kMaxPolls = 0xF0000;

constexpr u32 kCtlShadow = 0x800CA350u;
constexpr u32 kClearPacket = 0x800CA308u;
constexpr u32 kRestorePacket = 0x800CA330u;

constexpr u32 kFmtBadRect = 0x80025154u, kFmtName = 0x80025174u, kFmtRect = 0x80025160u;
}

u32 Game::get_graph_debug() {
    cover(0x800A808Cu);
    return m.r8(kGraphDebug);
}

void Game::gpu_check_rect(u32 name, u32 rect) {
    cover(0x800A81FCu);
    Frame f(*this, 0x20);
    const u32 level = m.r8(kGraphDebug);
    u32 fmt = kFmtName;
    if (level == 1) {
        const s32 w = m.r16s(rect + 4), max_w = m.r16s(kScreenW);
        bool bad = max_w < w;
        s32 x = 0, y = 0, h = 0;
        if (!bad) {
            x = m.r16s(rect);
            bad = max_w < w + x;
        }
        if (!bad) {
            y = m.r16s(rect + 2);
            const s32 max_h = m.r16s(kScreenH);
            bad = max_h < y;
            if (!bad) {
                h = m.r16s(rect + 6);
                bad = max_h < y + h;
            }
        }
        if (!bad && w > 0 && x >= 0 && y >= 0 && h > 0) return;
        fmt = kFmtBadRect;
    } else if (level != 2) {
        return;
    }
    lib_call(m.r32(kGpuPrintf), fmt, name);
    const u32 x = static_cast<u32>(static_cast<s32>(m.r16s(rect)));
    const u32 y = static_cast<u32>(static_cast<s32>(m.r16s(rect + 2)));
    const u32 w = static_cast<u32>(static_cast<s32>(m.r16s(rect + 4)));
    m.w32(f.at(0x10), static_cast<u32>(static_cast<s32>(m.r16s(rect + 6))));
    lib_call(m.r32(kGpuPrintf), kFmtRect, x, y, w);
}

u32 Game::gpu_mode_word(u32 dfe, u32 dtd, u32 tpage) {
    cover(0x800A9448u);
    u32 w = (dtd ? 0xE1000200u : 0xE1000000u) | (tpage & 0x9FFu);
    if (dfe) w |= 0x400u;
    return w;
}

u32 Game::gpu_clip_word(u32 fn, u32 x, u32 y) {
    cover(fn);
    auto inside = [&](u32 v, u32 size_at) -> u32 {
        const s32 s = static_cast<s16>(v);
        if (s < 0) return 0;
        if (m.r16s(size_at) - 1 < s) return m.r16(size_at) - 1u;
        return v;
    };
    const u32 cx = inside(x, kScreenW), cy = inside(y, kScreenH);
    return (fn == 0x800A9468u ? 0xE3000000u : 0xE4000000u) | ((cy & 0x3FFu) << 10) | (cx & 0x3FFu);
}

u32 Game::gpu_offset_word(u32 x, u32 y) {
    cover(0x800A9598u);
    return 0xE5000000u | ((y & 0x7FFu) << 11) | (x & 0x7FFu);
}

u32 Game::gpu_window_word(u32 rect) {
    cover(0x800A95B4u);
    Frame f(*this, 0x10);
    if (!rect) return 0;
    auto mask = [](s32 size) { return static_cast<u32>(-size & 0xFF) >> 3; };
    const u32 x = m.r8(rect) >> 3;
    m.w32(f.at(0), x);
    const u32 w = mask(m.r16s(rect + 4));
    m.w32(f.at(8), w);
    const u32 y = m.r8(rect + 2) >> 3;
    m.w32(f.at(4), y);
    const u32 h = mask(m.r16s(rect + 6));
    m.w32(f.at(12), h);
    return 0xE2000000u | (y << 15) | (x << 10) | (h << 5) | w;
}

u32 Game::gpu_set_draw_env(u32 fn, u32 dr, u32 env) {
    cover(fn);
    Frame f(*this, 0x28);
    const u32 rect = f.at(0x10);
    auto sx = [&](u32 at) { return static_cast<u32>(static_cast<s32>(m.r16s(at))); };
    auto s16of = [](u32 v) { return static_cast<u32>(static_cast<s32>(static_cast<s16>(v))); };
    m.w32(dr + 4, gpu_clip_word(0x800A9468u, sx(env), sx(env + 2)));
    const u32 right = s16of(m.r16(env + 4) + m.r16(env) - 1u);
    const u32 bottom = s16of(m.r16(env + 2) + m.r16(env + 6) - 1u);
    m.w32(dr + 8, gpu_clip_word(0x800A9500u, right, bottom));
    m.w32(dr + 12, gpu_offset_word(sx(env + 8), sx(env + 10)));
    m.w32(dr + 16, gpu_mode_word(m.r8(env + 0x17), m.r8(env + 0x16), m.r16(env + 0x14)));
    m.w32(dr + 20, gpu_window_word(env + 12));
    m.w32(dr + 24, 0xE6000000u);
    u32 words = 7;
    if (m.r8(env + 0x18)) {
        auto inside = [&](u32 v, u32 size_at) -> u32 {
            const s32 s = static_cast<s16>(v);
            if (s < 0) return 0;
            if (m.r16s(size_at) - 1 < s) return m.r16(size_at) - 1u;
            return v;
        };
        m.w16(rect, m.r16(env));
        m.w16(rect + 2, m.r16(env + 2));
        m.w16(rect + 4, m.r16(env + 4));
        m.w16(rect + 6, m.r16(env + 6));
        m.w16(rect + 4, inside(m.r16(rect + 4), kScreenW));
        m.w16(rect + 6, inside(m.r16(rect + 6), kScreenH));
        const bool fill = fn == 0x800A91D8u && !(m.r16(rect) & 0x3Fu) && !(m.r16(rect + 4) & 0x3Fu);
        if (!fill) {
            m.w16(rect, m.r16(rect) - m.r16(env + 8));
            m.w16(rect + 2, m.r16(rect + 2) - m.r16(env + 10));
        }
        const u32 colour = (static_cast<u32>(m.r8(env + 0x1B)) << 16) | (static_cast<u32>(m.r8(env + 0x1A)) << 8) |
                           m.r8(env + 0x19);
        m.w32(dr + 28, (fill ? 0x02000000u : 0x60000000u) | colour);
        m.w32(dr + 32, m.r32(rect));
        m.w32(dr + 36, m.r32(rect + 4));
        words = 10;
    }
    m.w8(dr + 3, words - 1);
    return words - 1;
}

u32 Game::gpu_status() {
    cover(0x800A9634u);
    return m.r32(m.r32(kGp1));
}

u32 Game::gpu_ctl(u32 command) {
    cover(0x800A9E18u);
    m.w32(m.r32(kGp1), command);
    m.w8(kCtlShadow + (command >> 24), command);
    return command >> 24;
}

u32 Game::gpu_ctl_last(u32 top) {
    cover(0x800A9E3Cu);
    return m.r8(kCtlShadow + top);
}

u32 Game::gpu_write_words(u32 p, u32 n) {
    cover(0x800A9E50u);
    m.w32(m.r32(kGp1), 0x04000000u);
    for (; n; --n, p += 4) m.w32(m.r32(kGp0), m.r32(p));
    return 0;
}

u32 Game::gpu_send_list(u32 p) {
    cover(0x800A9E90u);
    m.w32(m.r32(kGp1), 0x04000002u);
    m.w32(m.r32(kDma2Madr), p);
    m.w32(m.r32(kDma2Bcr), 0);
    const u32 chcr = m.r32(kDma2Chcr);
    m.w32(chcr, 0x01000401u);
    return chcr;
}

u32 Game::gpu_read_info(u32 n) {
    cover(0x800A9ED8u);
    m.w32(m.r32(kGp1), n | 0x10000000u);
    return m.r32(m.r32(kGp0)) & 0xFFFFFFu;
}

u32 Game::gpu_memset(u32 p, u32 c, u32 n) {
    cover(0x800AAD20u);
    for (; n; --n, ++p) m.w8(p, c);
    return 0xFFFFFFFFu;
}

u32 Game::gpu_timeout_start() {
    cover(0x800AA6C8u);
    Frame f(*this, 0x18);
    const u32 deadline = hw->vsync(-1) + 0xF0u;
    m.w32(kDeadline, deadline);
    m.w32(kPolls, 0);
    return deadline;
}

s32 Game::gpu_timeout_check() {
    cover(0x800AA6FCu);
    Frame f(*this, 0x20);
    if (!(m.r32s(kDeadline) < static_cast<s32>(hw->vsync(-1)))) {
        const s32 polls = m.r32s(kPolls);
        m.w32(kPolls, static_cast<u32>(polls) + 1);
        if (!(kMaxPolls < polls)) return 0;
    }
    m.w32(f.at(0x10), m.r32(m.r32(kDma2Madr)));
    const u32 mask = hw->set_intr_mask(0);
    m.w32(kQueueTail, 0);
    m.w32(kMaskReset, mask);
    m.w32(kQueueHead, m.r32(kQueueTail));
    m.w32(m.r32(kDma2Chcr), 0x401u);
    const u32 dpcr = m.r32(kDpcr);
    m.w32(dpcr, m.r32(dpcr) | 0x800u);
    m.w32(m.r32(kGp1), 0x02000000u);
    m.w32(m.r32(kGp1), 0x01000000u);
    hw->set_intr_mask(m.r32(kMaskReset));
    return -1;
}

s32 Game::gpu_clear_otag(u32 ot, u32 n) {
    cover(0x800A964Cu);
    Frame f(*this, 0x20);
    const u32 dpcr = m.r32(kDpcr);
    m.w32(dpcr, m.r32(dpcr) | 0x08000000u);
    m.w32(m.r32(kDma6Chcr), 0);
    m.w32(m.r32(kDma6Madr), ot + 4 * n - 4);
    m.w32(m.r32(kDma6Bcr), n);
    m.w32(m.r32(kDma6Chcr), 0x11000002u);
    gpu_timeout_start();
    while (m.r32(m.r32(kDma6Chcr)) & kDmaBusy)
        if (gpu_timeout_check()) return -1;
    return static_cast<s32>(n);
}

u32 Game::gpu_clear_image(u32 rect, u32 colour) {
    cover(0x800A972Cu);
    Frame f(*this, 0x20);
    auto inside = [&](u32 at, u32 size_at) -> u32 {
        const s32 s = m.r16s(at);
        const u32 v = m.r16(at);
        if (s < 0) return 0;
        if (m.r16s(size_at) - 1 < s) return m.r16(size_at) - 1u;
        return v;
    };
    m.w16(rect + 4, inside(rect + 4, kScreenW));
    m.w16(rect + 6, inside(rect + 6, kScreenH));
    const u32 dfe = (colour >> 31) << 10;
    if ((m.r16(rect) & 0x3Fu) || (m.r16(rect + 4) & 0x3Fu)) {
        m.w32(kClearPacket, (kRestorePacket & 0xFFFFFFu) | 0x08000000u);
        m.w32(kClearPacket + 0x10, 0xE6000000u);
        m.w32(kClearPacket + 4, 0xE3000000u);
        m.w32(kClearPacket + 8, 0xE4FFFFFFu);
        m.w32(kClearPacket + 12, 0xE5000000u);
        const u32 stat = m.r32(m.r32(kGp1));
        m.w32(kClearPacket + 0x18, (colour & 0xFFFFFFu) | 0x60000000u);
        m.w32(kClearPacket + 0x14, (stat & 0x7FFu) | 0xE1000000u | dfe);
        m.w32(kClearPacket + 0x1C, m.r32(rect));
        m.w32(kRestorePacket, 0x03FFFFFFu);
        m.w32(kClearPacket + 0x20, m.r32(rect + 4));
        m.w32(kRestorePacket + 4, gpu_read_info(3) | 0xE3000000u);
        m.w32(kRestorePacket + 8, gpu_read_info(4) | 0xE4000000u);
        m.w32(kRestorePacket + 12, gpu_read_info(5) | 0xE5000000u);
    } else {
        m.w32(kClearPacket, 0x05FFFFFFu);
        m.w32(kClearPacket + 4, 0xE6000000u);
        const u32 stat = m.r32(m.r32(kGp1));
        m.w32(kClearPacket + 12, (colour & 0xFFFFFFu) | 0x02000000u);
        m.w32(kClearPacket + 8, (stat & 0x7FFu) | 0xE1000000u | dfe);
        m.w32(kClearPacket + 0x10, m.r32(rect));
        m.w32(kClearPacket + 0x14, m.r32(rect + 4));
    }
    gpu_send_list(kClearPacket);
    return 0;
}

bool Game::gpu_image_size(u32 rect, u32& rest, u32& blocks) {
    auto inside = [&](u32 at, u32 size_at) -> u32 {
        const s32 s = m.r16s(at);
        const u32 v = m.r16(at);
        if (s < 0) return 0;
        if (m.r16s(size_at) < s) return m.r16(size_at);
        return v;
    };
    m.w16(rect + 4, inside(rect + 4, kScreenW));
    const u32 h = inside(rect + 6, kScreenH);
    const s32 pixels = m.r16s(rect + 4) * static_cast<s32>(static_cast<s16>(h));
    m.w16(rect + 6, h);
    s32 up = pixels + 1;
    up += static_cast<s32>(static_cast<u32>(up) >> 31);
    const s32 words = up >> 1;
    if (words <= 0) return false;
    blocks = static_cast<u32>(up >> 5);
    rest = static_cast<u32>(words) - (blocks << 4);
    return true;
}

bool Game::gpu_wait_status(u32 bit) {
    while (!(m.r32(m.r32(kGp1)) & bit))
        if (gpu_timeout_check()) return false;
    return true;
}

s32 Game::gpu_load_image(u32 rect, u32 src) {
    cover(0x800A995Cu);
    Frame f(*this, 0x30);
    gpu_timeout_start();
    u32 rest = 0, blocks = 0;
    if (!gpu_image_size(rect, rest, blocks)) return -1;
    if (!gpu_wait_status(kStatReady)) return -1;
    m.w32(m.r32(kGp1), 0x04000000u);
    m.w32(m.r32(kGp0), 0x01000000u);
    m.w32(m.r32(kGp0), 0xA0000000u);
    m.w32(m.r32(kGp0), m.r32(rect));
    m.w32(m.r32(kGp0), m.r32(rect + 4));
    for (; rest; --rest, src += 4) m.w32(m.r32(kGp0), m.r32(src));
    if (blocks) {
        m.w32(m.r32(kGp1), 0x04000002u);
        m.w32(m.r32(kDma2Madr), src);
        m.w32(m.r32(kDma2Bcr), (blocks << 16) | 0x10u);
        m.w32(m.r32(kDma2Chcr), 0x01000201u);
    }
    return 0;
}

s32 Game::gpu_store_image(u32 rect, u32 dst) {
    cover(0x800A9B98u);
    Frame f(*this, 0x28);
    gpu_timeout_start();
    u32 rest = 0, blocks = 0;
    if (!gpu_image_size(rect, rest, blocks)) return -1;
    if (!gpu_wait_status(kStatReady)) return -1;
    m.w32(m.r32(kGp1), 0x04000000u);
    m.w32(m.r32(kGp0), 0x01000000u);
    m.w32(m.r32(kGp0), 0xC0000000u);
    m.w32(m.r32(kGp0), m.r32(rect));
    m.w32(m.r32(kGp0), m.r32(rect + 4));
    if (!gpu_wait_status(kStatData)) return -1;
    for (; rest; --rest, dst += 4) m.w32(dst, m.r32(m.r32(kGp0)));
    if (blocks) {
        m.w32(m.r32(kGp1), 0x04000003u);
        m.w32(m.r32(kDma2Madr), dst);
        m.w32(m.r32(kDma2Bcr), (blocks << 16) | 0x10u);
        m.w32(m.r32(kDma2Chcr), 0x01000200u);
    }
    return 0;
}

u32 Game::gpu_run_queue() {
    cover(0x800AA1DCu);
    Frame f(*this, 0x20);
    if (m.r32(m.r32(kDma2Chcr)) & kDmaBusy) return 1;
    m.w32(kMaskRun, hw->set_intr_mask(0));
    while (m.r32(kQueueHead) != m.r32(kQueueTail) && !(m.r32(m.r32(kDma2Chcr)) & kDmaBusy)) {
        if (((m.r32(kQueueTail) + 1) & kQueueWrap) == m.r32(kQueueHead) && !m.r32(kDrawCallback))
            hw->dma_callback(2, 0);
        const u32 gp1 = m.r32(kGp1);
        while (!(m.r32(gp1) & kStatReady)) {
        }
        const u32 entry = kQueue + kEntrySize * m.r32(kQueueTail);
        const u32 arg0 = m.r32(entry + kEntryArg0), arg1 = m.r32(entry + kEntryArg1);
        lib_call(m.r32(entry + kEntryFn), arg0, arg1);
        m.w32(kQueueTail, (m.r32(kQueueTail) + 1) & kQueueWrap);
    }
    hw->set_intr_mask(m.r32(kMaskRun));
    if (m.r32(kQueueHead) == m.r32(kQueueTail) && !(m.r32(m.r32(kDma2Chcr)) & kDmaBusy) && m.r32(kDrawing)) {
        if (const u32 callback = m.r32(kDrawCallback)) {
            m.w32(kDrawing, 0);
            lib_call(callback);
        }
    }
    return (m.r32(kQueueHead) - m.r32(kQueueTail)) & kQueueWrap;
}

s32 Game::gpu_add_queue(u32 fn, u32 p, u32 size, u32 arg) {
    cover(0x800A9F2Cu);
    Frame f(*this, 0x28);
    gpu_timeout_start();
    while (((m.r32(kQueueHead) + 1) & kQueueWrap) == m.r32(kQueueTail)) {
        if (gpu_timeout_check()) return -1;
        gpu_run_queue();
    }
    m.w32(kMaskAdd, hw->set_intr_mask(0));
    const bool queued = m.r8(kQueueMode) != 0;
    m.w32(kDrawing, 1);
    if (!queued || (m.r32(kQueueHead) == m.r32(kQueueTail) && !(m.r32(m.r32(kDma2Chcr)) & kDmaBusy) &&
                    !m.r32(kDrawCallback))) {
        const u32 gp1 = m.r32(kGp1);
        while (!(m.r32(gp1) & kStatReady)) {
        }
        lib_call(fn, p, arg);
        hw->set_intr_mask(m.r32(kMaskAdd));
        return 0;
    }
    hw->dma_callback(2, kRunQueueFn);
    if (size) {
        const s32 words = static_cast<s32>(size) / 4;
        for (s32 i = 0; i < words; ++i)
            m.w32(kQueue + kEntrySize * m.r32(kQueueHead) + kEntryData + 4 * static_cast<u32>(i),
                  m.r32(p + 4 * static_cast<u32>(i)));
        const u32 entry = kQueue + kEntrySize * m.r32(kQueueHead);
        m.w32(entry + kEntryArg0, entry + kEntryData);
    } else {
        m.w32(kQueue + kEntrySize * m.r32(kQueueHead) + kEntryArg0, p);
    }
    m.w32(kQueue + kEntrySize * m.r32(kQueueHead) + kEntryArg1, arg);
    m.w32(kQueue + kEntrySize * m.r32(kQueueHead) + kEntryFn, fn);
    const u32 mask = m.r32(kMaskAdd);
    m.w32(kQueueHead, (m.r32(kQueueHead) + 1) & kQueueWrap);
    hw->set_intr_mask(mask);
    gpu_run_queue();
    return static_cast<s32>((m.r32(kQueueHead) - m.r32(kQueueTail)) & kQueueWrap);
}

s32 Game::gpu_add_queue_ptr(u32 fn, u32 p, u32 arg) {
    cover(0x800A9F08u);
    Frame f(*this, 0x18);
    return gpu_add_queue(fn, p, 0, arg);
}

u32 Game::gpu_version(u32 mode) {
    cover(0x800AA840u);
    m.w32(m.r32(kGp1), 0x10000007u);
    const u32 gp0 = m.r32(kGp0);
    if ((m.r32(gp0) & 0xFFFFFFu) != 2) {
        m.w32(gp0, (m.r32(m.r32(kGp1)) & 0x3FFFu) | 0xE1001000u);
        (void)m.r32(m.r32(kGp0));
        return 0;
    }
    if (mode & 8u) {
        m.w32(m.r32(kGp1), 0x09000001u);
        return 2;
    }
    return 1;
}

u32 Game::gpu_reset(u32 mode) {
    cover(0x800AA43Cu);
    Frame f(*this, 0x18);
    const u32 mask = hw->set_intr_mask(0);
    m.w32(kQueueTail, 0);
    m.w32(kMaskReset, mask);
    m.w32(kQueueHead, m.r32(kQueueTail));
    const u32 kind = mode & 7u;
    if (kind == 0 || kind == 5) {
        m.w32(m.r32(kDma2Chcr), 0x401u);
        const u32 dpcr = m.r32(kDpcr);
        m.w32(dpcr, m.r32(dpcr) | 0x800u);
        m.w32(m.r32(kGp1), 0);
        gpu_memset(kCtlShadow, 0, 0x100);
        gpu_memset(kQueue, 0, 0x1800);
    } else if (kind == 1 || kind == 3) {
        m.w32(m.r32(kDma2Chcr), 0x401u);
        const u32 dpcr = m.r32(kDpcr);
        m.w32(dpcr, m.r32(dpcr) | 0x800u);
        m.w32(m.r32(kGp1), 0x02000000u);
        m.w32(m.r32(kGp1), 0x01000000u);
    }
    hw->set_intr_mask(m.r32(kMaskReset));
    return kind == 0 ? gpu_version(mode) : 0u;
}

s32 Game::gpu_sync(u32 mode) {
    cover(0x800AA58Cu);
    Frame f(*this, 0x18);
    if (mode == 0) {
        gpu_timeout_start();
        while (m.r32(kQueueHead) != m.r32(kQueueTail)) {
            gpu_run_queue();
            if (gpu_timeout_check()) return -1;
        }
        while ((m.r32(m.r32(kDma2Chcr)) & kDmaBusy) || !(m.r32(m.r32(kGp1)) & kStatReady))
            if (gpu_timeout_check()) return -1;
        return 0;
    }
    const u32 queued = (m.r32(kQueueHead) - m.r32(kQueueTail)) & kQueueWrap;
    if (queued) gpu_run_queue();
    if (!(m.r32(m.r32(kDma2Chcr)) & kDmaBusy) && (m.r32(m.r32(kGp1)) & kStatReady)) return static_cast<s32>(queued);
    return queued ? static_cast<s32>(queued) : 1;
}
}

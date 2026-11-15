#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kPanel = 0x80112E18u, kMenu = 0x80112DC4u, kSector = 0x80112E14u;
constexpr u32 kPlayer = 0x800BD044u, kDisplay = 0x800BCE48u;
constexpr u32 kVlcBuffer = 0, kStripBuffer = 1, kDisplayPage = 2, kStripDone = 3, kFinished = 4;
constexpr u32 kBits24 = 5, kPageRects = 0x38, kUploadRect = 0x48, kVlcTable = 0x50;
constexpr u32 kWidth = 0x11050, kHeight = 0x11052, kFrameNumber = 0x11054, kStripWords = 0x11056;
constexpr u32 kTimeout = 0x11068, kRingState = 0x118;
u32 alignedBytes(u32 bytes) { return ((bytes + 3) >> 2) << 2; }
void updateDisplay(Memory& m) {
    const u32 display = m.r32(kDisplay);
    m.w8(display + 0x28, 0);
    m.w8(display + 0x29, m.r8(m.r32(kPlayer) + 0x38));
    m.w16(display + 0x20, m.r16(display + 8));
    m.w16(display + 0x22, m.r16(display + 12));
    m.w8(display + 0x44, 1);
    for (u32 offset : {0x45u, 0x46u, 0x47u}) m.w8(m.r32(kDisplay) + offset, 0);
}
}

u32 Game::o2_panel_video_setup(u32 descriptor) {
    cover(0x800FC600u);
    const u32 panel = m.r32(kPanel);
    for (u32 field = 0; field < 3; ++field)
        m.w32(panel + 0x11058 + 4 * field, m.r32(descriptor + 4 + 4 * field));
    for (u32 buffer = 0; buffer < 2; ++buffer)
        m.w32(panel + 8 + 4 * buffer, m.r32(descriptor + 16 + 4 * buffer));
    for (u32 buffer = 0; buffer < 10; ++buffer)
        m.w32(panel + 16 + 4 * buffer, m.r32(descriptor + 24 + 4 * buffer));
    m.w8(panel + kBits24, m.r8(descriptor));
    m.w16(panel + 0x38, m.r16(descriptor + 64));
    m.w16(panel + 0x3A, m.r16(descriptor + 66));
    m.w16(panel + 0x40, m.r16(descriptor + 68));
    for (u32 offset : {0x3Cu, 0x3Eu, 0x44u, 0x46u}) m.w16(panel + offset, 0);
    m.w16(panel + 0x42, m.r16(descriptor + 70));
    m.w16(panel + kUploadRect, m.r16(descriptor + 64));
    m.w16(panel + kUploadRect + 2, m.r16(descriptor + 66));
    m.w16(panel + kUploadRect + 4, m.r32(descriptor) ? 24 : 16);
    m.w16(panel + kUploadRect + 6, 0);
    for (u32 offset : {kHeight, kWidth, kStripWords}) m.w16(panel + offset, 0);
    for (u32 flag = 0; flag <= kFinished; ++flag) m.w8(panel + flag, 0);
    m.w32(panel + 0x11064, 0);
    m.w32(panel + kTimeout, 300);
    return panel + 0x10000;
}

u32 Game::o2_panel_video_open(u32 stream, u32 bits24, u32 base, u32 ringBytes,
                            u32 vlcBytes, u32 stripBytes, u32 x0, u32 y0, u32 x1, u32 y1) {
    cover(0x800FC764u);
    Frame frame(*this, 96);
    m.w32(kPanel, base);
    const u32 descriptor = frame.at(16);
    u32 cursor = base + 0x1106C;
    const u32 ringSize = alignedBytes(ringBytes), vlcSize = alignedBytes(vlcBytes);
    const u32 stripSize = alignedBytes(stripBytes);
    m.w32(descriptor, bits24);
    m.w32(descriptor + 4, stream);
    m.w32(descriptor + 8, ringSize);
    m.w32(descriptor + 12, cursor);
    cursor += ringSize;
    for (u32 buffer = 0; buffer < 2; ++buffer) {
        m.w32(descriptor + 16 + 4 * buffer, cursor);
        cursor += vlcSize;
    }
    for (u32 buffer = 0; buffer < 10; ++buffer) {
        m.w32(descriptor + 24 + 4 * buffer, cursor);
        cursor += stripSize;
    }
    m.w16(descriptor + 64, x0); m.w16(descriptor + 66, y0);
    m.w16(descriptor + 68, x1); m.w16(descriptor + 70, y1);
    o2_panel_video_setup(descriptor);
    o2_panel_decoder_init();
    cd_reset(0, 0);
    return cursor;
}

u32 Game::o2_panel_vlc_next(u32 panel) {
    cover(0x800FCC7Cu);
    Frame frame(*this, 48);
    const u32 state = m.r32(m.r32(kMenu) + kRingState);
    if (state == 1) {
        const u32 data = frame.at(16);
        if (kcd_ring_get(data, kSector)) return 0;
        m.w32(m.r32(kMenu) + kRingState, 0);
        const u32 sector = m.r32(kSector);
        if (!m.r16(sector + 6)) m.w8(panel + kFinished, 1);
        m.w16(panel + kFrameNumber, m.r16(sector + 6));
        const s32 height = m.r16s(sector + 10);
        if (m.r16s(panel + kHeight) != height) {
            m.w16(panel + kHeight, static_cast<u32>(height));
            const s32 words = wrap::mul(height, m.r16s(panel + kUploadRect + 4)) >> 1;
            m.w16(panel + kStripWords, static_cast<u32>(words));
            for (u32 offset : {0x4Eu, 0x46u, 0x3Eu}) m.w16(panel + offset, static_cast<u32>(height));
        }
        s32 width = m.r16s(m.r32(kSector) + 8);
        if (m.r16s(panel + kWidth) != width) {
            m.w16(panel + kWidth, static_cast<u32>(width));
            if (m.r8(panel + kBits24)) width = (3 * width) >> 1;
            m.w16(panel + 0x44, static_cast<u32>(width));
            m.w16(panel + 0x3C, static_cast<u32>(width));
        }
        m.w8(panel + kVlcBuffer, m.r8(panel + kVlcBuffer) ^ 1u);
        const s32 bufferSize = static_cast<s32>(m.r32(data));
        dct_vlc_size(static_cast<u32>(bufferSize / 4) + 1);
        if (dct_vlc2(m.r32(data), m.r32(panel + 8 + 4 * m.r8(panel + kVlcBuffer)), panel + kVlcTable) == 1)
            return 0;
    } else if (state == 0) {
        if (dct_vlc2(0, 0, panel + kVlcTable) == 1) return 0;
    } else {
        return 1;
    }
    m.w32(m.r32(kMenu) + kRingState, 1);
    kcd_ring_free(m.r32(kSector));
    return 1;
}

u32 Game::o2_panel_first_frame(u32 poll) {
    cover(0x800FC89Cu);
    Frame frame(*this, 40);
    if (poll) {
        if (o2_panel_vlc_next(m.r32(kPanel))) return 1;
        const u32 panel = m.r32(kPanel);
        const u32 remaining = m.r32(panel + kTimeout) - 1;
        m.w32(panel + kTimeout, remaining);
        if (static_cast<s32>(remaining) < 0) m.w8(panel + kFinished, 1);
        return 0;
    }
    m.w32(frame.at(16), 120);
    do {
        if (o2_panel_vlc_next(m.r32(kPanel))) return 1;
        kcd_pump();
        hw->vsync(0);
        m.w32(frame.at(16), m.r32(frame.at(16)) - 1);
    } while (m.r32s(frame.at(16)) >= 0);
    m.w8(m.r32(kPanel) + kFinished, 1);
    return 1;
}

u32 Game::o2_panel_video_decode(u32 poll) {
    cover(0x800FC970u);
    Frame frame(*this, 32);
    if (m.r8(m.r32(kPanel) + kFinished)) return 1;
    if (o2_panel_vlc_next(m.r32(kPanel))) {
        m.w8(m.r32(kPanel) + kStripDone, 0);
        u32 panel = m.r32(kPanel);
        hw->dec_dct_in(m.r32(panel + 8 + 4 * m.r8(panel + kVlcBuffer)), m.r8(panel + kBits24) | 2u);
        panel = m.r32(kPanel);
        hw->dec_dct_out(m.r32(panel + 16 + 4 * m.r8(panel + kStripBuffer)),
                        static_cast<u32>(m.r16s(panel + kStripWords)));
        o2_panel_strip_wait(m.r32(kPanel), poll);
        m.w32(m.r32(kPanel) + kTimeout, 300);
    } else {
        const u32 panel = m.r32(kPanel);
        if (m.r32s(panel + kTimeout) > 0) m.w32(panel + kTimeout, m.r32(panel + kTimeout) - 1);
        else m.w8(panel + kFinished, 1);
    }
    return 0;
}

u32 Game::o2_panel_strip_done() {
    cover(0x800FCB44u);
    Frame frame(*this, 24);
    u32 panel = m.r32(kPanel);
    hw->load_image(panel + kUploadRect, m.r32(panel + 16 + 4 * m.r8(panel + kStripBuffer)));
    panel = m.r32(kPanel);
    const u32 strip = m.r8(panel + kStripBuffer);
    m.w8(panel + kStripBuffer, strip < 9 ? strip + 1 : 0);
    panel = m.r32(kPanel);
    const u32 x = m.r16(panel + kUploadRect) + m.r16(panel + kUploadRect + 4);
    m.w16(panel + kUploadRect, x);
    const u32 pageRect = panel + kPageRects + 8 * m.r8(panel + kDisplayPage);
    if (static_cast<s16>(x) < m.r16s(pageRect) + m.r16s(pageRect + 4))
        return hw->dec_dct_out(m.r32(panel + 16 + 4 * m.r8(panel + kStripBuffer)),
                               static_cast<u32>(m.r16s(panel + kStripWords)));
    m.w8(panel + kStripDone, 1);
    panel = m.r32(kPanel);
    m.w8(panel + kDisplayPage, m.r8(panel + kDisplayPage) ^ 1u);
    panel = m.r32(kPanel);
    const u32 nextRect = panel + kPageRects + 8 * m.r8(panel + kDisplayPage);
    m.w16(panel + kUploadRect, m.r16(nextRect));
    const u32 y = m.r16(nextRect + 2);
    m.w16(panel + kUploadRect + 2, y);
    return y;
}

u32 Game::o2_panel_movie_begin() {
    cover(0x800FCE40u);
    Frame frame(*this, 48);
    const u32 scene = m.r32(g::kScene);
    m.w32(scene, m.r32(scene) | 0x10000000u);
    m.w32(m.r32(kDisplay) + 20, 0);
    m.w32(kPlayer, mem_alloc(68));
    m.w32(m.r32(kPlayer) + 4, mem_alloc(1024));
    m.w32(m.r32(kPlayer) + 8, mem_alloc(1024));
    const u32 player = m.r32(kPlayer);
    m.w32(player, 0);
    m.w16(player + 0x24, 832); m.w16(player + 0x26, 256);
    m.w16(player + 0x28, 832); m.w16(player + 0x2A, 384);
    m.w16(player + 0x2C, 192); m.w16(player + 0x2E, 128);
    m.w16(player + 0x30, 0); m.w16(player + 0x32, 0);
    const u32 stream = m.r32(m.r32(kMenu) + 0x12C);
    if (stream) m.w32(player + 0x34, stream);
    m.w32(player + 0x38, 0); m.w32(player + 0x3C, 0); m.w32(player + 0x40, 120);
    const u32 rect = frame.at(16);
    m.w16(rect, 832); m.w16(rect + 2, 256);
    m.w16(rect + 4, 192); m.w16(rect + 6, 128);
    hw->clear_image(rect, 0, 0, 0);
    m.w16(rect, 832); m.w16(rect + 2, 384);
    hw->clear_image(rect, 0, 0, 0);
    m.w32(m.r32(kMenu) + 0x114, 0x8000);
    m.w32(m.r32(kMenu) + kRingState, 1);
    return 1;
}

u32 Game::o2_panel_movie_end() {
    cover(0x800FCF80u);
    Frame frame(*this, 32);
    const u32 scene = m.r32(g::kScene);
    m.w32(scene, m.r32(scene) & ~0x10000000u);
    const u32 rect = frame.at(16);
    m.w16(rect, 832); m.w16(rect + 2, 256);
    m.w16(rect + 4, 192); m.w16(rect + 6, 128);
    hw->clear_image(rect, 0, 0, 0);
    m.w16(rect, 832); m.w16(rect + 2, 384);
    hw->clear_image(rect, 0, 0, 0);
    const u32 panel = m.r32(kPanel);
    m.w32(m.r32(kMenu) + 0x128, 1);
    return heap_set(panel);
}

u32 Game::o2_panel_movie_tick(u32 skipButtons) {
    cover(0x800FD028u);
    Frame frame(*this, 64);
    const u32 player = m.r32(kPlayer);
    switch (m.r32(player)) {
    case 0: {
        const u32 base = mem_peek(0);
        const u32 current = m.r32(kPlayer);
        const u32 stripBytes = m.r32(current + 0x38) ? 0x1800 : 0x1000;
        const u32 x0 = static_cast<u32>(m.r16s(current + 0x24) + m.r16s(current + 0x30));
        const u32 y0 = static_cast<u32>(m.r16s(current + 0x26) + m.r16s(current + 0x32));
        const u32 x1 = static_cast<u32>(m.r16s(current + 0x28) + m.r16s(current + 0x30));
        const u32 y1 = static_cast<u32>(m.r16s(current + 0x2A) + m.r16s(current + 0x32));
        m.w32(frame.at(16), 0xC404); m.w32(frame.at(20), stripBytes);
        m.w32(frame.at(24), x0); m.w32(frame.at(28), y0);
        m.w32(frame.at(32), x1); m.w32(frame.at(36), y1);
        o2_panel_video_open(m.r32(current + 0x34), m.r32(current + 0x38), base,
                            0x12300, 0xC404, stripBytes, x0, y0, x1, y1);
        o2_panel_video_request();
        m.w32(m.r32(kPlayer), 1);
        return 0;
    }
    case 1:
        if (!m.r32(m.r32(g::kScene) + 4) && o2_panel_first_frame(1)) {
            m.w32(m.r32(kPlayer), 2);
            gmv_first_page();
            updateDisplay(m);
        }
        if (m.r8(m.r32(kPanel) + kFinished) == 1) {
            gmv_close();
            m.w32(m.r32(kPlayer), 3);
        }
        return 0;
    case 2: {
        if (m.r32(m.r32(g::kScene) + 0x12C) & skipButtons)
            m.w8(m.r32(kPanel) + kFinished, 1);
        if (m.r8(m.r32(kPanel) + kFinished) == 1) {
            gmv_close();
            m.w32(m.r32(kPlayer), 3);
        }
        const u32 panel = m.r32(kPanel);
        if (!m.r32(panel + 0x11064)) {
            if (o2_panel_video_decode(1)) m.w32(m.r32(kPanel) + 0x11064, 1);
        } else if (m.r8(panel + kStripDone) == 1) {
            o2_panel_strip_wait(panel, 0);
            gmv_close();
            const u32 page = gmv_first_page();
            const u32 current = m.r32(kPlayer), rect = frame.at(40);
            m.w16(rect, m.r16(current + 0x24 + 4 * page));
            m.w16(rect + 2, m.r16(current + 0x26 + 4 * page));
            m.w16(rect + 6, m.r16(current + 0x2E));
            const s32 width = m.r16s(current + 0x2C);
            m.w16(rect + 4, m.r32(current + 0x38) ? static_cast<u32>((3 * width) >> 1)
                                                               : m.r16(current + 0x2C));
            hw->clear_image(rect, 0, 0, 0);
            m.w32(m.r32(kPlayer), 3);
            return 0;
        }
        gmv_first_page();
        updateDisplay(m);
        return 0;
    }
    case 3: m.w32(m.r32(kPlayer), 4); return 0;
    case 4: return 1;
    default: return 0;
    }
}

u32 Game::o2_panel_movie_dispatch(u32 skipButtons) {
    cover(0x800FD408u);
    Frame frame(*this, 32);
    switch (m.r32(m.r32(kMenu) + 0x114)) {
    case 0x4000: o2_panel_movie_begin(); return 0;
    case 0x10000: o2_panel_movie_end(); return 1;
    default:
        if (o2_panel_movie_tick(skipButtons)) m.w32(m.r32(kMenu) + 0x114, 0x10000);
        return 0;
    }
}
u32 Game::o2_panel_movie_quad(u32 packet, u32 ot, u32 bounds, u32 texture) {
    cover(0x800FD48Cu);
    constexpr u32 kVertexStride = 8, kTagWords = 9, kTexturePage = 0x11D;
    m.w8(packet + 3, kTagWords);
    m.w8(packet + 7, 0x2C);
    for (u32 channel = 0; channel < 3; ++channel) m.w8(packet + 4 + channel, 128);
    for (u32 vertex = 0; vertex < 4; ++vertex) {
        const u32 xy = packet + 8 + kVertexStride * vertex;
        m.w16(xy, m.r16(bounds) + ((vertex & 1u) ? m.r16(bounds + 4) : 0u));
        m.w16(xy + 2, m.r16(bounds + 2) + ((vertex & 2u) ? m.r16(bounds + 6) : 0u));
    }
    for (u32 vertex = 0; vertex < 4; ++vertex) {
        const u32 uv = packet + 12 + kVertexStride * vertex;
        m.w8(uv, m.r8(texture) + ((vertex & 1u) ? m.r8(texture + 4) : 0u));
        const u32 page = m.r8(m.r32(kPanel) + kDisplayPage);
        const u32 v = m.r8(texture + 2) + ((1u - page) << 7)
                      + ((vertex & 2u) ? m.r8(texture + 6) : 0u);
        if (vertex == 3) m.w16(packet + 0x16, kTexturePage);
        m.w8(uv + 1, v);
    }
    m.w32(packet, (m.r32(packet) & 0xFF000000u) | (m.r32(ot) & 0xFFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (packet & 0xFFFFFFu));
    return packet + 40;
}
}

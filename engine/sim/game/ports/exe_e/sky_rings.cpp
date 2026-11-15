#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
u32 Game::draw_sky_rings() {
    cover(0x800976F8u);
    Frame frame(*this, 48);
    constexpr u32 kCentre = g::kScratch + 28, kPoints = g::kScratch + 40;
    constexpr u32 kRingA = g::kScratch + 136, kRingB = kRingA + 400;
    constexpr u32 kCode = 0, kScreen = 4, kUv = 8, kColour = 12, kStride = 16;
    m.w16(kCentre, 12288); m.w16(kCentre + 2, 0); m.w16(kCentre + 4, 0);
    rot_matrix_yxz(kCentre, g::kScratch + 36);
    const u32 world = m.r32(g::kWorld);
    mul_matrix0(world + 108, g::kScratch + 36, g::kScratch + 68);
    for (u32 reg = 0; reg < 5; ++reg) gte.ctc2(8 + reg, m.r32(g::kScratch + 68 + 4 * reg));
    set_mul_matrix(world + 76, g::kScratch + 36);
    for (u32 reg = 5; reg < 8; ++reg) gte.ctc2(reg, 0);
    gte.ctc2(13, 768); gte.ctc2(14, 896); gte.ctc2(15, 1536);
    gte.ctc2(16, 4096); gte.ctc2(17, 0x10000000u); gte.ctc2(18, 0);
    gte.ctc2(19, 4096); gte.ctc2(20, 0);
    gte.mtc2(6, 0x34838383u); gte.mtc2(0, 0); gte.mtc2(1, 3072);
    gte.op(0x180001u);
    for (u32 vertex = 0; vertex < 24; ++vertex)
        m.w32(kPoints + 4 * vertex, m.r32(0x800B7104u + 4 * ((vertex * 4096) / 24)));
    if (static_cast<s32>(gte.cfc2(31)) >= 0) {
        m.w32(kCentre + kScreen, gte.mfc2(14));
        gte.mtc2(0, 0); gte.mtc2(1, static_cast<u32>(-4096)); gte.op(0x108041Bu);
        m.w32(kCentre + kCode, screen_outcode(kCentre + kScreen));
        m.w32(kCentre + 8, gte.mfc2(22));
    } else m.w32(kCentre + kCode, 0xFFFFFFFFu);
    gte.mtc2(6, 0x3C838383u);
    sky_ring_project(kRingA, kPoints, 1);

    struct TextureTile { u32 u, v; u16 clut; };
    auto tile_for = [&](u32 a, u32 b) {
        TextureTile tile{255, 0, 168};
        if (m.r16s(a + kUv) + m.r16s(b + kUv) >= 0) { tile.u = 0; tile.clut = 170; }
        if ((m.r16s(a + kUv + 2) + m.r16s(b + kUv + 2)) >> 1 >= 0) tile.clut += 4;
        else tile.v = 255;
        return tile;
    };
    auto uv = [&](u32 packet, u32 vertex, TextureTile tile) {
        m.w8(packet, m.r8(vertex + kUv) + tile.u);
        m.w8(packet + 1, m.r8(vertex + kUv + 2) + tile.v);
    };
    auto visible_quad = [&](u32 a, u32 b) {
        const u32 a0 = m.r32(a), a1 = m.r32(a + kStride), b0 = m.r32(b), b1 = m.r32(b + kStride);
        return a0 != 0xFFFFFFFFu && a1 != 0xFFFFFFFFu && b0 != 0xFFFFFFFFu && b1 != 0xFFFFFFFFu &&
            !(a0 & a1 & b0 & b1);
    };
    u32 packet = m.r32(g::kScratch + 4);
    const u32 centre_code = m.r32(kCentre);
    if (centre_code != 0xFFFFFFFFu) {
        for (u32 vertex = 0; vertex < 24; ++vertex) {
            const u32 a = kRingA + kStride * vertex, b = a + kStride;
            const u32 ca = m.r32(a), cb = m.r32(b);
            if (ca == 0xFFFFFFFFu || cb == 0xFFFFFFFFu || (centre_code & ca & cb)) continue;
            m.w32(packet, 0x09000000u | ((packet + 40) & 0xFFFFFFu));
            m.w32(packet + 4, m.r32(kCentre + 8));
            m.w32(packet + 16, m.r32(a + kColour)); m.w32(packet + 28, m.r32(b + kColour));
            m.w32(packet + 8, m.r32(kCentre + kScreen));
            m.w32(packet + 20, m.r32(a + kScreen)); m.w32(packet + 32, m.r32(b + kScreen));
            const TextureTile tile = tile_for(a, b);
            m.w16(packet + 26, tile.clut);
            m.w32(packet + 12, 0x3FF00000u | (tile.v << 8) | tile.u);
            uv(packet + 24, a, tile); uv(packet + 36, b, tile);
            packet += 40;
        }
        m.w32(g::kScratch + 4, packet);
    }
    for (s32 band = 1; band < 8; ++band) {
        const u32 old_ring = kRingA + 400 * ((band + 1) & 1), new_ring = kRingA + 400 * (band & 1);
        if (band == 7) {
            gte.ctc2(13, 1280); gte.ctc2(14, 1280); gte.ctc2(15, 1280);
            gte.ctc2(16, 2340); gte.ctc2(17, 0x09240000u); gte.ctc2(19, 2340);
            gte.mtc2(6, 0x3AA06858u);
        }
        sky_ring_project(new_ring, kPoints, band + 1);
        for (u32 vertex = 0; vertex < 24; ++vertex) {
            const u32 a = old_ring + kStride * vertex, b = new_ring + kStride * vertex;
            if (!visible_quad(a, b)) continue;
            m.w32(packet, 0x0C000000u | ((packet + 52) & 0xFFFFFFu));
            m.w32(packet + 4, m.r32(a + kColour)); m.w32(packet + 16, m.r32(a + kStride + kColour));
            m.w32(packet + 28, band < 7 ? m.r32(b + kColour) : 0);
            m.w32(packet + 40, band < 7 ? m.r32(b + kStride + kColour) : 0);
            m.w32(packet + 8, m.r32(a + kScreen)); m.w32(packet + 20, m.r32(a + kStride + kScreen));
            m.w32(packet + 32, m.r32(b + kScreen)); m.w32(packet + 44, m.r32(b + kStride + kScreen));
            const TextureTile tile = tile_for(b, b + kStride);
            m.w16(packet + 14, 0x3FF0); m.w16(packet + 26, tile.clut);
            uv(packet + 12, a, tile); uv(packet + 24, a + kStride, tile);
            uv(packet + 36, b, tile); uv(packet + 48, b + kStride, tile);
            packet += 52;
        }
    }
    m.w32(packet, 0x01000000u | ((packet + 8) & 0xFFFFFFu)); m.w32(packet + 4, 0xE1000220u);
    packet += 8; m.w32(g::kScratch + 4, packet);
    for (u32 vertex = 0; vertex < 24; ++vertex) {
        const u32 a = kRingA + kStride * vertex, b = kRingB + kStride * vertex;
        if (!visible_quad(a, b)) continue;
        m.w32(packet, 0x08000000u | ((packet + 36) & 0xFFFFFFu));
        m.w32(packet + 4, 0x3A000000u); m.w32(packet + 12, 0);
        m.w32(packet + 20, m.r32(b + kColour)); m.w32(packet + 28, m.r32(b + kStride + kColour));
        m.w32(packet + 8, m.r32(a + kScreen)); m.w32(packet + 16, m.r32(a + kStride + kScreen));
        m.w32(packet + 24, m.r32(b + kScreen)); m.w32(packet + 32, m.r32(b + kStride + kScreen));
        packet += 36;
    }
    m.w32(g::kScratch + 4, packet);
    sky_ring_project(kRingA, kPoints, 9);
    for (u32 vertex = 0; vertex < 24; ++vertex) {
        const u32 a = kRingA + kStride * vertex, b = kRingB + kStride * vertex;
        if (!visible_quad(a, b)) continue;
        for (u32 pass = 0; pass < 2; ++pass) {
            m.w32(packet, 0x09000000u | ((packet + 40) & 0xFFFFFFu));
            m.w32(packet + 4, pass == 0 ? 0xE1000240u : 0xE1000220u);
            m.w32(packet + 8, 0x3A010101u); m.w32(packet + 16, 0x00010101u);
            m.w32(packet + 24, pass == 0 ? 0x00939393u : m.r32(b + kColour));
            m.w32(packet + 32, pass == 0 ? 0x00939393u : m.r32(b + kStride + kColour));
            m.w32(packet + 12, m.r32(a + kScreen)); m.w32(packet + 20, m.r32(a + kStride + kScreen));
            m.w32(packet + 28, m.r32(b + kScreen)); m.w32(packet + 36, m.r32(b + kStride + kScreen));
            packet += 40;
        }
    }
    m.w32(g::kScratch + 4, packet);
    return 0;
}
}

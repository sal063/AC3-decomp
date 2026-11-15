#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kDialog = 0x80112DDCu;
constexpr u32 kTopLeft = 8, kTopRight = 16, kBottomLeft = 24, kBottomRight = 32;
constexpr u32 kRectWidth = 4, kRectHeight = 6;
}

u32 Game::o2_metadata_quad(u32 x, u32 y, u32 rectangle, u32 blend, u32 packet) {
    cover(0x800D1588u);
    m.w8(packet + 3, 9); m.w8(packet + 7, 0x2E);
    m.w16(packet + kTopLeft, x); m.w16(packet + kTopLeft + 2, y);
    const u32 width = m.r16(rectangle + kRectWidth);
    m.w16(packet + kBottomLeft, x); m.w16(packet + kTopRight + 2, y);
    m.w16(packet + kTopRight, x - 1 + width);
    m.w16(packet + kBottomLeft + 2, y - 1 + m.r16(rectangle + kRectHeight));
    m.w16(packet + kBottomRight, x - 1 + m.r16(rectangle + kRectWidth));
    m.w16(packet + kBottomRight + 2, y - 1 + m.r16(rectangle + kRectHeight));
    auto texture_u = [&](bool right) {
        const s32 base = (m.r16s(rectangle) % 64) * 4;
        return static_cast<u32>(base) + (right ? m.r8(rectangle + kRectWidth) - 1u : 0u);
    };
    auto texture_v = [&](bool bottom) {
        const s32 base = m.r16s(rectangle + 2) % 256;
        return static_cast<u32>(base) + (bottom ? m.r8(rectangle + kRectHeight) - 1u : 0u);
    };
    for (u32 vertex = 0; vertex < 3; ++vertex) {
        m.w8(packet + 12 + 8 * vertex, texture_u(vertex == 1));
        m.w8(packet + 13 + 8 * vertex, texture_v(vertex == 2));
    }
    m.w8(packet + 36, texture_u(true));
    const u32 lastV = texture_v(true);
    m.w8(packet + 4, 128); m.w8(packet + 5, 128); m.w8(packet + 6, 128);
    m.w8(packet + 37, lastV);
    const u32 textureY = m.r16(rectangle + 2), textureX = m.r16(rectangle);
    m.w16(packet + 14, 0x55A);
    m.w16(packet + 22, ((blend & 3) << 5) | ((textureY & 0x100) >> 4)
        | ((textureX & 0x3FF) >> 6) | ((textureY & 0x200) << 2));
    const u32 menu = m.r32(kMenu), table = m.r32(menu + 8);
    m.w32(packet, (m.r32(packet) & 0xFF000000u) | (m.r32(table + 4) & 0x00FFFFFFu));
    const u32 slot = m.r32(menu + 8) + 4;
    m.w32(slot, (m.r32(slot) & 0xFF000000u) | (packet & 0x00FFFFFFu));
    return packet + 40;
}

void Game::o2_metadata_icon_pair(u32 inheritedOffset) {
    cover(0x800D47DCu);
    Frame frame(*this, 48);
    const u32 dialog = m.r32(kDialog), preset = dialog + 136;
    const u32 choice = m.r8(preset + 3);
    u32 offset = choice >= 1 && choice <= 5 ? inheritedOffset : 0;
    if (choice == 0 || choice == 6 || choice == 8) offset = m.r8(dialog + 160);
    constexpr u32 kIconRects = 0x8010A880u;
    const u32 current = m.r32(kDialog);
    const u32 blend = ((m.r8(current + 30) & 31) == 4) ? 2 : 1;
    const u32 packet = m.r32(current + 8);
    m.w32(frame.at(16), packet);
    const u32 next = o2_metadata_quad(40, 335, kIconRects + 8 * (m.r8(preset + 3) + offset), blend, packet);
    m.w32(m.r32(kDialog) + 8, next);
    m.w32(frame.at(16), next);
    const u32 last = o2_metadata_quad(122, 335, kIconRects + 8 * (m.r8(preset + 3) + 1 - offset), blend, next);
    m.w32(m.r32(kDialog) + 8, last);
}
}

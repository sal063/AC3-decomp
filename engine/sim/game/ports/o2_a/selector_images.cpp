#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kSelector = 0x80112DD0u, kPanel = 0x80112DB8u;
constexpr u32 kImageIds = 516, kImageCount = 596, kImageTable = 468, kPaletteTable = 476;
constexpr u32 kImageX = 960, kImageY = 340, kPaletteX = 384;
}

void Game::o2_selector_image(u32 group, u32 index) {
    cover(0x800CF504u);
    Frame frame(*this, 32);
    const u32 id = m.r8(m.r32(kSelector) + kImageIds + 4 * group + index);
    const u32 tim = m.r32(m.r32(m.r32(kMenu) + kImageTable) + 4 * id);
    m.w32(frame.at(16), 6);
    o2_upload_tim_positioned(tim, kImageX, kImageY, kPaletteX, 6);
}

void Game::o2_selector_image_column(s32 column) {
    cover(0x800CF564u);
    Frame frame(*this, 48);
    const s32 count = m.r32s(m.r32(kSelector) + kImageCount), wholeColumns = count / 4;
    const s32 rows = column < wholeColumns ? 4 : count - wholeColumns * 4;
    for (s32 row = 0; row < rows; ++row) {
        const u32 index = static_cast<u32>(column) * 4 + static_cast<u32>(row);
        const u32 selector = m.r32(kSelector);
        if (static_cast<s32>(index) < m.r32s(selector + kImageCount)) {
            const u32 id = m.r8(selector + kImageIds + index);
            const u32 tim = m.r32(m.r32(m.r32(kMenu) + kImageTable) + 4 * id);
            m.w32(frame.at(16), 6);
            o2_upload_tim_positioned(tim, kImageX, kImageY + 18 * static_cast<u32>(row), kPaletteX, 6);
        }
    }
}

void Game::o2_selector_palette(u32 index) {
    cover(0x800CF640u);
    Frame frame(*this, 32);
    const u32 selector = m.r32(kSelector), relative = index + 4 * m.r8(selector + 2);
    if (static_cast<s32>(relative) < m.r32s(selector + kImageCount)) {
        const u32 id = m.r8(selector + kImageIds + relative);
        const u32 tim = m.r32(m.r32(m.r32(kMenu) + kPaletteTable) + 4 * id);
        m.w32(frame.at(16), 1);
        o2_upload_tim_positioned(tim, kImageX, 256, 768, 1);
    }
}

void Game::o2_panel_palette(u32 index) {
    cover(0x800D0970u);
    Frame frame(*this, 32);
    const u32 panel = m.r32(kPanel), relative = index + 4 * m.r8(panel + 2);
    const u32 aircraft = m.r8(panel + 4 + relative);
    const u32 palette = m.r8(0x80109E28u + 8 * aircraft + 5);
    const u32 tim = m.r32(m.r32(m.r32(kMenu) + kPaletteTable) + 4 * palette);
    m.w32(frame.at(16), 1);
    o2_upload_tim_positioned(tim, kImageX, 256, 768, 1);
}

void Game::o2_panel_image_column(s32 column) {
    cover(0x800D0830u);
    Frame frame(*this, 64);
    const s32 count = m.r32s(m.r32(kPanel) + 184), fullColumns = count / 4;
    const s32 rows = column < fullColumns ? 4 : count - fullColumns * 4;
    for (s32 row = 0; row < rows; ++row) {
        const u32 index = static_cast<u32>(column) * 4 + static_cast<u32>(row);
        u32 aircraft = m.r8(m.r32(kPanel) + 4 + index);
        u32 tim = m.r32(m.r32(m.r32(kMenu) + kImageTable) + 4 * aircraft - 104);
        m.w32(frame.at(16), 6);
        o2_upload_tim_positioned(tim, 960, 340 + 18 * static_cast<u32>(row), 384, 6);
        aircraft = m.r8(m.r32(kPanel) + 4 + index);
        const u32 secondTable = m.r32(m.r32(kMenu) + 472);
        const u32 palette = m.r8(0x80109E28u + 8 * aircraft + 5);
        tim = m.r32(secondTable + 4 * palette);
        m.w32(frame.at(16), 6);
        o2_upload_tim_positioned(tim, 992, 340 + 18 * static_cast<u32>(row), 384, 6);
    }
}
}

#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDialog = 0x80112DDCu, kScreens = 0x80112DC0u, kMenu = 0x80112DC4u;
constexpr u32 kLabelRects = 0x8010A7C0u, kIconRects = 0x8010A880u;
constexpr u32 kLabelSelection = 31, kIconSelection = 30;
struct LabelPlacement { u16 index, x, y; };
u32 selection(Memory& m, u32 field) { return 1u << (m.r8(m.r32(kDialog) + field) & 31); }
}

void Game::o2_metadata_label_at(Frame& frame, u32 labels, u32 mask, u32 index, u32 bit, u32 x, u32 y) {
    const u32 label = o2_metadata_label(m.r8(labels + index));
    const u32 packet = m.r32(m.r32(kDialog) + 8);
    m.w32(frame.at(16), packet);
    const u32 next = o2_metadata_quad(x, y, kLabelRects + 8 * label, 1 + ((mask >> bit) & 1), packet);
    m.w32(m.r32(kDialog) + 8, next);
}

void Game::o2_metadata_labels_full() {
    cover(0x800D3388u);
    Frame frame(*this, 48);
    const u32 dialog = m.r32(kDialog), labels = dialog + 36, palettes = dialog + 132;
    const bool markLabels = m.r8(dialog + 35) != 0;
    auto flags = [&] { return m.r32(m.r32(kScreens) + 92); };
    for (u32 i = 10; i < 32; ++i) m.w8(flags() + i, 0);
    if (markLabels) {
        for (u32 i = 0; i < 15; ++i) {
            const u32 table = flags();
            m.w8(table + 10 + m.r8(labels + i), 1);
        }
        for (u32 i = 0; i < 4; ++i) if (m.r8(palettes + i) < 8) {
            u32 table = flags(); m.w8(table + 10 + m.r8(palettes + i), 1);
            table = flags(); m.w8(table + 11 + m.r8(palettes + i), 1);
        }
    }
    const u32 mask = selection(m, kLabelSelection);
    constexpr LabelPlacement layout[] = {
        {0,40,149}, {1,40,183}, {2,40,216}, {3,40,249}, {4,40,282}, {5,40,315}, {6,40,348},
        {7,252,138}, {8,252,169}, {9,252,199}, {10,252,229}, {11,252,259}, {12,252,289},
        {13,252,319}, {14,252,349}
    };
    for (u32 i = 0; i < 15; ++i)
        o2_metadata_label_at(frame, labels, mask, layout[i].index, i + 1, layout[i].x, layout[i].y);
}

void Game::o2_metadata_labels_first() {
    cover(0x800D38ECu);
    Frame frame(*this, 48);
    const u32 dialog = m.r32(kDialog), labels = dialog + 52;
    const u32 mask = selection(m, kLabelSelection);
    for (u32 i = 0; i < 10; ++i) {
        const u32 column = i / 5, row = i % 5, index = column ? row + 8 : row + 1;
        o2_metadata_label_at(frame, labels, mask, index, i + 1, column ? 252 : 40, 183 + 33 * row);
    }
}

void Game::o2_metadata_labels_second() {
    cover(0x800D3BE0u);
    Frame frame(*this, 48);
    const u32 dialog = m.r32(kDialog), labels = dialog + 68;
    const u32 mask = selection(m, kLabelSelection);
    constexpr LabelPlacement layout[] = {
        {0,40,160}, {1,40,193}, {2,40,226}, {3,40,259}, {4,40,292}, {5,40,325},
        {7,252,149}, {8,252,180}, {9,252,211}, {10,252,242}, {11,252,273}, {12,252,304}, {14,252,335}
    };
    for (u32 i = 0; i < 13; ++i)
        o2_metadata_label_at(frame, labels, mask, layout[i].index, i + 1, layout[i].x, layout[i].y);
}

void Game::o2_metadata_labels_third() {
    cover(0x800D3FA0u);
    Frame frame(*this, 48);
    const u32 dialog = m.r32(kDialog), labels = dialog + 84;
    const u32 mask = selection(m, kLabelSelection);
    constexpr LabelPlacement layout[] = {
        {1,41,150}, {8,41,181}, {0,41,238}, {7,41,268}, {14,41,340},
        {2,144,145}, {3,144,177}, {4,144,208}, {5,144,241},
        {11,252,150}, {12,252,181}, {10,252,238}, {9,252,268}
    };
    for (u32 i = 0; i < 13; ++i)
        o2_metadata_label_at(frame, labels, mask, layout[i].index, i + 1, layout[i].x, layout[i].y);
    if (o2_fade_state(6) == 2) o2_text_ordering_table(m.r32(m.r32(kMenu) + 8));
    o2_text_ordering_table(0xFFFFFFFFu);
}

void Game::o2_metadata_icons_at(Frame& frame, u32 palettes, u32 options, u32 entry) {
    for (u32 i = 0; i < 4; ++i) {
        const u32 preset = m.r8(palettes + i);
        const u32 offset = preset == 0 || preset == 6 || preset == 8 ? m.r8(options + i) : 0;
        m.w32(frame.at(32 + 4 * i), offset);
    }
    const u32 mask = selection(m, kIconSelection);
    o2_entry_position_read(2, entry, frame.at(24));
    struct IconPlacement { s16 x; u16 y, pair; bool right; };
    constexpr IconPlacement layout[] = {
        {45,155,0,false}, {45,304,0,true}, {-1,231,1,false}, {82,231,1,true},
        {235,155,2,false}, {235,304,2,true}, {188,231,3,false}, {271,231,3,true}
    };
    u32 packet = m.r32(m.r32(kDialog) + 8);
    for (const auto& item : layout) {
        const u32 x = static_cast<u32>(static_cast<s32>(static_cast<s16>(m.r16(frame.at(24)) + item.x)));
        const u32 preset = m.r8(palettes + item.pair), offset = m.r32(frame.at(32 + 4 * item.pair));
        const u32 icon = item.right ? preset + 1 - offset : preset + offset;
        m.w32(frame.at(16), packet);
        packet = o2_metadata_quad(x, item.y, kIconRects + 8 * icon, 1 + ((mask >> (item.pair + 1)) & 1), packet);
        m.w32(m.r32(kDialog) + 8, packet);
    }
}

void Game::o2_metadata_icons_full() {
    cover(0x800D4430u);
    Frame frame(*this, 72);
    const u32 dialog = m.r32(kDialog);
    o2_metadata_icons_at(frame, dialog + 132, dialog + 156, 9);
}

void Game::o2_metadata_icons_third() {
    cover(0x800D48F8u);
    Frame frame(*this, 72);
    const u32 dialog = m.r32(kDialog);
    o2_metadata_icons_at(frame, dialog + 144, dialog + 168, 11);
}

void Game::o2_metadata_labels_draw() {
    cover(0x800D4394u);
    Frame frame(*this, 24);
    if (m.r16s(m.r32(kDialog) + 12) == 0) o2_metadata_labels_full();
    if (m.r16s(m.r32(kDialog) + 12) == 1) o2_metadata_labels_first();
    if (m.r16s(m.r32(kDialog) + 12) == 2) o2_metadata_labels_second();
    if (m.r16s(m.r32(kDialog) + 12) == 3) o2_metadata_labels_third();
}

void Game::o2_metadata_icons_draw(u32 inheritedOffset) {
    cover(0x800D4CA4u);
    Frame frame(*this, 24);
    if (m.r16s(m.r32(kDialog) + 12) == 0) o2_metadata_icons_full();
    if (m.r16s(m.r32(kDialog) + 12) == 1) o2_metadata_icon_pair(inheritedOffset);
    if (m.r16s(m.r32(kDialog) + 12) == 3) o2_metadata_icons_third();
}
}

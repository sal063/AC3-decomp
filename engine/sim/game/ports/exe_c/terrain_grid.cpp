#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include <algorithm>

namespace ac3::sim {
u32 Game::terrain_outline_grid(u32 outline) {
    cover(0x80060CB8u);
    Frame frame(*this, 0x278);
    const u32 work = frame.at(0x10), rows = work + 128, side = work + 256;
    const u32 bounds = work + 0x1F0, sample = work + 0x238;
    const s32 count = m.r32s(outline + 24);
    auto clearBounds = [&]() { for (u32 field = 72; field < 96; field += 4) m.w32(outline + field, 0); };
    if (!count) {
        clearBounds();
        const u32 staleRow = m.r32(rows);
        m.w32(outline + 96, staleRow);
        return staleRow;
    }
    for (u32 row = 0; row < 32; ++row) { m.w32(rows + 4 * row, 0); m.w32(work + 4 * row, 0xFFFFFFFFu); }
    m.w16(bounds + 2, 32767); m.w16(bounds, 32767);
    m.w16(bounds + 6, 0x8000); m.w16(bounds + 4, 0x8000);
    const u32 polygon = outline + 28;
    for (s32 vertex = 0; vertex < count; ++vertex) {
        const u32 point = polygon + 4 * static_cast<u32>(vertex);
        const s32 x = m.r16s(point), y = m.r16s(point + 2);
        if (m.r16s(bounds + 4) < x) m.w16(bounds + 4, static_cast<u32>(x));
        if (m.r16s(bounds + 6) < y) m.w16(bounds + 6, static_cast<u32>(y));
        if (x < m.r16s(bounds)) m.w16(bounds, static_cast<u32>(x));
        if (y < m.r16s(bounds + 2)) m.w16(bounds + 2, static_cast<u32>(y));
    }
    m.w16(bounds + 8, m.r16(bounds) & 0xF800u);
    m.w16(bounds + 12, (m.r16(bounds + 4) + 2047u) & 0xF800u);
    const s32 columns = (m.r16s(bounds + 12) - m.r16s(bounds + 8)) >> 11;
    m.w16(bounds + 10, m.r16(bounds + 2) & 0xF800u);
    m.w16(bounds + 14, (m.r16(bounds + 6) + 2047u) & 0xF800u);
    const s32 rowCount = (m.r16s(bounds + 14) - m.r16s(bounds + 10)) >> 11;
    if (columns < 0 || rowCount < 0 || columns >= 32 || rowCount >= 32) {
        clearBounds(); return 0;
    }
    for (s32 edge = 0; edge < count; ++edge) {
        const u32 a = polygon + 4 * static_cast<u32>(edge), b = polygon + 4 * static_cast<u32>((edge + 1) % count);
        const s32 x0 = m.r16s(a), x1 = m.r16s(b), y0 = m.r16s(a + 2), y1 = m.r16s(b + 2);
        const s32 x = std::min(x0, x1) & -2048, y = std::min(y0, y1) & -2048;
        const s32 nx = (((std::max(x0, x1) + 2047) & -2048) - x) >> 11;
        const s32 ny = (((std::max(y0, y1) + 2047) & -2048) - y) >> 11;
        const u32 data = work + 4 * static_cast<u32>(edge);
        m.w16(data + 0x200, static_cast<u32>(x)); m.w16(data + 0x202, static_cast<u32>(y));
        const s32 firstColumn = (x - m.r16s(bounds + 8)) >> 11, firstRow = (y - m.r16s(bounds + 10)) >> 11;
        m.w32(data + 0x1B8, static_cast<u32>(firstColumn)); m.w32(data + 0x1D4, static_cast<u32>(firstRow));
        m.w32(data + 0x180, static_cast<u32>(firstColumn + nx)); m.w32(data + 0x19C, static_cast<u32>(firstRow + ny));
    }
    auto bit = [](s32 column) { return 1u << (static_cast<u32>(column) & 31); };
    auto rowAddress = [&](u32 base, s32 row) { return base + 4 * static_cast<u32>(row); };
    auto mark = [&](s32 row, u32 mask) { const u32 address = rowAddress(rows, row); m.w32(address, m.r32(address) | mask); };
    for (s32 edge = 0; edge < count; ++edge) {
        const u32 data = work + 4 * static_cast<u32>(edge);
        gte.mtc2(12, m.r32(polygon + 4 * static_cast<u32>(edge)));
        gte.mtc2(13, m.r32(polygon + 4 * static_cast<u32>((edge + 1) % count)));
        const s32 left = m.r32s(data + 0x1B8), right = m.r32s(data + 0x180);
        const s32 top = m.r32s(data + 0x1D4), bottom = m.r32s(data + 0x19C);
        m.w16(sample + 2, m.r16(data + 0x202));
        for (s32 row = top; row <= bottom; ++row) {
            m.w16(sample, m.r16(data + 0x200)); gte.mtc2(14, m.r32(sample));
            u32 positive = 0;
            for (s32 column = left; column <= right; ++column) {
                gte.op(0x1400006u);
                m.w16(sample, m.r16(sample) + 2048u);
                if (static_cast<s32>(gte.mfc2(24)) > 0) positive |= bit(column);
                gte.mtc2(14, m.r32(sample));
            }
            m.w32(rowAddress(side, row), ~positive);
            m.w16(sample + 2, m.r16(sample + 2) + 2048u);
        }
        for (s32 row = top + 1; row < bottom; ++row) {
            const u32 signs = m.r32(rowAddress(side, row));
            for (s32 column = left + 1; column < right; ++column) {
                if (((signs >> ((static_cast<u32>(column) - 1u) & 31)) ^ (signs >> (static_cast<u32>(column) & 31))) & 1u) {
                    mark(row - 1, bit(column - 1)); mark(row, bit(column - 1)); break;
                }
            }
        }
        for (s32 column = left + 1; column < right; ++column) {
            u32 previous = m.r32(rowAddress(side, top));
            for (s32 row = top + 1; row < bottom; ++row) {
                const u32 current = m.r32(rowAddress(side, row));
                if ((previous ^ current) & bit(column)) { mark(row - 1, 3u << ((static_cast<u32>(column) - 1u) & 31)); break; }
                previous = current;
            }
        }
        if (right - left < 2) mark(bottom - 1, bit(left));
        else {
            const u32 previous = m.r32(rowAddress(side, bottom - 1));
            for (s32 column = left + 1; column < right; ++column)
                if ((previous ^ m.r32(rowAddress(side, bottom))) & bit(column)) mark(bottom - 1, 3u << ((static_cast<u32>(column) - 1u) & 31));
        }
        if (bottom - top < 2) mark(top, bit(right - 1));
        else {
            for (s32 row = top + 1; row < bottom; ++row) {
                const u32 signs = m.r32(rowAddress(side, row));
                if (((signs >> ((static_cast<u32>(right) - 1u) & 31)) ^ (signs >> (static_cast<u32>(right) & 31))) & 1u) {
                    mark(row - 1, bit(right - 1)); mark(row, bit(right - 1));
                }
            }
        }
    }
    for (s32 row = 0; row < rowCount; ++row) {
        u32 prefix = 1;
        for (s32 column = 0; column < columns; ++column) {
            if (prefix & m.r32(rowAddress(rows, row))) {
                m.w32(rowAddress(side, row), static_cast<u32>(static_cast<s32>(prefix) >> 1)); break;
            }
            prefix |= prefix << 1;
        }
    }
    for (s32 row = 0; row < rowCount; ++row) {
        u32 suffix = 0x80000000u;
        for (s32 column = 31; column >= 0; --column) {
            if (suffix & m.r32(rowAddress(rows, row))) {
                m.w32(rowAddress(rows, row), ~((suffix << 1) | m.r32(rowAddress(side, row)))); break;
            }
            suffix |= static_cast<u32>(static_cast<s32>(suffix) >> 1);
        }
    }
    for (u32 field = 0; field < 3; ++field) m.w32(outline + 72 + 4 * field, static_cast<u32>(m.r16s(bounds + 8 + 2 * field) >> 11));
    m.w32(outline + 88, static_cast<u32>(columns)); m.w32(outline + 92, static_cast<u32>(rowCount));
    u32 result = static_cast<u32>(m.r16s(bounds + 14) >> 11); m.w32(outline + 84, result);
    for (s32 row = 0; row < rowCount; ++row) {
        m.w32(outline + 96 + 4 * static_cast<u32>(row), m.r32(rowAddress(rows, row)));
        result = row + 1 < rowCount ? 1u : 0u;
    }
    return result;
}
}

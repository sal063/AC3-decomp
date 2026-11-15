#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kImage = 0x80112DFCu;
constexpr u32 kQuadCursor = 332, kPaletteRect = 336, kImageRect = 344;
constexpr u32 kUvRects = 352, kScreenRects = 384, kQuadCount = 416;
constexpr u32 kSchedule = 420, kScheduleEntry = 424, kTiles = 428;
constexpr u32 kColumns = 432, kRows = 434, kTileWidth = 436, kTileHeight = 438;
constexpr u32 kTick = 440, kUploadCursor = 52672, kExtraSchedule = 52676, kFrameCount = 52680;
constexpr u32 kQuadBytes = 40, kUploadBytes = 52;
void copy_rect(Memory& m, u32 dst, u32 src) {
    const u32 xy = m.r32(src), wh = m.r32(src + 4);
    m.w32(dst, xy); m.w32(dst + 4, wh);
}
void link_uploads(Memory& m, u32 ot, u32 first, u32 end) {
    if (first == end) return;
    const u32 last = end - kUploadBytes;
    m.w8(last, m.r8(ot)); m.w8(last + 1, m.r8(ot + 1)); m.w8(last + 2, m.r8(ot + 2));
    m.w8(ot, first); m.w8(ot + 1, first >> 8); m.w8(ot + 2, first >> 16);
}
}

void Game::o2_image_quads_init() {
    cover(0x800EC340u);
    for (u32 buffer = 0; buffer < 2; ++buffer) {
        for (s32 i = 0; i < m.r32s(m.r32(kImage) + kQuadCount); ++i) {
            const u32 image = m.r32(kImage);
            const u32 index = static_cast<u32>(i);
            const u32 packet = image + 12 + buffer * 160 + index * kQuadBytes;
            const u32 rect = image + kScreenRects + 8 * index;
            const u32 uv = image + kUvRects + 8 * index;
            m.w8(packet + 3, 9); m.w8(packet + 7, 0x2C);
            for (u32 point = 0; point < 4; ++point) {
                m.w16(packet + 8 + 8 * point, m.r16(rect) + ((point & 1) ? m.r16(rect + 4) : 0));
                m.w16(packet + 10 + 8 * point, m.r16(rect + 2) + ((point & 2) ? m.r16(rect + 6) : 0));
                m.w8(packet + 12 + 8 * point, m.r8(uv) + ((point & 1) ? m.r8(uv + 4) : 0));
                m.w8(packet + 13 + 8 * point, m.r8(uv + 2) + ((point & 2) ? m.r8(uv + 6) : 0));
            }
            for (u32 channel = 0; channel < 3; ++channel) m.w8(packet + 4 + channel, 0x80);
            m.w16(packet + 22, 0x9E);
            const u32 palette = m.r32(kImage) + kPaletteRect;
            m.w16(packet + 14, (m.r16(palette + 2) << 6) | ((m.r16(palette) >> 4) & 0x3F));
        }
    }
}

void Game::o2_image_layout(u32 imageRect, u32 uvRects, u32 paletteRect, u32 screenRects, s32 count) {
    cover(0x800EC4ECu);
    Frame frame(*this, 0x20);
    const u32 image = m.r32(kImage);
    copy_rect(m, image + kPaletteRect, paletteRect);
    copy_rect(m, image + kImageRect, imageRect);
    m.w32(image + kQuadCount, static_cast<u32>(count));
    for (s32 i = 0; i < count; ++i) {
        const u32 offset = 8 * static_cast<u32>(i);
        copy_rect(m, m.r32(kImage) + kUvRects + offset, uvRects + offset);
        copy_rect(m, m.r32(kImage) + kScreenRects + offset, screenRects + offset);
    }
    const u32 configured = m.r32(kImage);
    m.w32(configured, m.r32(m.r32(g::kScene) + 4));
    m.w32(configured + kScheduleEntry, m.r32(configured + kSchedule) + 4);
    hw->clear_image(imageRect, 0, 0, 0);
    hw->load_image(paletteRect, m.r32(m.r32(kImage) + kTiles) + 4);
    const u32 ready = m.r32(kImage);
    m.w16(ready + kColumns, static_cast<u32>(m.r16s(imageRect + 4) / 8));
    m.w16(ready + kRows, static_cast<u32>(m.r16s(imageRect + 6) / 8));
    m.w16(ready + kTileWidth, 4); m.w16(ready + kTileHeight, 4);
    m.w32(ready + kTick, 0);
    m.w16(ready + 8, 4); m.w16(ready + 10, 0);
    o2_image_quads_init();
    const u32 state = m.r32(kImage);
    m.w32(state + kExtraSchedule, 0); m.w32(state + kFrameCount, 0);
}

u32 Game::o2_image_open(u32 imageRect, u32 uvRects, u32 paletteRect, u32 screenRects, u32 pack, s32 count) {
    cover(0x800EC6B4u);
    Frame frame(*this, 0x38);
    m.w32(kImage, mem_alloc(0xCDCC));
    const u32 entries = ld_relocate_dir(pack);
    const u32 image = m.r32(kImage);
    m.w32(image + kSchedule, m.r32(entries));
    m.w32(image + kTiles, m.r32(entries + 8));
    for (s32 i = 0; i < m.r32s(m.r32(image + kSchedule)); ++i) {
        const u32 pointer = m.r32(image + kSchedule) + 12 + 12 * static_cast<u32>(i);
        m.w32(pointer, m.r32(pointer) + m.r32(entries + 4));
    }
    const u32 tiles = m.r32(m.r32(kImage) + kTiles);
    for (s32 i = 0; i < m.r32s(tiles); ++i) {
        const u32 pointer = tiles + 516 + 4 * static_cast<u32>(i);
        m.w32(pointer, m.r32(pointer) + m.r32(entries + 12));
    }
    m.w32(frame.at(16), static_cast<u32>(count));
    o2_image_layout(imageRect, uvRects, paletteRect, screenRects, count);
    return m.r32(m.r32(m.r32(kImage) + kSchedule));
}

u32 Game::o2_image_buffers() {
    cover(0x800EC824u);
    const u32 image = m.r32(kImage), scene = m.r32(g::kScene);
    m.w32(image + kQuadCursor, image + 12 + 160 * m.r32(scene + 4));
    const u32 uploads = image + 448 + 26112 * m.r32(scene + 4);
    m.w32(image + kUploadCursor, uploads);
    return uploads;
}

u32 Game::o2_image_upload_due(u32 ot) {
    cover(0x800EC880u);
    Frame frame(*this, 24);
    const u32 image = m.r32(kImage), schedule = m.r32(image + kSchedule);
    if (!m.r32(schedule) && m.r32s(m.r32(image + kExtraSchedule)) >= 31) return 1;
    if (m.r16s(image + 10) >= m.r32s(schedule)) return 1;
    const s32 divisor = m.r16s(image + 8), ticks = m.r32s(image + kTick);
    if (!divisor) unported(0x800EC914u, "zero image-update divisor");
    if (ticks == static_cast<s32>(0x80000000u) && divisor == -1)
        unported(0x800EC92Cu, "overflowing image-update division");
    const s32 elapsed = ticks / divisor;
    m.w32(image + kScheduleEntry, schedule + 4 + 12 * static_cast<u32>(m.r16s(image + 10)));
    const u32 first = m.r32(image + kUploadCursor);
    u32 cursor = first;
    while (elapsed >= m.r32s(m.r32(image + kScheduleEntry) + 4)) {
        const u32 update = m.r32(image + kScheduleEntry);
        const s32 count = m.r32s(update);
        u32 list = m.r32(update + 8);
        for (s32 i = 0; i < count; ++i, list += 4) {
            const s32 tile = m.r16s(list + 2), columns = m.r16s(image + kColumns);
            if (!columns) unported(0x800EC9C4u, "zero image tile columns");
            const u32 x = static_cast<u32>(m.r16s(image + kImageRect) + 4 * (tile % columns));
            const u32 y = static_cast<u32>(m.r16s(image + kImageRect + 2) + 8 * (tile / columns));
            m.w16(cursor + 8, x); m.w16(cursor + 10, y);
            m.w16(cursor + kUploadBytes + 8, x); m.w16(cursor + kUploadBytes + 10, y + 4);
            const u32 tileData = m.r32(m.r32(m.r32(kImage) + kTiles) + 516 + 4 * static_cast<u32>(m.r16s(list)));
            for (u32 half = 0; half < 2; ++half) {
                m.w32(cursor, 0x0C000000u | (((cursor << 8) + 0x3400u) >> 8));
                m.w32(cursor + 4, 0xA0000000u);
                m.w32(cursor + 12, 0x00040004u);
                m.w32(cursor + 48, 0x01000000u);
                for (u32 word = 0; word < 8; ++word)
                    m.w32(cursor + 16 + 4 * word, m.r32(tileData + half * 32 + 4 * word));
                cursor += kUploadBytes;
            }
        }
        m.w16(image + 10, m.r16(image + 10) + 1);
        m.w32(image + kScheduleEntry, m.r32(image + kSchedule) + 4 + 12 * static_cast<u32>(m.r16s(image + 10)));
        m.w32(image, 1 - m.r32(image));
        if (m.r16s(image + 10) >= m.r32s(m.r32(image + kSchedule))) {
            link_uploads(m, ot, first, cursor);
            if (cursor != first) m.w32(image + kUploadCursor, cursor);
            return 1;
        }
    }
    link_uploads(m, ot, first, cursor);
    if (cursor != first) m.w32(image + kUploadCursor, cursor);
    return 0;
}

u32 Game::o2_image_draw(u32 ot) {
    cover(0x800ECBECu);
    const u32 image = m.r32(kImage);
    u32 result = m.r32(image + kQuadCount);
    for (s32 i = 0; i < m.r32s(image + kQuadCount); ++i) {
        const u32 packet = m.r32(image + kQuadCursor);
        ot_link(ot, packet);
        result = m.r32(image + kQuadCursor) + kQuadBytes;
        m.w32(image + kQuadCursor, result);
    }
    return result;
}

u32 Game::o2_image_frame(u32 ot, u32 progress) {
    cover(0x800ECC70u);
    Frame frame(*this, 32);
    const u32 image = m.r32(kImage);
    m.w32(image + kQuadCursor, image + 12 + 160 * m.r32(m.r32(g::kScene) + 4));
    o2_image_buffers();
    const u32 done = o2_image_upload_due(ot);
    o2_image_draw(ot);
    const u32 updated = m.r32(kImage);
    m.w32(updated + kTick, m.r32(updated + kTick) + 1);
    m.w32(progress, static_cast<u32>(m.r16s(updated + 10)));
    const u32 state = m.r32(kImage);
    m.w32(state + kFrameCount, m.r32(state + kFrameCount) + 1);
    return done;
}
}

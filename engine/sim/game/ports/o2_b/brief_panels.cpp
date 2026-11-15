#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kBriefing = 0x80112DB0u, kMenu = 0x80112DC4u;
constexpr u32 kPanelTable = 1324, kPanelBytes = 160, kPanelPackets = 44;
constexpr u32 kKind = 0, kBegin = 2, kMiddle = 4, kEnd = 6;
constexpr u32 kStartRect = 8, kEndRect = 16;
enum PanelKind : s32 { kHidden, kHorizontalThenVertical, kStatic,
                      kInterpolateRect, kVerticalThenHorizontal, kFlipbook };
void quad_rect(Memory& m, u32 packet, s32 x, s32 y, s32 w, s32 h) {
    for (u32 point = 0; point < 4; ++point) {
        m.w16(packet + 8 + 8 * point, static_cast<u32>(x + ((point & 1) ? w : 0)));
        m.w16(packet + 10 + 8 * point, static_cast<u32>(y + ((point & 2) ? h : 0)));
    }
}
}

u32 Game::o2_brief_panels(u32 previousResult) {
    cover(0x800EF4F4u);
    Frame frame(*this, 32);
    const u32 ot = m.r32(m.r32(kMenu) + 8);
    for (u32 index = 0; index < 16; ++index) {
        const u32 briefing = m.r32(kBriefing);
        const u32 record = briefing + kPanelTable + kPanelBytes * index;
        const s32 kind = m.r16s(record + kKind);
        if (kind <= kHidden || kind > kFlipbook) continue;
        const u32 packet = briefing + kPanelPackets + 640 * m.r32(m.r32(g::kScene) + 4) + 40 * index;
        auto rect = [&](u32 offset, u32 field) { return static_cast<s32>(m.r16(record + offset + 2 * field)); };
        auto interpolate = [&](u32 field, s32 now, s32 from, s32 until) {
            const s32 start = m.r16s(record + kStartRect + 2 * field);
            const s32 finish = m.r16s(record + kEndRect + 2 * field);
            const s32 product = (finish - start) * (now - from);
            const s32 divisor = until - from;
            if (!divisor) {
                unported(0x800EF4F4u, "zero panel interpolation interval");
                return static_cast<s32>(m.r16(record + kStartRect + 2 * field)) + (product >= 0 ? -1 : 1);
            }
            if (product == static_cast<s32>(0x80000000u) && divisor == -1) {
                unported(0x800EF4F4u, "overflowing panel interpolation");
                return static_cast<s32>(m.r16(record + kStartRect + 2 * field)) + product;
            }
            return static_cast<s32>(m.r16(record + kStartRect + 2 * field)) + product / divisor;
        };
        if (kind == kFlipbook) {
            const s32 delay = m.r16s(record + 26), elapsed = m.r16s(record + 30);
            if (elapsed < delay) m.w16(record + 30, m.r16(record + 30) + 1);
            else {
                m.w16(record + 28, m.r16(record + 28) + 1);
                const s32 frames = m.r16s(record + 24);
                m.w16(record + 30, 0);
                if (m.r16s(record + 28) >= frames) m.w16(record + 28, 0);
            }
            const u32 updated = m.r32(kBriefing) + kPanelTable + kPanelBytes * index;
            const u32 cel = updated + 32 + 16 * static_cast<u32>(m.r16s(updated + 28));
            m.w16(packet + 22, m.r16(cel + 8)); m.w16(packet + 14, m.r16(cel + 10));
            quad_rect(m, packet, m.r16(updated + kStartRect), m.r16(updated + kStartRect + 2),
                      m.r16(updated + kStartRect + 4), m.r16(updated + kStartRect + 6));
            for (u32 point = 0; point < 4; ++point) {
                m.w8(packet + 12 + 8 * point, m.r8(cel) + ((point & 1) ? m.r8(cel + 4) - 1u : 0u));
                m.w8(packet + 13 + 8 * point, m.r8(cel + 2) + ((point & 2) ? m.r8(cel + 6) - 1u : 0u));
            }
            for (u32 channel = 0; channel < 3; ++channel) m.w8(packet + 4 + channel, m.r8(cel + 12 + channel));
            ot_link(ot, packet);
            continue;
        }
        if (kind == kStatic) {
            quad_rect(m, packet, rect(kEndRect, 0), rect(kEndRect, 1), rect(kEndRect, 2), rect(kEndRect, 3));
            ot_link(ot, packet);
            continue;
        }
        const s32 now = m.r32s(m.r32(kBriefing) + 16);
        const s32 begin = m.r16s(record + kBegin), middle = m.r16s(record + kMiddle), end = m.r16s(record + kEnd);
        if (now < begin) continue;
        if (now >= end) {
            quad_rect(m, packet, rect(kEndRect, 0), rect(kEndRect, 1), rect(kEndRect, 2), rect(kEndRect, 3));
            ot_link(ot, packet);
            m.w16(record + kKind, kind == kVerticalThenHorizontal ? kHidden : kStatic);
            continue;
        }
        s32 x, y, w, h;
        if (kind == kInterpolateRect) {
            x = interpolate(0, now, begin, end); y = interpolate(1, now, begin, end);
            w = interpolate(2, now, begin, end); h = interpolate(3, now, begin, end);
        } else {
            const bool horizontal = kind == kHorizontalThenVertical ? now < middle : now >= middle;
            const s32 from = now < middle ? begin : middle, until = now < middle ? middle : end;
            if (horizontal) {
                x = interpolate(0, now, from, until); w = interpolate(2, now, from, until);
                y = rect(kind == kHorizontalThenVertical ? kStartRect : kEndRect, 1); h = 2;
            } else {
                x = rect(kind == kHorizontalThenVertical ? kEndRect : kStartRect, 0);
                w = rect(kind == kHorizontalThenVertical ? kEndRect : kStartRect, 2);
                y = interpolate(1, now, from, until); h = interpolate(3, now, from, until);
            }
        }
        quad_rect(m, packet, x, y, w, h);
        ot_link(ot, packet);
    }
    const u32 briefing = m.r32(kBriefing), mode = m.r32(briefing + 3896);
    if (mode == 1) {
        const u32 done = o2_image_frame(m.r32(m.r32(kMenu) + 8) + 8, briefing + 3904);
        const u32 current = m.r32(m.r32(kBriefing) + 6300);
        const u32 mission = static_cast<u32>(m.r16s(current));
        if (!m.r8(0x8010B400u + 8 * mission + 6)) o2_brief_progress_draw();
        return done;
    }
    return mode == 2 ? 2u : previousResult;
}
}

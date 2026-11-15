#include "core/wrap.hpp"
#include "sim/game/game.hpp"

namespace ac3::sim {
u32 Game::o2_selector_chart(u32 packet) {
    cover(0x800F7EA4u);
    Frame frame(*this, 248);
    constexpr u32 kSelector = 0x80112E10u, kMenu = 0x80112DC4u, kChartPoints = 0x8010EB24u;
    constexpr u32 kLineColour = 64, kChartX = 236, kChartY = 308;
    const u32 selector = m.r32(kSelector);
    if (!m.r32(selector + 16)) return packet;
    const u32 specifications = m.r32(selector + 8), previous = m.r32(selector + 12);
    auto fadeValue = [&](u32 fade) { return o2_fade_value(fade); };
    s32 fade = fadeValue(6);
    if (fade > 20) fade = 20;
    const s32 remaining = wrap::sub(20, fade);
    s32 slide = wrap::mul(wrap::mul(remaining, remaining), 325) / 400;
    s32 ratings[5], numbers[5];
    const bool interpolate = o2_fade_state(2) == 1;
    if (interpolate) {
        m.w8(frame.at(16), m.r8(specifications));
        for (u32 byte = 0; byte < 3; ++byte) m.w8(frame.at(34 + byte), m.r8(specifications + 18 + byte));
        const s32 amount = fadeValue(2);
        for (u32 axis = 0; axis < 5; ++axis) {
            const s32 start = m.r8(previous + 1 + axis), end = m.r8(specifications + 1 + axis);
            ratings[axis] = wrap::add(wrap::mul(start, 25), wrap::mul(end - start, amount));
        }
        for (u32 field = 0; field < 5; ++field) {
            constexpr u32 offsets[] = {6, 8, 10, 12, 16};
            const u32 offset = offsets[field];
            const s32 start = offset == 12 ? m.r32s(previous + offset) : m.r16s(previous + offset);
            const s32 end = offset == 12 ? m.r32s(specifications + offset) : m.r16s(specifications + offset);
            numbers[field] = wrap::add(start, wrap::mul(wrap::sub(end, start), amount) / 25);
        }
    } else {
        for (u32 axis = 0; axis < 5; ++axis) ratings[axis] = m.r8(specifications + 1 + axis) * 25;
        numbers[0] = m.r16s(specifications + 6); numbers[1] = m.r16s(specifications + 8);
        numbers[2] = m.r16s(specifications + 10); numbers[3] = m.r32s(specifications + 12);
        numbers[4] = m.r16s(specifications + 16);
    }
    m.w32(frame.at(136), ratings[0]); m.w32(frame.at(140), ratings[1]); m.w32(frame.at(144), ratings[4]);
    for (u32 field = 0; field < 5; ++field) m.w32(frame.at(148 + 4 * field), numbers[field]);
    const u32 vertices = frame.at(40);
    m.w16(vertices, m.r16(kChartPoints) + kChartX); m.w16(vertices + 2, m.r16(kChartPoints + 2) + kChartY);
    constexpr u32 pointForAxis[] = {1, 2, 4, 5, 3};
    for (u32 axis = 0; axis < 5; ++axis) {
        const u32 point = kChartPoints + 4 * pointForAxis[axis];
        const s32 rating = static_cast<s16>(ratings[axis]);
        m.w16(vertices + 8 + 8 * axis, wrap::mul(rating, m.r16s(point)) / 375 + kChartX);
        m.w16(vertices + 10 + 8 * axis, wrap::mul(rating, m.r16s(point + 2)) / 375 + kChartY);
    }
    const u32 line = frame.at(88);
    m.w8(line + 8, kLineColour); m.w8(line + 9, kLineColour); m.w8(line + 10, kLineColour); m.w8(line + 11, 0);
    auto drawLine = [&](u32 start, u32 end) {
        m.w16(line, m.r16(kChartPoints + 4 * start) + kChartX + static_cast<u32>(slide));
        m.w16(line + 2, m.r16(kChartPoints + 4 * start + 2) + kChartY);
        m.w16(line + 4, m.r16(kChartPoints + 4 * end) + kChartX + static_cast<u32>(slide));
        m.w16(line + 6, m.r16(kChartPoints + 4 * end + 2) + kChartY);
        packet = o2_line_double(line, packet, m.r32(m.r32(kMenu) + 8));
    };
    for (u32 point = 1; point <= 5; ++point) drawLine(0, point);
    for (u32 point = 1; point <= 5; ++point) drawLine(point, point % 5 + 1);
    if (m.r32(m.r32(kSelector) + 16) & 2) {
        const u32 triangle = frame.at(104);
        for (u32 vertex = 0; vertex < 3; ++vertex) {
            m.w8(triangle + 16 + 4 * vertex, 207); m.w8(triangle + 17 + 4 * vertex, 139);
            m.w8(triangle + 18 + 4 * vertex, 23);
        }
        m.w8(triangle + 19, 0);
        constexpr u32 triangles[][3] = {{0, 1, 2}, {0, 2, 5}, {0, 5, 3}, {0, 3, 4}, {0, 4, 1}};
        for (const auto& points : triangles) {
            for (u32 vertex = 0; vertex < 3; ++vertex) {
                const u32 point = vertices + 8 * points[vertex];
                m.w16(triangle + 4 * vertex, m.r16(point) + static_cast<u32>(slide));
                m.w16(triangle + 4 * vertex + 2, m.r16(point + 2));
            }
            packet = o2_triangle_gouraud(triangle, packet, m.r32(m.r32(kMenu) + 8));
        }
    }
    if (m.r32(m.r32(kSelector) + 16) & 1) {
        const s32 textRemaining = wrap::sub(25, fadeValue(1));
        slide = wrap::neg(wrap::mul(wrap::mul(textRemaining, textRemaining), 325)) / 625;
        o2_text_font(7); o2_text_semi(0xFFFFFFFFu); o2_text_fold(1, 0, 1); o2_text_frame_mode(1);
        const u32 mode = o2_flag_value(1);
        if (mode == 1) o2_text_rgb(225, 95, 10);
        else if (o2_flag_value(1) == 0) o2_text_rgb(229, 229, 213);
        constexpr u32 order[] = {1, 0, 2};
        for (u32 lineIndex = 0; lineIndex < 3; ++lineIndex) {
            const s32 value = numbers[order[lineIndex]];
            o2_text_at(wrap::add(slide, 70), 258 + 11 * lineIndex);
            o2_text_printf(0x800CD384u, value / 100, 0, 0);
            o2_text_at(wrap::add(slide, 91), 258 + 11 * lineIndex);
            o2_text_printf_zero(0x800CD38Cu, value % 100, 0, 0);
        }
        o2_text_at(wrap::add(slide, 70), 291); o2_text_printf(0x800CD390u, numbers[3], 0, 0);
        o2_text_at(wrap::add(slide, 60), 302); o2_text_printf(0x800CD390u, numbers[4], 0, 0);
    }
    const u32 mode = o2_flag_value(1);
    o2_flag(6, m.r8(specifications + 19)); o2_flag(1, 1);
    o2_flag(5, m.r8(specifications + 18) + 3 * mode);
    return packet;
}
}
